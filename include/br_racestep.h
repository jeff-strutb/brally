/* br_racestep.h -- 0x10019A70, THE RACE STEP, and the two per-driver passes
 * it drives.
 *
 * REFERENCE IS orig/BRGlide.dll.  Every address below was checked with
 * tools/whereis.py before a line was written.
 *
 * ======================================================================
 * WHAT 0x10019A70 IS
 * ======================================================================
 * 11223 bytes (11,223 / 480,853 of BRGlide.dll `.text`).  It is the body
 * installed in the game-step slot at 0x106E79F4 (br_gamestep.h), so the
 * Win32 pump's state-2 arm calls it once per frame for the whole of a
 * race.  It is NOT a phase: the menu frame runs INSIDE it.
 *
 * ======================================================================
 * MATCHING PROTOCOL (byte-exact, not the port)
 * ======================================================================
 * One address, one C function.  The Clock / Begin / Frame / Lights split
 * below is the PORT.  A matching twin cannot be that split.
 *
 * Do not transcribe all 11 KB in one pass.  Win the prologue first:
 *
 *     10019A70  sub  esp, 0x34          ; 52 bytes of locals, 4-byte aligned
 *     10019A73  push ebx / ebp / esi / edi
 *     …         xor  ebp, ebp           ; ebp is a GENERAL register
 *
 * `and esp,-8` means some local wants 8-byte alignment (a `double` or
 * `__int64`; Ghidra's `float10` is an x87 return, not a slot).  Retype or
 * delete it and the compiler returns to `sub esp, 0x34` and frees ebp.
 * Until instruction one matches, nothing downstream can.
 *
 * Then grow section by section and recompile; the first divergence is the
 * progress bar.  131 distinct callees — wrong signatures corrupt call
 * sites — so this is last among the big targets, not first.  No
 * `@implements` until the whole 11,223 bytes diff clean.
 *
 * Its very first branch splits it in two, and the split is the shape of this
 * module:
 *
 *     0x10019AE4   eax = g_106... 0x105CCB94        the race SUBSTATE
 *     0x10019AF8   jne 0x1001AB71                   -> the PER-FRAME arm
 *                  (fall through)                   -> the ONE-TIME arm
 *
 * The one-time arm (0x10019AFE..0x1001AB70, 4,209 B) loads the track, builds
 * the field, seeds the start-light script and ends at 0x1001AA5E with
 * `0x105CCB94 = 1`, so it never runs again.  The part that is the RACE's own
 * state is ported here as BrRaceStepInit; ALL OF THE REST IS NOW PORTED TOO,
 * in port/src/racing/br_racebegin.c, which calls BrRaceStepInit for that one
 * block rather than repeating it.  This paragraph used to end "the rest is
 * named, counted and left out" -- that is no longer where the gap is.
 *
 * The per-frame arm (0x1001AB71..the end) is what this module is for.
 *
 * ======================================================================
 * THE PER-FRAME ARM, IN THE ORIGINAL'S ORDER
 * ======================================================================
 *   0x1001AB93  if (lights == 4) 0x10060A30 over every car     -- HOLE
 *   0x1001ABFB  THE START-LIGHT STATE MACHINE, below
 *   0x1001B186  0x1005C450, the per-frame scratch clear        -- ported
 *   0x1001B18B  0x10061430 over g_100B2F04 cars                -- ported
 *   0x1001B1B2  0x10061F60 over g_100B2F00 drivers             -- ported
 *   0x1001B1E9  0x100623A0 over the same                       -- HOLE
 *   0x1001B20B  0x100623E0 over the same                       -- ported
 *   0x1001B22D  if (mode == 0) 0x1005F6C0 over every car       -- HOLE
 *   0x1001B25C  0x1005F580, THE STANDINGS                      -- ported
 *   0x1001B261  the replay camera, the HUD, the mirror, the per-car render
 *               marshalling, the pause/camera input, the race exit and the
 *               frame limiter -- 5,094 B, THE ONLY PART STILL MISSING -- HOLE
 *
 * 0x1005F580 is worth its own line: it is the GLIDE TWIN of D3D 0x10066510,
 * which slice3_41.c already ports as BrRankAssign.  Read it side by side --
 * the +0x68 bit-1 skip, the key taken from pCar->fFF4 or from the slot's own
 * +0x50, the qsort on 8-byte {key, index} pairs, and the `n - j - 1` rank
 * written back to pCar->fFF8 or the slot's +0x54 -- it is the same function.
 * So the standings recompute this module needs was already in the tree under
 * the other build's address, and is called rather than re-transcribed.
 *
 * ======================================================================
 * THE START-LIGHT STATE MACHINE, AND WHY IT IS THE INTERESTING PART
 * ======================================================================
 * Three globals and one table:
 *
 *   0x105BC8F8  the light state, 0..7
 *   0x105BC880  seconds left in it
 *   0x105BC750  the index into the script
 *   0x100A9578  the SCRIPT: eight {int32 state; float seconds} pairs, read
 *               out of BRGlide.dll and reproduced verbatim below.
 *
 * The state selects one of five arms of a `cmp/jge` ladder at 0x1001ABFB:
 *
 *   state < 3   0x1001AC0F  EVERY driver gets  +0x68 |= 1  (0x1001ACEC).
 *               That bit is what 0x10061F60 tests first, and its arm sets
 *               the control word's BRAKE bit and calls the controller.  Be
 *               precise about what it does and does not stop: a slot with a
 *               CAR still gets its gate step, because 0x100623E0 does not
 *               look at the bit -- what holds the car on the grid is the
 *               brake, not the bookkeeping.  A slot with NO car is stopped
 *               dead, because the phantom arm is inside 0x10061F60 and the
 *               frozen arm returns before it.  State 2 also runs the 3-2-1
 *               countdown against the thresholds at 0x100A9548.
 *   state == 3  0x1001ADDD  EVERY driver gets  +0x68 &= ~1  (0x1001ADFF).
 *               GREEN.  The bit comes off at the END of the frame that
 *               expires the timer, so the first unfrozen frame is the one
 *               after -- that ordering is the original's.
 *   state == 4  0x1001AE41  RACING.  Walks the field and clears a local flag
 *               for every driver whose +0x68 bit 1 (BR_DRIVER_SKIP, the
 *               FINISHED bit 0x1005FF00 sets) is clear; if the flag survives
 *               -- i.e. everyone has finished -- it falls into the script
 *               advance.  That is the whole race-over condition.
 *   state == 5  0x1001B09D  and  state == 6  0x1001B0CD, the results.
 *   state == 7  0x1001B127  the fade out.
 *
 * The timer arm at 0x1001B0CD is shared by fallthrough (state 6) and by
 * explicit jumps from the two arms above (0x1001ADD6, 0x1001AE33):
 *
 *      if (paused) return;
 *      t -= dt;                       0x1001B0DF
 *      if (!(t < 0.0f)) return;       0x1001B0EB, 0x100773A4 == 0.0f
 *      ++i; t = script[i].dur; state = script[i].state;
 *
 * STATES 4, 5 AND 7 DO NOT DECREMENT THE TIMER.  State 4's `dur` is 0.0 and
 * it advances only on the all-finished flag; that is why a race does not time
 * out.  Reproducing that is the difference between a race and a cutscene.
 *
 * ======================================================================
 * 0x10061F60, 1073 B -- ONE DRIVER, PART ONE
 * ======================================================================
 * D3D 0x10068EF0, which slice3_41.h already cites for the +0x50 progress key
 * and for `car->f1034 += car->f1030 * dt`.  __thiscall on the 0x80-byte
 * driver record.  FOUR arms, chosen by +0x68 and by whether the slot has a
 * car:
 *
 *   +0x68 bit 0 set     0x1006201B  FROZEN.  Sets the control word's 0x40000
 *                       (brake) bit, mirrors pos -> posPrev, calls the
 *                       controller, and returns.  No gate step.
 *   +0x68 bit 1 set     0x10062086  FINISHED.  Pins the control word to
 *                       0xC0000 and the steering to -1.0f, bleeds +0x29B0
 *                       down by dt, mirrors, calls the controller.  No gate
 *                       step.
 *   pCar != NULL        0x1006216E  RACING.  The +0x29AF == 2 recovery bleed,
 *                       mirror, controller, then the odometer.  Still no gate
 *                       step -- that is 0x100623E0's job.
 *   pCar == NULL        0x10062238  THE PHANTOM ENTRANT, and this is the arm
 *                       that makes a headless race observable at all.
 *
 * THE PHANTOM ARM.  br_race.h already records that a driver with no car
 * "keeps its state only in the driver record"; this is where that state comes
 * from.  The slot is walked along the AI PATH RING analytically:
 *
 *      lapLen = root pts[0].arc                      (br_ai.h's BrAiLapLength)
 *      A      = (f44 + 1) * lapLen - f50 - pts[i+1].arc
 *      ratio  = A / (pts[i].arc - pts[i+1].arc)      how much of the segment
 *                                                    is still ahead
 *      0x1005ECF0(node, i, ratio, dist)              walk `dist` forward
 *      f50   += dist
 *      f00    = where it landed;  f18 = (f00 - f0C) / dt   -- its velocity
 *      0x1005FF00(this)                              the gate machine
 *
 * `dist` is |car[nEntrant + f74]->f1024| * dt when that borrowed car is live
 * (+0xF00 non-zero), and otherwise the FLAT 2.22 units per frame at
 * 0x10077A14.  Both arms are ported; the port takes the second one because
 * nothing here fills +0xF00.
 *
 * ======================================================================
 * 0x1005ECF0, 204 B -- THE PATH WALK
 * ======================================================================
 * The whole of the phantom's motion.  It consumes `dist` segment by segment
 * from (node, index), hopping SKIP nodes through +0x04 and whole nodes
 * through +0x00 exactly as BrAiAdvanceTarget does, and lands with two lerps:
 *
 *      P   = lerp(pts[i].centre, pts[i+1].centre, ratio)      where it is now
 *      out = lerp(pts[i+1].centre, P, dist / (segLen * ratio))  where it ends
 *
 * -- BrVec3Lerp being (a - b) * t + b, so the operand order above is the
 * original's and is load-bearing.
 *
 * THE FIRST SEGMENT IS SHORT AND THE REST ARE WHOLE: `ratio` is reset to
 * 1.0f at 0x1005ED59 the moment the walk leaves the segment it started in.
 *
 * The comparison that ends the walk is `fcomp` + `test ah,0x41` + `jne`, i.e.
 * it stops on LESS *OR EQUAL OR UNORDERED*, which `!(dist > avail)` is.  A
 * NaN distance therefore stops the walk in the first segment rather than
 * running the ring forever.
 *
 * DEVIATION, and it is representation-only.  The original's node is an N64
 * pointer relocated into the track image and its outputs are three globals
 * (0x10B1CBEC the node, 0x10AF07F0 the index, 0x10B1CE98 the position) which
 * 0x10061F60 reads back afterwards.  br_ai.h decodes a node as a VIEW over
 * big-endian data rather than overlaying a struct, so this takes and returns
 * the node's FILE OFFSET -- which is what the relocated pointer is -- and
 * keeps the same three globals under the same three names.  The driver
 * record's +0x28 holds the offset and +0x2C the index, exactly as the
 * original uses them.
 *
 * ======================================================================
 * 0x100623E0, 286 B -- ONE DRIVER, PART TWO
 * ======================================================================
 * Runs only for a slot that HAS a car, and it is where a car entrant's gate
 * step comes from:
 *
 *      if (paused || !pCar) return;
 *      if (pCar->b360) { ...the skid trail, 0x10034E30/0x10074560/0x10072100
 *                           /0x1000C4E0... }                        -- HOLE
 *      0x1006E9E0, 0x1006EA70, 0x1006EB00                           -- HOLE
 *      0x1005FF00(this)                       THE GATE MACHINE      -- ported
 *      0x1006EBC0                                                   -- HOLE
 *      pCar->f1024 = (pCar->pos - pCar->posPrev) / dt               -- ported
 *      ... 0x10034E30 into +0x2718 ...                              -- HOLE
 *      if (pCar->fF04) --pCar->fF04;                                -- ported
 *
 * The velocity line is the one that matters to anything else: +0x1024 is what
 * br_ai.h's lookahead measures speed from, and it is derived here from the
 * two positions the physics wrote, not from the rigid body.
 *
 * ======================================================================
 * THE HOLES, COUNTED
 * ======================================================================
 * Every one is entered through g_brRaceStepHooks and bumps
 * g_aBrRaceStepHole first, whether or not a hook is installed -- the same
 * contract br_carphys.h states, for the same reason: a silent no-op makes
 * "the race ran" unfalsifiable.
 */
