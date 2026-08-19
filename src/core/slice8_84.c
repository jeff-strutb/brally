/* slice8_84.c -- the control hooks the slice6_71 and slice6_72 screen
 * builders install.  See slice8_84.h for what this module is, which pairing
 * each hook was read out of, the five conflicts it reports and the list of
 * slots it deliberately leaves NULL.
 *
 * The file has three kinds of function and nothing else:
 *
 *   ADAPTER      a one-line forward to an existing, verified body.  Six of
 *                them, and every one is over a body whose stack argument the
 *                original PROVABLY never reads -- see the disassembly evidence
 *                in the header.  No second opinion is formed here; if one of
 *                those bodies is wrong it is slice2_25.c that is wrong.
 *   TRANSCRIPTION  a control-typed body for an address whose only existing
 *                port is over a byte-image model that cannot take a
 *                `BrUiCtl_ *` on LP64.  Same situation, same remedy and the
 *                same file-level shape as port/src/slice7_81.c.
 *   INSTALLER    the two table fills at the bottom.
 *
 * Transcribed from orig/BRD3D.dll (these are D3D addresses) and cross-checked
 * against orig/BRGlide.dll.
 */
#include "slice8_84.h"

#include <stddef.h>
#include <string.h>

BrUiHook84Ctx g_brHook84;

/* ==========================================================================
 * Cross-slice declarations
 *
 * slice2_25.h CANNOT be included here: it declares BrOptObjCtor over its
 * five-field partial view of the phase while slice6_73.h -- which this module
 * needs -- declares the same symbol over the canonical BrPhase_.  That is
 * slice6_73.h's CONFLICT 1 and the two headers are deliberately never
 * combined.  So the six entry points are declared by hand, each copied
 * VERBATIM from slice2_25.h so that a future diff of the two is a diff.
 * This is exactly what port/src/slice7_80.c does, for the same reason.
 * ========================================================================== */

/* XSLICE port/include/slice2_25.h:573, 587, 589, 642 -- the four cyclers. */
extern int BrOptCycleTrack(void);     /* 0x10042EE0 */
extern int BrOptCycleBD3E0(void);     /* 0x100430B0 */
extern int BrOptCycleAA2A00(void);    /* 0x10043180 */
extern int BrOptCycleAA2A18(void);    /* 0x10044600 */

/* XSLICE port/include/slice2_25.h:640, 643 -- the two openers.  The tag is
 * left INCOMPLETE on purpose: this file must not form an opinion about the
 * shape of slice2_25.h's byte-image model, and it never dereferences one.
 * `pUnused` is slice2_25.h's own parameter name and the disassembly agrees --
 * both [esp+4] reads in each body are the SEH frame unlink.  NULL is passed,
 * which is precedent: port/src/slice6_72.c:502 already calls the sibling
 * BrOptOpen294C(NULL) for the same reason. */
struct BrGameObj;
extern int BrOptOpen2950B(struct BrGameObj *pUnused);   /* 0x100443E0 */
extern int BrOptOpen2954 (struct BrGameObj *pUnused);   /* 0x100446D0 */

/* XSLICE 0x10041BD0 -- NOT PORTED.  0x100450C0 opens with a bare call to it
 * and the host harness satisfies it from port/host/br_stubs.c.  Declared with
 * the shape the ORIGINAL calls it with (no arguments, result discarded)
 * rather than with the stub's `long (void)`, exactly as br_stubs.c's banner
 * intends and as slice7_81.c declares 0x100509F0. */
extern void BrExt_10041BD0(void);

/* ==========================================================================
 * Shared shapes
 * ========================================================================== */

/* The prologue eleven of the thirteen LEAVE routines share:
 *
 *     mov ecx,[eax+0x2AE8] / mov edx,[ecx] / call [edx+0x1C]
 *
 * +0x2AE8 is the control's OWNING PHASE (br_ui.h `pOwner`) and phase vtable
 * slot +0x1C is 0x10048AA0, "release every page".  The owner is dereferenced
 * with no guard, as in the original; only the current phase is NULL-tested,
 * and that test is the original's own.
 *
 * Byte-for-byte the same helper slice7_81.c calls Br81LeavePrologue.  It is
 * repeated rather than shared because slice7_81.c's is static and this module
 * must not acquire a link dependency on another module's internals for four
 * instructions. */
