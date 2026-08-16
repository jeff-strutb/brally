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
#include "br_uictl.h"    /* BrUiCtlCtor           -- 0x100476C0 */
#include "br_uivt.h"     /* BrUiPageCtor_10048470 -- 0x10048470 */
#include "br_phase.h"
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

    g_env72.pAA2908     = g_pPhaseAA2908;
    g_env72.pHooks      = &g_hooks72;
    g_env72.pfnCtlCtor  = BrUiCtlCtor;
    g_env72.pfnPageCtor = BrUiPageCtor_10048470;
    g_pBr72Env = &g_env72;
}
