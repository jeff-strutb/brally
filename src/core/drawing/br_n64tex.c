/* br_n64tex.c -- CI4/LUT4 decoder (portable C99). See br_n64tex.h. */
#include "br_n64tex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t expand5(uint32_t v)
{
    v &= 0x1Fu;
    return (uint8_t)((v << 3) | (v >> 2));
}

int BrTexDecodeCI4(BrTex *pTex, const void *pvCi4, size_t cbCi4,
                   const void *pvLut, size_t cbLut,
                   uint32_t width, uint32_t height)
{
    const unsigned char *ci = (const unsigned char *)pvCi4;
    const unsigned char *lu = (const unsigned char *)pvLut;
    uint8_t pal[BR_LUT4_ENTRIES][4];
    uint32_t i, count;

    memset(pTex, 0, sizeof(*pTex));
    if (width == 0 || height == 0)
        return 1;
    count = width * height;
    if (cbLut < BR_LUT4_ENTRIES * 2 || cbCi4 < (size_t)count / 2)
        return 1;

    for (i = 0; i < BR_LUT4_ENTRIES; i++) {
        /* big-endian RGBA5551 */
        uint32_t v = ((uint32_t)lu[i * 2] << 8) | lu[i * 2 + 1];
        pal[i][0] = expand5(v >> 11);
        pal[i][1] = expand5(v >> 6);
        pal[i][2] = expand5(v >> 1);
        pal[i][3] = (v & 1u) ? 255 : 0;
    }

    pTex->pixels = (uint8_t *)malloc((size_t)count * 4);
    if (pTex->pixels == NULL)
        return 1;
    for (i = 0; i < count; i++) {
        unsigned char byte = ci[i >> 1];
        unsigned idx = (i & 1) ? (byte & 0x0Fu) : (byte >> 4);  /* high = left */
        memcpy(pTex->pixels + i * 4, pal[idx], 4);
    }
    pTex->width = width;
    pTex->height = height;
    return 0;
}

static void *slurp(const char *pszPath, size_t *pcb)
{
    FILE *f = fopen(pszPath, "rb");
    long cb; void *buf;
    if (f == NULL) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    cb = ftell(f); rewind(f);
    if (cb <= 0) { fclose(f); return NULL; }
    buf = malloc((size_t)cb);
    if (buf && fread(buf, 1, (size_t)cb, f) != (size_t)cb) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) *pcb = (size_t)cb;
    return buf;
}

int BrTexLoadCI4(BrTex *pTex, const char *pszCi4, const char *pszLut,
                 uint32_t width, uint32_t height)
{
    size_t cbC = 0, cbL = 0;
    void *c = slurp(pszCi4, &cbC), *l = slurp(pszLut, &cbL);
    int rc = 1;
    if (c && l) rc = BrTexDecodeCI4(pTex, c, cbC, l, cbL, width, height);
    free(c); free(l);
    return rc;
}

void BrTexFree(BrTex *pTex)
{
    free(pTex->pixels);
    memset(pTex, 0, sizeof(*pTex));
}
