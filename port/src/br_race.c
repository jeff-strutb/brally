/* br_race.c -- lap, gate and finish bookkeeping.
 *
 * Glide 0x1005FF00 (2538 B) == D3D 0x10066E90 (2534 B), transcribed from the
 * Glide build. See br_race.h for the mechanism, the address pairs, and the
 * four blocks of the original that are deliberately not here.
 *
 * The original's ten debug format strings are quoted at the line each one
 * belongs to. They are the clearest statement of intent anywhere in this
 * function and cost nothing to keep as comments.
 */
#include "br_race.h"
#include "br_crt.h"     /* BrFtolTrunc -- 0x1007C8A0 / MSVCRT _ftol */

/* Float constants read out of BRGlide.dll rather than assumed, per
 * CONVENTIONS.md. Addresses are Glide. */
#define BR_RACE_K100     100.0f          /* 0x1007799C */
#define BR_RACE_K001     0.01f           /* 0x100779A0 -- 0.009999999776f  */
#define BR_RACE_KZERO    0.0f            /* 0x100778D8 */
#define BR_RACE_KFINISH  0.9f            /* 0x3F666666 at 0x10060362       */

/* +0x68 bit 1. slice3_41.h already names it: BrRankAssign skips a slot that
 * carries it, which is exactly right -- a driver that has finished no longer
 * takes part in the running order. One bit, two consumers, one name. */
#define BR_RACE_FINISHED  BR_DRIVER_SKIP

/* ==========================================================================
 * 0x1005FF9B..0x1005FFAC -- unwrapped gate number to ring index
 * ========================================================================== */

int32_t BrRaceGateIndex(int32_t gate, int32_t nGates)
{
    int32_t q;

    if (gate >= 0) {
        q = gate;                         /* 0x1005FFAA */
    } else {
        /* `or eax,-1 / sub eax,edx / cdq / idiv ecx / mov eax,ecx /
         *  sub eax,edx / dec eax` -- the remainder of (-1 - gate) is taken
         * from the truncating divide and folded back the other way. */
        q = nGates - ((-1 - gate) % nGates) - 1;
    }
    /* The original divides a SECOND time here (0x1005FFAC `cdq` /
     * 0x1005FFB0 `idiv ecx`) and keeps that remainder. On a value already in
     * [0, nGates) it changes nothing; kept so the two can be diffed. */
    return q % nGates;
}

/* ==========================================================================
 * 0x1005FFAD..0x1005FFE3 -- the lap time, truncated to hundredths
 * ========================================================================== */

float BrRaceTruncHundredths(float t)
{
    /* `fld [ebp+0x30] / fmul 100.0 / push / fstp [esp] / call 0x10018990`,
     * then `fild [esp+0x18] / fmul 0.01`. 0x10018990 is a two-instruction
     * thunk onto MSVCRT `_ftol`, which is br_crt.h's BrFtolTrunc. */
    int32_t n = BrFtolTrunc(t * BR_RACE_K100);
    return (float)n * BR_RACE_K001;
}

/* ==========================================================================
 * The car mirror, 0x1005FF22..0x1005FF8B and 0x100608A3..0x100608DC
 * ========================================================================== */

void BrRaceLoadFromCar(BrDriver *pDrv)
{
    const BrDriverCar *pCar = pDrv->pCar;

    if (pCar == NULL)
        return;

    pDrv->f00 = pCar->pos;          /* +0x30  -> +0x00 */
    pDrv->f0C = pCar->posPrev;      /* +0xF80 -> +0x0C */
    pDrv->f48 = pCar->gateHi;       /* +0xFA0 -> +0x48 */
    pDrv->f4C = pCar->gate;         /* +0xFA4 -> +0x4C */
    pDrv->f40 = pCar->lap;          /* +0xFA8 -> +0x40 */
    pDrv->f44 = pCar->lapB;         /* +0xFAC -> +0x44 */
    pDrv->f30 = pCar->tRun;         /* +0xFB0 -> +0x30 */
    pDrv->f34 = pCar->tBest;        /* +0xFE4 -> +0x34 */
    pDrv->f50 = pCar->fFF4;         /* +0xFF4 -> +0x50 */
}

