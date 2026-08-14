/* br_n64tex.h -- N64-format CI4 texture + LUT4 palette (portable).
 *
 * The PC port ships these N64 formats verbatim in cargfx/, so the loader is
 * shared between both builds of the game.
 *
 *   .lut4  16 palette entries, RGBA5551, BIG-endian (N64 byte order)
 *   .ci4   4 bits per pixel, two pixels per byte, HIGH nibble is the left one
 *
 * Note this is RGBA5551 (alpha in bit 0), not the ARGB1555 used by some .img
 * files -- see br_img.h. Confirmed from skytexdesert.lut4, whose entries all
 * have bit 0 set (opaque) and are nonsense under ARGB1555.
 */
#ifndef BR_N64TEX_H
#define BR_N64TEX_H

#include <stddef.h>
#include <stdint.h>

#define BR_LUT4_ENTRIES 16

typedef struct BrTex {
    uint32_t  width;
    uint32_t  height;
    uint8_t  *pixels;        /* RGBA8888, caller frees */
} BrTex;

/* Decode CI4 + LUT4 into RGBA8888. Dimensions must be supplied by the caller:
 * neither file carries them, so the game knows them by asset convention. */
int  BrTexDecodeCI4(BrTex *pTex, const void *pvCi4, size_t cbCi4,
                    const void *pvLut, size_t cbLut,
                    uint32_t width, uint32_t height);
int  BrTexLoadCI4(BrTex *pTex, const char *pszCi4, const char *pszLut,
                  uint32_t width, uint32_t height);
void BrTexFree(BrTex *pTex);

#endif /* BR_N64TEX_H */
