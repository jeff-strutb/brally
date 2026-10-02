/* brr_soft.c: the reference renderer backend, in portable C.
 *
 * It rasterises what glide.c hands it the way a Voodoo Graphics board would:
 * Glide's colour, alpha and texture combiners, alpha test, alpha blending,
 * depth test and table fog, evaluated per pixel into a 32-bit colour buffer
 * and a float depth buffer. It is slow and exact on purpose -- the backends
 * for real graphics APIs are checked against its output.
 *
 *   BR_SHOT=<frame>:<file.png>   write frame <frame> (counted from 1) to a PNG
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr.h"

typedef struct { float r, g, b, a; } rgba;

typedef struct stex { int w, h; uint8_t *px; } stex;

static int s_w, s_h;
static uint32_t *s_col;          /* 0xAARRGGBB, being drawn */
static uint32_t *s_shown;        /* the frame last presented */
static float *s_dep;
static stex *s_tex;
static uint32_t s_ntex;
static unsigned long s_frame, s_tris;
static long s_shot_frame = -1;
static char s_shot_path[1024];

/* ---- PNG: stored deflate blocks, no compression library needed ------------------- */
static uint32_t crc_tab[256];

static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    size_t i;
    if (!crc_tab[1]) {
        uint32_t k, j, v;
        for (k = 0; k < 256; k++) {
            for (v = k, j = 0; j < 8; j++)
                v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_tab[k] = v;
        }
    }
    c = ~c;
    for (i = 0; i < n; i++)
        c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t h[8];
    uint32_t c;
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n)
        fwrite(data, 1, n, f);
    c = crc(crc(0, (const uint8_t *)type, 4), data, n);
    be32(h, c);
    fwrite(h, 1, 4, f);
}

static int write_png(const char *path)
{
    size_t row = (size_t)s_w * 3 + 1, raw_n = row * (size_t)s_h, nblk = (raw_n + 65534) / 65535;
    uint8_t *raw = (uint8_t *)malloc(raw_n), *z = (uint8_t *)malloc(raw_n + nblk * 5 + 6), ihdr[13];
    uint32_t a = 1, b = 0;
    size_t i, o = 0;
    int x, y;
    FILE *f = fopen(path, "wb");
    if (!f || !raw || !z) {
        if (f) fclose(f);
        free(raw);
        free(z);
        return 0;
    }
    for (y = 0; y < s_h; y++) {
        uint8_t *r = raw + row * (size_t)y;
        r[0] = 0;
        for (x = 0; x < s_w; x++) {
            uint32_t c = s_col[y * s_w + x];
            r[1 + 3 * x] = (uint8_t)(c >> 16);
            r[2 + 3 * x] = (uint8_t)(c >> 8);
            r[3 + 3 * x] = (uint8_t)c;
        }
    }
    z[o++] = 0x78;
    z[o++] = 0x01;
    for (i = 0; i < raw_n; i += 65535) {
        size_t n = raw_n - i < 65535 ? raw_n - i : 65535;
        z[o++] = i + n == raw_n;
        z[o++] = (uint8_t)n;
        z[o++] = (uint8_t)(n >> 8);
        z[o++] = (uint8_t)~n;
        z[o++] = (uint8_t)(~n >> 8);
        memcpy(z + o, raw + i, n);
        o += n;
    }
    for (i = 0; i < raw_n; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    be32(z + o, b << 16 | a);
    o += 4;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(ihdr, (uint32_t)s_w);
    be32(ihdr + 4, (uint32_t)s_h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)o);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    return 1;
}

/* ---- the interface ------------------------------------------------------------------ */
int brr_open(int width, int height)
{
    const char *shot = getenv("BR_SHOT");
    s_w = width;
    s_h = height;
    free(s_col);
    free(s_dep);
    s_col = (uint32_t *)calloc((size_t)width * (size_t)height, 4);
    s_dep = (float *)calloc((size_t)width * (size_t)height, sizeof(float));
    if (shot) {
        const char *c = strchr(shot, ':');
        if (c) {
            s_shot_frame = atol(shot);
            snprintf(s_shot_path, sizeof s_shot_path, "%s", c + 1);
        }
    }
    fprintf(stderr, "brr: software renderer %dx%d\n", width, height);
    return s_col && s_dep;
}

