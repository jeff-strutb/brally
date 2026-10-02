/* br_gamestep.c -- Glide 0x106E79F4 and the three functions around it.
 * See br_gamestep.h.
 */
#include "slice1_05.h"   /* br_globals: its objects */
#include <stddef.h>

#include "br_gamestep.h"

/* 0x106E79F4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

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

/* (port-only BrGameStepRegister removed) */


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
/* (port-only BrGameStepIsAddr removed) */


/* 0x1002E324 -- `call dword ptr [0x106E79F4]`.  The original does NOT test
 * for NULL; this does, because a null call is a crash rather than a
 * behaviour, and the harness needs to be able to say "nothing installed". */
/* WHAT IT DOES: runs one frame of whatever the game is currently doing. The
 * window's message pump calls this over and over, and it is the single point
 * where the race, or the front end, gets its turn each frame. */
/* @implements 0x1002E324 glide BrGameStepInvoke */
/* @n64 0x8021C6F0 located */
/* Orig: PUSH EBP / MOV EBP,ESP / CALL [g_pfnStep] / POP EBP / RET (11 B).
 * No NULL guard -- just calls through the pointer and returns whatever EAX is. */
int BrGameStepInvoke(void)
{
    return ((int (*)(void))g_pfnStep)();
}

/* (port-only BrGameStepGet removed) */


/* (port-only BrGameStepId removed) */


/* (port-only BrGameStepName removed) */


/* (port-only BrGameStepPump removed) */


/* -- Ghidra-matched functions --------------------------- */

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
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* g_brRaceBeginDrawn: frame is synthetic  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* accumulated ticks                       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* last raw tick                           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* ticks per second                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* milliseconds now                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* milliseconds at the previous frame      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 1000.0f: ms -> s                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the frame delta in seconds              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the frame delta in milliseconds         */
/* FUN_10059f00: prototype in br_funcs.h */
typedef union {
    struct { unsigned int lo; int hi; } u;
    __int64 q;
} BrFrameClockTicks;
/* BrPadTranslateAll: prototype in br_funcs.h */
/* FUN_10008d60: prototype in br_funcs.h */

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

    if ((*(int *)((char *)&g_aBrEntRecs + 0xA8)) != 0) {
        DAT_106ec740 += DAT_100ad7c0 * 0x1FCA055 / 1000000000;
        DAT_106e7294 = BrTickAdd_10078C10();
    } else if (DAT_106ec740 != 0) {
        now = BrTickAdd_10078C10();
        DAT_106ec740 += (unsigned int)(now - DAT_106e7294);
        DAT_106e7294 = now;
    } else {
        DAT_106e7294 = BrTickAdd_10078C10();
        DAT_106ec740 = (unsigned int)DAT_106e7294;
    }
    (*(unsigned int *)&DAT_106ed588) = (*(unsigned int *)&DAT_106ec768);
    (*(unsigned int *)&DAT_106ec768) = (unsigned int)(DAT_106ec740 * 1000000 / DAT_100ad7c0) / 1000;
    BrPadTranslateAll();
    BrPodNop();
    BrPodNop();
    /* The delta is widened by PARTS -- low dword stored, high dword an
     * immediate zero (`mov dword ptr [ebp-8], 0`) -- the LARGE_INTEGER
     * spelling, and the conversion is a SIGNED 64-bit `fild qword` (VC5 has
     * no unsigned-64 -> float: C2520).  A `(__int64)(unsigned)` cast widens
     * through a zeroed REGISTER instead (`xor eax,eax; mov [ebp-8],eax`).
     * The `(float)` rounding through [ebp-0x10] is the /Od /Op idiom
     * (match_sweep's Odp shape). */
    {
        BrFrameClockTicks t;
        t.u.lo = (*(unsigned int *)&DAT_106ec768) - (*(unsigned int *)&DAT_106ed588);
        t.u.hi = 0;
        g_brRaceFlyStep = (float)t.q / DAT_100774f0;
    }
    (*(unsigned int *)&DAT_106b7ac0) = (*(unsigned int *)&DAT_106ec768) - (*(unsigned int *)&DAT_106ed588);
}

