/* br_wire72.c -- supply slice6_72's module context. See br_wire71.c for why
 * each slice gets its own translation unit.
 *
 * Unlike slice6_71's, this context needs two REAL functions, not just non-NULL
 * storage: the page and control constructors. slice6_72 reaches both through
 * pointers precisely so its header would not have to redeclare addresses that
 * slice3_33.h already declares over different struct types.
 *
 * WHAT USED TO BE HERE, AND WHY IT IS GONE
 *
 * This file used to hand-declare `BrUiCtlCtor` and `BrUiPageCtor_10048470`
 * itself, with a banner explaining that it could not `#include "br_uictl.h"`
 * because that header pulls in slice6_73.h and slice6_72.h and slice6_73.h
 * BOTH defined `struct BrUiPage_` -- one C tag, two definitions, compiling
 * only because they never met in a translation unit. It also carried a
 * `Ctl72Ctor` shim that cast between `BrUi72Ctl *` and `struct BrUiCtl_ *`,
 * two partial models of the one 0x1E214 object, and a note that the cast
 * "becomes a real bug" the moment a signature starts depending on the layout.
 *
 * Both packets are now typed over br_ui.h, which owns the tag. There is one
 * definition of the page and one of the control, the two headers can share a
 * translation unit, and the two constructors are the ones br_uictl.h and
 * br_uivt.h declare -- so the declarations, the shim and the cast are all
 * deleted rather than commented. The include below is the whole workaround's
 * replacement.
 */
#include "slice6_72.h"
#include "slice8_84.h"
#include "br_uictl.h"    /* BrUiCtlCtor           -- 0x100476C0 */
#include "br_uivt.h"     /* BrUiPageCtor_10048470 -- 0x10048470 */
#include "br_phase.h"
#include "slice6_77.h"   /* BrFfbReprobe -- 0x100795D0 */
#include <string.h>
#include <stdlib.h>

/* --- the global phase at 0x10AA2908 -------------------------------------
 * 0x1005A6E0 reads `pE->pAA2908->fC4`. Same situation as br_wire71.c's fC0:
 * the original has a real global phase built by an unported init path, so the
 * harness stands in for that init using the REAL constructor.
 *
 * fC4 is the GRF list, and unlike slice6_71's season list this one HAS a
 * modelled type -- BrGrfList is vtable + 100 * 0x104 names -- so sizeof is
 * correct here and no hand-sizing is needed. The list is left empty (no
 * TimeAttack*.GRF present), a state the game genuinely has. The scan hook is a
 * no-op: a real scan needs a filesystem layout this port has not defined, and
 * inventing entries would put fictional saves on screen.
 * ------------------------------------------------------------------------ */
BrPhase_ *BrOptObjCtor(BrPhase_ *pThis);

static BrPhase_ *g_pPhaseAA2908;
static BrGrfList g_grfList;

static void GrfListScan(BrGrfList *pThis, const char *pszMask)
{
    (void)pThis; (void)pszMask;      /* finds nothing; see above */
}
static const BrGrfListVtbl g_grfListVtbl = { 0, GrfListScan };

static BrUi72Hooks g_hooks72;     /* all slots NULL, deliberately */
static Br72Env     g_env72;

/* 0x118ABDBC, slice3_45.c's `g_br18ABDBC`. Hand-declared rather than reached
 * through slice3_45.h, which cannot be included here: it defines `struct
 * BrEnt` and so does slice1_05.h, which slice6_72.h pulls in -- one C tag,
 * two definitions, exactly the class of clash br_wire71.c's banner describes
 * and the reason each slice gets its own translation unit. The type matches
 * slice3_45.h's declaration. */
extern int32_t g_br18ABDBC;

void BrHostWire72(void)
{
    memset(&g_hooks72, 0, sizeof(g_hooks72));
    memset(&g_env72,   0, sizeof(g_env72));

    if (!g_pPhaseAA2908) {
        g_pPhaseAA2908 = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
        if (g_pPhaseAA2908) BrOptObjCtor(g_pPhaseAA2908);
    }
    memset(&g_grfList, 0, sizeof(g_grfList));
    g_grfList.pVtbl = &g_grfListVtbl;
    if (g_pPhaseAA2908) g_pPhaseAA2908->fC4 = &g_grfList;

    /* The style rectangles at 0x100AB438.. -- see slice3_39.h. Left NULL until
     * 0x1005B910 was ported, because until then nothing in the port looked
     * inside one; 0x1005A6E0 passes p0AB4D8 straight into it and it reads four
     * int32s out. The banner about hook slots still holds. */
    g_env72.p0AB438 = BR_UI_STYLE(0x100AB438);
    g_env72.p0AB448 = BR_UI_STYLE(0x100AB448);
    g_env72.p0AB458 = BR_UI_STYLE(0x100AB458);
    g_env72.p0AB478 = BR_UI_STYLE(0x100AB478);
    g_env72.p0AB488 = BR_UI_STYLE(0x100AB488);
    g_env72.p0AB4A8 = BR_UI_STYLE(0x100AB4A8);
    g_env72.p0AB4B8 = BR_UI_STYLE(0x100AB4B8);
    g_env72.p0AB4C8 = BR_UI_STYLE(0x100AB4C8);
    g_env72.p0AB4D8 = BR_UI_STYLE(0x100AB4D8);
    g_env72.p0AB4F8 = BR_UI_STYLE(0x100AB4F8);
    g_env72.p0AB508 = BR_UI_STYLE(0x100AB508);

    g_env72.pAA2908     = g_pPhaseAA2908;
    g_env72.pHooks      = &g_hooks72;
    BrUiHook84Install72(&g_hooks72);   /* slice8_84's 16 slots */
    g_env72.pfnCtlCtor  = BrUiCtlCtor;
    g_env72.pfnPageCtor = BrUiPageCtor_10048470;

    /* Not a hook the builders merely store: BrExt_1004E830 CALLS this one,
     * two controls into its build, to refresh 0x118ABDBC before reading it.
     * 0x100795D0 is ported in slice6_77.c; the DirectInput root it ends up
     * dereferencing is the host's, in br_wire77.c. */
    g_env72.pfn100795D0 = BrFfbReprobe;

    /* ALIASED STORAGE -- READ THIS BEFORE TRUSTING n18ABDBC.
     *
     * 0x118ABDBC has two models in this tree: slice3_45.c owns it as the
     * global `g_br18ABDBC`, and Br72Env carries a COPY of it as `n18ABDBC`
     * (slice6_72.h) because slice6_72 reaches every global through its
     * context table. Until now nothing in the same process wrote the global,
     * so the copy could not go stale. Wiring the probe changes that:
     * BrFfbInit writes g_br18ABDBC, and BrExt_1004E830 re-reads pE->n18ABDBC
     * on the very next line expecting to see the result.
     *
     * The seed below keeps the two consistent at wiring time. It does NOT
     * make them one object, and it cannot: the copy is snapshotted here and
     * the builder's mid-function re-read still sees the snapshot. Both are 0
     * in this harness -- there is no force-feedback wheel, so BrFfbInit's
     * fallback path clears the global to 0 and the copy already was 0 -- so
     * nothing observable diverges today. The proper fix is to give Br72Env a
     * POINTER to slice3_45's global instead of a copy, which is a change to
     * slice6_72's model and belongs with that packet, not here. */
    g_env72.n18ABDBC = g_br18ABDBC;

    g_pBr72Env = &g_env72;
}
