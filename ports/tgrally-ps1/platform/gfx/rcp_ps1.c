/* rcp_ps1.c: the game's display lists drawn by the PlayStation's GPU.
 *
 * The RSP's work (matrices, vertices, lighting) is done here on the CPU in
 * the RSP's own fixed point (s15.16); the RDP's (rasterising, texturing,
 * combining, blending) is the GPU's, approximated: the combiner is reduced
 * per vertex to "texel times a colour" or "a colour", blending to the GPU's
 * four semi-transparency modes, texel alpha to its transparent texel and
 * STP bit.  There is no depth buffer: a run of depth-tested primitives is
 * sorted by depth in an ordering table and drawn far to near; everything
 * else is drawn in the order the game drew it.
 *
 * Textures are what the RDP's TMEM held, keyed by the loads that filled it
 * (source address, layout, palette) and a sample of the source's bytes,
 * decoded on a miss into the GPU's own formats (4- and 8-bit CLUT, 15-bit)
 * in VRAM beside the frame buffers.  The uploads ride at the head of the
 * frame's packet list, so they land before the primitives that use them and
 * after the previous frame has been drawn. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "ps1.h"
#include "gpu.h"

extern uint32_t tgr_rcp_frames;
static int s_log;                               /* TGR_RCPLOG */

/* ---- game memory ----------------------------------------------------------- */
static uint32_t s_seg[16];
static uint32_t seg_addr(uint32_t a) { return (s_seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x3FFFFF; }
static const uint8_t *mem(uint32_t phys) { return tgr_rdram + (phys & 0x3FFFFF); }
static uint32_t rd32(uint32_t phys) { const uint8_t *p = mem(phys); return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint16_t rd16(uint32_t phys) { const uint8_t *p = mem(phys); return (uint16_t)(p[0] << 8 | p[1]); }

/* ---- the frame's packets ------------------------------------------------------ */
#define PKT_WORDS (96 * 1024 / 4)
#define OT_LEN 1024
static uint32_t s_pkt[2][PKT_WORDS];
static int s_buf, s_used;
static uint32_t *s_head, *s_tail;               /* the frame's list */
static uint32_t *s_uphead, *s_uptail;           /* its uploads, sent first */
static uint32_t *s_ot_head[OT_LEN], *s_ot_tail[OT_LEN];
static int s_zrun, s_ot_lo, s_ot_hi;            /* a depth-sorted run is open; its buckets in use */
static int s_overflow;

static uint32_t *pkt_alloc(int words)
{
    uint32_t *p;
    if (s_used + words + 1 > PKT_WORDS) {
        s_overflow = 1;
        return NULL;
    }
    p = &s_pkt[s_buf][s_used];
    s_used += words + 1;
    p[0] = (uint32_t)words << 24 | 0xFFFFFF;
    return p;
}

static void link_after(uint32_t **tail, uint32_t *p)
{
    (*tail)[0] = ((*tail)[0] & 0xFF000000u) | ((uint32_t)p & 0xFFFFFF);
    *tail = p;
}

/* a depth-sorted run ends: its buckets join the list, far to near */
static void zrun_close(void)
{
    int i;
    if (!s_zrun)
        return;
    for (i = s_ot_hi; i >= s_ot_lo; i--)
        if (s_ot_head[i]) {
            link_after(&s_tail, s_ot_head[i]);
            s_tail = s_ot_tail[i];
            s_ot_head[i] = s_ot_tail[i] = NULL;
        }
    s_zrun = 0;
}

static void put_flat(uint32_t *p)
{
    zrun_close();
    link_after(&s_tail, p);
}

static void put_sorted(uint32_t *p, int z)
{
    if (z < 0) z = 0;
    if (z >= OT_LEN) z = OT_LEN - 1;
    if (!s_zrun) {
        s_zrun = 1;
        s_ot_lo = OT_LEN;
        s_ot_hi = -1;
    }
    if (z < s_ot_lo) s_ot_lo = z;
    if (z > s_ot_hi) s_ot_hi = z;
    if (s_ot_head[z])
        link_after(&s_ot_tail[z], p);
    else
        s_ot_head[z] = s_ot_tail[z] = p;
}

/* ---- frame buffers ---------------------------------------------------------- *
 * A 320-wide colour image is one of two 320 x 240 buffers (VRAM y 0 and 240),
 * drawn while the other is shown.  A 640-wide one (the menus) is the one
 * 640 x 480 interlaced buffer, which every 640-wide image of the game maps to. */
static uint32_t s_fbaddr[2] = { 0xFFFFFFFF, 0xFFFFFFFF };
static int s_target = -1;                       /* the buffer being drawn, -1 none; 2 the hi-res one */
static int s_shown = -1, s_show_pending = -1;
static int s_hires = -1;                        /* the display mode set: 1 640 x 480i, 0 320 x 240 */
static uint32_t s_cimg, s_zimg;

static int fb_index(uint32_t a, int width)
{
    int i;
    if (width == 640)
        return 2;
    for (i = 0; i < 2; i++)
        if (s_fbaddr[i] == a)
            return i;
    for (i = 0; i < 2; i++)
        if (s_fbaddr[i] == 0xFFFFFFFF) {
            tgr_log("rcp: frame buffer %d at %06X\n", i, a);
            s_fbaddr[i] = a;
            return i;
        }
    return -1;
}

static void fb_rect(int t, int *x, int *y, int *w, int *h)
{
    *x = 0;
    *y = t == 1 ? 240 : 0;
    *w = t == 2 ? 640 : 320;
    *h = t == 2 ? 480 : 240;
}

/* ---- the RSP's state ---------------------------------------------------------- */
#define G_ZBUFFER            0x00000001
#define G_SHADE              0x00000004
#define G_SHADING_SMOOTH     0x00000200
#define G_CULL_FRONT         0x00001000
#define G_CULL_BACK          0x00002000
#define G_FOG                0x00010000
#define G_LIGHTING           0x00020000
#define G_TEXTURE_GEN        0x00040000

typedef struct { int32_t m[4][4]; } M4;         /* s15.16 */
static M4 s_mv[10], s_proj, s_mvp;
static int s_mvn, s_mvp_dirty;
static uint32_t s_geom;
static struct { int32_t sx, sy, sz, tx, ty, tz; } s_vp;     /* 13.2 */
static struct { uint8_t col[3]; int8_t dir[3]; } s_light[8];
static int s_nlights = 1;
static struct { int on, tile, level; uint32_t ss, ts; } s_tex;

typedef struct {
    int16_t x, y;                               /* screen, pixels */
    int16_t z;                                  /* ordering depth, 0 near .. OT_LEN far */
    uint8_t r, g, b, a;
    int32_t s, t;                               /* texel coordinates, 10.5 after G_TEXTURE's scale */
    uint8_t clip;                               /* 1 x<-w, 2 x>w, 4 y<-w, 8 y>w, 16 behind */
} Vtx;
static Vtx s_vtx[32];

static void m4_mul(M4 *r, const M4 *a, const M4 *b)
{
    M4 t;
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            int64_t s = (int64_t)a->m[i][0] * b->m[0][j] + (int64_t)a->m[i][1] * b->m[1][j] +
                        (int64_t)a->m[i][2] * b->m[2][j] + (int64_t)a->m[i][3] * b->m[3][j];
            t.m[i][j] = (int32_t)(s >> 16);
        }
    *r = t;
}

static void m4_load(M4 *r, uint32_t a)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r->m[i][j] = (int32_t)((uint32_t)rd16(a + (i * 4 + j) * 2) << 16 | rd16(a + 32 + (i * 4 + j) * 2));
}

static void mvp_update(void)
{
    if (s_mvp_dirty) {
        m4_mul(&s_mvp, &s_mv[s_mvn], &s_proj);
        s_mvp_dirty = 0;
    }
}

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* a light's direction in model space: the modelview's upper 3x3 applied
 * transposed, normalised to 1.14 */
