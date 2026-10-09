/* libc.c: the C library the platform layer and the game's core need on the
 * console.  Files are the debugger's host files (PCDRV); stdout and stderr
 * go to the BIOS TTY.  Formatting is libultra's own (xprintf.c), as the game
 * was linked with. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ps1.h"

int tgr_vsprintf(char *dst, const char *fmt, va_list ap);

/* ---- memory and strings -------------------------------------------------- */
void ps1_bad_size(const char *fn, void *caller, size_t n);

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (n > 0x800000)
        ps1_bad_size("memcpy", __builtin_return_address(0), n);
    if ((((uintptr_t)d | (uintptr_t)s) & 3) == 0) {
        while (n >= 16) {
            uint32_t a = ((const uint32_t *)s)[0], b = ((const uint32_t *)s)[1];
            uint32_t c = ((const uint32_t *)s)[2], e = ((const uint32_t *)s)[3];
            ((uint32_t *)d)[0] = a; ((uint32_t *)d)[1] = b;
            ((uint32_t *)d)[2] = c; ((uint32_t *)d)[3] = e;
            d += 16; s += 16; n -= 16;
        }
        while (n >= 4) {
            *(uint32_t *)d = *(const uint32_t *)s;
            d += 4; s += 4; n -= 4;
        }
    }
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d <= s || d >= s + n)
        return memcpy(dst, src, n);
    while (n--)
        d[n] = s[n];
    return dst;
}

void *memset(void *p, int c, size_t n)
{
    uint8_t *d = p;
    uint32_t v = (uint8_t)c * 0x01010101u;
    if (n > 0x800000)
        ps1_bad_size("memset", __builtin_return_address(0), n);
    while (n && ((uintptr_t)d & 3)) { *d++ = (uint8_t)c; n--; }
    while (n >= 16) {
        ((uint32_t *)d)[0] = v; ((uint32_t *)d)[1] = v;
        ((uint32_t *)d)[2] = v; ((uint32_t *)d)[3] = v;
        d += 16; n -= 16;
    }
    while (n >= 4) { *(uint32_t *)d = v; d += 4; n -= 4; }
    while (n--)
        *d++ = (uint8_t)c;
    return p;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y)
            return *x - *y;
    return 0;
}

size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }
char *strncpy(char *d, const char *s, size_t n)
{
    size_t i;
    for (i = 0; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}
int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) a++, b++;
    return (uint8_t)*a - (uint8_t)*b;
}
int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (uint8_t)*a - (uint8_t)*b;
        if (!*a) return 0;
    }
    return 0;
}
char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
    }
}
char *strtok_r(char *s, const char *delim, char **save)
{
    char *t;
    if (!s) s = *save;
    while (*s && strchr(delim, *s)) s++;
    if (!*s) { *save = s; return NULL; }
    t = s;
    while (*s && !strchr(delim, *s)) s++;
    if (*s) *s++ = 0;
    *save = s;
    return t;
}
char *strtok(char *s, const char *delim) { static char *save; return strtok_r(s, delim, &save); }
char *strstr(const char *h, const char *n)
{
    size_t k = strlen(n);
    for (; *h; h++)
        if (!strncmp(h, n, k)) return (char *)h;
    return k ? NULL : (char *)h;
}
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
void bcopy(const void *src, void *dst, size_t n) { memmove(dst, src, n); }
void bzero(void *p, size_t n) { memset(p, 0, n); }
int  bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }

/* a size no call here can mean: a fault, with who asked */
void ps1_bad_size(const char *fn, void *caller, size_t n)
{
    static char msg[96];
    sprintf(msg, "%s of %08X bytes from %08X", fn, (unsigned)n, (unsigned)(uintptr_t)caller);
    ps1_fatal(msg);
}

/* ---- numbers ------------------------------------------------------------ */
unsigned long strtoul(const char *s, char **end, int base)
{
    unsigned long v = 0;
    while (*s == ' ' || *s == '\t') s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; }
    if (base == 0) base = 10;
    for (;; s++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'z' ? *s - 'a' + 10
              : *s >= 'A' && *s <= 'Z' ? *s - 'A' + 10 : 99;
        if (d >= base) break;
        v = v * base + d;
    }
    if (end) *end = (char *)s;
    return v;
}
long strtol(const char *s, char **end, int base)
{
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') return -(long)strtoul(s + 1, end, base);
    return (long)strtoul(s, end, base);
}
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
int abs(int x) { return x < 0 ? -x : x; }