void brr_close(void)
{
    fprintf(stderr, "brr: %lu frames, %lu triangles\n", s_frame, s_tris);
}

uint32_t brr_texture(uint32_t id, const uint8_t *px, int w, int h)
{
    stex *t;
    if (id == 0) {                 /* reuse a freed slot first */
        uint32_t i;
        for (i = 0; i < s_ntex; i++)
            if (!s_tex[i].px && !s_tex[i].w) {
                id = i + 1;
                break;
            }
    }
    if (id == 0 || id > s_ntex) {
        stex *n = (stex *)realloc(s_tex, (s_ntex + 1) * sizeof *s_tex);
        if (!n)
            return 0;
        s_tex = n;
        id = ++s_ntex;
        memset(&s_tex[id - 1], 0, sizeof *s_tex);
    }
    t = &s_tex[id - 1];
    free(t->px);
    t->px = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
    if (t->px)
        memcpy(t->px, px, (size_t)w * (size_t)h * 4);
    t->w = w;
    t->h = h;
    return id;
}

void brr_texture_free(uint32_t id)
{
    if (id && id <= s_ntex) {
        free(s_tex[id - 1].px);
        s_tex[id - 1].px = NULL;
        s_tex[id - 1].w = s_tex[id - 1].h = 0;
    }
}

static void clip_rect(const brr_state *st, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = st && st->clip_x0 > 0 ? st->clip_x0 : 0;
    *y0 = st && st->clip_y0 > 0 ? st->clip_y0 : 0;
    *x1 = st && st->clip_x1 < s_w ? st->clip_x1 : s_w;
    *y1 = st && st->clip_y1 < s_h ? st->clip_y1 : s_h;
}

void brr_clear(uint32_t argb, float depth, int colour, int depthbuf, const brr_state *st)
{
    int x0, y0, x1, y1, x, y;
    clip_rect(st, &x0, &y0, &x1, &y1);
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            if (colour)
                s_col[y * s_w + x] = argb;
            if (depthbuf)
                s_dep[y * s_w + x] = depth;
        }
}

/* ---- Glide's combine unit -------------------------------------------------------------- */
enum { F_ZERO, F_LOCAL, F_OTHER_ALPHA, F_LOCAL_ALPHA, F_TEXTURE_ALPHA, F_TEXTURE_RGB,
       F_ONE = 8, F_ONE_MINUS_LOCAL, F_ONE_MINUS_OTHER_ALPHA, F_ONE_MINUS_LOCAL_ALPHA,
       F_ONE_MINUS_TEXTURE_ALPHA, F_ONE_MINUS_LOD_FRACTION };