static void Br84LeavePrologue(BrUiCtl_ *pCtl)
{
    BrPhase_ *pOwner = pCtl->pOwner;
    BrPhase_ *pCur;

    pOwner->pVtbl->f1C(pOwner);

    /* The CURRENT phase is read here, AFTER +0x1C has run. */
    pCur = g_pBrUiNav->pAA2904;
    if (pCur != NULL)
        (void)pCur->pVtbl->f00(pCur, 1);
}

/* One bounded copy out of 0x1039B720 into one scratch buffer.
 *
 * DEVIATION (memory safety): the original's copies are unbounded inline
 * `rep movs` of strlen+1 bytes into fixed .data buffers.  Here they are
 * bounded by g_br73.cbScratch and always NUL-terminated -- the same deviation
 * slice7_81.c and slice3_31.c both take.  The source lives in the
 * zero-initialised tail of .data, so it is the empty string until something
 * else writes it. */
static void Br84CopyName(char *pszDst)
{
    size_t cb = g_br73.cbScratch;

    if (pszDst == NULL || cb == 0)
        return;
    strncpy(pszDst, g_aBr39B720, cb - 1);
    pszDst[cb - 1] = '\0';
}

/* The FULL name reset 0x10046870 performs.  Identical to slice7_81.c's
 * Br81NameReset, which 0x10046B10 and 0x10046EB0 share; 0x10046E10 does a
 * strictly smaller subset and does NOT call this. */
static void Br84NameResetFull(void)
{
    g_brHook81.pAA2928  = NULL;          /* 0x10AA2928 */
    g_pBrUiNav->pAA29C0 = NULL;          /* 0x10AA29C0 -- a CONTROL           */
    g_br73.pAA29CC      = NULL;          /* 0x10AA29CC -- slice7_81.h CONFL 3 */
    g_brHook81.nAA28E4  = 0;             /* 0x10AA28E4 */
    g_br0AB3F4          = -1;            /* 0x100AB3F4 */

    Br84CopyName(g_br73.szAA2518);
    Br84CopyName(g_br73.szA9D618);
}

void BrUiHook84Reset(void)
{
    memset(&g_brHook84, 0, sizeof(g_brHook84));
}

/* ==========================================================================
 * ADAPTERS -- six forwards, no decompilation
 * ========================================================================== */

int32_t BrUiHook84Opt_10042EE0(BrUiCtl_ *pCtl)
{
    (void)pCtl;                     /* the original never reads it */
    return (int32_t)BrOptCycleTrack();
}

int32_t BrUiHook84Opt_10043180(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleAA2A00();
}

int32_t BrUiHook84Opt_100430B0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleBD3E0();
}

int32_t BrUiHook84Opt_10044600(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleAA2A18();
}

int32_t BrUiHook84Opt_100443E0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptOpen2950B(NULL);
}

int32_t BrUiHook84Opt_100446D0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptOpen2954(NULL);
}

/* ==========================================================================
 * LEAVE
 * ========================================================================== */

int32_t BrUiHook84_10046F60(BrUiCtl_ *pCtl)
{
    BrPhase_ *pSaved;

    Br84LeavePrologue(pCtl);

    pSaved = g_brHook81.pAA292C;        /* read BEFORE the two clears */
    /* GOTCHA: the CURRENT phase is cleared here, so the notify below runs
     * with 0x10AA2904 == NULL.  It is set again at the bottom. */
    g_pBrUiNav->pAA2904 = NULL;
    g_brHook81.pAA2974  = NULL;
    if (pSaved != NULL) {
        (void)pSaved->pVtbl->f00(pSaved, 1);
        g_brHook81.pAA292C = NULL;      /* only on the non-NULL arm */
    }
    g_pBrUiNav->pAA2904 = g_pBrUiNav->pAA2908;
    return 0;
}

