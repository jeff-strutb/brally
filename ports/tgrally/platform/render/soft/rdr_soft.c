/* rdr_soft.c: the renderer interface (render/rdr.h) in software, on any
 * platform: a colour and depth buffer at the N64's framebuffer size, the
 * colour combiner evaluated per pixel, perspective-correct texturing with
 * the tiles' wrap rules, and the blender's GPU-shaped modes.  Headless runs
 * and screenshots use it; it is also the reference the GPU renderers follow.
 *
 * As the RDP and the VI: a primitive covers a pixel by 8 samples (in the
 * anti-aliased modes) and the count is kept with the pixel; colours are kept
 * at 5 bits a channel, dithered as the RDP dithers them; and a finished frame
 * goes through the VI's filters (anti-aliasing of partly covered pixels from
 * their fully covered neighbours, the dither filter, the divot filter, gamma). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../rdr.h"

typedef struct { uint8_t *px; int w, h; } Tex;

static Tex *s_tex;
static int s_ntex;
static uint32_t *s_col, *s_done;
static float *s_z;
static uint8_t *s_cvg;                  /* each pixel's coverage, 0..8 samples */
static int s_w, s_h, s_done_w, s_done_h, s_cap;
static uint32_t s_vi = 0x311E;          /* the VI's control register (rdr_vi) */
static int s_x, s_y;                    /* the pixel being written (its dither cell) */

int rdr_covers(void) { return 1; }
void rdr_vi(uint32_t ctrl) { s_vi = ctrl; }

int rdr_init(void) { return 1; }
void rdr_window(void) {}
int rdr_presents(void) { return 0; }

void rdr_frame_begin(float fb_wf, float fb_hf)
{
    int fb_w = (int)(fb_wf + 0.5f), fb_h = (int)(fb_hf + 0.5f);
    if (fb_w * fb_h > s_cap) {
        s_cap = fb_w * fb_h;
        s_col = (uint32_t *)realloc(s_col, (size_t)s_cap * 4);
        s_z = (float *)realloc(s_z, (size_t)s_cap * 4);
        s_cvg = (uint8_t *)realloc(s_cvg, (size_t)s_cap);
        memset(s_cvg, 8, (size_t)s_cap);
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

static int wrap(int i, int n, int clamp, int mirror, int mask, int cw)
{
    if (clamp && cw > 0)                    /* the RDP clamps to the tile first */
        i = i < 0 ? 0 : i >= cw ? cw - 1 : i;
    if (mask > 0) {
        int m = ((i % mask) + mask) % mask;
        if (mirror && (((i < 0 ? -i - 1 : i) / mask) & 1))
            m = mask - 1 - m;
        return m >= n ? n - 1 : m;
    }
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
        int ix = wrap((int)floorf(x), tx->w, t->clamp_s, t->mirror_s, t->mask_s, t->clamp_w);
        int iy = wrap((int)floorf(y), tx->h, t->clamp_t, t->mirror_t, t->mask_t, t->clamp_h);
        const uint8_t *p = tx->px + (iy * tx->w + ix) * 4;
        for (k = 0; k < 4; k++)
            out[k] = p[k] / 255.0f;
        return;
    }
    /* the RDP's bilinear filter: three texels, not four (the triangle of the
       2x2 square the sample falls in), texel i at coordinate i */
    x0 = (int)floorf(x);
    y0 = (int)floorf(y);
    fx = x - x0;
    fy = y - y0;
    {
        int xa = wrap(x0, tx->w, t->clamp_s, t->mirror_s, t->mask_s, t->clamp_w);
        int xb = wrap(x0 + 1, tx->w, t->clamp_s, t->mirror_s, t->mask_s, t->clamp_w);
        int ya = wrap(y0, tx->h, t->clamp_t, t->mirror_t, t->mask_t, t->clamp_h);
        int yb = wrap(y0 + 1, tx->h, t->clamp_t, t->mirror_t, t->mask_t, t->clamp_h);
        const uint8_t *p00 = tx->px + (ya * tx->w + xa) * 4, *p10 = tx->px + (ya * tx->w + xb) * 4;
        const uint8_t *p01 = tx->px + (yb * tx->w + xa) * 4, *p11 = tx->px + (yb * tx->w + xb) * 4;
        for (k = 0; k < 4; k++)
            out[k] = (fx + fy < 1 ? p00[k] + fx * (p10[k] - p00[k]) + fy * (p01[k] - p00[k])
                                  : p11[k] + (1 - fx) * (p01[k] - p11[k]) + (1 - fy) * (p10[k] - p11[k])) / 255.0f;
    }
}

static float s_lod_frac;

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
    case RDR_CC_LOD_FRAC: return s_lod_frac;
    case RDR_CC_PRIM_LOD_FRAC: return st->prim_lod_frac;
    case RDR_CC_K5: return st->k5;
    case RDR_CC_K4: return st->k4;
    default: return 0;
    }
}

