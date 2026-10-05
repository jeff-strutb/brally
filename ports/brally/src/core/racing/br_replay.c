/* br_replay.c -- racing: the replay ring, the parts that are byte-exact.
 *
 * The ring and its session globals, the recorder (BrReplayRecord, @t3),
 * the frame advance, playback apply and the transport (BrReplaySeek), plus
 * clearing, rewinding and the size in bytes. The recorder block was filed
 * out of slice3_42.c on 2026-09-13; br_replayon.h owns the on/off half.
 *
 * See slice3_42.h for the recovered layouts and the GOTCHAs.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_42.h"
#include "slice3_41.h"

/* .rdata constant carried from slice3_42.c, read out of orig/BRD3D.dll. */
#define BR_K_0008FAA8  30.0f    /* 0x1008FAA8 -- the simulation rate */

/* =====================================================================
 * 3. Replay recorder
 * ===================================================================== */

/* Explicitly initialised (rather than left tentative) so it lands in .bss
 * with natural alignment; a 12 MiB common symbol makes some linkers
 * over-align the whole section. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x10B50308 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* 0x10B502E8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                                        /* 0x11750308 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* 0x11750310 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* Byte access into an untyped car record, the slice2_17.h convention. */

/* `mov ecx,8; cmp eax,2; je L; cmp eax,4; jne M; L: mov ecx,1` */
/* (port-only BrReplayActiveCount removed) */


/* 0x1006AAB0 */
/* WHAT IT DOES: writes down where one car is and which way it is facing, into
 * that car's slot for the current replay frame. It does nothing if recording
 * is off, if playback is running, or if this car has already filled its
 * allowance of frames -- the recording simply stops rather than wrapping. */
/* @t4-pass 0x10063A60 1 2026-09-10 probes 48 bytes 105 insns 32 regions 2 rows 4 census yes  (tools/crank.py) */
/* @t4-pass 0x10063A60 2 2026-09-10 probes 48 bytes 105 insns 32 regions 2 rows 4 census yes  (tools/crank.py) */
/* @t4-pass 0x10063A60 3 2026-09-10 probes 48 bytes 105 insns 32 regions 2 rows 2 census yes  (tools/crank.py) */
/* @t4-pass 0x10063A60 4 2026-09-10 probes 48 bytes 105 insns 32 regions 2 rows 2 census yes  (tools/crank.py) */
/* @t3 0x10063A60 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 105/103 insns 32/30 rows 0+2 regions 2 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * residue after tools/crank.py: 48 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind. */
/* @implements 0x1006AAB0 d3d BrReplayRecord */
void BrReplayRecord(BrDriverCar *pCar)
{
    int32_t iPlayer;
    int32_t frame;

    iPlayer = ((*(int32_t *)&pCar->f140));

    if (g_BrReplayOn == 0)
        return;
    if (g_BrX06909B4 != 0)
        return;

    frame = g_BrReplayCount[iPlayer];
    if (frame >= (int32_t)BR_REPLAY_FRAMES)
        return;

    /* orig pushes esi only on this path (`lea esi,[eax*8+g_BrReplayBuf]`
     * must survive RecordToState). Nested so the 0xa0 state is the slow
     * path's frame, not a prologue that also saves esi. */
    {
        BrCarState state;
        BrCarRecordToState(&state, pCar);
        BrCarStatePack(
            &g_BrReplayBuf[((uint32_t)iPlayer << 16) + (uint32_t)frame].rec,
            &state);
    }
}

/* 0x1006AB20 */
/* WHAT IT DOES: moves the recording on by one frame once every car has been
 * written down, stopping each car's count at the end of its allowance. In the
 * two single-car modes it also nudges the playback position along, so the
 * recording and the thing watching it stay together. */
/* @implements 0x1006AB20 d3d BrReplayAdvance */
void BrReplayAdvance(void)
{
    int n;
    int32_t *p;
    int32_t left;

    if (g_BrReplayOn == 0)
        return;
    if (g_BrX06909B4 != 0)
        return;

    /* Same open-coded 8-or-1 as Reset: orig `mov ecx,8; cmp esi,2; je;
     * cmp esi,4; jne; mov ecx,1` then `test ecx,ecx / jle`. A helper call
     * is the extra `call` in the bag. */
    n = 8;
    if ((*(int32_t *)&g_brRaceRules.mode) == 2 || (*(int32_t *)&g_brRaceRules.mode) == 4)
        n = 1;

    /* orig `test ecx,ecx; jle` THEN `mov eax,&count; mov edx,ecx`. Setup
     * must sit inside the taken arm so it does not hoist above the jle. */
    if (n > 0) {
        p = g_BrReplayCount;
        left = n;
        do {
            if (*p < (int32_t)BR_REPLAY_FRAMES)
                ++*p;
            ++p;
        } while (--left);
    }

    if ((*(int32_t *)&g_brRaceRules.mode) == 2 || (*(int32_t *)&g_brRaceRules.mode) == 4) {
        /* orig: eax=count[1], ecx=cursor[1], `dec eax; cmp ecx,eax; jge;
         * mov eax,ecx; inc eax; store`.  The `mov eax,ecx` copy comes from
         * RE-READING the cursor global in the store: VC5 CSEs the reload
         * back into ecx and increments a fresh eax (2026-09-09, same lever
         * as BrOptCycleAA2A00's caption index).  ++b or b+1 on the local
         * increments in place, 2 B short. */
        int32_t a = g_BrReplayCount[1];
        int32_t b = g_BrReplayCursor[1];
        if (b < --a)
            g_BrReplayCursor[1] = g_BrReplayCursor[1] + 1;
    }
}

