/* icon.c: the disc's Boss.ico as the build's icon.
 *
 * The disc carries one 32x32 16-colour image. On macOS it becomes an .icns
 * of PNGs at every size, each a nearest-neighbour upscale (a smoothing
 * filter turns 32 pixels to mush at 1024), the ICO's AND mask as alpha, as
 * ports/brally/tools/mkicns.py does. On Windows the .ico's images go into the
 * exe as its icon resource. */
#include <stdlib.h>
#include <string.h>
#include "rb_int.h"

#ifdef _WIN32
#include <windows.h>
#endif

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }

/* the largest image of an ICO as RGBA, top row first; malloc'd */
static uint8_t *ico_rgba(const uint8_t *d, size_t n, int *pw, int *ph)
{
    uint32_t cnt, i, best = 0, off = 0, size = 0, hsz, bpp, ncol, xstride, mstride, px, mask;
    int w = 0, h = 0, x, y;
    uint8_t *out;
    const uint8_t *b;
    if (n < 6 || rd16(d + 2) != 1 || !(cnt = rd16(d + 4)) || n < 6 + 16 * cnt)
        return NULL;
    for (i = 0; i < cnt; i++) {
        const uint8_t *e = d + 6 + 16 * i;
        int ew = e[0] ? e[0] : 256, eh = e[1] ? e[1] : 256;
        if ((uint32_t)(ew * eh) > best) {
            best = (uint32_t)(ew * eh);
            w = ew;
            h = eh;
            size = rd32(e + 8);
            off = rd32(e + 12);
        }
    }
    if ((uint64_t)off + size > n || size < 40)
        return NULL;
    b = d + off;
    if (!memcmp(b, "\x89PNG", 4))
        return NULL;                                   /* not what the disc carries */
    hsz = rd32(b);
    bpp = rd16(b + 14);
    ncol = rd32(b + 32);
    if (!ncol && bpp <= 8)
        ncol = 1u << bpp;
    xstride = ((uint32_t)w * bpp + 31) / 32 * 4;
    mstride = ((uint32_t)w + 31) / 32 * 4;
    px = hsz + 4 * ncol;
    mask = px + xstride * (uint32_t)h;
    if (mask + mstride * (uint32_t)h > size || !(out = malloc((size_t)w * (size_t)h * 4)))
        return NULL;
    for (y = 0; y < h; y++) {
        uint32_t src = (uint32_t)(h - 1 - y);          /* BMP rows are bottom-up */
        for (x = 0; x < w; x++) {
            uint8_t *o = out + ((size_t)y * (size_t)w + (size_t)x) * 4;
            const uint8_t *row = b + px + src * xstride;
            if (bpp <= 8) {
                uint32_t bit = (uint32_t)x * bpp, idx = (row[bit / 8] >> (8 - bpp - bit % 8)) & ((1u << bpp) - 1);
                const uint8_t *c = b + hsz + 4 * idx;
                o[0] = c[2], o[1] = c[1], o[2] = c[0];
            } else {
                const uint8_t *c = row + (size_t)x * (bpp / 8);
                o[0] = c[2], o[1] = c[1], o[2] = c[0];
            }
            o[3] = (b[mask + src * mstride + (uint32_t)x / 8] >> (7 - x % 8) & 1) ? 0 : 255;
        }
    }
    *pw = w;
    *ph = h;
    return out;
}

/* ---- PNG: rows filtered Up when they repeat, else Sub; deflate's fixed codes
 * with distance-1 runs, which is all a nearest-neighbour upscale needs ------------- */
typedef struct obuf { uint8_t *p; size_t n, cap; uint32_t acc; int nacc; } obuf;

static void ob_byte(obuf *o, uint8_t v)
{
    if (o->n == o->cap) {
        o->cap = o->cap ? o->cap * 2 : 4096;
        o->p = realloc(o->p, o->cap);
    }
    o->p[o->n++] = v;
}

static void ob_bits(obuf *o, uint32_t v, int n)        /* deflate order: LSB first */
{
    o->acc |= v << o->nacc;
    o->nacc += n;
    while (o->nacc >= 8) {
        ob_byte(o, (uint8_t)o->acc);
        o->acc >>= 8;
        o->nacc -= 8;
    }
}

static void ob_huff(obuf *o, uint32_t code, int n)     /* Huffman codes go MSB first */
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < n; i++)
        r |= ((code >> i) & 1) << (n - 1 - i);
    ob_bits(o, r, n);
}

