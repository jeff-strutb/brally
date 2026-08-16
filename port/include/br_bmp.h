/* br_bmp.h -- 24-bit uncompressed BMP decoder for the menu sprite sheets.
 * See br_bmp.c for the format notes and why nothing else is accepted. */
#ifndef BR_BMP_H
#define BR_BMP_H

#include <stdint.h>

typedef struct BrBmp {
    uint32_t  w, h;
    uint8_t  *pRgba;      /* row 0 is the TOP row; RGBA8888 */
} BrBmp;

/* 0 on success. Refuses anything that is not 24bpp uncompressed rather than
 * decoding it wrongly. */
int  BrBmpLoad(BrBmp *pOut, const char *pszPath);

/* THE COLOUR KEY, read out of the image rather than guessed.
 *
 * 0x10001290 is the loader: LoadImageA, GetObjectA, convert (0x10001240), and
 * then one literal store --
 *
 *     10001308  66 c7 47 0c e0 07    mov word ptr [edi+0xC], 0x7E0
 *
 * +0x0C is the field 0x10001320 reads as the key when the sprite table entry's
 * +0x14 has bit 0 (`mov cx, word ptr [ecx+0xC]`).  The surface is 16-bit
 * RGB565, so 0x07E0 is R=0, G=63, B=0 -- PURE GREEN, and it is the same
 * constant for every image in the game.  Every keyed sheet on the disc agrees:
 * type_gry/wit/mid/yel, cursor, the four arrows, steerarr and soundptr all
 * carry a large field of exactly (0,255,0), which is what 0x07E0 expands to.
 *
 * This corrects a magenta constant that had been assumed here.  Magenta
 * appears in none of the files, so every keyed sprite was being drawn fully
 * opaque -- including all four font sheets, whose background is 63% of their
 * pixels. */
#define BR_UI_COLOUR_KEY  0x00FF00u

/* Zero the alpha of every texel matching `rgb` (0xRRGGBB). The blitter keys on
 * the SPRITE TABLE's flag bit, not on anything in the file, so the caller with
 * the table entry decides whether to call this. */
void BrBmpApplyKey(BrBmp *pBmp, uint32_t rgb);

void BrBmpFree(BrBmp *pBmp);

#endif /* BR_BMP_H */
