/* br_phaseact.h -- the activate body every phase routine inlines, once.
 *
 * WHY THIS IS ITS OWN MODULE
 *
 * Twenty-one "activate the Nth screen" routines across slice2_26.c and
 * slice3_31.c share one instruction sequence, differing only in WHICH
 * singleton slot they fill and WHICH enter hook they install.  Both files
 * transcribed it, independently, and the two copies DISAGREED: slice2_26's
 * wrapped the two flag stores in a NULL test and called that a deviation for
 * memory safety, and slice3_31's stored unconditionally, as the original
 * does.  Neither file could see the other's copy by grepping its own names.
 *
 * Putting it here rather than in either file is not tidiness: slice2_26.c
 * pulls in a dozen enter hooks and the slot table, so slice3_31's suite
 * cannot link it, and slice3_31.c is the same in reverse.  A leaf both can
 * link is the only shape that leaves exactly one body.
 *
 * THE SEQUENCE, from BRD3D 0x10045520 (and 0x10045900, which is the same
 * bytes with a different slot and hook):
 *
 *   10045536  mov eax,[SLOT] ; test eax,eax ; jne existing
 *   1004553F  push 0xC8 ; call operator new (0x1007DFE0 -- does NOT zero)
 *   1004555E  call the constructor 0x10048710 on it
 *   10045571  mov [SLOT],eax ; mov [0x10AA2904],eax     <- BOTH written even
 *                                                          when eax is NULL
 *   1004557B  jne ...  -> a NULL allocation returns here with 0
 *   1004558C  mov [eax+4], HOOK
 *   10045593  mov eax,[SLOT] ; push eax ; call [eax+4]  <- re-reads the SLOT
 *   1004559C  mov eax,[0x10AA2904] ; mov [eax+0xC],1    <- re-reads AGAIN
 *   100455AB  mov ecx,[0x10AA2904] ; mov [ecx+0x68],1   <- and AGAIN
 *
 * The three re-reads are the interesting part and are preserved: an enter
 * hook that repoints the current-phase global stamps the two flags on
 * whatever it moved to, not on the object just built.  There is no test
 * between 0x1004559C and the store, so a hook that CLEARS it faults -- which
 * is the behaviour, not an oversight.
 */
#ifndef BR_PHASEACT_H
#define BR_PHASEACT_H

#include "slice2_26.h"   /* BrPhaseCtx, BrPhase, BrPhaseEnterFn */

/* Which of the three things happened.  The callers need all three because a
 * per-phase epilogue may run only on the just-built path. */
typedef enum BrActResult {
    BR_ACT_FAILED   = 0,   /* operator new returned NULL; caller returns 0   */
    BR_ACT_EXISTING = 1,   /* the singleton was already built                */
    BR_ACT_CREATED  = 2    /* built here; the enter hook has run             */
} BrActResult;

BrActResult BrPhaseActivateSlot(BrPhaseCtx     *pCtx,
                                BrPhase       **ppSlot,
                                BrPhaseEnterFn  pfnEnter);

#endif /* BR_PHASEACT_H */
