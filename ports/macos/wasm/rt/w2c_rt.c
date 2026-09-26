/* w2c_rt.c -- the runtime under the translated game (port code, not decomp).
 *
 *   * reserves the game's 4 GB address space at W_BASE (PROT_NONE outside
 *     what is mapped, first 64 KB never mapped so NULL faults);
 *   * loads the ORIGINAL BRGlide.dll's sections at their original virtual
 *     addresses -- the game's globals, constant tables and function-pointer
 *     tables are that data, exactly as shipped;
 *   * lays the port's own data (string literals, port-only globals) below
 *     the image;
 *   * builds the dispatch table: every function with a game address, keyed
 *     by that address, so indirect calls through original data land on the
 *     translated function the verified build placed there.
 *
 * Memory map (game addresses):
 *   0x00000000-0x0000FFFF  unmapped (NULL guard)
 *   0x00010000-0x00EFFFFF  port data (w2c layout)
 *   0x00F00000-0x01FFFFFF  orphans: extern data the tree names but the build
 *                          never resolved (4 KB of zeros each)
 *   0x02000000-0x02FFFFFF  thread shadow stacks (1 MB each; main at the top)
 *   0x03000000-0x0FFFFFFF  heap (host_mem.c)
 *   0x10000000-0x118FFFFF  the original image, at its image base
 *   0x12000000-0xEFFFFFFF  heap overflow
 *   0xF0000000-            synthetic function addresses (never memory)
 */
#include "w2c_rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <execinfo.h>
#include <pthread.h>

_Thread_local u32 w_sp;
int w_tracing;

void w_trace(const char *fn)
{
    static const char *last;
    static unsigned rep;
    if (fn == last) { rep++; return; }
    if (rep) fprintf(stderr, "  (x%u)\n", rep + 1);
    rep = 0;
    last = fn;
    fprintf(stderr, "> %s\n", fn);
}

#define TEXT_LO 0x10001000u
#define TEXT_HI 0x10077000u
#define SYN_LO  0xF0000000u
#define SYN_MAX 65536

static const w_fentry **g_text;          /* (va - TEXT_LO) -> entry */
static const w_fentry *g_syn[SYN_MAX];   /* (va - SYN_LO) / 8 */
static w_fentry g_hostent[SYN_MAX];
static u32 g_nhost;
static u32 g_syn_next = 0xF0040000u;     /* host-registered: upper half */
static pthread_mutex_t g_reglock = PTHREAD_MUTEX_INITIALIZER;

_Noreturn void w_trap(const char *why)
{
    void *bt[64];
    int n = backtrace(bt, 64);
    fprintf(stderr, "\n*** trap: %s\n", why);
    backtrace_symbols_fd(bt, n, 2);
    abort();
}

void w_missing(const char *import)
{
    fprintf(stderr, "*** host import not implemented: %s\n", import);
    if (getenv("BR_MISSING_FATAL"))
        w_trap(import);
}

void w_missing_game(u32 va, const char *name)
{
    static u32 seen[4096];
    static int nseen;
    int i;
    for (i = 0; i < nseen; i++)
        if (seen[i] == va) return;
    if (nseen < 4096) seen[nseen++] = va;
    fprintf(stderr, "*** game function not in the tree: 0x%08X %s\n", va, name);
    if (getenv("BR_MISSING_FATAL"))
        w_trap(name);
}

u32 w_memory_size(void) { return 65536; }
u32 w_memory_grow(u32 pages) { (void)pages; return 0xFFFFFFFFu; }

_Thread_local const w_fentry *w_last;   /* for stubs that name themselves */

const w_fentry *w_lookup(u32 a)
{
    const w_fentry *e = 0;
    if (a >= TEXT_LO && a < TEXT_HI)
        e = g_text[a - TEXT_LO];
    else if (a >= SYN_LO && ((a - SYN_LO) >> 3) < SYN_MAX)
        e = g_syn[(a - SYN_LO) >> 3];
    if (!e) {
        char buf[96];
        snprintf(buf, sizeof buf, "indirect call to 0x%08X: no function there", a);
        w_trap(buf);
    }
    w_last = e;
    return e;
}

/* A host function handed to the game as a pointer (COM vtables, callbacks
 * the host implements, GetProcAddress results) gets a synthetic address. */
