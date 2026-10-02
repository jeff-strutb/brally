/* br_race.h -- lap, gate and finish bookkeeping.
 *
 * WHAT THIS IS
 *
 * Boss Rally's race is not a lap counter over a "start/finish line". It is a
 * ring of GATES. Every frame, each driver's motion segment (last frame's
 * position -> this frame's position) is tested against two of them: the gate
 * it is standing on and the next one round. Crossing the next one advances the
 * gate counter; crossing the current one puts it back. A lap is completed when
 * the gate the driver advances INTO is gate 0.
 *
 * That is why there is no "crossed the line" predicate anywhere in the binary,
 * and why the lap counter can go backwards.
 *
 * ADDRESSES
 *
 *   Glide 0x1005FF00 (2538 B)  ==  D3D 0x10066E90 (2534 B)
 *
 * config/shared.csv classes 0x10066E90 `d3d_only`, which it is NOT: the two
 * maps disagree about the extent by four bytes of trailing padding and a
 * crossdiff pair only matches on equal extents. Both builds have the function,
 * both reference the same ten debug format strings, and the Glide build is
 * what this transcription was read from. slice3_41.h lists 0x10066E90 under
 * "NOT PORTED -- 2534 bytes, likewise"; this module is the part of it that
 * could be resolved.
 *
 * Neighbours, for whoever picks this up next (Glide / D3D):
 *
 *   0x1005F310 / 0x100662A0   driver-record constructor; grid placement.
 *                             `shared`. Documented in slice3_41.h.
 *   0x1005F6C0 / 0x10066650   "saving/restoring lap (%d/%d) and gate (%d/%d)"
 *   0x10060A30 / 0x100679C0   "SAVING LAST LAP INFO"; race-start reset.
 *   0x1005EB90 / --           position-along-track from a distance; the source
 *                             of the constructor's initial lap and gate.
 *   0x100350F0 / 0x1003BA70   the 2-D segment intersection. ALREADY PORTED as
 *                             BrSeg2Intersect in slice2_21.c -- this module
 *                             calls it and does not re-transcribe it.
 *
 * THE STATE, AND WHERE IT LIVES
 *
 * Two copies of the same seven fields, mirrored in and out on every call:
 *
 *   - the DRIVER record, slice3_41.h's `BrDriver`, 0x80 bytes, Glide array
 *     base 0x10AF07F8 / D3D 0x10ACD498, one per participant. This is the
 *     working copy.
 *   - the CAR record, the 0x2B68-byte entity, reached through BrDriver::pCar
 *     (+0x60). This is the copy everything else in the engine reads.
 *     slice3_41.h's `BrDriverCar` models the fields involved.
 *
 * A driver whose pCar is NULL (a phantom entrant -- the constructor makes one
 * for every slot past the car count) keeps its state only in the driver
 * record. The mirror is skipped at both ends, which is why the whole function
 * still works for it.
 *
 * The gate ring itself is a separate table, Glide 0x106EED70, stride 0x14,
 * with the count at 0x106EEE38. Only three of its five dwords are read here.
 *
 * WHAT IS NOT IN THIS MODULE, AND WHY
 *
 * 0x1005FF00 also does four things THE PORT ARM leaves out. None of them
 * feeds back into the lap state; each is named here so the omission is a
 * decision and not a silence.
 *
 * As of 2026-09-03 the MATCHING arm (BR_MATCHING_BUILD) has all four, and
 * that is what took the function from 581 bytes to the original's 2,538.
 * The list below therefore describes the port arm only.
 *
 *   1. Ten debug printf calls (Glide 0x10008D60). Pure output. Their format
 *      strings are the best documentation in the binary and are quoted at the
 *      line they belong to in br_race.c.
 *   2. The HUD banner: car +0xFFC/+0x1000/+0x1004/+0x1008 and an sprintf into
 *      the car's own +0x100C text buffer, spelling "LAP 2", "FINAL LAP",
 *      "1st". Needs the string table behind Glide 0x1006D280.
 *   3. The two global RECORD tables at Glide 0x10AF2094 (+0xB0 best lap,
 *      +0x10C best total, indexed by track). Needs that object typed.
 *   4. The standings recompute at Glide 0x1006044B, ~600 bytes over the whole
 *      field, reached on every gate advance that is not a finish. It belongs
 *      with slice3_41.h's BrRankAssign, not here.
 *
 * WHAT THIS MODULE DOES REPRODUCE: every write to the seven mirrored fields,
 * the flag at +0x68, the per-lap time array, the finishing order, and the
 * mode-3 wrap. Those are the whole of the lap/gate/finish state machine.
 */