/* the combiner's output for one pixel; returns 0 if alpha compare drops it */
static int combine(const RdrState *st, const float shade[4], float s, float t, float lod, float out[4])
{
    float t0[4] = { 0 }, t1[4] = { 0 }, comb[4] = { 0 }, res[4];
    int c, ch;
    RdrState lst;
    if (st->lod_levels > 0) {                 /* the level and fraction from the texel rate */
        int level = 0, a, b;
        float frac = 0;
        if (lod >= 1.0f) {
            while (level < 7 && lod >= (float)(2 << level))
                level++;
            frac = lod / (float)(1 << level) - 1.0f;
            if (frac > 1)
                frac = 1;
        }
        a = level < st->lod_levels - 1 ? level : st->lod_levels - 1;
        b = level + 1 < st->lod_levels - 1 ? level + 1 : st->lod_levels - 1;
        if (level >= st->lod_levels - 1)
            frac = 1;                         /* past the last level: clamped */
        sample(&st->lod[a], st->filter, s, t, t0);
        sample(&st->lod[b], st->filter, s, t, t1);
        lst = *st;
        lst.lod_levels = 0;
        lst.tile[0].tex = lst.tile[1].tex = 0;
        s_lod_frac = frac;
        st = &lst;
    } else {
        s_lod_frac = 0;
        if (st->tile[0].tex)
            sample(&st->tile[0], st->filter, s, t, t0);
        if (st->tile[1].tex)
            sample(&st->tile[1], st->filter, s, t, t1);
    }
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
    if (st->alpha_compare == 4 && comb[3] < 1.0f / 255.0f)
        return 0;
    if (st->alpha_compare == 2 && comb[3] < (rand() & 255) / 255.0f)
        return 0;
    memcpy(out, comb, sizeof comb);
    return 1;
}

extern int tgr_rcp_tri;
static int s_probe = -2;           /* TGR_PIXEL=X,Y: the pixel index reported, -1 none */

/* a channel to the framebuffer's 5 bits, dithered as the RDP does (a 4x4
 * magic square or Bayer matrix, noise, or none), kept as the 8 bits the
 * blender reads back */
static const uint8_t k_magic[16] = { 0, 6, 1, 7, 4, 2, 5, 3, 3, 5, 2, 4, 7, 1, 6, 0 };
static const uint8_t k_bayer[16] = { 0, 4, 1, 5, 6, 2, 7, 3, 1, 5, 0, 4, 7, 3, 6, 2 };

static uint32_t to5(float v, int d)
{
    int c = (int)(v * 255 + 0.5f);
    c = c < 0 ? 0 : c > 255 ? 255 : c;
    if ((c & 7) > d && c < 248)
        c += 8;
    c >>= 3;
    return (uint32_t)(c << 3 | c >> 2);
}

static void store(const RdrState *st, int i, float r, float g, float b)
{
    int cell = (s_y & 3) * 4 + (s_x & 3);
    int d = st->rgb_dither == 0 ? k_magic[cell] : st->rgb_dither == 1 ? k_bayer[cell]
          : st->rgb_dither == 2 ? rand() & 7 : 7;
    s_col[i] = 0xFF000000u | to5(r, d) << 16 | to5(g, d) << 8 | to5(b, d);
}

