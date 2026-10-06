/* br_racestep.c -- 0x10019A70, the race step, and the two per-driver passes
 * it drives.  See br_racestep.h for the mechanism, the address pairs and the
 * blocks of the original that are deliberately not here.
 *
 * Transcribed from reference/brally/orig/BRGlide.dll.  Every branch below carries the address
 * of the instruction it is, so the two can be diffed.
 */
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_mat.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <stddef.h>
#include <string.h>

/* br_racestep.h declares this cdecl; the original is thiscall with the driver
 * in ecx and no stack argument, which BR_THISCALL1 reproduces exactly. Hide
 * the prototype so the matching definition is not a C2373 redefinition. */
#define BrRaceDriverAnim BrRaceDriverAnim_cdecl
#include "br_racestep.h"
#include "br_racebegin.h"
#undef BrRaceDriverAnim
#include "br_gamestep.h"

/* ==========================================================================
 * The data, read out of BRGlide.dll rather than assumed
 * ========================================================================== */

/* 0x100A9578, eight {int32 state; float seconds} pairs.  Entry 8 onward is
 * unrelated .rdata (0x100A95C0 is the start of a string), and the original
 * never indexes past 7: the script's last entry is state 7, whose arm does
 * not advance. */
const BrRaceLightStep g_aBrRaceLightScript[BR_RS_SCRIPT_LEN] = {
    { 0, 0.0f },   /* 0x100A9578 */
    { 1, 2.2f },   /* 0x100A9580 */
    { 2, 2.3f },   /* 0x100A9588 -- the 3-2-1 */
    { 3, 2.0f },   /* 0x100A9590 -- GREEN */
    { 4, 0.0f },   /* 0x100A9598 -- RACING; 0.0 and no timer arm */
    { 5, 2.0f },   /* 0x100A95A0 */
    { 6, 2.0f },   /* 0x100A95A8 */
    { 7, 0.0f }    /* 0x100A95B0 */
};

/* 0x100A9548.  The state-2 timer counts DOWN from 2.3, so these fire in
 * order.  See br_racestep.h for why there are five of them. */
const float g_aBrRaceBeepT[BR_RS_BEEP_COUNT] = {
    2.15f,   /* 0x100A9548 */
    1.50f,   /* 0x100A954C */
    0.85f,   /* 0x100A9550 */
    0.20f,   /* 0x100A9554 */
    0.00f    /* 0x100A9558 -- what the unbounded index reaches, and the
              *               reason the countdown stops at four sounds  */
};

/* ==========================================================================
 * The holes
 * ========================================================================== */

uint32_t g_aBrRaceStepHole[BR_RS_HOLE_COUNT];

static const char *const g_aBrRaceStepHoleName[BR_RS_HOLE_COUNT] = {
    "car+0xF08 control chain -> 0x1006F170",
    "0x10060A30 lap info      (286 B)",
    "0x1006E9E0..0x1006EBC0 skid/trail",
    "0x100623A0 -> 0x1005ACE0 + 0x10001CF0",
    "0x10060E00 / 0x10060DF0 countdown sound",
    "0x1001B27A HUD + mirror + render (~5.5 KB)",
    "0x1005F310 grid placement (538 B)",
    "0x1005F6C0 lap save/restore (2104 B)",
    "0x1001AD08 difficulty -> car+0xFF0",
    "0x1005C450 scratch clear (62 B)",
    "0x1005ECF0 walked off the track image"
};

/* (port-only BrRaceStepHoleReset removed) */


/* (port-only BrRaceStepHoleName removed) */


BrRaceStepHooks g_brRaceStepHooks;

#define BR_RS_HOLE(id, hook, arg)                       \
    do {                                                \
        ++g_aBrRaceStepHole[(id)];                      \
        if (g_brRaceStepHooks.hook != NULL)             \
            g_brRaceStepHooks.hook(arg);                \
    } while (0)

/* ==========================================================================
 * The globals
 * ========================================================================== */

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

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x3D088889, BR_PHYS_DT */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* a fixed-timestep host ticks always */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
const BrTrack *g_pBrRaceTrack;

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

void        (*g_pfnBrRaceAiControl)(BrDriverCar *);

/* ==========================================================================
 * 0x1005ECF0 -- the path walk
 * ========================================================================== */

