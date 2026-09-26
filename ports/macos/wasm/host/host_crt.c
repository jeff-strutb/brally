/* host_crt.c -- msvcrt.dll as BRGlide.dll imported it, over game memory.
 *
 * FILE objects are MSVC 5's own struct _iobuf, allocated in game memory,
 * because the game's getc() is MSVC's macro: it reads _cnt/_ptr directly
 * and calls _filbuf only to refill. Everything else keeps that buffer state
 * consistent. rand() is MSVC's LCG, so seeded sequences match the original.
 */
#include "host.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

int g_hlog;

/* ================================================================ heap == */
/* Size-class free lists + bump allocation, in the game heap region. Each
 * block has an 8-byte header: [size][magic]. */
#define HEAP_LO 0x03000000u
#define HEAP_HI 0x10000000u
#define HEAP2_LO 0x12000000u
#define HEAP2_HI 0xEF000000u
#define MAGIC 0xB055A11Cu
static pthread_mutex_t g_heaplock = PTHREAD_MUTEX_INITIALIZER;
static u32 g_bump = HEAP_LO, g_bump_hi = HEAP_HI;
static u32 g_free[32];          /* class k: blocks of 16<<k bytes */
static u32 g_bigfree;           /* > 16<<24: first-fit list */

static int cls_of(u32 n) { int k = 0; while ((16u << k) < n) k++; return k; }

u32 hmem_alloc(u32 n, int zero)
{
    u32 a, need = n + 8, cap;
    int k;
    pthread_mutex_lock(&g_heaplock);
    k = cls_of(need);
    if (k < 24) {
        cap = 16u << k;
        a = g_free[k];
        if (a) {
            g_free[k] = H32(a + 8);
        } else {
            if (g_bump + cap > g_bump_hi) {
                if (g_bump_hi == HEAP_HI) { g_bump = HEAP2_LO; g_bump_hi = HEAP2_HI; }
                else { pthread_mutex_unlock(&g_heaplock); return 0; }
            }
            a = g_bump;
            g_bump += cap;
        }
    } else {
        u32 *pp = &g_bigfree, p;
        cap = (need + 0xFFFF) & ~0xFFFFu;
        a = 0;
        while ((p = *pp)) {
            if (H32(p) >= cap) { a = p; *pp = H32(p + 8); cap = H32(p); break; }
            pp = (u32 *)W_P(p + 8);
        }
        if (!a) {
            if (g_bump + cap > g_bump_hi) {
                if (g_bump_hi == HEAP_HI) { g_bump = HEAP2_LO; g_bump_hi = HEAP2_HI; }
                else { pthread_mutex_unlock(&g_heaplock); return 0; }
            }
            a = g_bump;
            g_bump += cap;
        }
    }
    HW32(a, cap);
    HW32(a + 4, MAGIC);
    pthread_mutex_unlock(&g_heaplock);
    if (zero)
        memset(W_P(a + 8), 0, n);
    return a + 8;
}

u32 hmem_size(u32 p) { return p ? H32(p - 8) - 8 : 0; }

void hmem_free(u32 p)
{
    u32 a = p - 8, cap;
    if (!p) return;
    if (H32(a + 4) != MAGIC) { fprintf(stderr, "*** free of non-heap %08X\n", p); return; }
    cap = H32(a);
    HW32(a + 4, 0);
    pthread_mutex_lock(&g_heaplock);
    if (cap < (16u << 24)) {
        int k = cls_of(cap);
        HW32(a + 8, g_free[k]);
        g_free[k] = a;
    } else {
        HW32(a + 8, g_bigfree);
        g_bigfree = a;
    }
    pthread_mutex_unlock(&g_heaplock);
}

u32 hmem_realloc(u32 p, u32 n)
{
    u32 q, old;
    if (!p) return hmem_alloc(n, 0);
    if (!n) { hmem_free(p); return 0; }
    old = hmem_size(p);
    if (n <= old) return p;
    q = hmem_alloc(n, 0);
    if (q) { memcpy(W_P(q), W_P(p), old); hmem_free(p); }
    return q;
}

u32 hstrdup(const char *s)
{
    u32 a = hmem_alloc((u32)strlen(s) + 1, 0);
    strcpy(HS(a), s);
    return a;
}

