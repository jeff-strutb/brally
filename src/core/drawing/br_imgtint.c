/* br_imgtint.c -- drawing: the tint an image is drawn with.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice3_40.c, an address batch and not a module.  The state
 * itself is slice1_07's BrImgTintState; this is the setter.
 *
 * slice3_40.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
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

/* 0x10061460 */
/* WHAT IT DOES: sets the three colour multipliers that tint an image as it
 * is drawn. Its caller passes three consecutive bytes out of an opponent
 * car's record, so these are 0-to-255 colour components; what the tint is
 * used for there is not established here. */
/* @implements 0x10061460 d3d BrImgTintSetScale */
void BrImgTintSetScale(int32_t r, int32_t g, int32_t b)
{
    BrImgTintState.scaleR = r;   /* 0x10AA3440 */
    BrImgTintState.scaleG = g;   /* 0x10AA3448 */
    BrImgTintState.scaleB = b;   /* 0x10AA345C */
}

/* 0x1005A280 */
/* WHAT IT DOES: darkens an RGBA image through a second image of the same
 * size, scaling each pixel's red, green and blue by the other image's first
 * channel taken as a 0-to-255 fraction.  The alpha byte is left alone. */
/* @implements 0x1005A280 glide BrImgMulByMask */
void BrImgMulByMask(uint8_t *pPix, int32_t w, int32_t h, const uint8_t *pMask)
{
    unsigned int m;
    uint8_t r, g, b;
    int32_t n;

    n = h * w;
    if (n > 0) {
        do {
            m = *pMask;
            r = (uint8_t)((m * pPix[0]) / 0xFF);
            g = (uint8_t)((m * pPix[1]) / 0xFF);
            b = (uint8_t)((m * pPix[2]) / 0xFF);
            pPix[0] = r;
            pPix[1] = g;
            pPix[2] = b;
            pPix  += 4;
            pMask += 4;
        } while (--n != 0);
    }
}

/* 0x10AC67AC -- the tint textures, one pointer per index, each a 64x64
 * RGBA image whose first channel is the multiplier. */
extern const uint8_t *g_apBrImgTintTex[];

/* 0x1005A300 */
/* WHAT IT DOES: darkens an RGBA image through one of the stock 64-by-64
 * tint textures, stretched to the image: each pixel's red, green and blue
 * are scaled by the texture's first channel at the matching fraction of
 * the way across and down.  A missing texture or an empty image does
 * nothing.  Alpha is left alone.
 *
 * PARKED 2026-09-04 at 287/286 B, 97/97 insns, regnorm 3+3.  Three
 * residues: (1) the original keeps the height in edi (a `push edi` before
 * the texture-table load, so both early exits pop it) where this build
 * uses ecx; (2) the original's inner pointer starts at the row pointer and
 * reads [ecx-4..-2] after its `add ecx,4`, this build biases it by +2 at
 * loop entry and reads [ecx-6..-4]; (3) the tail's `add ecx,ebp` is a lea
 * here.  Dead probes: stride folded into the tail (frame one slot short,
 * KEEP the named stride); `rows = h` copied before the test (spills the
 * counter at entry, worse).  Sibling BrImgMulByMask above went byte-exact
 * with the same loop body, so the levers are in the outer loop. */
/* @implements 0x1005A300 glide BrImgMulByTexture */
void BrImgMulByTexture(int32_t iTex, uint8_t *pPix, int32_t w, int32_t h)
{
    const uint8_t *pTex;
    int32_t  yAcc, rows, stride;
    int32_t  xAcc, tx, ty;
    int32_t  t;
    uint8_t *p;

    pTex = g_apBrImgTintTex[iTex];
    if (pTex != NULL && h > 0) {
        stride = w * 4;
        yAcc = 0;
        rows = h;
        do {
            if (w > 0) {
                int32_t n = w;

                ty = (yAcc / h) * 64;
                xAcc = 0;
                p = pPix;
                do {
                    tx = xAcc / w;
                    xAcc += 64;
                    t = pTex[(ty + tx) * 4];
                    p[0] = (uint8_t)((t * p[0]) / 255);
                    p[1] = (uint8_t)((t * p[1]) / 255);
                    p[2] = (uint8_t)((t * p[2]) / 255);
                    p += 4;
                } while (--n != 0);
            }
            pPix += stride;
            yAcc += 64;
        } while (--rows != 0);
    }
}