static void model_dir(const int8_t d[3], int32_t out[3])
{
    const M4 *m = &s_mv[s_mvn];
    int64_t v[3], l2;
    int i;
    uint32_t l;
    for (i = 0; i < 3; i++)
        v[i] = ((int64_t)d[0] * m->m[i][0] + (int64_t)d[1] * m->m[i][1] + (int64_t)d[2] * m->m[i][2]) >> 8;
    l2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    l = 1;
    while ((int64_t)l * l < l2)                  /* integer square root, coarse */
        l <<= 1;
    {
        uint32_t lo = l >> 1, hi = l;
        while (lo + 1 < hi) {
            uint32_t mid = (lo + hi) / 2;
            if ((int64_t)mid * mid <= l2) lo = mid; else hi = mid;
        }
        l = lo ? lo : 1;
    }
    for (i = 0; i < 3; i++)
        out[i] = (int32_t)((v[i] << 14) / l);
}

static void load_vertices(uint32_t a, int n, int v0)
{
    int32_t ldir[8][3];
    int i, k;
    const M4 *m;
    mvp_update();
    m = &s_mvp;
    if (s_geom & G_LIGHTING)
        for (k = 0; k < s_nlights; k++)
            model_dir(s_light[k].dir, ldir[k]);
    for (i = 0; i < n && v0 + i < 32; i++) {
        uint32_t p = a + i * 16;
        const uint8_t *c = mem(p + 12);
        int32_t x = (int16_t)rd16(p), y = (int16_t)rd16(p + 2), z = (int16_t)rd16(p + 4);
        int32_t s = (int16_t)rd16(p + 8), t = (int16_t)rd16(p + 10);
        Vtx *v = &s_vtx[v0 + i];
        int32_t cx = (int32_t)(((int64_t)x * m->m[0][0] + (int64_t)y * m->m[1][0] + (int64_t)z * m->m[2][0]) >> 16) + (m->m[3][0] >> 16);
        int32_t cy = (int32_t)(((int64_t)x * m->m[0][1] + (int64_t)y * m->m[1][1] + (int64_t)z * m->m[2][1]) >> 16) + (m->m[3][1] >> 16);
        int32_t cz = (int32_t)(((int64_t)x * m->m[0][2] + (int64_t)y * m->m[1][2] + (int64_t)z * m->m[2][2]) >> 16) + (m->m[3][2] >> 16);
        int32_t cw = (int32_t)(((int64_t)x * m->m[0][3] + (int64_t)y * m->m[1][3] + (int64_t)z * m->m[2][3] + m->m[3][3]) >> 8);
        /* cx..cz are whole units; cw 8 fraction bits (an orthographic w is 1) */
        int32_t sx, sy, sz;
        v->clip = 0;
        if (cw < 16) {                          /* behind (or on) the eye */
            v->clip = 16;
            cw = 16;
        }
        cx <<= 8; cy <<= 8; cz <<= 8;
        if (cx < -cw) v->clip |= 1;
        if (cx > cw) v->clip |= 2;
        if (cy < -cw) v->clip |= 4;
        if (cy > cw) v->clip |= 8;
        {
            /* screen = viewport translation + ndc * viewport scale (13.2) */
            int sh = 0;
            int32_t w = cw, xx = cx, yy = cy, zz = cz;
            while (w > 0x7FFF || xx > 0x3FFFFF || xx < -0x3FFFFF || yy > 0x3FFFFF || yy < -0x3FFFFF ||
                   zz > 0x3FFFFF || zz < -0x3FFFFF) {
                w >>= 1; xx >>= 1; yy >>= 1; zz >>= 1;
                sh++;
            }
            if (w < 1) w = 1;
            sx = s_vp.tx + (int32_t)(((xx << 8) / w) * s_vp.sx >> 8);
            sy = s_vp.ty - (int32_t)(((yy << 8) / w) * s_vp.sy >> 8);
            sz = (zz << 8) / w;                 /* ndc z, 8 fraction bits */
        }
        sx >>= 2; sy >>= 2;
        v->x = (int16_t)(sx < -2048 ? -2048 : sx > 2047 ? 2047 : sx);
        v->y = (int16_t)(sy < -2048 ? -2048 : sy > 2047 ? 2047 : sy);
        sz = (sz + 256) * (OT_LEN / 2) >> 8;
        v->z = (int16_t)(sz < 0 ? 0 : sz >= OT_LEN ? OT_LEN - 1 : sz);
        if (s_geom & G_LIGHTING) {
            int r = s_light[s_nlights].col[0], g = s_light[s_nlights].col[1], b = s_light[s_nlights].col[2];
            int32_t nx = (int8_t)c[0], ny = (int8_t)c[1], nz = (int8_t)c[2];
            for (k = 0; k < s_nlights; k++) {
                int32_t d = (nx * ldir[k][0] + ny * ldir[k][1] + nz * ldir[k][2]) >> 7;    /* 1.14 */
                if (d > 0) {
                    r += (d * s_light[k].col[0]) >> 14;
                    g += (d * s_light[k].col[1]) >> 14;
                    b += (d * s_light[k].col[2]) >> 14;
                }
            }
            v->r = (uint8_t)clamp255(r);
            v->g = (uint8_t)clamp255(g);
            v->b = (uint8_t)clamp255(b);
        } else {
            v->r = c[0];
            v->g = c[1];
            v->b = c[2];
        }
        v->a = c[3];
        v->s = (int32_t)(((int64_t)s * s_tex.ss) >> 16);
        v->t = (int32_t)(((int64_t)t * s_tex.ts) >> 16);
    }
}

/* ---- the RDP's state ------------------------------------------------------------ */
static uint32_t s_omh, s_oml, s_fill;
static uint32_t s_timg;
static int s_timg_siz, s_timg_w;
static uint32_t s_cc0, s_cc1;
static uint8_t s_prim[4], s_env[4];
static int s_scissor[4];
typedef struct {
    int fmt, siz, line, tmem, pal, cmt, maskt, shiftt, cms, masks, shifts;
    int uls, ult, lrs, lrt;
} Tile;
static Tile s_tile[8];

/* what filled TMEM: each load's source and shape, replayed on a cache miss */
typedef struct {
    int kind;                                   /* 1 block, 2 tile, 3 tlut */
    uint32_t src;
    int siz, w, a0, a1, a2, a3, line, tmem, words;
} Load;
#define NLOADS 16
static Load s_loads[NLOADS];
static int s_nloads;

static void note_load(const Load *l)
{
    int i, j;
    /* a load replaces any earlier one it overlaps */
    for (i = j = 0; i < s_nloads; i++) {
        const Load *o = &s_loads[i];
        if (o->tmem + o->words <= l->tmem || l->tmem + l->words <= o->tmem)
            s_loads[j++] = *o;
    }
    s_nloads = j;
    if (s_nloads == NLOADS) {
        memmove(s_loads, s_loads + 1, (NLOADS - 1) * sizeof *s_loads);
        s_nloads--;
    }
    s_loads[s_nloads++] = *l;
}

/* ---- TMEM, rebuilt for a decode ------------------------------------------------------ */
static uint8_t s_tmem[4096];

static void tmem_put64(int addr, const uint8_t *src, int odd)
{
    int k;
    for (k = 0; k < 8; k++)
        s_tmem[((addr * 8) + (k ^ (odd ? 4 : 0))) & 0xFFF] = src[k];
}

static void replay(const Load *l)
{
    int i;
    if (l->kind == 1) {                         /* block: a0 uls, a1 ult, a2 lrs, a3 dxt */
        int bpp = l->siz == 0 ? 0 : 4 << l->siz;
        uint32_t src = l->src + (uint32_t)((l->a1 * l->w + l->a0) * (bpp ? bpp / 8 : 1) / (bpp ? 1 : 2));
        for (i = 0; i < l->words; i++)
            tmem_put64(l->tmem + i, mem(src + i * 8), ((l->a3 * i) >> 11) & 1);
    } else if (l->kind == 2) {                  /* tile: a0..a3 the 10.2 corners */
        int bps = l->siz == 0 ? 0 : (1 << l->siz) / 2;
        int x0 = l->a0 >> 2, y0 = l->a1 >> 2, x1 = l->a2 >> 2, y1 = l->a3 >> 2, y;
        for (y = y0; y <= y1; y++) {
            int rowbytes = l->siz == 0 ? (x1 - x0 + 2) / 2 : (x1 - x0 + 1) * bps;
            uint32_t src = l->src + (uint32_t)(l->siz == 0 ? (y * l->w + x0) / 2 : (y * l->w + x0) * bps);
            int w, words = (rowbytes + 7) / 8;
            for (w = 0; w < words; w++)
                tmem_put64(l->tmem + (y - y0) * l->line + w, mem(src + w * 8), (y - y0) & 1);
        }
    } else {                                    /* tlut: a0 entries */
        for (i = 0; i < l->a0; i++) {
            uint16_t c = rd16(l->src + i * 2);
            int k;
            for (k = 0; k < 4; k++) {
                s_tmem[(l->tmem * 8 + i * 8 + k * 2) & 0xFFF] = (uint8_t)(c >> 8);
                s_tmem[(l->tmem * 8 + i * 8 + k * 2 + 1) & 0xFFF] = (uint8_t)c;
            }
        }
    }
}

