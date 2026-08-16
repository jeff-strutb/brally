/* br_surf.h -- THE GAME'S OWN IMAGE SURFACE AND ITS TWO IMAGE DECODERS.
 *
 * ==========================================================================
 * WHY THIS MODULE EXISTS: br_bmp.c WAS RECREATION WHERE DECOMPILATION EXISTED
 * ==========================================================================
 *
 * br_bmp.c used to be a fresh 24-bit BMP decoder written from the format spec.
 * BRGlide.dll has TWO decoders of its own for exactly these files, and one of
 * them is byte-for-byte the loop br_bmp.c had independently arrived at.  This
 * module is those two, transcribed.
 *
 * THE UI-SPRITE CHAIN (16-bit, the one the menus use)
 *
 *     0x100583C0  DDraw_DoInit -- names itself in its own error strings
 *                 ("DDraw_DoInit: Bitmap %d failed to load!").  Creates the
 *                 back surface from 0x100A7514 / 0x100A7518, blits
 *                 images\loading.bmp through slot 0 and frees it again, then
 *                 walks the image table at 0x10AC53E8 from +0x04 to
 *                 0x10AC5874 -- stride 8, (0x10AC5874-0x10AC53EC)/8 == 145,
 *                 the count for the third independent time -- and for each
 *                 entry with a non-NULL NAME calls
 *       0x10001290  the loader.  LoadImageA twice (resource, then file),
 *                   GetObjectA into a Win32 BITMAP, then
 *         0x10001240  the 24bpp GATE and the conversion driver
 *           0x10001130  allocate the surface        -> BrSurfNew
 *           0x100011C0  24bpp bottom-up -> RGB565   -> BrSurfBlt24
 *                   then DeleteObject and one literal store of 0x07E0.
 *                 Back in the caller, 0x100014A0 sets the key AGAIN from
 *                 COLORREF 0x0000FF00.  It computes 0x07E0.
 *       0x10001190  the free                        -> BrSurfFree
 *
 * THE TEXTURE CHAIN (32-bit, car paint under Paint\)
 *
 *     0x1005A080  loads Paint\%s for four mip levels
 *       0x1005A210  LoadImageA(file), GetObjectA, gate, then
 *         0x10059F10  malloc(cx*cy*4), convert, publish cx/cy in globals
 *           0x10059F70  24bpp bottom-up -> R,G,B,0xFF
 *
 * 0x10059F70 IS br_bmp.c's old inner loop.  Same walk from the last stored
 * row backwards, same B,G,R read order, same 0xFF alpha.  The recreation was
 * correct; it was just a recreation.
 *
 * ==========================================================================
 * WHAT IS PLATFORM AND WHAT IS GAME -- WHERE THE LINE ACTUALLY FALLS
 * ==========================================================================
 *
 * NEITHER original contains a BMP PARSER.  Both hand the path to
 * USER32!LoadImageA with LR_LOADFROMFILE|LR_CREATEDIBSECTION and read the
 * result back through GDI32!GetObjectA as a `BITMAP`.  Windows does the file
 * parsing; the game only ever sees five numbers and a pointer.
 *
 * So the split is not a judgement call:
 *   - file bytes -> `BrGdiBitmap` is LoadImageA + GetObjectA, a 1999 platform
 *     API, and is recreated in br_bmp.c.  It is named there, in its banner.
 *   - `BrGdiBitmap` -> pixels is game code, and is THIS FILE.
 *
 * The 24bpp refusal that br_bmp.c used to own now sits where the original put
 * it: `cBitsPixel != 24` in BrSurfFromBitmap (0x10001240's
 * `cmp word ptr [esi+0x12], 0x18`) and in BrBmpLoadRgba (0x1005A247's
 * `cmp word ptr [esp+0x1a], 0x18`).  BOTH originals refuse anything that is
 * not 24bpp, so the port refusing 8bpp/palettised art is the original's
 * behaviour and not a shortcut -- and all 172 BMPs shipped in IMAGES\ are
 * 40-byte-header, 24bpp, BI_RGB, bottom-up, bfOffBits 54.  There is nothing
 * on the disc that either decoder would turn away.
 *
 * ==========================================================================
 * THE SURFACE
 * ==========================================================================
 *
 * 0x10001130 mallocs SIXTEEN bytes:
 *
 *     +0x00  uint16 *pPix     cx*cy*2 bytes, RGB565, NO row padding
 *     +0x04  int32   cx
 *     +0x08  int32   cy
 *     +0x0C  uint16  key      the colour key, in RGB565
 *
 * The destination pitch is cx, not a padded stride -- 0x100011C0 writes the
 * destination linearly across row boundaries -- and 0x10001320 relies on that
 * when it computes `dst + (y*cx + x)*2`.
 *
 * Row order: the source walk starts at `pBits + (cy-1)*cbWidthBytes` and
 * SUBTRACTS the stride each row, while the destination only ever moves
 * forward.  A GDI bottom-up DIB section stores the bottom row first, so
 * starting at the last stored row and walking backwards visits the image top
 * row first.  Destination row 0 is therefore the TOP row -- which is what the
 * sprite table's rectangles and the blitter both assume.
 *
 * A top-down BMP (negative biHeight) would come out VERTICALLY FLIPPED,
 * because the walk is unconditional -- there is no sign test anywhere in
 * 0x100011C0 or 0x10059F70.  Not a hypothetical defect worth working around:
 * no file on the disc is top-down, and inventing a sign test would be a
 * departure from the original with nothing to justify it.
 */
