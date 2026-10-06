/* md5.c: MD5 (RFC 1321), for checking dumps and for FLAC's STREAMINFO. */
#include <string.h>
#include "rb.h"

#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define ROTL(x, s) (((x) << (s)) | ((x) >> (32 - (s))))
#define STEP(f, a, b, c, d, x, t, s) (a) += f((b), (c), (d)) + (x) + (t); (a) = ROTL((a), (s)) + (b)

static void block(rb_md5 *m, const uint8_t *p)
{
    uint32_t x[16], a = m->a, b = m->b, c = m->c, d = m->d;
    int i;
    for (i = 0; i < 16; i++)
        x[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
    STEP(F, a, b, c, d, x[0], 0xd76aa478, 7);   STEP(F, d, a, b, c, x[1], 0xe8c7b756, 12);
    STEP(F, c, d, a, b, x[2], 0x242070db, 17);  STEP(F, b, c, d, a, x[3], 0xc1bdceee, 22);
    STEP(F, a, b, c, d, x[4], 0xf57c0faf, 7);   STEP(F, d, a, b, c, x[5], 0x4787c62a, 12);
    STEP(F, c, d, a, b, x[6], 0xa8304613, 17);  STEP(F, b, c, d, a, x[7], 0xfd469501, 22);
    STEP(F, a, b, c, d, x[8], 0x698098d8, 7);   STEP(F, d, a, b, c, x[9], 0x8b44f7af, 12);
    STEP(F, c, d, a, b, x[10], 0xffff5bb1, 17); STEP(F, b, c, d, a, x[11], 0x895cd7be, 22);
    STEP(F, a, b, c, d, x[12], 0x6b901122, 7);  STEP(F, d, a, b, c, x[13], 0xfd987193, 12);
    STEP(F, c, d, a, b, x[14], 0xa679438e, 17); STEP(F, b, c, d, a, x[15], 0x49b40821, 22);
    STEP(G, a, b, c, d, x[1], 0xf61e2562, 5);   STEP(G, d, a, b, c, x[6], 0xc040b340, 9);
    STEP(G, c, d, a, b, x[11], 0x265e5a51, 14); STEP(G, b, c, d, a, x[0], 0xe9b6c7aa, 20);
    STEP(G, a, b, c, d, x[5], 0xd62f105d, 5);   STEP(G, d, a, b, c, x[10], 0x02441453, 9);
    STEP(G, c, d, a, b, x[15], 0xd8a1e681, 14); STEP(G, b, c, d, a, x[4], 0xe7d3fbc8, 20);
    STEP(G, a, b, c, d, x[9], 0x21e1cde6, 5);   STEP(G, d, a, b, c, x[14], 0xc33707d6, 9);
    STEP(G, c, d, a, b, x[3], 0xf4d50d87, 14);  STEP(G, b, c, d, a, x[8], 0x455a14ed, 20);
    STEP(G, a, b, c, d, x[13], 0xa9e3e905, 5);  STEP(G, d, a, b, c, x[2], 0xfcefa3f8, 9);
    STEP(G, c, d, a, b, x[7], 0x676f02d9, 14);  STEP(G, b, c, d, a, x[12], 0x8d2a4c8a, 20);
    STEP(H, a, b, c, d, x[5], 0xfffa3942, 4);   STEP(H, d, a, b, c, x[8], 0x8771f681, 11);
    STEP(H, c, d, a, b, x[11], 0x6d9d6122, 16); STEP(H, b, c, d, a, x[14], 0xfde5380c, 23);
    STEP(H, a, b, c, d, x[1], 0xa4beea44, 4);   STEP(H, d, a, b, c, x[4], 0x4bdecfa9, 11);
    STEP(H, c, d, a, b, x[7], 0xf6bb4b60, 16);  STEP(H, b, c, d, a, x[10], 0xbebfbc70, 23);
    STEP(H, a, b, c, d, x[13], 0x289b7ec6, 4);  STEP(H, d, a, b, c, x[0], 0xeaa127fa, 11);
    STEP(H, c, d, a, b, x[3], 0xd4ef3085, 16);  STEP(H, b, c, d, a, x[6], 0x04881d05, 23);
    STEP(H, a, b, c, d, x[9], 0xd9d4d039, 4);   STEP(H, d, a, b, c, x[12], 0xe6db99e5, 11);
    STEP(H, c, d, a, b, x[15], 0x1fa27cf8, 16); STEP(H, b, c, d, a, x[2], 0xc4ac5665, 23);
    STEP(I, a, b, c, d, x[0], 0xf4292244, 6);   STEP(I, d, a, b, c, x[7], 0x432aff97, 10);
    STEP(I, c, d, a, b, x[14], 0xab9423a7, 15); STEP(I, b, c, d, a, x[5], 0xfc93a039, 21);
    STEP(I, a, b, c, d, x[12], 0x655b59c3, 6);  STEP(I, d, a, b, c, x[3], 0x8f0ccc92, 10);
    STEP(I, c, d, a, b, x[10], 0xffeff47d, 15); STEP(I, b, c, d, a, x[1], 0x85845dd1, 21);
    STEP(I, a, b, c, d, x[8], 0x6fa87e4f, 6);   STEP(I, d, a, b, c, x[15], 0xfe2ce6e0, 10);
    STEP(I, c, d, a, b, x[6], 0xa3014314, 15);  STEP(I, b, c, d, a, x[13], 0x4e0811a1, 21);
    STEP(I, a, b, c, d, x[4], 0xf7537e82, 6);   STEP(I, d, a, b, c, x[11], 0xbd3af235, 10);
    STEP(I, c, d, a, b, x[2], 0x2ad7d2bb, 15);  STEP(I, b, c, d, a, x[9], 0xeb86d391, 21);
    m->a += a;
    m->b += b;
    m->c += c;
    m->d += d;
}

void rb_md5_init(rb_md5 *m)
{
    m->a = 0x67452301;
    m->b = 0xefcdab89;
    m->c = 0x98badcfe;
    m->d = 0x10325476;
    m->len = 0;
}

void rb_md5_update(rb_md5 *m, const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t have = (size_t)(m->len & 63);
    m->len += n;
    if (have) {
        size_t take = 64 - have < n ? 64 - have : n;
        memcpy(m->buf + have, p, take);
        p += take;
        n -= take;
        if (have + take < 64)
            return;
        block(m, m->buf);
    }
    while (n >= 64) {
        block(m, p);
        p += 64;
        n -= 64;
    }
    memcpy(m->buf, p, n);
}

void rb_md5_final(rb_md5 *m, uint8_t out[16])
{
    static const uint8_t pad[64] = { 0x80 };
    uint8_t len[8];
    uint64_t bits = m->len * 8;
    size_t have = (size_t)(m->len & 63);
    int i;
    for (i = 0; i < 8; i++)
        len[i] = (uint8_t)(bits >> (8 * i));
    rb_md5_update(m, pad, have < 56 ? 56 - have : 120 - have);
    rb_md5_update(m, len, 8);
    for (i = 0; i < 4; i++) {
        uint32_t v = i == 0 ? m->a : i == 1 ? m->b : i == 2 ? m->c : m->d;
        out[4 * i] = (uint8_t)v;
        out[4 * i + 1] = (uint8_t)(v >> 8);
        out[4 * i + 2] = (uint8_t)(v >> 16);
        out[4 * i + 3] = (uint8_t)(v >> 24);
    }
}

void rb_md5_hex(const uint8_t d[16], char out[33])
{
    static const char hx[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 16; i++) {
        out[2 * i] = hx[d[i] >> 4];
        out[2 * i + 1] = hx[d[i] & 15];
    }
    out[32] = 0;
}
