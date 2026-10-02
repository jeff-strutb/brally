/* br_optcycle.c -- menus: the options rows' cyclers and openers, the packet
 * at 0x10042880-0x100446D0 (46 functions).
 *
 * The whole of the former slice2_25.c, moved rather than split (the
 * br_menucb.c precedent): every function here is an options-screen row
 * callback and they share one static module state and the inline message
 * helpers, so taking the file apart would duplicate that state -- and eight
 * of the cyclers are byte-exact only inside this translation unit.
 *
 * See slice2_25.h for what the module is and how the three repeated shapes
 * work. Everything here is a transcription; the DEVIATION list is at the
 * bottom of the file and every deviation is also marked at its line.
 *
 * A WARNING FOR INTEGRATION, about br_slots.h
 * -----------------------------------------------
 * br_slots.h declares
 *
 *     typedef struct BrSlotTable { BrSlot aSlots[8]; int count; } BrSlotTable;
 *
 * with the comment that `count` is 0x10AA288C. That struct is NOT the memory
 * layout: the slot array ends at 0x10AA2598 and 0x10AA288C is 0x2F4 bytes
 * further on. This packet reads and writes both, independently, and they
 * cannot be one object. The array is exposed here as g_aBrAA2538 and
 * 0x10AA288C as the separate flag g_brAA288C. (In this packet 0x10AA288C is
 * used as a flag -- set to 1 at 0x10043B10, tested at 0x10043925 -- not as a
 * count, which is further evidence they are unrelated.)
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_dl.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include "slice2_25.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* initialised in the original source (restored: the 64-bit core keeps them
 * private to this file; no Glide relocation places them elsewhere) */
static const char g_szBrTimeAttack[] = "TimeAttack";
static const char g_szBrGrfExt[]     = ".grf";

/* KERNEL32 IAT used verbatim by BrOpt3A00 (0x1003CF50) / BrOpt3810. */
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: GlobalUnlock is declared by the platform headers */
/* 64-bit core: GlobalFree is declared by the platform headers */


/* ==========================================================================
 * Storage
 * ========================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 0x100BD3E0 (D3D) / 0x100BCBE8 (Glide) -- THE LAP TOTAL, and it is 1 in the
 * image, not 0.
 *
 * This was a bare definition, so it started at zero. The race loop compares the
 * driver's lap against it and, on a match, replaces the reported best time with
 * a sentinel -- "on the final lap, report the sentinel instead". At zero that
 * branch is taken on the FIRST frame of every race, before a lap exists.
 *
 * Read out of BOTH binaries at the paired addresses; both say 1. Note the two
 * builds' addresses differ and neither name mentions the other, which is why a
 * grep for one finds nothing about the other -- br_race.h models the Glide
 * address as BrRaceRules::nLaps while this models the D3D one. Same object.
 * This definition owns the storage. */
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
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* g_brPAA2904 is deliberately absent: 0x10AA2904 is one dword and its storage
 * is the slot br_phasecur.h owns.  slice2_25.h makes the name an alias for it. */
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

/* Literals in the DLL's read-only data. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100AD33C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x100AD334 */

/* ==========================================================================
 * Shared helpers
 *
 * These three sequences appear byte-for-byte in many of the functions below.
 * Factoring them keeps the transcription honest -- a copy-paste error in one
 * of eleven copies would be invisible.
 * ========================================================================== */

/* The tail of every "show a message" block: hand the text to the UI and then
 * copy 0x1039B720 over it. 0x1039B720 lies past the end of the DLL's
 * initialised data (see slice1_06.h), so at load time it is an empty string
 * and this is effectively "clear the buffer" -- but only effectively. */
/* The original INLINES this in every announcing cycler: the 3-arg send,
 * then an intrinsic strcpy (repne scasb + rep movsd/movsb). */
static __inline void BrOptFlushMessage(void)
{
    FUN_100368a0((*(void * *)&g_brOwner5BC72C), g_brPA9D008, 1);
    strcpy(g_szBrName4DB0, g_aBr39B720);        /* DEVIATION: rep movsb */
}

/* The screen-object install sequence shared by 0x10043260, 0x10043330,
 * 0x100434C0, 0x10043CD0, 0x10043DA0, 0x10043E70, 0x100440D0, 0x10044280,
 * 0x100443E0 and 0x100446D0.
 *
 * Returns 1 when *ppSlot holds an object afterwards (created or reused) and
 * 0 when the allocation failed -- which is exactly the eax the original
 * leaves behind on each path. The C++ exception frame the original sets up
 * (`push -1 / push <funclet> / fs:[0]`, and the state variable it keeps at
 * [esp+0xC]) has no observable effect and is not reproduced. */
