/* br_gamestep.c -- Glide 0x106E79F4 and the three functions around it.
 * See br_gamestep.h.
 */
#include <stddef.h>

#include "br_gamestep.h"

/* 0x106E79F4 */
static BrGameStepFn g_pfnStep;

#define BR_GS_SLOTS 4
static BrGameStepFn g_apKnown[BR_GS_SLOTS];
static int          g_aKnownId[BR_GS_SLOTS];
static int          g_cKnown;

/* 0x1002E317 -- `mov eax,[ebp+8]; mov [0x106E79F4],eax`.  Thirteen bytes, no
 * validation of any kind: the original will happily install a null step and
 * the pump will happily call it. */
void BrGameStepSet(BrGameStepFn pfn)
{
    g_pfnStep = pfn;
}

void BrGameStepRegister(BrGameStepFn pfn, int id)
{
    int i;
    for (i = 0; i < g_cKnown; ++i) {
        if (g_apKnown[i] == pfn) { g_aKnownId[i] = id; return; }
    }
    if (g_cKnown < BR_GS_SLOTS) {
        g_apKnown[g_cKnown]  = pfn;
        g_aKnownId[g_cKnown] = id;
        ++g_cKnown;
    }
}

/* 0x1002E302 -- `xor ecx,ecx; cmp eax,[0x106E79F4]; sete cl`. */
int BrGameStepIs(BrGameStepFn pfn)
{
    return (g_pfnStep == pfn) ? 1 : 0;
}

/* 0x1002E324 -- `call dword ptr [0x106E79F4]`.  The original does NOT test
 * for NULL; this does, because a null call is a crash rather than a
 * behaviour, and the harness needs to be able to say "nothing installed". */
int BrGameStepInvoke(void)
{
    if (g_pfnStep == NULL) {
        return 0;
    }
    g_pfnStep();
    return 1;
}

BrGameStepFn BrGameStepGet(void)
{
    return g_pfnStep;
}

int BrGameStepId(void)
{
    int i;
    if (g_pfnStep == NULL) {
        return BR_GAMESTEP_NONE;
    }
    for (i = 0; i < g_cKnown; ++i) {
        if (g_apKnown[i] == g_pfnStep) {
            return g_aKnownId[i];
        }
    }
    return BR_GAMESTEP_OTHER;
}

const char *BrGameStepName(int id)
{
    switch (id) {
    case BR_GAMESTEP_NONE:     return "(none installed)";
    case BR_GAMESTEP_RACE:     return "0x10019A70 race";
    case BR_GAMESTEP_FRONTEND: return "0x10032680 front end";
    case BR_GAMESTEP_NULL:     return "0x10008D60 null step";
    default:                   return "(not one of the original three)";
    }
}

int BrGameStepPump(int state)
{
    if (state != BR_GAMESTATE_STEP) {
        /* Every other arm of the jump table is unported.  Saying so is the
         * point: returning 0 here would be indistinguishable from "the step
         * ran and did nothing". */
        return -1;
    }
    return BrGameStepInvoke();
}
