/* br_uibounds.c -- menus: the bounds-fits predicate on the +0x10 chain object
 * br_chain.c tears down (0x10058CC0), and the ask-and-discard call next to it
 * (0x10058F90).
 *
 * Filed out of slice5_63.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice5_63.c -- decompiled from BRD3D.dll, pass-63 packet (slice 5).
 *
 * See slice5_63.h for what is here, what is not, and the gotchas.
 *
 * ---------------------------------------------------------------------------
 * WHY slice1_06.h IS NOT INCLUDED
 * ---------------------------------------------------------------------------
 * This file needs slice2_25.h (the option globals, BrOptObj, BrStrGet,
 * BrSub1003F2B0, the lookup tables) AND slice1_06.h (BrOptSave / BrOptAvailB,
 * which are 0x1003E310 and 0x1003F320 already decompiled). Those two headers
 * cannot coexist: both define `BrDPlayVtbl`, with different contents. That
 * collision predates this packet.
 *
 * The five slice1_06 declarations this file needs are therefore repeated
 * below, VERBATIM from slice1_06.h, behind the guard slice1_06.h itself uses,
 * so that if the collision is ever fixed and slice1_06.h ends up included
 * first this file picks up the real declarations instead. No second
 * implementation of either function is created: the definitions in
 * slice1_06.c are the ones that run.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#endif
#include <string.h>

#ifdef BR_MATCHING_BUILD
#define BrExt_1007AC00 BrExt_1007AC00_decl
#endif
#include "slice5_63.h"
#ifdef BR_MATCHING_BUILD
#undef BrExt_1007AC00
#endif

#include "br_crt.h"      /* BrOperatorNew (0x1007DFE0)                       */
#include "slice1_03.h"   /* BrTextGetState, BrHudDrawTimeEntry               */

/* XSLICE 0x1007A940 (Glide 0x10058E20 -- byte-identical, shared.csv `body`)
 *
 * Carried verbatim from slice5_63.c, where this declaration lives in a local
 * cross-slice block rather than in any header. Without it the port arm below
 * calls it implicitly (C4013) and leaves an undefined external -- a link
 * failure match_sweep.py cannot see, because it only compiles the matching
 * configuration. Found by tools/portcheck.py. */
extern int BrSub1007A940(void);

/* 0x10058F90 (Glide) / 0x1007AC00 (D3D)
 *
 * BUILD DIVERGENCE -- A WHOLE GUARD, and the port had the D3D one.
 *
 *     Glide 0x10058F90, 12 bytes, FIVE instructions:
 *         call 0x10058E20 / neg eax / sbb eax,eax / neg eax / ret
 *
 *     D3D   0x1007AC00, 22 bytes:
 *         call 0x1007A840 / test eax,eax / jne +1 / ret
 *         call 0x1007A940 / neg / sbb / neg / ret
 *
 * config/shared.csv pairs 0x1007A940 with 0x10058E20 as `shared`/`body` --
 * byte-identical, so the CALLEE is the same routine in both builds.  The gate
 * is not: 0x1007A840 is class `unknown` with no glide_va, and a scan of
 * BRGlide.dll finds no counterpart of its 244-byte body.  It enumerates
 * display devices (its strings include "%s (Primary)" and it writes the
 * adapter global 0x118AC238), which is exactly the kind of thing the Glide
 * build reaches through glide2x.dll instead of doing itself.
 *
 * So UNDER GLIDE THE BODY ALWAYS RUNS.  The port's `if (... == 0) return;`
 * suppressed it whenever the D3D-only enumerator would have failed -- a gate
 * that does not exist in the reference build, standing in front of the only
 * call that has side effects.  Removed, and BrSub1007A840 with it: nothing
 * else references it and keeping a declaration for a function the reference
 * build does not contain invites it back.
 *
 * The `neg eax / sbb eax,eax / neg eax` tail is `(v != 0)` in both builds and
 * is dropped in both, because the caller declares this void. */
/* WHAT IT DOES: asks one question and throws the answer away, so the only
 * thing it accomplishes is whatever that call does along the way. What the
 * question is has not been established; the purpose is unclear. */
/* @implements 0x10058F90 glide BrExt_1007AC00 */
#ifdef BR_MATCHING_BUILD
/* Glide 0x10058F90 is 12 bytes: CALL 0x10058E20 / NEG EAX / SBB EAX,EAX /
 * NEG EAX / RET.  No BrSub1007A840 gate -- that guard exists only in D3D.
 * `!= 0` keeps EAX alive so VC5 /O2 emits neg/sbb/neg rather than a tail jmp. */
int BrExt_1007AC00(void)
{
    return BrSub1007A940() != 0;
}
#else
void BrExt_1007AC00(void)
{
    /* Kept as a call so the side effects of 0x10058E20 (== D3D 0x1007A940)
     * still happen; the comparison itself is dead.  No 0x1007A840 gate:
     * Glide 0x10058F90 has none. */
    (void)(BrSub1007A940() != 0);
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: thiscall predicate: 1 if `param_2` is non-NULL, this->+0x10 is non-NULL and
 * all three words of param_2 are <= the corresponding words of this->+0x10 (a bounds-fits
 * test). Spelled as __fastcall with an unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x10058CC0 glide BrBoundsFits_10058CC0 */

int __fastcall BrBoundsFits_10058CC0(int param_1,int _edx_unused,int *param_2)
{
  int *piVar1;
  
  if ((((param_2 != (int *)0x0) && (piVar1 = *(int **)(param_1 + 0x10), piVar1 != (int *)0x0)) &&
      (param_2[2] <= piVar1[2])) && ((*param_2 <= *piVar1 && (param_2[1] <= piVar1[1])))) {
    return 1;
  }
  return 0;
}

#endif /* BR_MATCHING_BUILD */