int32_t BrUiHook84_10046FC0(BrUiCtl_ *pCtl)
{
    /* Thirteen bytes, and not one of them touches the argument.  See the
     * header's CONFLICTS 1 and 2 for why this is not slice3_31.c's
     * `void BrPhaseGoto_10046FC0(void)`. */
    (void)pCtl;
    g_pBrUiNav->pAA2904 = g_brHook81.pAA292C;
    return 0;
}

int32_t BrUiHook84_10046E10(BrUiCtl_ *pCtl)
{
    Br84LeavePrologue(pCtl);

    g_pBrUiNav->pAA2924 = NULL;         /* 0x10AA2924 -- br_uinav.h owns it  */
    g_brHook84.nAA28E0  = 0;            /* 0x10AA28E0 */
    g_br0AB3F4          = -1;           /* 0x100AB3F4 */
    Br84CopyName(g_br73.szAA2518);
    Br84CopyName(g_br73.szA9D618);

    /* The destination is read AFTER the copies, as in 0x10046870.  Nothing
     * above writes it, so the order is unobservable; reproduced anyway. */
    g_pBrUiNav->pAA2904 = g_brHook81.pAA291C;
    return 0;
}

/* WHAT IT DOES: backs out of the screen the player is on. It closes that
 * screen's pages, forgets the rows and screens it had published for other code
 * to find, and makes the screen remembered as the one to return to current.
 * This one is not wired in when a screen is built -- it is poked onto a Back
 * row at the moment that screen is opened. */
