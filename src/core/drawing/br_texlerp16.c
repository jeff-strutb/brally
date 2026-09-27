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

/* One channel: lerp along s on both rows (u, v), then along t, round to
 * nearest (floor of x + 0.5: the constant is -0.5, subtracted), clamp,
 * truncate.  u and v are named locals: VC5 keeps them on the x87 stack and
 * pops each dead corner value right after its last use, as the original
 * does (as one expression the row lerps are temporaries and the dead
 * corner lingers to the end of the statement). */
#define BR_TEX_LERP(dst, x00, x10, x01, x11, hi)                              \
    u = ((x10) - (x00)) * s + (x00);                                          \
    v = ((x11) - (x01)) * s + (x01);                                          \
    f = (float)floor((double)((v - u) * t + u - DAT_10077434));              \
    if (f < DAT_10077438)                                                     \
        f = DAT_10077438;                                                     \
    if (f > (hi))                                                             \
        f = (hi);                                                             \
    (dst) = (int)f

/* WHAT IT DOES: bilinear-interpolate one ARGB1555 texel from four corners
 * at (s, t).  Each channel is rounded to nearest and clamped -- alpha to
 * 0..1, RGB to 0..31 -- then packed back into 16 bits. */
/* T2 (2026-09-27 re-transcription, raw 701 -> 373 B, size 847/843, REGNORM
 * 2+0).  The original unpacks all sixteen channel values to floats first,
 * corner by corner (one reused 16-bit `c`), then lerps channel by channel;
 * the four alpha floats stay on the x87 stack, the other twelve get homes
 * (the dead pointer-argument slots among them).  Identical to the original
 * through +0x152.  RESIDUE: two extra fxch where the alpha tail's `fmul t`
 * and the fild of r11 are scheduled the other way round; the r/g/b lerps
 * are identical, shifted 4 bytes.  Inert: every order and spelling of the
 * lerp terms, the c11 conversion spellings and placement, declaration
 * order, `register`, /TP, TU pads and headers, the predecessor TU
 * (br_texlerp.c) in front.  Unpacking c11 as a, b, g, r gives size-exact
 * 114 B but mis-orders the integer unpack. */
/* @implements 0x10024750 glide BrTexLerp1555 */
void BrTexLerp1555(unsigned short *pOut,
                   unsigned short *p00, unsigned short *p10,
                   unsigned short *p01, unsigned short *p11,
                   float s, float t)
{
    unsigned short c;
    float a00, r00, g00, b00, a10, r10, g10, b10, a01, r01, g01, b01, a11, r11, g11, b11;
    int a, r, g, b;
    float f, u, v;

    c = *p00;
    a00 = c >> 15;
    r00 = (c >> 10) & 0x1f;
    g00 = (c >> 5) & 0x1f;
    b00 = c & 0x1f;
    c = *p10;
    a10 = c >> 15;
    r10 = (c >> 10) & 0x1f;
    g10 = (c >> 5) & 0x1f;
    b10 = c & 0x1f;
    c = *p01;
    a01 = c >> 15;
    r01 = (c >> 10) & 0x1f;
    g01 = (c >> 5) & 0x1f;
    b01 = c & 0x1f;
    c = *p11;
    a11 = c >> 15;
    r11 = (c >> 10) & 0x1f;
    g11 = (c >> 5) & 0x1f;
    b11 = c & 0x1f;
    BR_TEX_LERP(a, a00, a10, a01, a11, DAT_10077440);
    BR_TEX_LERP(r, r00, r10, r01, r11, DAT_10077444);
    BR_TEX_LERP(g, g00, g10, g01, g11, DAT_10077444);
    BR_TEX_LERP(b, b00, b10, b01, b11, DAT_10077444);
    *pOut = (unsigned short)((((a << 5 | r) << 5 | g) << 5) | b);
}

/* WHAT IT DOES: bilinear-interpolate one ARGB4444 texel from four corners
 * at (s, t).  Each 4-bit channel is rounded to nearest and clamped to
 * 0..15, then packed back into 16 bits. */
/* T2 (2026-09-27 re-transcription, raw 718 -> 373 B, size 843/839, REGNORM
 * 2+0): the same shape and the same residue as BrTexLerp1555 above. */
/* @implements 0x10024AA0 glide BrTexLerp4444 */
void BrTexLerp4444(unsigned short *pOut,
                   unsigned short *p00, unsigned short *p10,
                   unsigned short *p01, unsigned short *p11,
                   float s, float t)
{
    unsigned short c;
    float a00, r00, g00, b00, a10, r10, g10, b10, a01, r01, g01, b01, a11, r11, g11, b11;
    float f, u, v;
    int a, r, g, b;

    c = *p00;
    a00 = c >> 12;
    r00 = (c >> 8) & 0xf;
    g00 = (c >> 4) & 0xf;
    b00 = c & 0xf;
    c = *p10;
    a10 = c >> 12;
    r10 = (c >> 8) & 0xf;
    g10 = (c >> 4) & 0xf;
    b10 = c & 0xf;
    c = *p01;
    a01 = c >> 12;
    r01 = (c >> 8) & 0xf;
    g01 = (c >> 4) & 0xf;
    b01 = c & 0xf;
    c = *p11;
    a11 = c >> 12;
    r11 = (c >> 8) & 0xf;
    g11 = (c >> 4) & 0xf;
    b11 = c & 0xf;
    BR_TEX_LERP(a, a00, a10, a01, a11, DAT_10077448);
    BR_TEX_LERP(r, r00, r10, r01, r11, DAT_10077448);
    BR_TEX_LERP(g, g00, g10, g01, g11, DAT_10077448);
    BR_TEX_LERP(b, b00, b10, b01, b11, DAT_10077448);
    *pOut = (unsigned short)((((a << 4 | r) << 4 | g) << 4) | b);
}

#endif /* BR_MATCHING_BUILD */
