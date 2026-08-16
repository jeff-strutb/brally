/* br_bmp.h -- THE PLATFORM HALF of the game's image loading, plus the two
 * original loaders that sit astride the seam.
 *
 * This file used to declare a hand-written 24-bit BMP decoder, on the grounds
 * that the port needed one.  It did -- but so did the game, and the game has
 * two.  br_surf.h documents the chain in full; the short version is that
 * NEITHER original parses a BMP.  Both call
 *
 *     LoadImageA(hinst, name, IMAGE_BITMAP, cx, cy,
 *                LR_LOADFROMFILE | LR_CREATEDIBSECTION)
 *     GetObjectA(hbm, sizeof(BITMAP), &bm)
 *
 * and work from the resulting `BITMAP`.  Windows owns the file format.
 *
 * So the line this project draws -- decompile game behaviour, recreate 1999
 * platform APIs -- falls in the middle of this operation, and this header is
 * the platform side of it:
 *
 *   BrBmpGdiLoad      recreates LoadImageA + GetObjectA.  This is the only
 *                     code here with no original address, and it has none
 *                     because the original is in USER32.DLL and GDI32.DLL.
 *   BrBmpLoadSurface  IS 0x10001290, the UI-sprite loader.
 *   BrBmpLoadRgba     IS 0x1005A210, the Paint\ texture loader.
 *   BrBmpLoad         the host's adaptor over BrBmpLoadSurface.
 *
 * Everything downstream of the `BITMAP` -- the 24bpp gate, the surface, the
 * conversions, the colour key -- is in br_surf.c, transcribed.
 */
#ifndef BR_BMP_H
#define BR_BMP_H

#include <stdint.h>

#include "br_surf.h"

/* ==========================================================================
 * The recreation: BMP file -> Win32 BITMAP
 * ==========================================================================
 *
 * Stands in for LoadImageA(..., LR_LOADFROMFILE|LR_CREATEDIBSECTION) followed
 * by GetObjectA.  It does NOT judge the format: LoadImageA loads 1/4/8/16/24
 * and 32bpp files, and LR_CREATEDIBSECTION keeps the file's own colour depth,
 * so it is the GAME that refuses everything but 24bpp -- twice, at 0x1000124B
 * and at 0x1005A24D.  Leaving the refusal there rather than here is the whole
 * point of splitting the two files.
 *
 * `pBits` points at the FIRST STORED ROW, which is what GDI reports and which
 * for the bottom-up DIBs on the disc is the image's bottom row.  `cy` is
 * |biHeight|, because BITMAP.bmHeight is always positive.
 *
 * KNOWN GAPS IN THE RECREATION, none of them reachable from the shipped art
 * (all 172 BMPs in IMAGES\ are 40-byte header, 24bpp, BI_RGB, bottom-up,
 * bfOffBits 54):
 *   - BI_RLE4/BI_RLE8 are refused.  LoadImageA would decompress them to an
 *     8bpp DIB section, which the game would then refuse at the gate, so the
 *     visible outcome is the same and only the reason differs.
 *   - BITMAPCOREHEADER (12-byte DIB header) is refused.
 *   - The resource half of LoadImageA has no host equivalent; see
 *     BrBmpLoadSurface.
 *
 * Returns 0 on success.  BrBmpGdiFree releases the pixel buffer. */
typedef struct BrGdiBitmapMem {
    BrGdiBitmap  bm;
    uint8_t     *pAlloc;      /* the buffer bm.pBits points into */
} BrGdiBitmapMem;

int  BrBmpGdiLoad(BrGdiBitmapMem *pOut, const char *pszPath);
void BrBmpGdiFree(BrGdiBitmapMem *pMem);

/* ==========================================================================
 * The two originals
 * ==========================================================================
 *
 * 0x10001290 -- the UI sprite loader.  `cx`/`cy` are LoadImageA's stretch
 * parameters and every caller in the binary passes 0, 0.  Returns a 16-bit
 * surface with its colour key already set to 0x07E0 by the literal store at
 * 0x10001308.
 *
 * The original tries the RESOURCE table of the process module first
 * (GetModuleHandleA(NULL), flags 0x2000 with no LR_LOADFROMFILE) and only
 * falls back to the file (flags 0x2010).  Every shipped name is a relative
 * path -- "images\work1a.bmp" -- and the DLL carries no bitmap resources, so
 * the first call always fails and the second is the one that works.  The port
 * has no resource table to search, so only the file arm exists here; the
 * outcome is identical for every name the game uses. */
