/* slice7_80.c -- the option-changing control hooks.  See slice7_80.h for what
 * this module is, why all six bodies live in slice2_25.c, and why the seam at
 * the bottom writes one original word into three host objects.
 *
 * NOTHING IS DECOMPILED IN THIS FILE.  Every hook is a one-line adapter over
 * an existing, verified body; the rest is installation, the input seam, and
 * read-only observation.  If a body ever turns out to be wrong, it is
 * slice2_25.c that is wrong -- this file cannot hide it and must not acquire
 * a second opinion.
 */
#include "slice7_80.h"

/* ==========================================================================
 * Cross-slice declarations.
 *
 * slice2_25.h CANNOT be included here: it declares
 *     BrOptObj *BrOptObjCtor(BrOptObj *)
 * over its five-field partial view of the phase, while slice6_73.h -- which
 * this module needs for BrUi73Hooks -- declares the same symbol over the
 * canonical BrPhase_.  That is slice6_73.h's CONFLICT 1, and the two headers
 * are deliberately never combined.
 *
 * So the six entry points and the globals are declared by hand, and each is
 * copied verbatim from slice2_25.h so a future diff of the two is a diff.
 * This is the same workaround br_wire72.c uses for g_br18ABDBC, for the same
 * class of reason.
 * ========================================================================== */

/* XSLICE port/include/slice2_25.h:576-577, 599-602 -- the six bodies.
 * `int (void)`, and that is not a mistake: none of the six reads its stack
 * argument.  See the ADAPTERS note in slice7_80.h. */
extern int BrOptCycleB4E708(void);   /* 0x10042CF0 */
extern int BrOptCycleB4E70C(void);   /* 0x10042D60 */
extern int BrOptCycleAA2A1C(void);   /* 0x10043590 */
extern int BrOptCycleAA2A28(void);   /* 0x100435F0 */
extern int BrOptCycleAA2A20(void);   /* 0x10043650 */
extern int BrOptCycleAA2A24(void);   /* 0x100436B0 */

/* XSLICE port/include/slice2_25.h:293-294 -- the two edit inputs. */
extern int32_t g_brAA33D4;      /* 0x10AA33D4  step the option UP   */
extern int32_t g_brAA33D0;      /* 0x10AA33D0  step the option DOWN */

/* XSLICE port/include/slice2_25.h -- the index words the six hooks cycle. */
extern int32_t g_brB4E708;      /* 0x10B4E708 */
extern int32_t g_brB4E70C;      /* 0x10B4E70C */
extern int32_t g_brAA2A1C;      /* 0x10AA2A1C */
extern int32_t g_brAA2A28;      /* 0x10AA2A28 */
extern int32_t g_brAA2A20;      /* 0x10AA2A20 */
extern int32_t g_brAA2A24;      /* 0x10AA2A24 */

/* XSLICE port/include/slice2_25.h -- the words the four toggles publish. */
extern int32_t g_brB4E1E0;      /* 0x10B4E1E0  <- 0x100AC530[0x10AA2A1C] */
extern int32_t g_brB4E7A0;      /* 0x10B4E7A0  <- 0x100AC548[0x10AA2A28] */
extern int32_t g_brB4E1D8;      /* 0x10B4E1D8  <- 0x100AC538[0x10AA2A20] */
extern int32_t g_brB4E1DC;      /* 0x10B4E1DC  <- 0x100AC540[0x10AA2A24] */

/* XSLICE port/include/slice2_25.h:333 -- which volume row ran last. */
extern int32_t g_br0AB3D8;      /* 0x100AB3D8 */

/* g_BrBtnEdge[4] (0x10AA33D0) comes from slice3_39.h, which slice6_73.h
 * already includes.  It is the SAME four dwords as g_brAA33D0/D4 above; see
 * the ALIASED STORAGE banner in slice7_80.h. */

/* ==========================================================================
 * The six adapters
 * ========================================================================== */

int32_t BrUiOptHook_10042CF0(BrUiCtl_ *pCtl)
{
    (void)pCtl;                     /* the original never reads it */
    return (int32_t)BrOptCycleB4E708();
}

int32_t BrUiOptHook_10042D60(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleB4E70C();
}

int32_t BrUiOptHook_10043590(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleAA2A1C();
}

int32_t BrUiOptHook_100435F0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleAA2A28();
}

int32_t BrUiOptHook_10043650(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleAA2A20();
}

int32_t BrUiOptHook_100436B0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleAA2A24();
}

/* ==========================================================================
 * Installers
 * ========================================================================== */

void BrUiOptInstall73(BrUi73Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }
    pHooks->p10042CF0 = BrUiOptHook_10042CF0;
    pHooks->p10042D60 = BrUiOptHook_10042D60;
    /* p100466C0 (Audio's "Back") is a phase-changing hook and is NOT this
     * module's; leaving it NULL keeps it a visible hole. */
}

void BrUiOptInstall72(BrUi72Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }
    pHooks->p10043590 = BrUiOptHook_10043590;
    pHooks->p100435F0 = BrUiOptHook_100435F0;
    pHooks->p10043650 = BrUiOptHook_10043650;
    pHooks->p100436B0 = BrUiOptHook_100436B0;
    /* p10046710 (Game Options' "Back") is likewise not ours. */
}

/* ==========================================================================
 * The edit seam
 * ========================================================================== */