/* WHAT IT DOES: slides a position a given distance forward along the track's
 * built-in racing line, stepping from one stretch of the line to the next and
 * skipping over stretches marked to be ignored, and leaves the resulting
 * position and place-in-the-line where the caller can pick them up. This is
 * how a car that has no physics of its own -- an entrant the player never
 * sees driving -- is moved round the circuit. If it runs out of line it leaves
 * the answer untouched rather than reporting an error. */
/* declared only (the Mac port keeps its own body in ports/brally-wasm/patch/); Glide match is src/brally/core/generated/0x1005ECF0.c
 * (the original walks relocated node POINTERS and reads the fields in place;
 * the BrAiNodeAt/BrAiPoint_ bounds-checked accessors below are a port
 * addition, as is the local BrVec3 the two lerps write into). */
/* BrRacePathAdvance: prototype in br_funcs.h */

/* NOT A PORT, and it is here rather than in a host because it is the exact
 * INVARIANT the phantom arm above requires, which is a fact about
 * 0x10061F60 and belongs beside it.
 *
 * 0x10062260 forms  (f44 + 1) * lapLen - f50 - pts[i+1].arc  and divides it
 * by the segment length to get how much of the segment is still ahead.  For
 * that to be a fraction in [0, 1], the slot's progress key must satisfy
 *
 *      f50 == (f44 + 1) * lapLen - arc(position)
 *
 * and the arm keeps it: f50 accumulates exactly the distance walked, so it
 * gains a lap length over the same circuit on which f44 gains one.  What it
 * cannot do is ESTABLISH it -- a slot seeded with the wrong triple walks off
 * the ring on its first lap rollover, extrapolating along one segment for
 * ever, and the position dump looks superficially plausible while doing it.
 *
 * The original establishes it in 0x1005F310 through 0x1005EB90, "position
 * along track from a distance", which is 538 + n bytes this module does not
 * own -- so this is a seed, not a transcription, and it is counted as the
 * grid hole.  `dist` is metres past the path root; it must put the slot
 * PAST gate 0, or the slot's first act is a backwards crossing that takes
 * f44 to -1 while f50 stays where it was and breaks the invariant. */
/* (port-only BrRaceSeedPhantom removed) */


/* ==========================================================================
 * 0x10061430 -- eleven bytes, and all of them
 * ========================================================================== */

/* WHAT IT DOES: clears one value on a car at the very start of every race
 * frame, before anything else touches it. What that value is FOR is unknown --
 * nothing transcribed so far reads it -- so all that can honestly be said is
 * that each car begins the frame with it at zero. */
/* @implements 0x10061430 glide BrRaceCarPre */
void BR_THISCALL1 BrRaceCarPre(BrDriverCar *pCar)
{
    /* `mov dword ptr [ecx+0xF78], 0` / `ret`.  Nothing ported here reads
     * car+0xF78; the store is kept because the whole function is the store. */
    pCar->fF78 = 0;
}

/* ==========================================================================
 * 0x10061F60 -- one driver, part one
 * ========================================================================== */

/* 0x10062046 / 0x1006212B / 0x100621E6 -- the same three dwords, three times.
 * car+0x30 is the position the physics wrote and car+0xF80 is last frame's;
 * 0x1005FF00 reads exactly this pair as its motion segment. */
/* (port-only BrRaceMirrorPos removed) */


/* 0x10062064 / 0x1006214C / 0x10062204 -- `mov ecx,[eax+0xF08]; test; call`.
 * The original does NOT null-check on the frozen arm and DOES on the other
 * two; both spellings are kept by checking here and by the callers' order. */
/* (port-only BrRaceControl removed) */


/* 0x10062238 -- THE PHANTOM ENTRANT */
/* (port-only BrRaceDriverPhantom removed) */


/* (port-only BrRaceDriverStep removed) */


/* ==========================================================================
 * 0x100623A0 -- a pure hole, transcribed for its control flow only
 * ========================================================================== */

/* The car field the network gate passes as the slot index. +0x144 falls inside
 * BrDriverCar's `_pad144`, and slice3_41.h is a shared header, so it is read
 * through an offset here rather than by widening the struct. */

