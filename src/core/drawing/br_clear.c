/* br_clear.c -- drawing: per-frame state that gets reset or read at the top
 * of a frame.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 *
 * 0x104AB4F0 and 0x104AB504 are a pair -- the same Ghidra batch declared
 * both together -- and the scene accumulators the third function zeroes are
 * cleared at the same point in the frame.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include "slice2_15.h"   /* g_4B16A0, g_4B16AC */

#ifdef BR_MATCHING_BUILD

extern int DAT_104ab4f0;
extern int DAT_104ab504;

/* WHAT IT DOES: clear a one-shot flag (zeroes it if set, idempotent). */
/* @implements 0x10013F00 glide BrClearFlag_AB504 */

int BrClearFlag_AB504(void)

{
  if (DAT_104ab504 != 0) {
    DAT_104ab504 = 0;
  }
  return;
}

/* WHAT IT DOES: return the value of a global flag at 0x104AB4F0. */
/* @implements 0x10013FC0 glide BrGetFlag_AB4F0 */

int BrGetFlag_AB4F0(void)

{
  return DAT_104ab4f0;
}

#endif /* BR_MATCHING_BUILD */

/* =====================================================================
 * 0x10017F60
 * ===================================================================== */
/* WHAT IT DOES: clears the two per-frame scene accumulators (g_4B16A0 and
 * g_4B16AC) back to zero.  The per-frame race render calls it once at the very
 * top of the frame -- right after BrSceneSetupFrame lays the background and just
 * before BrSpanBuildHull -- so the geometry pass (0x1000BEB0) accumulates into a
 * clean slate every frame.  Body is exactly two stores of 0.0f and a return.
 *
 * SOURCE: transcribed from the Glide build (asm/10010000.asm).  The D3D twin is
 * 0x1002AEF0 (shared, same 21 bytes) but its body sits in a run the D3D dump
 * folded into padding, so it is not the transcription source. */
/* @implements 0x10017F60 glide BrSceneAccumReset */
/* @n64 0x802237B4 located */
void BrSceneAccumReset(void)
{
    g_4B16AC = 0.0f;
    g_4B16A0 = 0.0f;
}
