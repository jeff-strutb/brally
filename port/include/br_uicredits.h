/* br_uicredits.h -- GLIDE 0x1003AED0 (D3D 0x10041970, `shared`), 90 bytes:
 * THE MAIN MENU'S "CREDITS" ROW ACTION.
 *
 * RESPONSIBILITY: menus/ -- "the front end: pages, controls, navigation".
 *
 * ==========================================================================
 * WHY THIS ONE, AND WHAT THE PACKET FOUND INSTEAD
 * ==========================================================================
 *
 * br_uiroot.h's install audit lists ten hooks that Glide 0x100425E0 stores
 * into the main menu's controls, and marks three of them "NOT PORTED":
 *
 *     +08  0x1003E4A0  (D3D 0x10044F50)  row 3  "Quick Race"
 *     +08  0x1003E730  (D3D 0x100451E0)  row 4  "Options"
 *     +08  0x1003AED0  (D3D 0x10041970)  row 5  "Credits"
 *
 * TWO OF THE THREE WERE ALREADY PORTED, under their D3D addresses, and the
 * two builds' listings were compared instruction by instruction to be sure:
 *
 *     0x1003E4A0  ==  D3D 0x10044F50  ==  slice2_26.c
 *                     BrPhaseActivate_10044F50
 *     0x1003E730  ==  D3D 0x100451E0  ==  slice2_26.c
 *                     BrPhaseActivate_100451E0
 *
 * -- the exact "grep BOTH builds' addresses" trap CONVENTIONS.md names, and
 * br_uiroot.h fell into it for these two after having escaped it for the
 * other seven.  The same check clears two more addresses the same packet was
 * asked to transcribe:
 *
 *     0x10041D10  ==  D3D 0x100488C0  ==  slice3_32.c BrPhaseTick_100488C0
 *                     (the cursor driver -- the sole reader of apCtl[199])
 *     0x1003AF30  ==  D3D 0x100419D0  ==  slice5_62.c BrExt_100419D0
 *                     (SetStatusText)
 *
 * So 0x1003AED0 is the ONLY one of the five with no body anywhere in the
 * tree, and it is what this module transcribes.  Nothing else here is a
 * second transcription: the status-line setter is CALLED, not re-written.
 *
 * ==========================================================================
 * WHAT IT DOES: it does not open a screen, it starts a MOVIE
 * ==========================================================================
 *
 * Every other main-menu row publishes a phase -- allocate 0xC8, run the
 * constructor, store a pfnEnter, call it.  This one does not allocate
 * anything.  It sets the game mode to 4, picks which cinematic mode 4 is to
 * play, and then TEARS THE FRONT END DOWN by clearing the owning phase's
 * +0x68 and calling the phase vtable's +0x18.
 *
 * The listing, with the ESP traced, because a displacement means nothing
 * without it.  Let E be esp on entry (after the call pushed the return
 * address, so [E+0] is that return address and [E+4] is argument 0):
 *
 *   1003AED0  push 0x100ACAD8               esp = E-4
 *   1003AED5  call 0x1003AF30               SetStatusText(0x100ACAD8)
 *   1003AEDA  mov  eax,[0x10AC5D98]         esp still E-4 -- the argument is
 *                                           still on the stack: 0x1003AF30 is
 *                                           cdecl and does NOT pop it
 *   1003AEDF  xor  edx,edx                  edx = 0 for the rest of the body
 *   1003AEE1  add  esp,4                    esp = E   <- the cdecl cleanup
 *   1003AEE4  cmp  eax,edx
 *   1003AEE6  mov  [0x100A9360],4           GAME MODE = 4
 *   1003AEF0  je   1003AF04
 *   1003AEF2  mov  [0x105BC760],2               "outro"
 *   1003AEFC  mov  [0x10AC5BF4],edx             ...and clear that flag
 *   1003AF02  jmp  1003AF0E
 *   1003AF04  mov  [0x105BC760],1               "credits"
 *   1003AF0E  mov  eax,[esp+4]              esp is E again, so THIS IS THE
 *                                           ONE ARGUMENT -- the control.
 *                                           Read as [E-4 + 4] it would be the
 *                                           return address instead.
 *   1003AF12  push edx                      f18's second argument: NULL
 *   1003AF13  mov  ecx,[eax+0x2AE8]         pCtl->pOwner  (br_ui.h)
 *   1003AF19  mov  [ecx+0x68],edx           pOwner->f68 = 0
 *   1003AF1C  mov  ecx,[eax+0x2AE8]         RE-READ, same value
 *   1003AF22  mov  eax,[ecx]                pOwner->pVtbl
 *   1003AF24  call [eax+0x18]               __thiscall; the callee pops the
 *                                           pushed NULL -- there is no
 *                                           `add esp,4` after it and the
 *                                           `ret` would otherwise be wrong
 *   1003AF27  xor  eax,eax
 *   1003AF29  ret                           RETURNS 0
 *
 * The D3D twin 0x10041970 is the same 90 bytes with the four global addresses
 * and the callee relocated; it was disassembled and compared, so the builds
 * cannot disagree about this body.
 *
 * ==========================================================================
 * THE SELECTOR AT 0x105BC760 IS THE CINEMATIC INDEX, AND THE MOVIES NAME IT
 * ==========================================================================
 *
 * 0x105BC760 (D3D 0x106805B8) has thirteen references in BRGlide's .text.
 * The one that decodes it is inside the step driver 0x10019A70 (D3D
 * 0x1002C500), at 0x10019DF1:
 *
 *     mov eax,[0x105BC760]
 *     sub eax,ebp   / je  1E25     -> "RallyIntro1.dat" / "RallyIntro2.dat"
 *     dec eax       / je  1E15     -> "RallyCredits.dat"
 *     dec eax       / jne 1E63     -> "RallyOutro.dat"
 *
 * ebp is ZERO through that whole block -- `xor ebp,ebp` at 0x10019A8E is the
 * only write to it at a lower address than 0x10019DF1, and the block itself
 * uses ebp three more times as a zero (0x10019DA5 and 0x10019DB5 store it
 * into globals, and 0x10019E52 resets a wrapping index to it).  So:
 *
 *     0 = intro     1 = credits     2 = outro
 *
 * and the two other writers agree with that reading without being told to:
 * the boot state machine's "set video mode" step (Glide 0x1001CE20,
 * br_boot.h's BR_APP_SET_MODE) sets mode 4 with selector 0 -- the attract
 * INTRO -- on the same `f68 = 0 / vtbl+0x18` teardown this function uses,
 * eight instructions that are otherwise identical; and slice2_17.c's
 * BrS17SetMode4 (Glide 0x10019900) sets mode 4 with selector 2, the OUTRO,
 * at the end of a race.  Three call sites, three different selector values,
 * each matching its own context.  That is the "check it a second way"
 * CONVENTIONS.md asks for, and it is what makes 0/1/2 a reading rather than
 * a guess.
 *
 * The flag that chooses between 1 and 2 is 0x10AC5D98 (D3D 0x10AA2A40), and
 * it has exactly TWO references in the whole image: this function reads it,
 * and Glide 0x100409C0 -- D3D 0x10047590, already ported as slice3_31.c's
 * BrPhaseMode_10047590 -- sets it to 1.  It is never cleared.  So the Credits
 * row plays the credits until something arms that flag, and the outro after.
 *
 * NOT CLAIMED: what arms it.  0x100409C0 is one of slice3_31.h's six "arm a
 * flag and change mode" callbacks and this packet did not chase its
 * installer.  Naming it "the championship was finished" would be a story, not
 * a reading, so the field is named for what is measured -- which movie it
 * selects -- and the rest is left open.
 *
 * ==========================================================================
 * THE RETURN VALUE IS LOAD-BEARING, unlike most in this corpus
 * ==========================================================================
 *
 * CONVENTIONS.md warns that a dead return value says nothing about side
 * effects.  Here the return value is not dead.  br_uinav.c's page frame
 * (0x10048530's activate arm) does
 *
 *     r = pCtl->pfn08(pCtl);
 *     if (r == 0)
 *         return 0;
 *
 * so a pfn08 returning 0 STOPS the frame immediately -- before the activate
 * bit is cleared and before the rest of the page is walked.  This hook
 * returns 0, and that is coherent with what it just did: the phase that owns
 * the page has been shut down, so continuing to walk its controls would be
 * walking a dead object.  The suite asserts the 0 and the mutation table
 * shows the assertion failing when it is changed to 1.
 *
 * ==========================================================================
 * STORAGE: FOUR GLOBALS, NONE OF THEM OWNED HERE
 * ==========================================================================
 *
 * CONVENTIONS.md's "aliased storage: a link-clean bug" is the whole reason
 * this module defines no globals of its own.  All four words already have
 * host storage, three of them under several names:
 *
 *   Glide       D3D          existing model(s)
 *   ----------  -----------  ---------------------------------------------
 *   0x100A9360  0x100AA010   br_appstart.h g_brCfgGameMode; br_race.h
 *                            BrRaceRules::mode; br_uinav.h BrUiNav::n0AA010;
 *                            slice2_26.h n0AA010; slice2_17.h f0AA010; ...
 *   0x105BC760  0x106805B8   slice2_17.h BrS17State::f6805B8
 *   0x10AC5D98  0x10AA2A40   slice3_31.h BrPhase31Ext::nAA2A40
 *   0x10AC5BF4  0x10AA289C   slice6_73.h nAA289C; slice2_25.h g_brAA289C;
 *                            slice1_06.h fAlt; slice2_24.h gAA289C; ...
 *
 * So the module takes POINTERS, supplied by whoever wires it, exactly as
 * br_uiroot.h takes its style rectangles and its error host.  Coining a fifth
 * name for 0x10AA289C is the failure mode that header is avoiding, and this
 * one avoids it the same way.
 *
 * ==========================================================================
 * AN ALIAS THIS PACKET FOUND AND DID NOT CREATE
 * ==========================================================================
 *
 * The status-line setter is called through slice5_62.c's BrExt_100419D0,
 * which is the ported 0x1003AF30 / D3D 0x100419D0.  Its "which control is the
 * status line" index lives in `BrX419D0State::index`, modelled from D3D
 * 0x10A9DBD0; br_uiroot.h models the SAME word from Glide 0x10AC4C58 as
 * `g_brUiRootStatusIdx` and br_uiroot.c is its only writer.  The pairing was
 * confirmed by disassembling both: Glide 0x1003AF30 reads 0x10AC4C58 where
 * D3D 0x100419D0 reads 0x10A9DBD0, at the same instruction.
 *
 * That is one object with two host storages, and the builder writes the one
 * the setter does not read -- so today the status line is captioned through
 * whatever index slice5_62's state happens to hold.  It is recorded here and
 * NOT patched: br_uiroot.c and slice5_62.c are both outside this packet, and
 * the fix is to make one a view onto the other (the precedent is
 * g_brAA26F4 in slice5_63), not to add a third writer.
 */