#ifndef BR_RACESTEP_H
#define BR_RACESTEP_H

#include <stdint.h>

#include "br_ai.h"        /* BrAiNode, BrAiPoint, BrAiLapLength           */
#include "br_race.h"      /* BrRaceRules, BrRaceGateStep                  */
#include "br_track.h"     /* BrTrack                                      */
#include "br_vec.h"       /* BrVec3                                       */
#include "slice3_41.h"    /* BrDriver, BrDriverCar, BrRankAssign          */
#include "br_match.h"     /* BR_THISCALL1 -- thiscall via __fastcall      */

/* ======================================================================
 * The start-light script, 0x100A9578
 * ====================================================================== */

typedef struct BrRaceLightStep {
    int32_t state;      /* -> 0x105BC8F8 */
    float   dur;        /* -> 0x105BC880, seconds */
} BrRaceLightStep;

#define BR_RS_SCRIPT_LEN  8
extern const BrRaceLightStep g_aBrRaceLightScript[BR_RS_SCRIPT_LEN];

/* The two light states with behaviour, named.  The rest are positional. */
#define BR_RS_LIGHTS_GO      3   /* the frame the freeze bit comes off */
#define BR_RS_LIGHTS_RACE    4   /* the arm that ends on all-finished  */
#define BR_RS_LIGHTS_COUNT   2   /* the arm that runs the 3-2-1        */