static void lit(obuf *o, uint32_t v)
{
    if (v < 144)       ob_huff(o, 0x30 + v, 8);
    else if (v < 256)  ob_huff(o, 0x190 + v - 144, 9);
    else if (v < 280)  ob_huff(o, v - 256, 7);
    else               ob_huff(o, 0xC0 + v - 280, 8);
}

static void match(obuf *o, int len)                    /* distance 1, len 3..258 */
{
    static const int base[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
                                67, 83, 99, 115, 131, 163, 195, 227, 258 };
    static const int extra[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    int i = 28;
    while (base[i] > len)
        i--;
    lit(o, 257 + (uint32_t)i);
    ob_bits(o, (uint32_t)(len - base[i]), extra[i]);
    ob_huff(o, 0, 5);                                  /* distance code 0: distance 1 */
}

static uint32_t crc32_of(const uint8_t *p, size_t n, uint32_t c)
{
    size_t i;
    int k;
    c = ~c;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++)
            c = c & 1 ? c >> 1 ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}

static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static void chunk(obuf *o, const char *type, const uint8_t *d, size_t n)
{
    uint8_t h[8];
    size_t s;
    be32(h, (uint32_t)n);
    memcpy(h + 4, type, 4);
    for (s = 0; s < 8; s++)
        ob_byte(o, h[s]);
    for (s = 0; s < n; s++)
        ob_byte(o, d[s]);
    be32(h, crc32_of(d, n, crc32_of((const uint8_t *)type, 4, 0)));
    for (s = 0; s < 4; s++)
        ob_byte(o, h[s]);
}

/* the image scaled by k as a PNG */
static void png_scaled(obuf *png, const uint8_t *rgba, int w, int h, int k)
{
    int W = w * k, H = h * k, y, x, i;
    size_t stride = (size_t)W * 4;
    uint8_t *raw = malloc(((size_t)H) * (stride + 1)), *prev = NULL, *row = malloc(stride), ihdr[13];
    uint32_t a = 1, b = 0;
    obuf z = { 0 };
    size_t n = 0, j;
    for (y = 0; y < H; y++) {
        const uint8_t *src = rgba + (size_t)(y / k) * (size_t)w * 4;
        uint8_t *r = raw + n;
        for (x = 0; x < W; x++)
            memcpy(row + (size_t)x * 4, src + (size_t)(x / k) * 4, 4);
        if (prev && !memcmp(prev, row, stride)) {
            r[0] = 2;                                  /* Up: all zero */
            memset(r + 1, 0, stride);
        } else {
            r[0] = 1;                                  /* Sub */
            for (j = 0; j < stride; j++)
                r[1 + j] = (uint8_t)(row[j] - (j >= 4 ? row[j - 4] : 0));
        }
        if (!prev)
            prev = malloc(stride);
        memcpy(prev, row, stride);
        n += stride + 1;
    }
    /* zlib: one fixed-code block */
    ob_byte(&z, 0x78);
    ob_byte(&z, 0x01);
    ob_bits(&z, 1, 1);                                 /* final */
    ob_bits(&z, 1, 2);                                 /* fixed codes */
    for (j = 0; j < n;) {
        size_t run = 1;
        lit(&z, raw[j]);
        while (j + run < n && raw[j + run] == raw[j] && run < 1 + 258 * 64)
            run++;
        j++;
        run--;
        while (run >= 3) {
            int m = run > 258 ? 258 : (int)run;
            if (run - (size_t)m > 0 && run - (size_t)m < 3)
                m -= 3;
            match(&z, m);
            run -= (size_t)m;
            j += (size_t)m;
        }
        while (run--) {
            lit(&z, raw[j]);
            j++;
        }
    }
    lit(&z, 256);
    if (z.nacc)
        ob_bits(&z, 0, 8 - z.nacc);
    for (j = 0; j < n; j++) {
        a = (a + raw[j]) % 65521;
        b = (b + a) % 65521;
    }
    {
        uint8_t ad[4];
        be32(ad, b << 16 | a);
        for (i = 0; i < 4; i++)
            ob_byte(&z, ad[i]);
    }
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    for (i = 0; i < 8; i++)
        ob_byte(png, sig[i]);
    be32(ihdr, (uint32_t)W);
    be32(ihdr + 4, (uint32_t)H);
    ihdr[8] = 8, ihdr[9] = 6, ihdr[10] = 0, ihdr[11] = 0, ihdr[12] = 0;
    chunk(png, "IHDR", ihdr, 13);
    chunk(png, "IDAT", z.p, z.n);
    chunk(png, "IEND", NULL, 0);
    free(z.p);
    free(raw);
    free(prev);
    free(row);
}