u32 h_malloc(u32 n) { return hmem_alloc(n, 0); }
u32 h_calloc(u32 n, u32 m) { return hmem_alloc(n * m, 1); }
u32 h_realloc(u32 p, u32 n) { return hmem_realloc(p, n); }
void h_free(u32 p) { hmem_free(p); }
u32 h__Znwm(u32 n) { return hmem_alloc(n ? n : 1, 0); }
void h__ZdlPv(u32 p) { hmem_free(p); }
u32 h_operator_new(u32 n) { return hmem_alloc(n ? n : 1, 0); }
void h_operator_delete(u32 p) { hmem_free(p); }

/* ============================================================= strings == */
u32 h_memcpy(u32 d, u32 s, u32 n) { memmove(W_P(d), W_P(s), n); return d; }
u32 h_memmove(u32 d, u32 s, u32 n) { memmove(W_P(d), W_P(s), n); return d; }
u32 h_memset(u32 d, u32 c, u32 n) { memset(W_P(d), (int)c, n); return d; }
u32 h_memcmp(u32 a, u32 b, u32 n) { return (u32)memcmp(W_P(a), W_P(b), n); }
u32 h_memchr(u32 a, u32 c, u32 n)
{
    u8 *p = memchr(W_P(a), (int)c, n);
    return p ? a + (u32)(p - (u8 *)W_P(a)) : 0;
}
u32 h_strlen(u32 s) { return (u32)strlen(HS(s)); }
u32 h_lstrlenA(u32 s) { return s ? (u32)strlen(HS(s)) : 0; }
u32 h_strcpy(u32 d, u32 s) { memmove(HS(d), HS(s), strlen(HS(s)) + 1); return d; }
u32 h_lstrcpyA(u32 d, u32 s) { return h_strcpy(d, s); }
u32 h_strncpy(u32 d, u32 s, u32 n) { strncpy(HS(d), HS(s), n); return d; }
u32 h_strcat(u32 d, u32 s) { strcat(HS(d), HS(s)); return d; }
u32 h_strcmp(u32 a, u32 b) { return (u32)strcmp(HS(a), HS(b)); }
u32 h_strncmp(u32 a, u32 b, u32 n) { return (u32)strncmp(HS(a), HS(b), n); }
u32 h__stricmp(u32 a, u32 b) { return (u32)strcasecmp(HS(a), HS(b)); }
u32 h_strchr(u32 s, u32 c)
{
    char *p = strchr(HS(s), (int)(c & 0xFF));
    return p ? s + (u32)(p - HS(s)) : 0;
}
u32 h_strrchr(u32 s, u32 c)
{
    char *p = strrchr(HS(s), (int)(c & 0xFF));
    return p ? s + (u32)(p - HS(s)) : 0;
}
u32 h_strstr(u32 s, u32 t)
{
    char *p = strstr(HS(s), HS(t));
    return p ? s + (u32)(p - HS(s)) : 0;
}
u32 h__strupr(u32 s) { char *p; for (p = HS(s); *p; p++) *p = (char)toupper((u8)*p); return s; }
u32 h_toupper(u32 c) { return (u32)toupper((int)c); }
u32 h_atoi(u32 s) { return (u32)atoi(HS(s)); }
u32 h__itoa(u32 v, u32 buf, u32 radix)
{
    char tmp[40];
    u32 u = v;
    int n = 0, neg = 0;
    char *o = HS(buf);
    if (radix == 10 && (s32)v < 0) { neg = 1; u = (u32)(-(s32)v); }
    do { u32 d = u % radix; tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); u /= radix; } while (u);
    if (neg) *o++ = '-';
    while (n) *o++ = tmp[--n];
    *o = 0;
    return buf;
}
static u32 g_errno_a, g_strerr_a;
u32 h__errno(void) { if (!g_errno_a) g_errno_a = hmem_alloc(4, 1); return g_errno_a; }
u32 h_strerror(u32 e)
{
    if (!g_strerr_a) g_strerr_a = hmem_alloc(128, 1);
    snprintf(HS(g_strerr_a), 128, "%s", strerror((int)e));
    return g_strerr_a;
}
u32 h_getenv(u32 s) { (void)s; return 0; }