/* 0x1005ACE0 and 0x10001CF0 -- both thiscall with the car as their only
 * argument, so BR_THISCALL1 is exact for them. Neither is ported. */
/* BrSub1005ACE0: prototype in br_funcs.h */
/* BrSub10001CF0: prototype in br_funcs.h */
/* BrCarPredictRemote: prototype in br_funcs.h */

/* WHAT IT DOES: runs one driver's car animation for the frame. A driver with
 * no car does nothing. In a network race the car is animated only if the
 * prediction step says this slot is live -- for the local player's own car,
 * and for a slot whose prediction was suppressed, it is skipped entirely.
 * Off the network every car animates. */
/* @implements 0x100623A0 glide BrRaceDriverAnim */
void BR_THISCALL1 BrRaceDriverAnim(BrDriver *pDrv)
{
    /* pDrv->pCar is respelled at each use rather than hoisted: the original
     * re-reads [esi+0x60] before each of the two calls, which is what a call
     * clobbering eax forces, and a hoisted local changes the reload. */
    if (pDrv->pCar != NULL) {                         /* 0x100623A8 */
        if (g_brRaceNet != 0) {
            if (BrCarPredictRemote(pDrv->pCar,
                                   ((*(const int32_t *)&pDrv->pCar->iNetPlayer))) == 0)
                return;                               /* 0x100623BC */
        }
        BrCarWheelSteerStep_1005ACE0(pDrv->pCar);                    /* 0x100623CB */
        BrCamChaseStep(pDrv->pCar);                    /* 0x100623D3 */
    }
}

/* ==========================================================================
 * 0x100623E0 -- one driver, part two.  The car entrant's gate step.
 * ========================================================================== */

/* WHAT IT DOES: the tidying-up pass over one driver after the driving has been
 * done for the frame: lay down skid marks if the car has been sliding, credit
 * the car with any lap gates it has just crossed, work out how fast it is
 * actually travelling from how far it moved, and run down a short per-car
 * countdown. Nothing happens at all while the game is paused. */
/* declared only (the Mac port keeps its own body in ports/brally-wasm/patch/); Glide match is
 * src/brally/core/racing/BrRaceDriverPost_100623E0.cpp (a __thiscall member). */
/* BrRaceDriverPost: prototype in br_funcs.h */

/* ==========================================================================
 * The start-light state machine
 * ========================================================================== */

/* 0x1001AEE2..0x1001B08C.  The original builds the answer in a stack slot
 * seeded to 1 at 0x1001AE4E and cleared at 0x1001B085 for every driver that
 * is NOT finished; the loop body it skips is the per-slot results bookkeeping,
 * which is HUD state.  What survives is the predicate. */
/* (port-only BrRaceStepAllFinished removed) */


/* 0x1001B0F8 -- the advance itself, reached from three places. */
/* (port-only BrRaceStepAdvanceScript removed) */


/* 0x1001B0CD -- the timer arm.  Shared by fallthrough from the state-6 case
 * and by explicit jumps from the two arms below 4. */
/* (port-only BrRaceStepTimer removed) */


/* (port-only BrRaceStepLights removed) */


/* ==========================================================================
 * 0x10019A70
 * ========================================================================== */

