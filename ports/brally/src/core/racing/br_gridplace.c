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
    BrDriver *d = (BrDriver *)pDrv;
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

    idx = (int32_t)(d - g_aBrRaceDriver);            /* (pDrv - 0x10AF07F8) >> 7 */
    d->f64 = idx;
    d->f5C = g_aBr0B37D0[idx * 3];
    d->f5D = g_aBr0B37D0[idx * 3 + 1];
    d->f5E = g_aBr0B37D0[idx * 3 + 2];
    d->f68 = z;
    nLive = g_brRaceNEntrant + 0xd;
    if (idx < nLive)
        d->f74 = z;
    else
        d->f74 = 1;

    if (idx < cap) {
        BrDriverCar *pCar = &g_aBrRaceCar[idx];
        d->pCar = pCar;
        pCar->pProfile = (void *)pDrv;
    } else {
        d->pCar = 0;
        if (d->f74 == 1)
            t = (idx - g_brRaceNEntrant) * 600 + -0x1e50;
        else if (g_Br0B380C == 2 || g_Br0B380C == 8)
            t = idx * 0x208;
        else
            t = idx * 0x226;
        d->f50 = (float)t;
        BrPathWalk(BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot), d->f50);
        d->f00 = *(BrVec3 *)&g_brRacePathPos;
        d->pPathNode = (struct BrAiPathNode *)g_brRacePathNode;
        d->f2C = (*(int32_t *)&g_brRacePathIndex);
        d->f4C = DAT_10b1ca20;
        d->f48 = DAT_10b1ca20;
        d->f44 = DAT_10b1cea4;
        d->f40 = DAT_10b1cea4;
        BrPodNop();
        d->f0C = d->f00;
        d->f30 = 0.0f;
        d->f38 = z;
        d->f34 = 0.0f;
        w = (int16_t)(*(int32_t *)&DAT_104b15e8) - 1;
        if (w > 2 || w < 0)
            w = 0;
        t = (*(int32_t *)&g_aBrRaceCar[0].f0E64) * 3 + (int32_t)w;
        /* orig: [ecx + edx*4 + 0x44] with edx = t*7: the award row's
         * seventh float, its bits copied into an int */
        d->f3C = *(int32_t *)&g_apBrRaceDiff[g_Br0B380C]->aAward[t * 7 + 6];
        d->f54 = g_brRaceNDriver - d->f64 - 1;
    }

    if (d->f64 >= g_brRaceNEntrant)
        BrMakeEnemyCarColorPanels(pDrv);
    else {
        d->aptex = 0;
        d->cptex = z;
    }
}