/* MSVC's rand: per-process seed, LCG 214013/2531011, 15 bits out */
static u32 g_seed = 1;
void h_srand(u32 s) { g_seed = s; }
u32 h_rand(void) { g_seed = g_seed * 214013u + 2531011u; return (g_seed >> 16) & 0x7FFF; }

/* =============================================================== math == */
f64 h_sin(f64 x) { return sin(x); }
f64 h_cos(f64 x) { return cos(x); }
f64 h_tan(f64 x) { return tan(x); }
f64 h_asin(f64 x) { return asin(x); }
f64 h_atan2(f64 y, f64 x) { return atan2(y, x); }
f64 h_pow(f64 x, f64 y) { return pow(x, y); }
f64 h_floor(f64 x) { return floor(x); }
f64 h_sqrt(f64 x) { return sqrt(x); }
u32 h_rint(f64 x) { return (u32)(s32)rint(x); }
u32 h__finite(f64 x) { return isfinite(x) ? 1 : 0; }
u32 h_isfinite(f64 x) { return isfinite(x) ? 1 : 0; }
/* x87 fistp under the default control word: round to nearest, ties even */
u32 h_w_fistp(f64 x) { return (u32)(s32)lrint(x); }
int w_fistp(double x) { return (int)lrint(x); }
u32 h_ftol(void)
{
    /* _ftol takes its operand in st(0), which a C call cannot carry. The
     * tree's C calls to it are casts the compiler already lowered; a call
     * that reaches here is a transcription that named the helper itself. */
    w_missing("_ftol (x87 operand)");
    return 0;
}

/* ========================================================== sort/misc == */
static u32 g_qs_cmp;
static int qs_thunk(const void *a, const void *b)
{
    /* the game's comparator sees GAME pointers */
    u32 ga = (u32)((const u8 *)a - (const u8 *)W_P(0));
    u32 gb = (u32)((const u8 *)b - (const u8 *)W_P(0));
    return (int)w_icall_ii_i(g_qs_cmp, ga, gb);
}
void h_qsort(u32 base, u32 n, u32 sz, u32 cmp)
{
    u32 save = g_qs_cmp;
    g_qs_cmp = cmp;
    qsort(W_P(base), n, sz, qs_thunk);
    g_qs_cmp = save;
}

static u32 g_atexit[64];
static int g_natexit;
u32 h__onexit(u32 fn) { if (g_natexit < 64) g_atexit[g_natexit++] = fn; return fn; }
u32 h__dllonexit(u32 fn, u32 pb, u32 pe) { (void)pb; (void)pe; return h__onexit(fn); }
void h_exit(u32 code)
{
    while (g_natexit) w_icall__i(g_atexit[--g_natexit]);
    fprintf(stderr, "game called exit(%d)\n", (int)code);
    exit((int)code);
}
u32 h__terminate__YAXXZ(void) { w_trap("terminate()"); }
void h_halt_baddata(void) { w_trap("halt_baddata (a Ghidra artefact reached)"); }

/* ========================================================= formatting == */
/* wasm32 varargs: each argument at its natural alignment in a block the
 * caller builds; floats promoted to double (8-aligned), long long 8. */