void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    uint8_t *b = base, t;
    size_t i, j, k;
    for (i = 1; i < n; i++)                     /* insertion sort: small tables only */
        for (j = i; j > 0 && cmp(b + (j - 1) * size, b + j * size) > 0; j--)
            for (k = 0; k < size; k++) {
                t = b[(j - 1) * size + k];
                b[(j - 1) * size + k] = b[j * size + k];
                b[j * size + k] = t;
            }
}

void *bsearch(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        const uint8_t *e = (const uint8_t *)base + mid * size;
        int c = cmp(key, e);
        if (c == 0) return (void *)e;
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return NULL;
}

/* ---- the environment: switches set by main (os/main_ps1.c) -------------- */
static const char *s_env[16][2];
void ps1_setenv(const char *name, const char *value)
{
    int i;
    for (i = 0; i < 16; i++)
        if (!s_env[i][0] || !strcmp(s_env[i][0], name)) {
            s_env[i][0] = name;
            s_env[i][1] = value;
            return;
        }
}
char *getenv(const char *name)
{
    int i;
    for (i = 0; i < 16 && s_env[i][0]; i++)
        if (!strcmp(s_env[i][0], name))
            return (char *)s_env[i][1];
    return NULL;
}

/* ---- memory: a bump allocator over the RAM past the image --------------- */
extern char __heap_start[], __heap_end[];
static char *s_brk;
void *malloc(size_t n)
{
    char *p;
    if (!s_brk) s_brk = __heap_start;
    p = (char *)(((uintptr_t)s_brk + 7) & ~(uintptr_t)7) + 8;     /* the size before it */
    if (p + n > __heap_end) ps1_fatal("malloc: out of memory");
    ((size_t *)p)[-1] = n;
    s_brk = p + n;
    return p;
}
void *realloc(void *old, size_t n)
{
    void *p;
    size_t had;
    if (!old) return malloc(n);
    had = ((size_t *)old)[-1];
    if (n <= had) return old;
    if ((char *)old + had == s_brk) {           /* the last block grows in place */
        if ((char *)old + n > __heap_end) ps1_fatal("realloc: out of memory");
        s_brk = (char *)old + n;
        ((size_t *)old)[-1] = n;
        return old;
    }
    p = malloc(n);
    memcpy(p, old, had);
    return p;
}
void *calloc(size_t n, size_t size) { void *p = malloc(n * size); memset(p, 0, n * size); return p; }
void free(void *p) { (void)p; }
size_t ps1_heap_used(void) { return s_brk ? (size_t)(s_brk - __heap_start) : 0; }

void abort(void) { ps1_fatal("abort"); }
void exit(int code) { (void)code; ps1_fatal("exit"); }

/* ---- stdio --------------------------------------------------------------- */
/* PCDRV reads past a file's end as zeros, so a file read knows its size */
struct PS1_FILE { int fd; int tty; int n; int pos, size; char buf[512]; };
static struct PS1_FILE s_tty = { -1, 1, 0, 0, 0, {0} };
FILE *const stderr = &s_tty, *const stdout = &s_tty;
static struct PS1_FILE s_files[6];

FILE *fopen(const char *path, const char *mode)
{
    int i, fd;
    for (i = 0; i < 6 && s_files[i].fd > 0; i++)
        ;
    if (i == 6) return NULL;
    if (mode[0] == 'w') fd = pcdrv_creat(path);
    else fd = pcdrv_open(path, mode[1] == '+' ? 2 : 0);
    if (fd < 0) return NULL;
    s_files[i].fd = fd + 1;                     /* 0 marks a free slot */
    s_files[i].tty = 0;
    s_files[i].n = 0;
    s_files[i].pos = 0;
    s_files[i].size = 0;
    if (mode[0] != 'w') {
        s_files[i].size = pcdrv_seek(fd, 0, 2);
        pcdrv_seek(fd, 0, 0);
    }
    return &s_files[i];
}

int fflush(FILE *f)
{
    if (f->tty) {
        f->buf[f->n] = 0;
        ps1_tty(f->buf);
    } else if (f->n) {
        pcdrv_write(f->fd - 1, f->buf, f->n);
    }
    f->n = 0;
    return 0;
}