static uint8_t tm8(int a) { return s_tmem[a & 0xFFF]; }

/* a texel of tile t as the RDP reads it: the value, or for CI the index */
static uint32_t texel_raw(const Tile *tl, int x, int y)
{
    int line = tl->line * 8, base = tl->tmem * 8, swap = (y & 1) ? 4 : 0, a;
    switch (tl->siz) {
    case 0: a = base + y * line + x / 2; return (x & 1) ? tm8(a ^ swap) & 0xF : tm8(a ^ swap) >> 4;
    case 1: a = base + y * line + x; return tm8(a ^ swap);
    case 2: a = base + y * line + x * 2; return (uint32_t)tm8(a ^ swap) << 8 | tm8((a ^ swap) + 1);
    default:
        swap = (y & 1) ? 8 : 0;
        a = base + y * line + x * 2;
        return (uint32_t)tm8(a ^ swap) << 24 | tm8((a ^ swap) + 1) << 16 | tm8((a ^ swap) + 0x800) << 8 |
               tm8((a ^ swap) + 0x801);
    }
}

/* an RGBA8 colour to the GPU's 15 bits: 0 is the transparent texel, so a
 * black that should show is 0x8000 (black with the STP bit, which only
 * matters on a semi-transparent primitive); alpha under 1/4 is transparent,
 * under 3/4 the STP bit (blended on a semi-transparent primitive) */
static uint16_t ps1_col(int r, int g, int b, int a)
{
    uint16_t c;
    if (a < 64)
        return 0;
    c = (uint16_t)((r >> 3) | (g >> 3) << 5 | (b >> 3) << 10);
    if (a < 192 || c == 0)
        c |= 0x8000;
    return c;
}

static uint16_t n64_5551(uint16_t c)
{
    return ps1_col(((c >> 11) & 31) << 3, ((c >> 6) & 31) << 3, ((c >> 1) & 31) << 3, (c & 1) ? 255 : 0);
}

/* ---- the texture cache in VRAM ---------------------------------------------------- *
 * Textures live in the 384 x 512 halfwords right of the frame buffers (x 640
 * up), in shelves across both halves of VRAM; one no wider than a texture
 * page (64 halfwords) never straddles one, a wider one (the menus' 280-texel
 * background strips) is drawn a page at a time.  Palettes sit in the 32 rows
 * under the frame buffers.  A shelf none of whose textures was used in this
 * frame or the last is emptied when room is needed. */
#define TEX_X 640
#define CLUT_Y 480
typedef struct {
    uint32_t key, check;
    uint16_t clut, vx, vy;                      /* the texture's place in VRAM, halfwords */
    uint16_t w, h;                              /* texels */
    uint8_t mode, shelf, has_alpha;             /* mode: 0 4-bit, 1 8-bit, 2 15-bit */
    uint32_t used;
} Tex;
#define NTEX 384
static Tex s_texs[NTEX];
static int s_ntex;
#define NSHELF 96
static struct { int16_t half, y, h, x; uint32_t used; } s_shelf[NSHELF];
static int s_nshelf, s_half_top[2];
static int s_clut_x, s_clut_y;
static uint32_t s_frame_no;

static void tex_flush_all(void)
{
    s_ntex = 0;
    s_nshelf = 0;
    s_half_top[0] = s_half_top[1] = 0;
}

static int shelf_fit(int i, int w)
{
    int x = s_shelf[i].x;
    if (w <= 64 && (x & 63) + w > 64)           /* no straddling a page */
        x = (x + 63) & ~63;
    return x + w <= 384 ? x : -1;
}

/* room for w x h halfwords: its place in VRAM, and its shelf */
static int vram_alloc(int w, int h, int *px, int *py)
{
    int i, x, best = -1;
    uint32_t oldest = 0xFFFFFFFF;
    for (i = 0; i < s_nshelf; i++)
        if (s_shelf[i].h >= h && s_shelf[i].h <= 2 * h + 8 && (x = shelf_fit(i, w)) >= 0)
            goto found;
    for (i = 0; i < 2; i++)
        if (s_nshelf < NSHELF && s_half_top[i] + h <= 256) {
            int k = s_nshelf++;
            s_shelf[k].half = (int16_t)i;
            s_shelf[k].y = (int16_t)s_half_top[i];
            s_shelf[k].h = (int16_t)h;
            s_shelf[k].x = 0;
            s_half_top[i] += h;
            i = k;
            x = 0;
            goto found;
        }
    for (i = 0; i < s_nshelf; i++)              /* evict: the stalest shelf tall enough */
        if (s_shelf[i].h >= h && s_shelf[i].used + 1 < s_frame_no && s_shelf[i].used < oldest) {
            oldest = s_shelf[i].used;
            best = i;
        }
    if (best < 0)
        return -1;
    {
        int j, k;
        for (j = k = 0; j < s_ntex; j++)
            if (s_texs[j].shelf != best)
                s_texs[k++] = s_texs[j];
        s_ntex = k;
        s_shelf[best].x = 0;
    }
    i = best;
    if ((x = shelf_fit(i, w)) < 0)
        return -1;
found:
    *px = TEX_X + x;
    *py = s_shelf[i].half * 256 + s_shelf[i].y;
    s_shelf[i].x = (int16_t)(x + w);
    s_shelf[i].used = s_frame_no;
    return i;
}

/* the GPU's tpage and u for texel u of a texture (u within the page holding it) */
static int tex_k(const Tex *t) { return t->mode == 0 ? 4 : t->mode == 1 ? 2 : 1; }
static uint16_t tex_tpage(const Tex *t, int hx)  /* hx: the halfword column */
{
    return (uint16_t)((hx / 64) | (t->vy / 256) << 4 | t->mode << 7);
}

static int clut_alloc(int n, int *cx, int *cy)
{
    if (s_clut_x + n > 320) {
        s_clut_x = 0;
        s_clut_y++;
        if (s_clut_y >= 32) {
            s_clut_y = 0;                       /* wrap: the oldest palettes go (TODO: tracked reuse) */
        }
    }
    *cx = s_clut_x;
    *cy = CLUT_Y + s_clut_y;
    s_clut_x += (n + 15) & ~15;
    return 0;
}

static uint32_t hash32(uint32_t h, uint32_t v)
{
    h ^= v;
    h *= 0x01000193u;
    return h ^ (h >> 15);
}

/* a sample of a load's source bytes: whether the game changed them */
static uint32_t src_check(const Load *l)
{
    uint32_t h = 0x811C9DC5u, n = (uint32_t)l->words * 8, step, i;
    if (l->kind == 3)
        n = (uint32_t)l->a0 * 2;
    if (l->kind == 2)
        n = (uint32_t)((l->a3 >> 2) - (l->a1 >> 2) + 1) * (uint32_t)l->w * (l->siz ? (1u << l->siz) / 2 : 1);
    step = n > 256 ? (n / 64) & ~3u : 4;
    for (i = 0; i + 4 <= n; i += step)
        h = hash32(h, rd32(l->src + i));
    return h;
}

/* the uploads ride at the head of the frame's packets: up to 255 words each */
static void upload(int x, int y, int w, int h, const uint16_t *px)
{
    int rows_per = (252 * 2) / w, row = 0;
    if (rows_per < 1)
        rows_per = 1;
    while (row < h) {
        int n = h - row < rows_per ? h - row : rows_per;
        int words = (w * n + 1) / 2;
        uint32_t *p = pkt_alloc(words + 4);
        if (!p)
            return;
        p[1] = 0x01000000;                      /* the texture cache is stale */
        p[2] = 0xA0000000;
        p[3] = (uint32_t)(y + row) << 16 | (uint32_t)x;
        p[4] = (uint32_t)n << 16 | (uint32_t)w;
        memcpy(&p[5], px + row * w, (size_t)w * n * 2);
        if (w * n & 1)
            ((uint16_t *)&p[5])[w * n] = 0;
        if (s_uptail)
            link_after(&s_uptail, p);
        else
            s_uphead = s_uptail = p;
        row += n;
    }
}

