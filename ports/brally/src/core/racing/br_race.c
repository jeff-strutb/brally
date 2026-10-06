/* br_race.c -- lap, gate and finish bookkeeping.
 *
 * Glide 0x1005FF00 (2538 B) == D3D 0x10066E90 (2534 B), transcribed from the
 * Glide build. See br_race.h for the mechanism and the address pairs.
 *
 * TWO ARMS. The port arm leaves out four blocks of the original (br_race.h
 * lists them) because none of them feeds the lap state back. The matching
 * arm, under BR_MATCHING_BUILD, has all of them: it is SIZE-EXACT and
 * INSTRUCTION-FOR-INSTRUCTION exact at 2,538 bytes / 720 instructions,
 * and 2,536 of those bytes are identical to the original.
 *
 * ============ CLOSED 2026-09-09: BYTE-EXACT, 2538/2538, 0 diffs ============
 *
 * The 2-byte residue below fell to the pairwise DECLARATION-ORDER tie-break
 * (docs/brally/VC5-IDIOMS.md, first seen on 0x100250D0's imul): name BOTH add
 * operands -- `(nGates = g_brTrkHdr.nGate)` inside the modulus and
 * `(gate = pDrv->f4C) < 0` in the conjunction, then `gate + nGates` -- and
 * declare `gate` BEFORE `nGates`.  The later-declared symbol is the
 * two-operand destination.  Measured (11 compiles, one lever): with the
 * count's temp declared after the field's temp, in the middle or at the
 * END of the list, or the two existing names swapped: BYTE-EXACT (4 of 4);
 * the count's temp declared BEFORE the field's: the old 2 bytes; naming
 * only the count (three spellings incl. `+=`): the old 2 bytes; both
 * assigned as statements before the `if`: 12 diffs (the loads move).
 * The dead list below is kept as history; none of it named both operands.
 *
 * -------- the residue as it stood 2026-09-07 (history) --------
 *
 * The whole remainder was ONE instruction pair at orig+0x197:
 *
 *     orig    add ecx, eax      /  mov dword ptr [ebp+0x4c], ecx
 *     ours    add eax, ecx      /  mov dword ptr [ebp+0x4c], eax
 *
 * Every byte before and after is equal, both registers hold the same values
 * (ecx is the divisor CSE of `pDrv->f48 % g_brTrkHdr.nGate` from orig+0x17d,
 * eax is the `pDrv->f4C` CSE from the `< 0` test at orig+0x18d), and both
 * are dead after the add. It is purely which of the two VC5 picks as the
 * two-operand destination.
 *
 * SPELLINGS PROVEN DEAD, all at 2,538 bytes / 720 instructions / 2 diffs:
 *   - `pDrv->f4C += g_brTrkHdr.nGate;`
 *   - `pDrv->f4C = g_brTrkHdr.nGate + pDrv->f4C;`   (global first)
 *   - `pDrv->f4C = pDrv->f4C + g_brTrkHdr.nGate;`   (field first)
 *   - a named temp seeded from the global and accumulated into
 *     (`int32_t n = g_brTrkHdr.nGate; n += pDrv->f4C; pDrv->f4C = n;`)
 *   - re-reading the field into the `gate` local first and adding that
 *   - the nested-if guard instead of the `&&` conjunction kept below
 *   - a named local as the add's second operand, assigned inside the
 *     conjunction (`(gate = pDrv->f4C) < 0`) so it is not the lvalue's own
 *     re-read -- the hypothesis that VC5 was recognising an accumulate
 * And WORSE (2,543 bytes): putting `pDrv->f44 += 1;` before the add -- the
 * intervening store kills the divisor CSE and VC5 re-loads the global.
 *
 * All four sweep variants (/O2, /Od, /O2 /Oy-, /O2 /Op) bottom out at the
 * same 2 diffs, so this is not a flag. Fresh leads only.
 *
 * AND THE SOURCE IS CONFIRMED, so those spellings can stop being re-tried:
 * the N64 twin is 0x8022A0E0 in Top Gear Rally (found by xref -- all four of
 * this function's format strings are in the ROM byte-identical and all four
 * point at it). It carries every structural element of this transcription in
 * the same order: the six globals, the gate count re-read inside the
 * `if (pCar)` block, the field-by-field vector copies, the floor-modulus arm
 * order, the "Hmm" predicate with its five printf arguments, and
 * `addu $t5, $s0, $s1` -- pDrv->f4C + nGates, in that operand order, at the
 * very instruction pair the two bytes live in. This residue is allocation,
 * not a source fork.
 * =========================================================================
 *
 * The original's debug format strings are transcribed verbatim in the
 * matching arm and quoted as comments in the port arm. They are the clearest
 * statement of intent anywhere in this function.
 */
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "slice3_41.h"   /* br_globals: its objects */
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
     * thunk onto MSVCRT `_ftol`, which is br_crt.h's BrFtolArg. */
    int32_t n = BrFtolArg(t * BR_RACE_K100);
    return (float)n * BR_RACE_K001;
}

