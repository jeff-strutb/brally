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
/* @t4-pass 0x1005A300 1 2026-09-07 probes 77 bytes 291 insns 98 regions 4 rows 5 census yes  (tools/crank.py) */
/* @t4-pass 0x1005A300 2 2026-09-07 probes 71 bytes 291 insns 98 regions 4 rows 5 census yes  (tools/crank.py) */
/* @t4-pass 0x1005A300 3 2026-09-10 probes 40 bytes 282 insns 96 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x1005A300 4 2026-09-10 probes 40 bytes 282 insns 96 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t3 0x1005A300 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 282/286 insns 96/97 rows 1+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * residue after tools/crank.py: 40 compiles this pass, levers accepted: mut:addr_taken:stride > mut:reorder_stmts;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
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
                    /* THE CURSOR IS BUMPED FIRST and the three channels are
                     * read back through negative displacements: the original
                     * walks at -4/-3/-2, where the bump-last spelling anchors
                     * ours three bytes earlier (-6/-5) and costs an extra
                     * `add R,2` (register-blind 2+2 -> 0+1). */
                    tx = xAcc / w;
                    xAcc += 64;
                    t = pTex[(ty + tx) * 4];
                    p += 4;
                    p[-4] = (uint8_t)((t * p[-4]) / 255);
                    p[-3] = (uint8_t)((t * p[-3]) / 255);
                    p[-2] = (uint8_t)((t * p[-2]) / 255);
                } while (--n != 0);
            }
            pPix += stride;
            yAcc += 64;
        } while (--rows != 0);
    }
}

#ifdef BR_MATCHING_BUILD
/* 0x1005A500 (D3D twin 0x10061480, port body BrImgTintBlit in slice1_07.c) */
/* WHAT IT DOES: copies a rectangle of RGBA pixels into a bottom-up
 * destination image, tinting the colour-keyed ones on the way. The source
 * is packed at the rectangle's own width; source row y lands on destination
 * row dstH - top - y - 1 starting at column left. Every pixel is copied
 * whole first; a keyed pixel (red 0, green equal to blue) then has its red,
 * green and blue replaced by that grey level times the current tint scale
 * over 255, keeping its alpha. A null source does nothing. Always returns 1.
 */
/* Transcribed from the Glide bytes: the pixel is copied as a dword before
 * the key test re-reads byte 0; the three channels share one grey value and
 * divide signed by 255 (the 0x80808081 sequence); scales are the tint
 * state's +0x00/+0x08/+0x1C words.
 * T2, 304/297 B, REGNORM 2+1 (2026-09-25). Load-bearing, each proven by the
 * diff: the row loop is a while with y++ before the pointer advance (a for
 * loop lets VC5 strength-reduce the destination row, +46 B); the row pointer
 * is its own local (so y packs into pSrc's slot); the key test compares two
 * int temporaries. Residue: the original forms the destination offset as
 * (pDst + row*4) - s where this build gets (row*4 - s) + pDst (one extra
 * instruction), and it pushes edi only after the row-count guard. Dead:
 * five destination spellings, dword-typed pointers, inline-indexed
 * destination, 86 declaration orders, pad-count 1..40 (two states only). */
/* @implements 0x1005A500 glide FUN_1005a500 */
int FUN_1005a500(const uint8_t *pSrc, int32_t left, int32_t right,
                 int32_t top, int32_t bottom, uint8_t *pDst, int32_t dstW,
                 int32_t dstH)
{
    const uint8_t *s, *p;
    uint8_t *d;
    int32_t rb, w, h, x, y, g, b;

    if (pSrc != NULL) {
        w = right - left;
        h = bottom - top;
        p = pSrc;
        y = 0;
        while (y < h) {
            rb = w * 4;
            s = p;
            d = pDst + ((dstH - top - y - 1) * dstW + left) * 4;
            for (x = 0; x < w; x++) {
                *(uint32_t *)d = *(const uint32_t *)s;
                if (s[0] == 0) {
                    g = s[1];
                    b = s[2];
                    if (g == b) {
                        d[0] = (uint8_t)(g * BrImgTintState.scaleR / 255);
                        d[1] = (uint8_t)(g * BrImgTintState.scaleG / 255);
                        d[2] = (uint8_t)(g * BrImgTintState.scaleB / 255);
                    }
                }
                s += 4;
                d += 4;
            }
            y++;
            p += rb;
        }
    }
    return 1;
}
#endif /* BR_MATCHING_BUILD */