static int tile_size(const Tile *tl, int *w, int *h)
{
    int ww = (((tl->lrs - tl->uls) & 0xFFF) >> 2) + 1, hh = (((tl->lrt - tl->ult) & 0xFFF) >> 2) + 1;
    if (tl->masks && (1 << tl->masks) < ww) ww = 1 << tl->masks;
    if (tl->maskt && (1 << tl->maskt) < hh) hh = 1 << tl->maskt;
    if (ww > 1024) ww = 1024;
    if (hh > 256) hh = 256;
    *w = ww;
    *h = hh;
    return ww > 0 && hh > 0;
}

static uint16_t s_dec[384 * 256];

static const Tex *tile_texture(int t)
{
    const Tile *tl = &s_tile[t];
    int w, h, i, x, y, mode, ci, tlut = (s_omh >> 14) & 3;
    uint32_t key = 0x811C9DC5u, check = 0;
    int px, py, pg, hw;
    Tex *tx;
    if (!tile_size(tl, &w, &h))
        return NULL;
    if (w > 384 * 4)
        w = 384 * 4;
    key = hash32(key, (uint32_t)(tl->fmt << 28 | tl->siz << 26 | tl->line << 16 | tl->tmem << 4 | tl->pal));
    key = hash32(key, (uint32_t)(w << 16 | h));
    key = hash32(key, (uint32_t)(tl->uls << 16 | tl->ult));
    ci = tl->fmt == 2 || (tl->fmt == 0 && tl->siz < 2);
    if (ci)
        key = hash32(key, (uint32_t)tlut);
    for (i = 0; i < s_nloads; i++) {
        const Load *l = &s_loads[i];
        int used = (l->kind == 3) ? ci : (l->tmem < tl->tmem + 256 && l->tmem + l->words > tl->tmem);
        if (!used)
            continue;
        key = hash32(key, l->src);
        key = hash32(key, (uint32_t)(l->kind << 28 | l->siz << 24 | l->w << 12 | l->tmem));
        key = hash32(key, (uint32_t)(l->a0 << 16 | l->a1));
        key = hash32(key, (uint32_t)(l->a2 << 16 | l->a3));
        check = hash32(check, src_check(l));
    }
    for (i = 0; i < s_ntex; i++)
        if (s_texs[i].key == key) {
            if (s_texs[i].check == check) {
                s_texs[i].used = s_frame_no;
                s_shelf[s_texs[i].shelf].used = s_frame_no;
                return &s_texs[i];
            }
            s_texs[i] = s_texs[--s_ntex];       /* changed: decode again */
            break;
        }
    /* a miss: rebuild TMEM from the loads and decode */
    for (i = 0; i < s_nloads; i++)
        replay(&s_loads[i]);
    mode = ci ? (tl->siz == 0 ? 0 : 1) : 2;
    hw = mode == 0 ? (w + 3) / 4 : mode == 1 ? (w + 1) / 2 : w;
    if (hw > 384) {
        w = (384 * (mode == 0 ? 4 : mode == 1 ? 2 : 1));
        hw = 384;
    }
    if (s_ntex == NTEX) {
        uint32_t oldest = 0xFFFFFFFF;
        int k = 0;
        for (i = 0; i < s_ntex; i++)
            if (s_texs[i].used < oldest) { oldest = s_texs[i].used; k = i; }
        s_texs[k] = s_texs[--s_ntex];
    }
    if ((pg = vram_alloc(hw, h, &px, &py)) < 0)
        return NULL;
    tx = &s_texs[s_ntex++];
    memset(tx, 0, sizeof *tx);
    tx->key = key;
    tx->check = check;
    tx->shelf = (uint8_t)pg;
    tx->mode = (uint8_t)mode;
    tx->w = (uint16_t)w;
    tx->h = (uint16_t)h;
    tx->used = s_frame_no;
    tx->vx = (uint16_t)px;
    tx->vy = (uint16_t)py;
    if (ci) {
        int n = mode == 0 ? 16 : 256, cx, cy;
        uint16_t pal[256];
        for (i = 0; i < n; i++) {
            int idx = mode == 0 ? (tl->pal << 4) | i : i;
            uint16_t c = (uint16_t)(tm8(0x800 + idx * 8) << 8 | tm8(0x800 + idx * 8 + 1));
            if (tlut == 3)                      /* IA16 entries */
                pal[i] = ps1_col(c >> 8, c >> 8, c >> 8, c & 0xFF);
            else
                pal[i] = n64_5551(c);
            if (!pal[i] || (pal[i] & 0x8000))
                tx->has_alpha = 1;
        }
        clut_alloc(n, &cx, &cy);
        upload(cx, cy, n, 1, pal);
        tx->clut = (uint16_t)(cy << 6 | cx >> 4);
        for (y = 0; y < h; y++)
            for (x = 0; x < hw; x++) {
                uint16_t v = 0;
                if (mode == 0) {
                    int k;
                    for (k = 0; k < 4; k++)
                        if (x * 4 + k < w)
                            v |= (uint16_t)(texel_raw(tl, x * 4 + k, y) << (4 * k));
                } else {
                    v = (uint16_t)texel_raw(tl, x * 2, y);
                    if (x * 2 + 1 < w)
                        v |= (uint16_t)(texel_raw(tl, x * 2 + 1, y) << 8);
                }
                s_dec[y * hw + x] = v;
            }
    } else {
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                uint32_t v = texel_raw(tl, x, y);
                uint16_t c;
                switch (tl->fmt << 4 | tl->siz) {
                case 0x02: c = n64_5551((uint16_t)v); break;
                case 0x03: c = ps1_col(v >> 24, (v >> 16) & 255, (v >> 8) & 255, v & 255); break;
                case 0x30: { int i8 = (int)(v >> 1) * 255 / 7; c = ps1_col(i8, i8, i8, (v & 1) ? 255 : 0); break; }
                case 0x31: c = ps1_col((int)(v >> 4) * 17, (int)(v >> 4) * 17, (int)(v >> 4) * 17, (int)(v & 15) * 17); break;
                case 0x32: c = ps1_col((int)(v >> 8), (int)(v >> 8), (int)(v >> 8), (int)(v & 255)); break;
                case 0x40: c = ps1_col((int)v * 17, (int)v * 17, (int)v * 17, (int)v * 17); break;
                default:   c = ps1_col((int)v, (int)v, (int)v, (int)v); break;
                }
                if (!c || (c & 0x8000))
                    tx->has_alpha = 1;
                s_dec[y * hw + x] = c;
            }
    }
    upload(px, py, hw, h, s_dec);
    return tx;
}

/* ---- the combiner, reduced ------------------------------------------------------- *
 * Each input is "a * texel + b" per channel (8.8 fixed); (A - B) * C + D of
 * those is again of that form when C is not the texel.  The result per
 * vertex: textured with colour a, or untextured with colour b. */
typedef struct { int16_t a[4], b[4]; } Lin;
enum { IN_COMB, IN_TEX, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO, IN_TEXA, IN_PRIMA, IN_SHADEA, IN_ENVA, IN_COMBA };
static const int8_t CA[16] = { IN_COMB, IN_TEX, IN_TEX, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO };
static const int8_t CB[16] = { IN_COMB, IN_TEX, IN_TEX, IN_PRIM, IN_SHADE, IN_ENV, IN_ZERO, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO };
static const int8_t CC[32] = { IN_COMB, IN_TEX, IN_TEX, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_COMBA,
                               IN_TEXA, IN_TEXA, IN_PRIMA, IN_SHADEA, IN_ENVA, IN_ONE, IN_PRIMA, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO };
static const int8_t CD[8] = { IN_COMB, IN_TEX, IN_TEX, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO };
static const int8_t AABD[8] = { IN_COMB, IN_TEX, IN_TEX, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO };
static const int8_t AC[8] = { IN_ONE, IN_TEX, IN_TEX, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO };

