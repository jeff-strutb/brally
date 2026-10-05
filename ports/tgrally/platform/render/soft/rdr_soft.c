/* rdr_soft.c: the renderer interface (render/rdr.h) in software, on any
 * platform: a colour and depth buffer at the N64's framebuffer size, the
 * colour combiner evaluated per pixel, perspective-correct texturing with
 * the tiles' wrap rules, and the blender's GPU-shaped modes.  Headless runs
 * and screenshots use it; it is also the reference the GPU renderers follow. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../rdr.h"

typedef struct { uint8_t *px; int w, h; } Tex;

static Tex *s_tex;
static int s_ntex;
static uint32_t *s_col, *s_done;
static float *s_z;
static int s_w, s_h, s_done_w, s_done_h, s_cap;

int rdr_init(void) { return 1; }

void rdr_frame_begin(int fb_w, int fb_h)
{
    if (fb_w * fb_h > s_cap) {
        s_cap = fb_w * fb_h;
        s_col = (uint32_t *)realloc(s_col, (size_t)s_cap * 4);
        s_z = (float *)realloc(s_z, (size_t)s_cap * 4);
    }
    s_w = fb_w;
    s_h = fb_h;
    /* the colour image keeps last frame's pixels, as the N64's did */
    {
        int i;
        for (i = 0; i < s_w * s_h; i++)
            s_z[i] = 1e30f;
    }
}

int rdr_texture(const uint8_t *rgba, int w, int h)
{
    int i;
    for (i = 1; i < s_ntex; i++)
        if (!s_tex[i].px)
            break;
    if (i >= s_ntex) {
        s_ntex = i + 64;
        s_tex = (Tex *)realloc(s_tex, sizeof *s_tex * (size_t)s_ntex);
        memset(s_tex + i, 0, sizeof *s_tex * 64);
    }
    s_tex[i].px = (uint8_t *)malloc((size_t)w * h * 4);
    memcpy(s_tex[i].px, rgba, (size_t)w * h * 4);
    s_tex[i].w = w;
    s_tex[i].h = h;
    return i;
}

void rdr_texture_free(int tex)
{
    if (tex > 0 && tex < s_ntex) {
        free(s_tex[tex].px);
        s_tex[tex].px = NULL;
    }
}

void rdr_clear_depth(void)
{
    int i;
    for (i = 0; i < s_w * s_h; i++)
        s_z[i] = 1e30f;
}

/* ---- per-pixel colour ----------------------------------------------------- */
typedef struct { float c[4]; } V4;

static int wrap(int i, int n, int clamp, int mirror, int mask)
{
    if (mask > 0) {
        if (mirror && ((i / mask) & 1) ^ (i < 0))
            i = mask - 1 - (((i % mask) + mask) % mask);
        else
            i = ((i % mask) + mask) % mask;
        if (i >= n)
            i = n - 1;
        return i;
    }
    (void)clamp;
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}

static void sample(const RdrTile *t, int filter, float s, float u, float out[4])
{
    const Tex *tx;
    float x, y, fx, fy;
    int x0, y0, k;
    if (t->tex <= 0 || t->tex >= s_ntex || !s_tex[t->tex].px) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    tx = &s_tex[t->tex];
    x = s * t->sscale - t->s0;
    y = u * t->tscale - t->t0;
    if (!filter) {
        int ix = wrap((int)floorf(x), tx->w, t->clamp_s, t->mirror_s, t->mask_s);
        int iy = wrap((int)floorf(y), tx->h, t->clamp_t, t->mirror_t, t->mask_t);
        const uint8_t *p = tx->px + (iy * tx->w + ix) * 4;
        for (k = 0; k < 4; k++)
            out[k] = p[k] / 255.0f;
        return;
    }
    x -= 0.5f;
    y -= 0.5f;
    x0 = (int)floorf(x);
    y0 = (int)floorf(y);
    fx = x - x0;
    fy = y - y0;
    {
        int xa = wrap(x0, tx->w, t->clamp_s, t->mirror_s, t->mask_s);
        int xb = wrap(x0 + 1, tx->w, t->clamp_s, t->mirror_s, t->mask_s);
        int ya = wrap(y0, tx->h, t->clamp_t, t->mirror_t, t->mask_t);
        int yb = wrap(y0 + 1, tx->h, t->clamp_t, t->mirror_t, t->mask_t);
        const uint8_t *p00 = tx->px + (ya * tx->w + xa) * 4, *p10 = tx->px + (ya * tx->w + xb) * 4;
        const uint8_t *p01 = tx->px + (yb * tx->w + xa) * 4, *p11 = tx->px + (yb * tx->w + xb) * 4;
        for (k = 0; k < 4; k++)
            out[k] = ((p00[k] * (1 - fx) + p10[k] * fx) * (1 - fy) + (p01[k] * (1 - fx) + p11[k] * fx) * fy) /
                     255.0f;
    }
}

