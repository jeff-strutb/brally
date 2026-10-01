/* w2c_trace.c -- the memory-access tracer of the traced build (port code).
 *
 * Built only with -DBR_TRACE into build/wasm_trace/brally. Per load/store
 * site (w2c.py's trace_sites.csv) it keeps: how often it ran, the address
 * range it touched, up to NS distinct addresses (a heap address is kept as
 * its allocation's site and offset), and how many of its 4-byte values were
 * addresses. Per allocation site: count and size range. Written to
 * $BR_TRACE_OUT (default build/wasm_trace/trace.bin) at exit, MERGED with
 * what the file already holds, so every scripted run adds to one picture.
 */
#include "w2c_rt.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS 16
#define NV 4

typedef struct {
    u64 cnt;
    u32 lo, hi;                 /* address range */
    u32 nptr, nnz;              /* 4-byte values that were addresses / non-zero */
    u32 ea[NS];                 /* static address, or heap offset */
    u32 asite[NS];              /* allocation site + 1 for a heap address, 0 static */
    u32 val[NV];                /* sample address values */
    u8 nea, nval, width, pad;
} TSite;

typedef struct { u32 base, end, site; } TAlloc;
typedef struct { u64 cnt; u32 lo, hi; } TAllocSite;

extern const unsigned w_trace_nsites;
unsigned w_tcall;
static TSite *g_s;
static TAllocSite *g_as;
static TAlloc *g_live;
static u32 g_nlive, g_caplive;
static pthread_mutex_t g_tl = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t g_rw = PTHREAD_RWLOCK_INITIALIZER;   /* the live-allocation map */

static void tdump(void);

static void tinit(void)
{
    g_s = calloc(w_trace_nsites, sizeof *g_s);
    g_as = calloc(w_trace_nsites, sizeof *g_as);
    atexit(tdump);
}

static TAlloc *find_alloc(u32 ea);

/* an address: inside the image's data, or inside a live allocation */
static int is_addr(u32 v)
{
    int r;
    if (v >= 0x10001000u && v < 0x118F2000u) return 1;      /* the image */
    if (v >= 0x02000000u && v < 0x03000000u) return 1;      /* thread stacks */
    if (v < 0x03000000u || v >= 0xEF000000u) return 0;
    pthread_rwlock_rdlock(&g_rw);
    r = find_alloc(v) != NULL;
    pthread_rwlock_unlock(&g_rw);
    return r;
}

static TAlloc *find_alloc(u32 ea)
{
    u32 lo = 0, hi = g_nlive;

    while (lo < hi) {
        u32 m = (lo + hi) / 2;
        if (g_live[m].end <= ea) lo = m + 1;
        else if (g_live[m].base > ea) hi = m;
        else return &g_live[m];
    }
    return NULL;
}

void w_tmem(unsigned site, u32 ea, u32 bits, int width)
{
    TSite *t;
    int i;

    if (!g_s) {
        pthread_mutex_lock(&g_tl);
        if (!g_s) tinit();
        pthread_mutex_unlock(&g_tl);
    }
    t = &g_s[site];
    if (t->cnt++ == 0) { t->lo = t->hi = ea; t->width = (u8)width; }
    if (ea < t->lo) t->lo = ea;
    if (ea > t->hi) t->hi = ea;
    if (width == 4 && bits) {
        t->nnz++;
        if (is_addr(bits)) {
            t->nptr++;
            if (t->nval < NV) {
                for (i = 0; i < t->nval && t->val[i] != bits; i++) ;
                if (i == t->nval) t->val[t->nval++] = bits;
            }
        }
    }
    if (t->nea < NS) {
        u32 e = ea, as = 0;
        TAlloc *a;
        pthread_mutex_lock(&g_tl);
        pthread_rwlock_rdlock(&g_rw);
        a = find_alloc(ea);
        if (a) { e = ea - a->base; as = a->site + 1; }
        pthread_rwlock_unlock(&g_rw);
        for (i = 0; i < t->nea; i++)
            if (t->ea[i] == e && t->asite[i] == as) break;
        if (i == t->nea && t->nea < NS) { t->ea[t->nea] = e; t->asite[t->nea] = as; t->nea++; }
        pthread_mutex_unlock(&g_tl);
    }
}