static void lin_in(Lin *o, int in, const Lin *comb, const uint8_t shade[4], int alpha)
{
    int k;
    memset(o, 0, sizeof *o);
    for (k = 0; k < 4; k++) {
        int c = alpha ? 3 : k;
        switch (in) {
        case IN_COMB: o->a[k] = comb->a[alpha ? 3 : k]; o->b[k] = comb->b[alpha ? 3 : k]; break;
        case IN_COMBA: o->a[k] = comb->a[3]; o->b[k] = comb->b[3]; break;
        case IN_TEX: if (alpha) o->a[k] = 256; else o->a[k] = 256; break;
        case IN_TEXA: o->a[k] = 256; break;     /* (texel alpha, taken as the texel) */
        case IN_PRIM: o->b[k] = s_prim[c]; break;
        case IN_PRIMA: o->b[k] = s_prim[3]; break;
        case IN_SHADE: o->b[k] = shade[c]; break;
        case IN_SHADEA: o->b[k] = shade[3]; break;
        case IN_ENV: o->b[k] = s_env[c]; break;
        case IN_ENVA: o->b[k] = s_env[3]; break;
        case IN_ONE: o->b[k] = 256; break;
        default: break;
        }
        if (o->b[k] && o->b[k] < 256 && in != IN_ONE && in != IN_COMB && in != IN_COMBA)
            o->b[k] = (int16_t)(o->b[k] + (o->b[k] >> 7));   /* 0..255 to 0..256 */
    }
}

static void lin_eval(Lin *out, int a, int b, int c, int d, const Lin *comb, const uint8_t shade[4], int alpha)
{
    Lin A, B, C, D;
    int k;
    lin_in(&A, a, comb, shade, alpha);
    lin_in(&B, b, comb, shade, alpha);
    lin_in(&C, c, comb, shade, alpha);
    lin_in(&D, d, comb, shade, alpha);
    for (k = 0; k < 4; k++) {
        int32_t ca = C.a[k], cb = C.b[k];
        int32_t da = A.a[k] - B.a[k], db = A.b[k] - B.b[k];
        if (ca && !da)                          /* (const) * texel */
            out->a[k] = (int16_t)((db * ca >> 8) + D.a[k]), out->b[k] = D.b[k];
        else if (ca)                            /* texel * texel: as one texel */
            out->a[k] = (int16_t)(((da * ca >> 8) + (db * ca >> 8)) + D.a[k]), out->b[k] = D.b[k];
        else
            out->a[k] = (int16_t)((da * cb >> 8) + D.a[k]), out->b[k] = (int16_t)((db * cb >> 8) + D.b[k]);
    }
}

static void combine(Lin *out, const uint8_t shade[4])
{
    Lin c1, c2;
    int cyc2 = ((s_omh >> 20) & 3) == 1;
    memset(&c1, 0, sizeof c1);
    lin_eval(&c1, CA[(s_cc0 >> 20) & 15], CB[(s_cc1 >> 28) & 15], CC[(s_cc0 >> 15) & 31], CD[(s_cc1 >> 15) & 7],
             &c1, shade, 0);
    {
        Lin al;
        lin_eval(&al, AABD[(s_cc0 >> 12) & 7], AABD[(s_cc1 >> 12) & 7], AC[(s_cc0 >> 9) & 7], AABD[(s_cc1 >> 9) & 7],
                 &c1, shade, 1);
        c1.a[3] = al.a[3];
        c1.b[3] = al.b[3];
    }
    if (!cyc2) {
        *out = c1;
        return;
    }
    lin_eval(&c2, CA[(s_cc0 >> 5) & 15], CB[(s_cc1 >> 24) & 15], CC[s_cc0 & 31], CD[(s_cc1 >> 6) & 7], &c1, shade, 0);
    {
        Lin al;
        lin_eval(&al, AABD[(s_cc1 >> 21) & 7], AABD[(s_cc1 >> 3) & 7], AC[(s_cc1 >> 18) & 7], AABD[s_cc1 & 7],
                 &c1, shade, 1);
        c2.a[3] = al.a[3];
        c2.b[3] = al.b[3];
    }
    *out = c2;
}

/* ---- the primitive's draw state ------------------------------------------------------ */
typedef struct {
    const Tex *tex;
    int textured, semi, abr, zsort;
    int32_t s0, t0;                             /* the tile's origin, 10.5 */
    int shifts, shiftt;
} Draw;

static void draw_state(Draw *d, int tile, int want_tex)
{
    uint32_t bl = s_oml >> 16;
    int p, a, m, b;
    memset(d, 0, sizeof *d);
    d->textured = want_tex;
    if (want_tex) {
        d->tex = tile_texture(tile);
        if (!d->tex)
            d->textured = 0;
        d->s0 = (s_tile[tile].uls > s_tile[tile].lrs ? s_tile[tile].uls - 4096 : s_tile[tile].uls) << 3;
        d->t0 = (s_tile[tile].ult > s_tile[tile].lrt ? s_tile[tile].ult - 4096 : s_tile[tile].ult) << 3;
        d->shifts = s_tile[tile].shifts;
        d->shiftt = s_tile[tile].shiftt;
    }
    p = (bl >> 14) & 3; a = (bl >> 10) & 3; m = (bl >> 6) & 3; b = (bl >> 2) & 3;
    if (((s_omh >> 20) & 3) == 1) {
        p = (bl >> 12) & 3; a = (bl >> 8) & 3; m = (bl >> 4) & 3; b = bl & 3;
    }
    if (p == 0 && m == 1 && b == 2) {           /* additive */
        d->semi = 1;
        d->abr = 1;
    } else if (p == 0 && m == 1 && a != 3 && b == 0 && (s_oml & 0x4000)) {
        d->semi = 2;                            /* by alpha: decided per primitive */
    }
    if (d->textured && d->tex && d->tex->has_alpha && !d->semi && (s_oml & 0x1000))
        d->semi = 0;                            /* texture edges cut: the transparent texel does it */
    d->zsort = (s_geom & G_ZBUFFER) && (s_oml & 0x30);
}

/* a texel coordinate (10.5, scaled by G_TEXTURE) to the texture's u or v */
static int tc(int32_t st, int32_t origin, int shift)
{
    int32_t v = st;
    if (shift > 10)
        v <<= 16 - shift;
    else
        v >>= shift;
    return (v - origin) >> 5;
}

/* ---- primitives ------------------------------------------------------------------------ */
static int clamp_u(int u, int w) { return u < 0 ? 0 : u > w ? w : u; }

