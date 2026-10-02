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

#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include <stdint.h>
#include "slice3_41.h"
#include "br_match.h"

/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x100A9360 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrPathWalk: prototype in br_funcs.h */
/* BrPodNop: prototype in br_funcs.h */
/* FUN_1005edc0: prototype in br_funcs.h */


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
    mode = (*(int32_t *)&g_brRaceRules.mode);
    mode = mode - z;
    if (mode != 0) {
        mode = mode - 1;
        if (mode != 0) {
            mode = mode - 5;
            if (mode != 0)
                cap = g_BrCarCount;
            else
                cap = 9999;
        } else
            cap = 2;
    } else
        cap = g_brRaceNEntrant;

    idx = (int32_t)(pDrv - (*(uint8_t (*)[])&g_aBrRaceDriver)) >> 7;
    ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x64))))) = idx;
    ((*(uint8_t *)((uint8_t *)((pDrv)) + ((0x5c))))) = g_aBr0B37D0[idx * 3];
    ((*(uint8_t *)((uint8_t *)((pDrv)) + ((0x5d))))) = g_aBr0B37D0[idx * 3 + 1];
    ((*(uint8_t *)((uint8_t *)((pDrv)) + ((0x5e))))) = g_aBr0B37D0[idx * 3 + 2];
    ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x68))))) = z;
    nLive = g_brRaceNEntrant + 0xd;
    if (idx < nLive)
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x74))))) = z;
    else
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x74))))) = 1;

    if (idx < cap) {
        BrDriverCar *pCar = ((uint8_t *)&g_aBrRaceCar[idx].fwd.x);
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x60))))) = (int32_t)pCar;
        pCar->pProfile = (void *)pDrv;
    } else {
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x60))))) = z;
        if (((*(int32_t *)((uint8_t *)((pDrv)) + ((0x74))))) == 1)
            t = (idx - g_brRaceNEntrant) * 600 + -0x1e50;
        else if (g_Br0B380C == 2 || g_Br0B380C == 8)
            t = idx * 0x208;
        else
            t = idx * 0x226;
        ((*(float *)((uint8_t *)((pDrv)) + ((0x50))))) = (float)t;
        BrPathWalk(BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot), ((*(float *)((uint8_t *)((pDrv)) + ((0x50))))));
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x00))))) = (*(int32_t *)&g_brRacePathPos);
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x04))))) = (*(int32_t *)((char *)&g_brRacePathPos + 0x4));
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x08))))) = (*(int32_t *)((char *)&g_brRacePathPos + 0x8));
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x28))))) = g_brRacePathNode;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x2c))))) = (*(int32_t *)&g_brRacePathIndex);
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x4c))))) = DAT_10b1ca20;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x48))))) = DAT_10b1ca20;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x44))))) = DAT_10b1cea4;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x40))))) = DAT_10b1cea4;
        BrPodNop();
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x0c))))) = ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x00)))));
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x10))))) = ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x04)))));
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x14))))) = ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x08)))));
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x30))))) = z;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x38))))) = z;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x34))))) = z;
        w = (int16_t)(*(int32_t *)&DAT_104b15e8) - 1;
        if (w > 2 || w < 0)
            w = 0;
        t = (*(int32_t *)&g_aBrRaceCar[0].f0E64) * 3 + (int32_t)w;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x3c))))) = (*(int32_t * (*)[])&g_apBrRaceDiff)[g_Br0B380C][t * 7 + 0x11];
        /* orig: [ecx + edx*4 + 0x44] with edx = t*7 (lea x8-x), so
         * offset 0x44/4 = 17 = 0x11, yes t*7+17. */
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x54))))) = g_brRaceNDriver - ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x64))))) - 1;
    }

    if (((*(int32_t *)((uint8_t *)((pDrv)) + ((0x64))))) >= g_brRaceNEntrant)
        BrMakeEnemyCarColorPanels(pDrv);
    else {
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x78))))) = z;
        ((*(int32_t *)((uint8_t *)((pDrv)) + ((0x7c))))) = z;
    }
}