/* ==========================================================================
 * The car mirror, 0x1005FF22..0x1005FF8B and 0x100608A3..0x100608DC
 * ========================================================================== */

/* (port-only BrRaceLoadFromCar removed) */


/* (port-only BrRaceStoreToCar removed) */


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
/* (port-only BrRaceFcomEqualOrNan removed) */


/* (port-only BrRaceBestLap removed) */


/* The block both lap arms run once a lap is judged complete:
 * 0x100601EF..0x10060231 and 0x10060796..0x100607D4. */
/* (port-only BrRaceRecordLap removed) */


/* 0x10060252..0x10060294 and 0x10060860..0x1006089C -- the mode-3 unwind.
 * Two byte-identical blocks reached from the two lap arms; the only
 * difference between them is which debug string precedes them, "(a)" or
 * "(b)". */
/* (port-only BrRaceModeWrapUnwind removed) */


/* ==========================================================================
 * 0x1005FF00
 * ========================================================================== */

/* WHAT IT DOES: advances one driver through the race's checkpoint gates. It
 * works out which gate they are heading for, tests whether the line they
 * moved along this frame crossed it, and if so moves them on -- rolling over
 * the lap counter and the lap time when they pass the last gate, and
 * unwinding it again if they cross backwards. This is where lap counting
 * actually happens. */

/* ==========================================================================
 * THE MATCHING ARM
 *
 * The port arm below leaves four blocks of the original out (br_race.h names
 * them).  None of them feeds the lap state back, which is why the port is
 * correct without them -- but the original is 2,538 bytes and the port arm is
 * 581, and every one of the missing 1,957 bytes is OUTPUT: twelve debug
 * printfs, ten string-table lookups, four sprintfs and the standings pass.
 * Restoring them is transcription, not discovery.
 *
 * Three shapes here are source, not schedule, and each is a rule from
 * docs/brally/VC5-IDIOMS.md:
 *
 *   - ONE argument, in ecx: `mov ebp, ecx` and a bare `ret`.  BrRaceRules is
 *     the accessor sub-case -- six separate absolute globals behind one
 *     struct pointer -- and the original re-reads 0x106EEE38 from memory four
 *     times inside the "Hmm" block, which no base register can produce.
 *   - The two shared blocks (the lap record, the mode-3 unwind) are MACROS,
 *     not statics: MSVC 5.0 will not inline a `static` with two callers, and
 *     a shared function-scope temp is what earns a stack slot.
 *   - The x87 comparisons are `fcomp` + `test ah,<mask>`, so the NaN side
 *     goes WITH the condition; spelled as negated compares, per
 *     CONVENTIONS.md and the note above BrRaceFcomEqualOrNan.
 * ========================================================================== */

#include <string.h>      /* strcpy / strlen, both inlined by /O2            */

/* The original reaches sprintf through the import table
 * (`call dword ptr [0x118F0570]`), which is what __declspec(dllimport)
 * emits.  Declaring it from <stdio.h> without the attribute produces a
 * direct `call rel32` instead -- four wrong call sites. */
__declspec(dllimport) int __cdecl sprintf(char *pDst, const char *pFmt, ...);

/* 0x10008D60 is one byte, `c3`.  The original still builds every argument and
 * pops the frame, so the CALL is part of the function's bytes even though the
 * callee does nothing. */
/* BrExt_10008D60: prototype in br_funcs.h */
/* BrStrGet: prototype in br_funcs.h */
/* BrTimeFormat: prototype in br_funcs.h */
/* BrSfxSrcBeep: prototype in br_funcs.h */

/* `fcomp` + `fnstsw` + `test ah,0x40` + `jne` is exactly what MSVC 5.0 emits
 * for a plain `x == 0.0f`, C3 being set for EQUAL and for UNORDERED alike.
 * The port arm spells the same test out as two negated compares because C's
 * `==` is FALSE for a NaN operand and the x87 flag is SET -- but the SOURCE
 * said `== 0`, and the long form costs a second fcom at every one of the
 * seven sites. */
