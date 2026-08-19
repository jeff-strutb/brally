/* br_uipages.c -- RESPONSIBILITY: menus/ -- the front end: pages, controls,
 * navigation.  Two of the root menu's destination screens: the multiplayer
 * name screen (Glide 0x1004F290, D3D 0x100563E0) and the quit confirmation
 * (Glide 0x10043050, D3D 0x10049C20).
 *
 * See br_uipages.h for the derivation, the two control tables, the float
 * constants, the traced stack frame and the frontier list.  Nothing is
 * repeated here that is stated there.
 *
 * REFERENCE BINARY: orig/BRGlide.dll, checked a second way against
 * orig/BRD3D.dll -- both functions are classed `shared`, matched by body,
 * with identical extents.
 */

#include <stddef.h>
#include <string.h>

#include "br_uipages.h"

/* XSLICE Glide 0x100418C0 == D3D 0x10048470 -- __thiscall page constructor;
 * returns `this`.  Declared rather than included, exactly as br_uiroot.c and
 * slice6_71.h do it: the two headers that own the name (slice3_32.h and
 * slice6_73.h) type it over page models that are not br_ui.h's, and this
 * module uses br_ui.h's. */
extern BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis);

/* XSLICE Glide 0x10040B10 == D3D 0x100476C0 -- __thiscall control
 * constructor.  port/src/menus/br_uictl.c implements it as BrUiCtlCtor. */
extern BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis);

/* XSLICE Glide 0x1006D280 == D3D 0x10074030 -- the string table, ported in
 * slice4_52.c.  Ids outside 1..0x12E return NULL.  The two captions that go
 * straight to the control vtable are not checked, exactly as every other
 * builder in the corpus does not check; the ONE that is dereferenced inside
 * this module is BrStrGet(0xC0), and see the note at its call site. */
extern const char *BrStrGet(int id);

/* XSLICE Glide 0x10B71648 == D3D 0x10B4E2E8 -- the saved multiplayer player
 * name.  slice4_50.c owns the pointer; this module reads and writes THROUGH
 * it so that one original address stays one host object.  br_uipages.h
 * FRONTIER 3. */
extern char *g_brPB4E2E8;

BrUiPagesCtx g_brUiPages;

int BrUiPagesCtxComplete(const BrUiPagesCtx *pCtx)
{
    return pCtx != NULL
        && pCtx->pHooks      != NULL
        && pCtx->pErrHost    != NULL
        && pCtx->pStyleTitle != NULL
        && pCtx->pStyleRow   != NULL;
}

int BrUiMultiCtxComplete(const BrUiPagesCtx *pCtx)
{
    return BrUiPagesCtxComplete(pCtx)
        && pCtx->ppCtlContinue != NULL
        && g_brPB4E2E8         != NULL;
}

/* ==========================================================================
 * The page prologue, identical in both builders except for fX
 *
 * The original's ORDER is the only thing in here that is not obvious: the
 * page-count word is read BEFORE the allocation (for aFlags) and RE-READ
 * after it (for aPages); the aPages store happens BEFORE the null test; the
 * error report happens BEFORE the count is bumped; and the count is bumped
 * even when the allocation failed.
 *
 * DEVIATION (memory safety, two places): the two array writes are bounded.
 * The original has the same implicit bounds -- br_phase.h's aPages ends
 * exactly where aFlags begins -- and does not check them.
 *
 * DEVIATION (memory safety): on failure the original reports error index 4
 * and then dereferences NULL at `mov [esi+0x340], ebp`.  Index 4 is FATAL in
 * slice1_06.c's g_aBrErrTable, so BrErrShow does not return there in the
 * shipped build; the port returns instead of faulting.  Same choice, same
 * reason, as br_uiroot.c's BrUiRootPageNew and slice6_71.c's Br71PageNew.
 * ========================================================================== */
