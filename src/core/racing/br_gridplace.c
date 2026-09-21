/* br_gridplace.c -- 0x1005F310, driver-record constructor / grid placement.
 *
 * thiscall on the 0x80-byte driver slot.  A callee of the race step
 * 0x10019A70 (br_racestep.h hole BR_RS_HOLE_GRID).  Matching build only.
 *
 * RESIDUE 2026-09-12: 541/538 B, 171/170 insns, REGNORM 8+7.  Head is the
 * mode cascade: orig `sub eax,edi / je` (edi is the live zero) vs our
 * `cmp eax,edi / je`; `sub eax,5` vs `add eax,-5`.  Two int-to-float
 * sites are `fild [esp]` in orig.
 */
#ifdef BR_MATCHING_BUILD

#include <stdint.h>
#include "br_match.h"

extern int32_t g_brRaceMode;             /* 0x100A9360 */
extern int32_t DAT_100b3858;
extern int32_t DAT_100b2f04;
extern int32_t DAT_100b2f00;
extern int32_t DAT_100b3014;
extern int32_t DAT_104b15e8;
extern int32_t DAT_10af206c;
extern uint8_t DAT_100b2fd8[];
extern uint8_t DAT_10af07f8[];
extern uint8_t DAT_10af1208[];
extern int32_t DAT_106eed48;
extern int32_t DAT_10b1ce98, DAT_10b1ce9c, DAT_10b1cea0, DAT_10b1cea4;
extern int32_t DAT_10b1cbec, DAT_10af07f0, DAT_10b1ca20;
extern int32_t *DAT_100bcab0[];

int  BrPathWalk(int32_t, float);
int  BrPodNop();
void FUN_1005edc0(void *);

#define DW(p, o) (*(int32_t *)((uint8_t *)(p) + (o)))
#define FL(p, o) (*(float *)((uint8_t *)(p) + (o)))
#define BY(p, o) (*(uint8_t *)((uint8_t *)(p) + (o)))

/* WHAT IT DOES: construct one driver's 0x80-byte race slot and, if the
 * slot is a phantom (index past the live-car cap), place it on the grid
 * by walking the path to a mode-dependent arc, copy the start pose, and
 * pick a car-data row.  Live slots are wired to their car record at
 * 0x10AF1208 + i*0x2B68. */
/* RESIDUE (2026-09-20): 541/538 B, 171/170 insns, 8 regions, regnorm 8+7.
 * All codegen: the mode dispatch (original chains destructive `sub eax,N; je`,
 * this build emits one `add eax,-5; test; je` -- inline compound `-=` moved
 * nothing), an int->float fild placement, and a shl-vs-lea index scaling.
 * A5 oracle EQUIVALENT. */
/* @t4-pass 0x1005F310 1 2026-09-20 probes 12 bytes 541 insns 171 regions 8 rows 15 census yes  (inline compound mode -= N dispatch: no move; codegen sub-vs-cmp/test) */
/* @t4-pass 0x1005F310 2 2026-09-20 probes 10 bytes 541 insns 171 regions 8 rows 15 census no   (baseline reconfirm; fild placement + shl-vs-lea index scaling unmoved) */
/* @t3 0x1005F310 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 541/538 insns 171/170 rows 7+8 regions 8 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is codegen only (see RESIDUE above).  A5 oracle EQUIVALENT is the
 * completeness proof (rule 12).  Do not reopen before the end-grind. */