int hfmt(char *out, size_t cap, const char *f, u32 va)
{
    size_t o = 0;
    char spec[64];
    char tmp[512];
    while (*f) {
        const char *s;
        int k = 0, ll = 0, h = 0, n;
        char conv;
        if (*f != '%') { if (o + 1 < cap) out[o] = *f; o++; f++; continue; }
        s = f++;
        if (*f == '%') { if (o + 1 < cap) out[o] = '%'; o++; f++; continue; }
        while (*f && strchr("-+ #0", *f)) f++;
        if (*f == '*') { f++; }                   /* width from args: rare */
        while (isdigit((u8)*f)) f++;
        if (*f == '.') { f++; if (*f == '*') f++; while (isdigit((u8)*f)) f++; }
        while (*f && strchr("hlLI6432", *f)) {
            if (*f == 'l' && f[1] == 'l') { ll = 1; f++; }
            else if (*f == 'I' && f[1] == '6' && f[2] == '4') { ll = 1; f += 2; }
            else if (*f == 'h') h = 1;
            f++;
        }
        conv = *f;
        if (!conv) break;
        f++;
        k = (int)(f - s);
        if (k >= (int)sizeof spec - 4) k = (int)sizeof spec - 4;
        /* host spec: strip our length modifiers, add our own */
        {
            int j = 0, i;
            for (i = 0; i < k - 1; i++) {
                char c = s[i];
                if (c == 'l' || c == 'h' || c == 'L' || c == 'I' ||
                    (i > 0 && (s[i - 1] == 'I' || s[i - 1] == '6') && (c == '6' || c == '4')))
                    continue;
                spec[j++] = c;
            }
            if (ll && strchr("diuxXo", conv)) { spec[j++] = 'l'; spec[j++] = 'l'; }
            spec[j++] = conv;
            spec[j] = 0;
        }
        switch (conv) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'c':
            if (ll) {
                u64 v;
                va = (va + 7) & ~7u;
                v = W_LD(u64, va, 0); va += 8;
                n = snprintf(tmp, sizeof tmp, spec, v);
            } else {
                u32 v = W_LD(u32, va, 0); va += 4;
                if (h) v = (conv == 'd' || conv == 'i') ? (u32)(s32)(s16)v : (u16)v;
                n = snprintf(tmp, sizeof tmp, spec, (conv == 'd' || conv == 'i') ? (int)v : v);
            }
            break;
        case 'p': {
            u32 v = W_LD(u32, va, 0); va += 4;
            n = snprintf(tmp, sizeof tmp, "%08X", v);
            break;
        }
        case 's': {
            u32 v = W_LD(u32, va, 0); va += 4;
            n = snprintf(tmp, sizeof tmp, spec, v ? HS(v) : "(null)");
            break;
        }
        case 'f': case 'e': case 'E': case 'g': case 'G': {
            f64 v;
            va = (va + 7) & ~7u;
            v = W_LD(f64, va, 0); va += 8;
            n = snprintf(tmp, sizeof tmp, spec, v);
            break;
        }
        case 'n':
            va += 4;
            n = 0;
            break;
        default:
            n = snprintf(tmp, sizeof tmp, "%%%c", conv);
        }
        if (n > (int)sizeof tmp - 1) n = (int)sizeof tmp - 1;
        if (n > 0) {
            int i;
            for (i = 0; i < n; i++) { if (o + 1 < cap) out[o] = tmp[i]; o++; }
        }
    }
    if (cap) out[o < cap ? o : cap - 1] = 0;
    return (int)o;
}

u32 h_sprintf(u32 buf, u32 fmt, u32 va) { return (u32)hfmt(HS(buf), 1 << 20, HS(fmt), va); }
u32 h_vsprintf(u32 buf, u32 fmt, u32 va) { return (u32)hfmt(HS(buf), 1 << 20, HS(fmt), H32(va) ? va : va); }
u32 h_wsprintfA(u32 buf, u32 fmt, u32 va) { return (u32)hfmt(HS(buf), 1024, HS(fmt), va); }
u32 h_snprintf(u32 buf, u32 n, u32 fmt, u32 va, u32 x4, u32 x5, u32 x6)
{
    (void)x4; (void)x5; (void)x6;
    return (u32)hfmt(HS(buf), n, HS(fmt), va);
}
u32 h_printf(u32 fmt, u32 va)
{
    char b[4096];
    int n = hfmt(b, sizeof b, HS(fmt), va);
    fputs(b, stdout);
    fflush(stdout);
    return (u32)n;
}
void h_OutputDebugStringA(u32 s) { if (s && g_hlog) fprintf(stderr, "[dbg] %s", HS(s)); }

/* sscanf: one host sscanf per directive, results written back to game
 * pointers taken from the varargs block */
