/* br_wire71.c -- supply slice6_71's module context for the host harness.
 *
 * WHY THIS IS A FILE OF ITS OWN
 *
 * Each slice module keeps the original's .data in a context struct it owns
 * (g_brS71 here, g_br73 in slice6_73, g_pBr72Env in slice6_72), and each of
 * those headers carries its own partial model of the shared UI types. Two of
 * them cannot be included in one translation unit -- that conflict is the whole
 * reason br_phase.h exists. So the wiring is split one file per slice, and the
 * host calls each entry point without ever seeing the conflicting headers.
 *
 * WHAT IT DOES NOT DO
 *
 * The hook pointers stay NULL. The builders only STORE them into control
 * slots during a build, so NULL is the faithful value at that point -- and if
 * something does call one, a NULL crash is the right outcome rather than a
 * silent no-op that hides a missing behaviour.
 *
 * What must be non-NULL is the context and its sub-structs themselves, because
 * the builders dereference those to reach the hook slots. That is the actual
 * bug this file fixes: `pH->p10047360` faulted at slice6_71.c:295 with
 * pHooks == NULL, which is a NULL STRUCT POINTER, not a NULL function pointer.
 * The two failures look identical in a debugger and mean opposite things.
 */
#include "slice6_71.h"
#include <string.h>

static BrS71Hooks g_hooks71;      /* all slots NULL, deliberately */
static BrS71Env   g_env71;
static char       g_a9d018[BR71_A9D018_SIZE];

void BrHostWire71(void)
{
    memset(&g_hooks71, 0, sizeof(g_hooks71));
    memset(&g_env71,   0, sizeof(g_env71));
    memset(g_a9d018,   0, sizeof(g_a9d018));

    g_brS71.pHooks  = &g_hooks71;
    g_brS71.pA9D018 = g_a9d018;
    g_brS71Env      = &g_env71;
}