void BrRaceStoreToCar(BrDriver *pDrv)
{
    BrDriverCar *pCar = pDrv->pCar;

    if (pCar == NULL)
        return;

    /* The positions are NOT sent back: they are inputs, owned by the physics
     * pass. Seven fields out against nine in. */
    pCar->gateHi = pDrv->f48;
    pCar->gate   = pDrv->f4C;
    pCar->lap    = pDrv->f40;
    pCar->lapB   = pDrv->f44;
    pCar->tRun   = pDrv->f30;
    pCar->tBest  = pDrv->f34;
    pCar->fFF4   = pDrv->f50;
}

/* ==========================================================================
 * The pieces the two lap arms share
 * ========================================================================== */

/* 0x10060155..0x1006017E, and again verbatim at 0x10060702..0x10060727.
 *
 * COMPARISON POLARITY, and this is the one that matters here. Both tests are
 * `fcomp` + `test ah,<mask>`, so UNORDERED takes the same side as the
 * condition:
 *
 *   `test ah,0x40 / jne` reads C3, which is set for equal AND for unordered.
 *   So a NaN best time is treated as "no best set yet" and is replaced.
 *
 *   `test ah,1 / je` reads C0, which is set for less-than AND for unordered.
 *   The jump is taken when C0 is CLEAR, i.e. when the new time is NOT less --
 *   so a NaN new time is accepted as a new best.
 *
 * Written as negated comparisons, per CONVENTIONS.md. */
/* `fcom`-class C3: set for EQUAL and for UNORDERED alike. Spelled out rather
 * than written as `a == b`, because C's == is FALSE for a NaN operand and the
 * x87 flag is SET -- the two disagree on exactly the case that matters. */
static int BrRaceFcomEqualOrNan(float a, float b)
{
    return !(a < b) && !(a > b);
}

static void BrRaceBestLap(BrDriver *pDrv, float tLap)
{
    /* 0x10060164 `test ah,0x40 / jne` -- C3 set jumps straight to the store,
     * so a NaN incumbent is treated as "no best yet". */
    if (!BrRaceFcomEqualOrNan(pDrv->f34, BR_RACE_KZERO)) {
        /* 0x10060172 `test ah,1 / je` -- C0 is set for LESS and for
         * UNORDERED; the skip is on C0 CLEAR. */
        int fLessOrUnordered = !(tLap >= pDrv->f34);
        if (!fLessOrUnordered)
            return;
    }
    pDrv->f34 = tLap;
    /* OMITTED (br_race.h, item 3): the per-track record table at 0x10AF2094
     * and the two HUD message ids 0x109 / 0x10A that follow from it. Guarded
     * by `pDrv->f64 < nCars`, and it writes no driver or car state. */
}

/* The block both lap arms run once a lap is judged complete:
 * 0x100601EF..0x10060231 and 0x10060796..0x100607D4. */
static void BrRaceRecordLap(BrDriver *pDrv, float tLap)
{
    BrDriverCar *pCar = pDrv->pCar;

    if (pCar != NULL) {
        /* "veh->lapTimeFinal[%d]=%f\n", pDrv->f40, tLap */
        /* DEVIATION (memory safety): the original indexes +0xFB4 by the lap
         * with no bound of any kind, and the array ends at +0xFE4 -- twelve
         * entries. A race configured for more than twelve laps therefore
         * walks into the best-lap and finish-time fields. The write is
         * dropped here instead; nothing else about the lap changes. */
        if ((uint32_t)pDrv->f40 < (uint32_t)BR_RACE_LAPTIME_MAX)
            pCar->aLapTime[pDrv->f40] = tLap;
    }
    /* Unconditional -- not inside the `if (veh)`. */
    pDrv->f30 -= tLap;
}

/* 0x10060252..0x10060294 and 0x10060860..0x1006089C -- the mode-3 unwind.
 * Two byte-identical blocks reached from the two lap arms; the only
 * difference between them is which debug string precedes them, "(a)" or
 * "(b)". */