u32 h_sscanf(u32 str, u32 fmt, u32 va)
{
    const char *s = HS(str), *f = HS(fmt);
    int got = 0;
    while (*f) {
        if (isspace((u8)*f)) { while (isspace((u8)*s)) s++; f++; continue; }
        if (*f != '%') { if (*s != *f) break; s++; f++; continue; }
        {
            char spec[32];
            int j = 0, used = 0, suppress = 0, lng = 0, hh = 0;
            spec[j++] = *f++;
            if (*f == '*') { suppress = 1; spec[j++] = *f++; }
            while (isdigit((u8)*f) && j < 20) spec[j++] = *f++;
            while (*f == 'l' || *f == 'h') { if (*f == 'l') lng = 1; else hh = 1; f++; }
            if (*f == '[') {
                while (*f && *f != ']' && j < 28) spec[j++] = *f++;
                if (*f == ']') spec[j++] = *f++;
            } else {
                spec[j++] = *f++;
            }
            spec[j] = 0;
            strcat(spec, "%n");
            {
                char conv = spec[j - 1];
                int r;
                if (conv == ']' ) conv = 's';
                if (suppress) {
                    r = sscanf(s, spec, &used);
                    if (r < 0 || !used) break;
                } else if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' || conv == 'o') {
                    int v;
                    r = sscanf(s, spec, &v, &used);
                    if (r < 1) break;
                    if (hh) W_ST(u16, H32(va), 0, v); else W_ST(u32, H32(va), 0, v);
                    va += 4; got++;
                } else if (conv == 'f' || conv == 'e' || conv == 'g') {
                    char sp2[32];
                    snprintf(sp2, sizeof sp2, "%s", spec);
                    if (lng) {
                        double v; char *p = strchr(sp2, conv);
                        memmove(p + 1, p, strlen(p) + 1); *p = 'l';
                        r = sscanf(s, sp2, &v, &used);
                        if (r < 1) break;
                        W_ST(f64, H32(va), 0, v);
                    } else {
                        float v;
                        r = sscanf(s, sp2, &v, &used);
                        if (r < 1) break;
                        W_ST(f32, H32(va), 0, v);
                    }
                    va += 4; got++;
                } else if (conv == 's' || conv == 'c') {
                    char buf[1024];
                    r = sscanf(s, spec, buf, &used);
                    if (r < 1) break;
                    if (conv == 's' || spec[j - 1] == ']') strcpy(HS(H32(va)), buf);
                    else *HS(H32(va)) = buf[0];
                    va += 4; got++;
                } else {
                    break;
                }
                s += used;
            }
        }
    }
    return (u32)got;
}

/* ================================================================ VFS == */
static char g_disc[1024], g_save[1024];
static char g_cwd[260] = "C:\\BOSSRALLY\\";
static int g_drive = 3;       /* 1=A, 3=C */

void vfs_init(const char *disc, const char *save)
{
    snprintf(g_disc, sizeof g_disc, "%s", disc);
    snprintf(g_save, sizeof g_save, "%s", save);
    mkdir(g_save, 0755);
}

const char *vfs_cwd(void) { return g_cwd; }

/* game path -> canonical "X:\A\B" (uppercase drive, backslashes, no ..) */
static void canon(const char *p, char *out, size_t n)
{
    char buf[520], *parts[64];
    int np = 0, i;
    char drive;
    const char *rest;
    char *q, *tok;
    if (p[0] && p[1] == ':') {
        drive = (char)toupper((u8)p[0]);
        rest = p + 2;
        if (*rest != '\\' && *rest != '/') {
            if (drive == g_cwd[0]) snprintf(buf, sizeof buf, "%s\\%s", g_cwd + 2, rest);
            else snprintf(buf, sizeof buf, "\\%s", rest);
        } else snprintf(buf, sizeof buf, "%s", rest);
    } else if (p[0] == '\\' || p[0] == '/') {
        drive = g_cwd[0];
        snprintf(buf, sizeof buf, "%s", p);
    } else {
        drive = g_cwd[0];
        snprintf(buf, sizeof buf, "%s\\%s", g_cwd + 2, p);
    }
    for (q = buf; *q; q++) if (*q == '/') *q = '\\';
    for (tok = strtok(buf, "\\"); tok; tok = strtok(NULL, "\\")) {
        if (!strcmp(tok, ".") || !*tok) continue;
        if (!strcmp(tok, "..")) { if (np) np--; continue; }
        if (np < 64) parts[np++] = tok;
    }
    snprintf(out, n, "%c:", drive);
    for (i = 0; i < np; i++) {
        strncat(out, "\\", n - strlen(out) - 1);
        strncat(out, parts[i], n - strlen(out) - 1);
    }
    if (np == 0) strncat(out, "\\", n - strlen(out) - 1);
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
        DIR *d;
        struct dirent *de;
        int found = 0;
        if (l >= sizeof comp) return 0;
        memcpy(comp, r, l);
        comp[l] = 0;
        d = opendir(cur);
        if (!d) return 0;
        while ((de = readdir(d))) {
            if (!strcasecmp(de->d_name, comp)) {
                size_t cl = strlen(cur);
                snprintf(cur + cl, sizeof cur - cl, "/%s", de->d_name);
                found = 1;
                break;
            }
        }
        closedir(d);
        if (!found) return 0;
        r += l;
        if (*r == '\\') r++;
    }
    snprintf(out, n, "%s", cur);
    return 1;
}