static void triangle(int ia, int ib, int ic)
{
    const Vtx *v[3];
    Draw d;
    Lin col[3];
    int k, words, cmd, semi = 0, abr = 0;
    uint32_t *p;
    int tile = s_tex.tile, want_tex;
    if (ia >= 32 || ib >= 32 || ic >= 32 || s_target < 0)
        return;
    v[0] = &s_vtx[ia]; v[1] = &s_vtx[ib]; v[2] = &s_vtx[ic];
    if (v[0]->clip & v[1]->clip & v[2]->clip & 15)
        return;
    if ((v[0]->clip | v[1]->clip | v[2]->clip) & 16)
        return;                                 /* reaching behind the eye: not yet clipped */
    {
        int32_t det = (v[1]->x - v[0]->x) * (v[2]->y - v[0]->y) - (v[2]->x - v[0]->x) * (v[1]->y - v[0]->y);
        if (((s_geom & G_CULL_BACK) && det >= 0) || ((s_geom & G_CULL_FRONT) && det <= 0))
            return;
    }
    {
        int minx = v[0]->x, maxx = v[0]->x, miny = v[0]->y, maxy = v[0]->y;
        for (k = 1; k < 3; k++) {
            if (v[k]->x < minx) minx = v[k]->x;
            if (v[k]->x > maxx) maxx = v[k]->x;
            if (v[k]->y < miny) miny = v[k]->y;
            if (v[k]->y > maxy) maxy = v[k]->y;
        }
        if (maxx - minx > 1023 || maxy - miny > 511)
            return;                             /* larger than the GPU draws: not yet split */
    }
    for (k = 0; k < 3; k++) {
        uint8_t sh[4];
        const Vtx *q = (s_geom & G_SHADING_SMOOTH) ? v[k] : v[0];
        sh[0] = q->r; sh[1] = q->g; sh[2] = q->b; sh[3] = v[k]->a;
        combine(&col[k], sh);
    }
    want_tex = s_tex.on && (col[0].a[0] | col[0].a[1] | col[0].a[2] | col[1].a[0] | col[2].a[0]) != 0;
    draw_state(&d, tile, want_tex);
    if (d.semi == 1) {
        semi = 1;
        abr = d.abr;
    } else if (d.semi == 2) {
        int al = (col[0].a[3] ? col[0].a[3] * 255 >> 8 : col[0].b[3]);
        if (al < 230) {
            semi = 1;
            abr = al < 96 ? 3 : 0;
        }
    }
    cmd = d.textured ? 0x34 : 0x30;
    if (semi)
        cmd |= 2;
    words = d.textured ? 9 : 6;
    if (!(p = pkt_alloc(words + (semi ? 1 : 0))))
        return;
    {
        uint32_t *w = p + 1;
        if (semi)
            *w++ = 0xE1000000 | 0x200 | 0x400 | (uint32_t)abr << 5 | (d.tex ? tex_tpage(d.tex, d.tex->vx) & 0x1FF : 0);
        for (k = 0; k < 3; k++) {
            int r, g, b;
            if (d.textured) {                    /* the GPU modulates: 128 is 1.0 */
                r = col[k].a[0] >> 1; g = col[k].a[1] >> 1; b = col[k].a[2] >> 1;
            } else {
                r = col[k].b[0]; g = col[k].b[1]; b = col[k].b[2];
            }
            w[0] = (uint32_t)clamp255(r) | (uint32_t)clamp255(g) << 8 | (uint32_t)clamp255(b) << 16;
            if (k == 0)
                w[0] |= (uint32_t)cmd << 24;
            w[1] = (uint32_t)(uint16_t)v[k]->y << 16 | (uint16_t)v[k]->x;
            if (d.textured) {
                /* (a texture wider than a page: drawn from its first page) */
                int kk = tex_k(d.tex), lim = (64 - (d.tex->vx & 63)) * kk - 1;
                int u = clamp_u(tc(v[k]->s, d.s0, d.shifts), d.tex->w - 1 < lim ? d.tex->w - 1 : lim);
                int vv = clamp_u(tc(v[k]->t, d.t0, d.shiftt), d.tex->h - 1);
                w[2] = (uint32_t)(((d.tex->vx & 63) * kk + u) & 255) | (uint32_t)(((d.tex->vy & 255) + vv) & 255) << 8;
                if (k == 0)
                    w[2] |= (uint32_t)d.tex->clut << 16;
                if (k == 1)
                    w[2] |= (uint32_t)(tex_tpage(d.tex, d.tex->vx) | abr << 5) << 16;
                w += 3;
            } else {
                w += 2;
            }
        }
    }
    if (d.zsort)
        put_sorted(p, (v[0]->z + v[1]->z + v[2]->z) / 3);
    else
        put_flat(p);
}

/* a textured rectangle as a 4-point textured polygon */
static void rect_tex(uint32_t w0, uint32_t w1, uint32_t h2, uint32_t hc)
{
    int x1 = (int)((w0 >> 12) & 0xFFF) >> 2, y1 = (int)(w0 & 0xFFF) >> 2;
    int x0 = (int)((w1 >> 12) & 0xFFF) >> 2, y0 = (int)(w1 & 0xFFF) >> 2;
    int tile = (w1 >> 24) & 7, cyc = (s_omh >> 20) & 3;
    int32_t s = (int16_t)(h2 >> 16), t = (int16_t)h2;          /* 10.5 */
    int32_t dsdx = (int16_t)(hc >> 16), dtdy = (int16_t)hc;    /* 5.10 */
    Draw d;
    Lin col;
    uint8_t white[4] = { 255, 255, 255, 255 };
    int u0, v0, u1, v1, semi = 0, abr = 0, r, g, b;
    uint32_t *p;
    if (s_target < 0)
        return;
    if (cyc == 2) {                             /* copy mode: inclusive, 4 texels a step */
        dsdx >>= 2;
        x1++;
        y1++;
        memset(&col, 0, sizeof col);
        col.a[0] = col.a[1] = col.a[2] = col.a[3] = 256;
    } else {
        combine(&col, white);
    }
    if (x1 <= x0 || y1 <= y0)
        return;
    draw_state(&d, tile, 1);
    if (!d.textured)
        return;
    u0 = (s - d.s0) >> 5;
    v0 = (t - d.t0) >> 5;
    u1 = u0 + (int)((dsdx * (x1 - x0)) >> 10);
    v1 = v0 + (int)((dtdy * (y1 - y0)) >> 10);
    u0 = clamp_u(u0, d.tex->w); u1 = clamp_u(u1, d.tex->w);
    v0 = clamp_u(v0, d.tex->h); v1 = clamp_u(v1, d.tex->h);
    if (cyc != 2 && d.semi == 2) {
        int al = col.a[3] ? col.a[3] * 255 >> 8 : col.b[3];
        if (al < 230) {
            semi = 1;
            abr = al < 96 ? 3 : 0;
        }
    } else if (cyc != 2 && d.semi == 1) {
        semi = 1;
        abr = 1;
    }
    r = cyc == 2 ? 128 : col.a[0] >> 1;
    g = cyc == 2 ? 128 : col.a[1] >> 1;
    b = cyc == 2 ? 128 : col.a[2] >> 1;
    {
        /* a piece per texture page the texels cross (the GPU's u is 8 bits
         * within a page); x scaled between the texel bounds */
        int kk = tex_k(d.tex), lo = u0 < u1 ? u0 : u1, hi = u0 < u1 ? u1 : u0, ua = lo;
        int vy0 = (d.tex->vy & 255) + v0, vy1 = (d.tex->vy & 255) + v1;
        if (hi == lo)
            hi = lo + 1;
        while (ua < hi) {
            int hx = d.tex->vx + ua / kk;                       /* the halfword column of texel ua */
            int pg_end = ((hx / 64) + 1) * 64;                  /* the page's end, halfwords */
            int ub = (pg_end - d.tex->vx) * kk;                 /* the first texel past it */
            int pa, pb, xa, xb, uu0, uu1;
            uint16_t tp = tex_tpage(d.tex, hx);
            if (ub > hi)
                ub = hi;
            pa = ((d.tex->vx * kk + ua) - (hx / 64) * 64 * kk) & 255;
            pb = pa + (ub - ua);
            if (u0 <= u1) {
                xa = x0 + (ua - lo) * (x1 - x0) / (hi - lo);
                xb = x0 + (ub - lo) * (x1 - x0) / (hi - lo);
                uu0 = pa; uu1 = pb;
            } else {
                xa = x0 + (hi - ub) * (x1 - x0) / (hi - lo);
                xb = x0 + (hi - ua) * (x1 - x0) / (hi - lo);
                uu0 = pb; uu1 = pa;
            }
            if (uu1 > 255) uu1 = 255;
            if (uu0 > 255) uu0 = 255;
            if (xb > xa && (p = pkt_alloc(9))) {
                p[1] = (uint32_t)(0x2C | (semi ? 2 : 0)) << 24 | (uint32_t)clamp255(r) | (uint32_t)clamp255(g) << 8 |
                       (uint32_t)clamp255(b) << 16;
                p[2] = (uint32_t)y0 << 16 | (uint16_t)xa;
                p[3] = (uint32_t)d.tex->clut << 16 | (uint32_t)(vy0 & 255) << 8 | (uint32_t)uu0;
                p[4] = (uint32_t)y0 << 16 | (uint16_t)xb;
                p[5] = (uint32_t)(tp | abr << 5) << 16 | (uint32_t)(vy0 & 255) << 8 | (uint32_t)uu1;
                p[6] = (uint32_t)y1 << 16 | (uint16_t)xa;
                p[7] = (uint32_t)(vy1 & 255) << 8 | (uint32_t)uu0;
                p[8] = (uint32_t)y1 << 16 | (uint16_t)xb;
                p[9] = (uint32_t)(vy1 & 255) << 8 | (uint32_t)uu1;
                put_flat(p);
            }
            ua = ub;
        }
    }
}