static float input(int code, int ch, const float comb[4], const float t0[4], const float t1[4],
                   const float shade[4], const RdrState *st)
{
    switch (code) {
    case RDR_CC_COMBINED: return comb[ch];
    case RDR_CC_TEXEL0: return t0[ch];
    case RDR_CC_TEXEL1: return t1[ch];
    case RDR_CC_PRIM: return st->prim[ch];
    case RDR_CC_SHADE: return shade[ch];
    case RDR_CC_ENV: return st->env[ch];
    case RDR_CC_ONE: return 1;
    case RDR_CC_NOISE: return (rand() & 255) / 255.0f;
    case RDR_CC_COMBINED_A: return comb[3];
    case RDR_CC_TEXEL0_A: return t0[3];
    case RDR_CC_TEXEL1_A: return t1[3];
    case RDR_CC_PRIM_A: return st->prim[3];
    case RDR_CC_SHADE_A: return shade[3];
    case RDR_CC_ENV_A: return st->env[3];
    case RDR_CC_LOD_FRAC: return 0;
    case RDR_CC_PRIM_LOD_FRAC: return st->prim_lod_frac;
    case RDR_CC_K5: return st->k5;
    case RDR_CC_K4: return st->k4;
    default: return 0;
    }
}

/* the combiner's output for one pixel; returns 0 if alpha compare drops it */
static int combine(const RdrState *st, const float shade[4], float s, float t, float out[4])
{
    float t0[4] = { 0 }, t1[4] = { 0 }, comb[4] = { 0 }, res[4];
    int c, ch;
    if (st->tile[0].tex)
        sample(&st->tile[0], st->filter, s, t, t0);
    if (st->tile[1].tex)
        sample(&st->tile[1], st->filter, s, t, t1);
    for (c = 0; c < st->cycle; c++) {
        for (ch = 0; ch < 3; ch++) {
            float a = input(st->cc.rgb[c][0], ch, comb, t0, t1, shade, st);
            float b = input(st->cc.rgb[c][1], ch, comb, t0, t1, shade, st);
            float cc = input(st->cc.rgb[c][2], ch, comb, t0, t1, shade, st);
            float d = input(st->cc.rgb[c][3], ch, comb, t0, t1, shade, st);
            res[ch] = (a - b) * cc + d;
        }
        {
            float a = input(st->cc.a[c][0], 3, comb, t0, t1, shade, st);
            float b = input(st->cc.a[c][1], 3, comb, t0, t1, shade, st);
            float cc = input(st->cc.a[c][2] == RDR_CC_LOD_FRAC ? RDR_CC_LOD_FRAC : st->cc.a[c][2], 3, comb, t0,
                             t1, shade, st);
            float d = input(st->cc.a[c][3], 3, comb, t0, t1, shade, st);
            res[3] = (a - b) * cc + d;
        }
        for (ch = 0; ch < 4; ch++)
            comb[ch] = res[ch] < 0 ? 0 : res[ch] > 1 ? 1 : res[ch];
    }
    if (st->alpha_compare == 1 && comb[3] < st->blend[3] + 1e-6f && comb[3] <= st->blend[3])
        if (comb[3] < st->blend[3] || st->blend[3] > 0)
            return 0;
    if (st->alpha_compare == 3 && comb[3] < 0.5f)
        return 0;
    if (st->alpha_compare == 2 && comb[3] < (rand() & 255) / 255.0f)
        return 0;
    memcpy(out, comb, sizeof comb);
    return 1;
}

