/* br_timer.c -- startup: the game's timer services.
 *
 * Filed out of the address batches; each section keeps the declarations the
 * batch it came from made locally, so the compiler's view of each body is
 * unchanged.
 */
#ifdef BR_MATCHING_BUILD

#include <windows.h>

/* ---- from slice1_09.c ---------------------------------------------- */

extern int g_br18AB118_S_S1499;

/* WHAT IT DOES: return the current timer subsystem state. */
/* @implements 0x1006E350 glide BrGetTimerState */

int BrGetTimerState(void)

{
  return g_br18AB118_S_S1499;
}

#endif /* BR_MATCHING_BUILD */

/* ---- from slice4_50.c ------------------------------------------------
 * The batch's own includes and declarations; the globals stay defined
 * there, the static helper below had no other user.
 * --------------------------------------------------------------------- */
#include <stdint.h>

#include "slice4_50.h"

static int32_t BrPerfToMs(int64_t counter)
{
    if (g_br18AB120 == 0) {
        return 0;               /* DEVIATION: the original divides by zero */
    }
    return (int32_t)((counter * 1000 + 500) / g_br18AB120);
}

/* 0x10075020 */
/* WHAT IT DOES: tells the caller how many milliseconds have passed since the
 * game started. On its first call it works out how to read the machine's
 * high-resolution clock and notes the starting point; from then on it uses
 * that, falling back to the ordinary Windows clock on any machine where the
 * precise one is unavailable. */
/* @implements 0x10075020 d3d BrSub10075020 */
#ifdef BR_MATCHING_BUILD
/* Direct IAT calls and globals; the ms conversion is inline __int64 math
 * ((t*1000+500)/freq via __allmul/__alldiv), truncated to int. The QPC
 * import pointer is CSE'd across both arms. */
extern int     DAT_100bb2dc;
extern __int64 DAT_118ee238;        /* frequency  */
extern int     DAT_118ee240;        /* QPF result */
extern int     DAT_118ee248;        /* baseline ms */
__declspec(dllimport) int __stdcall QueryPerformanceCounter(__int64 *);
__declspec(dllimport) int __stdcall QueryPerformanceFrequency(__int64 *);
__declspec(dllimport) unsigned long __stdcall timeGetTime(void);

int32_t BrSub10075020(void)
{
    __int64 t;

    if (DAT_100bb2dc != 0) {
        DAT_118ee240 = QueryPerformanceFrequency(&DAT_118ee238);
        QueryPerformanceCounter(&t);
        DAT_118ee248 = (int)((t * 1000 + 500) / DAT_118ee238);
        DAT_100bb2dc = 0;
    }
    if (DAT_118ee240 != 0) {
        if (QueryPerformanceCounter(&t) != 0)
            return (int)((t * 1000 + 500) / DAT_118ee238) - DAT_118ee248;
    }
    return (int)timeGetTime();
}
#else
/* The port twin of 0x10075020; the tag above the #ifdef covers both arms. */
int32_t BrSub10075020(void)
{
    int64_t now;

    if (g_br0BBAD4 != 0) {
        g_br18AB128 = BrPlatQueryPerfFreq(&g_br18AB120);
        now = 0;
        /* The original ignores this call's return value. */
        (void)BrPlatQueryPerfCounter(&now);
        g_br18AB130 = BrPerfToMs(now);
        g_br0BBAD4  = 0;
    }

    if (g_br18AB128 == 0) {
        return (int32_t)BrPlatTimeGetTime();
    }
    now = 0;
    if (BrPlatQueryPerfCounter(&now) == 0) {
        return (int32_t)BrPlatTimeGetTime();
    }
    return BrPerfToMs(now) - g_br18AB130;
}
#endif

/* ---- from slice4_53.c ------------------------------------------------
 * That batch reached the three window/DirectPlay globals through
 * slice4_53.h, with BrCarSub9020 renamed out of the way; kept verbatim.
 * --------------------------------------------------------------------- */
#ifdef BR_MATCHING_BUILD
#define BrCarSub9020 BrCarSub9020_port2
#include "slice4_53.h"
#undef BrCarSub9020
#include <windows.h>

extern int DAT_10ac306c;
extern int DAT_10ac408c;
int FUN_100356b0();
int FUN_10036300();

/* WHAT IT DOES: start the 1-second Windows timer and enable the timer tick state machine. */
/* @implements 0x10035870 glide BrTimerStart */

int BrTimerStart(void)

{
  FUN_100356b0();
  DAT_10ac306c = SetTimer(g_brP680584,1,1000,(TIMERPROC)0x0);
  DAT_10ac408c = 1;
  if (g_brPAA29D4 != 0) {
    FUN_10036300(g_brP277B40);
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */

/* ---- from slice8_86.c ------------------------------------------------
 * Section 10 of that batch: the 30 Hz tick stepper and the four statics
 * only it used.
 *
 * CORRECTION.  This note used to say the neighbours 0x1006E4A0 / 0x10019830
 * could not follow because they read a file-static g_br86HasPerf "which the
 * rest of that batch still uses".  That was wrong: only section 8 of the
 * batch ever touched that flag.  The whole of section 8 -- the flag, its
 * single writer and its readers -- has since moved to br_frametimer.c
 * together, so nothing was duplicated.  It is a separate file rather than
 * this one because slice8_86.h's header chain will not compile alongside
 * the four batches aggregated here (BrDPlayVtbl redefinition, and a
 * timeEndPeriod whose return type disagrees with windows.h's).
 * --------------------------------------------------------------------- */

#ifdef BR_MATCHING_BUILD
static int32_t g_br18AB12C;                    /* 0x118AB12C */
static int32_t g_br0BBAC8[3] = { 33, 33, 34 }; /* 0x100BBAC8 */
static int32_t g_br18AB118;                    /* 0x118AB118 */
static int32_t g_br18AB134;                    /* 0x118AB134 */

/* WHAT IT DOES: walks the game's thirty-tick-a-second clock forward by one
 * tick. A three-step counter picks 33, 33 or 34 milliseconds in turn from a
 * small table, that amount is added to the millisecond clock, and the tick
 * count goes up by one -- three calls add exactly one tenth of a second. */
/* @implements 0x10075150 d3d BrSub10075150 */
void BrSub10075150(void)
{
    if (++g_br18AB12C > 2)              /* `cmp eax,2 / jle` -- signed */
        g_br18AB12C = 0;
    g_br18AB118 += g_br0BBAC8[g_br18AB12C];
    g_br18AB134++;
}
#endif
