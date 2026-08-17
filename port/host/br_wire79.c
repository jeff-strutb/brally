/* br_wire79.c -- PUBLISH THE ROOT MENU'S CONTEXT, g_brUiRoot.
 *
 * ==========================================================================
 * THE GAP THIS CLOSES
 * ==========================================================================
 *
 * port/src/menus/br_uiroot.c is a full transcription of Glide 0x100425E0 --
 * the root phase's pfnEnter, the function that builds the main menu.  Its very
 * first statement is
 *
 *     if (!BrUiRootCtxComplete(&g_brUiRoot)) return;
 *
 * and until now the only thing in the tree that ever filled g_brUiRoot was
 * port/tests/test_br_uiroot.c.  So in every shipped build of the host the main
 * menu builder returned on its first line and there was no main menu at all.
 * That refusal is br_uiroot.h's own PORT-ONLY rule -- "a caller that supplies
 * nothing must not receive a plausible main menu" -- and this file is the
 * caller that supplies something.
 *
 * Six slots have to be non-NULL for the builder to run: the hook table, an
 * error host, three style rectangles and the status-line text.
 *
 * ==========================================================================
 * WHERE EACH VALUE COMES FROM
 * ==========================================================================
 *
 * THE HOOKS come from br_wire78.c, which is a separate translation unit
 * because br_uiroot.h and slice3_31.h cannot share one -- slice1_06.h and
 * slice2_25.h define incompatible `BrDPlayVtbl` structs.  Its eight adapters
 * are declared below by name rather than reached through an ordinal table, so
 * that a slot cannot silently receive a neighbouring row's action; the other
 * two hooks already have control-typed bodies and are installed straight from
 * their own headers.  br_wire71.c documents the same declare-locally dodge for
 * the same class of clash.
 *
 * THE THREE STYLE RECTANGLES are slice3_39.c's g_aBrUiStyle entries 15, 3 and
 * 20, reached by ORIGINAL ADDRESS through BR_UI_STYLE so no fourth name is
 * coined for them.  br_uiroot.h derives the mapping and checks it twice: the
 * Glide pointers 0x100AACA8 / 0x100AABE8 / 0x100AACF8 were read independently
 * out of BRGlide.dll and match the D3D-derived table to the byte, and two of
 * the three then agree with geometry the builder computes without being told
 * to -- the title lands at y = 10 and entry 15's top is 10, the status line
 * lands at y = 29 and entry 20's top is 29.
 *
 * THE STATUS TEXT is the literal .data buffer at 0x100ACAD8 (D3D 0x100AD300),
 * which holds a single space in the shipped image.  It is NOT a string-table
 * id; br_uiroot.c passes it straight to the control's setText, and Glide
 * 0x1003AF30 later overwrites the same control's text through the index this
 * builder publishes at 0x10AC4C58.
 *
 * THE ERROR HOST is a platform seam, not a stand-in for an unported function.
 * 0x1003E260 calls a string lookup, USER32!MessageBoxA and a terminate helper;
 * slice1_06.h injects all three rather than hardcoding them, and this is where
 * the host puts them.  It is reached only when `operator new` fails, and error
 * index 4 is FATAL in g_aBrErrTable, so the exit is the original's behaviour
 * and not a choice made here.
 *
 * ==========================================================================
 * WHAT IS STILL A HOLE, MEASURED
 * ==========================================================================
 *
 * Two of the seven rows -- "Quick Race" and "Options" -- reach
 * slice2_26.c's activate routines, which construct a phase and call
 * slice8_86.c's BrPhaseEnterPlaceholder_1004B430 / _1004BDC0.  Those two are
 * one-line adapters onto slice3_33.c's real screen builders, and they are
 * inert because g_pBrUiBuildCtx86 is NULL.
 *
 * THIS FILE DELIBERATELY DOES NOT BIND g_pBrUiBuildCtx86, and the reason is
 * measured rather than argued.  slice8_86.c reaches the builders as
 *
 *     BrExt_1004B430(g_pBrUiBuildCtx86, (BrUiPhase *)pSelf)
 *
 * where pSelf is a `BrPhase_ *` -- slice2_26.c calls `p->pfnEnter(p)` on the
 * object it just constructed.  slice3_33.h's BrUiPhase is a SHIFTED model of
 * that same object: it begins at the original's +0x10 and has no pVtbl /
 * pfnEnter / pfnHook / f0C.  On this host the two disagree from byte zero,
 * printed by a program that includes both headers:
 *
 *     BrPhase_   nPages @ 28   aPages @ 32   aFlags @ 204   sizeof 304
 *     BrUiPhase  cScreen@  0   apScreen@  8   aF6C  @ 184   sizeof 272
 *
 * so `pPhase->cScreen = ...` would write the low two bytes of the phase's
 * VTABLE POINTER, and the next frame would dispatch through it.  The same
 * shift is one level down as well: BrUiScreen::cCtl is at 4 where
 * BrUiPage_::cCtl is at 36.  Binding the context would not make those two
 * screens build -- it would corrupt the object they build into, and the damage
 * would surface somewhere else entirely.
 *
 * That is CONVENTIONS.md's "Two models of one object, shifted" hazard, named
 * there and still live, and br_phase.h's REMAINING WORK paragraph names this
 * exact set of types.  Note the models also disagree about SHAPE, not just
 * origin -- BrUiPhase has 22 screen slots where br_phase.h adjudicated 20 --
 * so no pointer adjustment can reconcile them either.  The fix is to retype
 * slice3_33 over br_phase.h / br_ui.h, which is a port/src change and not host
 * wiring.  Until then those two rows reach a real activate routine, construct
 * a real phase, and land on an empty screen, and the harness says so.
 */