/* DEVIATION (port), and the only one in this file: the original has ONE array
 * of four dwords at 0x10AA33D0 and this port has three objects modelling it
 * (slice3_39.c's g_BrBtnEdge, slice2_25.c's two scalars, and the host's
 * BrActiveFlags).  Writing one of them leaves the other two stale, and the
 * readers are split across all three -- the cyclers read slice2_25's,
 * 0x1003E080 reads BrActiveFlags'.  Until the storage is merged (see the
 * banner in slice7_80.h for how), this one function keeps them in step.
 *
 * It is written as a single helper rather than three call sites so that the
 * day the merge happens there is exactly one place to delete from. */
static void BrOptStoreEdge(BrActiveFlags *pFlags, int32_t up, int32_t down)
{
    g_brAA33D4 = up;
    g_brAA33D0 = down;

    g_BrBtnEdge[1] = up;            /* 0x10AA33D4 */
    g_BrBtnEdge[0] = down;          /* 0x10AA33D0 */

    if (pFlags != NULL) {
        pFlags->a1 = (int)up;       /* 0x10AA33D4 */
        pFlags->a0 = (int)down;     /* 0x10AA33D0 */
    }
}

void BrUiOptSetEdit(BrActiveFlags *pFlags, int dir)
{
    if (dir > 0) {
        BrOptStoreEdge(pFlags, 1, 0);
    } else if (dir < 0) {
        BrOptStoreEdge(pFlags, 0, 1);
    } else {
        BrOptStoreEdge(pFlags, 0, 0);
    }
}

void BrUiOptSetActivateOnly(BrActiveFlags *pFlags, int fDown)
{
    /* 0x10AA2AF0.  The edit words are cleared so the two paths cannot be
     * confused: this one activates the row and moves nothing. */
    BrOptStoreEdge(pFlags, 0, 0);
    if (pFlags != NULL) {
        pFlags->a5 = fDown ? 1 : 0;
    }
}

/* ==========================================================================
 * Observation -- host/test facing, not decompiled
 * ========================================================================== */

typedef struct BrOptDesc {
    int32_t    *pIndex;
    int32_t    *pPublished;     /* NULL when the index IS the value */
    uint32_t    va;
    const char *pszName;
} BrOptDesc;

/* Filled per call rather than returned from a shared static: a function that
 * hands back a pointer into one hidden buffer invites exactly the aliasing
 * this codebase spends a whole CONVENTIONS.md section on, and there is no
 * reason to introduce a small one here.  A file-scope const table would need
 * addresses of objects in another translation unit as initialisers, which C99
 * permits but which would separate each option's address from its name; the
 * switch keeps the two on one line, where a mismatch is visible. */
static int BrOptDescFor(BrUiOptId id, BrOptDesc *pOut)
{
    switch (id) {
    case BR_OPT_SFX_VOLUME:
        pOut->pIndex = &g_brB4E708; pOut->pPublished = NULL;
        pOut->va = 0x10B4E708u;     pOut->pszName = "SFX Volume";
        return 1;
    case BR_OPT_CD_VOLUME:
        pOut->pIndex = &g_brB4E70C; pOut->pPublished = NULL;
        pOut->va = 0x10B4E70Cu;     pOut->pszName = "CD VOlume";
        return 1;
    case BR_OPT_FORCE_FEEDBACK:
        pOut->pIndex = &g_brAA2A1C; pOut->pPublished = &g_brB4E1E0;
        pOut->va = 0x10AA2A1Cu;     pOut->pszName = "Force Feedback";
        return 1;
    case BR_OPT_SKID_MARKS:
        pOut->pIndex = &g_brAA2A28; pOut->pPublished = &g_brB4E7A0;
        pOut->va = 0x10AA2A28u;     pOut->pszName = "Skid Marks";
        return 1;
    case BR_OPT_CAR_SHADOW:
        pOut->pIndex = &g_brAA2A20; pOut->pPublished = &g_brB4E1D8;
        pOut->va = 0x10AA2A20u;     pOut->pszName = "Car Shadow";
        return 1;
    case BR_OPT_SPECULAR:
        pOut->pIndex = &g_brAA2A24; pOut->pPublished = &g_brB4E1DC;
        pOut->va = 0x10AA2A24u;     pOut->pszName = "Specular Lighting";
        return 1;
    default:
        return 0;
    }
}

int32_t BrUiOptGetIndex(BrUiOptId id)
{
    BrOptDesc d;
    return BrOptDescFor(id, &d) ? *d.pIndex : 0;
}

int32_t BrUiOptGetPublished(BrUiOptId id)
{
    BrOptDesc d;
    if (!BrOptDescFor(id, &d)) {
        return 0;
    }
    return (d.pPublished != NULL) ? *d.pPublished : *d.pIndex;
}

int32_t BrUiOptGetVolumeSelector(void) { return g_br0AB3D8; }

const char *BrUiOptName(BrUiOptId id)
{
    BrOptDesc d;
    return BrOptDescFor(id, &d) ? d.pszName : "?";
}

uint32_t BrUiOptAddress(BrUiOptId id)
{
    BrOptDesc d;
    return BrOptDescFor(id, &d) ? d.va : 0u;
}

int32_t BrUiOptGetEditUp(void)   { return g_brAA33D4; }
int32_t BrUiOptGetEditDown(void) { return g_brAA33D0; }