/* (port-only BrOptEnsureObj removed) */


/* The plain wrapping cycler: up on 0x10AA33D4, down on 0x10AA33D0, no
 * validity filtering. `*pv` is the option, [0..max] inclusive. Returns the
 * resulting value. Note the original always writes the global back on the
 * edited paths and never writes it when neither input is set. */
/* The original INLINES this body in every cycler (the 93-99 B siblings are
 * each three times the size of the un-inlined port build). */
/* (port-only BrOptCycle removed) */


/* ==========================================================================
 * 0x10042880
 * ========================================================================== */

/* Builds "TimeAttack" + decimal(*pIndex) + ".grf" and copies the result over
 * 0x11782BC8 -- which slice1_06.h identifies as one of the two fixed
 * save-file path buffers (the ghost one). So this call CLOBBERS that path.
 *
 * The two working buffers are laid out exactly as the original's stack frame
 * has them, because they OVERLAP: the itoa output sits at frame+0x10 and the
 * name being assembled starts at frame+0x14, i.e. only four bytes later. The
 * original therefore only survives while the number is at most three digits.
 * Reproduced rather than fixed. */
/* (port-only BrOptBeginTimeAttack removed) */


/* ==========================================================================
 * 0x10042A90 / 0x10042AC0 / 0x10042B00 -- three identical copies
 * ========================================================================== */

/* GOTCHA: 0x10AA28D8 is a latch, not a debounce -- nothing in this packet
 * ever clears it, so across all three entry points the field is toggled at
 * most once per clear of that global. The return value is 1 either way. */
/* (port-only BrOptToggle2F7C removed) */


/* (port-only BrOptToggle2F7C_A removed) */

/* (port-only BrOptToggle2F7C_B removed) */

/* (port-only BrOptToggle2F7C_C removed) */


/* ==========================================================================
 * 0x10042B30 -- track select
 *
 * GOTCHA, shared with 0x10042EE0: the "have I been all the way round"
 * comparison is against the value the search STARTED FROM, which is the
 * option already stepped once -- not the value on entry. So when every
 * candidate is rejected the option still ends up moved by one, on the first
 * candidate, rather than back where the user left it.
 * ========================================================================== */

/* The track-number step, written as the original's inline pre-increment on
 * the global.  As helpers, VC5 materialises 0 and 0x1F at every use site
 * the way the original does; written out in the caller it keeps them in
 * callee-saved registers. */
static __inline int32_t BrOptTrackStepUp(void)
{
    if (++(*(int32_t *)&g_brSel0ABDF4) > BR_OPT_TRACK_MAX)
        (*(int32_t *)&g_brSel0ABDF4) = 0;
    return (*(int32_t *)&g_brSel0ABDF4);
}

static __inline int32_t BrOptTrackStepDown(void)
{
    if (--(*(int32_t *)&g_brSel0ABDF4) < 0)
        (*(int32_t *)&g_brSel0ABDF4) = BR_OPT_TRACK_MAX;
    return (*(int32_t *)&g_brSel0ABDF4);
}

/* WHAT IT DOES: moves the track selection on to the next track the player
 * is allowed to pick, skipping any that are still locked, and announces the
 * new choice to the other players in a network game. If every track is
 * rejected the selection still ends up moved by one rather than back where
 * the player left it, because the search compares against the already-
 * stepped value. */