void w_talloc(u32 p, u32 n)
{
    u32 lo = 0, hi;
    unsigned site = w_tcall;

    if (!p) return;
    pthread_mutex_lock(&g_tl);
    if (!g_s) tinit();
    pthread_rwlock_wrlock(&g_rw);
    if (g_nlive == g_caplive) {
        g_caplive = g_caplive ? g_caplive * 2 : 4096;
        g_live = realloc(g_live, g_caplive * sizeof *g_live);
    }
    hi = g_nlive;
    while (lo < hi) { u32 m = (lo + hi) / 2; if (g_live[m].base < p) lo = m + 1; else hi = m; }
    memmove(&g_live[lo + 1], &g_live[lo], (g_nlive - lo) * sizeof *g_live);
    g_live[lo].base = p; g_live[lo].end = p + (n ? n : 1); g_live[lo].site = site;
    g_nlive++;
    pthread_rwlock_unlock(&g_rw);
    if (site < w_trace_nsites) {
        TAllocSite *s = &g_as[site];
        if (s->cnt++ == 0 || n < s->lo) s->lo = n;
        if (n > s->hi) s->hi = n;
    }
    pthread_mutex_unlock(&g_tl);
}

void w_tfree(u32 p)
{
    u32 lo = 0, hi;

    if (!p) return;
    pthread_mutex_lock(&g_tl);
    pthread_rwlock_wrlock(&g_rw);
    hi = g_nlive;
    while (lo < hi) { u32 m = (lo + hi) / 2; if (g_live[m].base < p) lo = m + 1; else hi = m; }
    if (lo < g_nlive && g_live[lo].base == p) {
        memmove(&g_live[lo], &g_live[lo + 1], (g_nlive - lo - 1) * sizeof *g_live);
        g_nlive--;
    }
    pthread_rwlock_unlock(&g_rw);
    pthread_mutex_unlock(&g_tl);
}

/* merge into the file: counts add, ranges widen, samples union */
static void tdump(void)
{
    const char *path = getenv("BR_TRACE_OUT");
    FILE *f;
    TSite *old;
    TAllocSite *olda;
    u32 n, k;
    int i, j;

    if (!g_s) return;
    if (!path) path = "build/wasm_trace/trace.bin";
    old = calloc(w_trace_nsites, sizeof *old);
    olda = calloc(w_trace_nsites, sizeof *olda);
    f = fopen(path, "rb");
    if (f) {
        if (fread(&n, 4, 1, f) == 1 && n == w_trace_nsites &&
            fread(old, sizeof *old, n, f) == n && fread(olda, sizeof *olda, n, f) == n) {
            for (k = 0; k < n; k++) {
                TSite *a = &g_s[k], *b = &old[k];
                if (!b->cnt) continue;
                if (!a->cnt) { *a = *b; continue; }
                a->cnt += b->cnt; a->nptr += b->nptr; a->nnz += b->nnz;
                if (b->lo < a->lo) a->lo = b->lo;
                if (b->hi > a->hi) a->hi = b->hi;
                for (i = 0; i < b->nea && a->nea < NS; i++) {
                    for (j = 0; j < a->nea; j++)
                        if (a->ea[j] == b->ea[i] && a->asite[j] == b->asite[i]) break;
                    if (j == a->nea) { a->ea[a->nea] = b->ea[i]; a->asite[a->nea] = b->asite[i]; a->nea++; }
                }
                for (i = 0; i < b->nval && a->nval < NV; i++) {
                    for (j = 0; j < a->nval && a->val[j] != b->val[i]; j++) ;
                    if (j == a->nval) a->val[a->nval++] = b->val[i];
                }
            }
            for (k = 0; k < n; k++) {
                TAllocSite *a = &g_as[k], *b = &olda[k];
                if (!b->cnt) continue;
                if (!a->cnt) { *a = *b; continue; }
                a->cnt += b->cnt;
                if (b->lo < a->lo) a->lo = b->lo;
                if (b->hi > a->hi) a->hi = b->hi;
            }
        }
        fclose(f);
    }
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "trace: cannot write %s\n", path); return; }
    n = w_trace_nsites;
    fwrite(&n, 4, 1, f);
    fwrite(g_s, sizeof *g_s, n, f);
    fwrite(g_as, sizeof *g_as, n, f);
    fclose(f);
    fprintf(stderr, "trace: %u sites written to %s\n", n, path);
}