/* rel path under the drive root, lowercase-insensitive; C:\BOSSRALLY\x and
 * D:\x both mean disc x (the installer only copies disc files). */
static const char *drive_rel(const char *c)
{
    if (c[0] == 'C' && !strncasecmp(c + 2, "\\BOSSRALLY", 10))
        return c[12] == '\\' ? c + 13 : c + 12;
    if (c[0] == 'D') return c[2] == '\\' ? c + 3 : c + 2;
    return NULL;
}

int vfs_resolve(const char *gp, char *host, size_t n, int forwrite)
{
    char c[520], flat[520], *q;
    const char *rel;
    canon(gp, c, sizeof c);
    rel = drive_rel(c);
    if (!rel) return 0;
    /* the overlay holds anything the game wrote, flattened by path */
    snprintf(flat, sizeof flat, "%s", rel);
    for (q = flat; *q; q++) { if (*q == '\\') *q = '_'; else *q = (char)tolower((u8)*q); }
    snprintf(host, n, "%s/%s", g_save, flat);
    if (forwrite) return 1;
    if (access(host, R_OK) == 0) return 1;
    if (!*rel) { snprintf(host, n, "%s", g_disc); return 1; }
    return ci_find(g_disc, rel, host, n);
}

int vfs_chdir(const char *p)
{
    char c[520], h[1024];
    canon(p, c, sizeof c);
    if (!drive_rel(c)) return -1;
    if (!vfs_resolve(c, h, sizeof h, 0)) return -1;
    snprintf(g_cwd, sizeof g_cwd, "%s", c);
    return 0;
}

u32 h__chdir(u32 p) { return (u32)vfs_chdir(HS(p)); }
u32 h__chdrive(u32 d)
{
    if (d != 3 && d != 4) return (u32)-1;
    g_drive = (int)d;
    snprintf(g_cwd, sizeof g_cwd, d == 3 ? "C:\\BOSSRALLY" : "D:\\");
    return 0;
}
u32 h__getdrive(void) { return (u32)(g_cwd[0] - 'A' + 1); }
u32 h__getcwd(u32 buf, u32 n)
{
    if (!buf) buf = hmem_alloc(n ? n : 260, 0);
    snprintf(HS(buf), n ? n : 260, "%s", g_cwd);
    return buf;
}

/* ============================================================== stdio == */
/* MSVC 5 struct _iobuf, 32 bytes */
enum { F_PTR = 0, F_CNT = 4, F_BASE = 8, F_FLAG = 12, F_FILE = 16, F_CHARBUF = 20,
       F_BUFSIZ = 24, F_TMP = 28 };
#define IOREAD 0x01
#define IOWRT 0x02
#define IOEOF 0x10
#define IOERR 0x20
#define BUFSZ 4096
static FILE *g_files[256];

static FILE *hf(u32 fp)
{
    u32 i = H32(fp + F_FILE);
    return (i < 256) ? g_files[i] : NULL;
}