#ifndef BR_UICREDITS_H
#define BR_UICREDITS_H

#include <stddef.h>
#include <stdint.h>

#include "br_ui.h"      /* BrUiCtl_ (pOwner at the original's +0x2AE8),
                         * BrUiCtlHookFn_ -- the slot's type (ADJ-8)       */
#include "br_phase.h"    /* BrPhase_, BrPhaseVtbl_::f18, BrPhase_::f68     */

/* ==========================================================================
 * The three constants the body writes, each an immediate in the listing.
 * ========================================================================== */

/* 0x1003AEE6 -- `mov dword ptr [0x100A9360], 4`. */
#define BR_UICREDITS_GAME_MODE      4

/* The cinematic selector's values, decoded from 0x10019DF1 (see the banner).
 * Only CREDITS and OUTRO are written here; INTRO is named because it is what
 * pins the other two -- the boot step Glide 0x1001CE20 writes it. */
#define BR_UICREDITS_MOVIE_INTRO    0
#define BR_UICREDITS_MOVIE_CREDITS  1   /* 0x1003AF04, the flag == 0 arm */
#define BR_UICREDITS_MOVIE_OUTRO    2   /* 0x1003AEF2, the flag != 0 arm */

/* ==========================================================================
 * Everything the body reaches outside itself.
 *
 * Supplied, never defaulted, for br_uiroot.h's reason: a caller that supplies
 * nothing must not receive a plausible menu action.
 * ========================================================================== */