static void write_px(const RdrState *st, int i, const float c[4], float fog)
{
    uint32_t d = s_col[i];
    float dr = ((d >> 16) & 255) / 255.0f, dg = ((d >> 8) & 255) / 255.0f, db = (d & 255) / 255.0f;
    float r = c[0], g = c[1], b = c[2], a = c[3];
    if (st->fog_blend) {
        r = r * (1 - fog) + st->fog[0] * fog;
        g = g * (1 - fog) + st->fog[1] * fog;
        b = b * (1 - fog) + st->fog[2] * fog;
    }
    switch (st->blend_mode) {
    case RDR_BLEND_ALPHA:
        r = r * a + dr * (1 - a);
        g = g * a + dg * (1 - a);
        b = b * a + db * (1 - a);
        break;
    case RDR_BLEND_ADD:
        r = r * a + dr;
        g = g * a + dg;
        b = b * a + db;
        break;
    case RDR_BLEND_MEM:
        return;
    default:
        break;
    }
    r = r > 1 ? 1 : r;
    g = g > 1 ? 1 : g;
    b = b > 1 ? 1 : b;
    s_col[i] = 0xFF000000u | (uint32_t)(r * 255 + 0.5f) << 16 | (uint32_t)(g * 255 + 0.5f) << 8 |
               (uint32_t)(b * 255 + 0.5f);
}

/* ---- triangles ----------------------------------------------------------- */
typedef struct { float x, y, z, iw, s, t, r, g, b, a; } SV;   /* screen space, attributes over w */

static void raster(const RdrState *st, const SV *a, const SV *b, const SV *c)
{
    float area = (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
    int x0, x1, y0, y1, x, y;
    if (fabsf(area) < 1e-6f)
        return;
    x0 = (int)floorf(fminf(a->x, fminf(b->x, c->x)));
    x1 = (int)ceilf(fmaxf(a->x, fmaxf(b->x, c->x)));
    y0 = (int)floorf(fminf(a->y, fminf(b->y, c->y)));
    y1 = (int)ceilf(fmaxf(a->y, fmaxf(b->y, c->y)));
    if (x0 < st->scissor[0]) x0 = st->scissor[0];
    if (y0 < st->scissor[1]) y0 = st->scissor[1];
    if (x1 > st->scissor[2]) x1 = st->scissor[2];
    if (y1 > st->scissor[3]) y1 = st->scissor[3];
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_w) x1 = s_w;
    if (y1 > s_h) y1 = s_h;
    for (y = y0; y < y1; y++) {
        float py = y + 0.5f;
        for (x = x0; x < x1; x++) {
            float px = x + 0.5f;
            float w0 = ((b->x - px) * (c->y - py) - (b->y - py) * (c->x - px)) / area;
            float w1 = ((c->x - px) * (a->y - py) - (c->y - py) * (a->x - px)) / area;
            float w2 = 1 - w0 - w1, iw, z, sh[4], col[4];
            int i;
            if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;
            iw = w0 * a->iw + w1 * b->iw + w2 * c->iw;
            z = w0 * a->z + w1 * b->z + w2 * c->z;
            i = y * s_w + x;
            if (st->z_test && z > s_z[i] + (st->z_decal ? 1e-3f : 0))
                continue;
            sh[0] = (w0 * a->r + w1 * b->r + w2 * c->r) / iw;
            sh[1] = (w0 * a->g + w1 * b->g + w2 * c->g) / iw;
            sh[2] = (w0 * a->b + w1 * b->b + w2 * c->b) / iw;
            sh[3] = (w0 * a->a + w1 * b->a + w2 * c->a) / iw;
            if (!combine(st, sh, (w0 * a->s + w1 * b->s + w2 * c->s) / iw,
                         (w0 * a->t + w1 * b->t + w2 * c->t) / iw, col))
                continue;
            if (st->z_write)
                s_z[i] = z;
            write_px(st, i, col, sh[3]);
        }
    }
}