/* +0x68 bit 0.  slice3_41.h already names bit 1 (BR_DRIVER_SKIP); this is
 * its neighbour, set by 0x1001ACEC and cleared by 0x1001ADFF. */
#define BR_RS_DRIVER_FROZEN  1u

/* 0x100A9548, compared against the state-2 timer.
 *
 * FIVE entries, not four, and the fifth is the point: the original indexes
 * this with 0x105CCB74 and does NOT bound it, so the frame on which the
 * counter reaches 4 reads the dword at 0x100A9558.  That dword is 0.0f, and
 * `0.0f > t` is false for every timer value state 2 can still be holding --
 * which is what stops the countdown after four sounds.  Dropping the fifth
 * entry and clamping to the fourth would make the last threshold (0.20f)
 * sticky and fire a sound per driver per frame for the rest of the state.
 * The array carries it so the port stops where the original stops. */
#define BR_RS_BEEP_COUNT  5
#define BR_RS_BEEP_REAL   4
extern const float g_aBrRaceBeepT[BR_RS_BEEP_COUNT];

/* 0x100773A4 -- the timer's expiry threshold, and it really is 0.0f.
 * 0x100773B4 -- the state-3 fade curve's scale.
 * 0x10077A14 -- the phantom entrant's flat step, as the NEGATIVE the original
 *               subtracts.  0x400E147B pushed at 0x10062317 is the same
 *               number as a positive immediate; both spellings are the
 *               original's and both are kept.
 * 0x10077A0C / 0x10077A10 / 0x100778F8 -- the +0x29B0 recovery bleed. */