/* @implements 0x10042B30 d3d BrOptCycleTrack */
int BrOptCycleTrack(void)
{
    int32_t v, vStart;

    if (g_act1 != 0) {
        v = BrOptTrackStepUp();
        vStart = v;
        if (BrOptAvailB(v) == 0) {
            for (;;) {
                v = BrOptTrackStepUp();
                /* Unlike 0x10042EE0's loop, the wrap path here still runs
                 * the full-circle test. */
                if (v == vStart)
                    break;
                if (BrOptAvailB(v) != 0) {
                    v = (*(int32_t *)&g_brSel0ABDF4);
                    break;
                }
            }
        } else {
            v = (*(int32_t *)&g_brSel0ABDF4);
        }
    } else if (g_act0 != 0) {
        v = BrOptTrackStepDown();
        vStart = v;
                if (BrOptAvailB(v) != 0) {
                    v = (*(int32_t *)&g_brSel0ABDF4);
                    break;
                } else {
            v = (*(int32_t *)&g_brSel0ABDF4);
        }
    } else {
        v = (*(int32_t *)&g_brSel0ABDF4);
    }

    (*(int32_t *)&g_226e7c) = g_aBrAC420[v];

    if (g_brP277B40 != NULL) {
        /* 32 tracks, 16 names: indices 0x10..0x1F reuse the first sixteen
         * name strings.  v itself is reduced (a separate index local turns
         * the `sub` into `add -0x10`). */
        if (v > 0xF)
            v -= 0x10;
        /* sprintf itself through the /MD import, not the BrSprintf
         * wrapper -- the original's `call dword ptr [sprintf]`. */
        sprintf(g_szBrName4DB0, BrStrGet(BR_OPT_STR_TRACK),
                BrStrGet((int)g_aBrAC368[v]));
        BrOptFlushMessage();
    }
    return 1;
}

/* ==========================================================================
 * 0x10042C80 .. 0x10042E80 -- plain cyclers
 * ========================================================================== */

/* 0x10042C80 */
/* WHAT IT DOES: steps one of the settings rows to its next value and copies
 * the result into the place the game reads it from. Which setting this is
 * was not established. */
/* @implements 0x10042C80 d3d BrOptCycleAC65C */
int BrOptCycleAC65C(void)
{
    /* Spelled on the global directly -- the corpus-proven form (four
     * byte-exact twins at 0x1003CAE0/0x1003CB40/0x1003CBA0/0x1003CC00):
     * through the BrOptCycle helper the counter lands in ecx and the
     * `mov eax,1` hoists above the table read, one byte long (2026-09-09). */
    if (g_act1 != 0) {
        g_i0AC65C = g_i0AC65C + 1;
        if (g_i0AC65C > BR_OPT_AC65C_MAX) {
            g_i0AC65C = 0;
        }
    }
    else {
        if (g_act0 != 0) {
            g_i0AC65C = g_i0AC65C - 1;
            if (g_i0AC65C < 0) {
                g_i0AC65C = BR_OPT_AC65C_MAX;
            }
        }
    }
    (*(int32_t *)&g_7b320) = g_i0AC65C;
    return 1;
}

/* 0x10042CF0. GOTCHA: 0x10060D90 is called on EVERY path, including the one
 * where neither input is set -- contrast 0x10044600. */
/* (port-only BrOptCycleB4E708 removed) */


/* 0x10042D60. Same as above but clears 0x100AB3D8 instead of setting it. */
/* (port-only BrOptCycleB4E70C removed) */


/* 0x10042DC0 */
/* WHAT IT DOES: steps another settings row on to its next value, looking
 * the real value up in that row's table. Which setting this is was not
 * established. */
/* @implements 0x10042DC0 d3d BrOptCycleAC64C */
int BrOptCycleAC64C(void)
{
    /* Spelled on the global directly -- the corpus-proven form (four
     * byte-exact twins at 0x1003CAE0/0x1003CB40/0x1003CBA0/0x1003CC00):
     * through the BrOptCycle helper the counter lands in ecx and the
     * `mov eax,1` hoists above the table read, one byte long (2026-09-09). */
    if (g_act1 != 0) {
        (*(int32_t *)&DAT_100abdec) = (*(int32_t *)&DAT_100abdec) + 1;
        if ((*(int32_t *)&DAT_100abdec) > BR_OPT_AC64C_MAX) {
            (*(int32_t *)&DAT_100abdec) = 0;
        }
    }
    else {
        if (g_act0 != 0) {
            (*(int32_t *)&DAT_100abdec) = (*(int32_t *)&DAT_100abdec) - 1;
            if ((*(int32_t *)&DAT_100abdec) < 0) {
                (*(int32_t *)&DAT_100abdec) = BR_OPT_AC64C_MAX;
            }
        }
    }
    (*(int32_t *)&g_7b32c) = g_aBrAC4A0[(*(int32_t *)&DAT_100abdec)];
    return 1;
}

/* 0x10042E20 */
/* WHAT IT DOES: steps another settings row on to its next value in the same
 * way. Which setting this is was not established. */
