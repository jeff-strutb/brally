/* br_surf.c -- see br_surf.h for the derivation and the address chain.
 *
 * Five functions, all from BRGlide.dll:
 *   0x10001130  BrSurfNew
 *   0x10001190  BrSurfFree
 *   0x100011C0  BrSurfBlt24
 *   0x10001240  BrSurfFromBitmap
 *   0x100014A0  BrSurfSetColourKey
 *
 * None of them exists in BRD3D.dll: that build puts its images on DirectDraw
 * surfaces instead, and the only piece it shares is the 24bpp gate, at
 * 0x10060EA5.  Scanned for by byte pattern in both binaries --
 * `66 83 7e 12 18` (the gate) hits twice in Glide and once in D3D;
 * `80 e2 fc` (the green mask in the 565 packer) and `81 e1 f8 ff 00 00` (the
 * key packer) hit in Glide only.  So there is nothing to cross-check against
 * for these, and config/shared.csv reports them unpaired for the same reason.
 */
#include "br_surf.h"

#include <stdlib.h>

/* ----------------------------------------------------------------------
 * 0x10001130 -- allocate the surface
 *
 * The original mallocs the literal 0x10 for the header. Per CONVENTIONS.md
 * ("never allocate an original size literal") this uses sizeof: on LP64 the
 * pointer at +0x00 widens and 16 bytes would under-allocate.
 *
 * Field order is the original's: cy is stored, then cx, then the key is
 * cleared, and only THEN is the pixel buffer allocated -- so a surface whose
 * pixel allocation fails has already had its dimensions written. It is freed
 * on that path, so nothing observes it, but the order is preserved because
 * there is no reason to depart from it.
 * ---------------------------------------------------------------------- */
BrSurf *BrSurfNew(int32_t cx, int32_t cy)
{
    BrSurf *pSurf = (BrSurf *)malloc(sizeof *pSurf);
    if (!pSurf) return NULL;

    pSurf->cy  = cy;
    pSurf->cx  = cx;
    pSurf->key = 0;

    /* `imul eax, ecx / shl eax, 1` -- cy*cx*2. Computed in size_t here; the
     * original wraps at 32 bits, and no shipped image comes near that. */
    pSurf->pPix = (uint16_t *)malloc((size_t)cx * (size_t)cy * 2u);
    if (!pSurf->pPix) {
        free(pSurf);
        return NULL;
    }
    return pSurf;
}

/* ----------------------------------------------------------------------
 * 0x10001190 -- free it
 * ---------------------------------------------------------------------- */
void BrSurfFree(BrSurf *pSurf)
{
    if (!pSurf) return;
    if (pSurf->pPix) free(pSurf->pPix);
    free(pSurf);
}

/* ----------------------------------------------------------------------
 * 0x100011C0 -- 24bpp bottom-up BGR to RGB565, top-down
 *
 * The source pointer starts at the LAST stored row and the stride is
 * SUBTRACTED; the destination advances continuously and is never re-based per
 * row, so the destination pitch is cx with no padding. Both facts are
 * load-bearing for 0x10001320, which addresses the surface as
 * `pPix[y*cx + x]`.
 *
 * The zero tests are the original's and are kept: `test eax, eax / je` on the
 * row count before the loop, and `test edi, edi / je` on the column count
 * inside it, so a 0-wide or 0-tall bitmap writes nothing rather than
 * underflowing a do/while.
 * ---------------------------------------------------------------------- */
void BrSurfBlt24(uint16_t *pDst, const uint8_t *pBits,
                 int32_t cx, int32_t cy, int32_t cbWidthBytes)
{
    const uint8_t *pRow = pBits + (size_t)(cy - 1) * (size_t)cbWidthBytes;
    int32_t        y;

    if (cy == 0) return;

    for (y = cy; y != 0; y--) {
        const uint8_t *pSrc = pRow;
        int32_t        x;

        for (x = cx; x != 0; x--) {
            /* Stored B,G,R. The masks are asymmetric -- 0xFC keeps six bits
             * of green, 0xF8 keeps five of red -- and blue is shifted rather
             * than masked. Transcribed as the original composes it. */
            uint32_t b = pSrc[0];
            uint32_t g = pSrc[1] & 0xFCu;
            uint32_t r = pSrc[2] & 0xF8u;
            pSrc += 3;

            *pDst++ = (uint16_t)(((((r << 5) | g) << 3) | (b >> 3)) & 0xFFFFu);
        }
        pRow -= cbWidthBytes;
    }
}