static BrUiPage_ *BrUiPagesPageNew(BrPhase_ *pPhase, float fX)
{
    BrUiPage_ *pScr;
    uint16_t   i;

    i = pPhase->nPages;                       /* 1004F2B0 / 1004306E */
    pPhase->iPage = 0;                        /* 1004F2BB / 1004307A */
    if (i < BR_PHASE_PAGES) {
        pPhase->aFlags[i] = 1;                /* 1004F2C4 / 10043083, edi == 1 */
    }

    pScr = (BrUiPage_ *)BrOperatorNew(BR_UI_PAGE_ALLOC_SIZE);  /* push 0x348 */
    pScr = (pScr != NULL) ? BrUiPageCtor_10048470(pScr) : NULL;

    i = pPhase->nPages;                       /* 1004F2ED -- a RE-READ */
    if (i < BR_PHASE_PAGES) {
        pPhase->aPages[i] = pScr;             /* 1004F300, before the test */
    }
    if (pScr == NULL) {
        BrErrShow(g_brUiPages.pErrHost, BR_UIPAGES_ERR_ALLOC);   /* 1004F308 */
    }
    pPhase->nPages++;                         /* 1004F310, on both paths */

    if (pScr == NULL) {
        return NULL;                          /* DEVIATION: see above */
    }

    pScr->pOwner = pPhase;                    /* 1004F319 */
    pScr->f10    = 0;                         /* 1004F31F */
    pScr->fX     = fX;                        /* 1004F322 */
    pScr->fY     = BR_UIPAGES_PAGE_Y;         /* 1004F32C  0x43020000 */
    return pScr;
}

/* The control prologue.  Same two DEVIATIONs: the store into apCtl[cCtl]
 * happens BEFORE the null test, and cCtl is NOT bumped here -- each control's
 * block bumps it at the point the original does, so a failed allocation
 * leaves a NULL in the array and still moves the cursor on. */
static BrUiCtl_ *BrUiPagesCtlNew(BrUiPage_ *pScr)
{
    BrUiCtl_ *pCtl;

    pCtl = (BrUiCtl_ *)BrOperatorNew(BR_UI_CTL_ALLOC_SIZE);   /* push 0x1E214 */
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;

    if (pScr->cCtl < BR_UI_PAGE_CTL_MAX) {
        pScr->apCtl[pScr->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        BrErrShow(g_brUiPages.pErrHost, BR_UIPAGES_ERR_ALLOC);
    }
    return pCtl;
}

/* Relies on the local names pScr / pCtl, which both builders declare. */
#define BR_UIPAGES_NEW_CTL()                            \
    do {                                                \
        pCtl = BrUiPagesCtlNew(pScr);                   \
        if (pCtl == NULL) { return; }                   \
    } while (0)

/* One menu row.  Four copies across the two builders, differing only in y,
 * caption id and the action hook.  The order inside is the original's and is
 * checkable: place, then +0x0C BEFORE +0x08, then w1E20C, then the caption.
 * cCtl and cSel are bumped by the caller, because the multiplayer builder
 * publishes a global between the caption and the two bumps. */
static void BrUiPagesRow(BrUiCtl_ *pCtl, BrUiPage_ *pScr, BrPhase_ *pPhase,
                         float y, BrUiCtlHookFn_ pfn08, int idStr)
{
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, y, 0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = g_brUiPages.pHooks->p100407B0;
    pCtl->pfn08  = pfn08;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(idStr), 1, 1, g_brUiPages.pStyleRow);
}

/* ==========================================================================
 * Glide 0x10043050 -- the quit confirmation
 * ========================================================================== */

/* WHAT IT DOES: puts up the "are you sure you want to leave the game?"
 * screen -- the heading "Quit Game" over two choices, "Yes, Quit" and
 * "Back" -- so that quitting always takes a second press rather than one. */