u32 w_addr_of_host(void *fn, const char *sig, const char *name)
{
    u32 i, a;
    pthread_mutex_lock(&g_reglock);
    for (i = 0; i < g_nhost; i++)
        if (g_hostent[i].fn == fn &&
            (!name || !g_hostent[i].name || !strcmp(g_hostent[i].name, name))) {
            a = g_hostent[i].va;
            pthread_mutex_unlock(&g_reglock);
            return a;
        }
    if (!sig) {
        const w_hentry *h;
        for (h = w_host_functions; h->name; h++)
            if (h->fn == fn) { sig = h->sig; name = h->name; break; }
    }
    if (!sig) {
        fprintf(stderr, "w_addr_of_host: no signature for %s\n", name ? name : "?");
        abort();
    }
    a = g_syn_next;
    g_syn_next += 8;
    g_hostent[g_nhost] = (w_fentry){ a, sig, fn, name, 0 };
    g_syn[(a - SYN_LO) >> 3] = &g_hostent[g_nhost];
    g_nhost++;
    pthread_mutex_unlock(&g_reglock);
    return a;
}

u64 w_generic_call(const w_fentry *e, const u32 *w, int n, const char *sig)
{
    const w_gcentry *g;
    (void)n;
    for (g = w_gcalls; g->sig; g++)
        if (!strcmp(g->sig, e->sig))
            break;
    if (!g->sig) {
        char buf[160];
        snprintf(buf, sizeof buf, "no generic caller for %s (%s), called as %s",
                 e->name, e->sig, sig);
        w_trap(buf);
    }
    if (getenv("BR_SIGTRACE"))
        fprintf(stderr, "sig bridge: %s is %s, called as %s\n", e->name, e->sig, sig);
    return g->call(e->fn, w);
}

/* An indirect call whose two sides disagree: lay the caller's arguments out
 * as the x86 machine would -- ecx, edx and stack words, by the CALLER's
 * convention -- then read the callee's parameters out of that frame by the
 * CALLEE's convention. This is what made such calls work on Windows. */
/* Words a callee-cleanup call left on the x86 stack because its callee
 * popped fewer than were pushed; the next call short of arguments reads them,
 * as the real stack would hand them over. The original relies on it:
 * M8A90 (0x10008A90) is `return v7(v3(a, b))` where v3 pops one word, so v7
 * (0x10008A30, `ret 8`) receives (result, b). */
static _Thread_local u32 g_lo[16];
static _Thread_local int g_nlo;

u64 w_x86_call(const w_fentry *e, u32 ccc, const char *csig, const u32 *w)
{
    u32 ecx = 0, edx = 0, stk[64] = { 0 }, out[64] = { 0 };
    int ns = 0, wi = 0, ri = 0, idx = 0, no = 0, si = 0;
    int cconv = (int)(ccc >> 20) & 3, econv = (int)(e->cc >> 20) & 3;
    u32 cmask = ccc & 0xFFFFF, emask = e->cc & 0xFFFFF;
    const char *p;
    const w_gcentry *g;
    for (p = csig; *p && *p != '_'; p++, idx++) {
        int nw = (*p == 'I' || *p == 'F') ? 2 : 1, k;
        if (cconv == 1 && idx == 0) ecx = w[wi];
        else if (cconv == 2 && *p == 'i' && (cmask >> idx & 1) && ri < 2) {
            if (ri++ == 0) ecx = w[wi]; else edx = w[wi];
        } else
            for (k = 0; k < nw && ns < 64; k++) stk[ns++] = w[wi + k];
        wi += nw;
    }
    ri = 0;
    idx = 0;
    for (p = e->sig; *p && *p != '_'; p++, idx++) {
        int nw = (*p == 'I' || *p == 'F') ? 2 : 1, k;
        if (econv == 1 && idx == 0) out[no++] = ecx;
        else if (econv == 2 && *p == 'i' && (emask >> idx & 1) && ri < 2)
            out[no++] = ri++ == 0 ? ecx : edx;
        else
            for (k = 0; k < nw && no < 64; k++) {
                if (si < ns) out[no++] = stk[si++];
                else if (si - ns < g_nlo) out[no++] = g_lo[si++ - ns];
                else { out[no++] = 0; si++; }
            }
    }
    /* what the callee did not pop stays above for the next call */
    if (cconv != 0 && si < ns) {
        int k;
        g_nlo = 0;
        for (k = si; k < ns && g_nlo < 16; k++) g_lo[g_nlo++] = stk[k];
    } else if (si > ns) {
        int used = si - ns, k;
        for (k = 0; k + used < g_nlo; k++) g_lo[k] = g_lo[k + used];
        g_nlo = g_nlo > used ? g_nlo - used : 0;
    }
    if (getenv("BR_SIGTRACE"))
        fprintf(stderr, "x86 bridge: %s (%s cc %X) called as %s cc %X\n",
                e->name, e->sig, e->cc, csig, ccc);
    for (g = w_gcalls; g->sig; g++)
        if (!strcmp(g->sig, e->sig))
            return g->call(e->fn, out);
    {
        char buf[160];
        snprintf(buf, sizeof buf, "no generic caller for %s (%s)", e->name, e->sig);
        w_trap(buf);
    }
}

