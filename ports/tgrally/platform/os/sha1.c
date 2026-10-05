/* sha1.c: SHA-1, for the digests the lockstep trace compares with n64box's
 * (Python's hashlib.sha1). */
#include <stdint.h>
#include <string.h>
#include "sha1.h"

#define ROL(v, b) (((v) << (b)) | ((v) >> (32 - (b))))

static void block(uint32_t h[5], const uint8_t *p)
{
    uint32_t w[80], a, b, c, d, e, t;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (; i < 80; i++)
        w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
    for (i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                    k = 0xCA62C1D6; }
        t = ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void sha1_init(Sha1 *s)
{
    s->h[0] = 0x67452301; s->h[1] = 0xEFCDAB89; s->h[2] = 0x98BADCFE; s->h[3] = 0x10325476; s->h[4] = 0xC3D2E1F0;
    s->len = 0;
    s->n = 0;
}

void sha1_update(Sha1 *s, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    s->len += len;
    while (len) {
        size_t take = 64 - s->n;
        if (take > len)
            take = len;
        memcpy(s->buf + s->n, p, take);
        s->n += take;
        p += take;
        len -= take;
        if (s->n == 64) {
            block(s->h, s->buf);
            s->n = 0;
        }
    }
}

void sha1_hex16(Sha1 *s, char out[17])
{
    uint64_t bits = s->len * 8;
    uint8_t pad = 0x80, z = 0, l[8];
    int i;
    sha1_update(s, &pad, 1);
    while (s->n != 56)
        sha1_update(s, &z, 1);
    for (i = 0; i < 8; i++)
        l[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_update(s, l, 8);
    for (i = 0; i < 8; i++)
        sprintf(out + 2 * i, "%02x", (unsigned)(s->h[i / 4] >> (24 - 8 * (i % 4))) & 0xFF);
    out[16] = 0;
}