/* @implements 0x10043050 glide BrUiQuitEnter_10043050 */
void BrUiQuitEnter_10043050(BrPhase_ *pPhase)
{
    const BrUiPagesHooks *pH;
    BrUiPage_            *pScr;
    BrUiCtl_             *pCtl;

    /* PORT-ONLY refusal, with no counterpart in the original.  br_boot.h's
     * BrRallyMainOps and br_uiroot.c set the precedent: a caller that
     * supplies nothing must not receive a plausible screen. */
    if (!BrUiPagesCtxComplete(&g_brUiPages)) {
        return;
    }
    pH = g_brUiPages.pHooks;

    pScr = BrUiPagesPageNew(pPhase, BR_UIQUIT_PAGE_X);
    if (pScr == NULL) {
        return;
    }

    /* --- apCtl[0], 1004313D: the backdrop.  a6 = 0, a7 = 0. ------------- */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;                                        /* 1004314F */

    /* --- apCtl[1], 100431A4: the title, "Quit Game" --------------------- */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;                                    /* 100431D0 */
    pCtl->pVtbl->f34(pCtl, BrStrGet(BR_UISTR_QUIT_TITLE), 1, 1,
                     g_brUiPages.pStyleTitle);
    pScr->cCtl++;                                        /* 100431E7 */

    /* --- apCtl[2], 1004323C: "Yes, Quit", y == fY ----------------------- */
    BR_UIPAGES_NEW_CTL();
    BrUiPagesRow(pCtl, pScr, pPhase, pScr->fY,
                 pH->p1003CC60, BR_UISTR_QUIT_YES);
    pScr->cCtl++;                                        /* 1004328F */
    pScr->cSel++;                                        /* 10043293 */

    /* --- apCtl[3], 100432EB: "Back", y == fY - (-114) ------------------- */
    BR_UIPAGES_NEW_CTL();
    BrUiPagesRow(pCtl, pScr, pPhase, pScr->fY - (-114.0f),
                 pH->p1003F940, BR_UISTR_BACK);
    pScr->cCtl++;                                        /* 10043347 */
    pScr->cSel++;                                        /* 1004334B */

    /* The sole exit sets eax = 1 and every caller discards it. */
}

/* ==========================================================================
 * Glide 0x1004F290 -- the multiplayer name screen
 * ========================================================================== */

/* WHAT IT DOES: puts up the screen where a player types the name they will be
 * known by in a network game -- a heading, the words "Player Name" in a
 * different colour from the rest of the page, a box already filled in with
 * whatever name was saved last time (or the word "Player" if there is not one
 * yet), and "Continue" and "Back" underneath. */
