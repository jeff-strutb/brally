#include "br_race.h"   /* br_globals: its objects */
#include "slice3_41.h"
/* br_cartick.c -- racing.
 * Per-car, per-frame bookkeeping that the driver step 0x100623E0 runs on
 * every car between the physics and the gate machine: the lap and air
 * clocks (0x1006E9E0), the two on-screen message countdowns (0x1006EA70)
 * and the 64x64 grid cell the car occupies (0x1006EBC0).  br_racestep.h
 * lists all three under BR_RS_HOLE_SKID beside 0x1006EB00, which is in
 * br_wrongway.c and writes the message slots these count down.
 */

/* 64-bit core: duplicate declaration removed (the frame dt, 0x106E9D8C) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x106EED48                              */
/* BrFtolTrunc: prototype in br_funcs.h */
/* FUN_1006e5c0: prototype in br_funcs.h */

/* The car record is not typed in this lane; these name the fields used. */

/* WHAT IT DOES: advance a car's clocks by one frame, unless its body is
 * flagged (bits 0-1 of +0x68 on the physics record) as not running.  In race
 * mode 3 only the lap clock (+0xFB0) runs; otherwise both the lap clock and
 * the total clock (+0xFEC) run, and in modes 1 and 6 a countdown (+0xFF0)
 * also ticks down, stopping at zero. */
/* @implements 0x1006E9E0 glide BrCarTickClocks */
void __fastcall BrCarTickClocks(BrDriverCar *pCar)
{
    if ((((*(unsigned char *)(((((((char *)pCar->pProfile))))) + ((0x68))))) & 3) == 0) {
        if ((*(int *)&g_brRaceRules.mode) == 3) {
            ((pCar->tRun)) += g_brRaceFlyStep;
            return;
        }
        ((pCar->tFinal)) += g_brRaceFlyStep;
        ((pCar->tRun)) += g_brRaceFlyStep;
        if ((*(int *)&g_brRaceRules.mode) == 1 || (*(int *)&g_brRaceRules.mode) == 6) {
            /* Written out, not `-=`: on a volatile operand `-=` loads the
             * volatile first and emits fsubr; this form keeps `fld x; fsub dt`. */
            ((pCar->fFF0)) = ((pCar->fFF0)) - g_brRaceFlyStep;
            if (((pCar->fFF0)) < 0.0f)
                ((pCar->fFF0)) = 0.0f;
        }
    }
}

/* WHAT IT DOES: count down a car's on-screen message.  The first slot
 * (+0xFFC id, +0x1000 seconds left) takes priority; only when it is idle
 * does the second (+0x1004, +0x1008) tick.  When a countdown runs out both
 * the timer and the id are cleared, which takes the message off screen. */
/* @implements 0x1006EA70 glide BrCarTickMessages */
void __fastcall BrCarTickMessages(BrDriverCar *pCar)
{
    if (((pCar->f1000)) != 0.0f) {
        ((pCar->f1000)) = ((pCar->f1000)) - g_brRaceFlyStep;  /* not -=: volatile */
        if (((pCar->f1000)) <= 0.0f) {
            ((pCar->f1000)) = 0.0f;
            pCar->pszBanner = 0;
        }
    }
    else if (((pCar->f1008)) != 0.0f) {
        ((pCar->f1008)) = ((pCar->f1008)) - g_brRaceFlyStep;  /* not -=: volatile */
        if (((pCar->f1008)) <= 0.0f) {
            ((pCar->f1008)) = 0.0f;
            pCar->psz1004 = 0;
        }
    }
}

/* WHAT IT DOES: work out which cell of a 64x64 grid (32 units a side) the
 * car's position falls in, store the two coordinates as bytes at +0x29BC/BD
 * clamped to 63, and hand the car to 0x1006E5C0, which uses that cell.
 * Skipped entirely while the global at 0x106EED48 is off. */
/* @implements 0x1006EBC0 glide BrCarTickGridCell */
void __fastcall BrCarTickGridCell(BrDriverCar *pCar)
{
    if (g_pBrRaceLapRec != 0) {
        /* Inline arguments, not two locals: a precomputed fy would have to
         * live across the first call and VC5 spills it (+7 B).  The position
         * reads are VOLATILE: that is what keeps `fld y; fmul k` together
         * ahead of the first call's `add esp,4 / mov [cx],al` instead of
         * letting the scheduler slot those two between them (11 diff B). */
        ((pCar->f29BC)) = (unsigned char)BrFtolArg((pCar->pos.x) * 0.03125f);
        ((pCar->f29BD)) = (unsigned char)BrFtolArg((pCar->pos.y) * 0.03125f);
        if (((pCar->f29BC)) >= 0x40)
            ((pCar->f29BC)) = 0x3f;
        if (((pCar->f29BD)) >= 0x40)
            ((pCar->f29BD)) = 0x3f;
        BrCarTrackLocate(pCar);
    }
}

