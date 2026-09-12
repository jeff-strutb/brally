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
/* @n64 0x8021C6D0 located */
int BrGameStepIs(BrGameStepFn pfn)
{
    return pfn == g_pfnStep;
}

/* The address-typed view of the SAME function, for slice4_50.c, whose whole
 * range models this slot as data (`const void *g_BrPadHookFn` is a literal
 * code address, not a callable).  The function-to-object conversion is
 * confined to this one line deliberately: it is the price of two modules
 * having modelled one dword under two C types, and putting it anywhere else
 * would spread it. */
/* @n64 0x8026B720 located */
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
/* @n64 0x8021C6F0 located */
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


/* 0x1002E186 -- the frame clock.  /Od like the rest of this range: every
 * cast and 64-bit step is spelled out (`__allmul` / `__aulldiv` for the
 * 64-bit multiply and divide, add/adc for the accumulate, `fild qword`
 * for the conversion).  The tick counter is one unsigned 64-bit global
 * (0x106EC740/44); the timer frequency another (0x100AD7C0/C4). */
extern int              DAT_106ed6d8;   /* g_brRaceBeginDrawn: frame is synthetic  */
extern unsigned __int64 DAT_106ec740;   /* accumulated ticks                       */
extern int              DAT_106e7294;   /* last raw tick                           */
extern unsigned __int64 DAT_100ad7c0;   /* ticks per second                        */
extern unsigned int     DAT_106ec768;   /* milliseconds now                        */
extern unsigned int     DAT_106ed588;   /* milliseconds at the previous frame      */
extern float            DAT_100774f0;   /* 1000.0f: ms -> s                        */
extern float            DAT_106e9d8c;   /* the frame delta in seconds              */
extern unsigned int     DAT_106b7ac0;   /* the frame delta in milliseconds         */
int  FUN_10059f00(void);                /* raw tick read                           */
typedef union {
    struct { unsigned int lo; int hi; } u;
    __int64 q;
} BrFrameClockTicks;
void BrPadTranslateAll(void);           /* 0x1002CE9A                              */
void FUN_10008d60(void);                /* the folded empty function, called twice */

/* WHAT IT DOES: advances the game clock by one frame.  While the race is
 * being drawn synthetically it adds a fixed thirtieth of a second's worth
 * of ticks; otherwise it adds the real ticks elapsed since the last read,
 * starting the count on the first call.  From the tick total it derives the
 * time in milliseconds, remembers the previous frame's value, translates
 * the pads, and publishes the frame delta both in seconds (as a float) and
 * in whole milliseconds. */
/* @implements 0x1002E186 glide BrFrameClockStep */
void BrFrameClockStep(void)
{
    int now;

    if (DAT_106ed6d8 != 0) {
        DAT_106ec740 += DAT_100ad7c0 * 0x1FCA055 / 1000000000;
        DAT_106e7294 = FUN_10059f00();
    } else if (DAT_106ec740 != 0) {
        now = FUN_10059f00();
        DAT_106ec740 += (unsigned int)(now - DAT_106e7294);
        DAT_106e7294 = now;
    } else {
        DAT_106e7294 = FUN_10059f00();
        DAT_106ec740 = (unsigned int)DAT_106e7294;
    }
    DAT_106ed588 = DAT_106ec768;
    DAT_106ec768 = (unsigned int)(DAT_106ec740 * 1000000 / DAT_100ad7c0) / 1000;
    BrPadTranslateAll();
    FUN_10008d60();
    FUN_10008d60();
    /* The delta is widened by PARTS -- low dword stored, high dword an
     * immediate zero (`mov dword ptr [ebp-8], 0`) -- the LARGE_INTEGER
     * spelling, and the conversion is a SIGNED 64-bit `fild qword` (VC5 has
     * no unsigned-64 -> float: C2520).  A `(__int64)(unsigned)` cast widens
     * through a zeroed REGISTER instead (`xor eax,eax; mov [ebp-8],eax`).
     * The `(float)` rounding through [ebp-0x10] is the /Od /Op idiom
     * (match_sweep's Odp shape). */
    {
        BrFrameClockTicks t;
        t.u.lo = DAT_106ec768 - DAT_106ed588;
        t.u.hi = 0;
        DAT_106e9d8c = (float)t.q / DAT_100774f0;
    }
    DAT_106b7ac0 = DAT_106ec768 - DAT_106ed588;
}

#endif /* BR_MATCHING_BUILD */