/* ---- image loading ---- */
static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }

static void load_image(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    u8 *d;
    u32 pe, nsec, optsz, base, i;
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc(n);
    if (fread(d, 1, n, f) != (size_t)n) { fprintf(stderr, "short read %s\n", path); exit(1); }
    fclose(f);
    pe = rd32(d + 0x3C);
    nsec = rd16(d + pe + 6);
    optsz = rd16(d + pe + 20);
    base = rd32(d + pe + 24 + 28);
    if (base != 0x10000000u) { fprintf(stderr, "%s: image base %08X\n", path, base); exit(1); }
    memcpy(W_P(base), d, 0x1000);                      /* headers */
    for (i = 0; i < nsec; i++) {
        const u8 *s = d + pe + 24 + optsz + i * 40;
        u32 vsz = rd32(s + 8), rva = rd32(s + 12), rsz = rd32(s + 16), rp = rd32(s + 20);
        memset(W_P(base + rva), 0, vsz > rsz ? vsz : rsz);
        memcpy(W_P(base + rva), d + rp, rsz < vsz || !vsz ? rsz : vsz);
    }
    free(d);
}

static void load_port_data(const char *blob)
{
    FILE *f = fopen(blob, "rb");
    long n;
    u8 *d;
    const w_segentry *s;
    const w_hostfix *h;
    if (!f) { fprintf(stderr, "cannot open %s\n", blob); exit(1); }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc(n ? n : 1);
    if (n && fread(d, 1, n, f) != (size_t)n) exit(1);
    fclose(f);
    for (s = w_port_segments; s->len; s++)
        memcpy(W_P(s->addr), d + s->off, s->len);
    free(d);
    for (h = w_port_hostfix; h->addr; h++)
        W_ST(u32, h->addr, 0, w_addr_of_host(h->fn, 0, 0));
}

const w_fentry *w_lookup_quiet(u32 a)
{
    return (a >= TEXT_LO && a < TEXT_HI) ? g_text[a - TEXT_LO] : 0;
}

/* a port-side stand-in replaces the function at va for indirect calls */
static w_fentry g_over[64];
static int g_nover;
void w_override(u32 va, void *fn, const char *sig, const char *name)
{
    if (va < TEXT_LO || va >= TEXT_HI || g_nover >= 64) return;
    g_over[g_nover] = (w_fentry){ va, sig, fn, name, 0 };
    g_text[va - TEXT_LO] = &g_over[g_nover++];
}

void w_init(const char *dll, const char *portdata)
{
    const w_fentry *e;
    void *want = (void *)(uintptr_t)W_BASE;
    void *m = mmap(want, 0x200000000ULL, PROT_NONE,
                   MAP_PRIVATE | MAP_ANON | MAP_NORESERVE | MAP_FIXED, -1, 0);
    if (m != want) { perror("mmap game address space"); exit(1); }
    /* everything but the NULL guard is ordinary memory */
    if (mprotect(W_P(0x10000), 0xFFFF0000ULL, PROT_READ | PROT_WRITE)) {
        perror("mprotect"); exit(1);
    }
    load_image(dll);
    load_port_data(portdata);
    {   /* the original IAT: each slot holds the host import's address */
        const w_iatent *i;
        const w_hentry *h;
        for (i = w_iat; i->slot; i++) {
            u32 a = 0;
            for (h = w_host_functions; h->name; h++)
                if (!strcmp(h->name, i->name)) { a = w_addr_of_host(h->fn, h->sig, h->name); break; }
            W_ST(u32, i->slot, 0, a);
        }
    }
    g_text = calloc(TEXT_HI - TEXT_LO, sizeof *g_text);
    for (e = w_functions; e->fn; e++) {
        if (e->va >= TEXT_LO && e->va < TEXT_HI)
            g_text[e->va - TEXT_LO] = e;
        else if (e->va >= SYN_LO)
            g_syn[(e->va - SYN_LO) >> 3] = e;
    }
    w_sp = 0x03000000u - 16;             /* main thread: top of the stacks */
    w_tracing = getenv("BR_TRACE") != NULL;
}
