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
/* WHAT IT DOES: chooses what the game does each frame. The game keeps one
 * slot naming the current activity -- racing, sitting in the front end, or
 * doing nothing -- and this is how that slot gets changed. It accepts
 * whatever it is handed without checking, so handing it nothing leaves the
 * game with no frame work to do. */
/* @implements 0x1002E317 glide BrGameStepSet */
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
/* WHAT IT DOES: answers "is this the activity the game is currently running?"
 * -- a yes/no check against the slot BrGameStepSet writes, used by code that
 * needs to know whether it is, say, in a race before acting. */
/* @implements 0x1002E302 glide BrGameStepIs */
int BrGameStepIs(BrGameStepFn pfn)
{
    return (g_pfnStep == pfn) ? 1 : 0;
}

/* The address-typed view of the SAME function, for slice4_50.c, whose whole
 * range models this slot as data (`const void *g_BrPadHookFn` is a literal
 * code address, not a callable).  The function-to-object conversion is
 * confined to this one line deliberately: it is the price of two modules
 * having modelled one dword under two C types, and putting it anywhere else
 * would spread it. */
int BrGameStepIsAddr(const void *pv)
{
    return ((const void *)g_pfnStep == pv) ? 1 : 0;
}

/* 0x1002E324 -- `call dword ptr [0x106E79F4]`.  The original does NOT test
 * for NULL; this does, because a null call is a crash rather than a
 * behaviour, and the harness needs to be able to say "nothing installed". */
/* WHAT IT DOES: runs one frame of whatever the game is currently doing. The
 * window's message pump calls this over and over, and it is the single point
 * where the race, or the front end, gets its turn each frame. */
/* @implements 0x1002E324 glide BrGameStepInvoke */
#ifdef BR_MATCHING_BUILD
/* Orig: PUSH EBP / MOV EBP,ESP / CALL [g_pfnStep] / POP EBP / RET (11 B).
 * No NULL guard -- just calls through the pointer and returns whatever EAX is. */
int BrGameStepInvoke(void)
{
    return ((int (*)(void))g_pfnStep)();
}
#else
int BrGameStepInvoke(void)
{
    if (g_pfnStep == NULL) {
        return 0;
    }
    g_pfnStep();
    return 1;
}
#endif

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

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E32F glide BrNop_1002E32F */

void BrNop_1002E32F(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E334 glide BrNop_1002E334 */

void BrNop_1002E334(void)

{
  return;
}

#endif /* BR_MATCHING_BUILD */