typedef struct BrUiCreditsCtx {
    /* 0x100ACAD8 (D3D 0x100AD300) -- the status line's text, a literal
     * pointer into .data holding a single space.  This is the SAME object
     * br_uiroot.h calls `pszStatus`; a host must bind both to one pointer.
     * No storage is defined for it here or there. */
    const char *pszStatus;

    /* 0x100A9360 (D3D 0x100AA010) -- the game mode.  Set to 4. */
    int32_t *pnGameMode;

    /* 0x105BC760 (D3D 0x106805B8) -- which cinematic mode 4 plays.
     * slice2_17.h's `BrS17State::f6805B8`. */
    int32_t *pnMovieSel;

    /* 0x10AC5D98 (D3D 0x10AA2A40) -- READ ONLY here; slice3_31.h's
     * `nAA2A40`, whose only writer in the image is 0x100409C0. */
    const int32_t *pnOutroFlag;

    /* 0x10AC5BF4 (D3D 0x10AA289C) -- cleared on the outro arm only.  Named
     * positionally on purpose: the corpus carries four disagreeing readings
     * of what it means, and this function's use does not settle it. */
    int32_t *pnAA289C;
} BrUiCreditsCtx;

extern BrUiCreditsCtx g_brUiCredits;

/* Non-zero when every slot the body dereferences is filled. */
int BrUiCreditsCtxComplete(const BrUiCreditsCtx *pCtx);

