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


#ifdef BR_MATCHING_BUILD
extern int g_br86HasPerf_S_S1437;
int BrSub10075020();
extern int g_br86HasPerf_S_S1437;

/* WHAT IT DOES: restart a stopwatch from now. Uses the high-resolution
 * performance counter when the machine has one and a coarser millisecond
 * clock when it does not, writing to a different pair of fields in each case
 * -- so the two halves of this record are alternatives, not both live. */
/* @implements 0x1006E3F0 glide br86_timer_restart */
void __fastcall br86_timer_restart(int *param_1)

{
  int uVar1;
  
  if (g_br86HasPerf_S_S1437 != 0) {
    QueryPerformanceCounter((LARGE_INTEGER *)(param_1 + 2));
    param_1[4] = *param_1;
    param_1[5] = param_1[1];
    return;
  }
  uVar1 = BrSub10075020();
  param_1[7] = uVar1;
  param_1[8] = param_1[6];
  return;
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

#ifdef BR_MATCHING_BUILD
extern int BrSub10075020(void);
extern unsigned int DAT_118ee230;
extern unsigned int DAT_118ee244;
extern unsigned int DAT_118ee24c;

/* The slice1_09.c body takes `(BrTimeState *pState, unsigned int ms)`; the
 * original takes NEITHER.  It reads nothing off the stack: the millisecond
 * count is the return of 0x1006E280 (BrSub10075020, already matched in
 * slice4_50.c) and the three destinations are absolute globals -- the
 * "state-pointer argument that the original never loads is absolute globals"
 * idiom, the same split BrFadeRelease and BrFadeLatch use.
 *
 * The division is spelled exactly as slice1_09.c has it.  VC5 emits BOTH a
 * real `div` by 100 -- whose remainder is what `% 100` wants -- and a
 * separate magic multiply by 0x51EB851F for the `/ 100` quotient, rather than
 * reusing the quotient the `div` already produced; the second magic multiply
 * by 0x3E0F83E1 is the `/ 33`.
 *
 * RESIDUE 45 bytes.  Size, instruction count and the register-blind
 * instruction multiset are all exact (68/68, REGNORM gap 0+0): the original
 * schedules the `div` and the `/ 33` FIRST and the `* 3` second, and holds
 * the divisor 100 in the callee-saved esi; the recompile emits the `* 3`
 * first and puts 100 in ecx.  Probed and ruled out, do not re-run -- all
 * BYTE-IDENTICAL to what is here: every order of the two summands
 * (`(ms/100)*3` first, `3 * (ms/100)` either way), naming the remainder
 * and/or the quotient as locals, naming the whole `/33` term, storing ms
 * before the zero and after the tick, dropping the `ms` local and re-reading
 * the global at all three uses, and /Oy- /Op /Ox /Og-/Ot (all 68/45) plus
 * /Od (84/54) and /O1 (61/43).  T3a. */
/* WHAT IT DOES: sample the clock and update the frame timing -- how long the
 * last frame took and the running total. Called once per frame, and
 * everything time-based reads what it leaves behind. */
/* @t4-pass 0x1006E360 1 2026-09-07 probes 24 bytes 68 insns 20 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x1006E360 2 2026-09-07 probes 24 bytes 68 insns 20 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x1006E360 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 68/68 insns 20/20 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is scheduling and register colouring only: the original orders the
 * div/`\/33` before the `*3` and holds 100 in esi, the recompile the reverse
 * with 100 in ecx; identical register-blind multiset (rows 0+0 after the
 * bare-decimal branch-target normaliser fix, 2d5a93d), 1 masked region.
 * Dossier and dead-probe list in the block above (T3a note); two counted
 * zero-movement @t4-pass lines from the crank ledger.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1006E360 glide BrTimeUpdate */
void BrTimeUpdate(void)

{
  unsigned int ms;

  ms = (unsigned int)BrSub10075020();
  DAT_118ee244 = 0;
  DAT_118ee230 = ms;
  DAT_118ee24c = (ms % 100) / 33 + (ms / 100) * 3;
}
#endif /* BR_MATCHING_BUILD */
