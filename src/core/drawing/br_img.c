/* br_img.c -- Boss Rally .img loader (portable C99). See br_img.h. */
#include "br_img.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BR_IMG_HEADER_SIZE 12
/* Guard against absurd headers; the largest retail image is 256x256. */
#define BR_IMG_MAX_DIM     8192u

static uint32_t rd_u32le(const unsigned char *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* 5-bit channel to 8-bit, replicating the high bits so 31 maps to 255. */
static uint8_t expand5(uint32_t v)
{
    v &= 0x1Fu;
    return (uint8_t)((v << 3) | (v >> 2));
}

int BrImgDecode(BrImage *pImg, const void *pvData, size_t cbData)
{
    const unsigned char *p = (const unsigned char *)pvData;
    uint32_t w, h, fmt, i, count;
    int argb;

    memset(pImg, 0, sizeof(*pImg));
    if (cbData < BR_IMG_HEADER_SIZE)
        return 1;

    w   = rd_u32le(p + 0);
    h   = rd_u32le(p + 4);
    fmt = rd_u32le(p + 8);

    if (w == 0 || h == 0 || w > BR_IMG_MAX_DIM || h > BR_IMG_MAX_DIM)
        return 1;
    if (fmt != BR_IMG_FORMAT_ARGB1555)
        return 1;                      /* no other format seen in retail data */

    count = w * h;
    if (cbData - BR_IMG_HEADER_SIZE < (size_t)count * 2)
        return 1;

    /* Detect the channel order.
     *
     * The header format field is 0x1555 in every retail .img, yet the files do
     * NOT agree on layout: splash.img is RGBA5551 (alpha in bit 0) and
     * loading.img is ARGB1555 (alpha in bit 15). Both decode to garbage under
     * the other interpretation, so the format word cannot be the discriminator
     * and the original must select the layout at the call site.
     *
     * Until that call site is decompiled, detect it from the data. The signal
     * is unambiguous because an alpha bit is nearly constant while a colour bit
     * is not: in splash.img every one of the 9759 nonzero pixels has bit 0 set,
     * and in loading.img all 51200 pixels have bit 15 set. A real colour LSB or
     * MSB would sit near 50%.
     *
     * TODO: replace with the flag the original passes, once the loader that
     * references "splash.img"/"loading.img" in BRD3D.dll is decompiled.
     */
    {
        const unsigned char *q = p + BR_IMG_HEADER_SIZE;
        uint32_t hi = 0, nonzero = 0, lo_nonzero = 0;
        for (i = 0; i < count; i++) {
            uint32_t v = (uint32_t)q[i * 2] | ((uint32_t)q[i * 2 + 1] << 8);
            if (v & 0x8000u) hi++;
            if (v) {
                nonzero++;
                if (v & 1u) lo_nonzero++;
            }
        }
        /* bit 0 behaves like alpha and bit 15 does not => RGBA5551 */
        argb = !(nonzero > 0 && lo_nonzero == nonzero && hi < count);
    }

    pImg->pixels = (uint8_t *)malloc((size_t)count * 4);
    if (pImg->pixels == NULL)
        return 1;

    p += BR_IMG_HEADER_SIZE;
    for (i = 0; i < count; i++) {
        uint32_t v = (uint32_t)p[i * 2] | ((uint32_t)p[i * 2 + 1] << 8);
        uint8_t *o = pImg->pixels + i * 4;
        if (argb) {                        /* A1 R5 G5 B5 */
            o[0] = expand5(v >> 10);
            o[1] = expand5(v >> 5);
            o[2] = expand5(v);
            o[3] = (v & 0x8000u) ? 255 : 0;
        } else {                           /* R5 G5 B5 A1 */
            o[0] = expand5(v >> 11);
            o[1] = expand5(v >> 6);
            o[2] = expand5(v >> 1);
            o[3] = (v & 1u) ? 255 : 0;
        }
    }
    pImg->width  = w;
    pImg->height = h;
    return 0;
}

int BrImgLoad(BrImage *pImg, const char *pszPath)
{
    FILE *f;
    long cb;
    void *buf;
    int rc;

    memset(pImg, 0, sizeof(*pImg));
    f = fopen(pszPath, "rb");
    if (f == NULL)
        return 1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 1; }
    cb = ftell(f);
    if (cb <= 0) { fclose(f); return 1; }
    rewind(f);

    buf = malloc((size_t)cb);
    if (buf == NULL) { fclose(f); return 1; }
    if (fread(buf, 1, (size_t)cb, f) != (size_t)cb) {
        free(buf); fclose(f); return 1;
    }
    fclose(f);

    rc = BrImgDecode(pImg, buf, (size_t)cb);
    free(buf);
    return rc;
}

/* @n64 0x8021DE2C located */
void BrImgFree(BrImage *pImg)
{
    free(pImg->pixels);
    memset(pImg, 0, sizeof(*pImg));
}