u32 h_fopen(u32 name, u32 mode)
{
    char host[1024];
    const char *m = HS(mode);
    int w = strchr(m, 'w') || strchr(m, 'a') || strchr(m, '+');
    FILE *f;
    u32 fp;
    int i;
    if (!vfs_resolve(HS(name), host, sizeof host, w)) {
        HLOG("fopen(%s, %s) -> not found\n", HS(name), m);
        return 0;
    }
    if (w && strchr(m, '+') && access(host, R_OK)) {
        /* r+ on a disc file: copy it into the overlay first */
        char src[1024];
        if (vfs_resolve(HS(name), src, sizeof src, 0) && strcmp(src, host)) {
            FILE *a = fopen(src, "rb"), *b = fopen(host, "wb");
            char buf[8192];
            size_t k;
            while (a && b && (k = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, k, b);
            if (a) fclose(a);
            if (b) fclose(b);
        }
    }
    f = fopen(host, m);
    HLOG("fopen(%s, %s) -> %s%s\n", HS(name), m, host, f ? "" : " FAILED");
    if (!f) return 0;
    for (i = 3; i < 256 && g_files[i]; i++) ;
    if (i == 256) { fclose(f); return 0; }
    g_files[i] = f;
    fp = hmem_alloc(32, 1);
    HW32(fp + F_FILE, i);
    HW32(fp + F_FLAG, w ? IOWRT : IOREAD);
    HW32(fp + F_BASE, hmem_alloc(BUFSZ, 0));
    HW32(fp + F_BUFSIZ, BUFSZ);
    HW32(fp + F_PTR, H32(fp + F_BASE));
    HW32(fp + F_CNT, 0);
    return fp;
}

u32 h_fclose(u32 fp)
{
    FILE *f;
    if (!fp) return (u32)-1;
    f = hf(fp);
    if (!f) return (u32)-1;
    fclose(f);
    g_files[H32(fp + F_FILE)] = NULL;
    hmem_free(H32(fp + F_BASE));
    hmem_free(fp);
    return 0;
}

/* refill; returns the first byte (consumed) or -1 */
u32 h__filbuf(u32 fp)
{
    FILE *f = hf(fp);
    size_t n;
    u32 base = H32(fp + F_BASE);
    if (!f) return (u32)-1;
    n = fread(W_P(base), 1, BUFSZ, f);
    if (!n) {
        HW32(fp + F_FLAG, H32(fp + F_FLAG) | IOEOF);
        HW32(fp + F_CNT, 0);
        return (u32)-1;
    }
    HW32(fp + F_PTR, base + 1);
    HW32(fp + F_CNT, (u32)n - 1);
    return *(u8 *)W_P(base);
}

u32 h_getc(u32 fp)
{
    s32 cnt = (s32)H32(fp + F_CNT) - 1;
    if (cnt >= 0) {
        u32 p = H32(fp + F_PTR);
        HW32(fp + F_CNT, cnt);
        HW32(fp + F_PTR, p + 1);
        return *(u8 *)W_P(p);
    }
    return h__filbuf(fp);
}

u32 h_ungetc(u32 c, u32 fp)
{
    u32 p = H32(fp + F_PTR), base = H32(fp + F_BASE);
    if ((s32)c == -1) return c;
    if (p > base) {
        HW32(fp + F_PTR, p - 1);
        *(u8 *)W_P(p - 1) = (u8)c;
    } else {
        /* buffer empty at its start: seek the host file back instead */
        FILE *f = hf(fp);
        if (f) fseek(f, -1 - (long)(s32)H32(fp + F_CNT), SEEK_CUR);
        HW32(fp + F_CNT, 0);
        return c;
    }
    HW32(fp + F_CNT, H32(fp + F_CNT) + 1);
    HW32(fp + F_FLAG, H32(fp + F_FLAG) & ~IOEOF);
    return c & 0xFF;
}

u32 h_fread(u32 buf, u32 sz, u32 n, u32 fp)
{
    FILE *f = hf(fp);
    u32 want = sz * n, got = 0;
    s32 cnt;
    if (!f || !want) return 0;
    cnt = (s32)H32(fp + F_CNT);
    if (cnt > 0) {
        u32 k = (u32)cnt < want ? (u32)cnt : want;
        memcpy(W_P(buf), W_P(H32(fp + F_PTR)), k);
        HW32(fp + F_PTR, H32(fp + F_PTR) + k);
        HW32(fp + F_CNT, cnt - k);
        got = k;
    }
    if (got < want)
        got += (u32)fread(W_P(buf + got), 1, want - got, f);
    if (got < want) HW32(fp + F_FLAG, H32(fp + F_FLAG) | IOEOF);
    return sz ? got / sz : 0;
}

u32 h_fwrite(u32 buf, u32 sz, u32 n, u32 fp)
{
    FILE *f = hf(fp);
    if (!f) return 0;
    return (u32)fwrite(W_P(buf), sz, n, f);
}

u32 h_ftell(u32 fp)
{
    FILE *f = hf(fp);
    if (!f) return (u32)-1;
    return (u32)(ftell(f) - (long)(s32)H32(fp + F_CNT));
}

u32 h_fseek(u32 fp, u32 off, u32 whence)
{
    FILE *f = hf(fp);
    long o = (long)(s32)off;
    if (!f) return (u32)-1;
    if (whence == SEEK_CUR) o -= (long)(s32)H32(fp + F_CNT);
    HW32(fp + F_CNT, 0);
    HW32(fp + F_PTR, H32(fp + F_BASE));
    HW32(fp + F_FLAG, H32(fp + F_FLAG) & ~IOEOF);
    return fseek(f, o, (int)whence) ? (u32)-1 : 0;
}

u32 h_fprintf(u32 fp, u32 fmt, u32 va)
{
    char b[8192];
    int n = hfmt(b, sizeof b, HS(fmt), va);
    FILE *f = fp ? hf(fp) : NULL;
    if (!f || (H32(fp + F_FILE) < 3)) { fputs(b, stderr); return (u32)n; }
    fputs(b, f);
    return (u32)n;
}

/* ======================================================== _findfirst == */
/* struct _finddata_t: attrib, time_create, time_access, time_write, size,
 * name[260] */
typedef struct { DIR *d; char dir[1024]; char pat[260]; int used; } hfind;
static hfind g_find[16];

static int wild(const char *p, const char *s)
{
    if (!*p) return !*s;
    if (*p == '*') return wild(p + 1, s) || (*s && wild(p, s + 1));
    if (*p == '?') return *s && wild(p + 1, s + 1);
    return tolower((u8)*p) == tolower((u8)*s) && wild(p + 1, s + 1);
}

static int find_next(int h, u32 fd)
{
    struct dirent *de;
    while ((de = readdir(g_find[h].d))) {
        char full[1300];
        struct stat st;
        if (de->d_name[0] == '.') continue;
        if (!wild(g_find[h].pat, de->d_name)) continue;
        snprintf(full, sizeof full, "%s/%s", g_find[h].dir, de->d_name);
        if (stat(full, &st)) continue;
        memset(W_P(fd), 0, 280);
        HW32(fd, S_ISDIR(st.st_mode) ? 0x10 : 0x01);   /* _A_SUBDIR / _A_RDONLY */
        HW32(fd + 12, (u32)st.st_mtime);
        HW32(fd + 16, (u32)st.st_size);
        snprintf(HS(fd + 20), 260, "%s", de->d_name);
        return 0;
    }
    return -1;
}

u32 h__findfirst(u32 spec, u32 fd)
{
    char c[520], host[1024], dirpart[520];
    char *bs;
    int h;
    canon(HS(spec), c, sizeof c);
    bs = strrchr(c, '\\');
    if (!bs) return (u32)-1;
    memcpy(dirpart, c, (size_t)(bs - c));
    dirpart[bs - c] = 0;
    if (!vfs_resolve(dirpart[2] ? dirpart : "D:\\", host, sizeof host, 0)) return (u32)-1;
    for (h = 0; h < 16 && g_find[h].used; h++) ;
    if (h == 16) return (u32)-1;
    g_find[h].d = opendir(host);
    if (!g_find[h].d) return (u32)-1;
    g_find[h].used = 1;
    snprintf(g_find[h].dir, sizeof g_find[h].dir, "%s", host);
    snprintf(g_find[h].pat, sizeof g_find[h].pat, "%s", bs + 1);
    if (find_next(h, fd)) { closedir(g_find[h].d); g_find[h].used = 0; return (u32)-1; }
    return (u32)(h + 1);
}
u32 h__findnext(u32 h, u32 fd)
{
    if (h < 1 || h > 16 || !g_find[h - 1].used) return (u32)-1;
    return (u32)find_next((int)h - 1, fd);
}
u32 h__findclose(u32 h)
{
    if (h < 1 || h > 16 || !g_find[h - 1].used) return (u32)-1;
    closedir(g_find[h - 1].d);
    g_find[h - 1].used = 0;
    return 0;
}
