/* br_bmp.c -- the menu's sprite sheets.
 *
 * The 145-entry sprite table (br_uispr.c) names 24-bit uncompressed BMPs that
 * ship in IMAGES\ on the retail disc. Every menu picture, button and font sheet
 * is one of these, so without a decoder the chrome can only be drawn as
 * placeholder rectangles.
 *
 * CONFIRMATION THAT THE TABLE IS RIGHT: but-sav.bmp is 127x33, which is exactly
 * the +0x7F / +0x21 rectangle every builder computes for a button. Three
 * earlier facts already landed on those numbers -- the builder's arithmetic, the
 * sprite table's rect, and the loader's string/store pairing that recovered the
 * name. The file's own header is the fourth, and the first from outside the
 * executable.
 *
 * FORMAT NOTES, all read from the files rather than assumed:
 *   - BITMAPINFOHEADER (40 bytes), biCompression 0, 24 bpp.
 *   - Rows are BOTTOM-UP when biHeight is positive, which these are. This
 *     decoder flips them, so row 0 of the output is the TOP row -- the order
 *     the sprite table's rectangles and the blitter both assume.
 *   - Each row is padded to a 4-byte boundary. The padding is not optional and
 *     a decoder that ignores it shears every image whose width is not a
 *     multiple of 4. 127*3 == 381, so but-sav.bmp shears immediately if it is
 *     got wrong -- which makes it a good canary.
 *   - Pixels are stored B,G,R.
 *
 * Colour-keying is NOT applied here. The blitter (0x10001320) keys on the
 * sprite table entry's flag bit, not on anything in the file, so that decision
 * belongs to the caller which has the table entry in hand.
 */
#include "br_bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int BrBmpLoad(BrBmp *pOut, const char *pszPath)
{
    FILE    *fh;
    uint8_t  hdr[54];
    uint8_t *pRaw = NULL;
    uint32_t off, dib, cb;
    int32_t  w, h, y;
    uint16_t bpp;
    size_t   stride, need;

    if (!pOut || !pszPath) return -1;
    memset(pOut, 0, sizeof(*pOut));

    fh = fopen(pszPath, "rb");
    if (!fh) return -1;
    if (fread(hdr, 1, sizeof hdr, fh) != sizeof hdr) { fclose(fh); return -1; }
    if (hdr[0] != 'B' || hdr[1] != 'M')              { fclose(fh); return -1; }

    off = rd32(hdr + 10);
    dib = rd32(hdr + 14);
    w   = (int32_t)rd32(hdr + 18);
    h   = (int32_t)rd32(hdr + 22);
    bpp = rd16(hdr + 28);

    /* Only the one format the game actually ships. Refuse anything else rather
     * than decode it wrongly and produce plausible garbage. */
    if (dib < 40 || bpp != 24 || rd32(hdr + 30) != 0) { fclose(fh); return -1; }
    if (w <= 0 || w > 4096 || h == 0 || h < -4096 || h > 4096) {
        fclose(fh); return -1;
    }

    stride = ((size_t)w * 3u + 3u) & ~(size_t)3u;   /* 4-byte row padding */
    need   = stride * (size_t)(h < 0 ? -h : h);

    if (fseek(fh, 0, SEEK_END) != 0) { fclose(fh); return -1; }
    cb = (uint32_t)ftell(fh);
    if (cb < off || (size_t)(cb - off) < need)      { fclose(fh); return -1; }

    pRaw = (uint8_t *)malloc(need);
    if (!pRaw) { fclose(fh); return -1; }
    if (fseek(fh, (long)off, SEEK_SET) != 0 || fread(pRaw, 1, need, fh) != need) {
        free(pRaw); fclose(fh); return -1;
    }
    fclose(fh);

    pOut->w = (uint32_t)w;
    pOut->h = (uint32_t)(h < 0 ? -h : h);
    pOut->pRgba = (uint8_t *)malloc((size_t)pOut->w * pOut->h * 4u);
    if (!pOut->pRgba) { free(pRaw); return -1; }

    for (y = 0; y < (int32_t)pOut->h; y++) {
        /* Positive biHeight means the FIRST stored row is the BOTTOM one. */
        const uint8_t *pSrc = pRaw + (size_t)(h > 0 ? (int32_t)pOut->h - 1 - y : y)
                                     * stride;
        uint8_t       *pDst = pOut->pRgba + (size_t)y * pOut->w * 4u;
        uint32_t       x;
        for (x = 0; x < pOut->w; x++) {
            pDst[x * 4 + 0] = pSrc[x * 3 + 2];   /* stored B,G,R */
            pDst[x * 4 + 1] = pSrc[x * 3 + 1];
            pDst[x * 4 + 2] = pSrc[x * 3 + 0];
            pDst[x * 4 + 3] = 0xFF;
        }
    }
    free(pRaw);
    return 0;
}

void BrBmpApplyKey(BrBmp *pBmp, uint32_t rgb)
{
    size_t n, i;
    if (!pBmp || !pBmp->pRgba) return;
    n = (size_t)pBmp->w * pBmp->h;
    for (i = 0; i < n; i++) {
        uint8_t *p = pBmp->pRgba + i * 4u;
        uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
        if (v == rgb) p[3] = 0;
    }
}

void BrBmpFree(BrBmp *pBmp)
{
    if (!pBmp) return;
    free(pBmp->pRgba);
    pBmp->pRgba = NULL;
    pBmp->w = pBmp->h = 0;
}