#define BR_FCOM_EQ0(x)   ((x) == BR_RACE_KZERO)

/* 0x100601EF..0x1006021F and 0x10060796..0x100607C6, byte-identical. */
#define BR_RACE_RECORD_LAP()                                                  \
    do {                                                                      \
        if (pCar != NULL) {                                                   \
            BrPodNop("veh->lapTimeFinal[%d]=%f\n", pDrv->f40, tLap);    \
            pCar->aLapTime[pDrv->f40] = tLap;                                 \
        }                                                                     \
        pDrv->f30 -= tLap;                                                    \
    } while (0)

/* 0x10060252..0x10060294 and 0x10060860..0x1006089C, byte-identical.  The
 * gate count is read from memory TWICE -- once per subtraction -- which is
 * what spelling the global at each use produces and what a cached local does
 * not. */
#define BR_RACE_WRAP_UNWIND()                                                 \
    do {                                                                      \
        pDrv->f40 -= 1;                                                       \
        pDrv->f44 -= 1;                                                       \
        pDrv->f48 -= g_brTrkHdr.nGate;                                           \
        pDrv->f4C -= g_brTrkHdr.nGate;                                           \
        if (BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot) != NULL)                                          \
            pDrv->f50 -= BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot)->aPt[0].arc;                         \
    } while (0)

/* WHAT IT DOES: run one car's race logic for a frame: which gate it is
 * heading for, whether it just passed one, lap and split timing, its
 * position against the others, and the messages that result. The rules of
 * the race, per car per frame. */