/* @implements 0x1004F290 glide BrUiMultiEnter_1004F290 */
void BrUiMultiEnter_1004F290(BrPhase_ *pPhase)
{
    const BrUiPagesHooks *pH;
    BrUiPage_            *pScr;
    BrUiCtl_             *pCtl;

    /* PORT-ONLY refusal.  BrUiMultiCtxComplete additionally requires the
     * published-control slot and slice4_50.c's name-buffer pointer -- see
     * FRONTIER 1 and 3 in br_uipages.h.  Refusing is deliberate: this screen
     * publishes a control that its own "Back" action reads back, so a screen
     * built without that store would leave the caller acting on a stale value
     * as though the screen had opened. */
    if (!BrUiMultiCtxComplete(&g_brUiPages)) {
        return;
    }
    pH = g_brUiPages.pHooks;

    pScr = BrUiPagesPageNew(pPhase, BR_UIMULTI_PAGE_X);
    if (pScr == NULL) {
        return;
    }

    /* --- apCtl[0], 1004F3CE: the backdrop ------------------------------- */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;                                        /* 1004F3E0 */

    /* --- apCtl[1], 1004F3E5: the title, "Multi-Player Name" ------------- */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;                                    /* 1004F411 */
    pCtl->pVtbl->f34(pCtl, BrStrGet(BR_UISTR_MULTI_TITLE), 1, 1,
                     g_brUiPages.pStyleTitle);
    pScr->cCtl++;                                        /* 1004F428 */

    /* --- apCtl[2], 1004F47D: the label, "Player Name" -------------------
     * NOT a menu row: flags 0x100009 has neither the ACTIVATE bit nor
     * anything selectable, and cSel is not bumped.  Two things here differ
     * from every other caption in the corpus and both are the original's:
     * w1E20C is 0x34 (a different font SHEET, i.e. a different colour --
     * CONVENTIONS.md) and the vtable's a3 is 4 rather than 1. */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 0x34;                                 /* 1004F4AB */
    pCtl->pVtbl->f34(pCtl, BrStrGet(BR_UISTR_PLAYER_NAME), 1, 4,
                     g_brUiPages.pStyleRow);
    pScr->cCtl++;                                        /* 1004F4C2 */

    /* --- apCtl[3], 1004F517: a decoration at (156, 172), a7 = 0x39 ------ */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 156.0f, 172.0f, 9, 2, 5, 0, 0x39);
    pScr->cCtl++;                                        /* 1004F533 */

    /* --- apCtl[4], 1004F588: THE NAME EDIT BOX -------------------------- */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 174.0f, 0x200001, 2, 5, 1, -1);
    /* Stored in the original's order: +0x08, then +0x04, then +0x10. */
    pCtl->pfn08  = pH->p1003BFF0;                        /* 1004F5B9 */
    pCtl->pfn04  = pH->p10038420;                        /* 1004F5C0 */
    pCtl->pfn10  = pH->p10038490;                        /* 1004F5C7 */
    pCtl->w1E20C = 3;                                    /* 1004F5CE */
    /* The text is the shared edit buffer itself, not a string-table id.
     * slice2_25.c owns its storage; slice3_39.h declares it. */
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 1, g_brUiPages.pStyleRow);

    /* 1004F5DA..1004F61B -- seed the saved name when it is empty or a single
     * character.  The original's `cmp ecx,1 / ja` is an UNSIGNED test on the
     * strlen, so <= 1 takes the copy.
     *
     * BrStrGet(0xC0) is dereferenced here rather than merely passed on, so a
     * string table that did not load would fault -- which is exactly what the
     * original does, and 0xC0 is inside the valid 1..0x12E range. */
    if (strlen(g_brPB4E2E8) <= 1) {
        strcpy(g_brPB4E2E8, BrStrGet(BR_UISTR_DEFAULT_NAME));
    }

    /* 1004F61D..1004F651 -- put it in the box and re-measure through the text
     * object's own vtable.  The original does not null-check either. */
    strcpy(pCtl->aText[0].sz, g_brPB4E2E8);
    pCtl->aText[0].pVtbl->pfn04(&pCtl->aText[0]);

    /* 1004F654..1004F68B -- one rectangle, written to the control and to
     * aText[0].  br_ui.h ADJ-2: +0x2F80.. are aText[0]'s fields. */
    pCtl->rcLeft   = BR_UIMULTI_EDIT_LEFT;
    pCtl->aText[0].left  = BR_UIMULTI_EDIT_LEFT;
    pCtl->rcRight  = BR_UIMULTI_EDIT_RIGHT;
    pCtl->aText[0].right = BR_UIMULTI_EDIT_RIGHT;
    pCtl->rcTop    = BR_UIMULTI_EDIT_TOP;
    pCtl->aText[0].f428  = BR_UIMULTI_EDIT_TOP;
    pCtl->rcBottom = BR_UIMULTI_EDIT_BOTTOM;
    pCtl->aText[0].f430  = BR_UIMULTI_EDIT_BOTTOM;

    /* 1004F691 -- the difference is taken in SIXTEEN bits and only the low
     * word is stored.  Transcribed as written rather than folded to 0x72. */
    pCtl->aText[0].f41C =
        (int16_t)((int32_t)(uint16_t)((uint16_t)pCtl->aText[0].right
                                      - (uint16_t)pCtl->aText[0].left) - 0x10);
    pScr->cCtl++;                                        /* 1004F6A9 */
    pScr->cSel++;                                        /* 1004F6AD */

    /* --- apCtl[5], 1004F700: "Continue", y == fY - (-95) ---------------- */
    BR_UIPAGES_NEW_CTL();
    BrUiPagesRow(pCtl, pScr, pPhase, pScr->fY - (-95.0f),
                 pH->p1003D220, BR_UISTR_CONTINUE);
    /* 1004F760 -- published BEFORE either counter is bumped, so the global
     * names THIS control.  FRONTIER 1: the slot belongs to whoever owns
     * Glide 0x10AC5D00 / D3D 0x10AA29A8, not to this module. */
    *g_brUiPages.ppCtlContinue = pCtl;
    pScr->cCtl++;                                        /* 1004F766 */
    pScr->cSel++;                                        /* 1004F76A */

    /* --- apCtl[6], 1004F7C2: "Back", y == fY - (-114) ------------------- */
    BR_UIPAGES_NEW_CTL();
    BrUiPagesRow(pCtl, pScr, pPhase, pScr->fY - (-114.0f),
                 pH->p1003D510, BR_UISTR_BACK);
    pScr->cCtl++;                                        /* 1004F81E */
    pScr->cSel++;                                        /* 1004F822 */

    /* --- apCtl[7], 1004F87A: a decoration at (80, 46), a7 = 7 ----------- */
    BR_UIPAGES_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 7);
    pScr->cCtl++;                                        /* 1004F896 */

    /* The sole exit sets eax = 1 and every caller discards it. */
}
