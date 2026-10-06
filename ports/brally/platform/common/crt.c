/* crt.c: the MSVC runtime the game links, beyond what the host C library
 * already provides -- its file names (DOS drives and backslashes), the
 * directory walk, the case-insensitive string helpers, the exit-handler
 * table, operator new/delete and __ftol.
 *
 * Paths: C:\BOSSRALLY\x and D:\x both name the disc's x (the installer only
 * copied disc files). Anything the game writes goes to the save directory,
 * the path flattened into one name, and a read looks there first. */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "plat.h"

/* ---- paths -------------------------------------------------------------------- */
static char s_cwd[520] = "C:\\BOSSRALLY";

/* game path -> "X:\A\B" (uppercase drive, backslashes, no . or ..) */
static void canon(const char *p, char *out, size_t n)
{
    char buf[520], *parts[64], *tok, *q;
    int np = 0, i;
    char drive;
    const char *rest;
    if (p[0] && p[1] == ':') {
        drive = (char)toupper((unsigned char)p[0]);
        rest = p + 2;
        if (*rest != '\\' && *rest != '/') {
            if (drive == s_cwd[0])
                snprintf(buf, sizeof buf, "%s\\%s", s_cwd + 2, rest);
            else
                snprintf(buf, sizeof buf, "\\%s", rest);
        } else {
            snprintf(buf, sizeof buf, "%s", rest);
        }
    } else if (p[0] == '\\' || p[0] == '/') {
        drive = s_cwd[0];
        snprintf(buf, sizeof buf, "%s", p);
    } else {
        drive = s_cwd[0];
        snprintf(buf, sizeof buf, "%s\\%s", s_cwd + 2, p);
    }
    for (q = buf; *q; q++)
        if (*q == '/')
            *q = '\\';
    for (tok = strtok(buf, "\\"); tok; tok = strtok(NULL, "\\")) {
        if (!*tok || !strcmp(tok, "."))
            continue;
        if (!strcmp(tok, "..")) {
            if (np)
                np--;
            continue;
        }
        if (np < 64)
            parts[np++] = tok;
    }
    snprintf(out, n, "%c:", drive);
    for (i = 0; i < np; i++) {
        strncat(out, "\\", n - strlen(out) - 1);
        strncat(out, parts[i], n - strlen(out) - 1);
    }
    if (np == 0)
        strncat(out, "\\", n - strlen(out) - 1);
}

/* the path under the drive root: C:\BOSSRALLY\x and D:\x both mean disc x;
 * anything else on C: (the settings files at C:\) lives in the save overlay
 * under its own name */
static const char *drive_rel(const char *c)
{
    if (c[0] == 'C' && !strncasecmp(c + 2, "\\BOSSRALLY", 10) && (c[12] == '\\' || c[12] == 0))
        return c[12] == '\\' ? c + 13 : c + 12;
    if (c[0] == 'C')
        return c[2] == '\\' ? c + 3 : c + 2;
    if (c[0] == 'D')
        return c[2] == '\\' ? c + 3 : c + 2;
    return NULL;
}

/* case-insensitive walk of a host directory for one relative path */
static int ci_find(const char *root, const char *rel, char *out, size_t n)
{
    char cur[1024], comp[260];
    const char *r = rel;
    snprintf(cur, sizeof cur, "%s", root);
    while (*r) {
        const char *e = strchr(r, '\\');
        size_t l = e ? (size_t)(e - r) : strlen(r);
        host_dir *d;
        const char *nm;
        int found = 0;
        if (l >= sizeof comp)
            return 0;
        memcpy(comp, r, l);
        comp[l] = 0;
        d = host_dir_open(cur);
        if (!d)
            return 0;
        while ((nm = host_dir_next(d, NULL, NULL)) != NULL) {
            if (!strcasecmp(nm, comp)) {
                size_t cl = strlen(cur);
                snprintf(cur + cl, sizeof cur - cl, "/%s", nm);
                found = 1;
                break;
            }
        }
        host_dir_close(d);
        if (!found)
            return 0;
        r += l;
        if (*r == '\\')
            r++;
    }
    snprintf(out, n, "%s", cur);
    return 1;
}