static float clamp1(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* one channel of grColorCombine / grAlphaCombine / grTexCombine:
 * the function of local, other and the factor, then the optional invert */
static float combine(int fn, float f, float local, float local_a, float other, int invert)
{
    float r;
    switch (fn) {
    case 0x0: r = 0; break;
    case 0x1: r = local; break;
    case 0x2: r = local_a; break;
    case 0x3: r = f * other; break;
    case 0x4: r = f * other + local; break;
    case 0x5: r = f * other + local_a; break;
    case 0x6: r = f * (other - local); break;
    case 0x7: r = f * (other - local) + local; break;
    case 0x8: r = f * (other - local) + local_a; break;
    case 0x9: r = -f * local + local; break;
    case 0x10: r = -f * local + local_a; break;
    default: r = local; break;
    }
    r = clamp1(r);
    return invert ? 1.0f - r : r;
}

static float factor(int k, float local, float local_a, float other_a, float tex_a)
{
    switch (k) {
    case F_ZERO: return 0;
    case F_LOCAL: return local;
    case F_OTHER_ALPHA: return other_a;
    case F_LOCAL_ALPHA: return local_a;
    case F_TEXTURE_ALPHA: return tex_a;
    case F_TEXTURE_RGB: return 0;               /* LOD fraction: one level here */
    case F_ONE: return 1;
    case F_ONE_MINUS_LOCAL: return 1 - local;
    case F_ONE_MINUS_OTHER_ALPHA: return 1 - other_a;
    case F_ONE_MINUS_LOCAL_ALPHA: return 1 - local_a;
    case F_ONE_MINUS_TEXTURE_ALPHA: return 1 - tex_a;
    case F_ONE_MINUS_LOD_FRACTION: return 1;
    default: return 0;
    }
}

static rgba texel_at(const stex *t, int x, int y, const brr_state *st)
{
    rgba c;
    const uint8_t *p;
    /* wrap by modulo: Glide sizes are powers of two, but a texture the game
     * built at another size must still never be read outside its pixels */
    if (st->clamp_s)
        x = x < 0 ? 0 : x >= t->w ? t->w - 1 : x;
    else
        x = (int)(((long)x % t->w + t->w) % t->w);
    if (st->clamp_t)
        y = y < 0 ? 0 : y >= t->h ? t->h - 1 : y;
    else
        y = (int)(((long)y % t->h + t->h) % t->h);
    p = t->px + ((size_t)y * (size_t)t->w + (size_t)x) * 4;
    c.r = p[0] / 255.0f;
    c.g = p[1] / 255.0f;
    c.b = p[2] / 255.0f;
    c.a = p[3] / 255.0f;
    return c;
}

static rgba sample(const brr_state *st, float s, float t)
{
    const stex *tx = st->texture && st->texture <= s_ntex ? &s_tex[st->texture - 1] : NULL;
    rgba c = { 1, 1, 1, 1 };
    float u, v, fu, fv;
    int x, y;
    if (!tx || !tx->px || tx->w <= 0 || tx->h <= 0)
        return c;
    u = s * (float)tx->w;
    v = t * (float)tx->h;
    if (st->mag_filter == 0 && st->min_filter == 0)
        return texel_at(tx, (int)floorf(u), (int)floorf(v), st);
    u -= 0.5f;
    v -= 0.5f;
    x = (int)floorf(u);
    y = (int)floorf(v);
    fu = u - (float)x;
    fv = v - (float)y;
    {
        rgba a = texel_at(tx, x, y, st), b = texel_at(tx, x + 1, y, st);
        rgba d = texel_at(tx, x, y + 1, st), e = texel_at(tx, x + 1, y + 1, st);
        c.r = (a.r * (1 - fu) + b.r * fu) * (1 - fv) + (d.r * (1 - fu) + e.r * fu) * fv;
        c.g = (a.g * (1 - fu) + b.g * fu) * (1 - fv) + (d.g * (1 - fu) + e.g * fu) * fv;
        c.b = (a.b * (1 - fu) + b.b * fu) * (1 - fv) + (d.b * (1 - fu) + e.b * fu) * fv;
        c.a = (a.a * (1 - fu) + b.a * fu) * (1 - fv) + (d.a * (1 - fu) + e.a * fu) * fv;
    }
    return c;
}

static int cmp(int fn, float a, float b)
{
    switch (fn) {
    case 0: return 0;
    case 1: return a < b;
    case 2: return a == b;
    case 3: return a <= b;
    case 4: return a > b;
    case 5: return a != b;
    case 6: return a >= b;
    default: return 1;
    }
}

static float blend_f(int k, int src, rgba s, rgba d, int ch)
{
    float sc = ch == 0 ? s.r : ch == 1 ? s.g : ch == 2 ? s.b : s.a;
    float dc = ch == 0 ? d.r : ch == 1 ? d.g : ch == 2 ? d.b : d.a;
    switch (k) {
    case 0: return 0;
    case 1: return s.a;
    case 2: return src ? dc : sc;               /* DST_COLOR as a source, SRC_COLOR as a dest */
    case 3: return d.a;
    case 4: return 1;
    case 5: return 1 - s.a;
    case 6: return src ? 1 - dc : 1 - sc;
    case 7: return 1 - d.a;
    case 0xF: return src ? (s.a < 1 - d.a ? s.a : 1 - d.a) : 0;
    default: return 1;
    }
}

/* Glide's fog table entry i stands for w = 2^(3 + i/4) / (8 - i%4) */
static float fog_w(int i) { return (float)(pow(2.0, 3.0 + (double)(i >> 2)) / (8 - (i & 3))); }

static float fog_table(const brr_state *st, float w)
{
    int i;
    if (w <= fog_w(0))
        return st->fog_table[0] / 255.0f;
    for (i = 0; i < 63; i++) {
        float w0 = fog_w(i), w1 = fog_w(i + 1);
        if (w < w1) {
            float k = (w - w0) / (w1 - w0);
            return (st->fog_table[i] * (1 - k) + st->fog_table[i + 1] * k) / 255.0f;
        }
    }
    return st->fog_table[63] / 255.0f;
}

static void shade(const brr_state *st, int px, float z, float oow, rgba it, float s, float t)
{
    rgba tex = { 0, 0, 0, 0 }, cc_l, cc_o, out, dst;
    float a_l, a_o, f;
    uint32_t d;
    int o = px;

    if (st->depth_mode && !cmp(st->depth_fn, z, s_dep[o]))
        return;
    if (st->texture) {
        rgba tx = sample(st, s, t);
        /* TMU0: local is the texel, nothing upstream */
        tex.r = combine(st->tc_rgb_fn, factor(st->tc_rgb_factor, tx.r, tx.a, 0, tx.a), tx.r, tx.a, 0, st->tc_rgb_invert);
        tex.g = combine(st->tc_rgb_fn, factor(st->tc_rgb_factor, tx.g, tx.a, 0, tx.a), tx.g, tx.a, 0, st->tc_rgb_invert);
        tex.b = combine(st->tc_rgb_fn, factor(st->tc_rgb_factor, tx.b, tx.a, 0, tx.a), tx.b, tx.a, 0, st->tc_rgb_invert);
        tex.a = combine(st->tc_alpha_fn, factor(st->tc_alpha_factor, tx.a, tx.a, 0, tx.a), tx.a, tx.a, 0, st->tc_alpha_invert);
    }
    {
        rgba k = { ((st->constant >> 16) & 0xFF) / 255.0f, ((st->constant >> 8) & 0xFF) / 255.0f,
                   (st->constant & 0xFF) / 255.0f, (st->constant >> 24) / 255.0f };
        cc_l = st->cc_local == 1 ? k : it;
        cc_o = st->cc_other == 1 ? tex : st->cc_other == 2 ? k : it;
        a_l = st->ac_local == 1 ? k.a : it.a;
        a_o = st->ac_other == 1 ? tex.a : st->ac_other == 2 ? k.a : it.a;
    }
    out.a = combine(st->ac_fn, factor(st->ac_factor, a_l, a_l, a_o, tex.a), a_l, a_l, a_o, st->ac_invert);
    f = 0;
    out.r = combine(st->cc_fn, factor(st->cc_factor, cc_l.r, a_l, cc_o.a, tex.a), cc_l.r, a_l, cc_o.r, st->cc_invert);
    out.g = combine(st->cc_fn, factor(st->cc_factor, cc_l.g, a_l, cc_o.a, tex.a), cc_l.g, a_l, cc_o.g, st->cc_invert);
    out.b = combine(st->cc_fn, factor(st->cc_factor, cc_l.b, a_l, cc_o.a, tex.a), cc_l.b, a_l, cc_o.b, st->cc_invert);
    if (st->atest_fn != 7 && !cmp(st->atest_fn, (float)(int)(out.a * 255.0f + 0.5f), (float)st->atest_ref))
        return;
    switch (st->fog_mode & 0xFF) {
    case 1: f = it.a; break;
    case 2: f = fog_table(st, oow > 0 ? 1.0f / oow : 65536.0f); break;
    case 3: f = clamp1(z); break;
    default: f = 0; break;
    }
    if (f > 0) {
        out.r += (((st->fog_color >> 16) & 0xFF) / 255.0f - out.r) * f;
        out.g += (((st->fog_color >> 8) & 0xFF) / 255.0f - out.g) * f;
        out.b += ((st->fog_color & 0xFF) / 255.0f - out.b) * f;
    }
    d = s_col[o];
    dst.r = ((d >> 16) & 0xFF) / 255.0f;
    dst.g = ((d >> 8) & 0xFF) / 255.0f;
    dst.b = (d & 0xFF) / 255.0f;
    dst.a = 1.0f;                               /* the board keeps no destination alpha */
    if (!(st->blend_rgb_src == 4 && st->blend_rgb_dst == 0)) {
        out.r = clamp1(out.r * blend_f(st->blend_rgb_src, 1, out, dst, 0) + dst.r * blend_f(st->blend_rgb_dst, 0, out, dst, 0));
        out.g = clamp1(out.g * blend_f(st->blend_rgb_src, 1, out, dst, 1) + dst.g * blend_f(st->blend_rgb_dst, 0, out, dst, 1));
        out.b = clamp1(out.b * blend_f(st->blend_rgb_src, 1, out, dst, 2) + dst.b * blend_f(st->blend_rgb_dst, 0, out, dst, 2));
    }
    s_col[o] = 0xFF000000u | (uint32_t)(out.r * 255.0f + 0.5f) << 16 | (uint32_t)(out.g * 255.0f + 0.5f) << 8
             | (uint32_t)(out.b * 255.0f + 0.5f);
    if (st->depth_mode && st->depth_mask)
        s_dep[o] = z;
}

static void tri(const brr_state *st, const brr_vertex *a, const brr_vertex *b, const brr_vertex *c)
{
    float area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y);
    int x0, y0, x1, y1, cx0, cy0, cx1, cy1, x, y;
    float inv;
    if (area == 0)
        return;
    if (area < 0) {
        const brr_vertex *t = b;
        b = c;
        c = t;
        area = -area;
    }
    inv = 1.0f / area;
    clip_rect(st, &cx0, &cy0, &cx1, &cy1);
    x0 = (int)floorf(fminf(a->x, fminf(b->x, c->x)));
    x1 = (int)ceilf(fmaxf(a->x, fmaxf(b->x, c->x)));
    y0 = (int)floorf(fminf(a->y, fminf(b->y, c->y)));
    y1 = (int)ceilf(fmaxf(a->y, fmaxf(b->y, c->y)));
    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    for (y = y0; y < y1; y++) {
        float py = (float)y + 0.5f;
        for (x = x0; x < x1; x++) {
            float px = (float)x + 0.5f;
            float w0 = (c->x - b->x) * (py - b->y) - (c->y - b->y) * (px - b->x);
            float w1 = (a->x - c->x) * (py - c->y) - (a->y - c->y) * (px - c->x);
            float w2 = (b->x - a->x) * (py - a->y) - (b->y - a->y) * (px - a->x);
            float oow, z, s, t;
            rgba it;
            if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;
            /* top-left rule: a pixel exactly on a right or bottom edge is the neighbour's */
            if ((w0 == 0 && !((c->y - b->y) < 0 || ((c->y - b->y) == 0 && (c->x - b->x) > 0))) ||
                (w1 == 0 && !((a->y - c->y) < 0 || ((a->y - c->y) == 0 && (a->x - c->x) > 0))) ||
                (w2 == 0 && !((b->y - a->y) < 0 || ((b->y - a->y) == 0 && (b->x - a->x) > 0))))
                continue;
            w0 *= inv;
            w1 *= inv;
            w2 *= inv;
            z = a->z * w0 + b->z * w1 + c->z * w2;
            it.r = clamp1((a->r * w0 + b->r * w1 + c->r * w2) / 255.0f);
            it.g = clamp1((a->g * w0 + b->g * w1 + c->g * w2) / 255.0f);
            it.b = clamp1((a->b * w0 + b->b * w1 + c->b * w2) / 255.0f);
            it.a = clamp1((a->a * w0 + b->a * w1 + c->a * w2) / 255.0f);
            oow = a->oow * w0 + b->oow * w1 + c->oow * w2;
            if (oow != 0) {
                s = (a->s * a->oow * w0 + b->s * b->oow * w1 + c->s * c->oow * w2) / oow;
                t = (a->t * a->oow * w0 + b->t * b->oow * w1 + c->t * c->oow * w2) / oow;
            } else {
                s = a->s * w0 + b->s * w1 + c->s * w2;
                t = a->t * w0 + b->t * w1 + c->t * w2;
            }
            shade(st, y * s_w + x, z, oow, it, s, t);
        }
    }
}