static void rect_fill(uint32_t w0, uint32_t w1)
{
    int x1 = (int)((w0 >> 12) & 0xFFF) >> 2, y1 = (int)(w0 & 0xFFF) >> 2;
    int x0 = (int)((w1 >> 12) & 0xFFF) >> 2, y0 = (int)(w1 & 0xFFF) >> 2;
    int cyc = (s_omh >> 20) & 3, r, g, b, semi = 0;
    uint32_t *p;
    if (s_target < 0)
        return;
    if (cyc == 3) {
        uint16_t c = (uint16_t)(s_fill >> 16);
        if (s_cimg == s_zimg)
            return;                             /* a depth clear: no depth buffer */
        x1++;
        y1++;
        r = ((c >> 11) & 31) << 3;
        g = ((c >> 6) & 31) << 3;
        b = ((c >> 1) & 31) << 3;
    } else {
        Lin col;
        uint8_t white[4] = { 255, 255, 255, 255 };
        Draw d;
        combine(&col, white);
        draw_state(&d, 0, 0);
        r = col.b[0]; g = col.b[1]; b = col.b[2];
        if (d.semi == 1 || (d.semi == 2 && col.b[3] < 230))
            semi = 1;
    }
    if (x1 <= x0 || y1 <= y0)
        return;
    if (!(p = pkt_alloc(semi ? 4 : 3)))
        return;
    if (semi) {
        p[1] = 0xE1000600;                      /* 50% blend */
        p[2] = (uint32_t)0x62 << 24 | (uint32_t)clamp255(r) | (uint32_t)clamp255(g) << 8 | (uint32_t)clamp255(b) << 16;
        p[3] = (uint32_t)y0 << 16 | (uint16_t)x0;
        p[4] = (uint32_t)(y1 - y0) << 16 | (uint16_t)(x1 - x0);
    } else {
        p[1] = (uint32_t)0x60 << 24 | (uint32_t)clamp255(r) | (uint32_t)clamp255(g) << 8 | (uint32_t)clamp255(b) << 16;
        p[2] = (uint32_t)y0 << 16 | (uint16_t)x0;
        p[3] = (uint32_t)(y1 - y0) << 16 | (uint16_t)(x1 - x0);
    }
    put_flat(p);
}

static void scissor(int x0, int y0, int x1, int y1)
{
    uint32_t *p;
    int bx, by, bw, bh;
    if (s_target < 0 || !(p = pkt_alloc(2)))
        return;
    fb_rect(s_target, &bx, &by, &bw, &bh);
    if (x1 > bw) x1 = bw;
    if (y1 > bh) y1 = bh;
    if (x1 <= x0 || y1 <= y0) { x1 = x0 + 1; y1 = y0 + 1; }
    p[1] = 0xE3000000 | (uint32_t)(by + y0) << 10 | (uint32_t)(bx + x0);
    p[2] = 0xE4000000 | (uint32_t)(by + y1 - 1) << 10 | (uint32_t)(bx + x1 - 1);
    put_flat(p);
}

/* ---- the task ------------------------------------------------------------------------- */
static void frame_begin(int target)
{
    uint32_t *p;
    int x, y, w, h, hires = target == 2;
    s_target = target;
    if (target < 0)
        return;
    if (hires != s_hires) {                     /* the display mode follows the image's width */
        gpu_wait();
        gpu_mode(hires);
        s_hires = hires;
        if (hires)
            gpu_show(0, 0);
        s_shown = hires ? 2 : -1;
    }
    fb_rect(target, &x, &y, &w, &h);
    p = pkt_alloc(5);
    gpu_env(p + 1, x, y, w, h);
    s_head = s_tail = p;
}

static void frame_end(void)
{
    if (s_target < 0)
        return;
    zrun_close();
    gpu_wait();                                 /* the previous frame is drawn */
    if (s_uphead) {
        link_after(&s_uptail, s_head);
        gpu_send_list(s_uphead);
    } else {
        gpu_send_list(s_head);
    }
    s_uphead = s_uptail = NULL;
    s_buf ^= 1;
    s_used = 0;
    s_target = -1;
    tgr_rcp_frames++;
}

