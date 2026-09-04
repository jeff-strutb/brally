/* Auto-generated from Ghidra decompilation — 0x1006E360 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
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