void brr_draw(const brr_state *st, const brr_vertex *v, int n)
{
    int i;
    {
        /* BRR_DUMP=N: list every triangle of present N */
        static long dump = -2;
        if (dump == -2) {
            const char *e = getenv("BRR_DUMP");
            dump = e ? atol(e) : -1;
        }
        if (dump >= 0 && (long)s_frame == dump && st->texture && st->texture <= s_ntex) {
            /* and each texture it uses, once, as BRR_DUMP_<id>.png */
            static unsigned char seen[8192];
            const stex *t = &s_tex[st->texture - 1];
            if (st->texture < sizeof seen && !seen[st->texture] && t->px) {
                char path[64];
                uint32_t *keep = s_col;
                int kw = s_w, kh = s_h;
                uint32_t *c = (uint32_t *)malloc((size_t)t->w * (size_t)t->h * 4);
                int k;
                seen[st->texture] = 1;
                if (c) {
                    for (k = 0; k < t->w * t->h; k++)   /* alpha as grey, so a glyph sheet reads */
                        c[k] = 0xFF000000u | (uint32_t)t->px[k * 4 + 3] * 0x010101u;
                    s_col = c; s_w = t->w; s_h = t->h;
                    snprintf(path, sizeof path, "build/portable/BRR_DUMP_%u.png", st->texture);
                    write_png(path);
                    s_col = keep; s_w = kw; s_h = kh;
                    free(c);
                }
            }
        }
        if (dump >= 0 && (long)s_frame == dump)
            for (i = 0; i + 2 < n; i += 3)
                fprintf(stderr, "brr: tri dm%d df%d dk%d tex %u cc %d/%d/%d/%d ac %d/%d/%d/%d blend %d/%d const %08X clip %d,%d-%d,%d"
                        " twh %dx%d st %.3f,%.3f %.3f,%.3f %.3f,%.3f | %.1f,%.1f z%.3f w%.4f c%.0f/%.0f/%.0f/%.0f | %.1f,%.1f | %.1f,%.1f\n",
                        st->depth_mode, st->depth_fn, st->depth_mask, st->texture, st->cc_fn, st->cc_factor, st->cc_local, st->cc_other,
                        st->ac_fn, st->ac_factor, st->ac_local, st->ac_other,
                        st->blend_rgb_src, st->blend_rgb_dst, st->constant,
                        st->clip_x0, st->clip_y0, st->clip_x1, st->clip_y1,
                        st->tex_w, st->tex_h, v[i].s, v[i].t, v[i+1].s, v[i+1].t, v[i+2].s, v[i+2].t,
                        v[i].x, v[i].y, v[i].z, v[i].oow, v[i].r, v[i].g, v[i].b, v[i].a,
                        v[i + 1].x, v[i + 1].y, v[i + 2].x, v[i + 2].y);
    }
    for (i = 0; i + 2 < n; i += 3)
        tri(st, &v[i], &v[i + 1], &v[i + 2]);
    s_tris += (unsigned long)n / 3;
}