/* 0x10019A70 IS DELIBERATELY UNCLAIMED, AND THIS IS WHERE THE CLAIM USED TO BE
 * ============================================================================
 * `/ * implements 0x10019A70 glide BrRaceStepInit * /` sat on this line and
 * it was false.  0x10019A70 is 11,223 bytes making 131 calls.  The body below
 * annotates 0x1001A97C..0x1001AA5E -- about 230 bytes, TWO PER CENT, and it
 * starts roughly 4 KB into the function.  Under that claim the address counted
 * as ported and the other 98% was neither transcribed, nor marked, nor counted
 * as missing.
 *
 * WHY IT WAS DROPPED RATHER THAN MOVED OR NARROWED
 *
 *   - NARROWED is not available.  tools/brally/manifest.py's form is
 *     `@implements 0xADDR BUILD SYMBOL` and nothing else; there is no
 *     sub-range or partial syntax anywhere in the tree.  A claim is
 *     whole-function or it is absent.
 *   - MOVED to BrRaceStepFrame was the obvious fix and is still wrong.
 *     BrRaceStepFrame is the right SHAPE -- it is 0x10019A70's entry and its
 *     substate branch, the per-frame arm the pump calls -- but it and
 *     BrRaceStepLights together cover 0x1001AB71..0x1001B261, about 1,780
 *     bytes.  That is 16%, not 100%.  Trading a 2% lie for a 16% one is not
 *     an honest outcome; it is a quieter one.
 *
 * SO THE ADDRESS READS AS UNPORTED, WHICH IT IS.  What exists is:
 *
 *     BrRaceStepInit    0x1001A97C..0x1001AA5E   the script seed          ~230 B
 *     BrRaceStepFrame   0x1001AB71..0x1001B261   the per-frame arm      ~1,780 B
 *     BrRaceStepLights  0x1001ABFB..0x1001B171   (inside the above)
 *
 * and, SINCE THIS NOTE WAS WRITTEN, port/src/racing/br_racebegin.c:
 *
 *     BrRaceStepClock   0x10019A70..0x10019AF8   the frame clock and the
 *                                                substate branch           144 B
 *     BrRaceStepBegin   0x10019AFE..0x1001AB70   THE WHOLE ONE-TIME ARM  4,209 B
 *
 * -- which is the first of the two blocks this note listed as missing.  It
 * delegates 0x1001A97C..0x1001AA5E back to BrRaceStepInit rather than
 * repeating it, so the five globals that block writes keep one host object
 * each.  br_racebegin.h carries the derivation, including the eight-byte
 * replay header whose writer and reader are both inside that arm.
 *
 * and, SINCE THIS NOTE WAS WRITTEN, two more below:
 *
 *     BrRaceStepEffects  0x1001B261..0x1001B364  the frame's screen-wide
 *                                                effects and the fly-past
 *                                                arming scan            260 B
 *     BrRaceStepSpecials 0x1001B365..0x1001B402  the special-object loop,
 *                        0x1001B870..0x1001B886  its three rotation arms
 *                                                and its tail            181 B
 *     BrRaceStepFlyPast  0x1001B403..0x1001B86F  the fly-past camera:
 *                                                the spline walk and the
 *                                                basis it builds       1,133 B
 *
 * What STILL does not exist:
 *
 *     0x1001B887..0x1001C646   the HUD, the rear-view mirror, the per-car
 *                              render marshalling, the pause and camera
 *                              input, the race exit and the frame limiter
 *                              -- 3,520 B, counted as BR_RS_HOLE_HUD
 *
 * so 7,703 of 11,223 bytes (68.6%) are transcribed and 31.4% are not.
 *
 * READ BEFORE STARTING 0x1001B887 -- three facts already off the bytes, and
 * one hazard:
 *   - 0x1001B887..0x1001B8AC is the per-car engine step: BrSndCarStep
 *     (thiscall) over 0x10AF1208 by 0x2B68, with the bound RE-READ from
 *     0x100B2F04 at the bottom of every pass.  Then BrSndNearestInvalidate.
 *   - 0x1001B8B3..0x1001B971 is the per-view sound pass over 0x106E86C8 by
 *     0x58.  Its index decode (`shl 3; sub; lea *5; shl 4; sub; lea *3` then
 *     scale 8) is x1389 then x8 == 0x2B68 -- it is a CAR RECORD stride, not a
 *     table of its own, so the value it loads is the field at +0x2734 of
 *     car[*pView] (0x10AF393C - 0x10AF1208 == 0x2734).  Offers go out through
 *     BrSndNearestOfferTrack when the fly-past is armed and through
 *     BrSndNearestOfferDefault for each entry of 0x105BC778 (count
 *     0x105BCAE8), then BrSndNearestCommit, BrRaceHudFrame, BrSndBankPickSlot.
 *   - !! HAZARD: 0x10008D60 is called with FIVE arguments at 0x1001B27A and
 *     0x1001B298 and with ONE at 0x1001B955 (`push edi; call; add esp,4`).
 *     It is BrPodNop, so both are harmless at runtime, but a single C
 *     prototype cannot spell both -- the matching arm needs two, and picking
 *     the wrong arity silently changes the caller's stack adjustment.
 *
 * !! 2026-09-10: THE CALLEE GATE IS SPENT.  All 64 distinct callees of the
 * remaining block already have symbols in this tree -- 115 of its 116 call
 * sites land on a report.csv row and the last (0x100325B0) is the C++ lane's
 * WM_DESTROY teardown.  What blocks this address now is transcription volume
 * (1,348 instructions), not unknown callees, and the frame cannot be won
 * from a fragment: `sub esp,0x34` is set by the WHOLE function's locals, so
 * it only becomes measurable once the last block is in.  The
 * claim still cannot be made, and putting one here would put the same defect
 * back.  0x1005F310 (538 B, the driver-record constructor and the grid
 * placement) is still the counter increment below.
 *
 * DO NOT READ "UNPORTED" AS "UNTOUCHED" AND SEND SOMEONE TO START OVER.
 * br_racestep.h is 500 lines of derivation for this address and the three
 * functions above are checked transcriptions of the parts they name.  The
 * missing 84% is asset loading and rendering, which this module does not own.
 * Restoring an @implements line here without also transcribing that 84% just
 * puts the same defect back. */