/* @implements 0x1005F310 glide BrRaceGridPlace */
void BR_THISCALL1 BrRaceGridPlace(uint8_t *pDrv)
{
    int32_t mode;
    int32_t cap;
    int32_t idx;
    int32_t nLive;
    int16_t w;
    int32_t t;
    int32_t z;

    /* Orig: xor edi,edi; sub eax,edi / je zero; dec / je; sub eax,5 / je.
     * Subtract the live zero so the test is `sub` not `cmp`.  Zero arm last. */
    z = 0;
    mode = g_brRaceMode;
    mode = mode - z;
    if (mode != 0) {
        mode = mode - 1;
        if (mode != 0) {
            mode = mode - 5;
            if (mode != 0)
                cap = DAT_100b2f04;
            else
                cap = 9999;
        } else
            cap = 2;
    } else
        cap = DAT_100b3858;

    idx = (int32_t)(pDrv - DAT_10af07f8) >> 7;
    DW(pDrv, 0x64) = idx;
    BY(pDrv, 0x5c) = DAT_100b2fd8[idx * 3];
    BY(pDrv, 0x5d) = DAT_100b2fd8[idx * 3 + 1];
    BY(pDrv, 0x5e) = DAT_100b2fd8[idx * 3 + 2];
    DW(pDrv, 0x68) = z;
    nLive = DAT_100b3858 + 0xd;
    if (idx < nLive)
        DW(pDrv, 0x74) = z;
    else
        DW(pDrv, 0x74) = 1;

    if (idx < cap) {
        uint8_t *pCar = DAT_10af1208 + idx * 0x2b68;
        DW(pDrv, 0x60) = (int32_t)pCar;
        DW(pCar, 0xf00) = (int32_t)pDrv;
    } else {
        DW(pDrv, 0x60) = z;
        if (DW(pDrv, 0x74) == 1)
            t = (idx - DAT_100b3858) * 600 + -0x1e50;
        else if (DAT_100b3014 == 2 || DAT_100b3014 == 8)
            t = idx * 0x208;
        else
            t = idx * 0x226;
        FL(pDrv, 0x50) = (float)t;
        BrPathWalk(DAT_106eed48, FL(pDrv, 0x50));
        DW(pDrv, 0x00) = DAT_10b1ce98;
        DW(pDrv, 0x04) = DAT_10b1ce9c;
        DW(pDrv, 0x08) = DAT_10b1cea0;
        DW(pDrv, 0x28) = DAT_10b1cbec;
        DW(pDrv, 0x2c) = DAT_10af07f0;
        DW(pDrv, 0x4c) = DAT_10b1ca20;
        DW(pDrv, 0x48) = DAT_10b1ca20;
        DW(pDrv, 0x44) = DAT_10b1cea4;
        DW(pDrv, 0x40) = DAT_10b1cea4;
        BrPodNop("Initial lap.gate for %d(%d): %d.%d (dist=%f)\n",
                 DW(pDrv, 0x64), DW(pDrv, 0x74),
                 DAT_10b1cea4, DAT_10b1ca20, (double)FL(pDrv, 0x50));
        DW(pDrv, 0x0c) = DW(pDrv, 0x00);
        DW(pDrv, 0x10) = DW(pDrv, 0x04);
        DW(pDrv, 0x14) = DW(pDrv, 0x08);
        DW(pDrv, 0x30) = z;
        DW(pDrv, 0x38) = z;
        DW(pDrv, 0x34) = z;
        w = (int16_t)DAT_104b15e8 - 1;
        if (w > 2 || w < 0)
            w = 0;
        t = DAT_10af206c * 3 + (int32_t)w;
        DW(pDrv, 0x3c) = DAT_100bcab0[DAT_100b3014][t * 7 + 0x11];
        /* orig: [ecx + edx*4 + 0x44] with edx = t*7 (lea x8-x), so
         * offset 0x44/4 = 17 = 0x11, yes t*7+17. */
        DW(pDrv, 0x54) = DAT_100b2f00 - DW(pDrv, 0x64) - 1;
    }

    if (DW(pDrv, 0x64) >= DAT_100b3858)
        FUN_1005edc0(pDrv);
    else {
        DW(pDrv, 0x78) = z;
        DW(pDrv, 0x7c) = z;
    }
}

#endif /* BR_MATCHING_BUILD */