void brr_lfb_write(int x, int y, int w, int h, const uint16_t *p, int stride)
{
    int i, j;
    for (j = 0; j < h; j++) {
        const uint16_t *r = (const uint16_t *)((const uint8_t *)p + (size_t)j * (size_t)stride);
        if (y + j < 0 || y + j >= s_h)
            continue;
        for (i = 0; i < w; i++) {
            uint32_t v = r[i];
            if (x + i < 0 || x + i >= s_w)
                continue;
            s_col[(y + j) * s_w + x + i] = 0xFF000000u | ((v >> 11) * 255 / 31) << 16 |
                                           (((v >> 5) & 63) * 255 / 63) << 8 | ((v & 31) * 255 / 31);
        }
    }
}

int brr_shot(const char *path)
{
    uint32_t *keep = s_col;
    int ok;
    if (!s_shown)
        return 0;
    s_col = s_shown;
    ok = write_png(path);
    s_col = keep;
    if (ok)
        fprintf(stderr, "brr: frame %lu -> %s\n", s_frame, path);
    return ok;
}

void brr_present(void)
{
    if (!s_shown)
        s_shown = (uint32_t *)malloc((size_t)s_w * (size_t)s_h * 4);
    if (s_shown)
        memcpy(s_shown, s_col, (size_t)s_w * (size_t)s_h * 4);
    s_frame++;
    {
        /* BRR_STATS=N: every Nth present, the triangles submitted since the last line */
        static long every = -1;
        static unsigned long tris0;
        if (every < 0) {
            const char *e = getenv("BRR_STATS");
            every = e ? atol(e) : 0;
        }
        if (every > 0 && s_frame % (unsigned long)every == 0) {
            fprintf(stderr, "brr: present %lu: %lu triangles\n", s_frame, s_tris - tris0);
            tris0 = s_tris;
        }
    }
    if ((long)s_frame == s_shot_frame) {
        if (write_png(s_shot_path))
            fprintf(stderr, "brr: frame %lu -> %s\n", s_frame, s_shot_path);
        else
            fprintf(stderr, "brr: frame %lu: cannot write %s\n", s_frame, s_shot_path);
    }
}
