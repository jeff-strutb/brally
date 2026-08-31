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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
/* @implements 0x10001130 glide BrSurfNew */
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
/* @implements 0x10001190 glide BrSurfFree */
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
/* WHAT IT DOES: copies a loaded Windows bitmap's pixels into the game's own
 * image format, converting full-colour pixels down to the 16-bit colour the
 * renderer uses and flipping the picture the right way up, since Windows
 * stores bitmaps bottom row first. A zero-width or zero-height picture copies
 * nothing rather than running away. */
/* @implements 0x100011C0 glide BrSurfBlt24 */
void BrSurfBlt24(uint16_t *pDst, const uint8_t *pBits,
                 int32_t cx, int32_t cy, int32_t cbWidthBytes)
{
    const uint8_t *pRow = pBits + (cy - 1) * cbWidthBytes;

    if (cy == 0) return;

    do {
        const uint8_t *pSrc = pRow;
        int32_t        x = cx;

        if (x != 0) {
            do {
                /* Byte loads, byte masks, 16-bit movzx, three `inc`s on the
                 * source, dest post-inc as `add esi,2` / `mov [esi-2]`. */
                unsigned char  b, g, r;
                unsigned short acc, rs;

                b = *pSrc;
                g = pSrc[1];
                pSrc++;
                g &= 0xFCu;
                pSrc++;
                pDst++;
                acc = (unsigned short)g;
                r = *pSrc;
                pSrc++;
                r &= 0xF8u;
                rs = (unsigned short)r;
                acc |= (unsigned short)(rs << 5);
                acc <<= 3;
                acc |= (unsigned short)((unsigned char)(b >> 3));
                pDst[-1] = acc;
            } while (--x);
        }
        pRow -= cbWidthBytes;
    } while (--cy);
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
/* WHAT IT DOES: turns a bitmap Windows has just loaded into an image the game
 * can draw, by making room for it and converting the pixels. This build only
 * understands full-colour 24-bit bitmaps, so anything else -- a paletted or
 * compressed BMP -- is simply rejected, and the caller reports that the image
 * failed to load. */
/* @implements 0x10001240 glide BrSurfFromBitmap */
BrSurf *BrSurfFromBitmap(const BrGdiBitmap *pbm)
{
    BrSurf *pSurf;

#ifndef BR_MATCHING_BUILD
    /* Port-only: orig has no NULL test (`mov esi,[esp+8]; cmp word [esi+12],18`). */
    if (!pbm) return NULL;
#endif
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
/* WHAT IT DOES: tells an image which colour counts as "see through", so that
 * colour is skipped when the image is drawn. The colour is given in the
 * ordinary Windows form and stored in the reduced form the renderer compares
 * against. */
/* @implements 0x100014A0 glide BrSurfSetColourKey */
void BrSurfSetColourKey(BrSurf *pSurf, uint32_t colorref)
{
    uint32_t ecx, edx, eax;

#ifndef BR_MATCHING_BUILD
    /* Port-only: orig loads pSurf after packing (`mov eax,[esp+4]; mov [eax+0xc],cx`). */
    if (!pSurf) return;
#endif

    ecx = colorref;
    edx = colorref;
    eax = colorref;
    ecx &= 0xFFF8u;
    edx >>= 8;
    ecx <<= 5;
    edx &= 0xFCu;
    eax >>= 16;
    ecx |= edx;
    eax = (unsigned char)eax >> 3;
    ecx <<= 3;
    eax &= 0x1Fu;
    ecx |= eax;

    pSurf->key = (uint16_t)ecx;
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

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
void __stdcall grLfbWriteRegion(int, int, int, int, int, int, int, int);

/* WHAT IT DOES: blit a surface's pixel data to the Glide linear frame buffer. */
/* @implements 0x100014E0 glide BrGlideLfbWrite */

void BrGlideLfbWrite(int *param_1)

{
  grLfbWriteRegion(1, 0, 0, 0, param_1[1], param_1[2], param_1[1] * 2, *param_1);
  return;
}

/* WHAT IT DOES: compute a predicted position from pos + vel*3 + accel*2 + offset (fastcall, via entity at ECX). */
/* @implements 0x10001C90 glide BrVec3Predict */

int __fastcall BrVec3Predict(int param_1)

{
  int iVar1;
  
  iVar1 = param_1 + 0x2838;
  *(int *)(param_1 + 0x2734) = param_1 + 0x2808;
  BrVec3MulAdd(iVar1,param_1 + 0x30,param_1,0x40c00000);
  BrVec3MulAddTo(iVar1,param_1 + 0x10,0x40000000);
  BrVec3AddTo(iVar1,param_1 + 0x20);
  *(int *)(param_1 + 0xf78) = 2;
  return;
}

#endif /* BR_MATCHING_BUILD */
