/* br_pointdepth.c -- drawing: how deep into the view a world point sits.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * One function, 0x1002B3F0, sitting in the /Od stretch of the image between
 * BrGfxFillRect (0x1002AD39) and BrFrameBeginDl (0x1002B997): it carries the
 * `push ebp / mov ebp,esp` frame and the stored-then-reloaded float temps
 * that stretch is compiled with, so it gets a translation unit of its own
 * rather than a home in an /O2 module.  br_framebegin.c's preamble is
 * carried over verbatim.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_mat.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "slice2_18.h"

/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x106ED6A8  depth cue enabled      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x106E9D84  depth scale (int)      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x106E86A8  depth bias (int)       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x106E9A38  the view matrix        */

/* WHAT IT DOES: answers how far into the scene a world point is, as a
 * fraction from 0 (nearest) to 1 (farthest): the point goes through the
 * view matrix, its depth is divided by its w, scaled and offset by two
 * integer tuning globals and brought down from a 0..255 range, then
 * clamped to 0..1.  With the depth cue switched off it is always 0. */
/* @implements 0x1002B3F0 glide BrPointDepthFrac */
float BrPointDepthFrac(const BrVec3 *pV)
{
    /* /Od homes locals in DECLARATION order from the bottom of the frame up:
     * `f` first lands it at ebp-0x14 and the array at ebp-0x10..-0x1, which
     * is the original's layout.  The other order costs 9 diff bytes. */
    float f;
    float v[4];

    if ((*(int *)((char *)&g_aBrEntRecs + 0x78)) == 0) {
        return 0.0f;
    }
    BrMat4TransformPoint4(v, pV, (*(float (*)[16])&g_BrCurMat));
    v[2] = v[2] / v[3];
    f = (v[2] * (float)DAT_106e9d84 + (float)DAT_106e86a8) * (1.0f / 255.0f);
    if (f < 0.0f) {
        return 0.0f;
    }
    if (f > 1.0f) {
        return 1.0f;
    }
    return f;
}