static SV to_screen(const RdrVtx *v)
{
    SV s;
    float iw = 1.0f / v->w;
    s.x = v->x * iw;
    s.y = v->y * iw;
    s.z = v->z * iw;
    s.iw = iw;
    s.s = v->s * iw;
    s.t = v->t * iw;
    s.r = v->r * iw;
    s.g = v->g * iw;
    s.b = v->b * iw;
    s.a = v->a * iw;
    return s;
}

static RdrVtx lerpv(const RdrVtx *a, const RdrVtx *b, float t)
{
    RdrVtx r;
    r.x = a->x + (b->x - a->x) * t;
    r.y = a->y + (b->y - a->y) * t;
    r.z = a->z + (b->z - a->z) * t;
    r.w = a->w + (b->w - a->w) * t;
    r.s = a->s + (b->s - a->s) * t;
    r.t = a->t + (b->t - a->t) * t;
    r.r = a->r + (b->r - a->r) * t;
    r.g = a->g + (b->g - a->g) * t;
    r.b = a->b + (b->b - a->b) * t;
    r.a = a->a + (b->a - a->a) * t;
    return r;
}

void rdr_triangles(const RdrState *st, const RdrVtx *v, int n)
{
    int k;
    for (k = 0; k + 2 < n; k += 3) {
        /* clip against w > epsilon: the RSP's near clip (depth beyond it is the z test's) */
        RdrVtx in[3] = { v[k], v[k + 1], v[k + 2] }, out[4];
        int no = 0, i;
        for (i = 0; i < 3; i++) {
            const RdrVtx *p = &in[i], *q = &in[(i + 1) % 3];
            float dp = p->w - 0.01f, dq = q->w - 0.01f;
            if (dp >= 0)
                out[no++] = *p;
            if ((dp >= 0) != (dq >= 0))
                out[no++] = lerpv(p, q, dp / (dp - dq));
        }
        for (i = 1; i + 1 < no; i++) {
            SV a = to_screen(&out[0]), b = to_screen(&out[i]), c = to_screen(&out[i + 1]);
            if (out[0].w <= 0 || out[i].w <= 0 || out[i + 1].w <= 0)
                continue;
            raster(st, &a, &b, &c);
        }
    }
}

void rdr_rect(const RdrState *st, float x0, float y0, float x1, float y1, float s, float t, float dsdx,
              float dtdy, int fill, const float rgba[4])
{
    int ix0 = (int)x0, iy0 = (int)y0, ix1 = (int)x1, iy1 = (int)y1, x, y;
    if (ix0 < st->scissor[0]) ix0 = st->scissor[0];
    if (iy0 < st->scissor[1]) iy0 = st->scissor[1];
    if (ix1 > st->scissor[2]) ix1 = st->scissor[2];
    if (iy1 > st->scissor[3]) iy1 = st->scissor[3];
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > s_w) ix1 = s_w;
    if (iy1 > s_h) iy1 = s_h;
    for (y = iy0; y < iy1; y++)
        for (x = ix0; x < ix1; x++) {
            float col[4];
            int i = y * s_w + x;
            if (fill == 1) {
                write_px(st, i, rgba, 0);
                continue;
            }
            {
                float sh[4] = { 0, 0, 0, 0 };
                float ss = s + (x - x0) * dsdx, tt = t + (y - y0) * dtdy;
                if (!combine(st, sh, ss, tt, col))
                    continue;
            }
            write_px(st, i, col, 0);
        }
}

void rdr_frame_end(void)
{
    if (s_done_w * s_done_h < s_w * s_h)
        s_done = (uint32_t *)realloc(s_done, (size_t)s_w * s_h * 4);
    memcpy(s_done, s_col, (size_t)s_w * s_h * 4);
    s_done_w = s_w;
    s_done_h = s_h;
}

const uint32_t *rdr_frame_pixels(int *w, int *h)
{
    *w = s_done_w;
    *h = s_done_h;
    return s_done;
}
