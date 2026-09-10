/* br_texlerp16.c -- drawing: bilinear texel sample, packed 16-bit formats.
 *
 * Siblings of BrTexLerpU8 (br_texlerp.c).  Kept in a separate TU so the
 * U8 interpolator's T3 object does not move.  Ghidra drops the post-floor
 * clamp; orig saturates each channel before __ftol.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <math.h>

#ifdef BR_MATCHING_BUILD

extern float DAT_10077434;   /* -0.5f */
extern float DAT_10077438;   /*  0.0f */
extern float DAT_10077440;   /*  1.0f  -- 1-bit alpha */
extern float DAT_10077444;   /* 31.0f  -- 5-bit RGB */
extern float DAT_10077448;   /* 15.0f  -- 4-bit ARGB4444 */

#define BR_TEX_LERP_CHAN(dst, a00, a10, a01, a11, s, t, lo, hi)               \
    do {                                                                      \
        float _u, _v, _r;                                                     \
        _u = ((a10) - (a00)) * (s) + (a00);                                   \
        _v = ((a11) - (a01)) * (s) + (a01);                                   \
        _r = (float)floor((double)((_v - _u) * (t) + _u - DAT_10077434));     \
        if (_r < (lo))                                                        \
            _r = (lo);                                                        \
        if (_r > (hi))                                                        \
            _r = (hi);                                                        \
        (dst) = (int)_r;                                                      \
    } while (0)

/* WHAT IT DOES: bilinear-interpolate one ARGB1555 texel from four corners
 * at (s, t).  Each channel is rounded to nearest and clamped -- alpha to
 * 0..1, RGB to 0..31 -- then packed back into 16 bits. */
/* @implements 0x10024750 glide BrTexLerp1555 */
void BrTexLerp1555(unsigned short *pOut,
                   unsigned short *p00, unsigned short *p10,
                   unsigned short *p01, unsigned short *p11,
                   float s, float t)
{
    unsigned short c00, c10, c01, c11;
    int a, r, g, b;

    c00 = *p00; c10 = *p10; c01 = *p01; c11 = *p11;
    BR_TEX_LERP_CHAN(a,
        (float)(c00 >> 15), (float)(c10 >> 15),
        (float)(c01 >> 15), (float)(c11 >> 15),
        s, t, DAT_10077438, DAT_10077440);
    BR_TEX_LERP_CHAN(r,
        (float)((c00 >> 10) & 0x1f), (float)((c10 >> 10) & 0x1f),
        (float)((c01 >> 10) & 0x1f), (float)((c11 >> 10) & 0x1f),
        s, t, DAT_10077438, DAT_10077444);
    BR_TEX_LERP_CHAN(g,
        (float)((c00 >> 5) & 0x1f), (float)((c10 >> 5) & 0x1f),
        (float)((c01 >> 5) & 0x1f), (float)((c11 >> 5) & 0x1f),
        s, t, DAT_10077438, DAT_10077444);
    BR_TEX_LERP_CHAN(b,
        (float)(c00 & 0x1f), (float)(c10 & 0x1f),
        (float)(c01 & 0x1f), (float)(c11 & 0x1f),
        s, t, DAT_10077438, DAT_10077444);
    *pOut = (unsigned short)((((a << 5 | r) << 5 | g) << 5) | b);
}

/* WHAT IT DOES: bilinear-interpolate one ARGB4444 texel from four corners
 * at (s, t).  Each 4-bit channel is rounded to nearest and clamped to
 * 0..15, then packed back into 16 bits. */
/* @implements 0x10024AA0 glide BrTexLerp4444 */
void BrTexLerp4444(unsigned short *pOut,
                   unsigned short *p00, unsigned short *p10,
                   unsigned short *p01, unsigned short *p11,
                   float s, float t)
{
    unsigned short c00, c10, c01, c11;
    int a, r, g, b;

    c00 = *p00; c10 = *p10; c01 = *p01; c11 = *p11;
    BR_TEX_LERP_CHAN(a,
        (float)(c00 >> 12), (float)(c10 >> 12),
        (float)(c01 >> 12), (float)(c11 >> 12),
        s, t, DAT_10077438, DAT_10077448);
    BR_TEX_LERP_CHAN(r,
        (float)((c00 >> 8) & 0xf), (float)((c10 >> 8) & 0xf),
        (float)((c01 >> 8) & 0xf), (float)((c11 >> 8) & 0xf),
        s, t, DAT_10077438, DAT_10077448);
    BR_TEX_LERP_CHAN(g,
        (float)((c00 >> 4) & 0xf), (float)((c10 >> 4) & 0xf),
        (float)((c01 >> 4) & 0xf), (float)((c11 >> 4) & 0xf),
        s, t, DAT_10077438, DAT_10077448);
    BR_TEX_LERP_CHAN(b,
        (float)(c00 & 0xf), (float)(c10 & 0xf),
        (float)(c01 & 0xf), (float)(c11 & 0xf),
        s, t, DAT_10077438, DAT_10077448);
    *pOut = (unsigned short)((((a << 4 | r) << 4 | g) << 4) | b);
}

#endif /* BR_MATCHING_BUILD */