/* WHAT IT DOES: starts a race. It rewinds the starting-light sequence to the
 * beginning and clears the finished-driver count. A replay, or the one mode
 * that has no start procedure, skips straight past the lights to the racing
 * state. Putting the cars on the grid is the one thing it does NOT do that
 * the original does: that is a separate 538-byte routine this module does not
 * own, so the per-driver loop below only counts how many times it would have
 * been called. */
/* (port-only BrRaceStepInit removed) */


/* (port-only BrRaceStepFrame removed) */


/* (port-only BrRaceStepInstall removed) */


/* ==========================================================================
 * 0x1001B261..0x1001B364 -- the first block out of BR_RS_HOLE_HUD
 *
 * The hole this module recorded ran 0x1001B261..0x1001C646 (5,094 B).  This
 * is its opening 260 bytes, transcribed 2026-09-10.  Addresses are on every
 * branch, as everywhere else in this file.
 *
 * READ THIS BEFORE TRANSCRIBING MORE OF THE HOLE: ebp is the function's
 * PINNED ZERO for the whole of 0x10019A70 (`xor ebp,ebp` at 0x10019A8E, and
 * again at 0x1001B30C / 0x1001B363 after the scan below borrows it).  So in
 * the listing `cmp X, ebp` reads `X == 0` and `push ebp` reads `push 0` --
 * neither is a variable.  Mis-reading those as a live value is the easiest
 * way to invent a defect here.
 *
 * The 64 distinct callees of the hole were resolved on 2026-09-10 and ALL of
 * them already have symbols in this tree (115 of the 116 call sites land on a
 * report.csv row; the last, 0x100325B0, is the C++ lane's WM_DESTROY
 * teardown).  The old "gated on 131 callee signatures" note is therefore
 * spent -- what blocks the address now is transcription volume, not unknown
 * callees.
 * ========================================================================== */

/* br_racebegin.h owns these four; declared here rather than pulling the whole
 * header in, as this file already does for its other cross-module globals. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105BC7C0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x105BC7C4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x105BC7C8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105BC7CC */

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x100B3014, the track/mode selector */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x106ED6AC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x106ED6B0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x106ED6B4 */

/* 0x10008D60 and 0x10033BB0 take their state in globals in the original and
 * no stack argument; br_racebegin.c already spells the first one this way. */
/* BrExt_10008D60: prototype in br_funcs.h */
void BrExt_10033BB0(void);           /* 0x10033BB0, the particle tick */
/* BrWeatherStepParticles: prototype in br_funcs.h */

/* The entrant records: 0x10AF3B54 walks by 0x2B68, its collision-object id
 * sits 0x19A4 below the cursor and its id list is the uint16 array 0x40
 * below it (0x10AF3B14 indexed in ELEMENTS, stride 0x15B4 == 0x2B68/2 --
 * the same array seen as words, which is why the listing's index arithmetic
 * looks like a second table and is not). */

/* WHAT IT DOES: the screen-wide effects for one frame of an actually-running
 * race, and the test that arms the fly-past.  Two colour submissions with the
 * particle tick between them and the weather step after; then, when the
 * fly-past is enabled and not already armed, a scan of every entrant's
 * collision-id list for the one id that triggers it -- finding it arms the
 * fly-past for the block that follows.  A paused race and a replay both skip
 * the whole thing.  Returns non-zero when the caller should go on to the
 * special-object arm, which is what the original's fall-through does. */