/* 0x1006ABD0 */
/* WHAT IT DOES: puts a car where the recording says it was, for the frame the
 * replay is currently showing. It also fakes the car's speed by looking ahead
 * to the next recorded frame and measuring how far the car is about to move,
 * so anything driven by speed -- engine note, wheel spin -- still behaves;
 * near the very end of the recording, with no next frame to look at, the
 * speed is left as it was. In one playback mode a handful of the car's
 * damage or effect flags are cleared as well. */
/* @implements 0x1006ABD0 d3d BrReplayApply */
void BrReplayApply(BrDriverCar *pCar, int32_t iPlayer)
{
    BrCarState    state;
    BrReplaySlot *pSlot;
    unsigned char z;

    pSlot = &g_BrReplayBuf[((uint32_t)iPlayer << 16)
                           + (uint32_t)g_BrReplayCursor[iPlayer]];

    /* The decoder leaves the velocity and spin words (7..12) alone and
     * BrCarGhostApply copies them onto the car, so the original applied
     * whatever its stack held there.  The core starts the record at 0. */
    memset(&state, 0, sizeof state);
    BrFixDecodeRecord_10007AA0(&state, &pSlot->rec);

    /* orig `mov edx,[car+0xFF4]; mov [state.f78],edx` -- dword copy, not
     * fld/fstp. Unpack leaves f78 alone. */
    *(int32_t *)&state.f78 = ((*(int32_t *)&pCar->fFF4));
    BrCarGhostApply_10059A80(pCar, &state);

    if ((DAT_105ccb68[8]) == 2) {
        /* orig `xor al,al` then nine `mov [esi+off],al`. 0x364/0x365 skipped;
         * 0x36C is written third. */
        z = 0;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f01FE)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f01FF)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f0208)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f0202)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f0203)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f0204)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f0205)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f0206)))) = z;
        ((*(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].f0207)))) = z;
    }

    /* orig reloads both cursor and count from [iPlayer*4+disp], then
     * `add ebx,0x18` for the next slot rather than rebuilding the index.
     *
     * THE SUBTRAHEND IS SPELLED AS A POINTER FIELD, AT EVERY USE.  The
     * original `fld`s the three state fields and subtracts the car fields
     * from memory (`fld [esp+S]; fsub [esi+0x1dc]`); the BR_CAR_F32
     * base-offset spelling gives the reverse (`fld [esi+0x1dc];
     * fsubr [esp+S]`) -- the operand hand is decided by how the operand is
     * NAMED, not by the term order, which would change the value.  A cached
     * `const BrVec3 *pPos` local gets the hand right but CSEs the base and
     * drops 6 instructions; the cast written out at each use keeps the
     * original's per-component re-derivation.  Byte-exact 2026-09-10.
     * Earlier dead list, all byte-identical to the macro form: a paren round
     * the state operand, named dx/dy/dz temps, a `BrCarState *ps = &state`
     * pointer, `float *pf = &state.f10` with pf[0..2], copying the three
     * fields to float locals first, `-car + state`, and a past-the-end
     * `pc[-3..-1]` pointer. */
    if (g_BrReplayCursor[iPlayer] < g_BrReplayCount[iPlayer] - 2) {
        BrFixDecodeRecord_10007AA0(&state, &pSlot[1].rec);

        ((*(float *)(void *)(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].rb.st.vel.x + 0)))) =
            (state.f10
             - (&pCar->aBody[0].rb.st.pos)->x)
            * BR_K_0008FAA8;
        ((*(float *)(void *)(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].rb.st.vel.x + 4)))) =
            (state.f14
             - (&pCar->aBody[0].rb.st.pos)->y)
            * BR_K_0008FAA8;
        ((*(float *)(void *)(((unsigned char *)&((BrDriverCar *)(((pCar))))->aBody[0].rb.st.vel.x + 8)))) =
            (state.f18
             - (&pCar->aBody[0].rb.st.pos)->z)
            * BR_K_0008FAA8;
    }
}