#define BR_RS_TIMER_END     0.0f
#define BR_RS_FADE_SCALE   15.0f
#define BR_RS_PHANTOM_STEP  2.22f
#define BR_RS_BLEED_K     (-1.6f)
#define BR_RS_BLEED_LO     0.375f
#define BR_RS_BLEED_HI     1.0f

/* ======================================================================
 * The holes
 * ====================================================================== */

enum {
    /* 0x1005D050 -> 0x1005C8B0 (human) and 0x1005E690 -> 0x1005D770 (AI),
     * both of which end in 0x1006F170 -> 0x1005A7A0.  Of those five only the
     * last is ported, and it takes the 0x2B68 record's PHYSICS part, which is
     * br_carphys.h's object and not this module's -- so the whole chain is
     * one hook and the host supplies a body for it. */
    BR_RS_HOLE_CONTROL,
    /* 0x10060A30, 286 B, over every car on every frame of state 4. */
    BR_RS_HOLE_LAPINFO,
    /* 0x1006E9E0 + 0x1006EA70 + 0x1006EB00 + 0x1006EBC0 and the skid-trail
     * block above them, inside 0x100623E0. */
    BR_RS_HOLE_SKID,
    /* 0x100623A0's two callees, 0x1005ACE0 and 0x10001CF0. */
    BR_RS_HOLE_ANIM,
    /* 0x10060E00 and 0x10060DF0, the countdown's two sounds.  PORTED as of
     * port/src/br_sfxsrc.c; the counter remains because it is what pins the
     * four-fires-per-race invariant.  Install with
     *     g_brRaceStepHooks.pfnSound = BrSfxSrcRaceCountdown;
     * or let port/src/br_wireaudio.c's BrHostWireAudio do it. */
    BR_RS_HOLE_SOUND,
    /* 0x1001B27A..0x1001C0xx: the HUD, the mirror view and the renderer,
     * about 5.5 KB, entered once per frame. */
    BR_RS_HOLE_HUD,
    /* 0x1005F310, 538 B, the driver-record constructor and grid placement,
     * once per slot in the one-time arm. */
    BR_RS_HOLE_GRID,
    /* 0x1005F6C0, 2104 B, "saving/restoring lap (%d/%d) and gate (%d/%d)",
     * over every car when mode == 0. */
    BR_RS_HOLE_SAVELAP,
    /* 0x1001AD08..0x1001AD56, the per-car difficulty lookup into
     * 0x100BCAB0 that writes car+0xFF0 and car+0x1000. */
    BR_RS_HOLE_DIFFICULTY,
    /* 0x1005C450, 62 B: it walks a NULL-terminated table of
     * {pointer, byte count} at 0x100B2F08 and zeroes each block.  The table
     * is built by start-up code this port has not reached, so there is
     * nothing to zero and the loop would not execute -- but saying that with
     * a counter is not the same as saying it with silence. */
    BR_RS_HOLE_SCRATCH,
    /* The path walk was asked to leave the track image.  The original does
     * not check and would fault; this counts and stops. */
    BR_RS_HOLE_PATHEDGE,
    BR_RS_HOLE_COUNT
};