#ifndef BR_RACE_H
#define BR_RACE_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>

#include "br_vec.h"
#include "br_trkhdr.h"   /* BrRaceGate, g_brTrkHdr */
#include "slice2_21.h"   /* BrVec2, BrSeg2Intersect -- 0x1003BA70 */
#include "slice3_41.h"   /* BrDriver, BrDriverCar, BR_RACE_LAPTIME_MAX */

/* ==========================================================================
 * The gate ring
 * ========================================================================== */

/* BrRaceGate: br_trkhdr.h -- the ring is part of the loaded track header. */

/* ==========================================================================
 * The globals 0x1005FF00 reads, gathered
 * ========================================================================== */

/* GAME MODE, and it is worth being explicit about the identity: this is
 * Glide 0x100A9360 == D3D 0x100AA010 == slice2_26.h's `n0AA010`, the field
 * every phase-activate routine assigns. The menu front end and the lap
 * counter are reading and writing the same word. BrPhaseActivate_100447D0
 * sets it to 6 on the way into a race.
 *
 * Only mode 3 changes what this module does: it is the mode whose lap counter
 * is pinned at zero (see BR_RACE_MODE_WRAP below). */
#define BR_RACE_MODE_WRAP  3

/* One card of the credits roll (0x100A9368, stride 0x20): its title, how
 * long it stays up, and up to six lines. */
typedef struct BrCreditCard {
    const char *pszTitle;        /* +0x00  NULL ends the roll */
    float       t;               /* +0x04  seconds on screen  */
    const char *apszLine[6];     /* +0x08  NULL ends the card */
} BrCreditCard;

#define BR_CREDIT_CARDS 15

/* 0x100A9354 -- the frame-rate ring's bookkeeping, the game mode and the
 * credits roll.  (The name is historical: the port lane once gathered the
 * gate-step inputs under it.) */
typedef struct BrRaceRules {
    int32_t      f00;            /* +0x00 */
    int32_t      nFpsSamples;    /* +0x04  length of the frame-delta ring */
    int32_t      iFpsSample;     /* +0x08  its write index; 0 = the ring just wrapped */
    int32_t      mode;           /* +0x0C  0x100A9360, the game mode */
    int32_t      f10;            /* +0x10 */
    BrCreditCard aCard[BR_CREDIT_CARDS]; /* +0x14 */
} BrRaceRules;

/* ==========================================================================
 * The pieces
 * ========================================================================== */

/* 0x1005FF9B..0x1005FFAC -- unwrapped gate number to ring index.
 *
 * A floor-modulus, spelled out by the original because x86 `idiv` truncates:
 * the negative arm computes  nGates - ((-1 - gate) % nGates) - 1.
 *
 * GOTCHA: the original then takes `% nGates` of the result a second time
 * (0x1005FFAC/0x1005FFB0), which is a no-op on the value it has just
 * normalised. Both steps are kept so the arithmetic can be diffed.
 *
 * Undefined for nGates <= 0 in the original (`idiv` faults on 0); the caller
 * has already returned in that case and this function is not reached. */
int32_t BrRaceGateIndex(int32_t gate, int32_t nGates);

/* 0x1005FFAD..0x1005FFE3 -- the lap time, truncated to hundredths.
 *
 *     BrFtolTrunc(t * 100.0f) * 0.01f
 *
 * TRUNCATION, not rounding, and it is __ftol's truncation: br_crt.h's
 * BrFtolTrunc returns 0 for anything out of int32 range, NaN included. So a
 * runaway or NaN lap timer records a lap time of exactly 0.0 -- which then
 * compares as a new best. Preserved; see the test. */
float BrRaceTruncHundredths(float t);

/* The entry and exit mirrors, 0x1005FF17..0x1005FF8E and
 * 0x100608A3..0x100608DC. Seven fields each way, and NOT the same seven in
 * the same order -- the load also brings across the two positions, which the
 * store does not send back. Both are no-ops when pDrv->pCar is NULL. */
