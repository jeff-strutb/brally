/* br_racesel.c -- racing.
 *
 * The race-selection block at 0x1021C650 (the 0x46-dword block br_track.c's
 * BrInit220B20 clears): built from the menu's season record before a race
 * is (re)started.
 *
 * @t4-pass 0x10002460 1 2026-09-08 probes 10 bytes 252 insns 65 regions 1 rows 0 census yes
 * @t4-pass 0x10002460 2 2026-09-09 probes 14 bytes 252 insns 65 regions 1 rows 0 census yes
 *
 * PARKED T2 after 9 probes: 252/252 B, 65/65 insns, REGNORM 0+0, 2 bytes.
 * Residue: the loop's two induction pointers are coloured eax/edx the other
 * way round (orig: eax = entry pointer and the loop test, edx = car pointer;
 * recomp: edx entry, eax car).  Everything else -- the xor/mov bl byte
 * widening, the entry rebased at +4, the car rebased at +0, the rep movsd
 * intrinsics -- is exact.
 * Dead: uint8_t local for the selector (spills through the stack with `and
 * 0xff`; an int local loaded straight from the byte is the exact form); a
 * pCar local (car rebased at +0x160, roles swapped); a pE local (roles right,
 * entry rebased at +8, 7 bytes); no p local (251 B); pCar as a stepped
 * pointer variable (roles right, car rebased at +0x160, 250 B); both pE and
 * pCar stepped (243 B); the grid index as a 2-D array (inert).
 */

#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdint.h>
#include <string.h>

/* One 0x40-byte per-player entry inside the block. */
/* BrRaceSelEntry: br_coretypes.h */

/* 0x1021C650, 0x46 dwords: 6 header dwords then four entries. */
/* BrRaceSel: br_coretypes.h */

/* The per-car record viewed from its +0xE8C field (0x10AF2094 for car 0),
 * stride 0x2B68. */
typedef struct BrRaceCarSel {
    uint8_t *pRec;         /* +0x000  the 0x53-dword season record */
    uint8_t  _pad004[0x11C - 0x004];
    int32_t  f11C;         /* +0x11C */
    uint8_t  _pad120[0x128 - 0x120];
    int32_t  a128[12];     /* +0x128 */
    uint8_t  _pad158[0x160 - 0x158];
    int32_t  f160;         /* +0x160 */
    uint8_t  _pad164[0x2B68 - 0x164];
} BrRaceCarSel;

/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x1021C650 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10AF2094, stride 0x2B68 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10AF2094 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x100A9360 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100B3014 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x104B15E8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x100B3858 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10AC4C60 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10AC5A48 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100A9560 */

/* BrSub1001CA30: prototype in br_funcs.h */
/* BrSelLookup: prototype in br_funcs.h */
/* BrSessionReinitVideo: prototype in br_funcs.h */

/* WHAT IT DOES: rebuilds the race-selection block from the menu's season
 * record and reinitialises the session's video.  The block is cleared and
 * its first slot takes the race mode.  In mode 0 (a normal race from the
 * menu) the record's two selector bytes and first dword, the chosen track
 * and the weather go into the header; the one local player's entry is
 * filled from its car record (its selector byte out of the record's grid,
 * that byte through the signed table at 0x100A9560, and twelve dwords of
 * setup); the season record is copied into both 0x53-dword option blocks,
 * the entrant count set to one, and the selection lookup run.  Every mode
 * ends with the video reinit. */
/* @t3 0x10002460 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 252/252 insns 65/65 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register allocation: the loop's two induction pointers are
 * coloured eax/edx the opposite way (orig eax=entry+loop test, edx=car;
 * recomp edx=entry, eax=car), a 2-byte swap; everything else is exact.  The
 * dossier and the dead-probe list are in this file's header block above.
 * Passes 1-2 (ledger lines in the header) moved nothing at 252/65/1/0;
 * hoisting the car-sel pointer and a selector temp both went worse, not
 * better.  Do not reopen before the end-grind. */
/* @implements 0x10002460 glide BrRaceSelFromMenu */
void BrRaceSelFromMenu(void)
{
    BrRaceSel     *p = &g_a220B20;
    uint8_t       *pRec;
    int            sel;
    int            i;

    memset(p, 0, sizeof(int32_t) * 0x46);
    p->mode = (*(int32_t *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */;
    if ((*(int32_t *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 0) {
        pRec = (*(uint8_t * *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */;
        p->b4 = pRec[4];
        p->b5 = pRec[5];
        p->rec0 = *(int32_t *)pRec;
        p->track = g_Br0B380C;
        p->weather = (*(int32_t *)&DAT_104b15e8);
        for (i = 0; i < 1; i++) {
            p->e[i].f00 = g_aBrRaceCar[i].lap;
            sel = g_aBrRaceCar[i].pEquip[p->b4 * 4 + p->b5 + 6];
            p->e[i].nSel = sel;
            p->e[i].nTbl = g_tblBrA9560[sel];
            p->e[i].f0C = (*(int32_t *)&g_aBrRaceCar[i].tFinal);
            memcpy(p->e[i].a10, (*(int32_t (*)[12])&g_aBrRaceCar[i].aLapTime[0]), sizeof(int32_t) * 12);
        }
        memcpy(g_aBrA9DBD8, pRec, sizeof(int32_t) * 0x53);
        g_brRaceNEntrant = 1;
        BrCarDamageTick();
        memcpy((*(int32_t (*)[83])&DAT_10ac5a48), (*(uint8_t * *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */, sizeof(int32_t) * 0x53);
        BrSelLookup();
    }
    BrSessionReinitVideo();
}