/* ==========================================================================
 * 0x1003AED0 itself.
 *
 * The type is br_ui.h's BrUiCtlHookFn_ (ADJ-8) -- __cdecl, one argument, and
 * the argument is the CONTROL -- so this is assignable straight into
 * br_uiroot.h's `BrUiRootHooks::p1003AED0` with no cast and no marshal.
 *
 * That is worth stating plainly, because slice8_90.h DECLINES to wire six
 * sibling hooks for the opposite reason: their bodies were written over
 * slice2_25.c's `BrGameObj`, a byte image that pins `pSub` at struct offset
 * 0x2AE8, and on LP64 `offsetof(BrUiCtl_, pOwner)` is not 0x2AE8.  This body
 * is written over br_ui.h's canonical control from the start, so the
 * displacement never appears in the C and the hazard does not exist here.
 * ========================================================================== */
int32_t BrUiCreditsAction_1003AED0(BrUiCtl_ *pCtl);

/* --- arithmetic that pins the constants (host-independent) --------------- */
#define BR_UICREDITS_ASSERT(name, cond) \
    typedef char BR_UICREDITS_##name[(cond) ? 1 : -1]

/* The two arms are adjacent selector values, and the credits arm is the one
 * taken when the flag is CLEAR.  If someone "tidies" the arms to match the
 * source order of the branch, this stops holding. */
BR_UICREDITS_ASSERT(outro_is_credits_plus_one,
                    BR_UICREDITS_MOVIE_OUTRO ==
                    BR_UICREDITS_MOVIE_CREDITS + 1);
/* The intro is the value the boot step writes, and it is below both. */
BR_UICREDITS_ASSERT(intro_is_lowest,
                    BR_UICREDITS_MOVIE_INTRO < BR_UICREDITS_MOVIE_CREDITS);

#endif /* BR_UICREDITS_H */
