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

/* Zero the alpha of every texel matching `rgb` (0xRRGGBB). The blitter keys on
 * the SPRITE TABLE's flag bit, not on anything in the file, so the caller with
 * the table entry decides whether to call this. */
void BrBmpApplyKey(BrBmp *pBmp, uint32_t rgb);

void BrBmpFree(BrBmp *pBmp);

#endif /* BR_BMP_H */