/* @implements 0x10042E20 d3d BrOptCycleAC650 */
int BrOptCycleAC650(void)
{
    /* Spelled on the global directly -- the corpus-proven form (four
     * byte-exact twins at 0x1003CAE0/0x1003CB40/0x1003CBA0/0x1003CC00):
     * through the BrOptCycle helper the counter lands in ecx and the
     * `mov eax,1` hoists above the table read, one byte long (2026-09-09). */
    if (g_act1 != 0) {
        (*(int32_t *)&DAT_100abdf0) = (*(int32_t *)&DAT_100abdf0) + 1;
        if ((*(int32_t *)&DAT_100abdf0) > BR_OPT_AC650_MAX) {
            (*(int32_t *)&DAT_100abdf0) = 0;
        }
    }
    else {
        if (g_act0 != 0) {
            (*(int32_t *)&DAT_100abdf0) = (*(int32_t *)&DAT_100abdf0) - 1;
            if ((*(int32_t *)&DAT_100abdf0) < 0) {
                (*(int32_t *)&DAT_100abdf0) = BR_OPT_AC650_MAX;
            }
        }
    }
    (*(int32_t *)&g_7b328) = g_aBrAC4B0[(*(int32_t *)&DAT_100abdf0)];
    return 1;
}

/* 0x10042E80 */
/* WHAT IT DOES: steps another settings row on to its next value in the same
 * way. Which setting this is was not established. */
/* @implements 0x10042E80 d3d BrOptCycleAA2A08 */
int BrOptCycleAA2A08(void)
{
    /* Spelled on the global directly -- the corpus-proven form (four
     * byte-exact twins at 0x1003CAE0/0x1003CB40/0x1003CBA0/0x1003CC00):
     * through the BrOptCycle helper the counter lands in ecx and the
     * `mov eax,1` hoists above the table read, one byte long (2026-09-09). */
    if (g_act1 != 0) {
        (*(int32_t *)&DAT_10ac5d60) = (*(int32_t *)&DAT_10ac5d60) + 1;
        if ((*(int32_t *)&DAT_10ac5d60) > BR_OPT_AA2A08_MAX) {
            (*(int32_t *)&DAT_10ac5d60) = 0;
        }
    }
    else {
        if (g_act0 != 0) {
            (*(int32_t *)&DAT_10ac5d60) = (*(int32_t *)&DAT_10ac5d60) - 1;
            if ((*(int32_t *)&DAT_10ac5d60) < 0) {
                (*(int32_t *)&DAT_10ac5d60) = BR_OPT_AA2A08_MAX;
            }
        }
    }
    (*(int32_t *)&g_7b324) = g_aBrAC518[(*(int32_t *)&DAT_10ac5d60)];
    return 1;
}

/* ==========================================================================
 * 0x10042EE0 -- vehicle select
 * ========================================================================== */

/* `neg / sbb / and 3 / add 0xB`: 14 when 0x10AA28FC is non-zero, else 11.
 * The original recomputes this at every single step of the search, so it is
 * a function here rather than a value hoisted out of the loop. */
/* (port-only BrOptCarMax removed) */


/* (port-only BrOptCycleCar removed) */


/* ==========================================================================
 * 0x100430B0 -- the 1-based cycler
 * ========================================================================== */

/* WHAT IT DOES: steps the number-of-laps-style setting up or down, wrapping
 * round its range -- and this one counts from one rather than zero, so it
 * never lands on nothing. It announces the new number to the other players. */