void BrRaceLoadFromCar(BrDriver *pDrv);
void BrRaceStoreToCar(BrDriver *pDrv);

/* 0x1005FF00 -- one frame of one driver's gate bookkeeping.
 *
 * Returns the number of laps completed on this call: 0 normally, 1 on a lap,
 * and 0 again when mode 3 immediately unwinds it. The original returns void;
 * this is a port-only convenience for callers and tests, and no behaviour
 * depends on it.
 *
 * `pRules->nFinished` is READ AND WRITTEN: it is the shared "who finished
 * next" counter, and it is incremented for every driver that reaches the
 * flag, whether or not that driver has a car. */

/* ==========================================================================
 * THE MATCHING ARM'S VIEW OF THE SAME STATE
 *
 * The original takes ONE argument, in ecx (`mov ebp, ecx` at 0x1005FF05, and
 * `ret` with no immediate at 0x100608E9), so it is a one-argument thiscall --
 * BR_THISCALL1.  Everything BrRaceRules gathers is a SEPARATE ABSOLUTE
 * GLOBAL in the original: 0x1005FF00 reads 0x106EEE38 with
 * `mov ecx, dword ptr [0x106EEE38]`, not through any base register, and it
 * re-reads it four times inside one block.  A struct pointer costs a base
 * register and cannot produce that, which is the accessor sub-case
 * docs/VC5-IDIOMS.md records.  The port arm keeps BrRaceRules; this arm
 * spells the globals out.
 * ========================================================================== */

#include "br_match.h"    /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

/* 0x106EED48 holds a POINTER (`mov eax,[0x106EED48]; test eax,eax;
 * fld [eax+0x64]`), so the lap length is a FIELD of the object, not a
 * standalone float: the object is the path ring root node (track header
 * +0x70, g_brTrkHdr.aPathRoot) and +0x64 is its aPt[0].arc. */

/* 0x10AF2094 -> the two per-track record tables, both indexed by the chosen
 * track (0x100B3014).  Best lap at +0xB0, best total at +0x10C. */
typedef struct BrRaceRecords {
    uint8_t _pad000[0xB0];
    float   aBestLap[(0x10C - 0xB0) / 4];   /* +0x0B0 */
    float   aBestTotal[1];                  /* +0x10C */
} BrRaceRecords;

/* 0x100BCAB0[track] -- the difficulty/award object.  Only its float array at
 * +0x2C is read here, with a computed index. */
typedef struct BrRaceDiffRec {
    uint8_t _pad00[0x2C];
    float   aAward[1];                  /* +0x2C */
} BrRaceDiffRec;

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x106EEE38                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x106EED70, stride 0x14      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x106EED48                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x100BCBE8                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x100A9360                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x118EE588                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100B3858                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x100B2F04                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x105CCB88                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100B3014                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x104B15E8                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10AF206C                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AF2094                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100BCAB0                   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100B3168 -- 0x10D, 0x10E.. */

/* The two record arrays are ARRAYS in the original, not pointers: every
 * access in 0x1005FF00 is absolute (`fld dword ptr [0x10AF21FC]` is
 * 0x10AF1208 + 0xFF4, i.e. g_aBrRaceCar[0].fFF4).  br_racestep.h's
 * g_pBrRaceCar / g_pBrRaceDriver are the port's pointer view of the same
 * storage and are deliberately NOT reused here. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AF1208, stride 0x2B68    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AF07F8, stride 0x80      */

/* BrRaceGateStep: prototype in br_funcs.h */
































#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif



























/* BR_GLOBALS_BEGIN: generated by ports/64b/tools/unify.py */
#ifdef __cplusplus
extern "C" {
#endif
#pragma push_macro("g_brRaceRules")
#undef g_brRaceRules
extern BrRaceRules g_brRaceRules;  /* 0x100A9354 */
#pragma pop_macro("g_brRaceRules")
#pragma push_macro("g_apBrRaceDiff")
#undef g_apBrRaceDiff
extern BrRaceDiffRec * g_apBrRaceDiff[];  /* 0x100BCAB0 */
#pragma pop_macro("g_apBrRaceDiff")
#ifdef __cplusplus
}
#endif
/* BR_GLOBALS_END */
#endif /* BR_RACE_H */