static void overlay(const char *rel, char *out, size_t n)
{
    char flat[520], *q;
    snprintf(flat, sizeof flat, "%s", rel);
    for (q = flat; *q; q++)
        *q = *q == '\\' ? '_' : (char)tolower((unsigned char)*q);
    snprintf(out, n, "%s/%s", host_save_dir(), flat);
}

static int exists(const char *host)
{
    FILE *f = fopen(host, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    {
        host_dir *d = host_dir_open(host);
        if (d) {
            host_dir_close(d);
            return 1;
        }
    }
    return 0;
}

/* BR_FILELOG=PATH: every file the game resolves, one host path per line,
 * appended (which of the disc's files a run reads) */
static void filelog(const char *host)
{
    static FILE *f;
    static int init;
    if (!init) {
        const char *p = getenv("BR_FILELOG");
        init = 1;
        if (p)
            f = fopen(p, "a");
    }
    if (f) {
        fprintf(f, "%s\n", host);
        fflush(f);
    }
}

static int plat_path_find(const char *game, char *host, size_t n)
{
    char c[520];
    const char *rel;
    canon(game, c, sizeof c);
    rel = drive_rel(c);
    if (!rel)
        return 0;
    overlay(rel, host, n);
    if (*rel && exists(host))
        return 1;
    if (!*rel) {
        snprintf(host, n, "%s", host_cd_dir());
        return 1;
    }
    return ci_find(host_cd_dir(), rel, host, n);
}

int plat_path(const char *game, char *host, size_t n)
{
    int r = plat_path_find(game, host, n);
    if (r)
        filelog(host);
    return r;
}

void plat_path_new(const char *game, char *host, size_t n)
{
    char c[520];
    const char *rel;
    canon(game, c, sizeof c);
    rel = drive_rel(c);
    overlay(rel ? rel : c, host, n);
}

int plat_drive(void) { return s_cwd[0] - 'A' + 1; }

int plat_chdrive(int d)
{
    if (d != 3 && d != PLAT_CD_DRIVE)
        return -1;
    snprintf(s_cwd, sizeof s_cwd, d == 3 ? "C:\\BOSSRALLY" : "D:\\");
    return 0;
}

int plat_chdir(const char *game)
{
    char c[520], h[1024];
    canon(game, c, sizeof c);
    if (!drive_rel(c) || !plat_path(c, h, sizeof h))
        return -1;
    snprintf(s_cwd, sizeof s_cwd, "%s", c);
    return 0;
}

void plat_getcwd(char *out, size_t n) { snprintf(out, n, "%s", s_cwd); }

/* ---- the C runtime's file names ------------------------------------------------- */
FILE *br_fopen(const char *path, const char *mode)
{
    char host[1024];
    FILE *f;
    if (strchr(mode, 'w') || strchr(mode, 'a')) {
        plat_path_new(path, host, sizeof host);
        /* append and update modes start from the file the game read */
        if (strchr(mode, 'a') || strchr(mode, '+')) {
            char src[1024];
            if (!exists(host) && plat_path(path, src, sizeof src) && strcmp(src, host)) {
                FILE *a = fopen(src, "rb"), *b = fopen(host, "wb");
                int ch;
                while (a && b && (ch = getc(a)) != EOF)
                    putc(ch, b);
                if (a)
                    fclose(a);
                if (b)
                    fclose(b);
            }
        }
    } else if (!plat_path(path, host, sizeof host)) {
        PLOG("fopen(%s): no such file\n", path);
        errno = ENOENT;
        return NULL;
    }
    f = fopen(host, mode);
    PLOG("fopen(%s, %s) -> %s%s\n", path, mode, host, f ? "" : " FAILED");
    return f;
}

int br_rename(const char *from, const char *to)
{
    char a[1024], b[1024];
    plat_path_new(from, a, sizeof a);
    plat_path_new(to, b, sizeof b);
    return rename(a, b);
}

int br_access(const char *path, int mode)
{
    char host[1024];
    (void)mode;
    return plat_path(path, host, sizeof host) ? 0 : -1;
}

int _chdir(const char *p)   { return plat_chdir(p); }
int _getdrive(void)         { return plat_drive(); }
int _chdrive(int d)         { return plat_chdrive(d); }
char *_getcwd(char *buf, int n)
{
    if (!buf) {
        n = n > 0 ? n : 260;
        buf = (char *)malloc((size_t)n);
    }
    plat_getcwd(buf, (size_t)n);
    return buf;
}

/* ---- _findfirst / _findnext ---------------------------------------------------- */
/* A listing is the save overlay's entries for the directory first, then the
 * disc's, skipping a disc name the overlay already gave: the files the game
 * wrote (saved races, seasons) live only in the overlay. */
typedef struct pfind {
    char dir[1024];            /* the disc directory, host path ("" = none) */
    char pat[260];
    char flat[520];            /* the overlay's prefix for the directory */
    int  phase;                /* 0 overlay, 1 disc */
    host_dir *d;
} pfind;

static int wild(const char *p, const char *s)
{
    if (!*p)
        return !*s;
    if (*p == '*')
        return wild(p + 1, s) || (*s && wild(p, s + 1));
    if (*s && (*p == '?' || tolower((unsigned char)*p) == tolower((unsigned char)*s)))
        return wild(p + 1, s + 1);
    return 0;
}

static void find_hit(struct _finddata_t *fd, const char *nm, int is_dir, uint64_t size)
{
    memset(fd, 0, sizeof *fd);
    fd->attrib = is_dir ? _A_SUBDIR : _A_ARCH;
    fd->size = (_fsize_t)size;
    snprintf(fd->name, sizeof fd->name, "%s", nm);
}

/* an overlay name under the listed directory: the part after its prefix,
 * provided that part is not itself in a subdirectory of the disc's */
static const char *overlay_leaf(const pfind *f, const char *nm)
{
    size_t l = strlen(f->flat);
    const char *u;
    if (strncmp(nm, f->flat, l))
        return NULL;
    nm += l;
    if ((u = strchr(nm, '_')) != NULL && f->dir[0]) {
        char sub[1024], h[1024];
        snprintf(sub, sizeof sub, "%.*s", (int)(u - nm), nm);
        if (ci_find(f->dir, sub, h, sizeof h))
            return NULL;
    }
    return nm;
}

static int find_next(pfind *f, struct _finddata_t *fd)
{
    const char *nm, *leaf;
    int is_dir;
    uint64_t size;
    for (;;) {
        if (!f->d) {
            if (f->phase++ != 0 || !f->dir[0] || !(f->d = host_dir_open(f->dir)))
                return -1;
        }
        while ((nm = host_dir_next(f->d, &is_dir, &size)) != NULL) {
            if (!strcmp(nm, ".") || !strcmp(nm, ".."))
                continue;
            if (f->phase == 0) {
                if ((leaf = overlay_leaf(f, nm)) != NULL && !is_dir && wild(f->pat, leaf)) {
                    find_hit(fd, leaf, 0, size);
                    return 0;
                }
                continue;
            }
            if (wild(f->pat, nm)) {
                char o[1100], *k;
                size_t l;
                snprintf(o, sizeof o, "%s/", host_save_dir());
                l = strlen(o);
                snprintf(o + l, sizeof o - l, "%s%s", f->flat, nm);
                for (k = o + l; *k; k++)
                    *k = (char)tolower((unsigned char)*k);
                if (!is_dir && exists(o))        /* the overlay gave it already */
                    continue;
                find_hit(fd, nm, is_dir, size);
                return 0;
            }
        }
        host_dir_close(f->d);
        f->d = NULL;
    }
}

intptr_t _findfirst(const char *spec, struct _finddata_t *fd)
{
    char c[520], dir[520], host[1024];
    char *slash, *q;
    const char *rel;
    pfind *f;
    canon(spec, c, sizeof c);
    snprintf(dir, sizeof dir, "%s", c);
    slash = strrchr(dir, '\\');
    if (!slash)
        return -1;
    *slash = 0;
    if (!(rel = drive_rel(dir)))
        return -1;
    f = (pfind *)calloc(1, sizeof *f);
    snprintf(f->pat, sizeof f->pat, "%s", slash + 1);
    snprintf(f->flat, sizeof f->flat, "%s%s", rel, *rel ? "\\" : "");
    for (q = f->flat; *q; q++)
        *q = *q == '\\' ? '_' : (char)tolower((unsigned char)*q);
    if (!*rel)
        snprintf(f->dir, sizeof f->dir, "%s", host_cd_dir());
    else if (ci_find(host_cd_dir(), rel, host, sizeof host))
        snprintf(f->dir, sizeof f->dir, "%s", host);
    if (f->dir[0]) {
        char what[1100];
        snprintf(what, sizeof what, "%s/%s", f->dir, f->pat);    /* a listing: the folder and its pattern */
        filelog(what);
    }
    f->d = host_dir_open(host_save_dir());
    if (find_next(f, fd) != 0) {
        if (f->d)
            host_dir_close(f->d);
        free(f);
        return -1;
    }
    return (intptr_t)f;
}

int _findnext(intptr_t h, struct _finddata_t *fd)
{
    return h == -1 ? -1 : find_next((pfind *)h, fd);
}

int _findclose(intptr_t h)
{
    if (h != -1) {
        if (((pfind *)h)->d)
            host_dir_close(((pfind *)h)->d);
        free((void *)h);
    }
    return 0;
}

/* ---- strings ------------------------------------------------------------------------- */
int _stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int _strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
char *_strupr(char *s)
{
    char *p;
    for (p = s; *p; p++)
        *p = (char)toupper((unsigned char)*p);
    return s;
}
char *_strlwr(char *s)
{
    char *p;
    for (p = s; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return s;
}

char *_ltoa(long v, char *out, int radix)
{
    /* MSVC: radix 10 is signed, any other radix shows the 32-bit pattern */
    char tmp[40];
    int i = 0, j, neg = radix == 10 && (int32_t)v < 0;
    uint32_t u = neg ? 0u - (uint32_t)v : (uint32_t)v;
    do {
        int d = (int)(u % (uint32_t)radix);
        tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        u /= (uint32_t)radix;
    } while (u);
    if (neg)
        tmp[i++] = '-';
    for (j = 0; j < i; j++)
        out[j] = tmp[i - 1 - j];
    out[i] = 0;
    return out;
}

char *_itoa(int v, char *out, int radix) { return _ltoa(v, out, radix); }
int _finite(double d) { return isfinite(d); }
#ifdef _WIN32
/* errno is (*_errno()) in the Windows CRT, and _errno here is the renamed
 * emulation (br_winemu.h): forward to the system's own */
#undef _errno
__declspec(dllimport) int *_errno(void);
int *brw__errno(void) { return _errno(); }
#define _errno brw__errno
#else
int *_errno(void) { return &errno; }
#endif

/* MSVC's rand/srand (one sequence for the process; the game seeds and draws
 * from one thread) */
static unsigned int s_holdrand = 1;
void br_srand(unsigned int seed) { s_holdrand = seed; }
int br_rand(void)
{
    s_holdrand = s_holdrand * 214013u + 2531011u;
    return (int)((s_holdrand >> 16) & 0x7FFF);
}

/* MSVC's qsort (VC5 qsort.c): partition around the middle element with an
 * explicit stack, selection sort below 9 elements. The algorithm, not just
 * a sort: ties land where the original's land (tools/brally/brbox_imports.py
 * models the same one for the oracle). */
static void qs_swap(char *a, char *b, size_t width)
{
    char t;
    if (a != b)
        while (width--) {
            t = *a;
            *a++ = *b;
            *b++ = t;
        }
}

void br_qsort(void *base, size_t num, size_t width, int (*comp)(const void *, const void *))
{
    char *lostk[32], *histk[32];
    char *lo, *hi, *mid, *loguy, *higuy, *p, *max;
    size_t size;
    int sp = 0;
    if (num < 2 || width == 0)
        return;
    lo = (char *)base;
    hi = (char *)base + width * (num - 1);
recurse:
    size = (size_t)(hi - lo) / width + 1;
    if (size <= 8) {
        while (hi > lo) {
            max = lo;
            for (p = lo + width; p <= hi; p += width)
                if (comp(p, max) > 0)
                    max = p;
            qs_swap(max, hi, width);
            hi -= width;
        }
    } else {
        mid = lo + (size / 2) * width;
        qs_swap(mid, lo, width);
        loguy = lo;
        higuy = hi + width;
        for (;;) {
            do
                loguy += width;
            while (loguy <= hi && comp(loguy, lo) <= 0);
            do
                higuy -= width;
            while (higuy > lo && comp(higuy, lo) >= 0);
            if (higuy < loguy)
                break;
            qs_swap(loguy, higuy, width);
        }
        qs_swap(lo, higuy, width);
        if (higuy - 1 - lo >= hi - loguy) {
            if (lo + width < higuy) {
                lostk[sp] = lo;
                histk[sp++] = higuy - width;
            }
            if (loguy < hi) {
                lo = loguy;
                goto recurse;
            }
        } else {
            if (loguy < hi) {
                lostk[sp] = loguy;
                histk[sp++] = hi;
            }
            if (lo + width < higuy) {
                hi = higuy - width;
                goto recurse;
            }
        }
    }
    if (--sp >= 0) {
        lo = lostk[sp];
        hi = histk[sp];
        goto recurse;
    }
}

/* ---- exit handlers, operator new/delete, __ftol ------------------------------------ */
typedef int (*BrCrtOnExitFn)(void);
static BrCrtOnExitFn s_onexit[64];
static int s_nonexit;

static void run_onexit(void)
{
    while (s_nonexit > 0)
        s_onexit[--s_nonexit]();
}

BrCrtOnExitFn _onexit(BrCrtOnExitFn fn)
{
    if (s_nonexit == 0)
        atexit(run_onexit);
    if (s_nonexit >= 64)
        return NULL;
    s_onexit[s_nonexit++] = fn;
    return fn;
}

BrCrtOnExitFn dllonexit(BrCrtOnExitFn fn, void *begin, void *end)
{
    (void)begin;
    (void)end;
    return _onexit(fn);
}

/* MSVC's structured-exception frame handler, named by the decompiled
 * frames; nothing raises a structured exception here */
int _except_handler3(void) { return 1; }

#ifdef _WIN32
/* mingw's float.h declares _finite dllimport; the import slot it then asks
 * for points at the emulation's (br_winemu.h renamed it) */
void *__imp_brw__finite = (void *)&_finite;
#endif

void *BrOperatorNew(size_t cb) { return malloc(cb ? cb : 1); }
void  BrOperatorDelete(void *p) { free(p); }

/* __ftol: a 64-bit fistp, low dword kept; out of range (and NaN) the x87
 * stores the integer indefinite, whose low dword is 0 */
int32_t BrFtolTrunc(float f)
{
    double d = (double)f;
    if (!(d >= -9223372036854775808.0) || !(d < 9223372036854775808.0))
        return 0;
    return (int32_t)(int64_t)d;
}
