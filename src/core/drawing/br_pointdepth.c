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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "slice2_18.h"

#ifdef BR_MATCHING_BUILD
extern int   DAT_106ed6a8;          /* 0x106ED6A8  depth cue enabled      */
extern int   DAT_106e9d84;          /* 0x106E9D84  depth scale (int)      */
extern int   DAT_106e86a8;          /* 0x106E86A8  depth bias (int)       */
extern float DAT_106e9a38[16];      /* 0x106E9A38  the view matrix        */

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

    if (DAT_106ed6a8 == 0) {
        return 0.0f;
    }
    BrMat4TransformPoint4(v, pV, DAT_106e9a38);
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
#endif /* BR_MATCHING_BUILD */