/* one pixel through the blender: cn is how many of its 8 samples the
 * primitive covers, z its depth (same: the pixel already holds the same
 * surface, within its depth slope) */
static void write_px(const RdrState *st, int i, const float c[4], float fog, int cn, int same)
{
    if (s_probe == -2) {
        const char *e = getenv("TGR_PIXEL");
        int x, y;
        s_probe = e && sscanf(e, "%d,%d", &x, &y) == 2 ? y * 100000 + x : -1;
    }
    if (s_probe >= 0 && i == (s_probe / 100000) * s_w + s_probe % 100000)
        fprintf(stderr, "pixel tri #%d tex %d/%d blend %d cvg %d in %.2f %.2f %.2f %.2f\n", tgr_rcp_tri,
                st->tile[0].tex, st->tile[1].tex, st->blend_mode, cn, c[0], c[1], c[2], c[3]);
    uint32_t d = s_col[i];
    int cm = s_cvg[i];
    float dr = ((d >> 16) & 255) / 255.0f, dg = ((d >> 8) & 255) / 255.0f, db = (d & 255) / 255.0f;
    float r = c[0], g = c[1], b = c[2], a = st->blend_alpha == 1 ? st->fog[3] : st->blend_alpha == 2 ? fog : c[3];
    if (st->fog_blend) {
        r = r * (1 - fog) + st->fog[0] * fog;
        g = g * (1 - fog) + st->fog[1] * fog;
        b = b * (1 - fog) + st->fog[2] * fog;
    }
    if (st->blend_mode == RDR_BLEND_MEM)
        return;
    if (st->force_bl || !(st->blend_mode == RDR_BLEND_OPAQUE || (st->aa && st->cvg_x_alpha))) {
        /* translucent: the blender always mixes; the coverage kept per CVG_DST */
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
        default:
            break;
        }
        s_cvg[i] = (uint8_t)(st->cvg_dst == 3 ? cm : st->cvg_dst == 2 ? 8 : cm + cn > 8 ? 8 : cm + cn);
    } else if (st->aa && same && cn < 8 && cm + cn <= 8) {
        /* an edge of the surface the pixel holds: mixed by the two coverages,
           which add up (an inner edge of a mesh ends fully covered) */
        float wn = (float)cn / (float)(cn + cm), wm = 1 - wn;
        r = r * wn + dr * wm;
        g = g * wn + dg * wm;
        b = b * wn + db * wm;
        s_cvg[i] = (uint8_t)(st->cvg_dst == 3 ? cm : st->cvg_dst == 2 ? 8 : cm + cn);
    } else {
        /* in front (or fully covering): the pixel is replaced, with its own coverage */
        s_cvg[i] = (uint8_t)(st->cvg_dst == 3 ? cm : st->cvg_dst == 2 || !st->aa ? 8 : cn);
    }
    store(st, i, r > 1 ? 1 : r, g > 1 ? 1 : g, b > 1 ? 1 : b);
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
            int i, cn = 8, same = 0;
            if (st->aa) {                     /* 8 samples: 4 sub-scanlines, 2 across */
                int k;
                cn = 0;
                for (k = 0; k < 8; k++) {
                    float qx = x + ((k & 1) ? 0.75f : 0.25f), qy = y + 0.125f + 0.25f * (k >> 1);
                    float e0 = ((b->x - qx) * (c->y - qy) - (b->y - qy) * (c->x - qx)) / area;
                    float e1 = ((c->x - qx) * (a->y - qy) - (c->y - qy) * (a->x - qx)) / area;
                    cn += e0 >= 0 && e1 >= 0 && 1 - e0 - e1 >= 0;
                }
                if (!cn)
                    continue;
            } else if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;
            iw = w0 * a->iw + w1 * b->iw + w2 * c->iw;
            z = w0 * a->z + w1 * b->z + w2 * c->z;
            i = y * s_w + x;
            if (st->z_test && z > s_z[i] + (st->z_decal ? 1e-3f : 0))
                continue;
            same = st->z_test && s_z[i] < 1e29f && z > s_z[i] - 2e-4f;
            sh[0] = (w0 * a->r + w1 * b->r + w2 * c->r) / iw;
            sh[1] = (w0 * a->g + w1 * b->g + w2 * c->g) / iw;
            sh[2] = (w0 * a->b + w1 * b->b + w2 * c->b) / iw;
            sh[3] = (w0 * a->a + w1 * b->a + w2 * c->a) / iw;
            {
                float ss = (w0 * a->s + w1 * b->s + w2 * c->s) / iw, tt = (w0 * a->t + w1 * b->t + w2 * c->t) / iw;
                float lod = 0;
                if (st->lod_levels > 0) {     /* the coordinates one pixel right and one down */
                    float dw0x = (b->y - c->y) / area, dw1x = (c->y - a->y) / area;
                    float dw0y = (c->x - b->x) / area, dw1y = (a->x - c->x) / area;
                    float u0 = w0 + dw0x, u1 = w1 + dw1x, u2 = 1 - u0 - u1;
                    float v0 = w0 + dw0y, v1 = w1 + dw1y, v2 = 1 - v0 - v1;
                    float iwx = u0 * a->iw + u1 * b->iw + u2 * c->iw, iwy = v0 * a->iw + v1 * b->iw + v2 * c->iw;
                    float sx = (u0 * a->s + u1 * b->s + u2 * c->s) / iwx, tx = (u0 * a->t + u1 * b->t + u2 * c->t) / iwx;
                    float sy = (v0 * a->s + v1 * b->s + v2 * c->s) / iwy, ty = (v0 * a->t + v1 * b->t + v2 * c->t) / iwy;
                    lod = fmaxf(fmaxf(fabsf(sx - ss), fabsf(tx - tt)), fmaxf(fabsf(sy - ss), fabsf(ty - tt)));
                }
                if (!combine(st, sh, ss, tt, lod, col))
                    continue;
            }
            if (st->aa && st->cvg_x_alpha) {  /* coverage times alpha: texture edges */
                cn = (cn * ((int)(col[3] * 255 + 0.5f) + 1)) >> 8;   /* an opaque texel keeps all 8 */
                if (!cn)
                    continue;
            }
            if (st->z_write && !(same && cn < 8))
                s_z[i] = z;
            s_x = x;
            s_y = y;
            write_px(st, i, col, sh[3], cn, same);
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
            s_x = x;
            s_y = y;
            if (fill == 1) {                  /* fill mode: the colour as it is, the alpha bit the coverage */
                s_col[i] = 0xFF000000u | to5(rgba[0], 7) << 16 | to5(rgba[1], 7) << 8 | to5(rgba[2], 7);
                s_cvg[i] = rgba[3] > 0.5f ? 8 : 0;
                continue;
            }
            {
                float sh[4] = { 0, 0, 0, 0 };
                float ss = s + (x - x0) * dsdx, tt = t + (y - y0) * dtdy;
                if (!combine(st, sh, ss, tt, fmaxf(fabsf(dsdx), fabsf(dtdy)), col))
                    continue;
            }
            write_px(st, i, col, 0, 8, 0);
        }
}