size_t fwrite(const void *p, size_t size, size_t n, FILE *f)
{
    const char *s = p;
    size_t len = size * n, i;
    if (!f->tty) {
        f->pos += (int)len;
        if (f->pos > f->size) f->size = f->pos;
    }
    if (!f->tty && len >= sizeof f->buf) {
        fflush(f);
        pcdrv_write(f->fd - 1, s, (int)len);
        return n;
    }
    for (i = 0; i < len; i++) {
        if (f->n == (int)sizeof f->buf - 1) fflush(f);
        f->buf[f->n++] = s[i];
        if (f->tty && s[i] == '\n') fflush(f);
    }
    return n;
}

size_t fread(void *p, size_t size, size_t n, FILE *f)
{
    int want = (int)(size * n), r;
    fflush(f);
    if (want > f->size - f->pos) want = f->size - f->pos;
    if (want <= 0) return 0;
    r = pcdrv_read(f->fd - 1, p, want);
    if (r <= 0) return 0;
    f->pos += r;
    return (size_t)r / (size ? size : 1);
}

int fseek(FILE *f, long off, int whence)
{
    int r;
    fflush(f);
    r = pcdrv_seek(f->fd - 1, (int)off, whence);
    if (r < 0) return -1;
    f->pos = r;
    return 0;
}
long ftell(FILE *f) { return f->pos; }
int fclose(FILE *f) { fflush(f); pcdrv_close(f->fd - 1); f->fd = 0; return 0; }
int fputc(int c, FILE *f) { char ch = (char)c; fwrite(&ch, 1, 1, f); return c; }
int fputs(const char *s, FILE *f) { fwrite(s, 1, strlen(s), f); return 0; }
int putchar(int c) { return fputc(c, stdout); }
int puts(const char *s) { fputs(s, stdout); fputc('\n', stdout); return 0; }

char *fgets(char *s, int n, FILE *f)
{
    int i = 0, c;
    while (i < n - 1 && (c = fgetc(f)) >= 0) {
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = 0;
    return i ? s : NULL;
}

static char s_fmt[2048];
int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    int n = tgr_vsprintf(s_fmt, fmt, ap);
    fwrite(s_fmt, 1, (size_t)n, f);
    return n;
}
int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt); n = vfprintf(f, fmt, ap); va_end(ap);
    return n;
}
int printf(const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt); n = vfprintf(stdout, fmt, ap); va_end(ap);
    return n;
}
int vsprintf(char *dst, const char *fmt, va_list ap) { return tgr_vsprintf(dst, fmt, ap); }
int vsnprintf(char *dst, size_t cap, const char *fmt, va_list ap)
{
    int n = tgr_vsprintf(s_fmt, fmt, ap);
    if (cap) {
        size_t k = (size_t)n < cap - 1 ? (size_t)n : cap - 1;
        memcpy(dst, s_fmt, k);
        dst[k] = 0;
    }
    return n;
}
int snprintf(char *dst, size_t cap, const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt); n = vsnprintf(dst, cap, fmt, ap); va_end(ap);
    return n;
}
int sprintf(char *dst, const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt); n = tgr_vsprintf(dst, fmt, ap); va_end(ap);
    return n;
}
int sscanf(const char *s, const char *fmt, ...) { (void)s; (void)fmt; return 0; }

