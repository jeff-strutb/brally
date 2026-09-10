/* br_texlerp.c -- drawing: bilinear texel sample.
 *
 * The rasteriser at 0x10024490 dispatches per-pixel to one of three
 * interpolators by texel format.  This file is that family.  Ghidra's
 * drafts drop the post-floor clamp against 0x10077438 / 0x1007743c
 * (0.0f and 255.0f); the original always saturates before __ftol.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <math.h>

#ifdef BR_MATCHING_BUILD

extern float DAT_10077434;   /* -0.5f -- floor(x - (-0.5)) == round-nearest */
extern float DAT_10077438;   /*  0.0f */
extern float DAT_1007743c;   /*  255.0f */

/* WHAT IT DOES: bilinear-interpolate one 8-bit texel from four corner
 * samples at (s, t), round to nearest with floor(x + 0.5), and clamp into
 * 0..255 so an out-of-range blend saturates rather than wrapping. */
/* @implements 0x10024680 glide BrTexLerpU8 */
void BrTexLerpU8(unsigned char *pOut,
                 unsigned char *p00, unsigned char *p10,
                 unsigned char *p01, unsigned char *p11,
                 float s, float t)
{
    float u, v, r;

    u = ((float)*p10 - (float)*p00) * s + (float)*p00;
    v = ((float)*p11 - (float)*p01) * s + (float)*p01;
    r = (float)floor((double)((v - u) * t + u - DAT_10077434));
    if (r < DAT_10077438)
        r = DAT_10077438;
    if (r > DAT_1007743c)
        r = DAT_1007743c;
    *pOut = (unsigned char)(int)r;
}

#endif /* BR_MATCHING_BUILD */
