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
#include <string.h>

static BrUi72Hooks g_hooks72;     /* all slots NULL, deliberately */
static Br72Env     g_env72;

void BrHostWire72(void)
{
    memset(&g_hooks72, 0, sizeof(g_hooks72));
    memset(&g_env72,   0, sizeof(g_env72));

    g_env72.pHooks      = &g_hooks72;
    g_env72.pfnCtlCtor  = BrUiCtlCtor;
    g_env72.pfnPageCtor = BrUiPageCtor_10048470;
    g_pBr72Env = &g_env72;
}
