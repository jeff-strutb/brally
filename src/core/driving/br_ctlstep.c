/* br_ctlstep.c -- driving: the two per-car controller steps.
 *
 * Filed out of the address batch slice3_40.c, whose preamble this file
 * carries verbatim.  These are the routines installed in a car's controller
 * slot -- one for a human at the wheel, one for a computer opponent -- so
 * they belong with how a car behaves rather than with the option sliders and
 * path walk that sat either side of them in the original packet.
 *
 * See slice3_40.h for the offset maps and the per-function gotchas.
 *
 * x87 NOTE (applies throughout).  The original is MSVC x87 code and the CRT
 * leaves the precision control at 53 bits, so an intermediate the original
 * never spills carries double precision, not float.  Wherever that is
 * observable the intermediate is a `double` here and the rounding to float
 * happens exactly where the original has an `fstp dword`.  Where the
 * original stores every intermediate, plain float is used.
 */

#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
void BrZeroRegions(void);
#endif

#include "br_match.h"    /* BR_THISCALL1 */

/* The two per-car controller entry points. Both are the same nine bytes --
 * `mov ecx,[esp+4]` then a `jmp` -- which is what VC5 emits for a cdecl
 * one-liner that tail-calls a thiscall function: the argument moves into ecx
 * and the frame is never built. Their bodies (0x1005D770 and 0x1005C8B0) are
 * not ported. */
void BR_THISCALL1 BrCtlHumanBody(BrCar *pCar);   /* 0x1005C8B0 */
void BR_THISCALL1 BrCtlAiBody(BrCar *pCar);      /* 0x1005D770 */

/* WHAT IT DOES: drive one car from the player's controls for this frame. This
 * is the routine installed in a car's controller slot when a human is at the
 * wheel; its AI counterpart is directly below. */
/* @implements 0x1005D050 glide BrCtlHuman */
void BrCtlHuman(BrCar *pCar)
{
    BrCtlHumanBody(pCar);
}

/* WHAT IT DOES: drive one car from the computer opponent's logic for this
 * frame -- the routine every slot without a human in it gets. */
/* @implements 0x1005E690 glide BrCtlAi */
void BrCtlAi(BrCar *pCar)
{
    BrCtlAiBody(pCar);
}