extern uint32_t g_aBrRaceStepHole[BR_RS_HOLE_COUNT];
void        BrRaceStepHoleReset(void);
const char *BrRaceStepHoleName(int i);

typedef struct BrRaceStepHooks {
    void (*pfnControl)(BrDriverCar *pCar);   /* car+0xF08's whole chain */
    void (*pfnSkid)(BrDriverCar *pCar);      /* inside 0x100623E0       */
    void (*pfnAnim)(BrDriverCar *pCar);      /* 0x100623A0              */
    void (*pfnLapInfo)(BrDriverCar *pCar);   /* 0x10060A30              */
    /* 0x10060DF0 / 0x10060E00.  iStep is the POST-increment beep counter,
     * so 4 is the GO horn and 1..3 are the countdown -- which is the branch
     * br_sfxsrc.c's BrSfxSrcRaceCountdown reproduces. */
    void (*pfnSound)(int iStep);
} BrRaceStepHooks;
extern BrRaceStepHooks g_brRaceStepHooks;

/* ======================================================================
 * The globals 0x10019A70 reads and writes, each under its own address
 * ====================================================================== */

extern BrDriver    *g_pBrRaceDriver;    /* 0x10AF07F8, stride 0x80         */
extern BrDriverCar *g_pBrRaceCar;       /* 0x10AF1208, stride 0x2B68       */
extern int32_t      g_brRaceNDriver;    /* 0x100B2F00 -- the driver loops  */
extern int32_t      g_brRaceNCar;       /* 0x100B2F04 -- the car loops     */
extern int32_t      g_brRaceNEntrant;   /* 0x100B3858                      */