/* ---- floating point the compiler's runtime does not give ------------------ */
/* sqrtf correctly rounded, as the N64's sqrt.s and every IEEE host give it */
float sqrtf(float x)
{
    union { float f; uint32_t u; } v;
    uint32_t m, q, r, s, t, bit;
    int e;
    v.f = x;
    if ((v.u & 0x7FFFFFFF) == 0 || (v.u >> 23) == 0xFF)
        return x;                               /* +-0, +inf, NaN */
    if (v.u >> 31) {
        v.u = 0x7FC00000;                       /* the square root of a negative */
        return v.f;
    }
    e = (int)(v.u >> 23);
    m = v.u & 0x7FFFFF;
    if (e == 0) {                               /* subnormal: normalise */
        e = 1;
        while (!(m & 0x800000)) { m <<= 1; e--; }
    } else {
        m |= 0x800000;
    }
    e -= 127;
    if (e & 1) m <<= 1;                          /* an even exponent */
    e >>= 1;
    /* 25 result bits (24 and a rounding bit) by the digit-by-digit method */
    m <<= 1;
    q = s = 0;
    r = m;
    bit = 1u << 24;
    while (bit) {
        t = s + bit;
        if (r >= t) {
            s = t + bit;
            r -= t;
            q += bit;
        }
        r <<= 1;
        bit >>= 1;
    }
    /* q holds 25 bits: the last is the rounding bit, r the sticky rest */
    if (q & 1) {
        if (r || (q & 2)) q += 2;               /* round half to even */
    }
    q >>= 1;
    v.u = (uint32_t)(e + 126) << 23;
    v.u += q;                                   /* q's top bit (bit 23) lifts the exponent */
    return v.f;
}
float fabsf(float x) { union { float f; uint32_t u; } v; v.f = x; v.u &= 0x7FFFFFFF; return v.f; }
double fabs(double x) { return x < 0 ? -x : x; }

/* ---- the rest of what the shared platform files call ---------------------- */
long labs(long x) { return x < 0 ? -x : x; }
ldiv_t ldiv(long a, long b) { ldiv_t r; r.quot = a / b; r.rem = a % b; return r; }
lldiv_t lldiv(long long a, long long b) { lldiv_t r; r.quot = a / b; r.rem = a % b; return r; }

int fgetc(FILE *f)
{
    unsigned char c;
    return fread(&c, 1, 1, f) == 1 ? c : -1;
}

/* PCDRV has no rename: the file's bytes are copied under the new name */
int rename(const char *from, const char *to)
{
    char buf[512];
    int a = pcdrv_open(from, 0), b, n, left;
    if (a < 0)
        return -1;
    if ((b = pcdrv_creat(to)) < 0) {
        pcdrv_close(a);
        return -1;
    }
    left = pcdrv_seek(a, 0, 2);
    pcdrv_seek(a, 0, 0);
    while (left > 0 && (n = pcdrv_read(a, buf, left < (int)sizeof buf ? left : (int)sizeof buf)) > 0) {
        pcdrv_write(b, buf, n);
        left -= n;
    }
    pcdrv_close(a);
    pcdrv_close(b);
    return 0;
}
int remove(const char *path) { (void)path; return -1; }

float fminf(float a, float b) { return a < b ? a : b; }
float fmaxf(float a, float b) { return a > b ? a : b; }
double fmin(double a, double b) { return a < b ? a : b; }
double fmax(double a, double b) { return a > b ? a : b; }
long lroundf(float x) { return x < 0 ? -(long)(-x + 0.5f) : (long)(x + 0.5f); }
double floor(double x) { long long i = (long long)x; return (double)(i - (x < (double)i)); }
float floorf(float x) { int i = (int)x; return (float)(i - (x < (float)i)); }

/* atan and tan for the lens of a view wider than 4:3 (os/view.c): series
 * after range reduction, to well under a float's precision */
double atan(double x)
{
    int neg = x < 0, inv;
    double x2, t, s;
    int k;
    if (neg) x = -x;
    inv = x > 1;
    if (inv) x = 1 / x;
    x = x / (1 + sqrt(1 + x * x));    /* atan(x) = 2 atan(x / (1 + sqrt(1 + x^2))) */
    x2 = x * x;
    t = x;
    s = 0;
    for (k = 1; k < 40; k += 2) {
        s += t / k;
        t *= -x2;
    }
    s *= 2;
    if (inv) s = M_PI / 2 - s;
    return neg ? -s : s;
}
double tan(double x)
{
    double s = 0, c = 0, t, x2;
    int k;
    while (x > M_PI) x -= 2 * M_PI;
    while (x < -M_PI) x += 2 * M_PI;
    x2 = x * x;
    t = x;
    for (k = 1; k < 30; k += 2) { s += t; t *= -x2 / ((k + 1) * (k + 2)); }
    t = 1;
    for (k = 0; k < 30; k += 2) { c += t; t *= -x2 / ((k + 1) * (k + 2)); }
    return s / c;
}
double sqrt(double x)
{
    double r;
    int i;
    if (x <= 0) return 0;
    r = x > 1 ? x : 1;
    for (i = 0; i < 60; i++) r = 0.5 * (r + x / r);
    return r;
}