/* ----------------------------------------------------------------------
 * 0x10001240 -- the 24bpp gate, then allocate and convert
 *
 * `cmp word ptr [esi+0x12], 0x18` is the whole of the format support in this
 * build. Anything Windows handed back that is not 24 bits per pixel -- an
 * 8bpp palettised BMP, a 4bpp one, an RLE one after GDI decompressed it to
 * 8bpp -- leaves here as NULL and the caller reports
 * "DDraw_DoInit: Bitmap %d failed to load!".
 * ---------------------------------------------------------------------- */
BrSurf *BrSurfFromBitmap(const BrGdiBitmap *pbm)
{
    BrSurf *pSurf;

    if (!pbm) return NULL;
    if (pbm->cBitsPixel != 24) return NULL;

    pSurf = BrSurfNew(pbm->cx, pbm->cy);
    if (!pSurf) return NULL;

    BrSurfBlt24(pSurf->pPix, pbm->pBits, pbm->cx, pbm->cy, pbm->cbWidthBytes);
    return pSurf;
}

/* ----------------------------------------------------------------------
 * 0x100014A0 -- COLORREF (0x00BBGGRR) to the surface's 16-bit key
 *
 * Transcribed instruction by instruction rather than rewritten as a tidy
 * shift-and-mask, because the original's intermediate `(v & 0xFFF8) << 5`
 * carries bits that only fall off the top when the final `<< 3` overflows the
 * word store -- the low byte of the COLORREF (red) survives as bits 11..15,
 * and everything above bit 15 is discarded by the `mov word ptr` and not by
 * any mask.
 *
 *   RGB(0,255,0) == 0x0000FF00 -> 0x07E0.
 * ---------------------------------------------------------------------- */
void BrSurfSetColourKey(BrSurf *pSurf, uint32_t colorref)
{
    uint32_t ecx, edx, eax;

    if (!pSurf) return;

    ecx = colorref & 0xFFF8u;          /* and ecx, 0xfff8 */
    edx = (colorref >> 8) & 0xFCu;     /* shr edx, 8 / and edx, 0xfc */
    ecx <<= 5;                         /* shl ecx, 5 */
    eax = (colorref >> 16) & 0xFFu;    /* shr eax, 0x10 -- then `shr al, 3` */
    ecx |= edx;                        /* or ecx, edx */
    eax = (eax >> 3) & 0x1Fu;          /* shr al, 3 / and eax, 0x1f */
    ecx <<= 3;                         /* shl ecx, 3 */
    ecx |= eax;                        /* or ecx, eax */

    pSurf->key = (uint16_t)(ecx & 0xFFFFu);   /* mov word ptr [eax+0xc], cx */
}

/* ----------------------------------------------------------------------
 * NOT in the original: 565 -> 0x00RRGGBB by bit replication.
 *
 * See br_surf.h for why replication and not a shift. The short version: the
 * colour key has to survive the widening, and 63 must come back as 255.
 * ---------------------------------------------------------------------- */
uint32_t BrSurf565ToRgb(uint16_t v)
{
    uint32_t r5 = (uint32_t)(v >> 11) & 0x1Fu;
    uint32_t g6 = (uint32_t)(v >>  5) & 0x3Fu;
    uint32_t b5 = (uint32_t)(v)       & 0x1Fu;
    uint32_t r  = (r5 << 3) | (r5 >> 2);
    uint32_t g  = (g6 << 2) | (g6 >> 4);
    uint32_t b  = (b5 << 3) | (b5 >> 2);
    return (r << 16) | (g << 8) | b;
}