/* 0x1006AD10 */
/* WHAT IT DOES: works the replay transport from the buttons the player is
 * holding -- play, step one frame either way, jump ten frames either way --
 * and moves every car's playback position by the amount decided, stopping at
 * the two ends of the recording. Holding a jump beats holding a single step,
 * and asking for play beats both. Letting go of everything while scrubbing
 * leaves the replay paused. */
/* @implements 0x1006AD10 d3d BrReplaySeek */
void BrReplaySeek(void)
{
    const uint32_t bits = g_BrX18ABAD0;
    int32_t        step = 0;
    int32_t        state;
    int            i;

    /* THE MODE STORE COMES FIRST IN EACH ARM, and the tail RE-READS the mode
     * out of the global rather than carrying it in a local.  Both are visible
     * in the codegen: storing `step` first makes VC5 materialise 10 before 3
     * (`mov R,0xa` where the original has `mov R,3`), and a carried local
     * loses the reload the original spends before the transport test.  With
     * both, size and instruction count are exact (215 B, 61 insns) and the
     * register-blind residue falls 6+6 -> 2+2.  Re-reading is the same value
     * on every path: each arm writes the local and the global together. */
    if (bits & 0x00200000u) {
        (DAT_105ccb68[8]) = 3; state = 3; step = 1;
    } else if (bits & 0x00400000u) {
        (DAT_105ccb68[8]) = 3; state = 3; step = -1;
    } else {
        state = (DAT_105ccb68[8]);
    }

    /* The second pair is tested unconditionally and overrides the first. */
    if (bits & 0x00800000u) {
        (DAT_105ccb68[8]) = 3; state = 3; step = 10;
    } else if (bits & 0x01000000u) {
        (DAT_105ccb68[8]) = 3; state = 3; step = -10;
    }

    if (bits & 0x00100000u) {
        (DAT_105ccb68[8]) = 1; state = 1;
    }

    state = (DAT_105ccb68[8]);
    if (state == 1) {
        step = 1;                   /* `mov esi,eax` with eax == 1 */
    } else if (state == 3 && step == 0) {
        (DAT_105ccb68[8]) = 2;
    }

    /* THE CURSOR IS A COMPOUND ASSIGNMENT ON THE GLOBAL, AND THE CLAMP
     * RE-READS THE GLOBAL.  `g[i] += step; if (g[i] < 0)` loads the cursor
     * into the register first and adds the step (`mov ecx,[..]; add ecx,esi`),
     * and the re-read compare is a separate expression from the add, so VC5
     * spends `cmp ecx, edi` against the zero web instead of folding the test
     * into the add's sign flag (`jns`).  A named `v = g[i] + step` local gave
     * the fold from every clamp spelling (ten probes, 2026-09-10).
     * Byte-exact 2026-09-13. */
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i) {
        g_BrReplayCursor[i] += step;
        if (g_BrReplayCursor[i] < 0) {
            g_BrReplayCursor[i] = 0;
        } else if (g_BrReplayCursor[i] > g_BrReplayCount[i] - 1) {
            g_BrReplayCursor[i] = g_BrReplayCount[i] - 1;
        }
    }
}


/* 0x1006AA50 */
/* WHAT IT DOES: throws away any recording in progress and switches recording
 * off, so the next race starts with an empty replay. How many cars it clears
 * depends on the game mode -- one in the two single-car modes, eight
 * otherwise. */
/* @implements 0x1006AA50 d3d BrReplayReset */
void BrReplayReset(void)
{
    int n;
    int i;

    /* Open-coded: two callers keep BrReplayActiveCount from inlining. */
    n = 8;
    if ((*(int32_t *)&g_brRaceRules.mode) == 2 || (*(int32_t *)&g_brRaceRules.mode) == 4)
        n = 1;

    /* The `test ecx,ecx / jle` guard is dead (n is 1 or 8) but is kept as the
     * loop bound so the shape matches. */
    for (i = 0; i < n; ++i)
        g_BrReplayCount[i] = 0;

    g_BrReplayOn = 0;
}

/* 0x1006ABB0 */
/* WHAT IT DOES: sends the replay back to the start -- every car's playback
 * position returns to its first recorded frame. The recording itself is
 * untouched. */
/* @implements 0x1006ABB0 d3d BrReplayRewind */
void BrReplayRewind(void)
{
    int i;
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        g_BrReplayCursor[i] = 0;
}

/* 0x1006ACF0 */
/* WHAT IT DOES: the same as the above, for a car that already knows its own
 * number -- it looks the number up on the car rather than being told it. */
/* @implements 0x1006ACF0 d3d BrReplayApplyCar */
void BrReplayApplyCar(BrDriverCar *pCar)
{
    BrReplayApply(pCar, ((*(int32_t *)&pCar->f140)));
}


/* WHAT IT DOES: return the byte size of the current replay (frame count * 0x18). */
/* @implements 0x10063B50 glide BrReplayGetSize */
/* @n64 0x80226070 located */

int BrReplayGetSize(void)

{
  return g_BrReplayCount[0] * 0x18;
}