#ifndef BR_SURF_H
#define BR_SURF_H

#include <stdint.h>

/* ==========================================================================
 * The Win32 BITMAP, as GetObjectA(hbm, 24, &bm) fills it
 * ==========================================================================
 *
 * Field names are the SDK's, minus the `bm` prefix.  The original reads it at
 * fixed byte offsets (+0x04 width, +0x08 height, +0x0C widthBytes, +0x12
 * bitsPixel, +0x14 bits); this is a host struct, so it is read by name and
 * the offsets are recorded here rather than relied on.  See CONVENTIONS.md,
 * "Byte offsets are 32-bit-only".
 *
 * `pBits` is the FIRST STORED ROW, exactly as GDI reports it: for the
 * bottom-up DIBs the game ships, that is the image's BOTTOM row. */
typedef struct BrGdiBitmap {
    int32_t        type;          /* +0x00  bmType, always 0 for a DIB      */
    int32_t        cx;            /* +0x04  bmWidth                         */
    int32_t        cy;            /* +0x08  bmHeight, always positive       */
    int32_t        cbWidthBytes;  /* +0x0C  bmWidthBytes, DWORD-aligned     */
    uint16_t       cPlanes;       /* +0x10  bmPlanes                        */
    uint16_t       cBitsPixel;    /* +0x12  bmBitsPixel -- THE GATE reads   */
    const uint8_t *pBits;         /* +0x14  bmBits                          */
} BrGdiBitmap;

/* ==========================================================================
 * The surface -- 0x10001130's sixteen bytes
 * ========================================================================== */
typedef struct BrSurf {
    uint16_t *pPix;    /* +0x00  RGB565, pitch == cx, no padding            */
    int32_t   cx;      /* +0x04                                             */
    int32_t   cy;      /* +0x08                                             */
    uint16_t  key;     /* +0x0C  colour key, RGB565                         */
} BrSurf;

/* 0x10001130.  NULL if either malloc fails; the header is freed again when
 * the pixel buffer is the one that failed. */
BrSurf *BrSurfNew(int32_t cx, int32_t cy);

/* 0x10001190.  Tolerates NULL, and tolerates a surface whose pPix is NULL.
 * Does NOT clear pPix before freeing the header -- there is nothing left to
 * clear it in. */
void BrSurfFree(BrSurf *pSurf);

/* 0x100011C0.  Writes cx*cy uint16 starting at pDst, reading cy rows of cx
 * BGR triples from pBits + (cy-1)*cbWidthBytes downwards.
 *
 * The packing is transcribed as the original computes it:
 *   ((((R & 0xF8) << 5) | (G & 0xFC)) << 3) | (B >> 3)
 * which is RGB565 -- and note the two masks are DIFFERENT (0xF8 and 0xFC),
 * which is the tell that green really does get six bits here. */
void BrSurfBlt24(uint16_t *pDst, const uint8_t *pBits,
                 int32_t cx, int32_t cy, int32_t cbWidthBytes);

/* 0x10001240.  The gate and the driver: refuses anything whose cBitsPixel is
 * not 24, allocates, converts.  NULL on refusal or on allocation failure --
 * the original cannot tell those apart either. */
BrSurf *BrSurfFromBitmap(const BrGdiBitmap *pbm);

/* 0x100014A0.  Takes a Win32 COLORREF -- 0x00BBGGRR, the layout the RGB()
 * macro produces -- and packs it into the surface's 16-bit key.
 *
 * 0x100583C0 calls this with 0x0000FF00 == RGB(0, 255, 0) for EVERY image it
 * loads, which yields 0x07E0.  0x10001290 has already stored that same 0x07E0
 * as a literal.  Two independent statements of the same constant, in two
 * different encodings, in two different functions. */
void BrSurfSetColourKey(BrSurf *pSurf, uint32_t colorref);

/* The key those two agree on, in the surface's own encoding. */
#define BR_SURF_KEY_565   0x07E0u    /* RGB565 (0, 63, 0) -- pure green */

/* ==========================================================================
 * 565 -> 8888, the ONE thing here that is not in the original
 * ==========================================================================
 *
 * The original never widens a surface: it blits 16-bit pixels onto a 16-bit
 * DirectDraw back buffer.  The host uploads RGBA8888 textures, so the port
 * has to widen somewhere, and it must widen by BIT REPLICATION rather than by
 * a plain shift.
 *
 * That is not a stylistic preference.  The colour key survives a replicating
 * expansion and does not survive a shifting one: G=63 replicates to
 * (63<<2)|(63>>4) == 255, so keyed green comes back as exactly 0x00FF00 and
 * the host's BrBmpApplyKey(BR_UI_COLOUR_KEY) still matches it.  Shifting
 * gives 252, every font sheet stays fully opaque, and 63% of the glyph
 * pixels turn into background blocks. */
uint32_t BrSurf565ToRgb(uint16_t v);

#endif /* BR_SURF_H */
