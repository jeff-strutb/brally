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
 * 0x1005FF00 also does four things this port leaves out. None of them feeds
 * back into the lap state; each is named here so the omission is a decision
 * and not a silence.
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

#include <stdint.h>

#include "br_vec.h"
#include "slice2_21.h"   /* BrVec2, BrSeg2Intersect -- 0x1003BA70 */
#include "slice3_41.h"   /* BrDriver, BrDriverCar, BR_RACE_LAPTIME_MAX */

/* ==========================================================================
 * The gate ring
 * ========================================================================== */

/* Glide 0x106EED70, stride 0x14. The two posts are handed to BrSeg2Intersect,
 * which reads only offsets 0 and 4 of each of its four arguments -- so the
 * gate is genuinely a 2-D segment and the driver's Z is never consulted.
 *
 * `tAward` is read once, to be printed ("moved ahead one gate, getting %f
 * seconds"). NOTHING in 0x1005FF00 applies it. Either the award is applied by
 * the checkpoint-timer code elsewhere, or the message outlived the feature;
 * this module does not guess which. */
typedef struct BrRaceGate {
    BrVec2 postA;     /* +0x00 */
    BrVec2 postB;     /* +0x08 */
    float  tAward;    /* +0x10 */
} BrRaceGate;

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

typedef struct BrRaceRules {
    const BrRaceGate *aGates;    /* 0x106EED70                              */
    int32_t           nGates;    /* 0x106EEE38 -- zero makes the step a nop */
    int32_t           nLaps;     /* 0x100BCBE8 -- the finish condition      */
    int32_t           mode;      /* 0x100A9360                              */
    int32_t           nFinished; /* 0x118EE588 -- next finishing position   */
    /* 0x106EED48 -> +0x64: the length of one lap, in the same units as the
     * driver's +0x50 progress key. NULL models the original's NULL pointer
     * test, on which the whole progress fixup is skipped. */
    const float      *pfLapLength;
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
int BrRaceGateStep(BrRaceRules *pRules, BrDriver *pDrv);

#endif /* BR_RACE_H */