/* @implements 0x100430B0 d3d BrOptCycleBD3E0 */
int BrOptCycleBD3E0(void)
{
    /* 0x20, not BR_OPT_TEXT_MAX: `sub esp,0x20` and `lea ecx,[esp+8]` with
     * esp == E-0x28 put the scratch at E-0x20, and it is the whole frame.
     * The value is 1..12, so three bytes are ever used. */
    char      aNum[0x20];
    BrDPlay  *pGate;
    int32_t   v;

    /* NOT `else if`: the original loads the value ONCE for both the
     * decrement and the do-nothing exit (0x1003C62C sits between the down
     * flag's `test` and its `je`), which is an `else { v = g; if (down) }`,
     * not two arms each with their own load. This shape also merges the two
     * wrap stores into the single `mov [g_0BD3E0],eax` the original has at
     * 0x1003C643.
     *
     * BYTE-EXACT 2026-09-10, and it took BOTH halves of that trade at once.
     * The two wrap stores are merged by a `goto` into the down arm's store
     * -- the single `mov [g_0BD3E0],eax` the original has at 0x1003C643 --
     * and the do-nothing exit is split off as its own `else if (down == 0)`
     * arm so the value load is NOT common to both paths.  With one shared
     * load VC5 hoists it above the first `je`, eax is busy, and the down
     * flag has to go to ecx (`8b 0d` where the original has the one-byte
     * shorter `a1`); with the exit split out the original's two separate
     * loads come back and the flag keeps eax.  Either half alone is worse
     * than the plain shape: `else if` on its own un-merges the wrap stores,
     * the shared wrap on its own leaves the hoisted load.  Also probed
     * dead: `v = g - 1` for the decrement (emits lea/test/jge for the
     * original's dec/jns). */
    if (g_act1 != 0) {
        v = (*(int32_t *)&g_CBE8) + 1;
        (*(int32_t *)&g_CBE8) = v;
        if (v > BR_OPT_BD3E0_MAX) {
            v = BR_OPT_BD3E0_MIN;        /* wraps to 1, NOT to 0 */
            goto BR_WRAP;
        }
    } else if (g_act0 == 0) {
        v = (*(int32_t *)&g_CBE8);
    } else {
        v = (*(int32_t *)&g_CBE8);
        /* load / --v / store, NOT `v = g - 1`: see BrOptCycle above. */
        --v;
        (*(int32_t *)&g_CBE8) = v;
        if (v < BR_OPT_BD3E0_MIN) {
            v = BR_OPT_BD3E0_MAX;
BR_WRAP:
            (*(int32_t *)&g_CBE8) = v;
        }
    }

    /* The gate read before the store, as in 0x10043180 below: that is what
     * puts `mov ecx,[gate]` ahead of `mov [g_0AC658],eax` at 0x1003C648. */
    pGate = g_brP277B40;
    (*(int32_t *)&DAT_100abdf8) = v;

    if (pGate != NULL) {
        /* _itoa and sprintf through the /MD imports, not the wrappers --
         * the original's `call dword ptr [__imp__itoa]` / `[__imp_sprintf]`. */
        _itoa(v, aNum, 10);
        sprintf(g_szBrName4DB0, BrStrGet(BR_OPT_STR_BD3E0), aNum);
        BrOptFlushMessage();
    }
    return 1;
}

/* ==========================================================================
 * 0x10043180
 * ========================================================================== */

/* WHAT IT DOES: steps another settings row on and announces the new choice
 * by name. Note the caption is looked up by the VALUE the row now holds,
 * not by the row's position. Which setting this is was not established. */
/* DEAD 2026-09-09 (all at 212 B RAW 5+5 REGNORM 0+0, the documented
 * index-eax/gate-ecx pairing): declaration order; a blank line; index
 * casts; unsigned v; store order; helper result discarded and re-read;
 * a gate ternary; a named element; if (pGate); sprintf arg parens;
 * inlining the cycle body (+1, 7+5 -- keep the helper); every slot in
 * the TU (39 of 45 compile).
 * @t4-pass 0x1003C6D0 1 2026-09-09 probes 11 bytes 212 insns 63 regions 2 rows 0 census yes  (hand, fn.py variants incl. helper inline)
 * @t4-pass 0x1003C6D0 2 2026-09-09 probes 39 bytes 212 insns 63 regions 2 rows 0 census yes  (position sweep) */
/* Was @t3-certified 2026-09-09 at 212/211 B on the eax/ecx pairing of the
 * index and gate loads; BYTE-EXACT 2026-09-12 with the three spellings that
 * matched BrOptCycleAA2A18 below: the arms step the GLOBAL directly (no
 * BrOptCycle helper, no `v` -- the helper's returned value is what paired
 * the index with ecx), the table index RE-READS the global, and the gate
 * is tested inline with no local.  Only the three together land it; the
 * gate-inline spelling alone was measured worse on the helper form. */