static void BrRaceModeWrapUnwind(const BrRaceRules *pRules, BrDriver *pDrv)
{
    pDrv->f40 -= 1;
    pDrv->f44 -= 1;
    pDrv->f48 -= pRules->nGates;
    pDrv->f4C -= pRules->nGates;
    if (pRules->pfLapLength != NULL)            /* 0x1006027F: 0x106EED48 */
        pDrv->f50 -= *pRules->pfLapLength;
}

/* ==========================================================================
 * 0x1005FF00
 * ========================================================================== */

int BrRaceGateStep(BrRaceRules *pRules, BrDriver *pDrv)
{
    BrDriverCar *pCar;
    const BrRaceGate *pGate;
    int32_t nGates, gate, iCur, iNext;
    float   tLap;
    BrVec2  segFrom, segTo;
    int     nLapped = 0;

    nGates = pRules->nGates;
    if (nGates == 0)                            /* 0x1005FF0F */
        return 0;

    pCar = pDrv->pCar;                          /* +0x60 */
    if (pCar != NULL) {
        BrRaceLoadFromCar(pDrv);
        nGates = pRules->nGates;                /* re-read at 0x1005FF8E */
    }

    gate  = pDrv->f4C;
    iCur  = BrRaceGateIndex(gate, nGates);
    iNext = (iCur + 1) % nGates;                /* 0x1005FFBE */

    tLap = BrRaceTruncHundredths(pDrv->f30);

    /* The motion segment. BrSeg2Intersect reads only offsets 0 and 4 of each
     * operand, so this is the driver's X and Y and never its Z. The gate's
     * posts go in SECOND POST FIRST: the original pushes +0x08 then +0x00. */
    segFrom.x = pDrv->f0C.x;  segFrom.y = pDrv->f0C.y;   /* last frame */
    segTo.x   = pDrv->f00.x;  segTo.y   = pDrv->f00.y;   /* this frame */

    /* ---- did it cross the gate it is standing on? (backwards) ---------- */
    pGate = &pRules->aGates[iCur];
    if (BrSeg2Intersect(&pGate->postB, &pGate->postA, &segFrom, &segTo) != 0) {
        /* "%d went backwards, from gate %d to gate %d\n"
         *  pDrv->f64, pDrv->f4C, pDrv->f4C - 1 */
        if (iCur == 0)                          /* 0x10060026 */
            pDrv->f44 -= 1;

        /* "Hmm %d == %d %% %d && %d < 0 ? %d\n" -- the original builds that
         * predicate into a register purely to print it, then recomputes the
         * modulus and branches on the recomputed value. Only the branch is
         * kept. */
        if (iCur == pDrv->f48 % nGates) {       /* 0x10060089, truncating % */
            if (pDrv->f4C < 0) {                /* 0x10060090 */
                pDrv->f4C += nGates;
                pDrv->f44 += 1;
                if (pRules->pfLapLength != NULL)
                    pDrv->f50 += *pRules->pfLapLength;
                /* "lapping %d forward to -1\n", pDrv->f64 */
            }
        }
        pDrv->f4C -= 1;                         /* 0x100600C3, both arms */
        goto tail;
    }

    /* ---- did it cross the next one? (forwards) ------------------------- */
    pGate = &pRules->aGates[iNext];
    if (BrSeg2Intersect(&pGate->postB, &pGate->postA, &segFrom, &segTo) == 0)
        goto tail;                              /* 0x100600EE */

    /* "%d went forward, from gate %d to gate %d\n"
     *  pDrv->f64, pDrv->f4C, pDrv->f4C + 1 */
    pDrv->f4C = gate + 1;                       /* 0x10060113 */

    if (pDrv->f4C > pDrv->f48) {                /* 0x10060119, `jle` */
        /* --- new furthest-gate high-water mark --------------------------- */
        pDrv->f48 = pDrv->f4C;
        /* "moved ahead one gate, getting %f seconds\n",
         *  pRules->aGates[iNext].tAward  -- read, printed, never applied. */

        if (iNext != 0)                         /* 0x1006013D */
            goto positions;

        if (pDrv->f40 < pRules->nLaps) {        /* 0x1006014D, `jge` */
            BrRaceBestLap(pDrv, tLap);
            BrRaceRecordLap(pDrv, tLap);
            pDrv->f40 += 1;                     /* 0x10060226 */
            pDrv->f44 = pDrv->f40;
            nLapped = 1;

            if (pRules->mode == BR_RACE_MODE_WRAP) {   /* 0x10060232 */
                if (pDrv->f40 == 1) {
                    /* "lapping %d back to 0 (a)\n", pDrv->f64 */
                    BrRaceModeWrapUnwind(pRules, pDrv);
                    nLapped = 0;
                }
            } else if (pCar != NULL) {          /* 0x1006029A */
                /* "******* LAP #%d ********\n", pDrv->f40 */
                /* OMITTED (br_race.h, item 2): the lap banner -- either the
                 * "final lap" string (id 0x10B, when f40 == nLaps - 1) or an
                 * sprintf of id 0x10C with f40 + 1 -- into car +0x100C, then
                 * car +0xFFC/+0x1000 and, when the best-lap block above
                 * produced a message id, +0x1004/+0x1008. */
            }
        }

        /* ---- THE FINISH CONDITION, 0x1006033B ------------------------- */
        /* Reached whether or not the lap block above ran: when f40 has
         * already reached nLaps the increment is skipped and this fires
         * again on every further crossing of gate 0. */
        if (pDrv->f40 == pRules->nLaps) {
            pDrv->f68 |= BR_RACE_FINISHED;      /* 0x1006034E, before the
                                                 * car test -- a driver with
                                                 * no car still finishes */
            if (pCar != NULL) {
                pCar->tFinal -= pDrv->f30;      /* 0x1006035C */
                pCar->f29B0   = BR_RACE_KFINISH;
                pDrv->f30     = BR_RACE_KZERO;
                pCar->fFF8    = pRules->nFinished;
                /* OMITTED (br_race.h, items 2 and 3): the placing banner
                 * ("1st"/"2nd"/...) and, when nLaps == 3 and this driver has
                 * a car index below the car count, the best-total record at
                 * 0x10AF2094 + 0x10C. */
            }
            pRules->nFinished += 1;             /* 0x10060440 -- reached from
                                                 * the no-car path too */
            goto tail;                          /* the finish arm SKIPS the
                                                 * standings recompute */
        }
    positions:
        /* OMITTED (br_race.h, item 4): 0x1006044B, the standings recompute
         * for modes 1, 6 and 2. It reads and writes the whole field and the
         * cars' HUD text; it touches none of the state this module owns. */
        goto tail;
    }

    /* --- forward, but back over ground already covered ------------------ */
    /* 0x100606E6. This is how a driver that dropped back and re-crossed gate
     * 0 is given the lap it has, in the original's words, technically
     * earned. */
    if (iNext != 0)
        goto tail;

    pDrv->f44 += 1;                             /* 0x100606F4 */
    if (pDrv->f44 > pDrv->f40) {                /* `jle` */
        BrRaceBestLap(pDrv, tLap);
        BrRaceRecordLap(pDrv, tLap);
        pDrv->f40 = pDrv->f44;                  /* 0x100607CF -- assignment,
                                                 * not an increment */
        nLapped = 1;
        /* "******* LAP #%d ********\n" and the same banner, omitted. */
    }

    /* "granting technically-earned lap %d/%d to %d\n",
     *  pDrv->f44, pDrv->f40, pDrv->f64 */
    if (pRules->mode == BR_RACE_MODE_WRAP && pDrv->f40 == 1) {
        /* "lapping %d back to 0 (b)\n", pDrv->f64 */
        BrRaceModeWrapUnwind(pRules, pDrv);
        nLapped = 0;
    }

    /* NOTE, and it is the original's: this arm has NO finish test. A lap
     * granted here can take f40 to nLaps without ever setting the finished
     * flag -- the flag is only reachable through the high-water arm. */

tail:
    if (pCar != NULL)                           /* 0x1006089F */
        BrRaceStoreToCar(pDrv);
    return nLapped;
}