BrSurf *BrBmpLoadSurface(const char *pszPath, int32_t cx, int32_t cy);

/* 0x1005A210 -- the Paint\ texture loader, and the function br_bmp.c had
 * unwittingly reinvented: 0x10059F70's inner loop is the same walk, the same
 * B,G,R read order and the same 0xFF alpha this file used to open with.
 *
 * Returns a malloc'd cx*cy*4 RGBA8888 buffer, or NULL.  The original
 * publishes the dimensions through two globals (0x10AC67C4 / 0x10AC67C8)
 * rather than returning them, and its caller 0x1005A080 reads them straight
 * back; here they are out-parameters, which is the same information without a
 * pair of host globals nothing else would ever touch.
 *
 * PRESERVED QUIRK: on the 24bpp refusal the original returns WITHOUT calling
 * DeleteObject, so the HBITMAP leaks.  It is reproduced (the pixel buffer is
 * released, but the refusal path is the one that leaks in the original and
 * there is nothing here to leak), and noted because it is the clearest
 * evidence that the gate was an afterthought bolted onto a working path. */
uint8_t *BrBmpLoadRgba(const char *pszPath, int32_t *pcx, int32_t *pcy);

/* ==========================================================================
 * The host adaptor
 * ==========================================================================
 *
 * The host uploads RGBA8888 textures; the game drew RGB565.  BrBmpLoad runs
 * the real 0x10001290 pipeline and then widens, so what the host sees is the
 * game's pixels -- five bits of red and blue, six of green -- and not the
 * file's.
 *
 * The colour key widens too, and MEASURED rather than assumed: 0x07E0 is
 * every colour with red < 8, green >= 252 and blue < 8, so quantising first
 * could in principle key a near-green fringe that a 24-bit comparison would
 * miss.  Counted over all 18 keyed sheets on the disc -- 517,440 pixels --
 * the two agree exactly: 54,169 keyed either way, zero extra.  The shipped
 * art has hard green edges and no anti-aliasing, so this is a difference the
 * original's arithmetic permits and the original's ARTWORK never exercises.
 * Recorded because "it would have mattered" and "it did not matter" are
 * different claims and only one of them was checked. */
typedef struct BrBmp {
    uint32_t  w, h;
    uint8_t  *pRgba;      /* row 0 is the TOP row; RGBA8888 */
} BrBmp;

/* 0 on success. */
int  BrBmpLoad(BrBmp *pOut, const char *pszPath);

/* THE COLOUR KEY, read out of the image rather than guessed, and now stated
 * by the original in two encodings that agree.
 *
 * 0x10001290 stores it as a literal --
 *
 *     10001308  66 c7 47 0c e0 07    mov word ptr [edi+0xC], 0x7E0
 *
 * -- and 0x100583C0, the loader loop, immediately overwrites it with
 * 0x100014A0(surf, 0x0000FF00).  That COLORREF is RGB(0, 255, 0) and
 * 0x100014A0 packs it to 0x07E0.  Same constant, once as a 16-bit literal and
 * once as a 24-bit colour put through the packer.
 *
 * +0x0C is the field 0x10001320 reads as the key when the sprite table
 * entry's +0x14 has bit 0 (`mov cx, word ptr [ecx+0xC]`).  Note that
 * 0x100583C0 sets the key on EVERY image; the table's flag chooses the keyed
 * blit (0x10001440) over the plain one (0x100013F0), it does not choose the
 * colour.
 *
 * Every keyed sheet on the disc agrees: type_gry/wit/mid/yel, cursor, the
 * four arrows, steerarr and soundptr all carry a large field of exactly
 * (0,255,0).
 *
 * This corrects a magenta constant that had been assumed here.  Magenta
 * appears in none of the files, so every keyed sprite was being drawn fully
 * opaque -- including all four font sheets, whose background is 63% of their
 * pixels. */
#define BR_UI_COLOUR_KEY  0x00FF00u

/* Zero the alpha of every texel matching `rgb` (0xRRGGBB).  The blitter keys
 * on the SPRITE TABLE's flag bit, not on anything in the file, so the caller
 * with the table entry decides whether to call this. */
void BrBmpApplyKey(BrBmp *pBmp, uint32_t rgb);

void BrBmpFree(BrBmp *pBmp);

#endif /* BR_BMP_H */