extern int32_t      g_brRaceLights;     /* 0x105BC8F8                      */
extern float        g_brRaceLightT;     /* 0x105BC880                      */
extern int32_t      g_brRaceScript;     /* 0x105BC750                      */
extern int32_t      g_brRaceSubstate;   /* 0x105CCB94                      */
extern int32_t      g_brRaceBeep;       /* 0x105CCB74                      */
extern float        g_brRaceFade;       /* 0x105BC764                      */

/* 0x106E9D8C, the frame delta every one of these functions multiplies by.
 *
 * DEVIATION: the original's is wall clock, written outside this function.
 * The harness runs a FIXED 1/30 s, which is also what br_phys.h's BR_PHYS_DT
 * is and what every integrator call site inside 0x1005A7A0 pushes, so the two
 * clocks agree by construction rather than by luck. */
extern float        g_brRaceStepDt;

extern int32_t      g_brRacePaused;     /* 0x105CCB5C                      */
extern int32_t      g_brRaceReplay;     /* 0x105CCB88                      */

/* 0x10226A44 -- THE FIXED-TIMESTEP TICK, and identifying it correctly is
 * what makes the light script advance at all.  It gates the script advance
 * (0x1001B0FE), one arm of the pre-race block (0x1001AC15) and two tests in
 * 0x1006F170, and reading it as a "demo mode" flag would freeze the race on
 * the grid for ever.  Two writers settle it:
 *
 *   0x100064EB  inside a WaitForSingleObject / ReleaseMutex pair on the
 *               mutex at 0x1021C81C: `if (0x100037D0() >= 0x10226A30)
 *               0x10226A44 = 1`.  A worker raising "a step's worth of time
 *               has passed".
 *   0x100628DC  the race-entry setup seeds it with `mode == 4`.
 *
 * So it is the simulation clock's own edge.  A fixed-timestep host raises it
 * on every step, which is what BrRaceStepFrame's caller does. */
extern int32_t      g_brRaceTick;       /* 0x10226A44                      */

/* 0x10226A48 -- written only by 0x10007F40, and every consumer treats it as
 * "somebody else owns these entrants": 0x1005F580 replaces the standings
 * sort with a per-car remote query, 0x10061F60 drops carless slots, and
 * 0x100623A0 gates on 0x10059D30.  Modelled as 0, which is the single-player
 * value and the .bss one. */
extern int32_t      g_brRaceNet;        /* 0x10226A48                      */

extern int32_t      g_brRaceHudA;       /* 0x105CCB58                      */
extern int32_t      g_brRaceHudB;       /* 0x105CCB78                      */

/* car+0xF08 == 0x1005E690 is the AI controller, and 0x10061F60 opens by
 * testing exactly that address (0x10061F82) before clearing the control
 * word.  A host pointer cannot be compared against 0x1005E690, so the body
 * that stands for it is registered here and the test is against this. */
