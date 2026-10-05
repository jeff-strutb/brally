/* addr.c: the game's memory (the RDRAM arena) and its 32-bit addresses
 * (tgr_addr.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tgr_addr.h"
#include "tgr_syms.h"

/* the N64's RAM, tgr_rdram, is defined with every data symbol of the game
 * as an alias into it at its original address (build/tgrally/gen/arena.s) */

/* ---- windows: native memory outside the arena the game stores an address of */
#define WIN_FIRST 0x20
#define WIN_LAST  0x7F
#define WIN_SPAN  0x1000000u
static uintptr_t s_win[WIN_LAST + 1];

uint32_t tgr_addr32_slow(const void *p)
{
    uintptr_t a = (uintptr_t)p, base = a & ~(uintptr_t)(WIN_SPAN - 1);
    int i;
    if (p == NULL)
        return 0;
    /* a pointer variable or table the port keeps natively: the address the
     * original kept it at (element k of a table is 4 bytes on, not 8) */
    for (i = 0; i < tgr_nnatives; i++) {
        const TgrNat *n = &tgr_natives[i];
        uintptr_t lo = (uintptr_t)n->nat, d = a - lo;
        if (a >= lo && d < n->count * sizeof(void *) && d % sizeof(void *) == 0)
            return n->addr + (uint32_t)(d / sizeof(void *)) * 4;
    }
    for (i = WIN_FIRST; i <= WIN_LAST; i++) {
        if (s_win[i] == base)
            return (uint32_t)i << 24 | (uint32_t)(a - base);
        if (s_win[i] == 0) {
            s_win[i] = base;
            return (uint32_t)i << 24 | (uint32_t)(a - base);
        }
    }
    fprintf(stderr, "tgr: out of address windows for %p\n", p);
    abort();
}

uint8_t tgr_natpage[TGR_RDRAM_SIZE >> 12];

/* mark the pages the native tables' original addresses fall in */
void tgr_addr_init(void)
{
    int i;
    for (i = 0; i < tgr_nnatives; i++) {
        uint32_t lo = tgr_natives[i].addr & (TGR_RDRAM_SIZE - 1);
        uint32_t hi = lo + 4 * tgr_natives[i].count - 1;
        uint32_t pg;
        for (pg = lo >> 12; pg <= hi >> 12; pg++)
            tgr_natpage[pg] = 1;
    }
}

void *tgr_ptr32_slow(uint32_t a)
{
    uint32_t w = a >> 24;
    int i;
    if (a == 0)
        return NULL;
    if (((a & 0xDFFFFFFFu) - 0x80000000u) < TGR_RDRAM_SIZE) {
        uint32_t k = (a & 0xDFFFFFFFu) | 0x80000000u;
        for (i = 0; i < tgr_nnatives; i++) {
            const TgrNat *n = &tgr_natives[i];
            if (k >= n->addr && k - n->addr < 4 * n->count && (k - n->addr) % 4 == 0)
                return &n->nat[(k - n->addr) / 4];
        }
        return tgr_rdram + (a & (TGR_RDRAM_SIZE - 1));
    }
    if (w >= WIN_FIRST && w <= WIN_LAST && s_win[w])
        return (void *)(s_win[w] + (a & (WIN_SPAN - 1)));
    if (a < TGR_RDRAM_SIZE)                 /* a physical address */
        return tgr_rdram + a;
    fprintf(stderr, "tgr: no memory at %08X\n", a);
    return tgr_rdram;                       /* reads zeros rather than faulting */
}

/* ---- game functions by original address */
static int fn_cmp_va(const void *k, const void *e)
{
    uint32_t a = *(const uint32_t *)k, b = ((const TgrFn *)e)->va;
    return a < b ? -1 : a > b;
}

void *tgr_fn(uint32_t a)
{
    const TgrFn *f;
    if (a == 0)
        return NULL;
    f = bsearch(&a, tgr_fns, tgr_nfns, sizeof *tgr_fns, fn_cmp_va);
    if (!f) {
        if ((a >> 24) >= WIN_FIRST && (a >> 24) <= WIN_LAST)
            return tgr_ptr32_slow(a);       /* a native function the game was given */
        fprintf(stderr, "tgr: no function at %08X\n", a);
        abort();
    }
    return f->fn;
}

uint32_t tgr_fnaddr(const void *fn)
{
    int i;
    if (fn == NULL)
        return 0;
    for (i = 0; i < tgr_nfns; i++)
        if (tgr_fns[i].fn == fn)
            return tgr_fns[i].va;
    return tgr_addr32_slow(fn);
}