/* ---- the VI ---------------------------------------------------------------- */
static int ch5(uint32_t p, int c) { return (int)((p >> (16 - 8 * c)) & 0xF8); }   /* 5 bits, as 8 */
static int cv3(int i) { int c = s_cvg[i]; return c >= 8 ? 7 : c > 0 ? c - 1 : 0; }

/* the second largest and second smallest of n values */
static void penult(const int *v, int n, int *lo, int *hi)
{
    int a[7], i, j;
    memcpy(a, v, sizeof(int) * (size_t)n);
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && a[j - 1] > a[j]; j--) {
            int t = a[j];
            a[j] = a[j - 1];
            a[j - 1] = t;
        }
    *lo = n > 1 ? a[1] : a[0];
    *hi = n > 1 ? a[n - 2] : a[0];
}

static uint32_t at(int x, int y)
{
    x = x < 0 ? 0 : x >= s_w ? s_w - 1 : x;
    y = y < 0 ? 0 : y >= s_h ? s_h - 1 : y;
    return s_col[y * s_w + x];
}

static int full(int x, int y)
{
    if (x < 0 || y < 0 || x >= s_w || y >= s_h)
        return 0;
    return cv3(y * s_w + x) == 7;
}

void rdr_frame_end(void)
{
    int aa = ((s_vi >> 8) & 3) != 3, dither = (s_vi & 0x10000) != 0, divot = (s_vi & 0x10) != 0;
    int gamma = (s_vi & 0x08) != 0, x, y, c;
    static int *row;
    static int cap;
    if (s_done_w * s_done_h < s_w * s_h)
        s_done = (uint32_t *)realloc(s_done, (size_t)s_w * s_h * 4);
    if (cap < s_w * s_h * 3) {
        cap = s_w * s_h * 3;
        row = (int *)realloc(row, sizeof(int) * (size_t)cap);
    }
    for (y = 0; y < s_h; y++)
        for (x = 0; x < s_w; x++) {
            int i = y * s_w + x, cv = cv3(i);
            uint32_t p = s_col[i];
            for (c = 0; c < 3; c++) {
                int v = ch5(p, c);
                if (aa && cv < 7) {
                    /* a partly covered pixel: the background it is missing, from the
                       fully covered pixels around it (both diagonals above and below,
                       two to each side), between their second extremes */
                    static const int nb[6][2] = { { -1, -1 }, { 1, -1 }, { -2, 0 }, { 2, 0 }, { -1, 1 }, { 1, 1 } };
                    int vals[7], n = 1, k, lo, hi;
                    vals[0] = v;
                    for (k = 0; k < 6; k++)
                        if (full(x + nb[k][0], y + nb[k][1]))
                            vals[n++] = ch5(at(x + nb[k][0], y + nb[k][1]), c);
                    penult(vals, n, &lo, &hi);
                    v += ((lo + hi - 2 * v) * (7 - cv) + 4) >> 3;
                } else if (dither && cv == 7) {
                    /* the dither filter: each of the 8 neighbours whose 5-bit value is
                       above or below the pixel's moves it by one */
                    static const int nb[8][2] = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 1 }, { 0, 1 }, { 1, 1 }, { -1, 0 }, { 1, 0 } };
                    int k;
                    for (k = 0; k < 8; k++) {
                        int n5 = ch5(at(x + nb[k][0], y + nb[k][1]), c);
                        v += n5 > ch5(p, c) ? 1 : n5 < ch5(p, c) ? -1 : 0;
                    }
                }
                row[i * 3 + c] = v < 0 ? 0 : v > 255 ? 255 : v;
            }
        }
    for (y = 0; y < s_h; y++)
        for (x = 0; x < s_w; x++) {
            int i = y * s_w + x, rgb[3];
            for (c = 0; c < 3; c++) {
                int v = row[i * 3 + c];
                if (divot && x > 0 && x < s_w - 1 && (cv3(i) < 7 || cv3(i - 1) < 7 || cv3(i + 1) < 7)) {
                    int l = row[(i - 1) * 3 + c], r = row[(i + 1) * 3 + c];   /* the median of three */
                    v = l > r ? (v > l ? l : v < r ? r : v) : (v > r ? r : v < l ? l : v);
                }
                if (gamma)
                    v = (int)(sqrtf((float)v / 255.0f) * 255.0f + 0.5f);
                rgb[c] = v;
            }
            s_done[i] = 0xFF000000u | (uint32_t)rgb[0] << 16 | (uint32_t)rgb[1] << 8 | (uint32_t)rgb[2];
        }
    s_done_w = s_w;
    s_done_h = s_h;
}

const uint32_t *rdr_frame_pixels(int *w, int *h)
{
    *w = s_done_w;
    *h = s_done_h;
    return s_done;
}