extern void       (*g_pfnBrRaceAiControl)(BrDriverCar *);

/* The six globals br_race.h gathers into one object.  Kept as one here too,
 * because BrRaceGateStep takes it and a second copy would be a second model
 * of one original state. */
extern BrRaceRules  g_brRaceRules;

/* The track the AI path ring lives in -- 0x106EED48 is track header +0x70,
 * which br_ai.h pins.  NULL disables the phantom arm. */
extern const BrTrack *g_pBrRaceTrack;

/* 0x1005ECF0's three outputs. */
extern uint32_t     g_brRacePathNode;   /* 0x10B1CBEC, as a file offset    */
extern uint32_t     g_brRacePathIndex;  /* 0x10AF07F0                      */
extern BrVec3       g_brRacePathPos;    /* 0x10B1CE98                      */

/* ======================================================================
 * The pieces
 * ====================================================================== */

/* 0x1005ECF0.  Writes the three globals above; leaves them untouched when
 * the ring runs out, which is the original's behaviour. */
void BrRacePathAdvance(uint32_t offNode, uint32_t index,
                       float ratio, float dist);

/* NOT A PORT: the seed 0x1005F310 -> 0x1005EB90 would establish for a
 * carless slot.  It puts the slot `dist` metres along the path from the root
 * and sets the progress key, the lap counters and the gate counter to the
 * consistent triple the phantom arm requires.  Counted as the grid hole.
 * `dist` must place the slot PAST gate 0 -- see the banner in br_racestep.c
 * for what happens when it does not.  Returns 0 on success. */
int  BrRaceSeedPhantom(BrDriver *pDrv, float dist);

/* 0x10061430, 11 bytes: `car->fF78 = 0`.  Its whole body. */
/* thiscall in the original: `this` in ecx, nothing pushed.  BR_THISCALL1 is
 * empty off-MSVC, so the port signature is unchanged. */
void BR_THISCALL1 BrRaceCarPre(BrDriverCar *pCar);

/* 0x10061F60 and 0x100623E0 -- one driver, before and after. */
void BrRaceDriverStep(BrDriver *pDrv);
void BrRaceDriverAnim(BrDriver *pDrv);   /* 0x100623A0, a pure hole */
void BrRaceDriverPost(BrDriver *pDrv);

/* 0x1001AEE2's walk, as a predicate: non-zero when EVERY driver carries
 * BR_DRIVER_SKIP.  Exposed because it is the race-over condition and a test
 * should be able to ask it directly. */
int  BrRaceStepAllFinished(void);

/* 0x1001ABFB..0x1001B171, the light machine on its own.  Exposed for the
 * same reason. */
void BrRaceStepLights(void);

/* 0x10019A70 CARRIES NO @implements LINE.
 *
 * The port transcribes pieces (Clock / Begin / Init / Frame / Lights).  A
 * matching twin must be one C function; see MATCHING PROTOCOL above.
 * Do not tag this address on any of the split helpers. */

/* The one-time arm's race state: the script seed at 0x1001A97C..0x1001A9EF
 * and 0x105CCB94 = 1.  Everything else in that arm is asset loading.
 * Returns the light state it seeded. */
int  BrRaceStepInit(void);

/* 0x1001AB93..0x1001B25C -- ONE FRAME.  The body the pump calls. */
void BrRaceStepFrame(void);

/* BrGameStepSet(BrRaceStepFrame) plus the registration that makes
 * BrGameStepId answer BR_GAMESTEP_RACE.  0x10019A70 is what the original
 * installs in that slot, so registering BrRaceStepFrame there is the right
 * wiring -- which is a statement about the SLOT, not an @implements claim on
 * the address.  See the note above BrRaceStepInit. */
void BrRaceStepInstall(void);

#endif /* BR_RACESTEP_H */