/* @implements 0x10043180 d3d BrOptCycleAA2A00 */
int BrOptCycleAA2A00(void)
{
    if (g_act1 != 0) {
        (*(int32_t *)&DAT_10ac5d58) = (*(int32_t *)&DAT_10ac5d58) + 1;
        if ((*(int32_t *)&DAT_10ac5d58) > BR_OPT_AA2A00_MAX)
            (*(int32_t *)&DAT_10ac5d58) = 0;
    }
    else {
        if (g_act0 != 0) {
            (*(int32_t *)&DAT_10ac5d58) = (*(int32_t *)&DAT_10ac5d58) - 1;
            if ((*(int32_t *)&DAT_10ac5d58) < 0)
                (*(int32_t *)&DAT_10ac5d58) = BR_OPT_AA2A00_MAX;
        }
    }
    (*(int32_t *)&g_226e80) = g_aBrAC4C0[(*(int32_t *)&DAT_10ac5d58)];
    if (g_brP277B40 != NULL) {
        /* Indexed by the table VALUE, and re-read from the global rather
         * than kept in a local: VC5 CSEs the reload back into eax, which is
         * what puts the `a1` moffs form on the index load and hoists the
         * gate's own load above the table lookup. */
        /* sprintf through the /MD import, not the BrSprintf wrapper --
         * the original's `call dword ptr [sprintf]`, as 0x10042A70 above. */
        sprintf(g_szBrName4DB0, BrStrGet(BR_OPT_STR_AA2A00),
                BrStrGet((int)g_aBrAC3B0[(*(int32_t *)&g_226e80)]));
        BrOptFlushMessage();
    }
    return 1;
}

/* ==========================================================================
 * 0x10043260, 0x10043330, 0x100434C0 -- screen installers
 * ========================================================================== */

/* WHAT IT DOES: opens one of the menu screens, building it the first time
 * and reusing it afterwards. Which screen this is was not established. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003C7B0.cpp */
int BrOptOpen296C(BrGameObj *pUnused);

/* WHAT IT DOES: opens another menu screen the same way. Which screen this
 * is was not established. */
/* (port-only BrOptOpen2970 removed) */


/* (port-only BrOptOpen2998 removed) */


/* ==========================================================================
 * 0x10043400 -- the cycler that skips 1
 * ========================================================================== */

/* WHAT IT DOES: steps a settings row that has a hole in it: value one is
 * skipped in both directions, so stepping up goes straight past it and
 * stepping down does too. The result also selects which of four control
 * layouts is the active one. */
/* @implements 0x10043400 d3d BrOptCycleAA2A0C */
/* Per-arm switch tail (record addresses as immediates).
 * BYTE-EXACT 2026-09-12 (was parked at 176/178 B on a hoisted load): every
 * step and test is spelled ON THE GLOBAL, no `v` until the tail, and the
 * down arm is a plain `else if` -- VC5 then forwards each store into the
 * next test, loads the global per arm (the `v = g_brAA2A0C` temp at the
 * head of the else arm was what let it hoist the load above the first
 * branch), cross-jumps the two skip stores by itself, and hoists the
 * inner else's read above its own `je` exactly as the original has it.
 * Same lever as BrOptCycleAA2A00 / BrOptCycleAA2A18 in this file. */
int BrOptCycleAA2A0C(void)
{
    int32_t v;

    if (g_act1 != 0) {
        (*(int32_t *)&g_brKind5D64) = (*(int32_t *)&g_brKind5D64) + 1;
        if ((*(int32_t *)&g_brKind5D64) >= BR_OPT_AA2A0C_MAX + 1)
            (*(int32_t *)&g_brKind5D64) = 0;
        if ((*(int32_t *)&g_brKind5D64) == 1)            /* stepping up skips 1 -> 2 */
            (*(int32_t *)&g_brKind5D64) = 2;
    } else if (g_act0 != 0) {
        (*(int32_t *)&g_brKind5D64) = (*(int32_t *)&g_brKind5D64) - 1;
        if ((*(int32_t *)&g_brKind5D64) < 0)
            (*(int32_t *)&g_brKind5D64) = BR_OPT_AA2A0C_MAX;
        if ((*(int32_t *)&g_brKind5D64) == 1)            /* stepping down skips 1 -> 0 */
            (*(int32_t *)&g_brKind5D64) = 0;
    }
    v = (*(int32_t *)&g_brKind5D64);

    (g_aBrB4E710[6]) = v;
    v = g_aBrAC520[v];
    (*(int32_t *)&g_BrCtrlCfg.active) = v;

    switch (v) {
    default:
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[0];
        return 1;
    case 3:
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[3];
        return 1;
    case 2:
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[2];
        return 1;
    case 1:
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[1];
        return 1;
    }
}

/* ==========================================================================
 * 0x10043590, 0x100435F0, 0x10043650, 0x100436B0 -- two-state cyclers
 * ========================================================================== */

/* (port-only BrOptCycleAA2A1C removed) */


/* (port-only BrOptCycleAA2A28 removed) */


/* (port-only BrOptCycleAA2A20 removed) */


