/* br_uicredits.c -- RESPONSIBILITY: menus/ -- the front end: pages, controls,
 * navigation.  This module is Glide 0x1003AED0 (D3D 0x10041970), the MAIN
 * MENU'S "CREDITS" ROW ACTION: the +0x08 hook Glide 0x100425E0 installs at
 * 0x10042E96 into the control it captions with BRString id 7.
 *
 * See br_uicredits.h for the full listing with the ESP traced, the decoding
 * of the cinematic selector, and the audit that found four of the five
 * addresses this packet was sent after already transcribed under their D3D
 * numbers.
 *
 * REFERENCE BINARY: orig/BRGlide.dll.  config/shared.csv classes 0x1003AED0
 * `shared` against D3D 0x10041970 with the same 90-byte extent, and both
 * listings were disassembled and compared instruction by instruction for this
 * module -- they differ only in the four global addresses and the two call
 * targets, so the builds cannot disagree about this body.
 *
 * THERE IS NO FLOATING POINT HERE, so CONVENTIONS.md's x87 rules -- operand
 * order, unordered compares -- have nothing to bite on.  The one comparison
 * is an integer `cmp eax, edx` against a register holding zero.
 *
 * THE ONE ORDERING GOTCHA, and it is a scheduling artefact rather than
 * behaviour: `add esp, 4` (the cdecl cleanup of the status setter's argument)
 * sits at 0x1003AEE1, AFTER the load of 0x10AC5D98 and the `xor edx, edx`.
 * The compiler hoisted two independent instructions into the gap.  The order
 * below is the listing's; nothing observable depends on it, and it is written
 * that way so a reader can follow the two side by side.
 */

#include <stddef.h>

#include "br_uicredits.h"

/* XSLICE 0x1003AF30 == D3D 0x100419D0 -- SetStatusText: re-caption whichever
 * control the root page's status-line index names.  ALREADY PORTED in
 * slice5_62.c as BrExt_100419D0, and it is CALLED, not re-transcribed.
 *
 * Declared here rather than including slice5_62.h, for br_uiroot.c's reason:
 * that header drags br_mat.h / br_pool.h / slice1_09.h in behind it for
 * unrelated packets, and this module needs one symbol.  The signature is
 * slice5_62.h's, unchanged -- `void (void *)`, one cdecl argument. */
extern void BrExt_100419D0(void *p);

BrUiCreditsCtx g_brUiCredits;

int BrUiCreditsCtxComplete(const BrUiCreditsCtx *pCtx)
{
    return pCtx != NULL
        && pCtx->pszStatus  != NULL
        && pCtx->pnGameMode != NULL
        && pCtx->pnMovieSel != NULL
        && pCtx->pnOutroFlag != NULL
        && pCtx->pnAA289C   != NULL;
}

/* ==========================================================================
 * 0x1003AED0
 * ========================================================================== */
/* @implements 0x1003AED0 glide BrUiCreditsAction_1003AED0 */
int32_t BrUiCreditsAction_1003AED0(BrUiCtl_ *pCtl)
{
    BrUiCreditsCtx *pCtx = &g_brUiCredits;
    BrPhase_       *pOwner;

    /* PORT-ONLY refusal, with no counterpart in the original.  br_uiroot.c
     * sets the precedent and states the reason: a caller that supplies
     * nothing must not receive a plausible menu action.  The original's
     * return value is 0 on its only exit, so refusing is invisible in the
     * result and is visible only as "nothing happened" -- which is the
     * honest frontier behaviour, not a stand-in. */
    if (!BrUiCreditsCtxComplete(pCtx)) {
        return 0;
    }

    /* 0x1003AED0/D5 -- blank the status line.  The pushed pointer is
     * 0x100ACAD8, the .data buffer holding a single space. */
    BrExt_100419D0((void *)(size_t)pCtx->pszStatus);

    /* 0x1003AEDA -- the flag is read BEFORE the mode is written.  Both are
     * globals and neither writer reads the other, so this is order for
     * fidelity rather than for behaviour. */
    {
        const int32_t nOutro = *pCtx->pnOutroFlag;

        /* 0x1003AEE6 -- unconditional, on both arms of the branch below. */
        *pCtx->pnGameMode = BR_UICREDITS_GAME_MODE;

        /* 0x1003AEF0.  The listing's `je` takes the CREDITS arm, so the
         * source order of the two blocks in the image is outro-then-credits;
         * spelled the other way round here because `== 0` reads better than
         * `!= 0` with an inverted body, and the arms are what the suite
         * discriminates.  Nothing is reordered: each arm's stores are its
         * own. */
        if (nOutro == 0) {
            /* 0x1003AF04 -- and NOTHING else.  0x10AC5BF4 is left alone on
             * this arm; a transcription that clears it on both is the
             * mutation the suite catches. */
            *pCtx->pnMovieSel = BR_UICREDITS_MOVIE_CREDITS;
        } else {
            /* 0x1003AEF2 then 0x1003AEFC, in that order. */
            *pCtx->pnMovieSel = BR_UICREDITS_MOVIE_OUTRO;
            *pCtx->pnAA289C   = 0;
        }
    }

    /* 0x1003AF0E -- the ONE argument, and it is the control.  See the ESP
     * trace in the header: esp is back at E by this point, so [esp+4] is
     * argument 0 and not the return address.
     *
     * 0x1003AF13 / 0x1003AF1C -- the original reads pCtl->pOwner TWICE, once
     * for the store and once for the dispatch, with nothing in between that
     * could change it.  Read once here; the re-read is recorded rather than
     * reproduced because it has no observable effect and reproducing it would
     * suggest it did.
     *
     * NO NULL GUARD, deliberately.  The original dereferences +0x2AE8 and
     * the vtable unconditionally, and CONVENTIONS.md's accuracy-first rule
     * says a fault here is the original's fault, not something to paper
     * over.  A control reaches this hook only after 0x10047FB0 has placed it,
     * and placing is what sets pOwner. */
    pOwner = pCtl->pOwner;

    /* 0x1003AF19 -- +0x68 is the flag br_phase.h records as "set 1 on
     * just-built"; slice3_32.c's BrPhaseRun_100489A0 bails out of the whole
     * phase when it is zero.  Clearing it is the teardown. */
    pOwner->f68 = 0;

    /* 0x1003AF24 -- vtable +0x18, the slot br_phase.h pins to 0x10048B20 and
     * port/host/brally.c calls "shutdown".  The pushed argument is the same
     * zero edx has held since 0x1003AEDF. */
    pOwner->pVtbl->f18(pOwner, NULL);

    /* 0x1003AF27 -- and this 0 STOPS the page frame; see the header. */
    return 0;
}
