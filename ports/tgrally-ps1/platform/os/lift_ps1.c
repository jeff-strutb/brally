/* lift_ps1.c: ports/tgrally/platform/os/lift.c with the data read from the
 * cartridge file, not from memory.  lift.c: the game's initialised data, from the cartridge's (tgr_romdata).
 *
 * What the N64's boot code did: the ROM's .data/.rodata (ROM 0x70AB0..
 * 0xAD400) lands at 0x8026FAB0 and .bss after it is zero.  Here it lands in
 * the arena; then each symbol's multi-byte units are turned to native order
 * (tools/globals.py's runs) and the pointer variables, which are native
 * objects, are given native pointers for the original addresses the ROM
 * holds in them. */
#include <stdio.h>
#include <string.h>
#include "tgr_addr.h"
#include "tgr_syms.h"
#include "plat.h"

#define DATA_VA   0x8026FAB0u
#define DATA_ROM  0x70AB0u
#define DATA_END  0xAD400u
#define TEXT_LO   0x80200000u

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
    tgr_rom_read(DATA_ROM, tgr_rdram + (DATA_VA & 0x7FFFFF), DATA_END - DATA_ROM);
    memset(s_done, 0, sizeof s_done);
    for (i = 0; i < tgr_nsyms; i++)
        swap_runs(tgr_syms[i].addr, tgr_syms[i].run0, tgr_syms[i].nruns);
    for (i = 0; i < tgr_nnatives; i++) {
        const TgrNat *n = &tgr_natives[i];
        if (!n->lifted)
            continue;
        for (k = 0; k < n->count; k++) {
            uint8_t b[4];
            uint32_t a;
            tgr_rom_read(DATA_ROM + (n->addr - DATA_VA) + 4 * k, b, 4);     /* the ROM's own bytes */
            a = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
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