/* (port-only BrOptCycleAA2A24 removed) */


/* ==========================================================================
 * 0x10043760 .. 0x10043A00 -- transitions and the lobby
 * ========================================================================== */

/* 0x10043760. Returns 0. */
/* WHAT IT DOES: leaves the current menu screen: it tells the screen to
 * close, resets a couple of race settings in single-player, saves the
 * settings block and returns to the caller with "stop here". */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003CCB0.cpp */
/* BrOpt3760: prototype in br_funcs.h */

/* 0x100437D0 */
/* WHAT IT DOES: backs out of a network screen when the connection has gone
 * away -- it closes the screen and tears the session down. If the
 * connection is still up it does nothing at all. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003CD20.cpp */
/* BrOpt37D0: prototype in br_funcs.h */

/* 0x10043810 */
/* WHAT IT DOES: the once-a-frame handler for the network lobby: it deals
 * with the player having left or the host having started, notes for each
 * connected player whether anybody else is present, and on the way out
 * closes the lobby and plays a sound. Most of the function is the several
 * different ways of leaving. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003CD60.cpp */
/* BrOpt3810: prototype in br_funcs.h */

/* ==========================================================================
 * 0x10043CD0 .. 0x10043E70 -- more screen installers
 * ========================================================================== */

/* WHAT IT DOES: opens another menu screen, building it once and reusing it.
 * Which screen this is was not established. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003D220.cpp */
/* BrOptOpen2940: prototype in br_funcs.h */

/* WHAT IT DOES: opens another menu screen the same way. Which screen this
 * is was not established. */
/* (port-only BrOptOpen298C removed) */


/* 0x10043E70. Unlike its siblings the reuse path does NOT return early: it
 * falls into the same tail as the create path. */
/* WHAT IT DOES: opens a menu screen and, unlike its siblings, keeps going
 * after finding an existing one -- so it also starts the network connection
 * attempt on every call, not just the first, provided the game is in a mode
 * that wants one. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003D3C0.cpp */
/* BrOptOpen2948: prototype in br_funcs.h */

/* ==========================================================================
 * 0x10043F50 .. 0x100440B0
 * ========================================================================== */

/* 0x10043F50. Returns 0. Glide match is src/core/cpp/0x1003D4A0.cpp
 * (C++ virtual thiscall; C __fastcall edx-slot colours vtbl into edx). */
/* WHAT IT DOES: leaves the current screen for the one behind it,
 * remembering which play mode was in force. The screen being left is told
 * to close first. */
/* BrOpt3F50: the placed body is BrOpt3F50_1003D4A0.cpp */

/* 0x10043FC0. Returns 0. Glide match is src/core/cpp/0x1003D510.cpp. */
/* WHAT IT DOES: leaves the current screen for the one behind it and clears
 * two globals belonging to the screen being closed. */
/* BrOpt3FC0: the placed body is BrOpt3FC0_1003D510.cpp */

/* (port-only BrOpt4010 removed) */

/* WHAT IT DOES: records the first play mode as the one under the cursor and
 * refreshes how that menu entry is drawn -- the highlight, not the choice. */
/* (port-only BrOpt4030 removed) */

/* (port-only BrOpt4050 removed) */

/* (port-only BrOpt4070 removed) */

/* WHAT IT DOES: chooses the third play mode and opens the screen that follows
 * it. */
/* (port-only BrOpt4090 removed) */

/* (port-only BrOpt40B0 removed) */


/* ==========================================================================
 * 0x100440D0 .. 0x100446D0
 * ========================================================================== */

/* WHAT IT DOES: opens another menu screen, building it once and reusing it.
 * Which screen this is was not established. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003D620.cpp */
/* BrOptOpen294C: prototype in br_funcs.h */

/* 0x10044280 */
/* WHAT IT DOES: joins somebody else's network game and opens the lobby
 * screen for it. It refuses to go ahead when the player has not typed a
 * long enough name in one of the modes, and falls back to setting the
 * connection up first if there is nothing to join yet. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003D7D0.cpp (T2 there: layout residue) */
/* BrOptOpen2950A: prototype in br_funcs.h */

/* 0x100443E0 */
/* WHAT IT DOES: the hosting counterpart: marks this machine as the host and
 * opens the same lobby screen, with the host's own set of controls attached
 * to it. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003D930.cpp */
/* BrOptOpen2950B: prototype in br_funcs.h */

