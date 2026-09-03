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
/* WHAT IT DOES: allocate an off-screen picture of cx by cy pixels, two bytes
 * per pixel, and hand back the header describing it. Returns NULL if either
 * allocation fails, having freed the header rather than leaking it. */
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
/* WHAT IT DOES: release a picture made by BrSurfNew, pixels first then the
 * header. Safe to call with NULL, and safe if the pixels were never
 * allocated. */
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
    int key;
    unsigned char b;

#ifndef BR_MATCHING_BUILD
    /* Port-only: orig loads pSurf after packing (`mov eax,[esp+4]; mov [eax+0xc],cx`). */
    if (!pSurf) return;
#endif

    /* 0x00BBGGRR -> 565 with red in the high bits.  The red and green terms
     * share a `<< 3`, and the source says so: the whole point of writing the
     * pack factored is that the shared shift is where the original's
     * `shl ecx,5 ... shl ecx,3` pair comes from.  Writing the two terms in
     * their natural finished positions instead (`(r >> 3) << 11`,
     * `(g >> 2) << 5`) makes VC5 emit one `shl 8` and a pre-shifted green
     * mask, and nothing recovers the pair.
     *
     * The red mask is 0xFFFF, not 0xFF: the green bits it drags in land at
     * bit 16 and up and the store is 16 bits wide, so they never appear --
     * and only the wide mask reproduces `and ecx,0xfff8`.
     *
     * Blue is a BYTE-SLOT local, and the split into two statements is what
     * buys it.  The original narrows to the byte lane -- `shr eax,0x10 /
     * shr al,3 / and eax,0x1f` -- and the only spelling that reproduces the
     * `shr al,3` without spilling is: the red/green half assigned to `key`
     * FIRST, then `b` loaded and shifted IN PLACE, then consumed by the very
     * next statement.  `b` has to die into the store: any statement between
     * `b >>= 3` and the use (an `int` widening copy, a `key |= b`) makes VC5
     * spill it to the frame -- +10 bytes and a `mov byte ptr [esp+I],B`.
     *
     * RESIDUE (size-exact 51/51, 16/16 instructions, register-blind gap 1):
     * ONE widening form.  Orig widens the byte with `and eax,0x1f` (a
     * range-known mask -- the byte-slot dword widening); we emit
     * `movzx cx,cl`, because the 16-bit destination narrows the OR to word
     * ops.  The accumulator/blue register homes are also swapped (orig
     * accumulates in ecx and puts blue in eax; we do the reverse), which is
     * allocation, not shape.
     * DO NOT RE-PROBE, all measured at this frame: `& 0x1F` on the char at
     * the use (mask is dropped, movzx stays); the mask inside the char cast;
     * OR operand order reversed; an `int blue = b;` widening copy (spills);
     * `key |= b;` as its own statement (spills); an `unsigned char` local
     * declared and computed BEFORE the red/green half (spills, +10 bytes);
     * the whole pack in one expression with the cast in place (`and
     * ecx,0xff` before a dword shift -- the cast materialises eagerly);
     * a plain `(colorref >> 19) & 0x1F` (one `shr eax,0x13`, 3 bytes short,
     * which is where this function sat before).  A uint32_t accumulator for
     * the whole pack is worse again (45 diffs). */
    key = (((((colorref & 0xFFFFu) >> 3) << 8)
            | ((colorref >> 8) & 0xFCu)) << 3);
    b = (unsigned char)(colorref >> 16);
    b >>= 3;

    pSurf->key = (uint16_t)(key | (b & 0x1F));
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
