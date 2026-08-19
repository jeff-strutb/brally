/* br_img.h -- Boss Rally .img image loader (portable).
 *
 * Format, confirmed against splash.img (256x256) and loading.img (256x200)
 * from the retail disc; in both cases width*height*2 + 12 is exactly the file
 * size.
 *
 *   +0x00  u32  width      (little-endian)
 *   +0x04  u32  height
 *   +0x08  u32  format     0x1555 in every retail file
 *   +0x0C  u16  pixels[width*height]   ARGB1555, little-endian
 *
 * ARGB1555 (not RGBA5551): bit 15 alpha, 14..10 red, 9..5 green, 4..0 blue.
 * This is the DirectDraw convention, which fits a DirectDraw-era title, and is
 * corroborated by loading.img whose border pixels are 0x8000 -- opaque black
 * under ARGB1555, but a transparent dark red under RGBA5551.
 *
 * Note the N64 build of this game uses RGBA5551 for the same artwork, so the
 * two are NOT interchangeable; see br_n64tex.h when that lands.
 */
#ifndef BR_IMG_H
#define BR_IMG_H

#include <stddef.h>
#include <stdint.h>

#define BR_IMG_FORMAT_ARGB1555 0x1555u

typedef struct BrImage {
    uint32_t  width;
    uint32_t  height;
    uint8_t  *pixels;      /* RGBA8888, width*height*4 bytes, caller frees */
} BrImage;

/* Load a .img file and expand it to RGBA8888. Returns 0 on success. */
int  BrImgLoad(BrImage *pImg, const char *pszPath);

/* Decode an in-memory .img (e.g. straight out of a POD). Returns 0 on success. */
int  BrImgDecode(BrImage *pImg, const void *pvData, size_t cbData);

void BrImgFree(BrImage *pImg);

#endif /* BR_IMG_H */