/* 0x100444C0. Returns 0. Glide match is src/core/cpp/0x1003DA10.cpp. */
/* WHAT IT DOES: leaves the network lobby and goes back to the screen behind
 * it, dropping the lobby's screens and, in the two modes that need it,
 * tearing the connection down and starting a fresh one. */
/* BrOpt44C0: the placed body is BrOpt44C0_1003DA10.cpp */

/* 0x10044600. GOTCHA: 0x10044540 is NOT called when neither input is set --
 * the "no input" branch jumps past it. Contrast 0x10042CF0. */
/* WHAT IT DOES: steps a settings row up or down through its values and
 * announces the new choice by name -- and unlike its cousin above it does
 * NOT re-apply the setting when neither the up nor the down input is
 * active. Which setting this is was not established. */
/* @implements 0x10044600 d3d BrOptCycleAA2A18 */
int BrOptCycleAA2A18(void)
{
    /* Byte-exact 2026-09-12.  Three spellings, all in the original: the
     * apply call sits INSIDE each arm (VC5 cross-jumps the two into one
     * `call` that the no-input path jumps past -- an `fEdited` flag costs
     * `xor eax,eax` at the top and a `test`); both arms step the GLOBAL
     * directly, the corpus-proven form of the plain cyclers above (a `v`
     * temp turns the down-arm's `dec` into `lea eax,[ecx-1]; test`); and
     * sprintf through the /MD import, as 0x10042A70. */
    if (g_act1 != 0) {
        (*(int32_t *)&DAT_10ac5d70) = (*(int32_t *)&DAT_10ac5d70) + 1;
        if ((*(int32_t *)&DAT_10ac5d70) >= BR_OPT_AA2A18_MAX + 1)
            (*(int32_t *)&DAT_10ac5d70) = 0;
        BrSub10044540();
    }
    else if (g_act0 != 0) {
        (*(int32_t *)&DAT_10ac5d70) = (*(int32_t *)&DAT_10ac5d70) - 1;
        if ((*(int32_t *)&DAT_10ac5d70) < 0)
            (*(int32_t *)&DAT_10ac5d70) = BR_OPT_AA2A18_MAX;
        BrSub10044540();
    }

    if (g_brP277B40 != NULL) {
        sprintf(g_szBrName4DB0, BrStrGet(BR_OPT_STR_AA2A18),
                BrStrGet((int)g_aBrAC3C8[(*(int32_t *)&DAT_10ac5d70)]));
        BrOptFlushMessage();
    }
    return 1;
}

/* 0x100446D0 */
/* WHAT IT DOES: opens the screen the game shows once a network race is
 * agreed on, switches the game into that mode, and, if this machine is the
 * host, starts hosting the session at that point. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003DC20.cpp */
/* BrOptOpen2954: prototype in br_funcs.h */

/* ==========================================================================
 * DEVIATIONs, collected
 * ==========================================================================
 *  - every `rep movsb` / `rep movsd` string move is written as strcpy,
 *    strcat or memcpy; the semantics are identical (the original always
 *    recomputes the length with `repne scasb` first)
 *  - the C++ exception frames (`push -1 / push funclet / fs:[0]` and the
 *    [esp+0xC] state variable) in the ten installers are dropped; nothing
 *    in this packet can throw
 *  - BrOptEnsureObj allocates sizeof(BrOptObj) instead of the literal 0xC8,
 *    because on a 64-bit host the three leading pointers make the object
 *    larger. Anything that assumes 0xC8 (notably the constructor at
 *    0x10048710) must be ported with the same struct
 *  - 0x100441A0 is declared void; the original leaves eax undefined on two
 *    of its three exits
 *  - the text buffers 0x10A9CDF0, 0x10A9DD28, 0x1039B720 and 0x11782BC8 are
 *    given a size of 0x104. The original bounds none of them, so no true
 *    size is recoverable; 0x104 is the path-buffer size the game uses
 *    elsewhere (slice1_06.h)
 *  - BrOptCycleBD3E0's itoa scratch is 0x104 bytes where the original's
 *    frame gives it 0x20; the original cannot overflow 0x20 with a value in
 *    1..12 either way
 *
 * NOT DEVIATED FROM, though it looks like a bug: the itoa scratch inside
 * 0x10042880 sits four bytes below the name buffer it is concatenated onto,
 * so a four-digit index would corrupt "TimeAttack" before it is read. The
 * port keeps both buffers at their original frame offsets so the behaviour
 * is preserved rather than silently repaired.
 */