int rb_ico_to_icns(const char *ico, const char *icns, char *err, size_t errlen)
{
    static const struct { const char *type; int px; } E[] = {
        { "icp4", 16 }, { "icp5", 32 }, { "icp6", 64 }, { "ic07", 128 }, { "ic08", 256 },
        { "ic09", 512 }, { "ic10", 1024 }, { "ic11", 32 }, { "ic12", 64 }, { "ic13", 256 }, { "ic14", 512 },
    };
    size_t n, i, k;
    uint8_t *d = rb_read_file(ico, &n), *rgba, h[8];
    obuf out = { 0 };
    int w, hh, ok;
    if (!d || !(rgba = ico_rgba(d, n, &w, &hh))) {
        free(d);
        rb_err(err, errlen, "the disc's icon is unreadable");
        return 0;
    }
    for (k = 0; k < 8; k++)
        ob_byte(&out, 0);                               /* header, filled in below */
    for (i = 0; i < sizeof E / sizeof E[0]; i++) {
        obuf png = { 0 };
        int scale = E[i].px / w > 0 ? E[i].px / w : 1;
        if (E[i].px < w)
            continue;                                   /* no downscaling */
        png_scaled(&png, rgba, w, hh, scale);
        memcpy(h, E[i].type, 4);
        be32(h + 4, (uint32_t)(png.n + 8));
        for (k = 0; k < 8; k++)
            ob_byte(&out, h[k]);
        for (k = 0; k < png.n; k++)
            ob_byte(&out, png.p[k]);
        free(png.p);
    }
    memcpy(out.p, "icns", 4);
    be32(out.p + 4, (uint32_t)out.n);
    ok = rb_write_file(icns, out.p, out.n);
    free(out.p);
    free(rgba);
    free(d);
    if (!ok)
        rb_err(err, errlen, "cannot write %s", icns);
    return ok;
}

#ifdef _WIN32
#pragma pack(push, 2)
typedef struct { BYTE w, h, colors, reserved; WORD planes, bpp; DWORD bytes; WORD id; } GRPENT;
#pragma pack(pop)

static wchar_t *wide_path(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = malloc(sizeof(wchar_t) * (size_t)(n > 0 ? n : 1));
    if (w)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n > 0 ? n : 1);
    return w;
}

int rb_set_exe_icon(const char *exe, const char *ico, char *err, size_t errlen)
{
    size_t n;
    uint8_t *d = rb_read_file(ico, &n), *grp;
    wchar_t *w;
    HANDLE u;
    uint32_t cnt, i;
    int ok = 1;
    if (!d || n < 6 || !(cnt = rd16(d + 4)) || n < 6 + 16 * cnt) {
        free(d);
        rb_err(err, errlen, "the disc's icon is unreadable");
        return 0;
    }
    w = wide_path(exe);
    u = w ? BeginUpdateResourceW(w, FALSE) : NULL;
    free(w);
    if (!u) {
        free(d);
        rb_err(err, errlen, "cannot set the game's icon");
        return 0;
    }
    grp = calloc(1, 6 + sizeof(GRPENT) * cnt);
    memcpy(grp, d, 6);
    for (i = 0; i < cnt && ok; i++) {
        const uint8_t *e = d + 6 + 16 * i;
        GRPENT g;
        uint32_t size = rd32(e + 8), off = rd32(e + 12);
        if ((uint64_t)off + size > n) {
            ok = 0;
            break;
        }
        memcpy(&g, e, 8);
        g.bytes = size;
        g.id = (WORD)(i + 1);
        memcpy(grp + 6 + sizeof(GRPENT) * i, &g, sizeof g);
        ok = UpdateResourceW(u, (LPCWSTR)RT_ICON, MAKEINTRESOURCEW(i + 1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                             (void *)(d + off), size) != 0;
    }
    if (ok)
        ok = UpdateResourceW(u, (LPCWSTR)RT_GROUP_ICON, MAKEINTRESOURCEW(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                             grp, (DWORD)(6 + sizeof(GRPENT) * cnt)) != 0;
    ok = EndUpdateResourceW(u, !ok) && ok;
    free(grp);
    free(d);
    if (!ok)
        rb_err(err, errlen, "cannot set the game's icon");
    return ok;
}
#endif