/* @implements 0x10046DC0 d3d BrUiHook84_10046DC0 */
int32_t BrUiHook84_10046DC0(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br84LeavePrologue(pCtl);

    pNext = g_pBrUiNav->pAA2924;        /* read BEFORE the clears */
    g_brHook81.pAA292C = NULL;
    g_brS71.pAA29B0    = NULL;          /* 0x10AA29B0 -- a CONTROL */
    g_brHook81.pAA2974 = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* WHAT IT DOES: the Back row of the game-options screen -- the one with skid
 * marks, specular lighting and car shadow on it. Picking it closes the options
 * pages and returns the player to the menu that opened them. */
/* @implements 0x10046710 d3d BrUiHook84_10046710 */
int32_t BrUiHook84_10046710(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br84LeavePrologue(pCtl);
    pNext = g_brHook81.pAA2918;         /* read BEFORE the clear */
    g_brHook84.pAA2988 = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* WHAT IT DOES: the Back row of the season-progress screen. It closes that
 * screen down and returns the player to the menu they came in from. */
/* @implements 0x10047060 d3d BrUiHook84_10047060 */
int32_t BrUiHook84_10047060(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br84LeavePrologue(pCtl);
    pNext = g_brHook81.pAA292C;         /* read BEFORE the clear */
    g_brHook84.pAA2930 = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* WHAT IT DOES: backs out of the options screen reached from the season
 * progress buttons. It is wired onto that screen's Back row at the moment the
 * screen is opened, and picking it closes the screen and returns the player to
 * the one that opened it. */
/* @implements 0x10046830 d3d BrUiHook84_10046830 */
int32_t BrUiHook84_10046830(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br84LeavePrologue(pCtl);
    pNext = g_brHook84.pAA2930;         /* read BEFORE the clear */
    g_brHook81.pAA2918 = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* WHAT IT DOES: the same kind of back-out as its neighbours, but it also
 * throws away whatever name was being edited -- both working copies of the name
 * are reset from the shared scratch text and the remembered selection is
 * cleared -- so leaving through this row abandons an edit rather than keeping
 * it. */
/* @implements 0x10046870 d3d BrUiHook84_10046870 */
int32_t BrUiHook84_10046870(BrUiCtl_ *pCtl)
{
    Br84LeavePrologue(pCtl);
    Br84NameResetFull();
    /* GOTCHA: this one reads its destination AFTER the clears and the copies
     * (0x100468FF), where the plain leaves read theirs first. */
    g_pBrUiNav->pAA2904 = g_brHook84.pAA2930;
    return 0;
}

int32_t BrUiHook84_10043FA0(BrUiCtl_ *pCtl)
{
    BrPhase_ *pOwner = pCtl->pOwner;

    /* `push 1 / mov ecx,[eax+0x2AE8] / mov edx,[ecx] / call [edx+0x18]`.
     * Slot +0x18, not +0x1C, and there is no current-phase notify at all.
     * The literal 1 has to be cast because br_phase.h types the argument
     * `void *` -- see the header's CONFLICT 4.  This is the conflict, not a
     * resolution of it. */
    pOwner->pVtbl->f18(pOwner, (void *)(size_t)1u);

    g_pBrUiNav->pAA2904 = g_pBrUiNav->pAA2908;
    return 0;
}

int32_t BrUiHook84_10043F50(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    /* 0x10AA287C is set BEFORE the prologue, and it is the same word
     * slice6_71.h models as g_brS71.nAA287C -- the one 0x1003BF60 tests for
     * 2 or 3 to decide whether to skip the entity reset.  Reached there
     * rather than duplicated. */
    g_brS71.nAA287C = 2;

    Br84LeavePrologue(pCtl);
    pNext = g_brHook84.pAA2948;         /* read BEFORE the clear */
    g_brHook84.pAA298C = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

int32_t BrUiHook84_10044B40(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br84LeavePrologue(pCtl);
    pNext = g_brHook81.pAA2940;         /* read BEFORE the clears */
    g_brHook84.pAA298C = NULL;
    if (g_pBr72Env != NULL)             /* DEVIATION: guarded, see below */
        g_pBr72Env->pAA29E8 = NULL;     /* 0x10AA29E8 -- a CONTROL */
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

int32_t BrUiHook84_10044C70(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br84LeavePrologue(pCtl);
    /* 0x10AA2908 -- the ROOT phase, not a per-screen singleton. */
    pNext = g_pBrUiNav->pAA2908;
    g_brHook84.pAA295C = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

int32_t BrUiHook84_10044F00(BrUiCtl_ *pCtl)
{
    BrPhase_ *pOwner = pCtl->pOwner;
    BrPhase_ *pNotify;
    BrPhase_ *pNext;

    /* Half the shared prologue only: the release runs, but the object
     * notified through +0x00 is 0x10AA2968 and NOT the current phase.  The
     * only routine in the family that does this, so it is written out rather
     * than reached through Br84LeavePrologue. */
    pOwner->pVtbl->f1C(pOwner);

    pNotify = g_brHook84.pAA2968;
    if (pNotify != NULL)
        (void)pNotify->pVtbl->f00(pNotify, 1);

    pNext = g_brHook84.pAA295C;         /* read BEFORE the clear */
    g_brHook84.pAA2968  = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    g_pBrUiNav->n0AA010 = 2;            /* 0x100AA010 <- 2, not 0 */
    return 0;
}

/* ==========================================================================
 * INSTALLERS
 * ========================================================================== */

/* The body 0x10045090 and 0x100450C0 share once the leading call has run.
 *
 * DEVIATION (memory safety): 0x10AA29B0 is NULL-guarded.  The original stores
 * through it with no test even though the activate it just ran can fail --
 * the same deviation slice7_81.c takes at its three twins. */
static int32_t Br84WireBackRow(void)
{
    BrUiCtl_ *pBack;

    (void)BrUiHook81Activate_10045C90();

    pBack = g_brS71.pAA29B0;            /* 0x10AA29B0 -- a CONTROL */
    if (pBack != NULL)
        pBack->pfn08 = BrUiHook84_10046DC0;

    g_pBrUiNav->n0AA010 = 0;
    return 1;
}

int32_t BrUiHook84_10045090(BrUiCtl_ *pCtl)
{
    (void)pCtl;                         /* pushed at 0x10045C90, never read */
    return Br84WireBackRow();
}

int32_t BrUiHook84_100450C0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    /* The one instruction that separates this from 0x10045090.  0x10041BD0 is
     * a stub in this build; the call is kept because the ORDER is the value
     * here, exactly as slice6_71.c keeps its stubbed callees. */
    BrExt_10041BD0();
    return Br84WireBackRow();
}

/* WHAT IT DOES: the Options button on the season-progress screen -- one of the
 * three picture buttons down its right-hand side. It opens the options screen
 * and wires that screen's Back row so the player comes back here when done. */
/* @implements 0x100457C0 d3d BrUiHook84_100457C0 */
int32_t BrUiHook84_100457C0(BrUiCtl_ *pCtl)
{
    BrUiCtl_ *pBack;

    (void)pCtl;                         /* pushed, ignored by the callee */
    (void)BrUiHook81Activate_100451E0();

    pBack = g_br73.pAA29C8;             /* 0x10AA29C8 -- a CONTROL */
    if (pBack != NULL)                  /* DEVIATION: guarded */
        pBack->pfn08 = BrUiHook84_10046830;
    return 1;
}

/* WHAT IT DOES: the Save button on the season-progress screen. It opens the
 * save-season screen and wires that screen's Back row to the variant that also
 * throws away the name being typed -- which is why backing out of a save
 * abandons the edit rather than keeping it. */
/* @implements 0x100457E0 d3d BrUiHook84_100457E0 */
int32_t BrUiHook84_100457E0(BrUiCtl_ *pCtl)
{
    BrUiCtl_ *pBack;

    (void)pCtl;
    (void)BrUiHook81Activate_10045BC0();

    pBack = g_br73.pAA29F4;             /* 0x10AA29F4 -- a CONTROL */
    if (pBack != NULL)                  /* DEVIATION: guarded */
        pBack->pfn08 = BrUiHook84_10046870;
    return 1;
}

int32_t BrUiHook84_100471B0(BrUiCtl_ *pCtl)
{
    BrPhase_ *pOwner;
    BrPhase_ *pDrop;

    /* GOTCHA (order): the ACTIVATE runs FIRST and the owner release SECOND.
     * Every other installer in the family does the release first, and the
     * argument is held in esi across the call precisely so the owner can be
     * reached afterwards. */
    (void)BrUiHook81Activate_10045C90();

    pOwner = pCtl->pOwner;
    pOwner->pVtbl->f1C(pOwner);

    pDrop = g_brHook84.pAA2970;
    if (pDrop != NULL)
        (void)pDrop->pVtbl->f00(pDrop, 1);
    g_brHook84.pAA2970 = NULL;

    /* 0x10AA2904 is NOT touched -- this one leaves the current phase alone. */
    return 0;                           /* 0, unlike its three neighbours */
}

/* ==========================================================================
 * MISC
 * ========================================================================== */

int32_t BrUiHook84_10047340(BrUiCtl_ *pCtl)
{
    /* `mov ecx,8 / xor eax,eax / lea edi,0x10A9D618 / rep stosd` -- EIGHT
     * dwords of ZERO.  Checked against the listing rather than assumed: this
     * codebase has `rep stosd` sites that fill -1, and this is not one.
     *
     * DEVIATION: bounded by g_br73.cbScratch.  The original's 32 is a literal
     * and the buffer's true capacity is not established anywhere. */
    size_t cb = g_br73.cbScratch;

    (void)pCtl;

    if (g_br73.szA9D618 != NULL) {
        if (cb > 32u)
            cb = 32u;
        memset(g_br73.szA9D618, 0, cb);
    }
    g_br73.nAA28A4     = 0;             /* 0x10AA28A4 */
    g_brHook84.bAA26F5 = 0;             /* 0x10AA26F5 -- a BYTE, not a dword */
    return 1;
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

void BrUiHook84Install71(BrS71Hooks *pHooks)
{
    if (pHooks == NULL)
        return;

    /* +0x0C on every selectable row of all four screens.  Not an adapter:
     * br_sprfont.c's body already takes a BrUiCtl_ *. */
    pHooks->p10047360 = BrSprFontKindHook_10047360;

    /* 0x10049F40 -- slice6_71.c:286, :298 */
    pHooks->p10046F60 = BrUiHook84_10046F60;
    pHooks->p10046FC0 = BrUiHook84_10046FC0;

    /* 0x10051D30 -- slice6_71.c:361 */
    pHooks->p100471B0 = BrUiHook84_100471B0;

    /* 0x1004F700 -- slice6_71.c:475, :502, :518 */
    pHooks->p10045090 = BrUiHook84_10045090;
    pHooks->p100450C0 = BrUiHook84_100450C0;
    pHooks->p10046E10 = BrUiHook84_10046E10;

    /* 0x100575F0 -- slice6_71.c:692.  0x100444C0 (:704) is NOT installed:
     * its body byte-addresses the control and no control-typed transcription
     * exists yet.  See NOT DONE (D) in slice8_84.h. */
    pHooks->p100443E0 = BrUiHook84Opt_100443E0;

    /* NOT TOUCHED, and each is a visible hole rather than a silent no-op:
     *   p1003EAE0 p1003F210 p1003F280            slice2_23 byte-image model
     *   p10040A50 p10040AC0 p10041300 p10041890  slice2_24 compressed model
     *   p10042170                                no body anywhere
     *   p10042B00 p100443E0's neighbour 0x100444C0
     *   p1003F720                                installed by no builder here
     */
}

void BrUiHook84Install72(BrUi72Hooks *pHooks)
{
    if (pHooks == NULL)
        return;

    /* +0x0C on every selectable row of all six screens.  0x100474B0 -- the
     * one row in 0x10057C10 that gets a different +0x0C -- is left NULL; see
     * NOT DONE (D). */
    pHooks->p10047360 = BrSprFontKindHook_10047360;

    /* 0x10056A10 -- slice6_72.c:737, :749 */
    pHooks->p10043F50 = BrUiHook84_10043F50;
    pHooks->p10044B40 = BrUiHook84_10044B40;

    /* 0x10057C10 -- slice6_72.c:799, :810, :821, :832, :846 */
    pHooks->p10042EE0 = BrUiHook84Opt_10042EE0;
    pHooks->p10043180 = BrUiHook84Opt_10043180;
    pHooks->p100430B0 = BrUiHook84Opt_100430B0;
    pHooks->p10044600 = BrUiHook84Opt_10044600;
    pHooks->p100446D0 = BrUiHook84Opt_100446D0;

    /* 0x10052030 -- slice6_72.c:965, :990, :1004, :1023, :1041.
     * 0x10045050 (:979) is NOT installed; see NOT DONE (D). */
    pHooks->p10047340 = BrUiHook84_10047340;
    pHooks->p10047060 = BrUiHook84_10047060;
    pHooks->p100457E0 = BrUiHook84_100457E0;
    pHooks->p10043FA0 = BrUiHook84_10043FA0;
    pHooks->p100457C0 = BrUiHook84_100457C0;

    /* 0x10059760 -- slice6_72.c:1226.  0x10046260 (:1202) and 0x10044D00
     * (:1213) are NOT installed; see NOT DONE (D). */
    pHooks->p10044C70 = BrUiHook84_10044C70;

    /* 0x1005A6E0 -- slice6_72.c:1323 */
    pHooks->p10044F00 = BrUiHook84_10044F00;

    /* 0x1004E830 -- slice6_72.c:1495 */
    pHooks->p10046710 = BrUiHook84_10046710;

    /* NOT TOUCHED ON PURPOSE:
     *   p10043590 p100435F0 p10043650 p100436B0  slice7_80.c's BrUiOptInstall72
     *                                            fills these four; this
     *                                            installer must not race it.
     *   the slice2_23 / slice2_24 pfn04 families and the three list callbacks
     *   0x10042560 / 0x10042740 -- see NOT DONE (A), (B), (C).
     */
}