/* @t4-pass 0x1005FF00 1 2026-09-07 probes 99 bytes 2538 insns 720 regions 2 rows 2 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1005FF00 2 2026-09-07 probes 99 bytes 2538 insns 720 regions 2 rows 2 census yes  (tools/brally/crank.py) */
/* @implements 0x1005FF00 glide BrRaceGateStep */
void BR_THISCALL1 BrRaceGateStep(BrDriver *pDrv)
{
    BrDriverCar *pCar;
    BrDriverCar *pc;
    /* `gate` BEFORE `nGates`: for `gate + nGates` VC5 makes the LATER-
     * declared symbol the two-operand add's DESTINATION (the pairwise
     * declaration-order tie-break, docs/brally/VC5-IDIOMS.md), and the original's
     * destination at 0x10060097 is the gate-count register. */
    int32_t      gate, nGates, q, iCur, iNext, iMode, iBest, i;
    float        tLap, dGap, vScale, fRatio;
    const char  *pszMsg;
    short        iWeather;

    nGates = g_brTrkHdr.nGate;                     /* 0x1005FF08 */
    if (nGates == 0)                            /* 0x1005FF0F */
        return;

    pCar = pDrv->pCar;                          /* 0x1005FF17 */
    if (pCar != NULL) {
        /* FIELD BY FIELD, not a struct assignment: a 12-byte struct copy
         * makes VC5 take the address of both sides (`lea eax,[ebx+0x30]` +
         * a base register for the destination); the original walks eax /
         * ecx / edx through six independent dword moves. */
        pDrv->f00.x = pCar->pos.x;              /* +0x30  -> +0x00 */
        pDrv->f00.y = pCar->pos.y;
        pDrv->f00.z = pCar->pos.z;
        pDrv->f0C.x = pCar->posPrev.x;          /* +0xF80 -> +0x0C */
        pDrv->f0C.y = pCar->posPrev.y;
        pDrv->f0C.z = pCar->posPrev.z;
        pDrv->f48 = pCar->gateHi;
        pDrv->f4C = pCar->gate;
        pDrv->f40 = pCar->lap;
        pDrv->f44 = pCar->lapB;
        pDrv->f30 = pCar->tRun;
        pDrv->f34 = pCar->tBest;
        pDrv->f50 = pCar->fFF4;
        nGates = g_brTrkHdr.nGate;                 /* 0x1005FF8E */
    }

    /* The floor-modulus, and the two arms are an if/ELSE producing one
     * value -- `if (gate < 0) gate = ...` re-assigns the same variable and
     * costs a pair of register copies at the join. */
    gate = pDrv->f4C;                           /* 0x1005FF94 */
    if (gate < 0)
        q = nGates - ((-1 - gate) % nGates) - 1;  /* 0x1005FF9B */
    else
        q = gate;                               /* 0x1005FFAA */
    iCur  = q % nGates;                         /* 0x1005FFAC */
    iNext = (iCur + 1) % nGates;                /* 0x1005FFBE */

    tLap = (float)BrFtolArg(pDrv->f30 * BR_RACE_K100) * BR_RACE_K001;

    /* ---- did it cross the gate it is standing on? (backwards) ---------- */
    if (BrSeg2Intersect(&g_brTrkHdr.aGate[iCur].postB, &g_brTrkHdr.aGate[iCur].postA,
                        (const BrVec2 *)&pDrv->f0C,
                        (const BrVec2 *)&pDrv->f00) != 0) {
        BrPodNop();
        if (iCur == 0)                          /* 0x10060026 */
            pDrv->f44 -= 1;

        /* The predicate is built ONLY to be printed; the branch below then
         * recomputes the modulus from memory and tests that. */
        BrPodNop();

        /* ONE conjunction, which is what the line above prints: the two
         * `jne`/`jge` go to the same target.  Both add operands are NAMED
         * (the count re-read here, as at 0x1005FF8E): unnamed, VC5 keeps
         * the field's register as the add destination; named, the later-
         * declared `nGates` is the destination, which is the original. */
        if (iCur == pDrv->f48 % (nGates = g_brTrkHdr.nGate)   /* 0x10060089 */
            && (gate = pDrv->f4C) < 0) {                   /* 0x10060090 */
            pDrv->f4C = gate + nGates;                     /* 0x10060097 */
            pDrv->f44 += 1;
            if (BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot) != NULL)           /* 0x100600A0 */
                pDrv->f50 += BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot)->aPt[0].arc;
            BrPodNop();
        }
        pDrv->f4C -= 1;                         /* 0x100600C3, both arms */
        goto tail;
    }

    /* ---- did it cross the next one? (forwards) ------------------------- */
    if (BrSeg2Intersect(&g_brTrkHdr.aGate[iNext].postB, &g_brTrkHdr.aGate[iNext].postA,
                        (const BrVec2 *)&pDrv->f0C,
                        (const BrVec2 *)&pDrv->f00) == 0)
        goto tail;                              /* 0x100600EE */

    BrPodNop();
    pDrv->f4C += 1;                             /* 0x10060113 */

    if (pDrv->f4C > pDrv->f48) {                /* 0x10060119, `jle` */
        pDrv->f48 = pDrv->f4C;
        /* Read, printed, never applied. */
        BrPodNop();

        if (iNext != 0)                         /* 0x1006013D */
            goto positions;

        if (pDrv->f40 < (*(int32_t *)&g_CBE8)) {         /* 0x1006014D, `jge` */
            pszMsg = NULL;                      /* 0x1006015E */

            /* ---- the best lap, 0x10060155 --------------------------- */
            if (BR_FCOM_EQ0(pDrv->f34) || !(tLap >= pDrv->f34)) {
                pDrv->f34 = tLap;               /* 0x1006017E */
                if (pDrv->f64 < g_brRaceNEntrant) {
                    pDrv->pCar->lapBest = pDrv->f40;     /* 0x1006018D */
                    if (BR_FCOM_EQ0(
                            (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestLap[g_Br0B380C])
                        || pDrv->f34
                             < (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestLap[g_Br0B380C]) {
                        (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestLap[g_Br0B380C]
                            = pDrv->f34;                 /* 0x100601DA */
                        pszMsg = BrStrGet(0x109);
                    } else if (pDrv->f40 != 0) {         /* 0x100601C7 */
                        pszMsg = BrStrGet(0x10A);
                    }
                }
            }

            BR_RACE_RECORD_LAP();               /* 0x100601EF */
            pDrv->f40 += 1;                     /* 0x10060226 */
            pDrv->f44 = pDrv->f40;

            if ((*(int32_t *)&g_brRaceRules.mode) == BR_RACE_MODE_WRAP) {     /* 0x10060232 */
                if (pDrv->f40 == 1) {
                    BrPodNop();
                    BR_RACE_WRAP_UNWIND();
                }
            } else if (pCar != NULL) {          /* 0x1006029A */
                BrPodNop();
                if (pDrv->f40 == (*(int32_t *)&g_CBE8) - 1)       /* 0x100602BA */
                    strcpy(pCar->sz100C, BrStrGet(0x10B));
                else
                    sprintf(pCar->sz100C, BrStrGet(0x10C), pDrv->f40 + 1);
                pCar->pszBanner  = (char *)pCar->sz100C;     /* 0x1006031A */
                pCar->f1000 = 1.0f;
                if (pszMsg != NULL) {                    /* 0x10060325 */
                    pCar->psz1004 = (char *)pszMsg;
                    pCar->f1008 = 1.0f;
                }
            }
        }

        /* ---- THE FINISH CONDITION, 0x1006033B ------------------------- */
        if (pDrv->f40 == (*(int32_t *)&g_CBE8)) {
            pDrv->f68 |= BR_RACE_FINISHED;      /* 0x1006034E, before the
                                                 * car test -- a driver with
                                                 * no car still finishes */
            if (pCar != NULL) {
                pCar->f29B0  = BR_RACE_KFINISH;          /* 0x10060362 */
                pCar->tFinal = pCar->tFinal - pDrv->f30;
                pDrv->f30    = BR_RACE_KZERO;
                pCar->fFF8   = g_brRaceNFinished;
                if ((*(int32_t *)&g_brRaceRules.mode) == 6)                   /* 0x1006038D */
                    pCar->pszBanner = (char *)"";
                else
                    pCar->pszBanner = (char *)BrStrGet(
                        g_aBrRacePlaceMsg[g_brRaceNFinished]);
                pCar->f1000 = 5.0f;                      /* 0x100603BA */

                if (pDrv->f64 < g_brRaceNEntrant && (*(int32_t *)&g_CBE8) == 3) {
                    if (BR_FCOM_EQ0(
                            (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestTotal[g_Br0B380C])
                        || !(pCar->tFinal
                             >= (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestTotal[
                                    g_Br0B380C])) {
                        (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestTotal[g_Br0B380C]
                            = pCar->tFinal;              /* 0x10060417 */
                        pCar->f1000 = 1.5f;
                        pCar->psz1004 = (char *)BrStrGet(0x121);
                        pCar->f1008 = 3.5f;
                    }
                }
            }
            g_brRaceNFinished += 1;             /* 0x10060440 -- reached from
                                                 * the no-car path too */
            goto tail;                          /* the finish arm SKIPS the
                                                 * standings recompute */
        }

    positions:
        /* ---- the standings pass, 0x1006044B --------------------------- */
        iMode = (*(int32_t *)&g_brRaceRules.mode);
        if (iMode == 1 || iMode == 6 || iMode == 2) {
            if (pDrv->f64 >= g_brRaceNEntrant)  /* 0x1006046D */
                goto tail;
            if ((DAT_105ccb68[8]) == 0)            /* 0x1006047A */
                BrSfxSrcBeep();

            /* dGap is DELIBERATELY not initialised: when the field has one
             * car, or when no entrant beats the running best, the original
             * reads the slot as it stands (0x100604A8 / 0x100604D1 both
             * `fld [esp+0x18]` with nothing having written it). */
            if ((*(int32_t *)&g_brRaceRules.mode) == 1) {            /* 0x10060481 */
                dGap = g_aBrRaceCar[0].fFF4 - g_aBrRaceCar[1].fFF4;
            } else {
                iBest = 0xFF;                   /* 0x1006049E */
                for (i = 1; i < g_BrCarCount; i++) {
                    if (iBest > g_aBrRaceCar[i].fFF8) {
                        dGap  = g_aBrRaceCar[0].fFF4 - g_aBrRaceCar[i].fFF4;
                        iBest = g_aBrRaceCar[i].fFF8;
                    }
                }
            }
            if (!(dGap >= BR_RACE_KZERO))       /* 0x100604DD, C0 */
                dGap = -dGap;

            vScale = pDrv->pCar->f1030 * 0.4464285671710968f;
            /* `!=`, not a negated `==`: C3 clear falls THROUGH to the divide
             * and the jne goes to the 1000 arm. */
            if (vScale != BR_RACE_KZERO) {      /* 0x100604FB, C3 */
                if (vScale < 25.0f)             /* 0x10060508 */
                    vScale = 25.0f;
                fRatio = dGap / vScale;
            } else {
                fRatio = 1000.0f;
            }

            for (i = 0; i < g_brRaceNEntrant; i++) {     /* 0x1006052D */
                pc = g_aBrRaceDriver[i].pCar;

                if (pc->pszBanner != (char *)pc->sz100C) {   /* 0x1006055C */
                    pc->pszBanner  = (char *)BrStrGet(0x122);
                    pc->f1000 = 0.4f;
                } else {
                    strcpy(pc->sz2ABC, pc->sz100C);      /* 0x1006057F */
                    pc->pszBanner = (char *)pc->sz2ABC;
                }

                /* The weather row, clamped BY A 16-BIT SIGNED COMPARE --
                 * br_carphys.h records the same shape. */
                iWeather = (short)((*(int32_t *)&DAT_104b15e8) - 1);   /* 0x100605AE */
                if (iWeather > 2 || iWeather < 0)
                    iWeather = 0;
                pc->fFF0 = g_apBrRaceDiff[g_Br0B380C]->aAward[
                               ((*(int32_t *)&g_aBrRaceCar[0].f0E64) * 3 + iWeather) * 7 + iNext]
                           + pc->fFF0;

                if (!BR_FCOM_EQ0(pc->tFinal) && g_BrCarCount > 1) {
                    sprintf(pc->sz100C, "%%ry");         /* 0x10060623 */
                    BrTimeFormat(pc->sz100C + 3, fRatio);
                    if (pc->fFF8 != 0) {                 /* 0x1006064C */
                        if (!(fRatio <= 120.0f))         /* 0x1006065C, C3|C0 */
                            pc->psz1004 = (char *)BrStrGet(0x123);
                        sprintf(pc->sz100C + strlen(pc->sz100C),
                                BrStrGet(0x124));
                    }
                    pc->psz1004 = (char *)pc->sz100C;     /* 0x1006069F */
                    pc->f1008 = BR_RACE_KFINISH;
                }

                pc->gateHi = pDrv->f48;                  /* 0x100606BA */
            }
        }
        goto tail;
    }

    /* --- forward, but back over ground already covered ------------------ */
    if (iNext != 0)                             /* 0x100606E6 */
        goto tail;

    pDrv->f44 += 1;                             /* 0x100606F4 */
    if (pDrv->f44 > pDrv->f40) {                /* `jle` */
        pszMsg = NULL;

        if (BR_FCOM_EQ0(pDrv->f34) || !(tLap >= pDrv->f34)) {   /* 0x10060702 */
            pDrv->f34 = tLap;                   /* 0x10060727 */
            if (pDrv->f64 < g_brRaceNEntrant) {
                pDrv->pCar->lapBest = pDrv->f40;         /* 0x10060737 */
                if (BR_FCOM_EQ0(
                        (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestLap[g_Br0B380C])
                    || pDrv->f34
                         < (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestLap[g_Br0B380C]) {
                    (*(BrRaceRecords * *)&g_aBrRaceCar[0].pEquip)->aBestLap[g_Br0B380C] = tLap;
                    pszMsg = BrStrGet(0x109);            /* 0x10060780 */
                } else if (pDrv->f40 != 0) {
                    pszMsg = BrStrGet(0x10A);
                }
            }
        }

        BR_RACE_RECORD_LAP();                   /* 0x10060796 */
        pDrv->f40 = pDrv->f44;                  /* 0x100607CF -- assignment,
                                                 * not an increment */
        if (pCar != NULL) {                     /* 0x100607CD */
            BrPodNop();
            sprintf(pCar->sz100C, BrStrGet(0x10C), pDrv->f40 + 1);
            pCar->pszBanner  = (char *)pCar->sz100C;         /* 0x1006080F */
            pCar->f1000 = 1.0f;
            if (pszMsg != NULL) {
                pCar->psz1004 = (char *)pszMsg;
                pCar->f1008 = 1.0f;
            }
        }
    }

    BrPodNop();

    /* NOTE, and it is the original's: this arm has NO finish test. */
    if ((*(int32_t *)&g_brRaceRules.mode) == BR_RACE_MODE_WRAP && pDrv->f40 == 1) {
        BrPodNop();
        BR_RACE_WRAP_UNWIND();                  /* 0x10060860 */
    }

tail:
    if (pCar != NULL) {                         /* 0x1006089F */
        pCar->gateHi = pDrv->f48;
        pCar->gate   = pDrv->f4C;
        pCar->lap    = pDrv->f40;
        pCar->lapB   = pDrv->f44;
        pCar->tRun   = pDrv->f30;
        pCar->tBest  = pDrv->f34;
        pCar->fFF4   = pDrv->f50;
    }
}

#undef BR_FCOM_EQ0
#undef BR_RACE_RECORD_LAP
#undef BR_RACE_WRAP_UNWIND