static void run(uint32_t dl)
{
    uint32_t stack[18];
    int sp = 0, guard = 0;
    uint32_t pc = seg_addr(dl);
    while (guard++ < 400000) {
        uint32_t w0 = rd32(pc), w1 = rd32(pc + 4);
        int op = (int)(w0 >> 24);
        pc += 8;
        switch (op) {
        case 0x01: {                                  /* G_MTX */
            M4 m;
            int p = (w0 >> 16) & 0xFF;
            m4_load(&m, seg_addr(w1));
            if (p & 1) {
                if (p & 2)
                    s_proj = m;
                else
                    m4_mul(&s_proj, &m, &s_proj);
            } else {
                if ((p & 4) && s_mvn < 9) {
                    s_mv[s_mvn + 1] = s_mv[s_mvn];
                    s_mvn++;
                }
                if (p & 2)
                    s_mv[s_mvn] = m;
                else
                    m4_mul(&s_mv[s_mvn], &m, &s_mv[s_mvn]);
            }
            s_mvp_dirty = 1;
            break;
        }
        case 0x03: {                                  /* G_MOVEMEM */
            int idx = (w0 >> 16) & 0xFF;
            uint32_t a = seg_addr(w1);
            if (idx == 0x80) {
                s_vp.sx = (int16_t)rd16(a);
                s_vp.sy = (int16_t)rd16(a + 2);
                s_vp.sz = (int16_t)rd16(a + 4);
                s_vp.tx = (int16_t)rd16(a + 8);
                s_vp.ty = (int16_t)rd16(a + 10);
                s_vp.tz = (int16_t)rd16(a + 12);
            } else if (idx >= 0x86 && idx <= 0x94) {
                int k = (idx - 0x86) / 2, j;
                for (j = 0; j < 3; j++) {
                    s_light[k].col[j] = mem(a)[j];
                    s_light[k].dir[j] = (int8_t)mem(a + 8)[j];
                }
            } else if (idx >= 0x98 && idx <= 0x9E) {  /* a forced matrix, a quarter at a time */
                static const int quarter[4] = { 1, 2, 3, 0 };
                int q = quarter[(idx - 0x98) / 2], j;
                int32_t *f = &s_mvp.m[0][0];
                mvp_update();
                for (j = 0; j < 8; j++) {
                    int e = q * 8 + j;
                    uint32_t h = rd16(a + j * 2);
                    if (e < 16)
                        f[e] = (int32_t)(((uint32_t)f[e] & 0xFFFF) | h << 16);
                    else
                        f[e - 16] = (int32_t)(((uint32_t)f[e - 16] & 0xFFFF0000u) | h);
                }
            }
            break;
        }
        case 0x04:                                    /* G_VTX */
            load_vertices(seg_addr(w1), (w0 >> 10) & 0x3F, ((w0 >> 16) & 0xFF) / 2);
            break;
        case 0x06:                                    /* G_DL */
            if (((w0 >> 16) & 0xFF) == 0 && sp < 18)
                stack[sp++] = pc;
            pc = seg_addr(w1);
            break;
        case 0xB1:                                    /* G_TRI2 */
            triangle(((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2);
            triangle(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
            break;
        case 0xBF:                                    /* G_TRI1 */
            triangle(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
            break;
        case 0xB6: s_geom &= ~w1; break;
        case 0xB7: s_geom |= w1; break;
        case 0xB8:                                    /* G_ENDDL */
            if (sp == 0)
                return;
            pc = stack[--sp];
            break;
        case 0xB9: case 0xBA: {
            int sh = (w0 >> 8) & 0xFF, len = w0 & 0xFF;
            uint32_t mask = (len >= 32 ? 0xFFFFFFFFu : ((1u << len) - 1)) << sh;
            uint32_t *m = op == 0xB9 ? &s_oml : &s_omh;
            *m = (*m & ~mask) | w1;
            break;
        }
        case 0xBB:                                    /* G_TEXTURE */
            s_tex.on = w0 & 0xFF;
            s_tex.tile = (w0 >> 8) & 7;
            s_tex.level = (w0 >> 11) & 7;
            s_tex.ss = (uint16_t)(w1 >> 16);
            s_tex.ts = (uint16_t)w1;
            break;
        case 0xBC: {                                  /* G_MOVEWORD */
            int idx = w0 & 0xFF, ofs = (w0 >> 8) & 0xFFFF;
            if (idx == 0x02)
                s_nlights = (int)((w1 - 0x80000000u) / 32) - 1;
            else if (idx == 0x06)
                s_seg[(ofs / 4) & 15] = w1 & 0x3FFFFF;
            else if (idx == 0x0A) {
                int k = ofs / 0x20;
                if ((ofs & 0x1F) == 0 && k < 8) {
                    s_light[k].col[0] = (uint8_t)(w1 >> 24);
                    s_light[k].col[1] = (uint8_t)(w1 >> 16);
                    s_light[k].col[2] = (uint8_t)(w1 >> 8);
                }
            }
            if (s_nlights < 0) s_nlights = 0;
            if (s_nlights > 7) s_nlights = 7;
            break;
        }
        case 0xBD:                                    /* G_POPMTX */
            if (s_mvn > 0)
                s_mvn--;
            s_mvp_dirty = 1;
            break;
        case 0xE4: case 0xE5: {                       /* G_TEXRECT: the halves follow */
            uint32_t h2 = rd32(pc + 4), hc = rd32(pc + 12);
            pc += 16;
            rect_tex(w0, w1, h2, hc);
            break;
        }
        case 0xED:                                    /* G_SETSCISSOR */
            s_scissor[0] = (int)((w0 >> 12) & 0xFFF) / 4;
            s_scissor[1] = (int)(w0 & 0xFFF) / 4;
            s_scissor[2] = (int)((w1 >> 12) & 0xFFF) / 4;
            s_scissor[3] = (int)(w1 & 0xFFF) / 4;
            scissor(s_scissor[0], s_scissor[1], s_scissor[2], s_scissor[3]);
            break;
        case 0xEF: s_omh = w0 & 0xFFFFFF; s_oml = w1; break;
        case 0xF0: {                                  /* G_LOADTLUT */
            Load l;
            int t = (w1 >> 24) & 7;
            memset(&l, 0, sizeof l);
            l.kind = 3;
            l.src = s_timg;
            l.a0 = (int)(((w1 >> 12) & 0xFFF) >> 2) + 1;
            l.tmem = s_tile[t].tmem;
            l.words = l.a0;
            note_load(&l);
            break;
        }
        case 0xF2: {                                  /* G_SETTILESIZE */
            int t = (w1 >> 24) & 7;
            s_tile[t].uls = (w0 >> 12) & 0xFFF;
            s_tile[t].ult = w0 & 0xFFF;
            s_tile[t].lrs = (w1 >> 12) & 0xFFF;
            s_tile[t].lrt = w1 & 0xFFF;
            break;
        }
        case 0xF3: {                                  /* G_LOADBLOCK */
            Load l;
            int t = (w1 >> 24) & 7, texels, bytes;
            memset(&l, 0, sizeof l);
            l.kind = 1;
            l.src = s_timg;
            l.siz = s_timg_siz;
            l.w = s_timg_w;
            l.a0 = (w0 >> 12) & 0xFFF;
            l.a1 = w0 & 0xFFF;
            l.a2 = (w1 >> 12) & 0xFFF;
            l.a3 = w1 & 0xFFF;
            l.tmem = s_tile[t].tmem;
            texels = l.a2 - l.a0 + 1;
            bytes = l.siz == 0 ? (texels + 1) / 2 : texels * ((4 << l.siz) / 8);
            l.words = (bytes + 7) / 8;
            note_load(&l);
            break;
        }
        case 0xF4: {                                  /* G_LOADTILE */
            Load l;
            int t = (w1 >> 24) & 7;
            memset(&l, 0, sizeof l);
            l.kind = 2;
            l.src = s_timg;
            l.siz = s_timg_siz;
            l.w = s_timg_w;
            l.a0 = (w0 >> 12) & 0xFFF;
            l.a1 = w0 & 0xFFF;
            l.a2 = (w1 >> 12) & 0xFFF;
            l.a3 = w1 & 0xFFF;
            l.line = s_tile[t].line;
            l.tmem = s_tile[t].tmem;
            l.words = ((l.a3 >> 2) - (l.a1 >> 2) + 1) * l.line;
            note_load(&l);
            break;
        }
        case 0xF5: {                                  /* G_SETTILE */
            int t = (w1 >> 24) & 7;
            s_tile[t].fmt = (w0 >> 21) & 7;
            s_tile[t].siz = (w0 >> 19) & 3;
            s_tile[t].line = (w0 >> 9) & 0x1FF;
            s_tile[t].tmem = w0 & 0x1FF;
            s_tile[t].pal = (w1 >> 20) & 15;
            s_tile[t].cmt = (w1 >> 18) & 3;
            s_tile[t].maskt = (w1 >> 14) & 15;
            s_tile[t].shiftt = (w1 >> 10) & 15;
            s_tile[t].cms = (w1 >> 8) & 3;
            s_tile[t].masks = (w1 >> 4) & 15;
            s_tile[t].shifts = w1 & 15;
            break;
        }
        case 0xF6: rect_fill(w0, w1); break;
        case 0xF7: s_fill = w1; break;
        case 0xFA:
            s_prim[0] = (uint8_t)(w1 >> 24); s_prim[1] = (uint8_t)(w1 >> 16);
            s_prim[2] = (uint8_t)(w1 >> 8); s_prim[3] = (uint8_t)w1;
            break;
        case 0xFB:
            s_env[0] = (uint8_t)(w1 >> 24); s_env[1] = (uint8_t)(w1 >> 16);
            s_env[2] = (uint8_t)(w1 >> 8); s_env[3] = (uint8_t)w1;
            break;
        case 0xFC: s_cc0 = w0 & 0xFFFFFF; s_cc1 = w1; break;
        case 0xFD:                                    /* G_SETTIMG */
            s_timg = seg_addr(w1);
            s_timg_siz = (w0 >> 19) & 3;
            s_timg_w = (int)(w0 & 0xFFF) + 1;
            break;
        case 0xFE: s_zimg = seg_addr(w1); break;
        case 0xFF: {                                  /* G_SETCIMG */
            uint32_t a = seg_addr(w1);
            if (s_log)
                tgr_log("rcp: cimg %06X w %d zimg %06X\n", a, (int)(w0 & 0xFFF) + 1, s_zimg);
            s_cimg = a;
            if (a != s_zimg && ((int)(w0 & 0xFFF) + 1 == 320 || (int)(w0 & 0xFFF) + 1 == 640)) {
                int t = fb_index(a, (int)(w0 & 0xFFF) + 1);
                if (t != s_target) {
                    frame_end();
                    frame_begin(t);
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

static int s_inited;
static uint32_t s_shot_at[16];
static int s_nshots;

static void init(void)
{
    const char *e = getenv("TGR_SHOT_AT");
    s_log = getenv("TGR_RCPLOG") != NULL;
    gpu_init();
    s_mvn = 0;
    s_scissor[2] = 320;
    s_scissor[3] = 240;
    while (e && *e && s_nshots < 16) {
        s_shot_at[s_nshots++] = (uint32_t)strtoul(e, (char **)&e, 10);
        if (*e == ',') e++;
    }
    s_inited = 1;
}

void tgr_rcp_task(uint32_t dl)
{
    if (!s_inited)
        init();
    s_frame_no++;
    s_mvn = 0;
    run(dl);
    frame_end();
}

void tgr_dump_state(const char *path, uint32_t dl) { (void)path; (void)dl; }

/* the swap: the buffer is shown at the next retrace, once drawn */
void tgr_rcp_swap(uint32_t fb)
{
    int i;
    if (s_log)
        tgr_log("rcp: swap %08X\n", fb);
    if (s_hires == 1)
        return;                                 /* one buffer: always shown */
    for (i = 0; i < 2; i++)
        if (s_fbaddr[i] == (fb & 0x3FFFFF))
            s_show_pending = i;
}

/* a named frame to a file: shotNNNNN.raw, 320 x 240 15-bit (TGR_SHOT_AT=F1,F2,...) */
static void shot(uint32_t frame)
{
    static uint16_t row[640 * 8];
    char name[32];
    int fd, y, x0, y0, w, h;
    if (s_shown < 0)
        return;
    fb_rect(s_shown, &x0, &y0, &w, &h);
    sprintf(name, "shot%05u_%dx%d.raw", (unsigned)frame, w, h);
    if ((fd = pcdrv_creat(name)) < 0)
        return;
    for (y = 0; y < h; y += 8) {
        gpu_read(x0, y0 + y, w, 8, row);
        pcdrv_write(fd, row, w * 8 * 2);
    }
    pcdrv_close(fd);
}

void tgr_rcp_present(uint32_t frame)
{
    int i;
    if (s_show_pending >= 0 && !gpu_busy()) {
        s_shown = s_show_pending;
        s_show_pending = -1;
        gpu_show(0, s_shown * 240);
    }
    for (i = 0; i < s_nshots; i++)
        if (s_shot_at[i] == frame) {
            gpu_wait();
            shot(frame);
        }
}
