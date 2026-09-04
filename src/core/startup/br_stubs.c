/* br_stubs.c -- startup: the one-line stubs, nops and global accessors.
 *
 * Filed out of the address batches.  Each section keeps the declarations the
 * batch it came from made locally, so the compiler's view of each body is
 * unchanged.  These are whole original functions, not placeholders: the
 * shipped code really is this small.
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)

/* ---- from slice2_12.c ---------------------------------------------- */

extern int g_br094294;

/* WHAT IT DOES: return the value of the global at g_br094294. */
/* @implements 0x100060A0 glide BrGetGlobal_94294 */

int BrGetGlobal_94294(void)

{
  return g_br094294;
}

/* ---- from slice2_18.c ---------------------------------------------- */

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002CB3F glide BrNop_1002CB3F */

void BrNop_1002CB3F(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002CB44 glide BrNop_1002CB44 */

void BrNop_1002CB44(void)

{
  return;
}

/* ---- from slice2_19.c ---------------------------------------------- */

extern int DAT_106e8a1c;
extern int DAT_106e8698;

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E136 glide BrNop_1002E136 */

void BrNop_1002E136(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E2DE glide BrNop_1002E2DE */

void BrNop_1002E2DE(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E2E3 glide BrNop_1002E2E3 */

void BrNop_1002E2E3(void)

{
  return;
}

/* WHAT IT DOES: store the argument into the global at 0x106E8A1C. */
/* @implements 0x1002E2E8 glide BrSet_106E8A1C */

void BrSet_106E8A1C(int param_1)

{
  DAT_106e8a1c = param_1;
  return;
}

/* WHAT IT DOES: store the argument into the global at 0x106E8698. */
/* @implements 0x1002E2F5 glide BrSet_106E8698 */

void BrSet_106E8698(int param_1)

{
  DAT_106e8698 = param_1;
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002EBCC glide BrNop_1002EBCC */

void BrNop_1002EBCC(void)

{
  return;
}

/* ---- from slice6_72.c ---------------------------------------------- */

_CRTIMP void __cdecl _except_handler3(void);

/* WHAT IT DOES: the compiler's structured-exception entry thunk -- it just
 * tail-calls the CRT handler. Not game code. */
/* @implements 0x10074AE6 glide FUN_10074ae6 */
/* auto-filed from ghidra --refine; transforms: as-is */

void
FUN_10074ae6(void)
{
    _except_handler3();
}

#endif /* BR_MATCHING_BUILD */

/* ---- from slice2_17.c ------------------------------------------------
 * Two members of that batch's 0x1002A840..0x1002A957 run -- one original
 * translation unit by every sign (the addresses are contiguous and the
 * whole run matches at /Od while the rest of the batch matches at /O2).
 *
 * Only these two came across.  Their four neighbours in the same run --
 * 0x1002A840 BrScratchRingAlloc, 0x1002A894 BrScratchRingDrain,
 * 0x1002A8D7 BrRenderCountersReset and 0x1002A93C BrScreenSizeApply --
 * all reach slice2_17.c's file-static g_s17, which is a decomp-invented
 * aggregate with 136 references across 72 functions in that batch and so
 * cannot travel.  These two touch no file-static in either build arm:
 * BrScratchRingNull reads only its own parameters, and BrScreenSizeInit
 * only calls its neighbour through slice2_17.h's declaration.
 *
 * The batch's preamble is carried verbatim, per this file's convention.
 * --------------------------------------------------------------------- */

#ifdef BR_MATCHING_BUILD
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_17.h"
#ifdef BR_MATCHING_BUILD
#undef BrPtrListContains
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

/* 0x10031212 -- DEVIATION: the original zeroes its own two argument slots
 * on the caller's stack. C parameters are by value, so the stores are not
 * observable and are omitted; the return value is what callers use. */
/* WHAT IT DOES: nothing. It takes two arguments, ignores both, and always
 * answers zero -- a placeholder that fits where a real routine would go. */
/* @implements 0x10031212 d3d BrScratchRingNull */
/* @n64 0x802173B8 exact */
int BrScratchRingNull(int a0, int a1)
{
    /* The original really does store zero into both of its OWN argument
     * slots (a1 first) before returning 0 -- kept for the byte match; C
     * parameters are by value so nothing observable changes. */
    a1 = 0;
    a0 = 0;
    return 0;
}

/* 0x10031282 */
/* WHAT IT DOES: sets the screen dimensions up at start-up, by doing exactly
 * what the routine below does and nothing else. */
/* @implements 0x1002A932 glide BrScreenSizeInit */
void BrScreenSizeInit(void)
{
    BrScreenSizeApply();
}
