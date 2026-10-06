/* util.c: files and folders by UTF-8 path, on POSIX and Windows. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "rb_int.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

void rb_err(char *err, size_t errlen, const char *fmt, ...)
{
    va_list ap;
    if (!err || !errlen)
        return;
    va_start(ap, fmt);
    vsnprintf(err, errlen, fmt, ap);
    va_end(ap);
}

void rb_join(char *out, size_t n, const char *a, const char *b)
{
    size_t la = strlen(a);
    if (la && (a[la - 1] == '/' || a[la - 1] == '\\'))
        snprintf(out, n, "%s%s", a, b);
    else
        snprintf(out, n, "%s%c%s", a, RB_SEP, b);
}

void rb_dirname(char *out, size_t n, const char *path)
{
    const char *s = strrchr(path, '/');
#ifdef _WIN32
    const char *t = strrchr(path, '\\');
    if (t && (!s || t > s))
        s = t;
#endif
    if (!s) {
        snprintf(out, n, ".");
        return;
    }
    snprintf(out, n, "%.*s", (int)(s - path), path);
    if (!out[0])
        snprintf(out, n, "%c", RB_SEP);
}

#ifdef _WIN32
static wchar_t *wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = malloc(sizeof(wchar_t) * (size_t)(n > 0 ? n : 1));
    if (w)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n > 0 ? n : 1);
    return w;
}

FILE *rb_fopen(const char *path, const char *mode)
{
    wchar_t *wp = wide(path), *wm = wide(mode);
    FILE *f = wp && wm ? _wfopen(wp, wm) : NULL;
    free(wp);
    free(wm);
    return f;
}

int rb_seek(FILE *f, uint64_t off) { return _fseeki64(f, (long long)off, SEEK_SET) == 0; }

uint64_t rb_file_size(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    wchar_t *w = wide(path);
    int ok = w && GetFileAttributesExW(w, GetFileExInfoStandard, &a);
    free(w);
    return ok && !(a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        ? ((uint64_t)a.nFileSizeHigh << 32 | a.nFileSizeLow) : 0;
}

static DWORD attrs(const char *path)
{
    wchar_t *w = wide(path);
    DWORD a = w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES;
    free(w);
    return a;
}

int rb_is_dir(const char *path)
{
    DWORD a = attrs(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int rb_exists(const char *path) { return attrs(path) != INVALID_FILE_ATTRIBUTES; }

static int mkdir1(const char *path)
{
    wchar_t *w = wide(path);
    int ok = w && (CreateDirectoryW(w, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    free(w);
    return ok;
}

int rb_rename(const char *from, const char *to)
{
    wchar_t *a = wide(from), *b = wide(to);
    int ok = a && b && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING);
    free(a);
    free(b);
    return ok;
}

int rb_rmtree(const char *path)
{
    char pat[2048], child[2048];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wchar_t *w;
    int ok = 1;
    DWORD a = attrs(path);
    if (a == INVALID_FILE_ATTRIBUTES)
        return 1;
    if (!(a & FILE_ATTRIBUTE_DIRECTORY)) {
        w = wide(path);
        SetFileAttributesW(w, FILE_ATTRIBUTE_NORMAL);
        ok = DeleteFileW(w) != 0;
        free(w);
        return ok;
    }
    rb_join(pat, sizeof pat, path, "*");
    w = wide(pat);
    h = FindFirstFileW(w, &fd);
    free(w);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char name[1024];
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
                continue;
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof name, NULL, NULL);
            rb_join(child, sizeof child, path, name);
            ok &= rb_rmtree(child);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    w = wide(path);
    ok &= RemoveDirectoryW(w) != 0;
    free(w);
    return ok;
}

int rb_set_exec(const char *path) { (void)path; return 1; }
#else
FILE *rb_fopen(const char *path, const char *mode) { return fopen(path, mode); }
int rb_seek(FILE *f, uint64_t off) { return fseeko(f, (off_t)off, SEEK_SET) == 0; }

uint64_t rb_file_size(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode) ? (uint64_t)st.st_size : 0;
}

int rb_is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int rb_exists(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0;
}

static int mkdir1(const char *path) { return mkdir(path, 0755) == 0 || rb_is_dir(path); }
int rb_rename(const char *from, const char *to) { return rename(from, to) == 0; }

int rb_rmtree(const char *path)
{
    struct stat st;
    DIR *d;
    struct dirent *e;
    int ok = 1;
    if (lstat(path, &st) != 0)
        return 1;
    if (!S_ISDIR(st.st_mode))
        return unlink(path) == 0;
    if ((d = opendir(path)) != NULL) {
        while ((e = readdir(d)) != NULL) {
            char child[2048];
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            rb_join(child, sizeof child, path, e->d_name);
            ok &= rb_rmtree(child);
        }
        closedir(d);
    }
    return ok & (rmdir(path) == 0);
}

int rb_set_exec(const char *path) { return chmod(path, 0755) == 0; }
#endif

int rb_mkdirs(const char *path)
{
    char p[2048];
    size_t i, n;
    snprintf(p, sizeof p, "%s", path);
    n = strlen(p);
    for (i = 1; i < n; i++) {
        if (p[i] == '/' || p[i] == '\\') {
            char c = p[i];
#ifdef _WIN32
            if (i == 2 && p[1] == ':')
                continue;                       /* C:\ */
#endif
            p[i] = 0;
            if (!rb_is_dir(p) && !mkdir1(p))
                return 0;
            p[i] = c;
        }
    }
    return rb_is_dir(p) || mkdir1(p);
}

int rb_write_file(const char *path, const void *p, size_t n)
{
    FILE *f = rb_fopen(path, "wb");
    int ok;
    if (!f)
        return 0;
    ok = fwrite(p, 1, n, f) == n;
    return (fclose(f) == 0) & ok;
}

void *rb_read_file(const char *path, size_t *n)
{
    uint64_t sz = rb_file_size(path);
    FILE *f;
    void *p;
    if (!sz || sz > ((uint64_t)1 << 31) || !(f = rb_fopen(path, "rb")))
        return NULL;
    p = malloc((size_t)sz);
    if (p && fread(p, 1, (size_t)sz, f) != (size_t)sz) {
        free(p);
        p = NULL;
    }
    fclose(f);
    if (p && n)
        *n = (size_t)sz;
    return p;
}