#include "br_uiroot.h"     /* g_brUiRoot, BrUiRootHooks, BrUiRootEnter_100425E0 */
#include "br_uicredits.h"  /* BrUiCreditsAction_1003AED0  -- Glide 0x1003AED0  */
#include "br_sprfont.h"    /* BrSprFontKindHook_10047360  -- D3D   0x10047360  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * The eight adapted hooks, from br_wire78.c.  Declared here rather than
 * included; see the banner.  Their one parameter type, BrUiCtl_, is br_ui.h's
 * and both files see that same definition, so these declarations cannot drift
 * from the definitions -- a mismatch would not link.
 * ========================================================================== */
extern void        BrHostWire78(void);
/* 0x100ACAD8 (D3D 0x100AD300), the one-space .data buffer.  ONE object, shared
 * with br_wire78.c's own users of the same address; br_uicredits.h asks for
 * exactly that. */
extern const char *BrHostStatusText(void);

extern int32_t BrHostRootAct_1003ED90(BrUiCtl_ *pCtl);   /* D3D 0x10045900 */
extern int32_t BrHostRootAct_1003D140(BrUiCtl_ *pCtl);   /* D3D 0x10043BF0 */
extern int32_t BrHostRootAct_1003E0E0(BrUiCtl_ *pCtl);   /* D3D 0x10044B90 */
extern int32_t BrHostRootAct_1003E4A0(BrUiCtl_ *pCtl);   /* D3D 0x10044F50 */
extern int32_t BrHostRootAct_1003E730(BrUiCtl_ *pCtl);   /* D3D 0x100451E0 */
extern int32_t BrHostRootAct_1003F610(BrUiCtl_ *pCtl);   /* D3D 0x10046170 */
extern int32_t BrHostRootTick_10040AF0(BrUiCtl_ *pCtl);  /* D3D 0x100474B0 */
extern int32_t BrHostRootTick_10040A20(BrUiCtl_ *pCtl);  /* D3D 0x100475F0 */

/* ==========================================================================
 * The error host.  0x1003E260's three injected platform calls.
 * ========================================================================== */

/* 0x10074030, the string table -- already ported in slice4_52.c under the name
 * br_uiroot.c itself uses.  Declared locally for the same reason as above. */
extern const char *BrStrGet(int id);

static const char *ErrLookup(void *pUser, uint32_t id)
{
    (void)pUser;
    return BrStrGet((int)id);
}

static void ErrMessage(void *pUser, const char *pszText,
                       const char *pszCaption, uint32_t uType)
{
    (void)pUser; (void)uType;
    fprintf(stderr, "br_wire79: %s: %s\n",
            pszCaption ? pszCaption : "(no caption)",
            pszText    ? pszText    : "(no text)");
}

/* The original's terminate helper.  Index 4 is FATAL, so this really is the
 * end of the program in the shipped build too -- it is not this file choosing
 * to give up. */
static void ErrExit(void *pUser, int code)
{
    (void)pUser;
    exit(code);
}

static const BrErrHost g_errHost = { ErrLookup, ErrMessage, ErrExit, NULL };

static BrUiRootHooks g_rootHooks;

void BrHostWireUiRoot(void);
void BrHostWireUiRoot(void)
{
    BrHostWire78();

    memset(&g_rootHooks, 0, sizeof g_rootHooks);

    /* control +0x08 -- the ACTION, one per row, in the page's order.  Each
     * member of BrUiRootHooks is named for the GLIDE address br_uiroot.c
     * stores, so the pairing below is checkable by name and not by position. */
    g_rootHooks.p1003ED90 = BrHostRootAct_1003ED90;   /* row 0 Championship */
    g_rootHooks.p1003D140 = BrHostRootAct_1003D140;   /* row 1 Multiplayer  */
    g_rootHooks.p1003E0E0 = BrHostRootAct_1003E0E0;   /* row 2 Time Attack  */
    g_rootHooks.p1003E4A0 = BrHostRootAct_1003E4A0;   /* row 3 Quick Race   */
    g_rootHooks.p1003E730 = BrHostRootAct_1003E730;   /* row 4 Options      */
    g_rootHooks.p1003AED0 = BrUiCreditsAction_1003AED0; /* row 5 Credits    */
    g_rootHooks.p1003F610 = BrHostRootAct_1003F610;   /* row 6 Quit         */

    /* control +0x0C -- the per-frame caption setter, NOT an action.
     * 0x100407B0 is the base and the other two wrap it. */
    g_rootHooks.p100407B0 = BrSprFontKindHook_10047360;  /* rows 0 and 3    */
    g_rootHooks.p10040AF0 = BrHostRootTick_10040AF0;     /* rows 1,2,4 and 6*/
    g_rootHooks.p10040A20 = BrHostRootTick_10040A20;     /* row 5           */

    g_brUiRoot.pHooks   = &g_rootHooks;
    g_brUiRoot.pErrHost = &g_errHost;

    /* g_aBrUiStyle entries 15, 3 and 20.  See the banner. */
    g_brUiRoot.pStyleTitle  = BR_UI_STYLE(0x100AB508);
    g_brUiRoot.pStyleRow    = BR_UI_STYLE(0x100AB448);
    g_brUiRoot.pStyleStatus = BR_UI_STYLE(0x100AB558);

    /* 0x100ACAD8 -- one space in the shipped image. */
    g_brUiRoot.pszStatus = BrHostStatusText();
}

/* Non-zero when 0x100425E0 will actually build rather than refuse on its first
 * line.  The host asks before installing the enter hook, so "the main menu did
 * not appear" can never be reported as a mystery. */
int BrHostUiRootReady(void);
int BrHostUiRootReady(void)
{
    return BrUiRootCtxComplete(&g_brUiRoot);
}