/* (port-only BrRaceStepEffects removed) */


/* ==========================================================================
 * 0x1001B365..0x1001B402 and 0x1001B870..0x1001B886 -- the specials loop
 * ========================================================================== */

/* One entry of the special-object table at 0x106EEE3C, stride 0xC.  The axis
 * byte at +8 is the switch selector; 0..2 are the three axis rotations and 3
 * is the fly-past camera (0x1001B403, still a hole). */
/* BrRaceSpecial: br_racebegin.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x106EEE3C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x106EEEFC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x106EED38, 0x54-byte records     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x106E7970, the scratch matrix    */

/* slice2_17.h and slice1_05.h own these; declared here rather than pulling
 * both headers in, as this file already does for its other externs. */
/* BrMat4RotateAxis: prototype in br_funcs.h */
/* BrMat4Mul: prototype in br_funcs.h */

/* The fly-past spline.  0x105BC7DC and 0x105BC7E0 are two parallel arrays of
 * BrVec3 -- the rail the camera flies and the rail it looks at -- walked by
 * one cursor.  0x105BC7C0 is BOTH the enable flag and the camera object's
 * index into the 0x54-byte records, which is why zero means "off". */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BC7DC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BC7E0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105BC7D0, the cursor  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105BC7D4, the count   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105BC7D8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x105BC7E4, distance left in this leg */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x105BC7E8, this leg's length         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x105BC7EC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x105BC7F8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x105BC804 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100773C4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100773C8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100773CC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100773D0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x106E9D8C, this frame's distance     */

/* BrVec3Midpoint: prototype in br_funcs.h */
/* BrVec3Dist: prototype in br_funcs.h */
/* BrVec3Sub: prototype in br_funcs.h */
/* BrVec3Cross: prototype in br_funcs.h */
/* BrVec3Lerp: prototype in br_funcs.h */
/* BrVec3ScaleBy: prototype in br_funcs.h */
/* br_dl_normalise: prototype in br_funcs.h */

/* The direction across a node: the midpoint of the leg after it minus the
 * midpoint of the leg before it, normalised.  0x1001B48B..0x1001B512 for the
 * current node, and the same shape again at 0x1001B549 and 0x1001B5CA for the
 * one before and the one after.  T1 is the frame's [esp+0x2C] BrVec3 and T2
 * its [esp+0x20]; the first call also leaves the leg's LENGTH behind in
 * 0x105BC7E8, which is the only reason it is not a plain helper. */
/* (port-only BrRaceFlyDirAt removed) */


/* WHAT IT DOES: flies the fly-past camera one frame further along its rail.
 * It spends this frame's distance out of the leg it is on; each time a leg
 * runs out it steps to the next node -- rebuilding the direction across the
 * node before, at and after the cursor -- and when the rail runs out the
 * fly-past switches itself off.  Then it places the camera: position lerped
 * along the leg, look-at lerped the same way, and an orientation built by
 * blending the two node directions, crossing it twice into an orthonormal
 * basis and scaling all three rows.  Distances, not times, so the camera
 * moves at a constant speed whatever the frame rate. */
/* (port-only BrRaceStepFlyPast removed) */


/* WHAT IT DOES: animates the track's special objects for one frame.  Each
 * entry names an object, an angle and one of four behaviours: spin it about
 * z, about x, or about y -- building the rotation in a scratch matrix,
 * multiplying it into the object's own and clearing the object's "hidden"
 * bit -- or, for the fourth, drive the fly-past camera along its spline.
 * An entry whose behaviour byte is out of range is skipped. */
/* (port-only BrRaceStepSpecials removed) */


/* -- Ghidra-matched functions --------------------------- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: zero every row in the sound-command table. */
/* @implements 0x10061440 glide BrSndTableClear */

int BrSndTableClear(void)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < (*(int *)&g_BrCarCount)) {
    do {
      BrWrap_10072B80(0x18,iVar1,0);
      iVar1 = iVar1 + 1;
    } while (iVar1 < (*(int *)&g_BrCarCount));
  }
  return;
}

