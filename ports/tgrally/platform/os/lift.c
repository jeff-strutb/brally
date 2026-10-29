/* lift.c: the game's initialised data, from the cartridge's (tgr_romdata).
 *
 * What the N64's boot code did: the ROM's .data/.rodata (on the USA
 * cartridge ROM 0x70AB0..0xAD400) lands at 0x8026FAB0 and .bss after it is
 * zero; each region's bounds are in gen/tgr_region.h.  Here it lands in
 * the arena; then each symbol's multi-byte units are turned to native order
 * (tools/globals.py's runs) and the pointer variables, which are native
 * objects, are given native pointers for the original addresses the ROM
 * holds in them. */
#include <stdio.h>
#include <string.h>
#include "tgr_addr.h"
#include "tgr_syms.h"
#include "plat.h"

#define DATA_VA   TGR_DATA_VA          /* the region's image (gen/tgr_region.h) */
#define DATA_ROM  TGR_DATA_ROM
#define DATA_END  TGR_DATA_END
#define TEXT_LO   TGR_TEXT_LO

static void swap_unit(uint8_t *p, uint32_t size)
{
    uint8_t t;
    switch (size) {
    case 2: t = p[0]; p[0] = p[1]; p[1] = t; break;
    case 4: t = p[0]; p[0] = p[3]; p[3] = t; t = p[1]; p[1] = p[2]; p[2] = t; break;
    case 8: {
        int i;
        for (i = 0; i < 4; i++) { t = p[i]; p[i] = p[7 - i]; p[7 - i] = t; }
        break;
    }
    }
}

/* bytes of .data already turned native: each byte is swapped once */
static uint8_t s_done[DATA_END - DATA_ROM];

static void swap_runs(uint32_t addr, uint32_t run0, uint32_t nruns)
{
    uint8_t *base = tgr_rdram + (addr & 0x7FFFFF);
    uint32_t j, k, b;
    for (j = 0; j < nruns; j++) {
        const TgrRun *r = &tgr_runs[run0 + j];
        for (k = 0; k < r->count; k++) {
            uint32_t a = addr + r->off + k * r->stride;
            int free = a >= DATA_VA && a + r->size <= DATA_VA + (DATA_END - DATA_ROM);
            for (b = 0; free && b < r->size; b++)
                free = !s_done[a - DATA_VA + b];
            if (!free)
                continue;
            swap_unit(base + r->off + k * r->stride, r->size);
            memset(s_done + (a - DATA_VA), 1, r->size);
        }
    }
}

void tgr_lift(void)
{
    int i;
    uint32_t k;
    const uint8_t *data = tgr_romdata + (DATA_ROM - TGR_ROMDATA_BASE);   /* ROM DATA_ROM on */
    memcpy(tgr_rdram + (DATA_VA & 0x7FFFFF), data, DATA_END - DATA_ROM);
    memset(s_done, 0, sizeof s_done);
    for (i = 0; i < tgr_nsyms; i++)
        swap_runs(tgr_syms[i].addr, tgr_syms[i].run0, tgr_syms[i].nruns);
    for (i = 0; i < tgr_nnatives; i++) {
        const TgrNat *n = &tgr_natives[i];
        if (!n->lifted)
            continue;
        for (k = 0; k < n->count; k++) {
            const uint8_t *b = data + (n->addr - DATA_VA) + 4 * k;
            uint32_t a = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
            if (a >= TEXT_LO && a < DATA_VA) {
                n->nat[k] = tgr_fn(a);
            } else if (a & 0x80000000u) {
                n->nat[k] = tgr_ptr32(a);
                if (n->nruns)                       /* what it points at, if in .data */
                    swap_runs(a, n->run0, n->nruns);
            } else {
                n->nat[k] = (void *)(uintptr_t)a;     /* a number the source types as a pointer */
            }
        }
    }
}

/* The bytes [addr, addr + size) of game memory, just written as the
 * cartridge has them (big-endian), turned native: every unit of a .data or
 * .bss symbol's layout lying wholly inside the range is swapped, and every
 * aligned word of a word-aligned range that no unit covers (a field the
 * port's layout leaves as padding: the game's records are words there).
 * (Europe's and Japan's intro demo copies game variables back out of its
 * recording.) */
static void be_to_native(const TgrSym *t, int n, uint32_t addr, uint32_t size, uint8_t *done)
{
    int i;
    uint32_t j, k, b, end = addr + size;
    for (i = 0; i < n; i++) {
        if (t[i].addr >= end || t[i].addr + t[i].size <= addr)
            continue;
        for (j = 0; j < t[i].nruns; j++) {
            const TgrRun *r = &tgr_runs[t[i].run0 + j];
            for (k = 0; k < r->count; k++) {
                uint32_t u = t[i].addr + r->off + k * r->stride;
                if (u >= addr && u + r->size <= end) {
                    swap_unit(tgr_rdram + (u & 0x7FFFFF), r->size);
                    for (b = 0; b < r->size; b++)
                        done[u - addr + b] = 1;
                }
            }
        }
    }
}

void tgr_be_to_native(uint32_t addr, uint32_t size)
{
    static uint8_t done[0x10000];
    uint32_t w;
    if (size > sizeof done)
        size = sizeof done;
    memset(done, 0, size);
    be_to_native(tgr_syms, tgr_nsyms, addr, size, done);
    be_to_native(tgr_bsyms, tgr_nbsyms, addr, size, done);
    if (size >= 4 && !(addr & 3))
        for (w = 0; w + 4 <= size; w += 4)
            if (!done[w] && !done[w + 1] && !done[w + 2] && !done[w + 3])
                swap_unit(tgr_rdram + ((addr + w) & 0x7FFFFF), 4);
}
