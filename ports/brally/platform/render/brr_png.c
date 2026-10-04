/* brr_png.c: a frame to a PNG file, with no library: stored (uncompressed)
 * deflate blocks, so any renderer on any OS can write its shots. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr_png.h"

static uint32_t s_crc[256];

static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    size_t i;
    if (!s_crc[1]) {
        uint32_t k, b;
        for (k = 0; k < 256; k++) {
            uint32_t v = k;
            for (b = 0; b < 8; b++)
                v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            s_crc[k] = v;
        }
    }
    for (i = 0; i < n; i++)
        c = s_crc[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t h[8];
    uint32_t c;
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n)
        fwrite(data, 1, n, f);
    c = crc(0xFFFFFFFFu, (const uint8_t *)type, 4);
    c = crc(c, data, n) ^ 0xFFFFFFFFu;
    be32(h, c);
    fwrite(h, 1, 4, f);
}

int brr_png_write(const char *path, const uint8_t *px, int w, int h, int stride, int order)
{
    size_t row = (size_t)w * 3 + 1, raw_n = row * (size_t)h, nblk = (raw_n + 65534) / 65535;
    size_t z_n = 2 + raw_n + nblk * 5 + 4, i, at;
    uint8_t *raw = (uint8_t *)malloc(raw_n), *z = (uint8_t *)malloc(z_n), ihdr[13];
    uint32_t a = 1, b = 0;
    FILE *f;
    int x, y;
    if (!raw || !z) {
        free(raw);
        free(z);
        return 0;
    }
    for (y = 0; y < h; y++) {
        uint8_t *o = raw + row * (size_t)y;
        const uint8_t *s = px + (size_t)stride * (size_t)y;
        *o++ = 0;                                   /* filter: none */
        for (x = 0; x < w; x++, s += 4) {
            if (order == BRR_PNG_BGRA) {
                *o++ = s[2];
                *o++ = s[1];
                *o++ = s[0];
            } else {
                *o++ = s[0];
                *o++ = s[1];
                *o++ = s[2];
            }
        }
    }
    z[0] = 0x78;
    z[1] = 0x01;
    at = 2;
    for (i = 0; i < raw_n; i += 65535) {
        size_t n = raw_n - i < 65535 ? raw_n - i : 65535;
        z[at++] = i + n == raw_n;
        z[at++] = (uint8_t)n;
        z[at++] = (uint8_t)(n >> 8);
        z[at++] = (uint8_t)~n;
        z[at++] = (uint8_t)(~n >> 8);
        memcpy(z + at, raw + i, n);
        at += n;
    }
    for (i = 0; i < raw_n; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    be32(z + at, b << 16 | a);
    at += 4;
    f = fopen(path, "wb");
    if (f) {
        static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        fwrite(sig, 1, 8, f);
        be32(ihdr, (uint32_t)w);
        be32(ihdr + 4, (uint32_t)h);
        ihdr[8] = 8;                                /* bit depth */
        ihdr[9] = 2;                                /* RGB */
        ihdr[10] = ihdr[11] = ihdr[12] = 0;
        chunk(f, "IHDR", ihdr, 13);
        chunk(f, "IDAT", z, (uint32_t)at);
        chunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(raw);
    free(z);
    return f != NULL;
}
