/* br_wire72.c -- supply slice6_72's module context. See br_wire71.c for why
 * each slice gets its own translation unit.
 *
 * Unlike slice6_71's, this context needs two REAL functions, not just non-NULL
 * storage: the page and control constructors. slice6_72 reaches both through
 * pointers precisely so its header would not have to redeclare addresses that
 * slice3_33.h and br_uictl.h already declare over different struct types.
 *
 * That leaves the cast below, which is worth being explicit about rather than
 * hiding: BrUi72Ctl and BrUiCtl_ are two partial models of ONE original object
 * (the 0x1E210-byte control). Casting between them is safe only because the
 * constructor is passed a pointer it merely initialises -- it never reads a
 * field through the caller's view. If a future signature starts depending on
 * the layout, this cast becomes a real bug and the two models must be merged
 * first, the same way br_phase.h merged the phase models.
 */
#include "slice6_72.h"

/* NOT #include "br_uictl.h".
 *
 * That header pulls in slice6_73.h, and slice6_72.h and slice6_73.h BOTH
 * define `struct BrUiPage_`. They cannot coexist in one translation unit --
 * the same class of conflict br_phase.h was created to resolve, still
 * unresolved for the page and control types.
 *
 * So the constructor is declared here directly. The declaration is written to
 * match br_uictl.h's exactly (same tag, same signature), so if either changes
 * the other stops compiling rather than silently disagreeing at the ABI. */
struct BrUiCtl_;
struct BrUiCtl_ *BrUiCtlCtor(struct BrUiCtl_ *pThis);

/* 0x10048470 from br_uivt.h, declared the same way and for the same reason.
 *
 * Unlike the control, the PAGE needs no cast at all: `BrUiPage_` is one tag
 * completed by both slice6_72.h and slice6_73.h, and the two definitions are
 * now field-for-field identical, so this declaration and br_uivt.h's describe
 * the same layout. If they ever diverge again the compiler will not catch it,
 * which is precisely why they were made to agree. */
BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis);
#include <string.h>

/* 0x100476C0, over slice6_72's view of the control. */
static BrUi72Ctl *Ctl72Ctor(BrUi72Ctl *pThis)
{
    return (BrUi72Ctl *)BrUiCtlCtor((struct BrUiCtl_ *)pThis);
}

static BrUi72Hooks g_hooks72;     /* all slots NULL, deliberately */
static Br72Env     g_env72;

void BrHostWire72(void)
{
    memset(&g_hooks72, 0, sizeof(g_hooks72));
    memset(&g_env72,   0, sizeof(g_env72));

    g_env72.pHooks     = &g_hooks72;
    g_env72.pfnCtlCtor  = Ctl72Ctor;
    g_env72.pfnPageCtor = BrUiPageCtor_10048470;
    g_pBr72Env = &g_env72;
}
