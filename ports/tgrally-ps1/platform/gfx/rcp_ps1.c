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
 * in VRAM beside the frame buffers, uploaded at once. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "ps1.h"
#include "gpu.h"
#include "gte.h"

#undef abs
#define abs(x) __builtin_abs(x)                 /* (freestanding: else a call) */

/* TGR_PERF region timers: root counter 2 (the system clock / 8), inclusive */
enum { RG_TASK, RG_DLC, RG_MESHV, RG_KTRI, RG_CLIP, RG_TRI, RG_RECT, RG_STATE, RG_TEX, RG_VTX, RG_END, RG_TOPTRI,
       RG_MTX, RG_MOVEMEM, RG_FILL, RG_RUNTRI, RG_CUTCLIP, RG_CUTEMIT, RG_GRID, RG_SETUP, RG_REC, RG_NMB, RG_DECODE, RG_N };
static const char *const s_rg_name[RG_N] = { "task", "kept lists", "kept vertices", "kept drawn", "kept cut",
                                             "slow triangles", "rectangles", "state", "texture lookup", "vertices",
                                             "frame end", "run", "matrices", "movemem", "fills", "run triangles", "cut: clipping", "cut: emit", "cut: grids", "matrix and light setup", "recording", "mesh build", "decode" };
static uint64_t s_rg[RG_N];                     /* (nanoseconds) */
static uint32_t s_cls_n[4];
static uint64_t s_cls_t[4];                    /* (nanoseconds) */
static uint64_t s_lh_t[2][5];                   /* TGR_PERF: list calls by length (< 0.25, 1, 4, 16 ms, more), compiled or not */
static uint32_t s_lh_n[2][5];
static int s_listlog = -1;                     /* TGR_LISTLOG=F */
static uint32_t s_ll_sig, s_ll_reads, s_ll_ntri, s_recq[4], s_tt[4], s_tq2[4], s_su[6], s_dryq[4], s_rq[4], s_rqo, s_stop[256], s_stopn[256];
static uint32_t s_st[8], s_nmwhy[8], s_nmempty[2], s_cutc, s_gridn[3], s_nmoff[3];                        /* TGR_PERF: list call stages (cycles / 8): find, check, key, variant, mesh vertices, triangles, state */
#define ST_NOW() (*(volatile uint32_t *)0xBF801120)
#define ST_ADD(k, t) (s_st[k] += (ST_NOW() - (t)) & 0xFFFF)         /* TGR_PERF: list time by where the list is (below 2 MB, 2 MB.., 0x2F.., 0x3E..) */
static int s_perfops;                           /* TGR_PERFOPS (or TGR_DLSTATS): commands counted and timed */
static int s_perfcalls;                         /* TGR_PERF or TGR_LISTLOG: list calls timed (two clock reads a call) */
static uint32_t s_runops[256], s_runopt[256];   /* TGR_PERF: top-level commands run, and their time (cycles / 8) */
uint64_t ps1_time_ns(void);
#define RG_BEGIN(id) uint32_t rg_t_##id = ST_NOW()
#define RG_END(id) (s_rg[id] += (uint64_t)(((ST_NOW() - rg_t_##id) & 0xFFFF) * 236u))   /* (counter 2 counts 8 cycles: 236 ns) */

extern uint32_t tgr_rcp_frames;
static int s_log;                               /* TGR_RCPLOG */
static uint32_t s_n_tri, s_n_piece, s_n_vtx, s_n_clip, s_n_gte, s_gte_flagor, s_n_fast, s_n_cull, s_n_rej, s_n_stamp, s_n_notok, s_n_edge;  /* TGR_PERF counts */
static uint32_t s_dstamp = 1;                   /* bumped by every display-list command that changes how triangles draw */
static uint32_t s_texgen = 1;                   /* bumped whenever tiles, loads or the TLUT mode change */
static uint32_t s_texepoch = 1;
static uint32_t s_epw[5];                       /* TGR_PERF: why the texture table moved */                 /* bumped whenever the texture table moves */
static int s_fog_mul, s_fog_ofs;                /* G_MW_FOG */
static int s_black;                             /* osViBlack: the display is off */

/* ---- game memory ----------------------------------------------------------- */
static uint32_t s_seg[16];
static uint32_t seg_addr(uint32_t a) { return (s_seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x3FFFFF; }
static const uint8_t *mem(uint32_t phys) { return tgr_rdram + (phys & 0x3FFFFF); }
/* big-endian words: one load and a byte swap when aligned (a load from RAM
 * costs the CPU several cycles, a shift one) */
static uint32_t rd32(uint32_t phys)
{
    const uint8_t *p = mem(phys);
    if (!((uintptr_t)p & 3)) {
        uint32_t v = *(const uint32_t *)p;
        return v << 24 | (v & 0xFF00) << 8 | (v >> 8 & 0xFF00) | v >> 24;
    }
    return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}
/* a display list's words: 8-byte aligned */
static inline __attribute__((always_inline)) uint32_t rd32a(uint32_t phys)
{
    uint32_t v = *(const uint32_t *)(tgr_rdram + (phys & 0x3FFFFF));
    return v << 24 | (v & 0xFF00) << 8 | (v >> 8 & 0xFF00) | v >> 24;
}
static uint16_t rd16(uint32_t phys)
{
    const uint8_t *p = mem(phys);
    if (!((uintptr_t)p & 1)) {
        uint32_t v = *(const uint16_t *)p;
        return (uint16_t)(v << 8 | v >> 8);
    }
    return (uint16_t)(p[0] << 8 | p[1]);
}

/* ---- the frame's packets ------------------------------------------------------ */
#define PKT_WORDS (128 * 1024 / 4)
#define OT_LEN 1024
static uint32_t s_pkt[2][PKT_WORDS];
static int s_buf, s_used;
static uint32_t *s_head, *s_tail;               /* the frame's list */
static uint32_t *s_ot_head[OT_LEN], *s_ot_tail[OT_LEN];
static int s_zrun, s_ot_lo, s_ot_hi;            /* a depth-sorted run is open; its buckets in use */
static int s_overflow;
static uint32_t s_n_over, s_peak_used;          /* TGR_PERF */
static uint32_t s_splitw[3];                    /* TGR_PERF: emit_tri's splits: by size, by texel span */

static uint32_t *pkt_alloc(int words)
{
    uint32_t *p;
    if (s_used + words + 1 > PKT_WORDS) {
        s_overflow = 1;
        s_n_over++;
        return NULL;
    }
    p = &s_pkt[s_buf][s_used];
    s_used += words + 1;
    if ((uint32_t)s_used > s_peak_used)
        s_peak_used = (uint32_t)s_used;
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
 * Two buffers at VRAM y 0 and 240, one drawn while the other is shown, each
 * standing for one of the game's two colour images (whose contents the game
 * relies on keeping: the paint shop redraws only what changed).  A 320-wide
 * image is 320 x 240; a 640-wide one (the menus, 640 x 480 on the N64) is
 * 640 x 240, every y halved: two 640 x 480 buffers do not fit the 1 MB of
 * VRAM, and one drawn while shown flickers. */
static uint32_t s_fbaddr[2] = { 0xFFFFFFFF, 0xFFFFFFFF };
static int s_target = -1;                       /* the buffer being drawn, -1 none */
static int s_shown = -1, s_show_pending = -1;
static int s_hires = -1;                        /* the display mode set: 1 640 x 240, 0 320 x 240 */
static int s_ysh;                               /* 1: game y halved (640-wide images) */
static uint32_t s_cimg, s_zimg;

/* the VRAM buffer standing for the game's colour image at a: a new image
 * takes the buffer used least recently (the menus' two give way to the
 * race's) */
static uint32_t s_fbused[2];
static uint32_t s_fbclock;
static int s_fbclear[2];                        /* newly given to an image: black first, as fresh RAM */

static int fb_index(uint32_t a)
{
    int i, k;
    for (i = 0; i < 2; i++)
        if (s_fbaddr[i] == a) {
            s_fbused[i] = ++s_fbclock;
            return i;
        }
    k = s_fbused[0] <= s_fbused[1] ? 0 : 1;
    tgr_log("rcp: frame buffer %d at %06X\n", k, a);
    s_fbaddr[k] = a;
    s_fbclear[k] = 1;
    s_fbused[k] = ++s_fbclock;
    return k;
}

static void fb_rect(int t, int *x, int *y, int *w, int *h)
{
    *x = 0;
    *y = t == 1 ? 240 : 0;
    *w = s_hires == 1 ? 640 : 320;
    *h = 240;
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
static uint32_t s_mvp_stamp = 1;                /* bumped whenever the MVP changes (the GTE's copy follows) */
static uint32_t s_geom;
static struct { int32_t sx, sy, sz, tx, ty, tz; } s_vp;     /* 13.2 */
static struct { uint8_t col[3]; int8_t dir[3]; } s_light[8];
static uint32_t s_light_stamp;                  /* bumped when a light changes */
static int s_nlights = 1;
static struct { int on, tile, level; uint32_t ss, ts; } s_tex;

typedef struct { int16_t a[4], b[4]; } Lin;    /* a colour as a * texel + b per channel (rgba, 8.8): the combiner's */

typedef struct {
    int32_t cx, cy, cz, cw;                     /* clip space, 8 fraction bits */
    int16_t x, y;                               /* screen, pixels */
    int16_t mx, my, mz;                         /* model space, as loaded */
    int16_t z;                                  /* ordering depth, 0 near .. OT_LEN far */
    uint8_t r, g, b, a;
    int32_t s, t;                               /* texel coordinates, 10.5 after G_TEXTURE's scale */
    uint8_t clip;                               /* 1 x<-w, 2 x>w, 4 y<-w, 8 y>w, 16 behind, 32 outside the guard band */
    uint32_t cc_stamp;                          /* the combiner state its colour was combined under */
    uint32_t fstamp;                            /* the draw state its fast-path fields were made under: */
    int16_t fr, fg, fb, ffr, ffg, ffb;          /* the GPU colour, the fog's added colour */
    int32_t fu, fv;                             /* texels from the tile's origin, 1/32 */
    uint8_t cc_flat;                            /* ... and whether with a flat shade (the first vertex's) */
    Lin col;
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

static __attribute__((noinline)) void m4_load(M4 *r, uint32_t a)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r->m[i][j] = (int32_t)((uint32_t)rd16(a + (i * 4 + j) * 2) << 16 | rd16(a + 32 + (i * 4 + j) * 2));
}

/* r = a b where b has zeros (a projection): only b's other entries' products */
static void m4_mul_sparse(M4 *r, const M4 *a, const M4 *b)
{
    M4 t;
    int i, j, k;
    for (j = 0; j < 4; j++) {
        int nk = 0, ks[4];
        for (k = 0; k < 4; k++)
            if (b->m[k][j])
                ks[nk++] = k;
        for (i = 0; i < 4; i++) {
            int64_t s = 0;
            for (k = 0; k < nk; k++)
                s += (int64_t)a->m[i][ks[k]] * b->m[ks[k]][j];
            t.m[i][j] = (int32_t)(s >> 16);
        }
    }
    *r = t;
}

static void mvp_update(void)
{
    if (s_mvp_dirty) {
        uint32_t q = ST_NOW();
        m4_mul_sparse(&s_mvp, &s_mv[s_mvn], &s_proj);
        s_su[0] += (ST_NOW() - q) & 0xFFFF;
        s_su[3]++;
        s_mvp_dirty = 0;
        s_mvp_stamp++;
    }
}

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* a light's direction in model space: the modelview's upper 3x3 applied
 * transposed, normalised to 1.14 */
static __attribute__((noinline)) void model_dir(const int8_t d[3], int32_t out[3])
{
    const M4 *m = &s_mv[s_mvn];
    int32_t v[3], mx = 0;
    uint32_t l2, l, b, r;
    int i, sh = 0;
    for (i = 0; i < 3; i++) {
        /* (an 8-bit direction by a 16.16 rotation: 32 bits do with a shift first) */
        v[i] = (d[0] * (m->m[i][0] >> 4) + d[1] * (m->m[i][1] >> 4) + d[2] * (m->m[i][2] >> 4)) >> 4;
        if (abs(v[i]) > mx)
            mx = abs(v[i]);
    }
    while ((mx >> sh) >= 0x4000)                /* (14 bits a part: the squares' sum in 32) */
        sh++;
    for (i = 0; i < 3; i++)
        v[i] >>= sh;
    l2 = (uint32_t)(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    for (l = 0, r = l2, b = 1u << 30; b; b >>= 2) {    /* the square root, a bit at a time */
        if (r >= l + b) {
            r -= l + b;
            l = (l >> 1) + b;
        } else {
            l >>= 1;
        }
    }
    if (!l)
        l = 1;
    for (i = 0; i < 3; i++)
        out[i] = (v[i] << 14) / (int32_t)l;
}

/* the ordering-table bucket of a depth: the clip w (8 fraction bits) on a
 * log scale, 32 buckets an octave (near 0 .. far OT_LEN - 1) */
static int wbucket(int32_t w)
{
    int p = 0;
    uint32_t u = (uint32_t)w;
    if (w <= 0)
        return 0;
    if (u >> 16) { p += 16; u >>= 16; }
    if (u >> 8) { p += 8; u >>= 8; }
    if (u >> 4) { p += 4; u >>= 4; }
    if (u >> 2) { p += 2; u >>= 2; }
    if (u >> 1) p += 1;
    return p * 32 + (int)(((uint32_t)w << (31 - p)) >> 26 & 31);
}

/* clip space (8 fraction bits, w at least W_NEAR) to the screen: the
 * viewport's translation plus the normalised coordinates times its scale
 * (13.2); z the ordering-table bucket from the normalised depth */
#define W_NEAR 16
#define GUARD 2                                 /* the guard band: |x|, |y| <= GUARD * w */
#define WIDE 1000                               /* a mesh vertex the GPU takes as it is: |x|, |y| <= WIDE (it reaches 1024; a
                                                 * triangle at most 1023 by 511): its record's ok 1, and 2 inside the guard band */
static void project(int32_t cx, int32_t cy, int32_t cz, int32_t cw, int16_t *px, int16_t *py, int16_t *pz)
{
    int32_t sx, sy, sz, w = cw, xx = cx, yy = cy, zz = cz;
    while (w > 0x7FFF || xx > 0x3FFFFF || xx < -0x3FFFFF || yy > 0x3FFFFF || yy < -0x3FFFFF ||
           zz > 0x3FFFFF || zz < -0x3FFFFF) {
        w >>= 1; xx >>= 1; yy >>= 1; zz >>= 1;
    }
    if (w < 1) w = 1;
    sx = s_vp.tx + (int32_t)(((xx << 8) / w) * s_vp.sx >> 8);
    sy = s_vp.ty - (int32_t)(((yy << 8) / w) * s_vp.sy >> 8);
    sz = (zz << 8) / w;                         /* normalised depth, 8 fraction bits */
    sx >>= 2;
    sy >>= 2 + s_ysh;
    *px = (int16_t)(sx < -2048 ? -2048 : sx > 2047 ? 2047 : sx);
    *py = (int16_t)(sy < -2048 ? -2048 : sy > 2047 ? 2047 : sy);
    (void)sz;
    *pz = (int16_t)wbucket(cw);
}

/* ---- the GTE's transform --------------------------------------------------------- *
 * The MVP's x, y and w columns are the GTE's rotation rows, scaled by 2^-k
 * to its 4.12 range (the ratios a projection divides are unchanged), y
 * folded with the viewport's aspect and flip, so RTPS gives the screen
 * position (OFX/OFY the viewport's centre, H its x scale, w as SZ3) and in
 * MAC1-3 the scaled clip coordinates.  A vertex the GTE saturates or
 * cannot divide for goes the CPU's way. */
static uint32_t s_gte_stamp;
static int s_gte_ok, s_gte_k, s_gte_ysh;
static int32_t s_gte_fy;                        /* the y row's factor: -(sy / sx) / 2^ysh, 16.16 */
static int32_t s_gte_ify;                       /* its inverse, 16.16 */
static int s_nogte = -1;

static int32_t sh_r(int64_t v, int sh) { return (int32_t)(sh >= 0 ? v >> sh : v << -sh); }

static uint32_t s_gte_cfg[3][8];                /* the rotation and translation registers at each scale */
static int s_gte_cur = -1;
static const int8_t s_gte_scale[3] = { 0, -3, 4 };

static int32_t s_gte_rt[3][3], s_gte_tr[3];
static uint8_t s_gte_have;                      /* (the configurations built: 1 << c) */
static void gte_cfg_build(int c)
{
    static const int8_t sc[3] = { 0, -3, 4 };
    int m = sc[c];
    uint32_t *g = s_gte_cfg[c];
    g[0] = (sh_r(s_gte_rt[0][0], m) & 0xFFFF) | (uint32_t)sh_r(s_gte_rt[0][1], m) << 16;
    g[1] = (sh_r(s_gte_rt[0][2], m) & 0xFFFF) | (uint32_t)sh_r(s_gte_rt[1][0], m) << 16;
    g[2] = (sh_r(s_gte_rt[1][1], m) & 0xFFFF) | (uint32_t)sh_r(s_gte_rt[1][2], m) << 16;
    g[3] = (sh_r(s_gte_rt[2][0], m) & 0xFFFF) | (uint32_t)sh_r(s_gte_rt[2][1], m) << 16;
    g[4] = sh_r(s_gte_rt[2][2], m) & 0xFFFF;
    g[5] = (uint32_t)sh_r(s_gte_tr[0], m);
    g[6] = (uint32_t)sh_r(s_gte_tr[1], m);
    g[7] = (uint32_t)sh_r(s_gte_tr[2], m);
    s_gte_have |= (uint8_t)(1 << c);
}

static void gte_cfg(int c)
{
    const uint32_t *g = s_gte_cfg[c];
    if (s_gte_cur == c)
        return;
    if (!(s_gte_have >> c & 1))
        gte_cfg_build(c);
    s_gte_cur = c;
    gte_ctc(GTE_RT11RT12, g[0]);
    gte_ctc(GTE_RT13RT21, g[1]);
    gte_ctc(GTE_RT22RT23, g[2]);
    gte_ctc(GTE_RT31RT32, g[3]);
    gte_ctc(GTE_RT33, g[4]);
    gte_ctc(GTE_TRX, g[5]);
    gte_ctc(GTE_TRY, g[6]);
    gte_ctc(GTE_TRZ, g[7]);
}

static void gte_setup_(void);
static uint32_t s_gsw[4];
static void gte_setup(void)
{
    uint32_t q = ST_NOW();
    s_gsw[s_gte_ysh != s_ysh ? 1 : s_gte_stamp + 1 == s_mvp_stamp ? 0 : 2]++;
    gte_setup_();
    s_su[1] += (ST_NOW() - q) & 0xFFFF;
    s_su[4]++;
}
static void gte_setup_(void)
{
    const M4 *m = &s_mvp;
    int64_t e[3][3], t[3], mx = 0;
    int r, i, k;
    s_gte_stamp = s_mvp_stamp;
    s_gte_ysh = s_ysh;
    s_gte_ok = 0;
    if (s_nogte < 0)
        s_nogte = getenv("TGR_NOGTE") != NULL;
    if (s_nogte || s_vp.sx <= 0 || s_vp.sy <= 0)
        return;
    {
        static int32_t vsx, vsy, vys = -1;     /* (the viewport's: worked out when it changes) */
        if (vsx != s_vp.sx || vsy != s_vp.sy || vys != s_ysh) {
            vsx = s_vp.sx;
            vsy = s_vp.sy;
            vys = s_ysh;
            s_gte_fy = -(int32_t)(((int64_t)s_vp.sy << 16) / s_vp.sx) >> s_ysh;
            s_gte_ify = s_gte_fy ? (int32_t)(((int64_t)1 << 32) / s_gte_fy) : 0;
        }
    }
    if (!s_gte_fy)
        return;
    for (i = 0; i < 3; i++) {
        e[0][i] = m->m[i][0];
        e[1][i] = ((int64_t)m->m[i][1] * s_gte_fy) >> 16;
        e[2][i] = m->m[i][3];
    }
    t[0] = m->m[3][0];
    t[1] = ((int64_t)m->m[3][1] * s_gte_fy) >> 16;
    t[2] = m->m[3][3];
    for (r = 0; r < 3; r++)
        for (i = 0; i < 3; i++) {
            int64_t a = e[r][i] < 0 ? -e[r][i] : e[r][i];
            if (a > mx) mx = a;
        }
    if (!mx)
        return;
    {
        /* the largest entry inside 1.0: headroom for the outputs (the first k from -12
         * with mx >> (4 + k) <= 4095, from mx's length) */
        uint32_t hi32 = (uint32_t)((uint64_t)mx >> 32), lo32 = (uint32_t)mx, u = hi32 ? hi32 : lo32;
        int bl = hi32 ? 32 : 0;
        while (u) {
            bl++;
            u >>= 1;
        }
        k = bl - 16;                            /* (mx < 2^bl: mx >> (bl - 12) <= 4095) */
        if (k < -12)
            k = -12;
        if (k > 20)
            k = 20;
        while (k > -12 && sh_r(mx, 4 + k - 1) <= 4095)
            k--;
        while (k < 20 && sh_r(mx, 4 + k) > 4095)
            k++;
    }
    s_gte_k = k;

    {
        for (r = 0; r < 3; r++) {
            for (i = 0; i < 3; i++)
                s_gte_rt[r][i] = (int32_t)sh_r(e[r][i], 4 + k);
            s_gte_tr[r] = (int32_t)sh_r(t[r], 16 + k);
        }
        /* the same transform at three scales (their w a power of two apart):
         * the base one; 8 times larger (a vertex too near for the base one's
         * divide); 16 times smaller (one too far for its 16-bit depth); the
         * last two made when first wanted */
        s_gte_have = 0;
        s_gte_cur = -1;
        gte_cfg(0);
        gte_ctc(GTE_OFX, (uint32_t)s_vp.tx << 14);
        gte_ctc(GTE_OFY, (uint32_t)(s_vp.ty >> s_ysh) << 14);
        gte_ctc(GTE_H, (uint32_t)(s_vp.sx >> 2));
    }
    s_gte_ok = 1;
}

/* a vertex through the GTE: 1 when it gave the screen and clip coordinates */
static __attribute__((noinline)) int gte_vertex(Vtx *v, int32_t x, int32_t y, int32_t z)
{
    uint32_t flag, sxy;
    int32_t m1, m2, m3, sh = 8 + s_gte_k;
    gte_mtc(GTE_VXY0, (uint32_t)(y << 16) | (uint32_t)(x & 0xFFFF));
    gte_mtc(GTE_VZ0, (uint32_t)z);
    gte_rtps();
    flag = gte_cfc(GTE_FLAG);
    m3 = (int32_t)gte_mfc(GTE_MAC3);
    /* any saturation (MAC, IR, SZ, SX/SY) or a divide overflow: the CPU's way */
    s_gte_flagor |= flag;
    if ((flag & 0x7FC7F000u) || m3 <= 0)
        return 0;
    m1 = (int32_t)gte_mfc(GTE_MAC1);
    m2 = (int32_t)gte_mfc(GTE_MAC2);
    sxy = gte_mfc(GTE_SXY2);
    v->cw = sh_r(m3, -sh);
    if (v->cw < W_NEAR)
        return 0;
    v->cx = sh_r(m1, -sh);
    v->cy = (int32_t)(((int64_t)sh_r(m2, -sh) * s_gte_ify) >> 16);
    v->x = (int16_t)(sxy & 0xFFFF);
    v->y = (int16_t)(sxy >> 16);
    v->z = (int16_t)wbucket(v->cw);
    v->clip = 0;
    if (v->cx < -v->cw) v->clip |= 1;
    if (v->cx > v->cw) v->clip |= 2;
    if (v->cy < -v->cw) v->clip |= 4;
    if (v->cy > v->cw) v->clip |= 8;
    if (abs(v->cx) > GUARD * v->cw || abs(v->cy) > GUARD * v->cw)
        v->clip |= 32;                          /* outside the guard band: clipped in triangle() */
    return 1;
}

static uint32_t hash32(uint32_t h, uint32_t v);
static uint32_t s_lsig;                         /* the last lit vertices' lighting (display lists kept) */

/* a vertex as the display list holds it, in the CPU's byte order */
typedef struct { int16_t x, y, z, pad, s, t; uint8_t c[4]; } NVtx;   /* (16 bytes: x and y one word) */

static void load_vertices_n(const NVtx *nv, int n, int v0);

static uint32_t s_mvstale;

static __attribute__((noinline)) void load_vertices(uint32_t a, int n, int v0)
{
    NVtx nv[32];
    int i;
    s_mvstale &= ~(n >= 32 ? 0xFFFFFFFFu : (((1u << n) - 1) << v0));
    for (i = 0; i < n && v0 + i < 32; i++) {
        uint32_t p = a + i * 16;
        const uint8_t *c = mem(p + 12);
        nv[i].x = (int16_t)rd16(p); nv[i].y = (int16_t)rd16(p + 2); nv[i].z = (int16_t)rd16(p + 4);
        nv[i].s = (int16_t)rd16(p + 8); nv[i].t = (int16_t)rd16(p + 10);
        nv[i].c[0] = c[0]; nv[i].c[1] = c[1]; nv[i].c[2] = c[2]; nv[i].c[3] = c[3];
    }
    load_vertices_n(nv, i, v0);
}

/* the lights in model space: again only when the modelview or the lights
 * changed; with them the lighting as kept display lists know it (the
 * directions to 1/256, the colours) */
static void light_setup(int32_t ldir[8][3])
{
    static int32_t cached[8][3];
    static uint32_t cached_stamp, cached_lights;
    uint32_t ls = s_light_stamp ^ (uint32_t)s_nlights << 24 ^ (uint32_t)s_mvn << 28;
    int k;
    if (cached_stamp != s_mvp_stamp || cached_lights != ls) {
        uint32_t h = 0x811C9DC5u ^ (uint32_t)s_nlights, q = ST_NOW();
        s_su[5]++;
        for (k = 0; k < s_nlights; k++)
            model_dir(s_light[k].dir, cached[k]);
        cached_stamp = s_mvp_stamp;
        cached_lights = ls;
        {
            /* the GTE's lights the same: directions (4.12) as the light matrix's
             * rows, colours (x16) as the colour matrix's columns, the ambient as
             * the back colour; NCS on a normal gives the RSP's shade (the first 3 lights) */
            int32_t l[3][3], c[3][3];
            int j;
            for (k = 0; k < 3; k++)
                for (j = 0; j < 3; j++) {
                    l[k][j] = k < s_nlights ? cached[k][j] >> 2 : 0;
                    c[j][k] = k < s_nlights ? s_light[k].col[j] * 16 : 0;
                }
            gte_ctc(GTE_L11L12, (l[0][0] & 0xFFFF) | (uint32_t)l[0][1] << 16);
            gte_ctc(GTE_L13L21, (l[0][2] & 0xFFFF) | (uint32_t)l[1][0] << 16);
            gte_ctc(GTE_L22L23, (l[1][1] & 0xFFFF) | (uint32_t)l[1][2] << 16);
            gte_ctc(GTE_L31L32, (l[2][0] & 0xFFFF) | (uint32_t)l[2][1] << 16);
            gte_ctc(GTE_L33, l[2][2] & 0xFFFF);
            gte_ctc(GTE_LR1LR2, (c[0][0] & 0xFFFF) | (uint32_t)c[0][1] << 16);
            gte_ctc(GTE_LR3LG1, (c[0][2] & 0xFFFF) | (uint32_t)c[1][0] << 16);
            gte_ctc(GTE_LG2LG3, (c[1][1] & 0xFFFF) | (uint32_t)c[1][2] << 16);
            gte_ctc(GTE_LB1LB2, (c[2][0] & 0xFFFF) | (uint32_t)c[2][1] << 16);
            gte_ctc(GTE_LB3, c[2][2] & 0xFFFF);
            gte_ctc(GTE_RBK, s_light[s_nlights & 7].col[0] * 16);
            gte_ctc(GTE_GBK, s_light[s_nlights & 7].col[1] * 16);
            gte_ctc(GTE_BBK, s_light[s_nlights & 7].col[2] * 16);
        }
        for (k = 0; k <= s_nlights && k < 8; k++) {
            if (k < s_nlights)
                h = hash32(hash32(hash32(h, (uint32_t)(cached[k][0] >> 6)), (uint32_t)(cached[k][1] >> 6)),
                           (uint32_t)(cached[k][2] >> 6));
            h = hash32(h, (uint32_t)s_light[k].col[0] << 16 | s_light[k].col[1] << 8 | s_light[k].col[2]);
        }
        s_lsig = h;
        s_su[2] += (ST_NOW() - q) & 0xFFFF;
    }
    if (ldir)
        memcpy(ldir, cached, sizeof cached);
}

static __attribute__((noinline)) void load_vertices_n(const NVtx *nv, int n, int v0)
{
    int32_t ldir[8][3];
    int i, k;
    const M4 *m;
    mvp_update();
    m = &s_mvp;
    if (s_gte_stamp != s_mvp_stamp || s_gte_ysh != s_ysh)
        gte_setup();
    if (s_geom & G_LIGHTING)
        light_setup(ldir);
    s_n_vtx += (uint32_t)n;
    for (i = 0; i < n && v0 + i < 32; i++) {
        const uint8_t *c = nv[i].c;
        int32_t x = nv[i].x, y = nv[i].y, z = nv[i].z;
        int32_t s = nv[i].s, t = nv[i].t;
        Vtx *v = &s_vtx[v0 + i];
        v->mx = (int16_t)x; v->my = (int16_t)y; v->mz = (int16_t)z;
        if (s_gte_ok && gte_vertex(v, x, y, z)) {
            s_n_gte++;
            if (s_geom & G_FOG)
                v->cz = (int32_t)(((int64_t)x * m->m[0][2] + (int64_t)y * m->m[1][2] + (int64_t)z * m->m[2][2] +
                                   m->m[3][2]) >> 8);
            goto colours;
        }
        {
        int32_t cx = (int32_t)(((int64_t)x * m->m[0][0] + (int64_t)y * m->m[1][0] + (int64_t)z * m->m[2][0] + m->m[3][0]) >> 8);
        int32_t cy = (int32_t)(((int64_t)x * m->m[0][1] + (int64_t)y * m->m[1][1] + (int64_t)z * m->m[2][1] + m->m[3][1]) >> 8);
        int32_t cz = (int32_t)(((int64_t)x * m->m[0][2] + (int64_t)y * m->m[1][2] + (int64_t)z * m->m[2][2] + m->m[3][2]) >> 8);
        int32_t cw = (int32_t)(((int64_t)x * m->m[0][3] + (int64_t)y * m->m[1][3] + (int64_t)z * m->m[2][3] + m->m[3][3]) >> 8);
        v->cx = cx; v->cy = cy; v->cz = cz; v->cw = cw;
        v->clip = 0;
        if (cw < W_NEAR)
            v->clip = 16;                       /* behind (or at) the eye: clipped in triangle() */
        if (cx < -cw) v->clip |= 1;
        if (cx > cw) v->clip |= 2;
        if (cy < -cw) v->clip |= 4;
        if (cy > cw) v->clip |= 8;
        if (abs(cx) > GUARD * cw || abs(cy) > GUARD * cw)
            v->clip |= 32;
        if (!(v->clip & 16))
            project(cx, cy, cz, cw, &v->x, &v->y, &v->z);
        }
colours:
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
        v->cc_stamp = 0;
        v->fstamp = 0;
        if (s_geom & G_FOG) {                   /* the shade alpha is the fog: depth times mul plus offset */
            int32_t f, w = v->cw < W_NEAR ? W_NEAR : v->cw, zz = v->cz;
            while (w > 0x7FFF || zz > 0x3FFFFF || zz < -0x3FFFFF) { w >>= 1; zz >>= 1; }
            f = ((zz << 8) / (w ? w : 1)) * s_fog_mul / 256 + s_fog_ofs;
            v->a = (uint8_t)(f < 0 ? 0 : f > 255 ? 255 : f);
        }
        v->s = (s * (int32_t)s_tex.ss) >> 16;   /* (16 by 16 bits: a 32-bit product) */
        v->t = (t * (int32_t)s_tex.ts) >> 16;
    }
}

/* ---- vertices for kept display lists -------------------------------------------- *
 * A kept list's vertices go through the GTE into a short record (the screen
 * place, the ordering bucket, w, which sides of the screen it is past);
 * the full vertex (clip space, colour, texture coordinates) is made only for
 * a triangle that needs the general path. */
typedef struct { int16_t x, y, z; uint8_t clip, ok; int32_t w; uint8_t r, g, b, lit; } MVtx;
#define s_mvx ((MVtx *)0x1F800000u)              /* in the CPU's 1 KB of fast RAM (512 bytes of it) */
static const NVtx *s_mvsrc[32];
static uint32_t s_mvstale;                      /* slots whose s_vtx is not yet made from s_mvsrc */
static int s_mb_x0, s_mb_x1, s_mb_y0, s_mb_y1, s_mb_gx0, s_mb_gx1, s_mb_gy0, s_mb_gy1;
static uint32_t s_mb_stamp;
static uint32_t s_flagbits[32], s_flagbits2[3][32];                 /* TGR_PERF: RTPT triples clean ([0]) and each flag bit's */

/* a slot's full vertex, made now if the list's loader left it short */
static void mesh_fill(int slot)
{
    if (s_mvstale >> slot & 1) {
        s_mvstale &= ~(1u << slot);
        load_vertices_n(s_mvsrc[slot], 1, slot);
        s_n_vtx--;
    }
}

/* 0 when the GTE cannot take them (then the general loader does) */
static int mesh_vertices_one(const NVtx *nv, int n, int v0, int lit);

/* the GTE three at a time (RTPT, and NCT for the shades): a triple it flags
 * goes vertex by vertex */
static __attribute__((noinline)) int mesh_vertices_gen(const NVtx *nv, int n, int v0, int lit)
{
    int i, sh, zb;
    {
        RG_BEGIN(RG_SETUP);
        mvp_update();
        if (s_gte_stamp != s_mvp_stamp || s_gte_ysh != s_ysh)
            gte_setup();
        if (lit && s_gte_ok)
            light_setup(NULL);
        RG_END(RG_SETUP);
    }
    if (!s_gte_ok)
        return 0;
    if (s_mb_stamp != s_mvp_stamp) {            /* the screen's sides and the guard band, in pixels */
        int ys = 2 + s_ysh;
        s_mb_stamp = s_mvp_stamp;
        s_mb_x0 = (s_vp.tx - s_vp.sx) >> 2; s_mb_x1 = (s_vp.tx + s_vp.sx) >> 2;
        s_mb_y0 = (s_vp.ty - s_vp.sy) >> ys; s_mb_y1 = (s_vp.ty + s_vp.sy) >> ys;
        s_mb_gx0 = (s_vp.tx - GUARD * s_vp.sx) >> 2; s_mb_gx1 = (s_vp.tx + GUARD * s_vp.sx) >> 2;
        s_mb_gy0 = (s_vp.ty - GUARD * s_vp.sy) >> ys; s_mb_gy1 = (s_vp.ty + GUARD * s_vp.sy) >> ys;
    }
    sh = 8 + s_gte_k;
    zb = 32 * sh;                               /* (the bucket of w = SZ << sh: SZ's, 32 an octave on) */
    if (n > 32 - v0)
        n = 32 - v0;
    for (i = 0; i + 3 <= n; i += 3) {
        const uint32_t *q0 = (const uint32_t *)&nv[i], *q1 = (const uint32_t *)&nv[i + 1], *q2 = (const uint32_t *)&nv[i + 2];
        int cfg, nearm;
        uint32_t flagged;
        uint32_t sxy[3], sz[3], rgb[3] = { 0, 0, 0 };
        int k;
        uint32_t tc0 = *(volatile uint32_t *)0xBF801120;
        gte_mtc(GTE_VXY0, q0[0]); gte_mtc(GTE_VZ0, (uint32_t)(int32_t)(int16_t)q0[1]);
        gte_mtc(GTE_VXY1, q1[0]); gte_mtc(GTE_VZ1, (uint32_t)(int32_t)(int16_t)q1[1]);
        gte_mtc(GTE_VXY2, q2[0]); gte_mtc(GTE_VZ2, (uint32_t)(int32_t)(int16_t)q2[1]);
        gte_rtpt();
        cfg = 0;
        flagged = gte_cfc(GTE_FLAG) & 0x7FC7F000u;
        if (flagged)
            s_flagbits[1]++;
        else
            s_flagbits[0]++;
        sxy[0] = gte_mfc(GTE_SXY0); sxy[1] = gte_mfc(GTE_SXY1); sxy[2] = gte_mfc(GTE_SXY2);
        sz[0] = gte_mfc(GTE_SZ1); sz[1] = gte_mfc(GTE_SZ2); sz[2] = gte_mfc(GTE_SZ3);
        nearm = 0;
        if (flagged) {
            /* in front but too near for the divide: the transform 8 times larger
             * puts them back in its reach */
            int hz = (s_vp.sx >> 3) + 1;
            for (k = 0; k < 3; k++)
                if (sz[k] && (int32_t)sz[k] < hz)
                    nearm |= 1 << k;
            if (nearm) {
                uint32_t nxy[3], nz[3], f2;
                gte_cfg(1);
                gte_rtpt();
                f2 = gte_cfc(GTE_FLAG);
                nxy[0] = gte_mfc(GTE_SXY0); nxy[1] = gte_mfc(GTE_SXY1); nxy[2] = gte_mfc(GTE_SXY2);
                nz[0] = gte_mfc(GTE_SZ1); nz[1] = gte_mfc(GTE_SZ2); nz[2] = gte_mfc(GTE_SZ3);
                gte_cfg(0);
                (void)f2;
                for (k = 0; k < 3; k++)
                    if (nearm >> k & 1) {
                        if ((int32_t)nz[k] >= hz && nz[k] < 0xFFFF) {
                            sxy[k] = nxy[k];
                            sz[k] = nz[k];
                        } else {
                            nearm &= ~(1 << k);
                            sz[k] = 1;          /* (still too near: by itself) */
                        }
                    }
            }
        }
        if (lit) {
            gte_mtc(GTE_VXY0, (uint32_t)((int32_t)(int8_t)nv[i].c[1] * 32 << 16) | (uint16_t)((int8_t)nv[i].c[0] * 32));
            gte_mtc(GTE_VZ0, (uint32_t)((int32_t)(int8_t)nv[i].c[2] * 32));
            gte_mtc(GTE_VXY1, (uint32_t)((int32_t)(int8_t)nv[i + 1].c[1] * 32 << 16) | (uint16_t)((int8_t)nv[i + 1].c[0] * 32));
            gte_mtc(GTE_VZ1, (uint32_t)((int32_t)(int8_t)nv[i + 1].c[2] * 32));
            gte_mtc(GTE_VXY2, (uint32_t)((int32_t)(int8_t)nv[i + 2].c[1] * 32 << 16) | (uint16_t)((int8_t)nv[i + 2].c[0] * 32));
            gte_mtc(GTE_VZ2, (uint32_t)((int32_t)(int8_t)nv[i + 2].c[2] * 32));
            gte_nct();
            rgb[0] = gte_mfc(GTE_RGB0); rgb[1] = gte_mfc(GTE_RGB1); rgb[2] = gte_mfc(GTE_RGB2);
        }
        for (k = 0; k < 3; k++) {
            MVtx *q = &s_mvx[v0 + i + k];
            int x = (int16_t)(sxy[k] & 0xFFFF), y = (int16_t)(sxy[k] >> 16), c = 0;
            int shc = sh + s_gte_scale[nearm >> k & 1 ? 1 : cfg];
            int32_t w = shc >= 0 ? (int32_t)sz[k] << shc : (int32_t)sz[k] >> -shc;
            s_mvsrc[v0 + i + k] = &nv[i + k];
            if (flagged) {
                /* the GTE flags the three at once: behind the eye (the depth saturated at
                 * 0: no CPU work, the triangles' clip path sorts them out), too near for its
                 * divide or saturated on the screen (by itself), else this one is good */
                if (!sz[k]) {
                    s_flagbits[2]++;
                    q->ok = 0;
                    q->clip = 16;
                    q->lit = (uint8_t)lit;
                    q->r = (uint8_t)rgb[k]; q->g = (uint8_t)(rgb[k] >> 8); q->b = (uint8_t)(rgb[k] >> 16);
                    s_mvstale |= 1u << (v0 + i + k);
                    continue;
                }
                if ((!(nearm >> k & 1) && (int32_t)sz[k] < (s_vp.sx >> 3) + 1) || x <= -1024 || x >= 1023 ||
                    y <= -1024 || y >= 1023 || sz[k] >= 0xFFFF) {
                    /* (too near even so, or off the GTE's screen: the clip path's; its flags the CPU's) */
                    const M4 *mm = &s_mvp;
                    int32_t X = nv[i + k].x, Y = nv[i + k].y, Z = nv[i + k].z, ccx, ccy, ccw;
                    s_flagbits[(!(nearm >> k & 1) && (int32_t)sz[k] < (s_vp.sx >> 3) + 1) ? 3 : sz[k] >= 0xFFFF ? 4 : 5]++;
                    ccx = (int32_t)(((int64_t)X * mm->m[0][0] + (int64_t)Y * mm->m[1][0] + (int64_t)Z * mm->m[2][0] + mm->m[3][0]) >> 8);
                    ccy = (int32_t)(((int64_t)X * mm->m[0][1] + (int64_t)Y * mm->m[1][1] + (int64_t)Z * mm->m[2][1] + mm->m[3][1]) >> 8);
                    ccw = (int32_t)(((int64_t)X * mm->m[0][3] + (int64_t)Y * mm->m[1][3] + (int64_t)Z * mm->m[2][3] + mm->m[3][3]) >> 8);
                    c = 0;
                    if (ccw < W_NEAR) c |= 16;
                    if (ccx < -ccw) c |= 1;
                    if (ccx > ccw) c |= 2;
                    if (ccy < -ccw) c |= 4;
                    if (ccy > ccw) c |= 8;
                    q->ok = 0;
                    q->clip = (uint8_t)c;
                    q->lit = (uint8_t)lit;
                    q->r = (uint8_t)rgb[k]; q->g = (uint8_t)(rgb[k] >> 8); q->b = (uint8_t)(rgb[k] >> 16);
                    s_mvstale |= 1u << (v0 + i + k);
                    continue;
                }
            }
            if (!sz[k] || w < W_NEAR) {         /* (too near: the vertex by itself) */
                mesh_vertices_one(nv + i + k, 1, v0 + i + k, lit);
                continue;
            }
            if (x < s_mb_x0) c |= 1;
            if (x > s_mb_x1) c |= 2;
            if (y < s_mb_y0) c |= 4;
            if (y > s_mb_y1) c |= 8;
            q->ok = (uint8_t)((!(x < -WIDE || x > WIDE || y < -WIDE || y > WIDE)) |
                              (!(x < s_mb_gx0 || x > s_mb_gx1 || y < s_mb_gy0 || y > s_mb_gy1)) << 1);
            q->clip = (uint8_t)c;
            q->x = (int16_t)x;
            q->y = (int16_t)y;
            q->z = (int16_t)(wbucket((int32_t)sz[k]) + zb + 32 * s_gte_scale[nearm >> k & 1 ? 1 : cfg]);
            q->w = w;
            q->lit = (uint8_t)lit;
            if (lit) {
                q->r = (uint8_t)rgb[k]; q->g = (uint8_t)(rgb[k] >> 8); q->b = (uint8_t)(rgb[k] >> 16);
            }
        }
        s_mvstale |= 7u << (v0 + i);
        s_n_gte += 3;
        s_n_vtx += 3;
        if (!flagged) {
            s_flagbits[6] += (*(volatile uint32_t *)0xBF801120 - tc0) & 0xFFFF;
            s_flagbits[7]++;
        }
    }
    if (i < n)
        mesh_vertices_one(nv + i, n - i, v0 + i, lit);
    return 1;
}

/* the screen's sides, the guard band and the depth scaling, for the vertex
 * pass: in the CPU's fast RAM (one cycle a load) */
typedef struct { int32_t x0, x1, y0, y1, gx0, gx1, gy0, gy1, sh, zb, hz; } MBnd;
#define s_mbk ((MBnd *)0x1F800300u)
static const int8_t s_lg8[256] = {      /* the top bit's place in a byte */
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };

/* one vertex of a clean triple (the GTE took all three) into its record */
static inline __attribute__((always_inline)) void mv_put(MVtx *q, uint32_t sxy, uint32_t sz, uint32_t rgb, const MBnd *K)
{
    int x = (int16_t)sxy, y = (int32_t)sxy >> 16, p;
    q->clip = (uint8_t)((x < K->x0) | (x > K->x1) << 1 | (y < K->y0) << 2 | (y > K->y1) << 3);
    q->ok = (uint8_t)((!((x < -WIDE) | (x > WIDE) | (y < -WIDE) | (y > WIDE))) |
                      (!((x < K->gx0) | (x > K->gx1) | (y < K->gy0) | (y > K->gy1))) << 1);
    *(uint32_t *)&q->x = sxy;                   /* (x and y in one store) */
    p = sz >> 8 ? 8 + s_lg8[sz >> 8] : s_lg8[sz];
    q->z = (int16_t)(p * 32 + (int)((sz << (15 - p)) >> 10 & 31) + K->zb);
    q->w = K->sh >= 0 ? (int32_t)sz << K->sh : (int32_t)sz >> -K->sh;
    *(uint32_t *)&q->r = rgb;                   /* (r, g, b and the lit flag) */
}

/* one vertex of a triple the GTE flagged: good if its own results are (in
 * front, past the near plane and the divide's limit, on its screen, its
 * depth not saturated); else for the clip path, flagged behind the eye (a
 * depth of 0) or by the sides its clamped screen place is past (a too-near
 * vertex's true place is further out the same way) */
static inline __attribute__((always_inline)) void mv_put_rough(MVtx *q, uint32_t sxy, uint32_t sz, uint32_t rgb, const MBnd *K)
{
    int x = (int16_t)sxy, y = (int32_t)sxy >> 16;
    int32_t hz = K->hz & 0xFFFFF;
    if (sz >= (uint32_t)hz && sz < 0xFFFF && x > -1024 && x < 1023 && y > -1024 && y < 1023 &&
        (K->sh >= 0 ? (int32_t)sz << K->sh : (int32_t)sz >> -K->sh) >= W_NEAR) {
        mv_put(q, sxy, sz, rgb, K);
        return;
    }
    q->ok = 0;
    if (!sz)
        q->clip = 16;
    else if (sz >= 0xFFFF)
        q->clip = 0;                            /* (too far: no telling) */
    else
        q->clip = (uint8_t)((x < K->x0) | (x > K->x1) << 1 | (y < K->y0) << 2 | (y > K->y1) << 3);
    *(uint32_t *)&q->r = rgb;
}

static inline __attribute__((always_inline)) uint32_t nrm_xy(uint32_t c) /* a packed normal's x, y (x32) */
{
    return (uint32_t)(((int32_t)(c << 16) >> 24) * 32) << 16 | (uint16_t)(((int32_t)(c << 24) >> 24) * 32);
}

/* the GTE three at a time (RTPT, and NCT for the shades); a triple it flags
 * (behind the eye, too near, off its screen) goes the careful way */
static int mesh_vertices(const NVtx *nv, int n, int v0, int lit)
{
    MBnd *K = s_mbk;
    MVtx *q;
    int i;
    {
        RG_BEGIN(RG_SETUP);
        mvp_update();
        if (s_gte_stamp != s_mvp_stamp || s_gte_ysh != s_ysh)
            gte_setup();
        if (lit && s_gte_ok)
            light_setup(NULL);
        RG_END(RG_SETUP);
    }
    if (!s_gte_ok)
        return 0;
    if (s_mb_stamp != s_mvp_stamp || K->hz != (s_vp.sx >> 3) + 1 + (s_gte_k << 20)) {
        int ys = 2 + s_ysh;
        s_mb_stamp = s_mvp_stamp;
        s_mb_x0 = (s_vp.tx - s_vp.sx) >> 2; s_mb_x1 = (s_vp.tx + s_vp.sx) >> 2;
        s_mb_y0 = (s_vp.ty - s_vp.sy) >> ys; s_mb_y1 = (s_vp.ty + s_vp.sy) >> ys;
        s_mb_gx0 = (s_vp.tx - GUARD * s_vp.sx) >> 2; s_mb_gx1 = (s_vp.tx + GUARD * s_vp.sx) >> 2;
        s_mb_gy0 = (s_vp.ty - GUARD * s_vp.sy) >> ys; s_mb_gy1 = (s_vp.ty + GUARD * s_vp.sy) >> ys;
        K->x0 = s_mb_x0; K->x1 = s_mb_x1; K->y0 = s_mb_y0; K->y1 = s_mb_y1;
        K->gx0 = s_mb_gx0; K->gx1 = s_mb_gx1; K->gy0 = s_mb_gy0; K->gy1 = s_mb_gy1;
        K->sh = 8 + s_gte_k;
        K->zb = 32 * (8 + s_gte_k);
        K->hz = (s_vp.sx >> 3) + 1 + (s_gte_k << 20);
    }
    if (n > 32 - v0)
        n = 32 - v0;
    q = &s_mvx[v0];
    for (i = 0; i + 3 <= n; i += 3, q += 3) {
        const uint32_t *p0 = (const uint32_t *)&nv[i];
        uint32_t s0, s1, s2, z0, z1, z2, c0 = (uint32_t)lit << 24, c1 = c0, c2 = c0, flagged;
        gte_mtc(GTE_VXY0, p0[0]); gte_mtc(GTE_VZ0, (uint32_t)(int32_t)(int16_t)p0[1]);
        gte_mtc(GTE_VXY1, p0[4]); gte_mtc(GTE_VZ1, (uint32_t)(int32_t)(int16_t)p0[5]);
        gte_mtc(GTE_VXY2, p0[8]); gte_mtc(GTE_VZ2, (uint32_t)(int32_t)(int16_t)p0[9]);
        gte_rtpt();
        flagged = gte_cfc(GTE_FLAG) & 0x7FC7F000u;
        s_flagbits[flagged ? 1 : 0]++;
        s0 = gte_mfc(GTE_SXY0); s1 = gte_mfc(GTE_SXY1); s2 = gte_mfc(GTE_SXY2);
        z0 = gte_mfc(GTE_SZ1); z1 = gte_mfc(GTE_SZ2); z2 = gte_mfc(GTE_SZ3);
        if (lit) {
            uint32_t n0 = p0[3], n1 = p0[7], n2 = p0[11];   /* (the packed normals) */
            gte_mtc(GTE_VXY0, nrm_xy(n0)); gte_mtc(GTE_VZ0, (uint32_t)(((int32_t)(n0 << 8) >> 24) * 32));
            gte_mtc(GTE_VXY1, nrm_xy(n1)); gte_mtc(GTE_VZ1, (uint32_t)(((int32_t)(n1 << 8) >> 24) * 32));
            gte_mtc(GTE_VXY2, nrm_xy(n2)); gte_mtc(GTE_VZ2, (uint32_t)(((int32_t)(n2 << 8) >> 24) * 32));
            gte_nct();
            c0 = (gte_mfc(GTE_RGB0) & 0xFFFFFF) | 1u << 24;
            c1 = (gte_mfc(GTE_RGB1) & 0xFFFFFF) | 1u << 24;
            c2 = (gte_mfc(GTE_RGB2) & 0xFFFFFF) | 1u << 24;
        }
        if (flagged || ((int32_t)z0 << K->sh) < W_NEAR || ((int32_t)z1 << K->sh) < W_NEAR || ((int32_t)z2 << K->sh) < W_NEAR) {
            mv_put_rough(q, s0, z0, c0, K);
            mv_put_rough(q + 1, s1, z1, c1, K);
            mv_put_rough(q + 2, s2, z2, c2, K);
        } else {
            mv_put(q, s0, z0, c0, K);
            mv_put(q + 1, s1, z1, c1, K);
            mv_put(q + 2, s2, z2, c2, K);
        }
        s_mvsrc[v0 + i] = &nv[i];
        s_mvsrc[v0 + i + 1] = &nv[i + 1];
        s_mvsrc[v0 + i + 2] = &nv[i + 2];
        s_mvstale |= 7u << (v0 + i);
    }
    s_n_gte += (uint32_t)i;
    s_n_vtx += (uint32_t)i;
    if (i < n)
        mesh_vertices_gen(nv + i, n - i, v0 + i, lit);
    return 1;
}

static int mesh_vertices_one(const NVtx *nv, int n, int v0, int lit)
{
    int i, sh;
    mvp_update();
    if (s_gte_stamp != s_mvp_stamp || s_gte_ysh != s_ysh)
        gte_setup();
    if (!s_gte_ok)
        return 0;
    if (lit)
        light_setup(NULL);
    if (s_mb_stamp != s_mvp_stamp) {            /* the screen's sides and the guard band, in pixels */
        int ys = 2 + s_ysh;
        s_mb_stamp = s_mvp_stamp;
        s_mb_x0 = (s_vp.tx - s_vp.sx) >> 2; s_mb_x1 = (s_vp.tx + s_vp.sx) >> 2;
        s_mb_y0 = (s_vp.ty - s_vp.sy) >> ys; s_mb_y1 = (s_vp.ty + s_vp.sy) >> ys;
        s_mb_gx0 = (s_vp.tx - GUARD * s_vp.sx) >> 2; s_mb_gx1 = (s_vp.tx + GUARD * s_vp.sx) >> 2;
        s_mb_gy0 = (s_vp.ty - GUARD * s_vp.sy) >> ys; s_mb_gy1 = (s_vp.ty + GUARD * s_vp.sy) >> ys;
    }
    sh = 8 + s_gte_k;
    s_n_vtx += (uint32_t)n;
    for (i = 0; i < n && v0 + i < 32; i++) {
        MVtx *q = &s_mvx[v0 + i];
        uint32_t flag, sxy;
        int32_t m3, m1, m2, w;
        int x, y, c;
        s_mvsrc[v0 + i] = &nv[i];
        s_mvstale |= 1u << (v0 + i);
        gte_mtc(GTE_VXY0, (uint32_t)((int32_t)nv[i].y << 16) | (uint16_t)nv[i].x);
        gte_mtc(GTE_VZ0, (uint32_t)(int32_t)nv[i].z);
        gte_rtps();
        flag = gte_cfc(GTE_FLAG);
        m3 = (int32_t)gte_mfc(GTE_MAC3);
        m1 = (int32_t)gte_mfc(GTE_MAC1);
        m2 = (int32_t)gte_mfc(GTE_MAC2);
        sxy = gte_mfc(GTE_SXY2);
        w = sh_r(m3, -sh);
        q->lit = 0;
        if (lit) {                              /* its shade: the lights on its normal, by the GTE */
            uint32_t rgb;
            gte_mtc(GTE_VXY0, (uint32_t)((int32_t)(int8_t)nv[i].c[1] * 32 << 16) | (uint16_t)((int8_t)nv[i].c[0] * 32));
            gte_mtc(GTE_VZ0, (uint32_t)((int32_t)(int8_t)nv[i].c[2] * 32));
            gte_ncs();
            rgb = gte_mfc(GTE_RGB2);
            q->r = (uint8_t)rgb; q->g = (uint8_t)(rgb >> 8); q->b = (uint8_t)(rgb >> 16);
            q->lit = 1;
        }
        if ((flag & 0x7FC7F000u) || m3 <= 0 || w < W_NEAR) {
            /* behind, saturated: the general path; its clip flags the CPU's (the
             * GTE's accumulators are too coarse this near) */
            const M4 *mm = &s_mvp;
            int32_t X = nv[i].x, Y = nv[i].y, Z = nv[i].z;
            int32_t ccx = (int32_t)(((int64_t)X * mm->m[0][0] + (int64_t)Y * mm->m[1][0] + (int64_t)Z * mm->m[2][0] + mm->m[3][0]) >> 8);
            int32_t ccy = (int32_t)(((int64_t)X * mm->m[0][1] + (int64_t)Y * mm->m[1][1] + (int64_t)Z * mm->m[2][1] + mm->m[3][1]) >> 8);
            int32_t ccw = (int32_t)(((int64_t)X * mm->m[0][3] + (int64_t)Y * mm->m[1][3] + (int64_t)Z * mm->m[2][3] + mm->m[3][3]) >> 8);
            (void)m1; (void)m2;
            c = 0;
            if (ccw < W_NEAR) c |= 16;
            if (ccx < -ccw) c |= 1;
            if (ccx > ccw) c |= 2;
            if (ccy < -ccw) c |= 4;
            if (ccy > ccw) c |= 8;
            q->ok = 0;
            q->clip = (uint8_t)c;
            continue;
        }
        x = (int16_t)(sxy & 0xFFFF);
        y = (int16_t)(sxy >> 16);
        c = 0;
        if (x < s_mb_x0) c |= 1;
        if (x > s_mb_x1) c |= 2;
        if (y < s_mb_y0) c |= 4;
        if (y > s_mb_y1) c |= 8;
        q->ok = (uint8_t)((!(x < -WIDE || x > WIDE || y < -WIDE || y > WIDE)) |
                          (!(x < s_mb_gx0 || x > s_mb_gx1 || y < s_mb_gy0 || y > s_mb_gy1)) << 1);
        q->clip = (uint8_t)c;
        q->x = (int16_t)x;
        q->y = (int16_t)y;
        q->z = (int16_t)wbucket(w);
        q->w = w;
        s_n_gte++;
    }
    return 1;
}

/* ---- the RDP's state ------------------------------------------------------------ */
static uint32_t s_omh, s_oml, s_fill;
static uint32_t s_timg;
static int s_timg_siz, s_timg_w;
static uint32_t s_cc0, s_cc1;
static uint8_t s_prim[4], s_env[4], s_blend[4], s_fogc[4], s_prim_lod;
static int s_scissor[4];
typedef struct {
    int fmt, siz, line, tmem, pal, cmt, maskt, shiftt, cms, masks, shifts;
    int uls, ult, lrs, lrt;
} Tile;
static Tile s_tile[8];

/* what filled TMEM: each load's source and shape, replayed on a cache miss */
typedef struct {
    uint8_t kind;                               /* 1 block, 2 tile, 3 tlut */
    uint8_t siz;
    uint16_t w;
    uint32_t src;
    int16_t a0, a1, a2, a3, line, tmem, words, pad;
} Load;                                         /* (24 bytes: copied whole as the loads change) */
#define NLOADS 16
static Load s_loads[NLOADS];
static int s_nloads;

static void note_load(const Load *l)
{
    int i, j;
    if (s_nloads) {
        /* the common case: it overlaps the last load only (the same TMEM again):
         * that one goes and this one is last, in its place */
        const Load *o = &s_loads[s_nloads - 1];
        if (!(o->tmem + o->words <= l->tmem || l->tmem + l->words <= o->tmem)) {
            for (i = 0; i < s_nloads - 1; i++) {
                const Load *e = &s_loads[i];
                if (!(e->tmem + e->words <= l->tmem || l->tmem + l->words <= e->tmem))
                    break;
            }
            if (i == s_nloads - 1) {
                s_loads[s_nloads - 1] = *l;
                return;
            }
        }
    }
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
static uint8_t s_tmem[4096] __attribute__((aligned(8)));

static void tmem_put64(int addr, const uint8_t *src, int odd)
{
    int k;
    if (!((uintptr_t)src & 3)) {                /* (two words, the halves swapped on an odd row) */
        uint32_t *t = (uint32_t *)&s_tmem[(addr * 8) & 0xFF8];
        uint32_t w0 = ((const uint32_t *)src)[0], w1 = ((const uint32_t *)src)[1];
        t[odd ? 1 : 0] = w0;
        t[odd ? 0 : 1] = w1;
        return;
    }
    for (k = 0; k < 8; k++)
        s_tmem[((addr * 8) + (k ^ (odd ? 4 : 0))) & 0xFFF] = src[k];
}

static int axis_map(int x, int mask, int mirror);

/* a row of a tile's texels as texel_raw reads them, the columns through
 * xmap (the mask's repeat or mirror, worked out once for the texture) */
static void tile_row(const Tile *tl, int y, int n, const uint16_t *xmap, uint32_t *out)
{
    int x, a, rb, sw;
    y = axis_map(y, tl->maskt, tl->cmt & 1);
    rb = tl->tmem * 8 + y * tl->line * 8;
    sw = (y & 1) ? 4 : 0;
    switch (tl->siz) {
    case 0:
        for (x = 0; x < n; x++) {
            int xm = xmap[x];
            uint8_t b = s_tmem[((rb + (xm >> 1)) ^ sw) & 0xFFF];
            out[x] = (xm & 1) ? b & 0xFu : (uint32_t)b >> 4;
        }
        break;
    case 1:
        for (x = 0; x < n; x++)
            out[x] = s_tmem[((rb + xmap[x]) ^ sw) & 0xFFF];
        break;
    case 2:
        for (x = 0; x < n; x++) {
            a = (rb + xmap[x] * 2) ^ sw;
            out[x] = (uint32_t)s_tmem[a & 0xFFF] << 8 | s_tmem[(a + 1) & 0xFFF];
        }
        break;
    default:
        sw = (y & 1) ? 8 : 0;
        for (x = 0; x < n; x++) {
            a = (rb + xmap[x] * 2) ^ sw;
            out[x] = (uint32_t)s_tmem[a & 0xFFF] << 24 | (uint32_t)s_tmem[(a + 1) & 0xFFF] << 16 |
                     (uint32_t)s_tmem[(a + 0x800) & 0xFFF] << 8 | s_tmem[(a + 0x801) & 0xFFF];
        }
        break;
    }
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

/* a texel coordinate through the tile's mask: repeated, or mirrored every
 * other period */
static int axis_map(int x, int mask, int mirror)
{
    int m, r;
    if (!mask || x < 0)
        return x;
    m = 1 << mask;
    r = x & (m - 1);
    if (mirror && ((x >> mask) & 1))
        r = m - 1 - r;
    return r;
}

/* a texel of tile t as the RDP reads it: the value, or for CI the index */
static uint32_t texel_raw(const Tile *tl, int x, int y)
{
    int line, base, swap, a;
    x = axis_map(x, tl->masks, tl->cms & 1);
    y = axis_map(y, tl->maskt, tl->cmt & 1);
    line = tl->line * 8; base = tl->tmem * 8; swap = (y & 1) ? 4 : 0;
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
static int s_opaque;                            /* the draw ignores texel alpha: every texel shows */
static int s_mask;                              /* the texture is only an alpha shape: white texels */

static uint16_t ps1_col(int r, int g, int b, int a)
{
    uint16_t c;
    if (s_opaque)
        a = 255;
    if (s_mask)
        r = g = b = 255;
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
    uint8_t wraps, wrapt;                       /* repeats across (down): placed aligned to its size */
    uint8_t up;                                 /* decoded at twice the size, filtered: texels are halves */
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

static int s_tex_full;                          /* a texture found no room: the next frame packs afresh */

static void tex_flush_all(void)
{
    int i;
    for (i = 0; i < s_ntex; i++)
        s_texs[i].key = 0;
    s_ntex = 0; s_texepoch++; s_epw[0]++;
    s_nshelf = 0;
    s_half_top[0] = s_half_top[1] = 0;
}

static int s_align_x = 1, s_align_y = 1;        /* the next allocation's alignment (halfwords, rows) */

static int shelf_fit(int i, int w)
{
    int x = s_shelf[i].x;
    if (s_shelf[i].y % s_align_y)
        return -1;
    x = (x + s_align_x - 1) / s_align_x * s_align_x;
    if (w <= 64 && (x & 63) + w > 64)           /* no straddling a page */
        x = (x + 63) & ~63;
    return x + w <= 384 ? x : -1;
}

/* room for w x h halfwords: its place in VRAM, and its shelf */
static int vram_alloc(int w, int h, int *px, int *py)
{
    int i, x, best = -1;
    uint32_t oldest = 0xFFFFFFFF;
    int hc = (h + 3) & ~3;                      /* shelves come in heights of 4 */
    for (i = 0; i < s_nshelf; i++)
        if (s_shelf[i].h >= h && s_shelf[i].h <= hc + (hc >> 1) + 4 && (x = shelf_fit(i, w)) >= 0)
            goto found;
    h = hc;
    for (i = 0; i < 2; i++)
        if (s_nshelf < NSHELF && (s_half_top[i] + s_align_y - 1) / s_align_y * s_align_y + h <= 256) {
            int k = s_nshelf++;
            s_half_top[i] = (s_half_top[i] + s_align_y - 1) / s_align_y * s_align_y;
            s_shelf[k].half = (int16_t)i;
            s_shelf[k].y = (int16_t)s_half_top[i];
            s_shelf[k].h = (int16_t)h;
            s_shelf[k].x = 0;
            s_half_top[i] += h;
            i = k;
            x = 0;
            goto found;
        }
    for (i = 0; i < s_nshelf; i++)              /* any shelf tall enough with room across */
        if (s_shelf[i].h >= h && (x = shelf_fit(i, w)) >= 0)
            goto found;
    /* evict: uploads land once the previous frame is drawn, so only what
     * this frame uses must stay; the stalest shelf tall enough */
    for (i = 0; i < s_nshelf; i++)
        if (s_shelf[i].h >= h && s_shelf[i].used != s_frame_no && s_shelf[i].used < oldest) {
            oldest = s_shelf[i].used;
            best = i;
        }
    if (best < 0) {
        /* none tall enough: a run of stacked shelves this frame does not use,
         * tall enough together, becomes one */
        int half;
        for (half = 0; half < 2; half++) {
            int y0 = -1, run = 0, yy = 0, j;
            while (yy < s_half_top[half]) {
                int sh = -1;
                for (j = 0; j < s_nshelf; j++)
                    if (s_shelf[j].half == half && s_shelf[j].y == yy)
                        sh = j;
                if (sh < 0)
                    break;
                if (s_shelf[sh].used == s_frame_no) {
                    y0 = -1;
                    run = 0;
                } else {
                    if (y0 < 0)
                        y0 = yy;
                    run += s_shelf[sh].h;
                }
                yy += s_shelf[sh].h;
                if (y0 >= 0 && run >= h)
                    break;
            }
            if (y0 >= 0 && run >= h) {
                int map[NSHELF], n = 0, k, keep = -1;
                for (j = 0; j < s_nshelf; j++) {
                    int in = s_shelf[j].half == half && s_shelf[j].y >= y0 && s_shelf[j].y < y0 + run;
                    if (in && s_shelf[j].y != y0) {
                        map[j] = -1;
                        continue;
                    }
                    map[j] = n;
                    s_shelf[n] = s_shelf[j];
                    if (in) {
                        keep = n;
                        s_shelf[n].h = (int16_t)run;
                        s_shelf[n].x = 0;
                    }
                    n++;
                }
                for (j = 0; j < s_ntex; j++) {
                    int sh;
                    if (!s_texs[j].key)
                        continue;
                    sh = map[s_texs[j].shelf];
                    if (sh < 0 || sh == keep)
                        s_texs[j].key = 0;      /* (a hole: the others stay where they are) */
                    else
                        s_texs[j].shelf = (uint8_t)sh;
                }
                (void)k;
                s_texepoch++; s_epw[1]++;
                s_nshelf = n;
                best = keep;
                break;
            }
        }
        if (best < 0)
            return -1;
    }
    {
        int j;
        for (j = 0; j < s_ntex; j++)
            if (s_texs[j].shelf == best)
                s_texs[j].key = 0;
        s_texepoch++; s_epw[2]++;
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

/* the intensity formats' palettes, shared: every IA4, I4, IA8 and I8 texel is
 * its own index (lossless); IA16 keeps 6 bits of intensity and 2 of alpha.
 * Row 480 holds IA4, I4 and IA8, row 481 I8 and IA16; rows 482 and 483 the
 * same opaque (for draws that ignore texel alpha), 484 and 485 white with
 * the alpha (a texture used only for its shape); the textures' own palettes
 * follow from row 486. */
enum { GP_IA4, GP_I4, GP_IA8, GP_I8, GP_IA16, GP_N };
static const uint16_t GREY_X[GP_N] = { 0, 16, 32, 0, 256 }, GREY_Y[GP_N] = { 480, 480, 480, 481, 481 };
static int s_grey_up;
static void upload(int x, int y, int w, int h, const uint16_t *px);
static uint16_t ps1_col(int r, int g, int b, int a);

static void grey_palettes(void)
{
    uint16_t pal[256];
    int i, k;
    int op, keep = s_opaque, keepm = s_mask;
    if (s_grey_up)
        return;
    s_grey_up = 1;
    for (op = 0; op < 3; op++)
    for (k = 0; k < GP_N; k++) {
        int n = k <= GP_I4 ? 16 : 256;
        for (i = 0; i < n; i++) {
            int in, al;
            switch (k) {
            case GP_IA4: in = (i >> 1) * 255 / 7; al = (i & 1) ? 255 : 0; break;
            case GP_I4: in = al = i * 17; break;
            case GP_IA8: in = (i >> 4) * 17; al = (i & 15) * 17; break;
            case GP_I8: in = al = i; break;
            default: in = (i & 63) * 255 / 63; al = (i >> 6) * 85; break;
            }
            s_opaque = op == 1;
            s_mask = op == 2;
            pal[i] = ps1_col(in, in, in, al);
        }
        upload(GREY_X[k], GREY_Y[k] + 2 * op, n, 1, pal);
    }
    s_opaque = keep;
    s_mask = keepm;
}
static uint16_t grey_clut(int k)
{
    return (uint16_t)((GREY_Y[k] + (s_mask ? 4 : 2 * s_opaque)) << 6 | GREY_X[k] >> 4);
}

static int clut_alloc(int n, int *cx, int *cy)
{
    if (s_clut_x + n > 320) {
        s_clut_x = 0;
        s_clut_y++;
        if (s_clut_y >= 26) {
            s_clut_y = 0;                       /* wrap: the oldest palettes go (TODO: tracked reuse) */
        }
    }
    *cx = s_clut_x;
    *cy = CLUT_Y + 6 + s_clut_y;
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
static uint32_t src_check_now(const Load *l);

/* (once a frame for a given source, a texture drawn many times read once,
 * unless four words of it say the game rewrote it between two draws) */
static struct { uint32_t src, n, frame, h, sig; } s_chk[64];

static uint32_t src_sig(const Load *l)
{
    uint32_t n = (uint32_t)l->words * 8;
    if (n < 16)
        return rd32(l->src);
    return rd32(l->src) ^ rd32(l->src + (n / 3 & ~3u)) * 3 ^ rd32(l->src + (2 * n / 3 & ~3u)) * 5 ^ rd32(l->src + n - 4) * 7;
}

static uint32_t src_check(const Load *l)
{
    uint32_t k = ((l->src >> 3) ^ (l->src >> 11)) & 63, n = (uint32_t)(l->kind << 28 | l->words << 12 | l->a0);
    uint32_t sig = src_sig(l);
    if (s_chk[k].src == l->src && s_chk[k].n == n && s_chk[k].frame == s_frame_no && s_chk[k].sig == sig)
        return s_chk[k].h;
    s_chk[k].src = l->src;
    s_chk[k].n = n;
    s_chk[k].frame = s_frame_no;
    s_chk[k].sig = sig;
    s_chk[k].h = src_check_now(l);
    return s_chk[k].h;
}

static uint32_t src_check_now(const Load *l)
{
    uint32_t h = 0x811C9DC5u, n = (uint32_t)l->words * 8, step, i;
    if (l->kind == 3)
        n = (uint32_t)l->a0 * 2;
    if (l->kind == 2)
        n = (uint32_t)((l->a3 >> 2) - (l->a1 >> 2) + 1) * (uint32_t)l->w * (l->siz ? (1u << l->siz) / 2 : 1);
    step = n > 256 ? (n / 32) & ~3u : 4;        /* (a small one whole: the HUD's digits change a few words) */
    for (i = 0; i + 4 <= n; i += step)
        h = hash32(h, rd32(l->src + i));
    return h;
}

/* an upload goes to the GPU at once: the previous frame's list is drawn by
 * then (gpu_load waits for it) and this frame's is not sent yet, so it lands
 * after every primitive that used the old texels and before every one that
 * uses the new */
static void upload(int x, int y, int w, int h, const uint16_t *px)
{
    if (s_log)
        tgr_log("rcp: upload %d,%d %dx%d frame %u\n", x, y, w, h, (unsigned)tgr_frame());
    gpu_load(x, y, w, h, px);
}

static int tile_size(const Tile *tl, int *w, int *h)
{
    /* what the tile shows: one period of its mask (repeated, or mirrored
     * every other) when it repeats, or is clamped past more than one; else
     * the texels up to its lower right */
    int ww = (((tl->lrs - tl->uls) & 0xFFF) >> 2) + 1, hh = (((tl->lrt - tl->ult) & 0xFFF) >> 2) + 1;
    if (tl->masks && (((tl->cms & 2) && ww > (1 << tl->masks) << (tl->cms & 1)) || (!(tl->cms & 2) && ww >= (1 << tl->masks))))
        ww = (1 << tl->masks) << (tl->cms & 1);
    if (tl->maskt && (((tl->cmt & 2) && hh > (1 << tl->maskt) << (tl->cmt & 1)) || (!(tl->cmt & 2) && hh >= (1 << tl->maskt))))
        hh = (1 << tl->maskt) << (tl->cmt & 1);
    if (ww > 1024) ww = 1024;
    if (hh > 256) hh = 256;
    *w = ww;
    *h = hh;
    return ww > 0 && hh > 0;
}

static uint16_t s_dec[384 * 256] __attribute__((aligned(8)));
static uint32_t s_tq[4], s_decx, s_decq[4];                        /* TGR_PERF: texture lookups' key, table search (cycles / 8), decodes; lookups */
static uint16_t s_xmap[1536];                   /* (a decode's columns through the tile's mask) */
static uint32_t s_row[1536 + 4];                /* (a decode's row of texels) */

/* a tile's texture: its key (the tile, the loads under it, the palette
 * mode) and a sample of its source bytes; 0 for no texture */
static uint32_t tile_key(int t, uint32_t *pcheck, int *pw, int *ph)
{
    const Tile *tl = &s_tile[t];
    int w, h, i, ci, tlut = (s_omh >> 14) & 3;
    uint32_t key = 0x811C9DC5u, check = 0;
    if (!tile_size(tl, &w, &h))
        return 0;
    if (w > 384 * 4)
        w = 384 * 4;
    key = hash32(key, (uint32_t)(tl->fmt << 28 | tl->siz << 26 | tl->line << 16 | tl->tmem << 4 | tl->pal));
    key = hash32(key, (uint32_t)(w << 16 | h | s_opaque << 15 | s_mask << 14));
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
    *pcheck = check;
    *pw = w;
    *ph = h;
    return key ? key : 1;
}

/* a band of a tile's rows decoded as a texture of its own: a tall 2D image
 * (rect_tex) in shelves of at most BAND rows, which pack where a whole one
 * would find no room */
#define BAND 32
static int s_band_y0, s_band_h;
static int s_band_y0_of(int band, int b0) { return band ? b0 : 0; }

static const Tex *tile_texture_find(int t);

/* the texture tile t draws, cached until the tiles, loads or modes change;
 * the source's bytes checked for the game's writes once a frame */
static int s_in_rect;
static int s_upsample = -1;                     /* TGR_UPSAMPLE=1: on (it costs VRAM: off) */
static struct { uint32_t gen, epoch, frame; int t, opaque, mask, by0, bh, rect; const Tex *tex; } s_tcache[4];
static int s_tcache_next;

static const Tex *tile_texture_(int t);
static const Tex *tile_texture(int t)
{
    return tile_texture_(t);
}

static const Tex *tile_texture_(int t)
{
    int i;
    const Tex *tx;
    for (i = 0; i < 4; i++)
        if (s_tcache[i].gen == s_texgen && s_tcache[i].epoch == s_texepoch && s_tcache[i].frame == s_frame_no &&
            s_tcache[i].t == t && s_tcache[i].opaque == s_opaque && s_tcache[i].mask == s_mask &&
            s_tcache[i].by0 == s_band_y0 && s_tcache[i].bh == s_band_h && s_tcache[i].rect == s_in_rect) {
            ((Tex *)s_tcache[i].tex)->used = s_frame_no;
            return s_tcache[i].tex;
        }
    {
        uint32_t q = s_tq[1];
        RG_BEGIN(RG_TEX);
        uint32_t rg_t_RG_DECODE = rg_t_RG_TEX;
        tx = tile_texture_find(t);
        if (s_tq[1] == q) {                     /* (not found in the table: decoded, or none) */
            RG_END(RG_DECODE);
        }
        RG_END(RG_TEX);
    }
    i = s_tcache_next++ & 3;
    s_tcache[i].gen = s_texgen;
    s_tcache[i].epoch = s_texepoch;
    s_tcache[i].frame = s_frame_no;
    s_tcache[i].t = t;
    s_tcache[i].opaque = s_opaque;
    s_tcache[i].mask = s_mask;
    s_tcache[i].by0 = s_band_y0;
    s_tcache[i].bh = s_band_h;
    s_tcache[i].rect = s_in_rect;
    s_tcache[i].tex = tx;
    return tx;
}

/* frames of animated textures, decoded: put back in place without decoding */
#define DC_N 8
#define DC_MAX 4096
static struct { uint32_t key, check, used; uint16_t hw, h, n, alpha; uint16_t pal[256]; uint16_t px[DC_MAX]; } s_dc[DC_N];
static uint16_t s_last_pal[256];
static int s_last_pal_n;

/* the table entry with a key: where it was last found, else a search */
static uint16_t s_tfi[256];
static int tex_index(uint32_t key)
{
    int h = (int)((key * 2654435761u) >> 24), i = s_tfi[h];
    if (i < s_ntex && s_texs[i].key == key)
        return i;
    for (i = 0; i < s_ntex; i++)
        if (s_texs[i].key == key) {
            s_tfi[h] = (uint16_t)i;
            return i;
        }
    return -1;
}

/* rows of 4- or 8-bit texels into s_dec as words (the tile's columns as they are) */
static void tex_rows_words(const Tile *tl, int by0, int w, int h, int hw)
{
    int y, i, nw = tl->siz ? w / 4 : w / 8, line8 = tl->line * 8, base8 = tl->tmem * 8;
    for (y = 0; y < h; y++) {
        int ym = axis_map(y + by0, tl->maskt, tl->cmt & 1), rb = base8 + ym * line8, sw = (ym & 1) ? 4 : 0;
        uint32_t *o = (uint32_t *)&s_dec[y * hw];
        if (tl->siz)
            for (i = 0; i < nw; i++)
                o[i] = *(const uint32_t *)&s_tmem[((rb + i * 4) ^ sw) & 0xFFC];
        else
            for (i = 0; i < nw; i++) {
                uint32_t v = *(const uint32_t *)&s_tmem[((rb + i * 4) ^ sw) & 0xFFC];
                o[i] = ((v >> 4) & 0x0F0F0F0Fu) | ((v << 4) & 0xF0F0F0F0u);
            }
    }
}

static const Tex *tile_texture_find(int t)
{
    const Tile *tl = &s_tile[t];
    int w, h, i, x, y, mode, ci, grey, tlut = (s_omh >> 14) & 3;
    uint32_t key, check;
    int px, py, pg, hw;
    Tex *tx, old = { 0 };
    int reuse = 0, ri = -1, wrap_s = 0, wrap_t = 0, up, inplace = 0, xid = 0;
    int by0 = s_band_y0;
    uint32_t tq0 = ST_NOW();
    s_tq[3]++;
    if (!(key = tile_key(t, &check, &w, &h)))
        return NULL;
    s_tq[0] += (ST_NOW() - tq0) & 0xFFFF;
    tq0 = ST_NOW();
    key = hash32(key, (uint32_t)s_in_rect);
    if (!key)
        key = 1;                                /* (0: an empty entry) */
    if (s_band_h) {                             /* a band of rows only (rect_tex) */
        key = hash32(key, (uint32_t)(s_band_y0 << 16 | s_band_h));
        h = h - by0 < s_band_h ? h - by0 : s_band_h;
        if (h <= 0)
            return NULL;
    } else {
        by0 = 0;
    }
    ci = tl->fmt == 2 || (tl->fmt == 0 && tl->siz < 2);
    i = tex_index(key);
    if (i >= 0) {
        if (s_texs[i].check == check) {
            s_texs[i].used = s_frame_no;
            s_shelf[s_texs[i].shelf].used = s_frame_no;
            s_tq[1] += (ST_NOW() - tq0) & 0xFFFF;
            return &s_texs[i];
        }
        old = s_texs[i];                        /* changed (the game wrote it): decode again, in its place */
        reuse = 1;
        ri = i;
    }
    /* a miss: decode (from TMEM rebuilt from the loads), or a frame of an
     * animated texture decoded before */
    {
        int f = tl->fmt << 4 | tl->siz;
        grey = f == 0x30 ? GP_IA4 : f == 0x40 ? GP_I4 : f == 0x31 ? GP_IA8 : f == 0x41 ? GP_I8 :
               f == 0x32 ? GP_IA16 : -1;
    }
    mode = ci || grey == GP_IA4 || grey == GP_I4 ? (ci && tl->siz != 0 ? 1 : 0) : grey >= 0 ? 1 : 2;
    hw = mode == 0 ? (w + 3) / 4 : mode == 1 ? (w + 1) / 2 : w;
    if (hw > 384) {
        w = (384 * (mode == 0 ? 4 : mode == 1 ? 2 : 1));
        hw = 384;
    }
    /* a small 15-bit texture drawn on triangles: at twice the size, filtered,
     * where the GPU magnifies it without filtering (the road close up) */
    if (s_upsample < 0)
        s_upsample = getenv("TGR_UPSAMPLE") != NULL;
    up = s_upsample && mode == 2 && !s_band_h && !s_in_rect && w <= 128 && h <= 128 && w * h <= 4096;
    if (reuse && !(old.used != s_frame_no && old.w == w << up && old.h == h << up && old.mode == mode)) {
        s_texs[ri].key = 0; s_texepoch++; s_epw[3]++;   /* not in its place: it goes */
        reuse = 0;
        ri = -1;
    }
    if (!reuse) {                               /* its entry: a hole, the end, or the stalest */
        for (ri = 0; ri < s_ntex && s_texs[ri].key; ri++)
            ;
        if (ri == NTEX) {
            uint32_t oldest = 0xFFFFFFFF;
            for (i = 0; i < s_ntex; i++)
                if (s_texs[i].used < oldest) { oldest = s_texs[i].used; ri = i; }
            s_texs[ri].key = 0; s_texepoch++; s_epw[4]++;
        }
    }
    {
        /* a power-of-two size that the tile repeats: the GPU's texture window
         * repeats it if it sits aligned to its size within the page */
        int ws = !s_band_h && tl->masks && w == (1 << tl->masks) << (tl->cms & 1) && w >= 8 && w <= 256 &&
                 (!(tl->cms & 2) || (((tl->lrs - tl->uls) & 0xFFF) >> 2) + 1 > w);
        int wt = !s_band_h && tl->maskt && h == (1 << tl->maskt) << (tl->cmt & 1) && h >= 8 && h <= 256 &&
                 (!(tl->cmt & 2) || (((tl->lrt - tl->ult) & 0xFFF) >> 2) + 1 > h);
        int k = mode == 0 ? 4 : mode == 1 ? 2 : 1;
        s_align_x = ws ? ((w << up) / k < 1 ? 1 : (w << up) / k) : 1;
        s_align_y = wt ? h << up : 1;
        if (ws && (w << up) / k > 64) { ws = 0; s_align_x = 1; }
        wrap_s = ws;
        wrap_t = wt;
    }
    if (reuse && old.used != s_frame_no && old.w == w << up && old.h == h << up && old.mode == mode) {
        px = old.vx;
        py = old.vy;
        pg = old.shelf;
        s_shelf[pg].used = s_frame_no;
        inplace = 1;
        for (i = 0; i < DC_N; i++)
            if (s_dc[i].key == key && s_dc[i].check == check && s_dc[i].hw == (hw << up) && s_dc[i].h == (h << up)) {
                /* this frame of it decoded before: up again, in its place */
                s_dc[i].used = s_frame_no;
                upload(px, py, s_dc[i].hw, s_dc[i].h, s_dc[i].px);
                if (s_dc[i].n)
                    upload((old.clut & 63) << 4, old.clut >> 6, s_dc[i].n, 1, s_dc[i].pal);
                tx = &s_texs[ri];
                *tx = old;
                tx->key = key;
                tx->check = check;
                tx->used = s_frame_no;
                tx->has_alpha = (uint8_t)s_dc[i].alpha;
                return tx;
            }
    } else if ((pg = vram_alloc(hw << up, h << up, &px, &py)) < 0) {
        s_align_x = s_align_y = 1;
        s_tex_full = 1;
        if (s_log)
        {
            int j;
            tgr_log("rcp: no room for %dx%d (%d halfwords wide), %d shelves\n", w, h, hw, s_nshelf);
            for (j = 0; j < s_nshelf; j++)
                tgr_log("rcp:   shelf %d half %d y %d h %d x %d used %u (now %u)\n", j, s_shelf[j].half, s_shelf[j].y,
                        s_shelf[j].h, s_shelf[j].x, (unsigned)s_shelf[j].used, (unsigned)s_frame_no);
        }
        return NULL;
    }
    tx = &s_texs[ri];                           /* (entries never move: kept display lists hold them) */
    if (ri == s_ntex)
        s_ntex++;
    memset(tx, 0, sizeof *tx);
    tx->key = key;
    tx->check = check;
    tx->shelf = (uint8_t)pg;
    tx->mode = (uint8_t)mode;
    tx->w = (uint16_t)(w << up);
    tx->h = (uint16_t)(h << up);
    tx->up = (uint8_t)up;
    tx->used = s_frame_no;
    tx->vx = (uint16_t)px;
    tx->vy = (uint16_t)py;
    tx->wraps = (uint8_t)wrap_s;
    tx->wrapt = (uint8_t)wrap_t;
    s_align_x = s_align_y = 1;
    if (s_log) {                                /* (a check: no two live textures share VRAM) */
        int j;
        for (j = 0; j < s_ntex; j++) {
            const Tex *o = &s_texs[j];
            if (!o->key || o == tx)
                continue;
            int ow = o->mode == 0 ? (o->w + 3) / 4 : o->mode == 1 ? (o->w + 1) / 2 : o->w;
            if (px < o->vx + ow && o->vx < px + hw && py < o->vy + o->h && o->vy < py + h)
                tgr_log("rcp: OVERLAP new %d,%d %dx%d shelf %d with %d,%d %dx%d shelf %d\n", px, py, hw, h, pg,
                        o->vx, o->vy, ow, o->h, o->shelf);
        }
    }
    if (s_log)
        tgr_log("rcp: decode f%d s%d %dx%d at %d,%d mode %d tlut %d opaque %d mask %d clut %04X frame %u src %06X tmem %d line %d wrap %d%d cm %d%d mask %d%d uls %d lrs %d\n", tl->fmt, tl->siz, w, h, px, py, mode, tlut, s_opaque, s_mask, tx->clut, (unsigned)tgr_frame(), s_nloads ? (unsigned)s_loads[s_nloads - 1].src : 0u, tl->tmem, tl->line, wrap_s, wrap_t, tl->cms, tl->cmt, tl->masks, tl->maskt, tl->uls, tl->lrs);
    s_tq[2]++;
    s_decx += (uint32_t)(w * h);
    if (s_listlog > 0 && tgr_frame() >= (uint32_t)s_listlog && tgr_frame() < (uint32_t)s_listlog + 60)
        fprintf(stderr, "decode %u f%d s%d %dx%d mode %d grey %d ci %d reuse %d inplace %d up %d src %06X\n", (unsigned)tgr_frame(), tl->fmt,
                tl->siz, w, h, mode, grey, ci, reuse, inplace, up, s_nloads ? (unsigned)s_loads[s_nloads - 1].src : 0u);
    {
        uint32_t q = ST_NOW();
        for (i = 0; i < s_nloads; i++)
            replay(&s_loads[i]);
        s_decq[0] += (ST_NOW() - q) & 0xFFFF;
        s_decq[3] = ST_NOW();
    }
    {
        int n = hw * (mode == 0 ? 4 : mode == 1 ? 2 : 1);
        if (n > 1536)
            n = 1536;
        xid = 1;
        for (x = 0; x < n; x++) {
            s_xmap[x] = (uint16_t)axis_map(x, tl->masks, tl->cms & 1);
            if (s_xmap[x] != x)
                xid = 0;
        }
    }
    s_last_pal_n = 0;
    if (grey >= 0 && grey != GP_IA16 && xid && !(w & 7) && tl->siz <= 1) {
        /* 4 or 8 bits, the columns as they are: whole words of TMEM, each
         * row's halves of 8 bytes swapped on odd rows as TMEM keeps them; 4
         * bits with the two texels of a byte the other way round */
        grey_palettes();
        tx->clut = grey_clut(grey);
        tx->has_alpha = grey != GP_I4 && grey != GP_I8;
        tex_rows_words(tl, by0, w, h, hw);
    } else if (grey >= 0) {
        grey_palettes();
        tx->clut = grey_clut(grey);
        tx->has_alpha = grey != GP_I4 && grey != GP_I8;
        for (y = 0; y < h; y++) {
            tile_row(tl, y + by0, w, s_xmap, s_row);
            for (x = 0; x < hw; x++) {
                uint16_t v = 0;
                int k, per = mode == 0 ? 4 : 2;
                for (k = 0; k < per; k++) {
                    uint32_t t;
                    if (x * per + k >= w)
                        break;
                    t = s_row[x * per + k];
                    if (grey == GP_IA16)
                        t = (t >> 10 & 63) | (t & 255) * 3 / 255 << 6;
                    v |= (uint16_t)(t << ((16 / per) * k));
                }
                s_dec[y * hw + x] = v;
            }
        }
    } else if (ci) {
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
        if (reuse && old.used != s_frame_no && old.clut && old.mode == mode) {
            cx = (old.clut & 63) << 4;
            cy = old.clut >> 6;
        } else {
            clut_alloc(n, &cx, &cy);
        }
        upload(cx, cy, n, 1, pal);
        memcpy(s_last_pal, pal, (size_t)n * 2);
        s_last_pal_n = n;
        if (s_log)
            tgr_log("rcp: clut %d,%d for %dx%d frame %u\n", cx, cy, w, h, (unsigned)tgr_frame());
        tx->clut = (uint16_t)(cy << 6 | cx >> 4);
        if (xid && !(w & 7) && tl->siz <= 1)
            tex_rows_words(tl, by0, w, h, hw);
        else
        for (y = 0; y < h; y++) {
            uint16_t *o = &s_dec[y * hw];
            tile_row(tl, y + by0, w, s_xmap, s_row);
            s_row[w] = s_row[w + 1] = s_row[w + 2] = 0;
            if (mode == 0)
                for (x = 0; x < hw; x++)
                    o[x] = (uint16_t)(s_row[x * 4] | s_row[x * 4 + 1] << 4 | s_row[x * 4 + 2] << 8 | s_row[x * 4 + 3] << 12);
            else
                for (x = 0; x < hw; x++)
                    o[x] = (uint16_t)(s_row[x * 2] | s_row[x * 2 + 1] << 8);
        }
    } else if (tl->fmt == 0 && tl->siz == 2) {
        /* 16-bit RGBA, the common one: read and made in one pass (ps1_col of
         * n64_5551, the alpha a bit) */
        int op = s_opaque, mk = s_mask, ha = 0, line8 = tl->line * 8, base8 = tl->tmem * 8;
        for (y = 0; y < h; y++) {
            int ym = axis_map(y + by0, tl->maskt, tl->cmt & 1), rb = base8 + ym * line8, sw = (ym & 1) ? 4 : 0;
            uint16_t *o = &s_dec[y * hw];
            for (x = 0; x < w; x++) {
                int a = (rb + s_xmap[x] * 2) ^ sw;
                uint32_t c = (uint32_t)s_tmem[a & 0xFFF] << 8 | s_tmem[(a + 1) & 0xFFF];
                uint32_t q;
                if (!((c & 1) | op)) {
                    q = 0;
                    ha = 1;
                } else {
                    q = mk ? 0x7FFF : ((c >> 11) & 31) | ((c >> 6) & 31) << 5 | ((c >> 1) & 31) << 10;
                    if (!q) {
                        q = 0x8000;
                        ha = 1;
                    }
                }
                o[x] = (uint16_t)q;
            }
        }
        if (ha)
            tx->has_alpha = 1;
    } else {
        int f = tl->fmt << 4 | tl->siz, ha = 0;
        for (y = 0; y < h; y++) {
            tile_row(tl, y + by0, w, s_xmap, s_row);
            for (x = 0; x < w; x++) {
                uint32_t v = s_row[x];
                uint16_t c;
                switch (f) {
                case 0x02: c = n64_5551((uint16_t)v); break;
                case 0x03: c = ps1_col(v >> 24, (v >> 16) & 255, (v >> 8) & 255, v & 255); break;
                case 0x30: { int i8 = (int)(v >> 1) * 255 / 7; c = ps1_col(i8, i8, i8, (v & 1) ? 255 : 0); break; }
                case 0x31: c = ps1_col((int)(v >> 4) * 17, (int)(v >> 4) * 17, (int)(v >> 4) * 17, (int)(v & 15) * 17); break;
                case 0x32: c = ps1_col((int)(v >> 8), (int)(v >> 8), (int)(v >> 8), (int)(v & 255)); break;
                case 0x40: c = ps1_col((int)v * 17, (int)v * 17, (int)v * 17, (int)v * 17); break;
                default:   c = ps1_col((int)v, (int)v, (int)v, (int)v); break;
                }
                if (!c || (c & 0x8000))
                    ha = 1;
                s_dec[y * hw + x] = c;
            }
        }
        if (ha)
            tx->has_alpha = 1;
    }
    if (up) {
        /* each texel and the halfway points to its right and below it (the next
         * texel round the repeat, or the edge's again): the GPU's nearest
         * texel of these is the bilinear one at the half texel; a texture with
         * transparent texels keeps them whole */
        static uint16_t big[256 * 256];
        int x1, y1, w2 = w * 2;
        for (y = 0; y < h; y++) {
            y1 = y + 1 < h ? y + 1 : wrap_t ? 0 : y;
            for (x = 0; x < w; x++) {
                uint16_t a = s_dec[y * hw + x], b, c, d;
                x1 = x + 1 < w ? x + 1 : wrap_s ? 0 : x;
                b = s_dec[y * hw + x1]; c = s_dec[y1 * hw + x]; d = s_dec[y1 * hw + x1];
                big[(2 * y) * w2 + 2 * x] = a;
                if (tx->has_alpha) {
                    big[(2 * y) * w2 + 2 * x + 1] = a;
                    big[(2 * y + 1) * w2 + 2 * x] = a;
                    big[(2 * y + 1) * w2 + 2 * x + 1] = a;
                } else {
#define AVG2(p, q) (uint16_t)(((((p) & 0x7BDE) + ((q) & 0x7BDE)) >> 1) | ((p) & 0x8000))
                    big[(2 * y) * w2 + 2 * x + 1] = AVG2(a, b);
                    big[(2 * y + 1) * w2 + 2 * x] = AVG2(a, c);
                    big[(2 * y + 1) * w2 + 2 * x + 1] = AVG2(AVG2(a, d), AVG2(b, c));
#undef AVG2
                }
            }
        }
        upload(px, py, w2, h * 2, big);
        return tx;
    }
    s_decq[1] += (ST_NOW() - s_decq[3]) & 0xFFFF;
    {
        uint32_t q = ST_NOW();
        upload(px, py, hw, h, s_dec);
        s_decq[2] += (ST_NOW() - q) & 0xFFFF;
    }
    if (inplace && hw * h <= DC_MAX) {
        /* a texture whose bytes the game rewrites (an animation): this frame kept */
        int k = 0;
        uint32_t oldest = 0xFFFFFFFF;
        for (i = 0; i < DC_N; i++)
            if (s_dc[i].used < oldest) {
                oldest = s_dc[i].used;
                k = i;
            }
        s_dc[k].key = key;
        s_dc[k].check = check;
        s_dc[k].used = s_frame_no;
        s_dc[k].hw = (uint16_t)hw;
        s_dc[k].h = (uint16_t)h;
        s_dc[k].n = (uint16_t)s_last_pal_n;
        s_dc[k].alpha = tx->has_alpha;
        memcpy(s_dc[k].px, s_dec, (size_t)(hw * h) * 2);
        memcpy(s_dc[k].pal, s_last_pal, (size_t)s_last_pal_n * 2);
    }
    return tx;
}

/* ---- the combiner, reduced ------------------------------------------------------- *
 * Each input is "a * texel + b" per channel (8.8 fixed); (A - B) * C + D of
 * those is again of that form when C is not the texel.  The result per
 * vertex: textured, the GPU modulating the texel by a + b (the combiner's
 * value at texel 1: exact for a texel times a colour, near for a lerp by
 * the texel), or untextured with colour b. */
/* TX0/TX1: the combiner's TEXEL0 and TEXEL1.  In a cycle one of them is the
 * texture drawn (symbolic, a); the other is the second texture sampled at
 * the vertex (a number, b): cycle 1 draws TEXEL0, cycle 2 TEXEL1 (the RDP's
 * second cycle sees the texels swapped, so either way the tile drawn) */
enum { IN_COMB, IN_TX0, IN_TX1, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO, IN_TX0A, IN_TX1A, IN_PRIMA,
       IN_SHADEA, IN_ENVA, IN_COMBA, IN_PRIMLOD };
static const int8_t CA[16] = { IN_COMB, IN_TX0, IN_TX1, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO };
static const int8_t CB[16] = { IN_COMB, IN_TX0, IN_TX1, IN_PRIM, IN_SHADE, IN_ENV, IN_ZERO, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO };
/* (the LOD fraction is 0: the finest level of a mipmapped texture is the one drawn) */
static const int8_t CC[32] = { IN_COMB, IN_TX0, IN_TX1, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_COMBA,
                               IN_TX0A, IN_TX1A, IN_PRIMA, IN_SHADEA, IN_ENVA, IN_ZERO, IN_PRIMLOD, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO,
                               IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO, IN_ZERO };
static const int8_t CD[8] = { IN_COMB, IN_TX0, IN_TX1, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO };
static const int8_t AABD[8] = { IN_COMB, IN_TX0, IN_TX1, IN_PRIM, IN_SHADE, IN_ENV, IN_ONE, IN_ZERO };
static const int8_t AC[8] = { IN_ZERO, IN_TX0, IN_TX1, IN_PRIM, IN_SHADE, IN_ENV, IN_PRIMLOD, IN_ZERO };

static const uint8_t WHITE[4] = { 255, 255, 255, 255 };

static int to256(int v) { return v + (v >> 7); }          /* 0..255 to 0..256 */

/* with texture LOD on and no further level (G_TEXTURE's level 0) the second
 * texel is the same tile's: both are the texture drawn */
static int same_tile(void) { return ((s_omh >> 16) & 1) && s_tex.level == 0; }

/* The combiner compiled for its state (the combine mode, the cycle type, the
 * colours): each cycle's A, B, C, D per channel as a kind and a value, so a
 * vertex runs straight through.  Kinds: a constant (value), the texture
 * drawn, the second texture's or the shade's channel (value), the first
 * cycle's result's channel (value). */
enum { K_CONST, K_TEX, K_OTHER, K_SHADE, K_COMB };
typedef struct { int8_t kind[2][4][4]; int16_t val[2][4][4]; int cycles; } CCode;
static CCode s_cc;
static uint32_t s_cc_key[6] = { 0xFFFFFFFF };
#define CCC_N 8
static struct { uint32_t key[6], stamp; CCode cc; } s_ccc[CCC_N];   /* (the last compiled combiner states) */
static int s_ccc_next;
static uint32_t s_cc_stamps = 1;                /* (the last stamp given) */
static uint32_t s_cc_stamp = 1;                 /* bumped when the compiled combiner changes */

static void cc_slot(int cyc, int slot, int in, int sym, int alpha)
{
    int k;
    if (in == IN_TX0 || in == IN_TX1)
        in = (in == IN_TX1) == sym || same_tile() ? -1 : -2;
    else if (in == IN_TX0A || in == IN_TX1A)
        in = (in == IN_TX1A) == sym || same_tile() ? -3 : -4;
    for (k = 0; k < 4; k++) {
        int c = alpha ? 3 : k, kind = K_CONST, val = 0;
        switch (in) {
        case IN_COMB: kind = K_COMB; val = c; break;
        case IN_COMBA: kind = K_COMB; val = 3; break;
        case -1: case -3: kind = K_TEX; break;
        case -2: kind = K_OTHER; val = c; break;
        case -4: kind = K_OTHER; val = 3; break;
        case IN_PRIM: val = to256(s_prim[c]); break;
        case IN_PRIMA: val = to256(s_prim[3]); break;
        case IN_SHADE: kind = K_SHADE; val = c; break;
        case IN_SHADEA: kind = K_SHADE; val = 3; break;
        case IN_ENV: val = to256(s_env[c]); break;
        case IN_ENVA: val = to256(s_env[3]); break;
        case IN_ONE: val = 256; break;
        case IN_PRIMLOD: val = to256(s_prim_lod); break;
        default: break;
        }
        if (alpha && k != 3)
            continue;
        if (!alpha && k == 3)
            continue;
        s_cc.kind[cyc][slot][k] = (int8_t)kind;
        s_cc.val[cyc][slot][k] = (int16_t)val;
    }
}

static uint32_t s_cc_dstamp;

static void cc_compile(void)
{
    uint32_t key[6];
    if (s_cc_dstamp == s_dstamp)
        return;                                 /* nothing that could change it was drawn since */
    s_cc_dstamp = s_dstamp;
    key[0] = s_cc0;
    key[1] = s_cc1;
    key[2] = (s_omh & 0x00310000) | (uint32_t)same_tile();
    key[3] = (uint32_t)s_prim[0] << 24 | s_prim[1] << 16 | s_prim[2] << 8 | s_prim[3];
    key[4] = (uint32_t)s_env[0] << 24 | s_env[1] << 16 | s_env[2] << 8 | s_env[3];
    key[5] = s_prim_lod;
    if (!memcmp(key, s_cc_key, sizeof key))
        return;
    memcpy(s_cc_key, key, sizeof key);
    {
        /* one of the last few combiner states again: as it was compiled, under
         * its stamp (colours combined under it still hold) */
        int i;
        for (i = 0; i < CCC_N; i++)
            if (s_ccc[i].stamp && !memcmp(s_ccc[i].key, key, sizeof key)) {
                s_cc = s_ccc[i].cc;
                s_cc_stamp = s_ccc[i].stamp;
                return;
            }
    }
    s_cc_stamp = ++s_cc_stamps;
    s_cc.cycles = ((s_omh >> 20) & 3) == 1 ? 2 : 1;
    cc_slot(0, 0, CA[(s_cc0 >> 20) & 15], 0, 0);
    cc_slot(0, 1, CB[(s_cc1 >> 28) & 15], 0, 0);
    cc_slot(0, 2, CC[(s_cc0 >> 15) & 31], 0, 0);
    cc_slot(0, 3, CD[(s_cc1 >> 15) & 7], 0, 0);
    cc_slot(0, 0, AABD[(s_cc0 >> 12) & 7], 0, 1);
    cc_slot(0, 1, AABD[(s_cc1 >> 12) & 7], 0, 1);
    cc_slot(0, 2, AC[(s_cc0 >> 9) & 7], 0, 1);
    cc_slot(0, 3, AABD[(s_cc1 >> 9) & 7], 0, 1);
    cc_slot(1, 0, CA[(s_cc0 >> 5) & 15], 1, 0);
    cc_slot(1, 1, CB[(s_cc1 >> 24) & 15], 1, 0);
    cc_slot(1, 2, CC[s_cc0 & 31], 1, 0);
    cc_slot(1, 3, CD[(s_cc1 >> 6) & 7], 1, 0);
    cc_slot(1, 0, AABD[(s_cc1 >> 21) & 7], 1, 1);
    cc_slot(1, 1, AABD[(s_cc1 >> 3) & 7], 1, 1);
    cc_slot(1, 2, AC[(s_cc1 >> 18) & 7], 1, 1);
    cc_slot(1, 3, AABD[s_cc1 & 7], 1, 1);
    {
        int i = s_ccc_next++ % CCC_N;
        memcpy(s_ccc[i].key, key, sizeof key);
        s_ccc[i].cc = s_cc;
        s_ccc[i].stamp = s_cc_stamp;
    }
}

/* the combiner's results by shade, for a combiner state, without a second
 * texture: vertices share colours */
static struct { uint32_t stamp, shade; Lin res; } s_cmemo[64];

/* the combiner for a vertex: shade its colour, other the second texture there */
static void combine(Lin *out, const uint8_t shade[4], const uint8_t *other)
{
    int c, k, sl;
    Lin res = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
    uint32_t sk = 0, h = 0;
    if (!other) {
        sk = (uint32_t)shade[0] | (uint32_t)shade[1] << 8 | (uint32_t)shade[2] << 16 | (uint32_t)shade[3] << 24;
        h = (sk * 2654435761u) >> 26;
        if (s_cmemo[h].stamp == s_cc_stamp && s_cmemo[h].shade == sk) {
            *out = s_cmemo[h].res;
            return;
        }
        other = WHITE;
    }
    for (c = 0; c < s_cc.cycles; c++) {
        Lin prev = res;
        for (k = 0; k < 4; k++) {
            int32_t ia[4], ib[4], ca, cb, da, db;
            for (sl = 0; sl < 4; sl++) {
                int v = s_cc.val[c][sl][k];
                switch (s_cc.kind[c][sl][k]) {
                case K_CONST: ia[sl] = 0; ib[sl] = v; break;
                case K_TEX:   ia[sl] = 256; ib[sl] = 0; break;
                case K_OTHER: ia[sl] = 0; ib[sl] = to256(other[v]); break;
                case K_SHADE: ia[sl] = 0; ib[sl] = to256(shade[v]); break;
                default:      ia[sl] = prev.a[v]; ib[sl] = prev.b[v]; break;
                }
            }
            ca = ia[2]; cb = ib[2];
            da = ia[0] - ia[1]; db = ib[0] - ib[1];
            if (ca && !da) {                    /* (const) * texel */
                res.a[k] = (int16_t)((db * ca >> 8) + ia[3]);
                res.b[k] = (int16_t)ib[3];
            } else if (ca) {                    /* texel * texel: as one texel */
                res.a[k] = (int16_t)(((da * ca >> 8) + (db * ca >> 8)) + ia[3]);
                res.b[k] = (int16_t)ib[3];
            } else {
                res.a[k] = (int16_t)((da * cb >> 8) + ia[3]);
                res.b[k] = (int16_t)((db * cb >> 8) + ib[3]);
            }
        }
    }
    *out = res;
    if (other == WHITE && sk | h) {
        s_cmemo[h].stamp = s_cc_stamp;
        s_cmemo[h].shade = sk;
        s_cmemo[h].res = res;
    }
}

/* whether the combiner's result depends on the second texture (TEXEL1 in
 * cycle 1, TEXEL0 in cycle 2): the combiner run with it black and with it
 * white, for every shade; kept per combiner state */
static const uint8_t BLACK[4] = { 0, 0, 0, 0 };

static int uses_other(void)
{
    static uint32_t key_cc0 = 0xFFFFFFFF, key_cc1, key_omh;
    static int key_same, result;
    uint32_t omh = s_omh & 0x00310000;          /* the cycle type and LOD */
    int same = same_tile();
    if (key_cc0 == s_cc0 && key_cc1 == s_cc1 && key_omh == omh && key_same == same)
        return result;
    key_cc0 = s_cc0;
    key_cc1 = s_cc1;
    key_omh = omh;
    key_same = same;
    if (same) {
        result = 0;
    } else {
        Lin a, b;
        cc_compile();
        combine(&a, WHITE, BLACK);
        combine(&b, WHITE, WHITE);
        result = memcmp(&a, &b, sizeof a) != 0;
        if (!result) {
            combine(&a, BLACK, BLACK);
            combine(&b, BLACK, WHITE);
            result = memcmp(&a, &b, sizeof a) != 0;
        }
    }
    return result;
}

/* ---- the second texture, sampled on the CPU ---------------------------------------- *
 * A tile decoded to RGBA8 on a miss (TMEM rebuilt from the loads), kept by
 * the same key as the VRAM cache; up to 4096 texels (the ramps and masks the
 * game pairs with a texture), else read as white. */
#define NSAMP 16
static struct { uint32_t key, check; int w, h; uint8_t *rgba; uint32_t used; } s_samp[NSAMP];

static void texel_rgba(const Tile *tl, int x, int y, int tlut, uint8_t out[4])
{
    uint32_t v = texel_raw(tl, x, y);
    int f = tl->fmt << 4 | tl->siz;
    if (tl->fmt == 2 || (tl->fmt == 0 && tl->siz < 2)) {
        int idx = tl->siz == 0 ? (tl->pal << 4) | (int)v : (int)v;
        uint16_t c = (uint16_t)(tm8(0x800 + idx * 8) << 8 | tm8(0x800 + idx * 8 + 1));
        if (tlut == 3) {
            out[0] = out[1] = out[2] = (uint8_t)(c >> 8);
            out[3] = (uint8_t)c;
            return;
        }
        v = c;
        f = 0x02;
    }
    switch (f) {
    case 0x02:
        out[0] = (uint8_t)(((v >> 11) & 31) * 255 / 31); out[1] = (uint8_t)(((v >> 6) & 31) * 255 / 31);
        out[2] = (uint8_t)(((v >> 1) & 31) * 255 / 31); out[3] = (v & 1) ? 255 : 0;
        break;
    case 0x03: out[0] = (uint8_t)(v >> 24); out[1] = (uint8_t)(v >> 16); out[2] = (uint8_t)(v >> 8); out[3] = (uint8_t)v; break;
    case 0x30: out[0] = out[1] = out[2] = (uint8_t)((v >> 1) * 255 / 7); out[3] = (v & 1) ? 255 : 0; break;
    case 0x31: out[0] = out[1] = out[2] = (uint8_t)((v >> 4) * 17); out[3] = (uint8_t)((v & 15) * 17); break;
    case 0x32: out[0] = out[1] = out[2] = (uint8_t)(v >> 8); out[3] = (uint8_t)v; break;
    case 0x40: out[0] = out[1] = out[2] = out[3] = (uint8_t)(v * 17); break;
    default: out[0] = out[1] = out[2] = out[3] = (uint8_t)v; break;
    }
}


/* tile t's texel at the texture coordinate (s, t) (10.5, after G_TEXTURE's scale) */
static void sample(int t, int32_t ss, int32_t tt, uint8_t out[4])
{
    const Tile *tl = &s_tile[t];
    uint32_t check, key;
    int w, h, i, slot = -1, x, y;
    uint32_t oldest = 0xFFFFFFFF;
    static struct { uint32_t gen, frame, key, check; int t, op, mk, w, h; } km;   /* (the tile's key, kept while nothing it reads changes) */
    memcpy(out, WHITE, 4);
    if (km.gen == s_texgen && km.frame == s_frame_no && km.t == t && km.op == s_opaque && km.mk == s_mask) {
        key = km.key;
        check = km.check;
        w = km.w;
        h = km.h;
    } else {
        key = tile_key(t, &check, &w, &h);
        km.gen = s_texgen;
        km.frame = s_frame_no;
        km.t = t;
        km.op = s_opaque;
        km.mk = s_mask;
        km.key = key;
        km.check = check;
        km.w = w;
        km.h = h;
    }
    if (!key || w * h > 4096)
        return;
    for (i = 0; i < NSAMP; i++)
        if (s_samp[i].rgba && s_samp[i].key == key && s_samp[i].check == check) {
            slot = i;
            break;
        }
    if (slot < 0) {
        for (i = 0; i < NSAMP; i++)
            if (!s_samp[i].rgba || s_samp[i].used < oldest) {
                oldest = s_samp[i].rgba ? s_samp[i].used : 0;
                slot = i;
            }
        if (!s_samp[slot].rgba)
            s_samp[slot].rgba = (uint8_t *)malloc(4096 * 4);
        for (i = 0; i < s_nloads; i++)
            replay(&s_loads[i]);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                texel_rgba(tl, x, y, (s_omh >> 14) & 3, s_samp[slot].rgba + (y * w + x) * 4);
        s_samp[slot].key = key;
        s_samp[slot].check = check;
        s_samp[slot].w = w;
        s_samp[slot].h = h;
    }
    s_samp[slot].used = s_frame_no;
    {
        int32_t origin_s = (tl->uls > tl->lrs ? tl->uls - 4096 : tl->uls) << 3;
        int32_t origin_t = (tl->ult > tl->lrt ? tl->ult - 4096 : tl->ult) << 3;
        int32_t vs = tl->shifts > 10 ? ss << (16 - tl->shifts) : ss >> tl->shifts;
        int32_t vt = tl->shiftt > 10 ? tt << (16 - tl->shiftt) : tt >> tl->shiftt;
        x = (vs - origin_s) >> 5;
        y = (vt - origin_t) >> 5;
        if (tl->masks && w == (1 << tl->masks) << (tl->cms & 1)) x &= w - 1;
        if (tl->maskt && h == (1 << tl->maskt) << (tl->cmt & 1)) y &= h - 1;
        x = x < 0 ? 0 : x >= w ? w - 1 : x;
        y = y < 0 ? 0 : y >= h ? h - 1 : y;
        memcpy(out, s_samp[slot].rgba + (y * w + x) * 4, 4);
    }
}

/* ---- the primitive's draw state ------------------------------------------------------ */
typedef struct {
    const Tex *tex;
    int textured, semi, abr, zsort;
    int32_t s0, t0;                             /* the tile's origin, 10.5 */
    int shifts, shiftt;
} Draw;

static void draw_state(Draw *d, int tile, int want_tex, int mask)
{
    uint32_t bl = s_oml >> 16;
    int p, a, m, b;
    memset(d, 0, sizeof *d);
    d->textured = want_tex;
    p = (bl >> 14) & 3; a = (bl >> 10) & 3; m = (bl >> 6) & 3; b = (bl >> 2) & 3;
    if (((s_omh >> 20) & 3) == 1) {
        p = (bl >> 12) & 3; a = (bl >> 8) & 3; m = (bl >> 4) & 3; b = bl & 3;
    }
    /* texel alpha matters only to an alpha compare that can fail (a
     * threshold above 0, or dithered), to coverage times alpha, or to
     * blending by alpha: else every texel is drawn (the paint shop's canvas
     * has its alpha bits clear) */
    s_opaque = !(((s_oml & 3) == 1 && s_blend[3] > 0) || (s_oml & 3) == 3 || (s_oml & 0x1000) ||
                 (p == 0 && m == 1 && a != 3 && b == 0 && (s_oml & 0x4000)));
    s_mask = mask;
    if (want_tex) {
        d->tex = tile_texture(tile);
        if (!d->tex)
            d->textured = 0;
        d->s0 = (s_tile[tile].uls > s_tile[tile].lrs ? s_tile[tile].uls - 4096 : s_tile[tile].uls) << 3;
        d->t0 = (s_tile[tile].ult > s_tile[tile].lrt ? s_tile[tile].ult - 4096 : s_tile[tile].ult) << 3;
        d->shifts = s_tile[tile].shifts;
        d->shiftt = s_tile[tile].shiftt;
    }
    s_mask = 0;
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

/* blending by alpha (IN * A + MEM * (1 - A)) on the GPU: an alpha that comes
 * from the texture is the texels' STP bit (set on the decoded texels whose
 * alpha is between 1/4 and 3/4), which blends half and half on a
 * semi-transparent primitive and leaves the others opaque; a constant alpha
 * picks the nearest of the GPU's modes: opaque from 3/4, half and half from
 * 1/4, below that a bright colour adds a quarter and a dark one is not drawn
 * (a fade to black steps through these) */
static int blend_mode(const Lin *c, int textured, int *abr)
{
    int al;
    *abr = 0;
    if (textured && c->a[3])
        return 1;
    al = c->b[3] > 255 ? 255 : c->b[3] < 0 ? 0 : c->b[3];
    if (al >= 192)                              /* nearly opaque: opaque (a fade's last steps) */
        return 0;
    if (al >= 64)
        return 1;
    if (c->b[0] + c->b[1] + c->b[2] > 384) {    /* faint and bright: a quarter added */
        *abr = 3;
        return 1;
    }
    return -1;                                  /* faint and dark: nothing to see */
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
/* a texture coordinate (10.5, scaled by G_TEXTURE) to texels from the tile's origin, 1/32 texel */
static int32_t tc32(int32_t st, int32_t origin, int shift)
{
    int32_t v = st;
    if (shift > 10)
        v <<= 16 - shift;
    else
        v >>= shift;
    return v - origin;
}
static int clamp_u(int u, int w) { return u < 0 ? 0 : u > w ? w : u; }

/* ---- triangles: clipped in clip space, split to what the GPU draws -------------------- */
typedef struct { int32_t x, y, z, w, u, v, r, g, b, fr, fg, fb, mx, my, mz; } CV;  /* (and model space, 4 fraction bits) */   /* clip space; u, v 1/32 texel */
typedef struct { int16_t x, y, z; int32_t u, v; int16_t r, g, b, fr, fg, fb; } SV; /* screen */


static int32_t plane_d(const CV *c, int pl)
{
    switch (pl) {
    case 0: return c->w - W_NEAR;
    case 1: return GUARD * c->w - c->x;
    case 2: return GUARD * c->w + c->x;
    case 3: return GUARD * c->w - c->y;
    default: return GUARD * c->w + c->y;
    }
}

static void cv_lerp(CV *o, const CV *a, const CV *b, int32_t t)   /* t 16.16 */
{
#define L(f) o->f = a->f + (int32_t)(((int64_t)(b->f - a->f) * t) >> 16)
    L(x); L(y); L(z); L(w); L(u); L(v); L(r); L(g); L(b); L(fr); L(fg); L(fb); L(mx); L(my); L(mz);
#undef L
}

/* the middle of a and b (a halving needs no product) */
static void cv_mid(CV *o, const CV *a, const CV *b)
{
#define M(f) o->f = (a->f + b->f) >> 1
    M(x); M(y); M(z); M(w); M(u); M(v); M(r); M(g); M(b); M(fr); M(fg); M(fb); M(mx); M(my); M(mz);
#undef M
}

/* Sutherland-Hodgman against one plane: n vertices in, the count out */
static int clip_plane(const CV *in, int n, CV *out, int pl)
{
    int i, m = 0;
    for (i = 0; i < n; i++) {
        const CV *a = &in[i], *b = &in[(i + 1) % n];
        int32_t da = plane_d(a, pl), db = plane_d(b, pl);
        if (da >= 0)
            out[m++] = *a;
        if ((da >= 0) != (db >= 0)) {
            int32_t t = (int32_t)(((int64_t)da << 16) / ((int64_t)da - db));
            cv_lerp(&out[m++], a, b, t);
        }
    }
    return m;
}

/* what the primitive draws with, shared by its pieces */
typedef struct {
    const Tex *tex;
    int textured, semi, abr, cmd, zsort;
    int wraps, wrapt, ub, vb;                   /* wrapping (a texture window) and the texels' base in the page */
    uint32_t window;
    int fog;                                    /* a second pass adds the fog colour (fr, fg, fb) */
    int lit, flat;                              /* (kept lists) lit: the GPU colour is c0 + sl * shade / 256 per channel */
    int16_t c0[3], sl[3];
} Prim;

static int span_ok(const SV *a, const SV *b, const SV *c, const Prim *pr, int *split)
{
    int minx = a->x, maxx = a->x, miny = a->y, maxy = a->y, k;
    const SV *v[3] = { a, b, c };
    int32_t minu = a->u, maxu = a->u, minv = a->v, maxv = a->v;
    for (k = 1; k < 3; k++) {
        if (v[k]->x < minx) minx = v[k]->x;
        if (v[k]->x > maxx) maxx = v[k]->x;
        if (v[k]->y < miny) miny = v[k]->y;
        if (v[k]->y > maxy) maxy = v[k]->y;
        if (v[k]->u < minu) minu = v[k]->u;
        if (v[k]->u > maxu) maxu = v[k]->u;
        if (v[k]->v < minv) minv = v[k]->v;
        if (v[k]->v > maxv) maxv = v[k]->v;
    }
    *split = 0;
    if (maxx - minx > 1000 || maxy - miny > 500)
        *split = 1;
    else if (pr->textured && ((pr->wraps && maxu - minu > 192 * 32) || (pr->wrapt && maxv - minv > 192 * 32)))
        *split = 2;
    return !*split;
}

static void emit_tri(const SV *v0, const SV *v1, const SV *v2, const Prim *pr, int depth)
{
    s_n_piece++;
    const SV *v[3] = { v0, v1, v2 };
    uint32_t *p, *w;
    int k, split, words;
    if (!span_ok(v0, v1, v2, pr, &split) && depth < 7) {
        s_splitw[split]++;
        /* split the longest edge (on the screen, or in texels when the span is the trouble) */
        SV m;
        int e = 0, i;
        int32_t best = -1;
        for (i = 0; i < 3; i++) {
            const SV *a = v[i], *b = v[(i + 1) % 3];
            int32_t len = split == 2 ? (abs(a->u - b->u) + abs(a->v - b->v)) >> 5 : abs(a->x - b->x) + abs(a->y - b->y);
            if (len > best) { best = len; e = i; }
        }
        {
            const SV *a = v[e], *b = v[(e + 1) % 3], *c = v[(e + 2) % 3];
            m.x = (int16_t)((a->x + b->x) >> 1); m.y = (int16_t)((a->y + b->y) >> 1);
            m.z = (int16_t)((a->z + b->z) >> 1);
            m.u = (a->u + b->u) >> 1; m.v = (a->v + b->v) >> 1;
            m.r = (int16_t)((a->r + b->r) >> 1); m.g = (int16_t)((a->g + b->g) >> 1); m.b = (int16_t)((a->b + b->b) >> 1);
            m.fr = (int16_t)((a->fr + b->fr) >> 1); m.fg = (int16_t)((a->fg + b->fg) >> 1);
            m.fb = (int16_t)((a->fb + b->fb) >> 1);
            emit_tri(a, &m, c, pr, depth + 1);
            emit_tri(&m, b, c, pr, depth + 1);
        }
        return;
    }
    {
        int minx = v0->x, maxx = v0->x, miny = v0->y, maxy = v0->y;
        for (k = 1; k < 3; k++) {
            if (v[k]->x < minx) minx = v[k]->x;
            if (v[k]->x > maxx) maxx = v[k]->x;
            if (v[k]->y < miny) miny = v[k]->y;
            if (v[k]->y > maxy) maxy = v[k]->y;
        }
        if (maxx - minx > 1023 || maxy - miny > 511)
            return;
    }
    words = (pr->textured ? 9 : 6) + (pr->semi ? 1 : 0) + (pr->window ? 2 : 0) + (pr->fog ? 7 : 0);
    if (!(p = pkt_alloc(words)))
        return;
    w = p + 1;
    if (pr->semi)
        *w++ = 0xE1000000 | 0x200 | 0x400 | (uint32_t)pr->abr << 5 | (pr->tex ? tex_tpage(pr->tex, pr->tex->vx) & 0x1FF : 0);
    if (pr->window)
        *w++ = pr->window;
    {
        /* the texels: a wrapping axis moved by whole periods to start at 0 (the
         * window adds the base), a clamped one held inside the texture */
        int32_t su = 0, sv = 0;
        if (pr->textured && pr->wraps) {
            int32_t mn = v0->u < v1->u ? v0->u : v1->u;
            if (v2->u < mn) mn = v2->u;
            su = (mn >> 5) & ~(pr->tex->w - 1);
        }
        if (pr->textured && pr->wrapt) {
            int32_t mn = v0->v < v1->v ? v0->v : v1->v;
            if (v2->v < mn) mn = v2->v;
            sv = (mn >> 5) & ~(pr->tex->h - 1);
        }
        for (k = 0; k < 3; k++) {
            w[0] = (uint32_t)clamp255(v[k]->r) | (uint32_t)clamp255(v[k]->g) << 8 | (uint32_t)clamp255(v[k]->b) << 16;
            if (k == 0)
                w[0] |= (uint32_t)pr->cmd << 24;
            w[1] = (uint32_t)(uint16_t)v[k]->y << 16 | (uint16_t)v[k]->x;
            if (pr->textured) {
                int u = v[k]->u >> 5, vv = v[k]->v >> 5;
                if (pr->wraps)
                    u = u - su;
                else
                    u = pr->ub + clamp_u(u, pr->tex->w - 1);
                if (pr->wrapt)
                    vv = vv - sv;
                else
                    vv = pr->vb + clamp_u(vv, pr->tex->h - 1);
                w[2] = (uint32_t)(clamp255(u)) | (uint32_t)(clamp255(vv)) << 8;
                if (k == 0)
                    w[2] |= (uint32_t)pr->tex->clut << 16;
                if (k == 1)
                    w[2] |= (uint32_t)(tex_tpage(pr->tex, pr->tex->vx) | pr->abr << 5) << 16;
                w += 3;
            } else {
                w += 2;
            }
        }
    }
    if (pr->window)
        *w++ = 0xE2000000;
    if (pr->fog) {
        /* the fog: its colour times the factor added over the texel times (1 - factor) */
        *w++ = 0xE1000000 | 0x200 | 0x400 | 1u << 5;
        for (k = 0; k < 3; k++) {
            w[0] = (uint32_t)clamp255(v[k]->fr) | (uint32_t)clamp255(v[k]->fg) << 8 | (uint32_t)clamp255(v[k]->fb) << 16;
            if (k == 0)
                w[0] |= 0x32u << 24;
            w[1] = (uint32_t)(uint16_t)v[k]->y << 16 | (uint16_t)v[k]->x;
            w += 2;
        }
    }
    if (pr->zsort)
        put_sorted(p, (v0->z + v1->z + v2->z) / 3);
    else
        put_flat(p);
}

/* an edge the GPU's affine mapping would bend: the texel at its middle
 * drawn more than 3 pixels from where perspective puts it (an edge of
 * screen length L between depths wa, wb: L |wb - wa| / 2 (wa + wb)); decided
 * by the edge alone, so the two triangles sharing it cut it alike */
static int edge_split(int32_t wa, int32_t wb, int ax, int ay, int bx, int by)
{
    int32_t l = abs(ax - bx) + abs(ay - by), d, sum;
    if (l <= 6)
        return 0;                               /* (|wb - wa| < wa + wb: never) */
    if (wa < W_NEAR) wa = W_NEAR;
    if (wb < W_NEAR) wb = W_NEAR;
    d = abs(wb - wa);
    sum = wa + wb;
    while (sum >= 0x10000) {                    /* (32 bits: l is under 2048) */
        sum >>= 1;
        d >>= 1;
    }
    return l * d > 6 * sum;
}

static int persp_split(const Vtx *a, const Vtx *b, const Vtx *c)
{
    return edge_split(a->cw, b->cw, a->x, a->y, b->x, b->y) || edge_split(b->cw, c->cw, b->x, b->y, c->x, c->y) ||
           edge_split(c->cw, a->cw, c->x, c->y, a->x, a->y);
}

/* a clip-space point to the screen: through the GTE from its model-space
 * place when the GTE holds the vertices' transform (no divides), else the
 * CPU's way */
static void sv_of(SV *o, const CV *c)
{
    o->u = c->u; o->v = c->v;
    o->r = (int16_t)c->r; o->g = (int16_t)c->g; o->b = (int16_t)c->b;
    o->fr = (int16_t)c->fr; o->fg = (int16_t)c->fg; o->fb = (int16_t)c->fb;
    if (s_gte_ok && c->w >= W_NEAR) {
        int32_t x = (c->mx + 8) >> 4, y = (c->my + 8) >> 4, z = (c->mz + 8) >> 4;
        if (x == (int16_t)x && y == (int16_t)y && z == (int16_t)z) {
            uint32_t flag, sxy;
            gte_mtc(GTE_VXY0, (uint32_t)(y << 16) | (uint32_t)(x & 0xFFFF));
            gte_mtc(GTE_VZ0, (uint32_t)z);
            gte_rtps();
            flag = gte_cfc(GTE_FLAG);
            if (!(flag & 0x7FC7F000u) && (int32_t)gte_mfc(GTE_MAC3) > 0) {
                sxy = gte_mfc(GTE_SXY2);
                o->x = (int16_t)(sxy & 0xFFFF);
                o->y = (int16_t)(sxy >> 16);
                o->z = (int16_t)wbucket(c->w);
                return;
            }
        }
    }
    project(c->x, c->y, c->z, c->w < W_NEAR ? W_NEAR : c->w, &o->x, &o->y, &o->z);
}

/* a piece of a cut triangle, small and inside the guard band, as its packet:
 * emit_tri's work without the checks a piece does not need (the texture's
 * span is checked: a repeat wider than the page's 256 texels goes there) */
static void emit_piece(const SV *a, const SV *b, const SV *c, const Prim *pr)
{
    const SV *v[3] = { a, b, c };
    uint32_t *p, *w;
    int k, su = 0, sv = 0;
    if (pr->fog || !pr->textured) {
        emit_tri(a, b, c, pr, 0);
        return;
    }
    if (pr->wraps || pr->wrapt) {
        int32_t mnu = a->u, mxu = a->u, mnv = a->v, mxv = a->v;
        for (k = 1; k < 3; k++) {
            if (v[k]->u < mnu) mnu = v[k]->u;
            if (v[k]->u > mxu) mxu = v[k]->u;
            if (v[k]->v < mnv) mnv = v[k]->v;
            if (v[k]->v > mxv) mxv = v[k]->v;
        }
        if ((pr->wraps && mxu - mnu > 192 * 32) || (pr->wrapt && mxv - mnv > 192 * 32)) {
            emit_tri(a, b, c, pr, 0);
            return;
        }
        if (pr->wraps)
            su = (mnu >> 5) & ~(pr->tex->w - 1);
        if (pr->wrapt)
            sv = (mnv >> 5) & ~(pr->tex->h - 1);
    }
    if (!(p = pkt_alloc(9 + (pr->semi ? 1 : 0) + (pr->window ? 2 : 0))))
        return;
    s_n_piece++;
    w = p + 1;
    if (pr->semi)
        *w++ = 0xE1000000 | 0x200 | 0x400 | (uint32_t)pr->abr << 5 | (tex_tpage(pr->tex, pr->tex->vx) & 0x1FF);
    if (pr->window)
        *w++ = pr->window;
    for (k = 0; k < 3; k++) {
        int u = v[k]->u >> 5, vv = v[k]->v >> 5;
        u = pr->wraps ? u - su : pr->ub + (u < 0 ? 0 : u >= pr->tex->w ? pr->tex->w - 1 : u);
        vv = pr->wrapt ? vv - sv : pr->vb + (vv < 0 ? 0 : vv >= pr->tex->h ? pr->tex->h - 1 : vv);
        w[0] = (uint32_t)clamp255(v[k]->r) | (uint32_t)clamp255(v[k]->g) << 8 | (uint32_t)clamp255(v[k]->b) << 16;
        w[1] = (uint32_t)(uint16_t)v[k]->y << 16 | (uint16_t)v[k]->x;
        w[2] = (uint32_t)clamp255(u) | (uint32_t)clamp255(vv) << 8;
        w += 3;
    }
    w[-9] |= (uint32_t)pr->cmd << 24;
    w[-7] |= (uint32_t)pr->tex->clut << 16;
    w[-4] |= (uint32_t)(tex_tpage(pr->tex, pr->tex->vx) | pr->abr << 5) << 16;
    if (pr->window)
        *w++ = 0xE2000000;
    if (pr->zsort)
        put_sorted(p, (a->z + b->z + c->z) / 3);
    else
        put_flat(p);
}

/* a clipped triangle to the GPU: a textured one cut in clip space (where its
 * texels run true) at the middles of the edges that need it, again in the
 * pieces, so the texture runs on across its neighbours' cuts; each point
 * goes to the screen once */
static void emit_cv3(const CV *a, const SV *sa, const CV *b, const SV *sb, const CV *c, const SV *sc, const Prim *pr,
                     int depth)
{
    if (pr->textured && depth < 5) {
        int e0 = edge_split(a->w, b->w, sa->x, sa->y, sb->x, sb->y);
        int e1 = edge_split(b->w, c->w, sb->x, sb->y, sc->x, sc->y);
        int e2 = edge_split(c->w, a->w, sc->x, sc->y, sa->x, sa->y);
        CV ab, bc, ca;
        SV sab, sbc, sca;
        if (e0) { cv_mid(&ab, a, b); sv_of(&sab, &ab); }
        if (e1) { cv_mid(&bc, b, c); sv_of(&sbc, &bc); }
        if (e2) { cv_mid(&ca, c, a); sv_of(&sca, &ca); }
        depth++;
        switch (e0 | e1 << 1 | e2 << 2) {
        case 0: break;
        case 1: emit_cv3(a, sa, &ab, &sab, c, sc, pr, depth); emit_cv3(&ab, &sab, b, sb, c, sc, pr, depth); return;
        case 2: emit_cv3(b, sb, &bc, &sbc, a, sa, pr, depth); emit_cv3(&bc, &sbc, c, sc, a, sa, pr, depth); return;
        case 4: emit_cv3(c, sc, &ca, &sca, b, sb, pr, depth); emit_cv3(&ca, &sca, a, sa, b, sb, pr, depth); return;
        case 3:                                 /* ab and bc */
            emit_cv3(&ab, &sab, b, sb, &bc, &sbc, pr, depth);
            emit_cv3(a, sa, &ab, &sab, &bc, &sbc, pr, depth);
            emit_cv3(a, sa, &bc, &sbc, c, sc, pr, depth);
            return;
        case 6:                                 /* bc and ca */
            emit_cv3(&bc, &sbc, c, sc, &ca, &sca, pr, depth);
            emit_cv3(b, sb, &bc, &sbc, &ca, &sca, pr, depth);
            emit_cv3(b, sb, &ca, &sca, a, sa, pr, depth);
            return;
        case 5:                                 /* ca and ab */
            emit_cv3(&ca, &sca, a, sa, &ab, &sab, pr, depth);
            emit_cv3(c, sc, &ca, &sca, &ab, &sab, pr, depth);
            emit_cv3(c, sc, &ab, &sab, b, sb, pr, depth);
            return;
        default:
            emit_cv3(a, sa, &ab, &sab, &ca, &sca, pr, depth);
            emit_cv3(&ab, &sab, b, sb, &bc, &sbc, pr, depth);
            emit_cv3(&ca, &sca, &bc, &sbc, c, sc, pr, depth);
            emit_cv3(&ab, &sab, &bc, &sbc, &ca, &sca, pr, depth);
            return;
        }
        if (depth > 1) {                        /* (a piece of a cut one) */
            int minx = sa->x, maxx = sa->x, miny = sa->y, maxy = sa->y;
            if (sb->x < minx) minx = sb->x;
            if (sb->x > maxx) maxx = sb->x;
            if (sc->x < minx) minx = sc->x;
            if (sc->x > maxx) maxx = sc->x;
            if (sb->y < miny) miny = sb->y;
            if (sb->y > maxy) maxy = sb->y;
            if (sc->y < miny) miny = sc->y;
            if (sc->y > maxy) maxy = sc->y;
            if (maxx - minx <= 1000 && maxy - miny <= 500) {
                emit_piece(sa, sb, sc, pr);
                return;
            }
        }
    }
    emit_tri(sa, sb, sc, pr, 0);
}

/* how many times an edge must be halved for its affine error to stay under
 * 3 pixels (the error falls by 4 with each halving): 0..3 */
static int edge_cuts(int32_t wa, int32_t wb, int ax, int ay, int bx, int by)
{
    int32_t l = abs(ax - bx) + abs(ay - by), d, sum, e;
    int k;
    if (l <= 6)
        return 0;
    if (wa < W_NEAR) wa = W_NEAR;
    if (wb < W_NEAR) wb = W_NEAR;
    d = abs(wb - wa);
    sum = wa + wb;
    while (sum >= 0x10000) {
        sum >>= 1;
        d >>= 1;
    }
    e = l * d;                                  /* the error is e / (2 sum) pixels */
    for (k = 0; k < 2 && e > 6 * sum; k++)
        e >>= 2;
    return k;
}

/* a grid point of a cut triangle on the screen */
typedef struct { int16_t x, y, z, u, v; uint8_t r, g, b, ok; } GP;

/* a textured triangle cut into an n by n grid of pieces (n a power of two):
 * every point through the GTE once, from its model-space place; 0 when the
 * GTE cannot take a point (then the halving way) */
/* a grid point as its packet words, in the CPU's fast RAM after the vertex records */
typedef struct { uint32_t xy, uv, rgb; int32_t z; } GPt;
#define s_gpt ((GPt *)0x1F800200u)               /* (at most 15 points: 240 bytes) */

static int emit_grid_words(const CV *a, const CV *b, const CV *c, const Prim *pr, int lg);
static int s_grid_strict;                       /* emit_grid_words: a point the GTE cannot take fails the grid (nothing written) */
static int emit_grid_slow(const CV *a, const CV *b, const CV *c, const Prim *pr, int lg);

static int emit_grid(const CV *a, const CV *b, const CV *c, const Prim *pr, int lg)
{
    return (lg <= 2 && emit_grid_words(a, b, c, pr, lg)) || emit_grid_slow(a, b, c, pr, lg);
}

/* the fast way: one texture window shift for the whole triangle (its texels
 * span under 192 in each repeating direction), the points' words made once by
 * stepping, the pieces' packets copied from them */
static int emit_grid_words(const CV *a, const CV *b, const CV *c, const Prim *pr, int lg)
{
    int n = 1 << lg, i, j, k, sh = 8 + s_gte_k;
    int tw = pr->tex->w, th = pr->tex->h, su = 0, svv = 0;
    int32_t mn, mx;
    /* the window shift: the triangle's least texel, whole periods */
    if (pr->wraps) {
        mn = a->u < b->u ? a->u : b->u; if (c->u < mn) mn = c->u;
        mx = a->u > b->u ? a->u : b->u; if (c->u > mx) mx = c->u;
        if (mx - mn > 192 * 32)
            return 0;
        su = (mn >> 5) & ~(tw - 1);
    }
    if (pr->wrapt) {
        mn = a->v < b->v ? a->v : b->v; if (c->v < mn) mn = c->v;
        mx = a->v > b->v ? a->v : b->v; if (c->v > mx) mx = c->v;
        if (mx - mn > 192 * 32)
            return 0;
        svv = (mn >> 5) & ~(th - 1);
    }
    {
        /* steps along i (towards b) and j (towards c); model space 4 fraction
         * bits, texels 1/32, colours 8.8 */
        int32_t dxi = (b->mx - a->mx) >> lg, dyi = (b->my - a->my) >> lg, dzi = (b->mz - a->mz) >> lg;
        int32_t dxj = (c->mx - a->mx) >> lg, dyj = (c->my - a->my) >> lg, dzj = (c->mz - a->mz) >> lg;
        int32_t dui = (b->u - a->u) >> lg, dvi = (b->v - a->v) >> lg, duj = (c->u - a->u) >> lg, dvj = (c->v - a->v) >> lg;
        int32_t dri = ((b->r - a->r) << 8) >> lg, dgi = ((b->g - a->g) << 8) >> lg, dbi = ((b->b - a->b) << 8) >> lg;
        int32_t drj = ((c->r - a->r) << 8) >> lg, dgj = ((c->g - a->g) << 8) >> lg, dbj = ((c->b - a->b) << 8) >> lg;
        /* (and clip space, for a point too near for the GTE's divide: the CPU projects it) */
        int32_t dcxi = (b->x - a->x) >> lg, dcyi = (b->y - a->y) >> lg, dczi = (b->z - a->z) >> lg, dcwi = (b->w - a->w) >> lg;
        int32_t dcxj = (c->x - a->x) >> lg, dcyj = (c->y - a->y) >> lg, dczj = (c->z - a->z) >> lg, dcwj = (c->w - a->w) >> lg;
        int32_t rx = a->mx, ry = a->my, rz = a->mz, ru = a->u, rv = a->v, rr = a->r << 8, rg = a->g << 8, rb = a->b << 8;
        int32_t rcx = a->x, rcy = a->y, rcz = a->z, rcw = a->w;
        for (j = 0, k = 0; j <= n; j++) {
            int32_t x = rx, y = ry, z = rz, u = ru, v = rv, r = rr, g = rg, bb = rb;
            int32_t cx = rcx, cy = rcy, cz = rcz, cw = rcw;
            for (i = 0; i + j <= n; i++, k++) {
                int32_t px = (x + 8) >> 4, py = (y + 8) >> 4, pz = (z + 8) >> 4, m3 = 0, uu, vv;
                GPt *q = &s_gpt[k];
                int ok = s_gte_ok && px == (int16_t)px && py == (int16_t)py && pz == (int16_t)pz;
                if (ok) {
                    gte_mtc(GTE_VXY0, (uint32_t)(py << 16) | (uint32_t)(px & 0xFFFF));
                    gte_mtc(GTE_VZ0, (uint32_t)pz);
                    gte_rtps();
                    ok = !(gte_cfc(GTE_FLAG) & 0x7FC7F000u) && (m3 = (int32_t)gte_mfc(GTE_MAC3)) > 0;
                }
                if (ok) {
                    q->xy = gte_mfc(GTE_SXY2);
                    q->z = wbucket(sh_r(m3, -sh));
                } else if (s_grid_strict) {
                    return 0;
                } else {
                    int16_t sx, sy, sz;
                    project(cx, cy, cz, cw < W_NEAR ? W_NEAR : cw, &sx, &sy, &sz);
                    q->xy = (uint32_t)(uint16_t)sy << 16 | (uint16_t)sx;
                    q->z = sz;
                }
                uu = u >> 5;
                vv = v >> 5;
                uu = pr->wraps ? uu - su : pr->ub + (uu < 0 ? 0 : uu >= tw ? tw - 1 : uu);
                vv = pr->wrapt ? vv - svv : pr->vb + (vv < 0 ? 0 : vv >= th ? th - 1 : vv);
                q->uv = (uint32_t)(uu & 255) | (uint32_t)(vv & 255) << 8;
                q->rgb = (uint32_t)clamp255(r >> 8) | (uint32_t)clamp255(g >> 8) << 8 | (uint32_t)clamp255(bb >> 8) << 16;
                x += dxi; y += dyi; z += dzi; u += dui; v += dvi; r += dri; g += dgi; bb += dbi;
                cx += dcxi; cy += dcyi; cz += dczi; cw += dcwi;
            }
            rx += dxj; ry += dyj; rz += dzj; ru += duj; rv += dvj; rr += drj; rg += dgj; rb += dbj;
            rcx += dcxj; rcy += dcyj; rcz += dczj; rcw += dcwj;
        }
    }
    if (s_grid_strict) {
        /* each piece no bigger than the GPU draws (the corners may be past the guard band) */
        int row = 0;
        for (j = 0; j < n; j++) {
            int next = row + (n + 1 - j);
            for (i = 0; i < n - j; i++) {
                const GPt *q[4] = { &s_gpt[row + i], &s_gpt[row + i + 1], &s_gpt[next + i], &s_gpt[i + 1 < n - j ? next + i + 1 : next + i] };
                int32_t x0 = 4096, x1 = -4096, y0 = 4096, y1 = -4096, e;
                for (e = 0; e < 4; e++) {
                    int32_t x = (int16_t)q[e]->xy, y = (int32_t)q[e]->xy >> 16;
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
                if (x1 - x0 > 1023 || y1 - y0 > 511)
                    return 0;
            }
            row = next;
        }
    }
    {
        uint32_t clut = (uint32_t)pr->tex->clut << 16, tpage = (uint32_t)(tex_tpage(pr->tex, pr->tex->vx) | pr->abr << 5) << 16;
        uint32_t cmd = (uint32_t)pr->cmd << 24, pre = pr->semi ? 0xE1000000 | 0x200 | 0x400 | (uint32_t)pr->abr << 5 | (tpage >> 16 & 0x1FF) : 0;
        int row = 0, words = 9 + (pr->semi ? 1 : 0) + (pr->window ? 2 : 0), zs = pr->zsort;
        for (j = 0; j < n; j++) {
            int next = row + (n + 1 - j);
            for (i = 0; i < n - j; i++) {
                int tri;
                for (tri = 0; tri < (i + 1 < n - j ? 2 : 1); tri++) {
                    const GPt *p0, *p1, *p2;
                    uint32_t *p, *w;
                    if (!tri) { p0 = &s_gpt[row + i]; p1 = &s_gpt[row + i + 1]; p2 = &s_gpt[next + i]; }
                    else { p0 = &s_gpt[row + i + 1]; p1 = &s_gpt[next + i + 1]; p2 = &s_gpt[next + i]; }
                    if (!(p = pkt_alloc(words)))
                        return 1;
                    s_n_piece++;
                    w = p + 1;
                    if (pre)
                        *w++ = pre;
                    if (pr->window)
                        *w++ = pr->window;
                    w[0] = p0->rgb | cmd; w[1] = p0->xy; w[2] = p0->uv | clut;
                    w[3] = p1->rgb; w[4] = p1->xy; w[5] = p1->uv | tpage;
                    w[6] = p2->rgb; w[7] = p2->xy; w[8] = p2->uv;
                    if (pr->window)
                        w[9] = 0xE2000000;
                    if (zs)
                        put_sorted(p, (p0->z + p1->z + p2->z) / 3);
                    else
                        put_flat(p);
                }
            }
            row = next;
        }
    }
    return 1;
}

static int emit_grid_slow(const CV *a, const CV *b, const CV *c, const Prim *pr, int lg)
{
    GP pt[45];
    int n = 1 << lg, i, j, k, sh = 8 + s_gte_k;
    int32_t dmx1 = (b->mx - a->mx) >> lg, dmy1 = (b->my - a->my) >> lg, dmz1 = (b->mz - a->mz) >> lg;
    int32_t dmx2 = (c->mx - a->mx) >> lg, dmy2 = (c->my - a->my) >> lg, dmz2 = (c->mz - a->mz) >> lg;
    if (!s_gte_ok)
        return 0;
    for (j = 0, k = 0; j <= n; j++)
        for (i = 0; i + j <= n; i++, k++) {
            int32_t x = (a->mx + dmx1 * i + dmx2 * j + 8) >> 4, y = (a->my + dmy1 * i + dmy2 * j + 8) >> 4;
            int32_t z = (a->mz + dmz1 * i + dmz2 * j + 8) >> 4, m3;
            uint32_t sxy;
            GP *q = &pt[k];
            if (x != (int16_t)x || y != (int16_t)y || z != (int16_t)z)
                return 0;
            gte_mtc(GTE_VXY0, (uint32_t)(y << 16) | (uint32_t)(x & 0xFFFF));
            gte_mtc(GTE_VZ0, (uint32_t)z);
            gte_rtps();
            if (gte_cfc(GTE_FLAG) & 0x7FC7F000u)
                return 0;
            m3 = (int32_t)gte_mfc(GTE_MAC3);
            if (m3 <= 0)
                return 0;
            sxy = gte_mfc(GTE_SXY2);
            q->x = (int16_t)(sxy & 0xFFFF);
            q->y = (int16_t)(sxy >> 16);
            q->z = (int16_t)wbucket(sh_r(m3, -sh));
            /* the texels and colours: the corners' by the point's place (n is 1 << lg) */
            q->u = (int16_t)((a->u + (((b->u - a->u) * i + (c->u - a->u) * j) >> lg)) >> 5);
            q->v = (int16_t)((a->v + (((b->v - a->v) * i + (c->v - a->v) * j) >> lg)) >> 5);
            q->r = (uint8_t)clamp255(a->r + (((b->r - a->r) * i + (c->r - a->r) * j) >> lg));
            q->g = (uint8_t)clamp255(a->g + (((b->g - a->g) * i + (c->g - a->g) * j) >> lg));
            q->b = (uint8_t)clamp255(a->b + (((b->b - a->b) * i + (c->b - a->b) * j) >> lg));
        }
    {
        uint32_t clut = (uint32_t)pr->tex->clut << 16, tpage = (uint32_t)(tex_tpage(pr->tex, pr->tex->vx) | pr->abr << 5) << 16;
        uint32_t cmd = (uint32_t)pr->cmd << 24;
        int tw = pr->tex->w, th = pr->tex->h, row = 0, words = 9 + (pr->semi ? 1 : 0) + (pr->window ? 2 : 0);
        for (j = 0; j < n; j++) {
            int next = row + (n + 1 - j);           /* the first point of row j + 1 */
            for (i = 0; i < n - j; i++) {
                int tri;
                for (tri = 0; tri < (i + 1 < n - j ? 2 : 1); tri++) {
                    const GP *v[3];
                    uint32_t *p, *w;
                    int m, su = 0, svv = 0;
                    if (!tri) { v[0] = &pt[row + i]; v[1] = &pt[row + i + 1]; v[2] = &pt[next + i]; }
                    else { v[0] = &pt[row + i + 1]; v[1] = &pt[next + i + 1]; v[2] = &pt[next + i]; }
                    if (pr->wraps || pr->wrapt) {
                        int mnu = v[0]->u, mnv = v[0]->v;
                        for (m = 1; m < 3; m++) {
                            if (v[m]->u < mnu) mnu = v[m]->u;
                            if (v[m]->v < mnv) mnv = v[m]->v;
                        }
                        su = mnu & ~(tw - 1);
                        svv = mnv & ~(th - 1);
                    }
                    if (!(p = pkt_alloc(words)))
                        return 1;
                    s_n_piece++;
                    w = p + 1;
                    if (pr->semi)
                        *w++ = 0xE1000000 | 0x200 | 0x400 | (uint32_t)pr->abr << 5 | (tpage >> 16 & 0x1FF);
                    if (pr->window)
                        *w++ = pr->window;
                    for (m = 0; m < 3; m++) {
                        int u = v[m]->u, vv = v[m]->v;
                        u = pr->wraps ? u - su : pr->ub + (u < 0 ? 0 : u >= tw ? tw - 1 : u);
                        vv = pr->wrapt ? vv - svv : pr->vb + (vv < 0 ? 0 : vv >= th ? th - 1 : vv);
                        w[0] = (uint32_t)v[m]->r | (uint32_t)v[m]->g << 8 | (uint32_t)v[m]->b << 16;
                        w[1] = (uint32_t)(uint16_t)v[m]->y << 16 | (uint16_t)v[m]->x;
                        w[2] = (uint32_t)(u & 255) | (uint32_t)(vv & 255) << 8;
                        w += 3;
                    }
                    w[-9] |= cmd;
                    w[-7] |= clut;
                    w[-4] |= tpage;
                    if (pr->window)
                        *w++ = 0xE2000000;
                    if (pr->zsort)
                        put_sorted(p, (v[0]->z + v[1]->z + v[2]->z) / 3);
                    else
                        put_flat(p);
                }
            }
            row = next;
        }
    }
    return 1;
}

static void emit_cv(const CV *a, const CV *b, const CV *c, const Prim *pr, int depth)
{
    SV sv[3];
    sv_of(&sv[0], a);
    sv_of(&sv[1], b);
    sv_of(&sv[2], c);
    if (pr->textured && !pr->fog && pr->tex) {
        int k0 = edge_cuts(a->w, b->w, sv[0].x, sv[0].y, sv[1].x, sv[1].y);
        int k1 = edge_cuts(b->w, c->w, sv[1].x, sv[1].y, sv[2].x, sv[2].y);
        int k2 = edge_cuts(c->w, a->w, sv[2].x, sv[2].y, sv[0].x, sv[0].y);
        int lg = k0 > k1 ? k0 : k1;
        if (k2 > lg)
            lg = k2;
        if (lg > 0 && emit_grid(a, b, c, pr, lg))
            return;
        if (lg == 0) {
            emit_tri(&sv[0], &sv[1], &sv[2], pr, 0);
            return;
        }
    } else if (!pr->textured) {
        emit_tri(&sv[0], &sv[1], &sv[2], pr, 0);
        return;
    }
    emit_cv3(a, &sv[0], b, &sv[1], c, &sv[2], pr, depth);
}

/* what the slow path worked out for the current draw state, reused by the
 * triangles after it while nothing changes (most of a model's) */
static struct {
    uint32_t stamp;
    int ok, flat, mask, textured;
    Prim pr;
    int32_t s0, t0;
    int shifts, shiftt;
} s_fs;

static void vtx_fast(Vtx *vk, const Vtx *v0)
{
    uint8_t sh[4];
    Lin col;
    const Vtx *q = s_fs.flat ? v0 : vk;
    int r, g, b;
    sh[0] = q->r; sh[1] = q->g; sh[2] = q->b; sh[3] = vk->a;
    if (!s_fs.flat && vk->cc_stamp == s_cc_stamp) {
        col = vk->col;
    } else {
        combine(&col, sh, NULL);
        if (!s_fs.flat) {
            vk->col = col;
            vk->cc_stamp = s_cc_stamp;
        }
    }
    if (s_fs.textured && s_fs.mask) {
        r = col.b[0] >> 1; g = col.b[1] >> 1; b = col.b[2] >> 1;
    } else if (s_fs.textured) {
        r = (col.a[0] + col.b[0]) >> 1; g = (col.a[1] + col.b[1]) >> 1; b = (col.a[2] + col.b[2]) >> 1;
    } else {
        r = col.b[0]; g = col.b[1]; b = col.b[2];
    }
    vk->ffr = vk->ffg = vk->ffb = 0;
    if (s_fs.pr.fog) {
        int f = vk->a, nf = 255 - f;
        r = r * nf / 255; g = g * nf / 255; b = b * nf / 255;
        vk->ffr = (int16_t)(s_fogc[0] * f / 255); vk->ffg = (int16_t)(s_fogc[1] * f / 255);
        vk->ffb = (int16_t)(s_fogc[2] * f / 255);
    }
    vk->fr = (int16_t)r; vk->fg = (int16_t)g; vk->fb = (int16_t)b;
    if (s_fs.textured) {
        vk->fu = tc32(vk->s, s_fs.s0, s_fs.shifts) << s_fs.pr.tex->up;
        vk->fv = tc32(vk->t, s_fs.t0, s_fs.shiftt) << s_fs.pr.tex->up;
    } else {
        vk->fu = vk->fv = 0;
    }
    vk->fstamp = s_fs.stamp;
}

/* a triangle wholly inside the frustum under an unchanged draw state */
static int tri_fast(Vtx *v0, Vtx *v1, Vtx *v2)
{
    Vtx *v[3];
    SV sv[3];
    int k;
    if (s_fs.stamp != s_dstamp) { s_n_stamp++; return 0; }
    if (!s_fs.ok) { s_n_notok++; return 0; }
    if ((v0->clip | v1->clip | v2->clip) & (16 | 32)) { s_n_edge++; return 0; }
    if (s_fs.textured && persp_split(v0, v1, v2)) { s_n_edge++; return 0; }
    s_n_fast++;
    {
        int32_t det = (v1->x - v0->x) * (v2->y - v0->y) - (v2->x - v0->x) * (v1->y - v0->y);
        if (((s_geom & G_CULL_BACK) && det >= 0) || ((s_geom & G_CULL_FRONT) && det <= 0)) {
            s_n_cull++;
            return 1;
        }
    }
    v[0] = v0; v[1] = v1; v[2] = v2;
    cc_compile();
    for (k = 0; k < 3; k++)
        if (v[k]->fstamp != s_fs.stamp || s_fs.flat)
            vtx_fast(v[k], v0);
    {
        /* the common case written straight out: a plain (Gouraud, maybe
         * textured, clamped) triangle the GPU takes whole */
        const Prim *pr = &s_fs.pr;
        int minx = v0->x, maxx = v0->x, miny = v0->y, maxy = v0->y;
        if (v1->x < minx) minx = v1->x;
        if (v1->x > maxx) maxx = v1->x;
        if (v2->x < minx) minx = v2->x;
        if (v2->x > maxx) maxx = v2->x;
        if (v1->y < miny) miny = v1->y;
        if (v1->y > maxy) maxy = v1->y;
        if (v2->y < miny) miny = v2->y;
        if (v2->y > maxy) maxy = v2->y;
        if (!pr->window && !pr->fog && !pr->semi && maxx - minx <= 1000 &&
            maxy - miny <= 500) {
            uint32_t *p = pkt_alloc(pr->textured ? 9 : 6), *w;
            if (!p)
                return 1;
            w = p + 1;
            for (k = 0; k < 3; k++) {
                const Vtx *q = v[k];
                w[0] = (uint32_t)clamp255(q->fr) | (uint32_t)clamp255(q->fg) << 8 | (uint32_t)clamp255(q->fb) << 16;
                w[1] = (uint32_t)(uint16_t)q->y << 16 | (uint16_t)q->x;
                if (pr->textured) {
                    int u = q->fu >> 5, vv = q->fv >> 5;
                    u = pr->ub + (u < 0 ? 0 : u >= pr->tex->w ? pr->tex->w - 1 : u);
                    vv = pr->vb + (vv < 0 ? 0 : vv >= pr->tex->h ? pr->tex->h - 1 : vv);
                    w[2] = (uint32_t)(u & 255) | (uint32_t)(vv & 255) << 8;
                    w += 3;
                } else {
                    w += 2;
                }
            }
            p[1] |= (uint32_t)pr->cmd << 24;
            if (pr->textured) {
                p[3] |= (uint32_t)pr->tex->clut << 16;
                p[6] |= (uint32_t)tex_tpage(pr->tex, pr->tex->vx) << 16;
            }
            s_n_piece++;
            if (pr->zsort)
                put_sorted(p, (v0->z + v1->z + v2->z) / 3);
            else
                put_flat(p);
            return 1;
        }
    }
    for (k = 0; k < 3; k++) {
        sv[k].x = v[k]->x; sv[k].y = v[k]->y; sv[k].z = v[k]->z;
        sv[k].u = v[k]->fu; sv[k].v = v[k]->fv;
        sv[k].r = v[k]->fr; sv[k].g = v[k]->fg; sv[k].b = v[k]->fb;
        sv[k].fr = v[k]->ffr; sv[k].fg = v[k]->ffg; sv[k].fb = v[k]->ffb;
    }
    emit_tri(&sv[0], &sv[1], &sv[2], &s_fs.pr, 0);
    return 1;
}

static void triangle_(int ia, int ib, int ic);
static int s_dry;                               /* triangle() works out the draw (s_fs) and draws nothing */
static void triangle(int ia, int ib, int ic)
{
    triangle_(ia, ib, ic);
}

static void triangle_(int ia, int ib, int ic)
{
    const Vtx *v[3];
    Draw d;
    Lin col[3];
    Prim pr;
    CV poly[2][12];
    int k, n, semi = 0, abr = 0, mask, cur;
    int tile = s_tex.tile, want_tex;
    if (ia >= 32 || ib >= 32 || ic >= 32 || s_target < 0)
        return;
    if (s_mvstale) {
        mesh_fill(ia);
        mesh_fill(ib);
        mesh_fill(ic);
    }
    v[0] = &s_vtx[ia]; v[1] = &s_vtx[ib]; v[2] = &s_vtx[ic];
    if (!s_dry) {
    s_n_tri++;
    if (v[0]->clip & v[1]->clip & v[2]->clip & 31) {
        s_n_rej++;
        return;                                 /* wholly off one side, or behind the eye */
    }
    if (tri_fast(&s_vtx[ia], &s_vtx[ib], &s_vtx[ic]))
        return;
    {
        /* the facing, from the homogeneous coordinates: det(x, y, w) keeps its
         * sign for a triangle reaching behind the eye (as the RSP's does) */
        int64_t det = (int64_t)v[0]->cx * (((int64_t)v[1]->cy * v[2]->cw - (int64_t)v[2]->cy * v[1]->cw) >> 8) -
                      (int64_t)v[0]->cy * (((int64_t)v[1]->cx * v[2]->cw - (int64_t)v[2]->cx * v[1]->cw) >> 8) +
                      (int64_t)v[0]->cw * (((int64_t)v[1]->cx * v[2]->cy - (int64_t)v[2]->cx * v[1]->cy) >> 8);
        if (((s_geom & G_CULL_BACK) && det <= 0) || ((s_geom & G_CULL_FRONT) && det >= 0))
            return;
    }
    }
    uint32_t dq0 = ST_NOW(), dq1;
    cc_compile();
    {
        int other = s_tex.on && uses_other(), flat = !(s_geom & G_SHADING_SMOOTH);
        for (k = 0; k < 3; k++) {
            uint8_t sh[4], ot[4];
            Vtx *vk = &s_vtx[k == 0 ? ia : k == 1 ? ib : ic];
            const Vtx *q = flat ? v[0] : v[k];
            if (!other && !flat && vk->cc_stamp == s_cc_stamp) {   /* combined for an earlier triangle */
                col[k] = vk->col;
                continue;
            }
            sh[0] = q->r; sh[1] = q->g; sh[2] = q->b; sh[3] = v[k]->a;
            if (other)
                sample((tile + 1) & 7, v[k]->s, v[k]->t, ot);
            combine(&col[k], sh, other ? ot : NULL);
            if (!other && !flat) {
                vk->col = col[k];
                vk->cc_stamp = s_cc_stamp;
            }
        }
    }
    {
        /* textured for the colour, or only for the alpha's shape (a mask), or not */
        int rgb_t = (col[0].a[0] | col[0].a[1] | col[0].a[2] | col[1].a[0] | col[2].a[0]) != 0;
        mask = !rgb_t && col[0].a[3] != 0;
        want_tex = s_tex.on && (rgb_t || mask);
    }
    dq1 = ST_NOW();
    if (s_dry)
        s_dryq[0] += (dq1 - dq0) & 0xFFFF;
    draw_state(&d, tile, want_tex, mask);
    if (s_dry)
        s_dryq[1] += (ST_NOW() - dq1) & 0xFFFF;
    if (d.semi == 1) {
        semi = 1;
        abr = d.abr;
    } else if (d.semi == 2) {
        if ((semi = blend_mode(&col[0], d.textured, &abr)) < 0)
            return;
    }
    memset(&pr, 0, sizeof pr);
    pr.tex = d.tex;
    pr.textured = d.textured;
    pr.semi = semi;
    pr.abr = abr;
    pr.zsort = d.zsort;
    pr.cmd = (d.textured ? 0x34 : 0x30) | (semi ? 2 : 0);
    if (d.textured) {
        pr.wraps = d.tex->wraps;
        pr.wrapt = d.tex->wrapt;
        pr.ub = (d.tex->vx & 63) * tex_k(d.tex);
        pr.vb = d.tex->vy & 255;
        if (pr.wraps || pr.wrapt)
            pr.window = 0xE2000000 |
                        (pr.wraps ? (uint32_t)((~(d.tex->w - 1) & 0xFF) >> 3) | (uint32_t)(pr.ub >> 3) << 10 : 0) |
                        (pr.wrapt ? (uint32_t)((~(d.tex->h - 1) & 0xFF) >> 3) << 5 | (uint32_t)(pr.vb >> 3) << 15 : 0);
    }
    {
        /* fogged: the blender takes the fog colour by the shade alpha (P = fog, A = shade alpha) */
        uint32_t bl = s_oml >> 16;
        pr.fog = (s_geom & G_FOG) && ((bl >> 14) & 3) == 3 && ((bl >> 10) & 3) == 2;
    }
    /* the fast path may take the next triangles: no second texture to sample, a
     * blend that does not hang on this triangle's colours */
    s_fs.ok = !(s_tex.on && uses_other()) && (d.semi != 2 || (d.textured && col[0].a[3]));
    s_fs.stamp = s_dstamp;
    s_fs.flat = !(s_geom & G_SHADING_SMOOTH);
    s_fs.mask = mask;
    s_fs.textured = d.textured;
    s_fs.pr = pr;
    s_fs.s0 = d.s0; s_fs.t0 = d.t0;
    s_fs.shifts = d.shifts; s_fs.shiftt = d.shiftt;
    if (s_dry)
        return;
    for (k = 0; k < 3; k++) {
        CV *c = &poly[0][k];
        int r, g, b;
        c->x = v[k]->cx; c->y = v[k]->cy; c->z = v[k]->cz; c->w = v[k]->cw;
        c->mx = v[k]->mx * 16; c->my = v[k]->my * 16; c->mz = v[k]->mz * 16;
        if (d.textured) {
            c->u = tc32(v[k]->s, d.s0, d.shifts) << d.tex->up;
            c->v = tc32(v[k]->t, d.t0, d.shiftt) << d.tex->up;
        } else {
            c->u = c->v = 0;
        }
        if (d.textured && mask) {               /* white texels times the colour */
            r = col[k].b[0] >> 1; g = col[k].b[1] >> 1; b = col[k].b[2] >> 1;
        } else if (d.textured) {                /* the GPU modulates (128 is 1.0) by the colour at texel 1 */
            r = (col[k].a[0] + col[k].b[0]) >> 1; g = (col[k].a[1] + col[k].b[1]) >> 1;
            b = (col[k].a[2] + col[k].b[2]) >> 1;
        } else {
            r = col[k].b[0]; g = col[k].b[1]; b = col[k].b[2];
        }
        c->r = r; c->g = g; c->b = b;
        c->fr = c->fg = c->fb = 0;
        if (pr.fog) {
            int f = v[k]->a, nf = 255 - f;
            c->r = r * nf / 255; c->g = g * nf / 255; c->b = b * nf / 255;
            c->fr = s_fogc[0] * f / 255; c->fg = s_fogc[1] * f / 255; c->fb = s_fogc[2] * f / 255;
        }
    }
    n = 3;
    cur = 0;
    {
        int pl, any = (v[0]->clip | v[1]->clip | v[2]->clip) & 16;
        int far = 0;
        for (k = 0; k < 3 && !any; k++)         /* the guard band: only when a vertex is outside it */
            if (abs(v[k]->cx) > GUARD * v[k]->cw || abs(v[k]->cy) > GUARD * v[k]->cw)
                far = 1;
        for (pl = 0; pl < 5 && n >= 3; pl++) {
            if (pl == 0 && !any)
                continue;
            if (pl > 0 && !far && !any)
                break;
            n = clip_plane(poly[cur], n, poly[cur ^ 1], pl);
            s_n_clip++;
            cur ^= 1;
        }
    }
    if (n < 3)
        return;
    for (k = 1; k + 1 < n; k++)
        emit_cv(&poly[cur][0], &poly[cur][k], &poly[cur][k + 1], &pr, 0);
}

/* a textured rectangle as a 4-point textured polygon */
/* a textured rectangle's quads on the GPU, for texels [u0, u1) x [v0, v1) of
 * one texture: one quad per texture page the texels cross (the GPU's u is 8
 * bits within a page), x scaled between the texel bounds; rgb the corners'
 * colours (top left, top right, bottom left, bottom right), Gouraud-shaded
 * when shaded */
static void put_texrect(const Tex *tex, int x0, int y0, int x1, int y1, int u0, int v0, int u1, int v1,
                        int rgb[4][3], int shaded, int semi, int abr)
{
    int kk = tex_k(tex), lo = u0 < u1 ? u0 : u1, hi = u0 < u1 ? u1 : u0, ua = lo;
    int vy0 = (tex->vy & 255) + v0, vy1 = (tex->vy & 255) + v1;
    uint32_t *p;
    if (hi == lo)
        hi = lo + 1;
    while (ua < hi) {
        int hx = tex->vx + ua / kk;                         /* the halfword column of texel ua */
        int pg_end = ((hx / 64) + 1) * 64;                  /* the page's end, halfwords */
        int ub = (pg_end - tex->vx) * kk;                   /* the first texel past it */
        int pa, pb, xa, xb, uu0, uu1, c, fa, fb;
        uint32_t ca[2], cb[2];
        uint16_t tp = tex_tpage(tex, hx);
        if (ub > hi)
            ub = hi;
        pa = ((tex->vx * kk + ua) - (hx / 64) * 64 * kk) & 255;
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
        fa = (xa - x0) * 256 / (x1 - x0);               /* the piece's ends across the rectangle */
        fb = (xb - x0) * 256 / (x1 - x0);
        for (c = 0; c < 2; c++) {                       /* top (corners 0, 1), bottom (2, 3) */
            const int *l = rgb[c * 2], *r = rgb[c * 2 + 1];
            ca[c] = (uint32_t)(l[0] + ((r[0] - l[0]) * fa >> 8)) | (uint32_t)(l[1] + ((r[1] - l[1]) * fa >> 8)) << 8 |
                    (uint32_t)(l[2] + ((r[2] - l[2]) * fa >> 8)) << 16;
            cb[c] = (uint32_t)(l[0] + ((r[0] - l[0]) * fb >> 8)) | (uint32_t)(l[1] + ((r[1] - l[1]) * fb >> 8)) << 8 |
                    (uint32_t)(l[2] + ((r[2] - l[2]) * fb >> 8)) << 16;
        }
        if (xb > xa && shaded && (p = pkt_alloc(12))) {
            p[1] = (uint32_t)(0x3C | (semi ? 2 : 0)) << 24 | ca[0];
            p[2] = (uint32_t)y0 << 16 | (uint16_t)xa;
            p[3] = (uint32_t)tex->clut << 16 | (uint32_t)(vy0 & 255) << 8 | (uint32_t)uu0;
            p[4] = cb[0];
            p[5] = (uint32_t)y0 << 16 | (uint16_t)xb;
            p[6] = (uint32_t)(tp | abr << 5) << 16 | (uint32_t)(vy0 & 255) << 8 | (uint32_t)uu1;
            p[7] = ca[1];
            p[8] = (uint32_t)y1 << 16 | (uint16_t)xa;
            p[9] = (uint32_t)(vy1 & 255) << 8 | (uint32_t)uu0;
            p[10] = cb[1];
            p[11] = (uint32_t)y1 << 16 | (uint16_t)xb;
            p[12] = (uint32_t)(vy1 & 255) << 8 | (uint32_t)uu1;
            put_flat(p);
        } else if (xb > xa && !shaded && (p = pkt_alloc(9))) {
            p[1] = (uint32_t)(0x2C | (semi ? 2 : 0)) << 24 | ca[0];
            p[2] = (uint32_t)y0 << 16 | (uint16_t)xa;
            p[3] = (uint32_t)tex->clut << 16 | (uint32_t)(vy0 & 255) << 8 | (uint32_t)uu0;
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

/* a textured rectangle (G_TEXRECT); a tile taller than a band is drawn a band
 * of rows at a time, each its own texture */
static void rect_tex_(uint32_t w0, uint32_t w1, uint32_t h2, uint32_t hc);
static void rect_tex(uint32_t w0, uint32_t w1, uint32_t h2, uint32_t hc)
{
    RG_BEGIN(RG_RECT);
    rect_tex_(w0, w1, h2, hc);
    RG_END(RG_RECT);
}

static void rect_tex_(uint32_t w0, uint32_t w1, uint32_t h2, uint32_t hc)
{
    int x1 = (int)((w0 >> 12) & 0xFFF) >> 2, y1 = (int)(w0 & 0xFFF) >> 2;
    int x0 = (int)((w1 >> 12) & 0xFFF) >> 2, y0 = (int)(w1 & 0xFFF) >> 2;
    int tile = (w1 >> 24) & 7, cyc = (s_omh >> 20) & 3;
    int32_t s = (int16_t)(h2 >> 16), t = (int16_t)h2;          /* 10.5 */
    int32_t dsdx = (int16_t)(hc >> 16), dtdy = (int16_t)hc;    /* 5.10 */
    Draw d;
    Lin col[4];                                 /* the corners: top left, top right, bottom left, bottom right */
    int rgb[4][3];
    int u0, v0, u1, v1, semi = 0, abr = 0, k, shaded = 0, mask, tw, th, band, vlo, vhi;
    if (s_target < 0)
        return;
    if (cyc == 2) {                             /* copy mode: inclusive, 4 texels a step */
        dsdx >>= 2;
        x1++;
        y1++;
    }
    if (x1 <= x0 || y1 <= y0)
        return;
    uint32_t rq0 = ST_NOW();
    s_rq[3]++;
    cc_compile();
    if (cyc == 2) {
        memset(col, 0, sizeof col);
        for (k = 0; k < 4; k++)
            col[k].a[0] = col[k].a[1] = col[k].a[2] = col[k].a[3] = 256;
    } else if (uses_other()) {
        int32_t s1 = s + ((dsdx * (x1 - x0)) >> 5), t1 = t + ((dtdy * (y1 - y0)) >> 5);
        s_rqo++;
        for (k = 0; k < 4; k++) {
            uint8_t ot[4];
            sample((tile + 1) & 7, (k & 1) ? s1 : s, (k & 2) ? t1 : t, ot);
            combine(&col[k], WHITE, ot);
        }
        shaded = 1;
    } else {
        combine(&col[0], WHITE, NULL);
        col[1] = col[2] = col[3] = col[0];
    }
    mask = cyc != 2 && !(col[0].a[0] | col[0].a[1] | col[0].a[2]) && col[0].a[3];
    for (k = 0; k < 4; k++) {                   /* the GPU modulates: 128 is 1.0 */
        rgb[k][0] = clamp255((mask ? col[k].b[0] : col[k].a[0] + col[k].b[0]) >> 1);
        rgb[k][1] = clamp255((mask ? col[k].b[1] : col[k].a[1] + col[k].b[1]) >> 1);
        rgb[k][2] = clamp255((mask ? col[k].b[2] : col[k].a[2] + col[k].b[2]) >> 1);
    }
    s_rq[0] += (ST_NOW() - rq0) & 0xFFFF;
    if (!tile_size(&s_tile[tile], &tw, &th))
        return;
    band = th > 64 ? BAND : 0;
    /* the texels drawn: s, t to the tile's coordinates; v across the rectangle */
    {
        int32_t s0 = (s_tile[tile].uls > s_tile[tile].lrs ? s_tile[tile].uls - 4096 : s_tile[tile].uls) << 3;
        int32_t t0 = (s_tile[tile].ult > s_tile[tile].lrt ? s_tile[tile].ult - 4096 : s_tile[tile].ult) << 3;
        u0 = (s - s0) >> 5;
        v0 = (t - t0) >> 5;
        u1 = u0 + (int)((dsdx * (x1 - x0)) >> 10);
        v1 = v0 + (int)((dtdy * (y1 - y0)) >> 10);
    }
    u0 = clamp_u(u0, tw); u1 = clamp_u(u1, tw);
    v0 = clamp_u(v0, th); v1 = clamp_u(v1, th);
    vlo = v0 < v1 ? v0 : v1;
    vhi = v0 < v1 ? v1 : v0;
    if (vhi == vlo)
        vhi = vlo + 1;
    {
        int b0 = band ? (vlo / band) * band : 0;
        for (; b0 < (band ? vhi : 1); b0 += band ? band : 1) {
            int a, b, ya, yb, gy0, gy1, va, vb;
            s_band_y0 = band ? b0 : 0;
            s_band_h = band;
            s_in_rect = 1;
            {
                uint32_t q = ST_NOW();
                draw_state(&d, tile, 1, mask);
                s_rq[1] += (ST_NOW() - q) & 0xFFFF;
            }
            s_in_rect = 0;
            s_band_y0 = s_band_h = 0;
            if (!d.textured) {
                if (s_log)
                    tgr_log("rcp: texrect without a texture (tile %d fmt %d siz %d)\n", tile, s_tile[tile].fmt,
                            s_tile[tile].siz);
                return;
            }
            if (cyc != 2 && d.semi == 2 && (semi = blend_mode(&col[0], 1, &abr)) < 0)
                return;
            else if (cyc != 2 && d.semi == 1) {
                semi = 1;
                abr = 1;
            }
            /* this band's texels, and the game rows they cover */
            a = band ? (vlo > b0 ? vlo : b0) : vlo;
            b = band ? (vhi < b0 + band ? vhi : b0 + band) : vhi;
            if (b <= a)
                continue;
            if (v0 <= v1) {
                gy0 = y0 + (a - vlo) * (y1 - y0) / (vhi - vlo);
                gy1 = y0 + (b - vlo) * (y1 - y0) / (vhi - vlo);
                va = a - s_band_y0_of(band, b0); vb = b - s_band_y0_of(band, b0);
            } else {
                gy0 = y0 + (vhi - b) * (y1 - y0) / (vhi - vlo);
                gy1 = y0 + (vhi - a) * (y1 - y0) / (vhi - vlo);
                va = b - s_band_y0_of(band, b0); vb = a - s_band_y0_of(band, b0);
            }
            ya = gy0 >> s_ysh;
            yb = gy1 >> s_ysh;
            if (yb <= ya)
                yb = ya + 1;
            {
                uint32_t q = ST_NOW();
                put_texrect(d.tex, x0, ya, x1, yb, u0, va, u1, vb, rgb, shaded, semi, abr);
                s_rq[2] += (ST_NOW() - q) & 0xFFFF;
            }
        }
    }
}

static __attribute__((noinline)) void rect_fill(uint32_t w0, uint32_t w1)
{
    int x1 = (int)((w0 >> 12) & 0xFFF) >> 2, y1 = (int)(w0 & 0xFFF) >> 2;
    int x0 = (int)((w1 >> 12) & 0xFFF) >> 2, y0 = (int)(w1 & 0xFFF) >> 2;
    int cyc = (s_omh >> 20) & 3, r, g, b, semi = 0, abr = 0;
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
        Draw d;
        cc_compile();
        combine(&col, WHITE, NULL);
        draw_state(&d, 0, 0, 0);
        r = col.b[0]; g = col.b[1]; b = col.b[2];
        if (d.semi == 1)
            semi = 1;
        else if (d.semi == 2 && (semi = blend_mode(&col, 0, &abr)) < 0)
            return;
    }
    y0 >>= s_ysh;
    y1 >>= s_ysh;
    if (x1 <= x0 || y1 <= y0)
        return;
    if (!(p = pkt_alloc(semi ? 4 : 3)))
        return;
    if (semi) {
        p[1] = 0xE1000600 | (uint32_t)abr << 5;   /* the blend mode */
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
    y0 >>= s_ysh;
    y1 = (y1 + s_ysh) >> s_ysh;
    if (x1 > bw) x1 = bw;
    if (y1 > bh) y1 = bh;
    if (x1 <= x0 || y1 <= y0) { x1 = x0 + 1; y1 = y0 + 1; }
    p[1] = 0xE3000000 | (uint32_t)(by + y0) << 10 | (uint32_t)(bx + x0);
    p[2] = 0xE4000000 | (uint32_t)(by + y1 - 1) << 10 | (uint32_t)(bx + x1 - 1);
    put_flat(p);
}

/* ---- the task ------------------------------------------------------------------------- */
static void frame_begin(int target, int hires)
{
    uint32_t *p;
    int x, y, w, h;
    s_target = target;
    if (target < 0)
        return;
    if (hires != s_hires) {                     /* the display mode follows the image's width */
        gpu_wait();
        gpu_mode(hires);
        s_hires = hires;
        s_ysh = hires;
    }
    fb_rect(target, &x, &y, &w, &h);
    p = pkt_alloc(5);
    gpu_env(p + 1, x, y, w, h);
    s_head = s_tail = p;
    if (s_fbclear[target]) {
        uint32_t *f = pkt_alloc(3);
        s_fbclear[target] = 0;
        if (f) {
            f[1] = 0x02000000;                  /* fill the buffer black */
            f[2] = (uint32_t)y << 16 | (uint32_t)x;
            f[3] = (uint32_t)h << 16 | 640;
            link_after(&s_tail, f);
        }
    }
}

static void frame_end(void)
{
    if (s_target < 0)
        return;
    zrun_close();
    gpu_wait();                                 /* the previous frame is drawn */
    if (s_overflow && s_log)
        tgr_log("rcp: the frame's packets overflowed\n");
    s_overflow = 0;
    gpu_send_list(s_head);
    s_buf ^= 1;
    s_used = 0;
    s_target = -1;
    tgr_rcp_frames++;
}

/* TGR_DLSTATS: commands by opcode, and G_DL targets by how much they hold */
static uint32_t s_opcount[256];
static int s_dlstats = -1;
#define NDLT 512
static struct { uint32_t a; uint32_t calls, cmds, tris, vtx, state, mtx, dl, movemem, depth; } s_dlt[NDLT];
static int s_ndlt;

static void dl_census(uint32_t a, int depth)
{
    int i, guard = 0;
    uint32_t pc = a;
    for (i = 0; i < s_ndlt; i++)
        if (s_dlt[i].a == a) {
            s_dlt[i].calls++;
            return;
        }
    if (s_ndlt == NDLT)
        return;
    i = s_ndlt++;
    memset(&s_dlt[i], 0, sizeof s_dlt[i]);
    s_dlt[i].a = a;
    s_dlt[i].calls = 1;
    s_dlt[i].depth = (uint32_t)depth;
    while (guard++ < 4000) {
        uint32_t w0 = rd32(pc), w1 = rd32(pc + 4);
        int op = (int)(w0 >> 24);
        pc += 8;
        s_dlt[i].cmds++;
        if (op == 0xB8) break;
        if (op == 0xB1) s_dlt[i].tris += 2;
        else if (op == 0xBF) s_dlt[i].tris++;
        else if (op == 0x04) s_dlt[i].vtx++;
        else if (op == 0x01 || op == 0xBD) s_dlt[i].mtx++;
        else if (op == 0x06) { s_dlt[i].dl++; if (((w0 >> 16) & 0xFF) == 1) break; }
        else if (op == 0x03) s_dlt[i].movemem++;
        else if (op == 0xE4 || op == 0xE5) { pc += 16; s_dlt[i].state++; }
        else s_dlt[i].state++;
        (void)w1;
    }
}

/* a display-list command that sets state (everything but the matrices,
 * vertices, triangles, lists and texture rectangles) */
static void exec_state_(int op, uint32_t w0, uint32_t w1);
static void exec_state(int op, uint32_t w0, uint32_t w1)
{
    exec_state_(op, w0, w1);
}

static void exec_state_(int op, uint32_t w0, uint32_t w1)
{
    s_dstamp++;
    switch (op) {
    case 0xB6: s_geom &= ~w1; break;
    case 0xB7: s_geom |= w1; break;
    case 0xB9: case 0xBA: {
        int sh = (w0 >> 8) & 0xFF, len = w0 & 0xFF;
        s_texgen++;
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
        else if (idx == 0x08) {             /* G_MW_FOG */
            s_fog_mul = (int16_t)(w1 >> 16);
            s_fog_ofs = (int16_t)w1;
        }
        else if (idx == 0x0A) {
            int k = ofs / 0x20;
            if ((ofs & 0x1F) == 0 && k < 8) {
                if (s_light[k].col[0] != (uint8_t)(w1 >> 24) || s_light[k].col[1] != (uint8_t)(w1 >> 16) ||
                    s_light[k].col[2] != (uint8_t)(w1 >> 8))
                    s_light_stamp++;            /* (only a change) */
                s_light[k].col[0] = (uint8_t)(w1 >> 24);
                s_light[k].col[1] = (uint8_t)(w1 >> 16);
                s_light[k].col[2] = (uint8_t)(w1 >> 8);
            }
        }
        if (s_nlights < 0) s_nlights = 0;
        if (s_nlights > 7) s_nlights = 7;
        break;
    }
    case 0xED:                                    /* G_SETSCISSOR */
        s_scissor[0] = (int)((w0 >> 12) & 0xFFF) / 4;
        s_scissor[1] = (int)(w0 & 0xFFF) / 4;
        s_scissor[2] = (int)((w1 >> 12) & 0xFFF) / 4;
        s_scissor[3] = (int)(w1 & 0xFFF) / 4;
        scissor(s_scissor[0], s_scissor[1], s_scissor[2], s_scissor[3]);
        break;
    case 0xEF: s_omh = w0 & 0xFFFFFF; s_oml = w1; s_texgen++; break;
    case 0xF0: {                                  /* G_LOADTLUT */
        s_texgen++;
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
        s_texgen++;
        int t = (w1 >> 24) & 7;
        s_tile[t].uls = (w0 >> 12) & 0xFFF;
        s_tile[t].ult = w0 & 0xFFF;
        s_tile[t].lrs = (w1 >> 12) & 0xFFF;
        s_tile[t].lrt = w1 & 0xFFF;
        break;
    }
    case 0xF3: {                                  /* G_LOADBLOCK */
        s_texgen++;
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
        s_texgen++;
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
        s_texgen++;
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
    case 0xF8:
        s_fogc[0] = (uint8_t)(w1 >> 24); s_fogc[1] = (uint8_t)(w1 >> 16);
        s_fogc[2] = (uint8_t)(w1 >> 8); s_fogc[3] = (uint8_t)w1;
        break;
    case 0xF9:
        s_blend[0] = (uint8_t)(w1 >> 24); s_blend[1] = (uint8_t)(w1 >> 16);
        s_blend[2] = (uint8_t)(w1 >> 8); s_blend[3] = (uint8_t)w1;
        break;
    case 0xFA:
        s_prim[0] = (uint8_t)(w1 >> 24); s_prim[1] = (uint8_t)(w1 >> 16);
        s_prim[2] = (uint8_t)(w1 >> 8); s_prim[3] = (uint8_t)w1;
        s_prim_lod = (uint8_t)w0;
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
            int t = fb_index(a), hires = (int)(w0 & 0xFFF) + 1 == 640;
            if (t != s_target || hires != s_hires) {
                frame_end();
                frame_begin(t, hires);
            }
        }
        break;
    }
    default:
        break;
    }
}

/* ---- display lists compiled ---------------------------------------------------------- *
 * The game draws its models (the track's pieces, the cars) by calling the
 * same small display lists frame after frame: vertices, triangles and the
 * state they draw with, under a matrix set by the caller.  Such a list is
 * kept decoded (its commands and vertices in the CPU's byte order), and what
 * each of its triangles draws with that does not hang on the matrix (the
 * primitive, the texels, the colours before fog) is kept from the first
 * time it is drawn, for the state it was called under: the next calls
 * transform the vertices and write the triangles' packets straight away.
 * The state a list reads before it sets it is its call's key; its bytes are
 * checked at every call, its texture sources once a frame. */
enum {
    R_CC = 1 << 0, R_OMH = 1 << 1, R_OML = 1 << 2, R_PRIM = 1 << 3, R_ENV = 1 << 4, R_BLEND = 1 << 5,
    R_FOGC = 1 << 6, R_GEOM = 1 << 7, R_TEX = 1 << 8, R_TIMG = 1 << 9, R_TMEM = 1 << 10,
    R_TDESC = 1 << 11,                          /* ... 8 tiles' descriptors: bits 11-18 */
    R_TSIZE = 1 << 19,                          /* ... 8 tiles' sizes: bits 19-26 */
    R_DRAW = R_CC | R_OMH | R_OML | R_PRIM | R_ENV | R_BLEND | R_FOGC | R_GEOM | R_TEX
};

typedef struct { uint32_t w0, w1; const void *aux; } DCmd;
typedef struct {
    int32_t u[3], v[3];                         /* 1/32 texel from the tile's origin */
    int16_t r[3], g[3], b[3];                   /* the GPU colour before fog */
    uint8_t prim;
    uint8_t kind;                               /* 0 not yet seen, 1 kept, 2 always the slow way */
    uint8_t cull;                               /* the geometry mode's culling when it was last drawn (1 front, 2 back) */
} Tpl;

/* a kept list as a PlayStation mesh: its vertex batches and, for each
 * triangle, its packet with the screen places (and a lit one's colours) left
 * to fill in */
typedef struct {
    uint32_t idx;                               /* a | b << 5 | c << 10 | prim << 15 | flags << 24 */
    uint8_t nw, off;                            /* the packet's words; the first corner's colour word */
    const uint32_t *w;
} NTri;
enum { NT_CULLF = 1, NT_CULLB = 2, NT_LIT = 4, NT_FLAT = 8, NT_ZS = 16, NT_TEX = 32, NT_CUT = 64 };
typedef struct { const NVtx *nv; uint8_t n, v0, lit, pad; uint16_t t0, nt; } NBat;
typedef struct {
    int nbat, nst;
    uint32_t place[48];                         /* each draw state's texture as baked: its key and VRAM place */
    NBat *bat;
    NTri *tri;
    const uint32_t *st;                         /* the list's state commands (w0, w1), run after it draws */
    void *mem;                                  /* (its blocks in the list arena, itself among them) */
} NMesh;
#define DLC_PRIMS 48
typedef struct {
    uint32_t sig, texepoch, frame, stamp, lsig;
    int recorded;                               /* the triangles were kept under this texture epoch */
    Tpl *tpl;
    Prim *prim;                                 /* (room for the list's most: its triangle runs after state) */
    uint32_t *pstamp, *pkey;                    /* (the texture's key: its entry is still it) */
    int nprim, cap;
    uint32_t vlit, vfog;                        /* the vertex batches loaded lit, fogged (by their order) */
    NMesh *nm;                                  /* (made once every triangle's draw is kept) */
} DlcVar;
typedef struct Dlc {
    struct Dlc *next;
    uint32_t addr, sum, qsum, reads, checked;   /* (checked: the frame its bytes were last checked in) */
    int vol;                                    /* (its address's lists change: checked whole every frame) */
    int stable;                                 /* (frames it passed a whole check: from 64, mostly the quick one) */
    int ncmd, ntri, bad, maxprim;
    uint8_t tsa, tsa_on, tsa_tile;              /* (its triangles' texture from the caller's G_TEXTURE: as it was at the compile) */
    DCmd *cmd;
    uint32_t segmask, segval[16];
    DlcVar *var[2];                             /* (made at the list's first draw under a key) */
    int nextvar;
    void *mem;                                  /* (its blocks in the list arena, itself among them; its meshes' are their own) */
} Dlc;

#define DLC_ARENA (1024 * 1024)
#define DLC_HASH 256
static uint32_t s_dlc_arena[DLC_ARENA / 4];
static int s_dlc_used;
static Dlc *s_dlc[DLC_HASH];
static int s_dlc_on = -1;
static uint32_t s_root_dl;                      /* the frame's display list (where the game builds its lists) */
static int s_nm_on = 1;                         /* TGR_NOMESH: kept lists not drawn as meshes */
static uint32_t s_n_dlc_hit, s_n_dlc_tpl, s_n_dlc_rec;  /* TGR_PERF */
/* TGR_PERF counts: why lists were compiled or recorded again (compiled, changed, new key, texture
 * table, source, lights); kept-list triangles without a kept draw; the paths triangles took (off or
 * away, drawn kept, cut, slow, general); why a triangle was never kept; the cut path's outcomes */
static uint32_t s_dlc_why[6], s_dlc_tw[5];
static uint32_t s_path[8];
static uint32_t s_k2why[4];
static uint32_t s_cutw[6];

/* the arena's blocks: sizes in steps of a quarter (or so) of a power of two,
 * each with its owner's chain and its size class in front; a freed block
 * waits on its size's free list for the next of that size */
#define DLC_NCLS 64
typedef struct DBlk { struct DBlk *next; uint32_t cls; } DBlk;
static uint32_t s_dcls[DLC_NCLS];
static int s_ndcls;
static DBlk *s_dfree[DLC_NCLS];
static int s_dlc_live;                          /* TGR_PERF: bytes in blocks held */

static int dlc_evict(void);

static void *dlc_alloc_in(void **chain, int n)
{
    DBlk *b;
    int c, tries = 0;
    if (!s_ndcls) {
        uint32_t z = 16;
        while (s_ndcls < DLC_NCLS && z <= DLC_ARENA) {
            s_dcls[s_ndcls++] = z;
            z = (z + z / 4 + 7) & ~7u;
        }
    }
    n += (int)sizeof(DBlk);
    for (c = 0; c < s_ndcls && s_dcls[c] < (uint32_t)n; c++)
        ;
    if (c == s_ndcls)
        return NULL;
again:
    if ((b = s_dfree[c]) != NULL) {
        s_dfree[c] = b->next;
    } else {
        if (s_dlc_used + (int)s_dcls[c] > DLC_ARENA) {
            if (tries++ < 3 && dlc_evict())
                goto again;                     /* (room made: lists not drawn lately gone) */
            return NULL;
        }
        b = (DBlk *)((uint8_t *)s_dlc_arena + s_dlc_used);
        s_dlc_used += (int)s_dcls[c];
    }
    s_dlc_live += (int)s_dcls[c];
    memset(b + 1, 0, s_dcls[c] - sizeof(DBlk));
    b->cls = (uint32_t)c;
    b->next = (DBlk *)*chain;
    *chain = b;
    return b + 1;
}

/* every block on a chain back to its free list */
static void dlc_free_chain(void *chain)
{
    DBlk *b = (DBlk *)chain, *n;
    for (; b; b = n) {
        n = b->next;
        s_dlc_live -= (int)s_dcls[b->cls];
        b->next = s_dfree[b->cls];
        s_dfree[b->cls] = b;
    }
}

static void nm_free(NMesh *nm)
{
    if (nm)
        dlc_free_chain(nm->mem);
}

/* a list gone (changed under its address): unhooked, its blocks and its meshes' freed */
static void dlc_free(Dlc *d, uint32_t a)
{
    Dlc **pp = &s_dlc[(a >> 3) & (DLC_HASH - 1)];
    int k;
    while (*pp && *pp != d)
        pp = &(*pp)->next;
    if (*pp)
        *pp = d->next;
    for (k = 0; k < 2; k++)
        if (d->var[k])
            nm_free(d->var[k]->nm);
    dlc_free_chain(d->mem);
}

/* the arena full: the lists not drawn for the longest go (a quarter of what
 * is held, the oldest first, none drawn this frame or the last) */
static uint32_t s_dlc_evicted;
static int dlc_evict(void)
{
    static uint32_t age[64];
    int h, i, want = s_dlc_live / 4, freed = 0, n = 0;
    uint32_t cut = 0;
    Dlc *d, *nx;
    memset(age, 0, sizeof age);
    for (h = 0; h < DLC_HASH; h++)
        for (d = s_dlc[h]; d; d = d->next) {
            uint32_t a = s_frame_no - d->checked;
            age[a > 63 ? 63 : a]++;
            n++;
        }
    /* the age from which a quarter of the lists are as old or older */
    for (i = 63, h = 0; i >= 2; i--) {
        h += (int)age[i];
        cut = (uint32_t)i;
        if (h * 4 >= n)
            break;
    }
    if (cut < 2)
        return 0;
    for (h = 0; h < DLC_HASH; h++)
        for (d = s_dlc[h]; d; d = nx) {
            nx = d->next;
            if (s_frame_no - d->checked >= cut) {
                int before = s_dlc_live;
                dlc_free(d, d->addr);
                freed += before - s_dlc_live;
                s_dlc_evicted++;
            }
        }
    (void)want;
    return freed > 0;
}

static uint32_t s_dlc_flushes;
static void dlc_flush(void)
{
    s_dlc_flushes++;
    memset(s_dlc, 0, sizeof s_dlc);
    memset(s_dfree, 0, sizeof s_dfree);
    s_dlc_used = 0;
    s_dlc_live = 0;
}

static uint32_t rot(uint32_t h, uint32_t w) { return (h << 5 | h >> 27) + w; }

/* the list's words and its vertex blocks': all of them, or (quick) each
 * block's first and last vertex */
static uint32_t dlc_sum_(const Dlc *d, int quick);
static uint32_t dlc_sum(const Dlc *d) { return dlc_sum_(d, 0); }

/* addresses whose lists have changed under them: checked whole every frame;
 * one that changed 4 times running (each within two frames of the last) is
 * drawn straight, not kept (the game builds a list there every frame) */
static uint32_t s_dlc_vol[256], s_dlc_volf[256];
static uint8_t s_dlc_voln[256];

static int dlc_vol_slot(uint32_t a)
{
    uint32_t h = (a >> 3) * 2654435761u >> 24;
    int k;
    for (k = 0; k < 8; k++)
        if (s_dlc_vol[(h + k) & 255] == a)
            return (int)((h + k) & 255);
    return -1;
}

static int dlc_volatile(uint32_t a)
{
    uint32_t h = (a >> 3) * 2654435761u >> 24;
    int k;
    for (k = 0; k < 8; k++)
        if (s_dlc_vol[(h + k) & 255] == a)
            return 1;
    return 0;
}

static void dlc_set_volatile(uint32_t a)
{
    uint32_t h = (a >> 3) * 2654435761u >> 24;
    int k;
    for (k = 0; k < 8; k++)
        if (!s_dlc_vol[(h + k) & 255] || s_dlc_vol[(h + k) & 255] == a) {
            h = (h + k) & 255;
            break;
        }
    if (s_dlc_vol[h] != a) {
        s_dlc_vol[h] = a;
        s_dlc_voln[h] = 0;
    } else if (s_dlc_volf[h] + 2 < s_frame_no) {
        s_dlc_voln[h] = 0;                      /* (not in the last two frames: a run starts again; the game's
                                                 * lists of a frame alternate between two places) */
    }
    if (s_dlc_voln[h] < 255)
        s_dlc_voln[h]++;
    s_dlc_volf[h] = s_frame_no;
}

static uint32_t dlc_sum_(const Dlc *d, int quick)
{
    const uint32_t *w = (const uint32_t *)mem(d->addr);
    uint32_t h = 0x9E3779B9u;
    int i;
    for (i = 0; i < d->ncmd * 2; i++)
        h = rot(h, w[i]);
    for (i = 0; i < d->ncmd; i++)
        if ((d->cmd[i].w0 >> 24) == 0x04) {
            const uint32_t *v = (const uint32_t *)mem(d->cmd[i].w1);
            int n = (int)((d->cmd[i].w0 >> 10) & 0x3F);
            int j;
            if (quick) {
                h = rot(rot(rot(rot(h, v[0]), v[1]), v[2]), v[3]);
                v += (n - 1) * 4;
                h = rot(rot(rot(rot(h, v[0]), v[1]), v[2]), v[3]);
            } else {
                for (j = 0; j < n * 4; j += 4) /* (every vertex: the game rewrites some each frame) */
                    h = rot(rot(rot(rot(h, v[j]), v[j + 1]), v[j + 2]), v[j + 3]);
            }
        }
    return h;
}

/* the TMEM words [a, a + n) all filled inside the list */
static int tmem_have(const uint32_t *bits, int a, int n)
{
    int i;
    if (n <= 0)
        return 1;
    for (i = a; i < a + n; i++)
        if (i < 0 || i >= 512 || !(bits[i >> 5] >> (i & 31) & 1))
            return 0;
    return 1;
}

static void tmem_fill(uint32_t *bits, int a, int n)
{
    int i;
    for (i = a; i < a + n && i < 512; i++)
        if (i >= 0)
            bits[i >> 5] |= 1u << (i & 31);
}

static Dlc *dlc_compile(uint32_t a)
{
    Dlc *d;
    uint32_t pc = a, written = 0, reads = 0, tbits[16];
    uint32_t loaded = 0;                        /* the vertex slots the list loads */
    int n = 0, i, k, ntri = 0, bad = 0, textile = -1, runs = 0, chg = 1, tsa = 0;
    struct { int fmt, siz, line, tmem, pal, uls, ult, lrs, lrt; } td[8];
    memset(tbits, 0, sizeof tbits);
    memset(td, 0, sizeof td);
    /* the list's length and whether it is the kind kept */
    while (n < 512) {
        uint32_t w0 = rd32(pc), w1 = rd32(pc + 4);
        int op = (int)(w0 >> 24);
        pc += 8;
        n++;
        if (op == 0xB8)
            break;
        if (op == 0xB1 || op == 0xBF) {
            runs += chg;                        /* (a new draw state at most at each run of triangles after state) */
            chg = 0;
        } else if (op != 0x04) {
            chg = 1;
        }
        switch (op) {
        case 0x04: {
            int vn = (int)((w0 >> 10) & 0x3F), v0 = (int)((w0 >> 16) & 0xFF) / 2;
            reads |= (R_TEX | R_GEOM) & ~written;
            for (k = v0; k < v0 + vn && k < 32; k++)
                loaded |= 1u << k;
            break;
        }
        case 0xB1: case 0xBF: {
            uint32_t need = R_DRAW, idx[2] = { op == 0xB1 ? w0 : w1, w1 };
            int m, nt = op == 0xB1 ? 2 : 1;
            for (m = 0; m < nt; m++)
                for (k = 0; k < 3; k++) {
                    int vi = (int)((idx[m] >> (16 - 8 * k)) & 0xFF) / 2;
                    if (vi >= 32 || !(loaded >> vi & 1))
                        bad = 1;                /* a vertex from before the call */
                }
            ntri += nt;
            if (!(written & R_TEX) && !s_tex.on) {
                tsa = 1;                        /* (texturing off as called: no tile read while it stays so) */
            } else {
                int t, ok = 1, tt = textile;
                if (!(written & R_TEX)) {
                    tt = s_tex.tile;            /* (the caller's tile, as it is now) */
                    tsa = 1;
                }
                for (t = tt; t <= tt + 1 && t < 8; t++) {
                    need |= (R_TDESC << t) | (R_TSIZE << t);
                    if (!((written >> 11 >> t) & 1) || !((written >> 19 >> t) & 1)) {
                        ok = 0;
                        continue;
                    }
                    if (t == tt) {
                        int rows = ((td[t].lrt - td[t].ult) >> 2) + 1, ci = td[t].fmt == 2 || (td[t].fmt == 0 && td[t].siz < 2);
                        if (!tmem_have(tbits, td[t].tmem, td[t].line * (rows < 1 ? 1 : rows)))
                            ok = 0;
                        if (ci && !tmem_have(tbits, 256 + (td[t].siz == 0 ? td[t].pal * 16 : 0), td[t].siz == 0 ? 16 : 256))
                            ok = 0;
                    }
                }
                if (!ok)
                    need |= R_TMEM;
            }
            reads |= need & ~written;
            break;
        }
        case 0xB6: case 0xB7: reads |= R_GEOM & ~written; written |= R_GEOM; break;
        case 0xB9: reads |= R_OML & ~written; written |= R_OML; break;
        case 0xBA: reads |= R_OMH & ~written; written |= R_OMH; break;
        case 0xEF: written |= R_OMH | R_OML; break;
        case 0xBB: written |= R_TEX; textile = (int)((w0 >> 8) & 7); break;
        case 0xBC:
            if ((w0 & 0xFF) == 0x06)
                bad = 1;                        /* a segment set inside: not kept */
            break;
        case 0xFC: written |= R_CC; break;
        case 0xFA: written |= R_PRIM; break;
        case 0xFB: written |= R_ENV; break;
        case 0xF9: written |= R_BLEND; break;
        case 0xF8: written |= R_FOGC; break;
        case 0xFD: written |= R_TIMG; break;
        case 0xF5: {
            int t = (int)((w1 >> 24) & 7);
            td[t].fmt = (int)((w0 >> 21) & 7); td[t].siz = (int)((w0 >> 19) & 3);
            td[t].line = (int)((w0 >> 9) & 0x1FF); td[t].tmem = (int)(w0 & 0x1FF);
            td[t].pal = (int)((w1 >> 20) & 15);
            written |= R_TDESC << t;
            break;
        }
        case 0xF2: {
            int t = (int)((w1 >> 24) & 7);
            td[t].uls = (int)((w0 >> 12) & 0xFFF); td[t].ult = (int)(w0 & 0xFFF);
            td[t].lrs = (int)((w1 >> 12) & 0xFFF); td[t].lrt = (int)(w1 & 0xFFF);
            written |= R_TSIZE << t;
            break;
        }
        case 0xF3: case 0xF4: case 0xF0: {
            int t = (int)((w1 >> 24) & 7), words;
            reads |= (R_TIMG | (R_TDESC << t)) & ~written;
            if (op == 0xF3) {
                int siz = (int)((rd32(pc - 16) >> 19) & 3);   /* (unused: the SETTIMG's size is the state's) */
                int texels = (int)((w1 >> 12) & 0xFFF) - (int)((w0 >> 12) & 0xFFF) + 1;
                (void)siz;
                words = (texels * 2 + 7) / 8 + 1;              /* (at most 16 bits a texel) */
            } else if (op == 0xF4) {
                words = ((int)((w1 & 0xFFF) >> 2) - (int)((w0 & 0xFFF) >> 2) + 1) * td[t].line;
            } else {
                words = (int)(((w1 >> 12) & 0xFFF) >> 2) + 1;
            }
            if ((written >> 11 >> t) & 1)
                tmem_fill(tbits, td[t].tmem, words);
            break;
        }
        case 0x00: case 0xE6: case 0xE7: case 0xE8: case 0xF7: case 0xED:
            break;
        default:
            bad = 1;
            break;
        }
    }
    if (n >= 512)
        bad = 1;
    {
        void *mem = NULL;
        d = (Dlc *)dlc_alloc_in(&mem, (int)sizeof *d);
        if (!d)
            return NULL;
        d->mem = mem;
        d->checked = s_frame_no;                /* (new: not one to evict) */
    }
    d->addr = a;
    d->bad = bad;
    d->next = s_dlc[(a >> 3) & (DLC_HASH - 1)];
    s_dlc[(a >> 3) & (DLC_HASH - 1)] = d;
    if (bad)
        return d;
    d->cmd = (DCmd *)dlc_alloc_in(&d->mem, n * (int)sizeof(DCmd));
    if (!d->cmd) {
        d->bad = 1;
        return d;
    }
    d->ncmd = n;
    d->ntri = ntri;
    d->reads = reads;
    d->maxprim = runs < 1 ? 1 : runs > DLC_PRIMS ? DLC_PRIMS : runs;
    d->tsa = (uint8_t)tsa;
    d->tsa_on = (uint8_t)(s_tex.on != 0);
    d->tsa_tile = (uint8_t)s_tex.tile;
    for (i = 0, pc = a; i < n; i++, pc += 8) {
        uint32_t w0 = rd32(pc), w1 = rd32(pc + 4);
        int op = (int)(w0 >> 24);
        d->cmd[i].w0 = w0;
        d->cmd[i].w1 = w1;
        if (op == 0x04 || op == 0xFD) {
            int sg = (int)((w1 >> 24) & 0xF);
            d->segmask |= 1u << sg;
            d->segval[sg] = s_seg[sg];
            if (op == 0x04)
                d->cmd[i].w1 = seg_addr(w1);
        }
        if (op == 0x04) {
            int vn = (int)((w0 >> 10) & 0x3F), j;
            uint32_t va = d->cmd[i].w1;
            NVtx *nv = (NVtx *)dlc_alloc_in(&d->mem, vn * (int)sizeof(NVtx));
            if (!nv || (va & 3)) {
                d->bad = 1;
                return d;
            }
            for (j = 0; j < vn; j++) {
                uint32_t p = va + (uint32_t)j * 16;
                const uint8_t *c = mem(p + 12);
                nv[j].x = (int16_t)rd16(p); nv[j].y = (int16_t)rd16(p + 2); nv[j].z = (int16_t)rd16(p + 4);
                nv[j].s = (int16_t)rd16(p + 8); nv[j].t = (int16_t)rd16(p + 10);
                nv[j].c[0] = c[0]; nv[j].c[1] = c[1]; nv[j].c[2] = c[2]; nv[j].c[3] = c[3];
            }
            d->cmd[i].aux = nv;
        }
    }
    d->sum = dlc_sum_(d, 0);
    d->qsum = dlc_sum_(d, 1);
    d->vol = dlc_volatile(a);
    return d;
}

/* the state the list reads before it sets it */
static uint32_t dlc_sig(const Dlc *d)
{
    uint32_t reads = d->reads;
    if (d->tsa && ((s_tex.on != 0) != d->tsa_on || s_tex.tile != d->tsa_tile))
        reads |= R_TMEM | (0xFFu * R_TDESC) | (0xFFu * R_TSIZE);   /* (called with another texture tile: all it could read) */
    uint32_t h = 0x811C9DC5u;
    int t;
    if (reads & R_CC) { h = hash32(h, s_cc0); h = hash32(h, s_cc1); }
    if (reads & R_OMH) h = hash32(h, s_omh);
    if (reads & R_OML) h = hash32(h, s_oml);
    if (reads & R_PRIM) h = hash32(h, (uint32_t)s_prim[0] << 24 | s_prim[1] << 16 | s_prim[2] << 8 | (s_prim[3] ^ (uint32_t)s_prim_lod << 7));
    if (reads & R_ENV) h = hash32(h, (uint32_t)s_env[0] << 24 | s_env[1] << 16 | s_env[2] << 8 | s_env[3]);
    if (reads & R_BLEND) h = hash32(h, (uint32_t)s_blend[0] << 24 | s_blend[1] << 16 | s_blend[2] << 8 | s_blend[3]);
    if (reads & R_FOGC) h = hash32(h, (uint32_t)s_fogc[0] << 24 | s_fogc[1] << 16 | s_fogc[2] << 8 | s_fogc[3]);
    if (reads & R_GEOM) h = hash32(h, s_geom);
    if (reads & R_TEX) {
        h = hash32(h, (uint32_t)(s_tex.on << 8 | s_tex.tile << 4 | s_tex.level));
        h = hash32(h, s_tex.ss << 16 | s_tex.ts);
    }
    if (reads & R_TIMG) { h = hash32(h, s_timg); h = hash32(h, (uint32_t)(s_timg_siz << 16 | s_timg_w)); }
    for (t = 0; t < 8; t++) {
        const Tile *tl = &s_tile[t];
        if (reads & (R_TDESC << t)) {
            h = hash32(h, (uint32_t)(tl->fmt << 28 | tl->siz << 26 | tl->line << 16 | tl->tmem << 4 | tl->pal));
            h = hash32(h, (uint32_t)(tl->cmt << 28 | tl->maskt << 24 | tl->shiftt << 20 | tl->cms << 16 |
                                     tl->masks << 12 | tl->shifts << 8));
        }
        if (reads & (R_TSIZE << t))
            h = hash32(h, hash32((uint32_t)(tl->uls << 16 | tl->ult), (uint32_t)(tl->lrs << 16 | tl->lrt)));
    }
    if (reads & R_TMEM) {
        int i;
        for (i = 0; i < s_nloads; i++) {
            const Load *l = &s_loads[i];
            h = hash32(h, l->src);
            h = hash32(h, (uint32_t)(l->kind << 28 | l->siz << 24 | l->w << 12 | l->tmem));
            h = hash32(h, (uint32_t)(l->a0 << 16 | l->a1));
            h = hash32(h, (uint32_t)(l->a2 << 16 | l->a3) ^ (uint32_t)l->line << 7 ^ (uint32_t)l->words << 13);
        }
    }
    return h ? h : 1;
}

/* a triangle's draw kept: what tri_fast works out for it, but the colours before fog */
static void dlc_capture(DlcVar *v, Tpl *t, int ia, int ib, int ic)
{
    Vtx *vv[3];
    int k, p, other;
    if (s_fs.stamp != s_dstamp) {
        s_dlc_tw[3]++;
        if (s_dry) {
            t->kind = 2;
            s_k2why[0]++;
        }                        /* (no draw to keep: the slow way) */
        return;                                 /* the slow path did not get to it: next time */
    }
    other = s_tex.on && uses_other();         /* (the second texture: sampled at the vertices, as triangle() does) */
    if (!s_fs.ok && !other) {
        t->kind = 2; s_k2why[1]++;
        return;
    }
    cc_compile();
    if (s_fs.pr.fog)
        for (k = 0; k < 3; k++)
            if (s_cc.kind[0][2][k] == K_SHADE && s_cc.val[0][2][k] == 3) {
                t->kind = 2; s_k2why[2]++;                    /* the colour uses the shade alpha, the fog's */
                return;
            }
    for (p = 0; p < v->nprim; p++)
        if (v->pstamp[p] == s_fs.stamp)
            break;
    if (p == v->nprim) {
        if (p == v->cap) {
            t->kind = 2; s_k2why[3]++;
            return;
        }
        v->prim[p] = s_fs.pr;
        v->pstamp[p] = s_fs.stamp;
        v->prim[p].flat = s_fs.flat;
        if (s_geom & G_LIGHTING)
            v->lsig = s_lsig;
        v->prim[p].lit = 0;
        if ((s_geom & G_LIGHTING) && !other) {
            /* the GPU colour from the shade, worked out each frame from the GTE's
             * lighting: the combiner's result is a straight line in the shade
             * (checked at its middle) */
            int c, j, e[3][3];
            static const uint8_t lv[3] = { 0, 128, 255 };
            for (j = 0; j < 3; j++) {
                uint8_t sh[4];
                Lin col;
                sh[0] = sh[1] = sh[2] = lv[j];
                sh[3] = s_vtx[ia].a;
                combine(&col, sh, NULL);
                for (c = 0; c < 3; c++)
                    e[j][c] = s_fs.textured && s_fs.mask ? col.b[c] >> 1 : s_fs.textured ? (col.a[c] + col.b[c]) >> 1 : col.b[c];
            }
            v->prim[p].lit = 1;
            for (c = 0; c < 3; c++) {
                v->prim[p].c0[c] = (int16_t)e[0][c];
                v->prim[p].sl[c] = (int16_t)(e[2][c] - e[0][c]);
                if (abs(e[0][c] + ((e[2][c] - e[0][c]) * 128 >> 8) - e[1][c]) > 3)
                    v->prim[p].lit = 0;         /* not a line: kept colours (the lights stay put) */
            }
        }
        v->pkey[p] = s_fs.pr.tex ? s_fs.pr.tex->key : 0;
        v->nprim++;
    }
    vv[0] = &s_vtx[ia]; vv[1] = &s_vtx[ib]; vv[2] = &s_vtx[ic];
    for (k = 0; k < 3; k++) {
        const Vtx *q = s_fs.flat ? vv[0] : vv[k];
        uint8_t sh[4];
        Lin col;
        int r, g, b;
        sh[0] = q->r; sh[1] = q->g; sh[2] = q->b; sh[3] = vv[k]->a;
        if (other) {
            uint8_t ot[4];
            sample((s_tex.tile + 1) & 7, vv[k]->s, vv[k]->t, ot);
            combine(&col, sh, ot);
        } else if (!s_fs.flat && vv[k]->cc_stamp == s_cc_stamp) {
            col = vv[k]->col;                   /* (the dry run's: the same combiner, the same shade) */
        } else {
            combine(&col, sh, NULL);
        }
        if (s_fs.textured && s_fs.mask) {
            r = col.b[0] >> 1; g = col.b[1] >> 1; b = col.b[2] >> 1;
        } else if (s_fs.textured) {
            r = (col.a[0] + col.b[0]) >> 1; g = (col.a[1] + col.b[1]) >> 1; b = (col.a[2] + col.b[2]) >> 1;
        } else {
            r = col.b[0]; g = col.b[1]; b = col.b[2];
        }
        t->r[k] = (int16_t)r; t->g[k] = (int16_t)g; t->b[k] = (int16_t)b;
        if (s_fs.textured) {
            t->u[k] = tc32(vv[k]->s, s_fs.s0, s_fs.shifts) << s_fs.pr.tex->up;
            t->v[k] = tc32(vv[k]->t, s_fs.t0, s_fs.shiftt) << s_fs.pr.tex->up;
        } else {
            t->u[k] = t->v[k] = 0;
        }
    }
    t->prim = (uint8_t)p;
    t->kind = 1;
}

static int edge_split(int32_t wa, int32_t wb, int ax, int ay, int bx, int by);

/* a kept triangle drawn from its vertices' short records (all on the GTE's
 * screen, inside the guard band) */
/* a kept corner's GPU colour: kept, or from its vertex's shade by the draw's line */
static void corner_rgb(const Tpl *t, const Prim *pr, const MVtx *m, int k, int *r, int *g, int *b)
{
    if (pr->lit) {
        *r = pr->c0[0] + (pr->sl[0] * m->r >> 8);
        *g = pr->c0[1] + (pr->sl[1] * m->g >> 8);
        *b = pr->c0[2] + (pr->sl[2] * m->b >> 8);
    } else {
        *r = t->r[k]; *g = t->g[k]; *b = t->b[k];
    }
}

static void dlc_tri(const Tpl *t, const Prim *pr, const MVtx *v0, const MVtx *v1, const MVtx *v2)
{
    const MVtx *v[3];
    SV sv[3];
    int k;
    s_n_tri++;
    if (v0->clip & v1->clip & v2->clip) {
        s_n_rej++;
        return;
    }
    s_n_dlc_tpl++;
    {
        int32_t det = (v1->x - v0->x) * (v2->y - v0->y) - (v2->x - v0->x) * (v1->y - v0->y);
        if (((s_geom & G_CULL_BACK) && det >= 0) || ((s_geom & G_CULL_FRONT) && det <= 0)) {
            s_n_cull++;
            return;
        }
    }
    v[0] = v0; v[1] = v1; v[2] = v2;
    if (!pr->window && !pr->fog && !pr->semi) {
        int minx = v0->x, maxx = v0->x, miny = v0->y, maxy = v0->y;
        for (k = 1; k < 3; k++) {
            if (v[k]->x < minx) minx = v[k]->x;
            if (v[k]->x > maxx) maxx = v[k]->x;
            if (v[k]->y < miny) miny = v[k]->y;
            if (v[k]->y > maxy) maxy = v[k]->y;
        }
        if (maxx - minx <= 1000 && maxy - miny <= 500) {
            uint32_t *p = pkt_alloc(pr->textured ? 9 : 6), *w;
            if (!p)
                return;
            w = p + 1;
            for (k = 0; k < 3; k++) {
                int r, g, b;
                corner_rgb(t, pr, pr->flat ? v0 : v[k], k, &r, &g, &b);
                w[0] = (uint32_t)clamp255(r) | (uint32_t)clamp255(g) << 8 | (uint32_t)clamp255(b) << 16;
                w[1] = (uint32_t)(uint16_t)v[k]->y << 16 | (uint16_t)v[k]->x;
                if (pr->textured) {
                    int u = t->u[k] >> 5, vv = t->v[k] >> 5;
                    u = pr->ub + (u < 0 ? 0 : u >= pr->tex->w ? pr->tex->w - 1 : u);
                    vv = pr->vb + (vv < 0 ? 0 : vv >= pr->tex->h ? pr->tex->h - 1 : vv);
                    w[2] = (uint32_t)(u & 255) | (uint32_t)(vv & 255) << 8;
                    w += 3;
                } else {
                    w += 2;
                }
            }
            p[1] |= (uint32_t)pr->cmd << 24;
            if (pr->textured) {
                p[3] |= (uint32_t)pr->tex->clut << 16;
                p[6] |= (uint32_t)tex_tpage(pr->tex, pr->tex->vx) << 16;
            }
            s_n_piece++;
            if (pr->zsort)
                put_sorted(p, (v0->z + v1->z + v2->z) / 3);
            else
                put_flat(p);
            return;
        }
    }
    for (k = 0; k < 3; k++) {
        sv[k].x = v[k]->x; sv[k].y = v[k]->y; sv[k].z = v[k]->z;
        sv[k].u = t->u[k]; sv[k].v = t->v[k];
        sv[k].fr = sv[k].fg = sv[k].fb = 0;
        {
            int r, g, b;
            corner_rgb(t, pr, pr->flat ? v0 : v[k], k, &r, &g, &b);
            sv[k].r = (int16_t)r; sv[k].g = (int16_t)g; sv[k].b = (int16_t)b;
        }
    }
    emit_tri(&sv[0], &sv[1], &sv[2], pr, 0);
}

/* a kept list's vertex in clip space and its clip flags, for the clip and
 * cut path (no colour: the kept draw has it): the GTE's MACs when it took
 * the vertex, else the CPU's product */

static void vtx_clip(int slot, CV *c, int *clip)
{
    const NVtx *nv = s_mvsrc[slot];

    int32_t x = nv->x, y = nv->y, z = nv->z, cx, cy, cw;
    int k = 0;
    if (s_mvx[slot].ok) {
        int sh = 8 + s_gte_k;
        gte_mtc(GTE_VXY0, (uint32_t)(y << 16) | (uint32_t)(x & 0xFFFF));
        gte_mtc(GTE_VZ0, (uint32_t)z);
        gte_rtps();
        cw = sh_r((int32_t)gte_mfc(GTE_MAC3), -sh);
        cx = sh_r((int32_t)gte_mfc(GTE_MAC1), -sh);
        cy = (int32_t)(((int64_t)sh_r((int32_t)gte_mfc(GTE_MAC2), -sh) * s_gte_ify) >> 16);
        c->z = 0;
    } else {
        const M4 *m = &s_mvp;
        cx = (int32_t)(((int64_t)x * m->m[0][0] + (int64_t)y * m->m[1][0] + (int64_t)z * m->m[2][0] + m->m[3][0]) >> 8);
        cy = (int32_t)(((int64_t)x * m->m[0][1] + (int64_t)y * m->m[1][1] + (int64_t)z * m->m[2][1] + m->m[3][1]) >> 8);
        c->z = (int32_t)(((int64_t)x * m->m[0][2] + (int64_t)y * m->m[1][2] + (int64_t)z * m->m[2][2] + m->m[3][2]) >> 8);
        cw = (int32_t)(((int64_t)x * m->m[0][3] + (int64_t)y * m->m[1][3] + (int64_t)z * m->m[2][3] + m->m[3][3]) >> 8);
    }
    if (cw < W_NEAR) k |= 16;
    if (cx < -cw) k |= 1;
    if (cx > cw) k |= 2;
    if (cy < -cw) k |= 4;
    if (cy > cw) k |= 8;
    if (abs(cx) > GUARD * cw || abs(cy) > GUARD * cw)
        k |= 32;
    c->x = cx; c->y = cy; c->w = cw;
    c->mx = x * 16; c->my = y * 16; c->mz = z * 16;
    *clip = k;
}

/* a kept triangle the GPU cannot take whole (reaching behind the eye, past
 * the guard band, or deep enough to need cutting): clipped and cut from its
 * kept draw */
static uint32_t s_cwt[6];                       /* TGR_PERF: the clip path's time by outcome (cycles / 8) */
static void dlc_tri_clip_(const Tpl *t, const Prim *pr, int ia, int ib, int ic, int cull);
static void dlc_tri_clip(const Tpl *t, const Prim *pr, int ia, int ib, int ic, int cull)
{
    uint32_t q = ST_NOW(), w0[6];
    memcpy(w0, s_cutw, sizeof w0);
    dlc_tri_clip_(t, pr, ia, ib, ic, cull);
    {
        int k;
        for (k = 0; k < 6; k++)
            if (s_cutw[k] != w0[k])
                s_cwt[k] += (ST_NOW() - q) & 0xFFFF;
    }
}
static void dlc_tri_clip_(const Tpl *t, const Prim *pr, int ia, int ib, int ic, int cull)
{
    CV poly[2][12];
    int k, n = 3, cur = 0, cl[3];
    const int idx[3] = { ia, ib, ic };
    s_n_tri++;
    if (pr->fog) {                              /* (the fog wants the full vertices' depth cue) */
        mesh_fill(ia);
        mesh_fill(ib);
        mesh_fill(ic);
        for (k = 0; k < 3; k++) {
            const Vtx *q = &s_vtx[idx[k]];
            CV *c = &poly[0][k];
            c->x = q->cx; c->y = q->cy; c->z = q->cz; c->w = q->cw;
            c->mx = q->mx * 16; c->my = q->my * 16; c->mz = q->mz * 16;
            cl[k] = q->clip;
        }
    } else {
        for (k = 0; k < 3; k++)
            vtx_clip(idx[k], &poly[0][k], &cl[k]);
    }
    if (cl[0] & cl[1] & cl[2] & 31) {           /* off one side or behind the eye: first, before any colour */
        s_n_rej++;
        s_cutw[0]++;
        return;
    }
    {
        const CV *v0 = &poly[0][0], *v1 = &poly[0][1], *v2 = &poly[0][2];
        int64_t det = (int64_t)v0->x * (((int64_t)v1->y * v2->w - (int64_t)v2->y * v1->w) >> 8) -
                      (int64_t)v0->y * (((int64_t)v1->x * v2->w - (int64_t)v2->x * v1->w) >> 8) +
                      (int64_t)v0->w * (((int64_t)v1->x * v2->y - (int64_t)v2->x * v1->y) >> 8);
        if (((cull & 2) && det <= 0) || ((cull & 1) && det >= 0)) {
            s_cutw[1]++;
            return;
        }
    }
    s_cutw[2 + !!((cl[0] | cl[1] | cl[2]) & 16) + 2 * !!((cl[0] | cl[1] | cl[2]) & 32)]++;
    for (k = 0; k < 3; k++) {
        CV *c = &poly[0][k];
        int r, g, b;
        corner_rgb(t, pr, &s_mvx[pr->flat ? ia : idx[k]], k, &r, &g, &b);
        c->u = t->u[k]; c->v = t->v[k];
        c->r = r; c->g = g; c->b = b;
        c->fr = c->fg = c->fb = 0;
        if (pr->fog) {
            int f = s_vtx[idx[k]].a, nf = 255 - f;
            c->r = c->r * nf / 255; c->g = c->g * nf / 255; c->b = c->b * nf / 255;
            c->fr = s_fogc[0] * f / 255; c->fg = s_fogc[1] * f / 255; c->fb = s_fogc[2] * f / 255;
        }
    }
    {
    RG_BEGIN(RG_CUTCLIP);
    {
        int pl, any = (cl[0] | cl[1] | cl[2]) & 16, far = (cl[0] | cl[1] | cl[2]) & 32;
        for (pl = 0; pl < 5 && n >= 3; pl++) {
            if (pl == 0 && !any)
                continue;
            if (pl > 0 && !far && !any)
                break;
            n = clip_plane(poly[cur], n, poly[cur ^ 1], pl);
            s_n_clip++;
            cur ^= 1;
        }
    }
    if (n < 3)
        return;
    RG_END(RG_CUTCLIP);
    }
    {
    RG_BEGIN(RG_CUTEMIT);
    for (k = 1; k + 1 < n; k++)
        emit_cv(&poly[cur][0], &poly[cur][k], &poly[cur][k + 1], pr, 0);
    RG_END(RG_CUTEMIT);
    }
}

/* ---- kept lists as PlayStation meshes ------------------------------------------------- */
static int nm_why(const Dlc *d, const DlcVar *v)    /* (TGR_PERF: why a variant has no mesh) */
{
    int i;
    if (v->vfog) return 0;
    for (i = 0; i < d->ntri; i++) {
        if (v->tpl[i].kind == 0) return 1;
        if (v->tpl[i].kind == 2) return 2;
        if (v->prim[v->tpl[i].prim].fog) return 3;
    }
    return 4;
}

/* the mesh of a variant whose every triangle has its draw kept (none fogged):
 * NULL while it cannot be one */
/* a mesh's state commands (run after it draws, for what comes next) cut to
 * their net effect, in place: the geometry mode and the two other-mode words
 * each one and/or (0x00 pairs at the end: the and, then the or applied); a
 * colour, combine, texture or fill command dropped when a later one of its
 * kind follows; a tile's size when a later size of the tile follows; a
 * tile's descriptor or the texture image when a later one follows with no
 * load between that reads it; loads and the rest kept in order */
static int st_compress(uint32_t *st, int n)
{
    uint8_t keep[600];
    uint32_t gand = ~0u, gor = 0, oand[2] = { ~0u, ~0u }, oor[2] = { 0, 0 };
    int i, k, m = 0, hg = 0, ho[2] = { 0, 0 }, f5[8], f2[8], fd = 0;
    uint8_t seen[256];
    if (n > 600)
        return n;
    memset(seen, 0, sizeof seen);
    memset(f5, 0, sizeof f5);
    memset(f2, 0, sizeof f2);
    for (i = n - 1; i >= 0; i--) {              /* (backwards: what follows each command) */
        uint32_t w0 = st[i * 2], w1 = st[i * 2 + 1];
        int op = (int)(w0 >> 24), t = (int)((w1 >> 24) & 7);
        keep[i] = 1;
        switch (op) {
        case 0xB6: case 0xB7: case 0xB9: case 0xBA: case 0xEF:
            keep[i] = 0;                        /* (composed below) */
            break;
        case 0xFC: case 0xFA: case 0xFB: case 0xF9: case 0xF8: case 0xF7: case 0xBB:
            if (seen[op])
                keep[i] = 0;
            seen[op] = 1;
            break;
        case 0xF2:
            if (f2[t])
                keep[i] = 0;
            f2[t] = 1;
            break;
        case 0xF5:
            if (f5[t])
                keep[i] = 0;
            f5[t] = 1;
            break;
        case 0xFD:
            if (fd)
                keep[i] = 0;
            fd = 1;
            break;
        case 0xF3: case 0xF4: case 0xF0:
            f5[t] = 0;                          /* (it reads its tile's descriptor and the image) */
            fd = 0;
            break;
        default:
            break;
        }
    }
    for (i = 0; i < n; i++) {                   /* (forwards: the modes composed) */
        uint32_t w0 = st[i * 2], w1 = st[i * 2 + 1];
        int op = (int)(w0 >> 24);
        if (op == 0xB6) {
            gand &= ~w1;
            gor &= ~w1;
            hg = 1;
        } else if (op == 0xB7) {
            gor |= w1;
            hg = 1;
        } else if (op == 0xB9 || op == 0xBA) {
            int sh = (w0 >> 8) & 0xFF, len = w0 & 0xFF, r = op == 0xBA;
            uint32_t mask = (len >= 32 ? 0xFFFFFFFFu : ((1u << len) - 1)) << sh;
            oand[r] &= ~mask;
            oor[r] = (oor[r] & ~mask) | w1;
            ho[r] = 1;
        } else if (op == 0xEF) {
            oand[0] = oand[1] = 0;
            oor[1] = w0 & 0xFFFFFF;
            oor[0] = w1;
            ho[0] = ho[1] = 1;
        }
    }
    for (i = 0; i < n; i++)
        if (keep[i]) {
            st[m * 2] = st[i * 2];
            st[m * 2 + 1] = st[i * 2 + 1];
            m++;
        }
    for (k = 0; k < 3; k++) {
        uint32_t a = k == 0 ? gand : oand[k - 1], o = k == 0 ? gor : oor[k - 1];
        if (!(k == 0 ? hg : ho[k - 1]))
            continue;
        st[m * 2] = (uint32_t)(k + 1);          /* (0x00: 1 geometry, 2 other-mode low, 3 high) */
        st[m * 2 + 1] = a;
        m++;
        st[m * 2] = 0x100u | (uint32_t)(k + 1);
        st[m * 2 + 1] = o;
        m++;
    }
    return m;
}

/* a mesh's state commands run (st_compress's form) */
static void st_replay(const uint32_t *st, int n)
{
    uint32_t a = 0;
    int i;
    for (i = 0; i < n; i++) {
        uint32_t w0 = st[i * 2], w1 = st[i * 2 + 1];
        if (w0 >> 24)
            exec_state((int)(w0 >> 24), w0, w1);
        else if (!(w0 & 0x100))
            a = w1;
        else if ((w0 & 3) == 1)
            s_geom = (s_geom & a) | w1;
        else if ((w0 & 3) == 2)
            s_oml = (s_oml & a) | w1;
        else
            s_omh = (s_omh & a) | w1;
    }
    s_dstamp++;
    s_texgen++;
}

static NMesh *nm_build(const Dlc *d, const DlcVar *v)
{
    NMesh *nm;
    uint32_t *words;
    int i, k, nb = 0, nst = 0, ti = 0, b = -1;
    if (v->vfog || !d->ntri)
        return NULL;
    for (i = 0; i < d->ntri; i++)
        if (v->tpl[i].kind != 1 || v->prim[v->tpl[i].prim].fog)
            return NULL;
    for (i = 0; i < d->ncmd; i++) {
        int op = (int)(d->cmd[i].w0 >> 24);
        if (op == 0x04)
            nb++;
        else if (op != 0xB1 && op != 0xBF && op != 0xB8 && op != 0xE6 && op != 0xE7 && op != 0xE8 && op != 0x00)
            nst++;
    }
    if (nb > 32)
        return NULL;
    {
        void *mem = NULL;
        nm = (NMesh *)dlc_alloc_in(&mem, (int)sizeof(NMesh));
        if (!nm)
            return NULL;
        nm->bat = (NBat *)dlc_alloc_in(&mem, nb * (int)sizeof(NBat));
        nm->tri = (NTri *)dlc_alloc_in(&mem, d->ntri * (int)sizeof(NTri));
        words = (uint32_t *)dlc_alloc_in(&mem, d->ntri * 12 * 4);
        nm->st = (const uint32_t *)dlc_alloc_in(&mem, (nst + 6) * 8 + 4);   /* (and room for the composed modes) */
        nm->mem = mem;
        if (!nm->bat || !nm->tri || !words || !nm->st) {
            nm_free(nm);
            return NULL;
        }
    }
    nm->nbat = nb;
    nm->nst = nst;
    nst = 0;
    for (k = 0; k < v->nprim && k < 48; k++) {
        const Tex *tx = v->prim[k].tex;
        nm->place[k] = tx ? tx->key ^ ((uint32_t)tx->vx << 20 | (uint32_t)tx->vy << 10 | tx->clut) * 2654435761u : 0;
    }
    for (i = 0; i < d->ncmd; i++) {
        uint32_t w0 = d->cmd[i].w0, w1 = d->cmd[i].w1;
        int op = (int)(w0 >> 24), m, nt;
        if (op == 0x04) {
            NBat *bt = &nm->bat[++b];
            bt->nv = (const NVtx *)d->cmd[i].aux;
            bt->n = (uint8_t)((w0 >> 10) & 0x3F);
            bt->v0 = (uint8_t)(((w0 >> 16) & 0xFF) / 2);
            bt->lit = (uint8_t)(v->vlit >> b & 1);
            bt->t0 = (uint16_t)ti;
            bt->nt = 0;
            continue;
        }
        if (op != 0xB1 && op != 0xBF) {
            if (op != 0xB8 && op != 0xE6 && op != 0xE7 && op != 0xE8 && op != 0x00) {
                ((uint32_t *)nm->st)[nst * 2] = w0;
                ((uint32_t *)nm->st)[nst * 2 + 1] = w1;
                nst++;
            }
            continue;
        }
        if (b < 0)
            return NULL;
        nt = op == 0xB1 ? 2 : 1;
        for (m = 0; m < nt; m++, ti++) {
            uint32_t x = op == 0xB1 && m == 0 ? w0 : w1;
            int ia = (int)((x >> 16) & 0xFF) / 2, ib = (int)((x >> 8) & 0xFF) / 2, ic = (int)(x & 0xFF) / 2;
            const Tpl *t = &v->tpl[ti];
            const Prim *pr = &v->prim[t->prim];
            NTri *nt3 = &nm->tri[ti];
            uint32_t *w = words + ti * 12, fl = t->cull;
            int n = 0, su = 0, sv = 0;
            nm->bat[b].nt++;
            if (pr->lit && nm->bat[b].lit) fl |= NT_LIT;
            if (pr->flat) fl |= NT_FLAT;
            if (pr->zsort) fl |= NT_ZS;
            if (pr->textured) fl |= NT_TEX;
            if (pr->textured && pr->tex) {
                int32_t mnu = t->u[0], mxu = t->u[0], mnv = t->v[0], mxv = t->v[0];
                for (k = 1; k < 3; k++) {
                    if (t->u[k] < mnu) mnu = t->u[k];
                    if (t->u[k] > mxu) mxu = t->u[k];
                    if (t->v[k] < mnv) mnv = t->v[k];
                    if (t->v[k] > mxv) mxv = t->v[k];
                }
                if ((pr->wraps && mxu - mnu > 192 * 32) || (pr->wrapt && mxv - mnv > 192 * 32))
                    fl |= NT_CUT;               /* (wider than a page: split by the cut path) */
                su = pr->wraps ? (mnu >> 5) & ~(pr->tex->w - 1) : 0;
                sv = pr->wrapt ? (mnv >> 5) & ~(pr->tex->h - 1) : 0;
            }
            if (pr->semi)
                w[n++] = 0xE1000000 | 0x200 | 0x400 | (uint32_t)pr->abr << 5 |
                         (pr->tex ? tex_tpage(pr->tex, pr->tex->vx) & 0x1FF : 0);
            if (pr->window)
                w[n++] = pr->window;
            nt3->off = (uint8_t)n;
            for (k = 0; k < 3; k++) {
                uint32_t rgb = (uint32_t)clamp255(t->r[k]) | (uint32_t)clamp255(t->g[k]) << 8 | (uint32_t)clamp255(t->b[k]) << 16;
                if (k == 0)
                    rgb |= (uint32_t)pr->cmd << 24;
                w[n++] = rgb;
                w[n++] = 0;                     /* (the screen place) */
                if (pr->textured && pr->tex) {
                    int u = t->u[k] >> 5, vv = t->v[k] >> 5;
                    u = pr->wraps ? u - su : pr->ub + (u < 0 ? 0 : u >= pr->tex->w ? pr->tex->w - 1 : u);
                    vv = pr->wrapt ? vv - sv : pr->vb + (vv < 0 ? 0 : vv >= pr->tex->h ? pr->tex->h - 1 : vv);
                    w[n] = (uint32_t)clamp255(u) | (uint32_t)clamp255(vv) << 8;
                    if (k == 0)
                        w[n] |= (uint32_t)pr->tex->clut << 16;
                    if (k == 1)
                        w[n] |= (uint32_t)(tex_tpage(pr->tex, pr->tex->vx) | pr->abr << 5) << 16;
                    n++;
                }
            }
            if (pr->window)
                w[n++] = 0xE2000000;
            nt3->nw = (uint8_t)n;
            nt3->w = w;
            nt3->idx = (uint32_t)ia | (uint32_t)ib << 5 | (uint32_t)ic << 10 | (uint32_t)t->prim << 15 | fl << 24;
        }
    }
    nm->nst = st_compress((uint32_t *)nm->st, nm->nst);
    return nm;
}

/* a kept list drawn as its mesh: 0 when the GTE does not hold a transform */
/* edge_cuts for the mesh loop, inline */
static inline __attribute__((always_inline)) int ecut(int32_t wa, int32_t wb, int ax, int ay, int bx, int by)
{
    int32_t l = abs(ax - bx) + abs(ay - by), d, sum, e;
    int k;
    if (l <= 6)
        return 0;
    if (wa < W_NEAR) wa = W_NEAR;
    if (wb < W_NEAR) wb = W_NEAR;
    d = abs(wb - wa);
    sum = wa + wb;
    if (sum >= 0x10000) {                       /* (the halvings that bring sum under 0x10000, at once) */
        int sh = sum >= 0x1000000 ? (sum >= 0x10000000 ? (sum >= 0x40000000 ? 15 : 13) : (sum >= 0x4000000 ? 11 : 9))
                                  : (sum >= 0x100000 ? (sum >= 0x400000 ? 7 : 5) : (sum >= 0x40000 ? 3 : 1));
        if (sum >> sh >= 0x10000)
            sh++;
        sum >>= sh;
        d >>= sh;
    }
    e = l * d;
    for (k = 0; k < 2 && e > 6 * sum; k++)
        e >>= 2;
    return k;
}

static int nm_draw(const Dlc *d, DlcVar *v)
{
    const NMesh *nm = v->nm;
    int b, i;
    uint32_t pieces0 = s_n_piece, cut0 = s_path[2], rej0 = s_n_rej, tri0 = s_n_tri, mv0 = s_n_vtx;
    for (b = 0; b < v->nprim && b < 48; b++) {    /* (the textures where they were baked) */
        const Tex *tx = v->prim[b].tex;
        if (tx && (tx->key ^ ((uint32_t)tx->vx << 20 | (uint32_t)tx->vy << 10 | tx->clut) * 2654435761u) != nm->place[b]) {
            nm_free(v->nm);
            v->nm = NULL;
            return 0;
        }
    }
    mvp_update();
    if (s_gte_stamp != s_mvp_stamp || s_gte_ysh != s_ysh)
        gte_setup();
    if (!s_gte_ok)
        return 0;
    for (b = 0; b < nm->nbat; b++) {
        const NBat *bt = &nm->bat[b];
        const NTri *tr = &nm->tri[bt->t0];
        {
            uint32_t c0 = *(volatile uint32_t *)0xBF801120;
            mesh_vertices(bt->nv, bt->n, bt->v0, bt->lit);
            ST_ADD(4, c0);
            s_flagbits[8] += (*(volatile uint32_t *)0xBF801120 - c0) & 0xFFFF;
            s_flagbits[10] += bt->n;
        }
        {
        uint32_t tl0 = ST_NOW();
        const Prim *prims = v->prim;
        const Tpl *tpls = v->tpl + bt->t0;
        uint32_t *base = s_pkt[s_buf], *pk = base + s_used, *pend = base + PKT_WORDS - 16;
        int lo = s_ot_lo, hi = s_ot_hi, zr = s_zrun;
        uint32_t n_rej = 0, n_cull = 0, n_piece = 0, n_cut = 0, n_tri = 0;
        /* (the packet cursor and the depth run in registers: written back round anything that adds packets) */
#define NM_OUT() (s_used = (int)(pk - base), s_ot_lo = lo, s_ot_hi = hi, s_zrun = zr)
#define NM_IN() (pk = base + s_used, lo = s_ot_lo, hi = s_ot_hi, zr = s_zrun)
        for (i = 0; i < bt->nt; i++, tr++) {
            uint32_t idx = tr->idx, fl = idx >> 24, ka, kb, kc, xa, xb, xc;
            int ia = (int)(idx & 31), ib = (int)(idx >> 5 & 31), ic = (int)(idx >> 10 & 31);
            const MVtx *A = &s_mvx[ia], *B = &s_mvx[ib], *C = &s_mvx[ic];
            const Prim *pr;
            uint32_t *p, *w;
            int32_t det, ax, ay, bx, by, cx, cy;
            n_tri++;
            ka = *(const uint16_t *)&A->clip;  /* (clip, and ok above it) */
            kb = *(const uint16_t *)&B->clip;
            kc = *(const uint16_t *)&C->clip;
            if (ka & kb & kc & 31) {
                n_rej++;
                continue;
            }
            if (!(ka & kb & kc & 0x100) || (fl & NT_CUT)) {
                uint32_t cc = ST_NOW();
                NM_OUT();
                s_n_tri--;
                dlc_tri_clip(&tpls[i], &prims[idx >> 15 & 63], ia, ib, ic, (int)(fl & 3));
                NM_IN();
                s_cutc += (ST_NOW() - cc) & 0xFFFF;
                n_cut++;
                continue;
            }
            xa = *(const uint32_t *)A; xb = *(const uint32_t *)B; xc = *(const uint32_t *)C;
            ax = (int16_t)xa; ay = (int32_t)xa >> 16;
            bx = (int16_t)xb; by = (int32_t)xb >> 16;
            cx = (int16_t)xc; cy = (int32_t)xc >> 16;
            det = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
            if (((fl & NT_CULLB) && det >= 0) || ((fl & NT_CULLF) && det <= 0)) {
                n_cull++;
                continue;
            }
            pr = &prims[idx >> 15 & 63];
            if (fl & NT_TEX) {
                int lg = ecut(A->w, B->w, ax, ay, bx, by), k1 = ecut(B->w, C->w, bx, by, cx, cy),
                    k2 = ecut(C->w, A->w, cx, cy, ax, ay);
                if (k1 > lg) lg = k1;
                if (k2 > lg) lg = k2;
                if (lg) {
                    /* deep enough to need cutting: its grid straight from the vertex records
                     * and the kept draw, else the clip path */
                    uint32_t cc = ST_NOW();
                    int done = 0;
                    NM_OUT();
                    if (!pr->fog && pr->tex && lg <= 2) {
                        const Tpl *tp = &tpls[i];
                        CV q[3];
                        const MVtx *m3[3];
                        const int ix[3] = { ia, ib, ic };
                        int k;
                        m3[0] = A; m3[1] = B; m3[2] = C;
                        for (k = 0; k < 3; k++) {
                            const NVtx *nv = s_mvsrc[ix[k]];
                            int r, g, bb;
                            corner_rgb(tp, pr, fl & NT_FLAT ? A : m3[k], k, &r, &g, &bb);
                            q[k].mx = nv->x * 16; q[k].my = nv->y * 16; q[k].mz = nv->z * 16;
                            q[k].x = q[k].y = q[k].z = 0;
                            q[k].w = m3[k]->w;
                            q[k].u = tp->u[k]; q[k].v = tp->v[k];
                            q[k].r = r; q[k].g = g; q[k].b = bb;
                            q[k].fr = q[k].fg = q[k].fb = 0;
                        }
                        s_grid_strict = 1;
                        done = emit_grid_words(&q[0], &q[1], &q[2], pr, lg);
                        s_grid_strict = 0;
                        s_gridn[done ? 0 : 2]++;
                    } else if (!pr->fog && pr->tex) {
                        s_gridn[1]++;
                    }
                    if (!done) {
                        s_n_tri--;
                        dlc_tri_clip(&tpls[i], pr, ia, ib, ic, (int)(fl & 3));
                    } else {
                        s_n_tri++;
                    }
                    NM_IN();
                    s_cutc += (ST_NOW() - cc) & 0xFFFF;
                    n_cut++;
                    continue;
                }
            }
            if (!(ka & kb & kc & 0x200)) {
                /* past the guard band: drawn whole only if the GPU takes its size */
                int32_t x0 = ax < bx ? ax : bx, x1 = ax < bx ? bx : ax, y0 = ay < by ? ay : by, y1 = ay < by ? by : ay;
                if (cx < x0) x0 = cx;
                if (cx > x1) x1 = cx;
                if (cy < y0) y0 = cy;
                if (cy > y1) y1 = cy;
                if (x1 - x0 > 1023 || y1 - y0 > 511) {
                    uint32_t cc = ST_NOW();
                    NM_OUT();
                    s_n_tri--;
                    dlc_tri_clip(&tpls[i], pr, ia, ib, ic, (int)(fl & 3));
                    NM_IN();
                    s_cutc += (ST_NOW() - cc) & 0xFFFF;
                    n_cut++;
                    continue;
                }
            }
            {
                int nw = tr->nw, k;
                const uint32_t *src = tr->w;
                if (pk + nw + 1 > pend) {
                    s_overflow = 1;
                    s_n_over++;
                    continue;
                }
                p = pk;
                pk += nw + 1;
                p[0] = (uint32_t)nw << 24 | 0xFFFFFF;
                w = p + 1;
                for (k = 0; k < nw; k++)
                    w[k] = src[k];
            }
            w += tr->off;
            if (fl & NT_TEX) {
                w[1] = xa;
                w[4] = xb;
                w[7] = xc;
            } else {
                w[1] = xa;
                w[3] = xb;
                w[5] = xc;
            }
            if (fl & NT_LIT) {
                const MVtx *cs[3];
                int k, step = fl & NT_TEX ? 3 : 2;
                cs[0] = A;
                cs[1] = fl & NT_FLAT ? A : B;
                cs[2] = fl & NT_FLAT ? A : C;
                for (k = 0; k < 3; k++) {
                    int r = pr->c0[0] + (pr->sl[0] * cs[k]->r >> 8), g = pr->c0[1] + (pr->sl[1] * cs[k]->g >> 8);
                    int bb = pr->c0[2] + (pr->sl[2] * cs[k]->b >> 8);
                    w[k * step] = (w[k * step] & 0xFF000000u) | (uint32_t)clamp255(r) | (uint32_t)clamp255(g) << 8 |
                                  (uint32_t)clamp255(bb) << 16;
                }
            }
            n_piece++;
            if (fl & NT_ZS) {
                int z = (int)(((uint32_t)(A->z + B->z + C->z) * 21846u) >> 16);   /* (/ 3) */
                if (z < 0) z = 0;
                if (z >= OT_LEN) z = OT_LEN - 1;
                if (!zr) {
                    zr = 1;
                    lo = OT_LEN;
                    hi = -1;
                }
                if (z < lo) lo = z;
                if (z > hi) hi = z;
                if (s_ot_head[z]) {
                    uint32_t *t = s_ot_tail[z];
                    t[0] = (t[0] & 0xFF000000u) | ((uint32_t)p & 0xFFFFFF);
                } else {
                    s_ot_head[z] = p;
                }
                s_ot_tail[z] = p;
            } else {
                NM_OUT();
                put_flat(p);
                NM_IN();
            }
        }
        NM_OUT();
#undef NM_OUT
#undef NM_IN
        s_n_tri += n_tri;
        s_n_rej += n_rej;
        s_n_cull += n_cull;
        s_path[0] += n_rej + n_cull;
        s_path[1] += n_piece;
        s_path[2] += n_cut;
        s_n_piece += n_piece;
        s_n_dlc_tpl += n_piece;
        if ((uint32_t)s_used > s_peak_used)
            s_peak_used = (uint32_t)s_used;
        ST_ADD(5, tl0);
        }
    }
    if (s_n_piece == pieces0 && s_path[2] == cut0) {
        s_nmempty[0]++;
        if (s_n_rej - rej0 == s_n_tri - tri0) {
            s_nmoff[0]++;                       /* (every triangle off the screen) */
            s_nmoff[1] += s_n_vtx - mv0;
        }
    }
    s_nmoff[2] += s_n_vtx - mv0;
    s_nmempty[1]++;
    {
        uint32_t c0 = ST_NOW();
        st_replay(nm->st, nm->nst);
        ST_ADD(6, c0);
    }
    return 1;
}

/* TGR_LISTLOG: a list drawn straight, its commands */
static void list_dump(uint32_t a)
{
    uint32_t q;
    if (s_listlog <= 0 || tgr_frame() < (uint32_t)s_listlog || tgr_frame() >= (uint32_t)s_listlog + 3)
        return;
    fprintf(stderr, "straight %06X frame %u\n", (unsigned)a, (unsigned)tgr_frame());
    for (q = a; q <= a + 8 * 64; q += 8) {
        fprintf(stderr, "  %08X %08X\n", (unsigned)rd32(q), (unsigned)rd32(q + 4));
        if ((rd32(q) >> 24) == 0xB8)
            break;
    }
}

/* a call of the list at a drawn from its kept form: 0 when it is not kept */
static int dlc_call(uint32_t a)
{
    Dlc *d;
    DlcVar *v;
    uint32_t sig;
    int i, k, ti = 0, vb = 0;
    if (s_dlc_on < 0) {
        s_dlc_on = getenv("TGR_NODLC") == NULL;
        s_nm_on = getenv("TGR_NOMESH") == NULL;
    }
    uint32_t st0 = ST_NOW();
    if (!s_dlc_on || s_target < 0)
        return 0;
    for (d = s_dlc[(a >> 3) & (DLC_HASH - 1)]; d && d->addr != a; d = d->next)
        ;
    if (!d) {
        int vs = dlc_vol_slot(a);
        if (vs >= 0 && s_dlc_voln[vs] >= 4) {
            if (s_dlc_volf[vs] + 2 >= s_frame_no) {
                s_dlc_volf[vs] = s_frame_no;    /* (still built every frame: drawn straight) */
                s_dlc_why[4]++;
                list_dump(a);
                return 0;
            }
            s_dlc_voln[vs] = 0;                 /* a frame without it: kept again */
        }
    }
    ST_ADD(0, st0);
    st0 = ST_NOW();
    if (d && !d->bad) {
        for (k = 0; k < 16; k++)
            if ((d->segmask >> k & 1) && d->segval[k] != s_seg[k])
                break;
        int whole = d->vol || d->stable < 64 || !((s_frame_no + (a >> 3)) & 7);
        if (d->checked != s_frame_no && whole && d->stable < 64)
            d->stable++;
        if (k < 16 || (d->checked != s_frame_no &&
                       (whole ? dlc_sum_(d, 0) != d->sum : dlc_sum_(d, 1) != d->qsum))) {
            dlc_set_volatile(a);
            if (getenv("TGR_LOG"))
                tgr_log("rcp: list %06X changed (root %06X)\n", (unsigned)a, (unsigned)s_root_dl);
            dlc_free(d, a);                     /* changed: compiled again */
            d = NULL;
            s_dlc_why[1]++;
            {
                int vs = dlc_vol_slot(a);
                if (vs >= 0 && s_dlc_voln[vs] >= 4) {
                    s_dlc_why[4]++;             /* (changed every frame or two: drawn straight from now) */
                    list_dump(a);
                    return 0;
                }
            }
        }
    }
    if (!d) {
        s_dlc_why[0]++;
        d = dlc_compile(a);
        if (!d) {
            dlc_flush();
            d = dlc_compile(a);
        }
        if (!d)
            return 0;
    }
    if (d->bad)
        return 0;
    d->checked = s_frame_no;
    ST_ADD(1, st0);
    st0 = ST_NOW();
    sig = dlc_sig(d);
    s_ll_sig = sig;
    s_ll_reads = d->reads;
    s_ll_ntri = d->ntri;
    ST_ADD(2, st0);
    st0 = ST_NOW();
    v = d->var[0] && d->var[0]->sig == sig ? d->var[0] : d->var[1] && d->var[1]->sig == sig ? d->var[1] : NULL;
    if (!v) {
        if (!d->var[d->nextvar]) {
            DlcVar *nv = (DlcVar *)dlc_alloc_in(&d->mem, (int)sizeof(DlcVar));
            Tpl *t = nv ? (Tpl *)dlc_alloc_in(&d->mem, d->ntri * (int)sizeof(Tpl)) : NULL;
            Prim *pr = t ? (Prim *)dlc_alloc_in(&d->mem, d->maxprim * (int)(sizeof(Prim) + 8)) : NULL;
            if (!pr) {
                dlc_flush();                    /* full: everything compiled again */
                return 0;
            }
            nv->tpl = t;
            nv->prim = pr;
            nv->pstamp = (uint32_t *)(pr + d->maxprim);
            nv->pkey = nv->pstamp + d->maxprim;
            nv->cap = d->maxprim;
            d->var[d->nextvar] = nv;
        }
        v = d->var[d->nextvar];
        d->nextvar ^= 1;
        v->sig = sig;
        s_dlc_why[2]++;
        nm_free(v->nm);
        v->nm = NULL;
        v->recorded = 0;
        v->nprim = 0;
        memset(v->tpl, 0, (size_t)d->ntri * sizeof(Tpl));
    } else if (v->recorded && v->frame != s_frame_no) {
        /* once a frame: the texture table and the texture sources as they were */
        int stale = 0;
        for (i = 0; i < v->nprim && !stale; i++)
            if (v->prim[i].tex && v->prim[i].tex->key != v->pkey[i]) {
                stale = 1;
                s_dlc_why[3]++;
                if (s_listlog > 0 && tgr_frame() >= (uint32_t)s_listlog && tgr_frame() < (uint32_t)s_listlog + 10)
                    fprintf(stderr, "stale %u list %06X prim %d key %08X was %08X at %d,%d %dx%d used %u\n", (unsigned)tgr_frame(),
                            (unsigned)d->addr, i, (unsigned)v->prim[i].tex->key, (unsigned)v->pkey[i], v->prim[i].tex->vx,
                            v->prim[i].tex->vy, v->prim[i].tex->w, v->prim[i].tex->h, (unsigned)v->prim[i].tex->used);
            }
        /* (a texture's bytes changing does not matter: the draws keep where it is, not what) */
        if (stale) {
            nm_free(v->nm);
            v->nm = NULL;
            v->recorded = 0;
            v->nprim = 0;
                memset(v->tpl, 0, (size_t)d->ntri * sizeof(Tpl));
        }
    }
    if (!v->recorded) {
        v->texepoch = s_texepoch;
        s_n_dlc_rec++;
    } else {
        s_n_dlc_hit++;
    }
    v->frame = s_frame_no;
    for (k = 0; k < v->nprim; k++)
        if (v->prim[k].tex) {
            ((Tex *)v->prim[k].tex)->used = s_frame_no;
            s_shelf[v->prim[k].tex->shelf].used = s_frame_no;
        }
    ST_ADD(3, st0);
    if (v->nm && s_nm_on && nm_draw(d, v)) {
        s_st[7] += 1u << 16;
        return 1;
    }
    s_st[7]++;
    if (!v->nm)
        s_nmwhy[v->recorded ? nm_why(d, v) : 7]++;
    for (i = 0; i < d->ncmd; i++) {
        uint32_t w0 = d->cmd[i].w0, w1 = d->cmd[i].w1;
        int op = (int)(w0 >> 24), m, nt;
        switch (op) {
        case 0x04: {
            int vn = (int)((w0 >> 10) & 0x3F), v0 = (int)((w0 >> 16) & 0xFF) / 2, j;
            {
                int ok;
                RG_BEGIN(RG_MESHV);
                if (vb < 32) {
                    if (s_geom & G_LIGHTING) v->vlit |= 1u << vb; else v->vlit &= ~(1u << vb);
                    if (s_geom & G_FOG) v->vfog |= 1u << vb; else v->vfog &= ~(1u << vb);
                }
                vb++;
                ok = !(s_geom & G_FOG) && mesh_vertices((const NVtx *)d->cmd[i].aux, vn, v0, (s_geom & G_LIGHTING) != 0);
                RG_END(RG_MESHV);
                if (ok)
                    break;
            }
            load_vertices_n((const NVtx *)d->cmd[i].aux, vn, v0);
            for (j = v0; j < v0 + vn && j < 32; j++) {
                s_mvx[j].ok = 0;                /* (the full vertices: the general path) */
                s_mvstale &= ~(1u << j);
            }
            break;
        }
        case 0xB1: case 0xBF:
            nt = op == 0xB1 ? 2 : 1;
            for (m = 0; m < nt; m++, ti++) {
                uint32_t x = op == 0xB1 && m == 0 ? w0 : w1;
                int ia = (int)((x >> 16) & 0xFF) / 2, ib = (int)((x >> 8) & 0xFF) / 2, ic = (int)(x & 0xFF) / 2;
                Tpl *t = &v->tpl[ti];
                t->cull = (uint8_t)((s_geom >> 12) & 3);
                if (t->kind == 1 && (s_geom & G_LIGHTING) && !v->prim[t->prim].lit && v->lsig != s_lsig) {
                    t->kind = 0;                /* kept colours and the lights moved: kept again */
                    nm_free(v->nm);
                    v->nm = NULL;
                    s_dlc_why[5]++;
                }
                if (t->kind == 0) {
                    /* its draw worked out once, whether or not it shows (so the list becomes a mesh) */
                    RG_BEGIN(RG_REC);
                    uint32_t q0 = ST_NOW(), q1, q2;
                    mesh_fill(ia);
                    mesh_fill(ib);
                    mesh_fill(ic);
                    q1 = ST_NOW();
                    s_dry = 1;
                    triangle(ia, ib, ic);
                    q2 = ST_NOW();
                    dlc_capture(v, t, ia, ib, ic);
                    s_recq[0] += (q1 - q0) & 0xFFFF;
                    s_recq[1] += (q2 - q1) & 0xFFFF;
                    s_recq[2] += (ST_NOW() - q2) & 0xFFFF;
                    s_dry = 0;
                    s_n_dlc_rec++;
                    RG_END(RG_REC);
                }
                if (t->kind != 1)
                    s_dlc_tw[t->kind]++;
                {
                    /* off the screen or facing away: nothing to keep or draw */
                    const MVtx *a0 = &s_mvx[ia], *a1 = &s_mvx[ib], *a2 = &s_mvx[ic];
                    if (a0->clip & a1->clip & a2->clip & 31) {
                        s_n_tri++;
                        s_n_rej++;
                        s_path[0]++;
                        continue;
                    }
                    if (a0->ok && a1->ok && a2->ok) {
                        int32_t det;
                        if (a0->clip & a1->clip & a2->clip) {
                            s_n_tri++;
                            s_n_rej++;
                            s_path[0]++;
                            continue;
                        }
                        det = (a1->x - a0->x) * (a2->y - a0->y) - (a2->x - a0->x) * (a1->y - a0->y);
                        if (((s_geom & G_CULL_BACK) && det >= 0) || ((s_geom & G_CULL_FRONT) && det <= 0)) {
                            s_n_tri++;
                            s_n_cull++;
                            s_path[0]++;
                            continue;
                        }
                    }
                }
                if (t->kind == 1 &&
                    (!v->prim[t->prim].tex || v->prim[t->prim].tex->key == v->pkey[t->prim])) {
                    const MVtx *a0 = &s_mvx[ia], *a1 = &s_mvx[ib], *a2 = &s_mvx[ic];
                    const Prim *pr = &v->prim[t->prim];
                    if ((a0->ok & a1->ok & a2->ok & 2) &&
                        !(pr->textured && (edge_split(a0->w, a1->w, a0->x, a0->y, a1->x, a1->y) ||
                                           edge_split(a1->w, a2->w, a1->x, a1->y, a2->x, a2->y) ||
                                           edge_split(a2->w, a0->w, a2->x, a2->y, a0->x, a0->y)))) {
                        dlc_tri(t, pr, a0, a1, a2);
                        s_path[1]++;
                        continue;
                    }
                    {
                        RG_BEGIN(RG_CLIP);
                        dlc_tri_clip(t, pr, ia, ib, ic, (int)(s_geom >> 12) & 3);
                        RG_END(RG_CLIP);
                    }
                    s_path[2]++;
                    continue;
                }
                if (!(s_mvx[ia].ok && s_mvx[ib].ok && s_mvx[ic].ok) && (s_mvstale >> ia & s_mvstale >> ib & s_mvstale >> ic & 1)) {
                    /* near the eye or past the guard band: off the screen or facing away, found
                     * from clip space before any full vertex is made */
                    CV q[3];
                    int cl[3];
                    int64_t det;
                    vtx_clip(ia, &q[0], &cl[0]);
                    vtx_clip(ib, &q[1], &cl[1]);
                    vtx_clip(ic, &q[2], &cl[2]);
                    if (cl[0] & cl[1] & cl[2] & 31) {
                        s_n_tri++;
                        s_n_rej++;
                        s_path[0]++;
                        continue;
                    }
                    det = (int64_t)q[0].x * (((int64_t)q[1].y * q[2].w - (int64_t)q[2].y * q[1].w) >> 8) -
                          (int64_t)q[0].y * (((int64_t)q[1].x * q[2].w - (int64_t)q[2].x * q[1].w) >> 8) +
                          (int64_t)q[0].w * (((int64_t)q[1].x * q[2].y - (int64_t)q[2].x * q[1].y) >> 8);
                    if (((s_geom & G_CULL_BACK) && det <= 0) || ((s_geom & G_CULL_FRONT) && det >= 0)) {
                        s_n_tri++;
                        s_path[0]++;
                        continue;
                    }
                }
                triangle(ia, ib, ic);
                s_path[3]++;
                if (t->kind == 0)
                    dlc_capture(v, t, ia, ib, ic);
            }
            break;
        case 0xB8: case 0x00: case 0xE6: case 0xE7: case 0xE8:
            break;
        default:
            exec_state(op, w0, w1);
            break;
        }
    }
    v->recorded = 1;
    if (!v->nm && s_nm_on) {
        RG_BEGIN(RG_NMB);
        v->nm = nm_build(d, v);
        RG_END(RG_NMB);
    }
    return 1;
}

static void run(uint32_t dl)
{
    uint32_t stack[18];
    int sp = 0, guard = 0, opp = 0;
    uint32_t pc = seg_addr(dl) & ~7u, opt = ST_NOW();
    while (guard++ < 400000) {
        uint32_t w0 = rd32a(pc), w1 = rd32a(pc + 4);
        int op = (int)(w0 >> 24);
        pc += 8;
        if (s_perfops) {
            uint32_t now = ST_NOW();
            if (s_dlstats)
                s_opcount[op]++;
            s_runops[op]++;
            s_runopt[opp] += (now - opt) & 0xFFFF;
            opt = now;
            opp = op;
        }
        switch (op) {
        case 0x00: case 0xE6: case 0xE7: case 0xE8:   /* (no-ops here: the syncs) */
            break;
        case 0x01: {                                  /* G_MTX */
            M4 m;
            int p = (w0 >> 16) & 0xFF;
            RG_BEGIN(RG_MTX);
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
            RG_END(RG_MTX);
            break;
        }
        case 0x03: {                                  /* G_MOVEMEM */
            int idx = (w0 >> 16) & 0xFF;
            RG_BEGIN(RG_MOVEMEM);
            uint32_t a = seg_addr(w1);
            if (idx == 0x80) {
                int16_t sx = (int16_t)rd16(a), sy = (int16_t)rd16(a + 2), sz = (int16_t)rd16(a + 4);
                int16_t tx = (int16_t)rd16(a + 8), ty = (int16_t)rd16(a + 10), tz = (int16_t)rd16(a + 12);
                if (sx != s_vp.sx || sy != s_vp.sy || sz != s_vp.sz || tx != s_vp.tx || ty != s_vp.ty || tz != s_vp.tz)
                    s_mvp_stamp++;              /* (the same viewport again: nothing to work out again) */
                s_vp.sx = sx;
                s_vp.sy = sy;
                s_vp.sz = sz;
                s_vp.tx = tx;
                s_vp.ty = ty;
                s_vp.tz = tz;
            } else if (idx >= 0x86 && idx <= 0x94) {
                int k = (idx - 0x86) / 2, j;
                for (j = 0; j < 3; j++) {
                    if (s_light[k].col[j] != mem(a)[j] || s_light[k].dir[j] != (int8_t)mem(a + 8)[j])
                        s_light_stamp++;        /* (only a change: the lights worked out again) */
                    s_light[k].col[j] = mem(a)[j];
                    s_light[k].dir[j] = (int8_t)mem(a + 8)[j];
                }
            } else if (idx >= 0x98 && idx <= 0x9E) {  /* a forced matrix, a quarter at a time */
                static const int quarter[4] = { 1, 2, 3, 0 };
                int q = quarter[(idx - 0x98) / 2], j;
                int32_t *f = &s_mvp.m[0][0];
                mvp_update();
                s_mvp_stamp++;
                for (j = 0; j < 8; j++) {
                    int e = q * 8 + j;
                    uint32_t h = rd16(a + j * 2);
                    if (e < 16)
                        f[e] = (int32_t)(((uint32_t)f[e] & 0xFFFF) | h << 16);
                    else
                        f[e - 16] = (int32_t)(((uint32_t)f[e - 16] & 0xFFFF0000u) | h);
                }
            }
            RG_END(RG_MOVEMEM);
            break;
        }
        case 0x04: {                                  /* G_VTX */
            RG_BEGIN(RG_VTX);
            load_vertices(seg_addr(w1), (w0 >> 10) & 0x3F, ((w0 >> 16) & 0xFF) / 2);
            RG_END(RG_VTX);
            break;
        }
            break;
        case 0x06:                                    /* G_DL */
            if (s_dlstats == 1 && tgr_frame() + 1 >= (uint32_t)atoi(getenv("TGR_DLSTATS")))
                dl_census(seg_addr(w1), sp);
            if (((w0 >> 16) & 0xFF) == 0 && !s_perfcalls) {
                if (dlc_call(seg_addr(w1)))
                    break;                            /* drawn from its kept form */
            } else if (((w0 >> 16) & 0xFF) == 0) {
                int done;
                uint64_t c0 = ps1_time_ns();
                uint32_t la = seg_addr(w1), why0 = s_dlc_why[0], why1 = s_dlc_why[1], why2 = s_dlc_why[2], cut0 = s_cutc;
                uint32_t pa0 = s_path[0], pa1 = s_path[1], pa2 = s_path[2], pa3 = s_path[3], pc0 = s_n_piece, nmh = s_st[7];
                done = dlc_call(la);
                {
                    int cls = la >= 0x3E0000 ? 3 : la >= 0x2F0000 ? 2 : la >= 0x200000 ? 1 : 0;
                    uint64_t dt = ps1_time_ns() - c0;
                    int b = dt < 250000 ? 0 : dt < 1000000 ? 1 : dt < 4000000 ? 2 : dt < 16000000 ? 3 : 4;
                    int cmp = s_dlc_why[0] != why0 || s_dlc_why[1] != why1 || s_dlc_why[2] != why2;
                    s_cls_t[cls] += dt;
                    s_lh_t[cmp][b] += dt;
                    if (dt > 2000000 && s_listlog > 0 && tgr_frame() >= (uint32_t)s_listlog && tgr_frame() < (uint32_t)s_listlog + 10)
                        fprintf(stderr, "listlog %u %06X %u us why %u%u%u tris %u sig %08X reads %08X cut %u us\n", (unsigned)tgr_frame(), (unsigned)la,
                                (unsigned)(dt / 1000), (unsigned)(s_dlc_why[0] - why0), (unsigned)(s_dlc_why[1] - why1),
                                (unsigned)(s_dlc_why[2] - why2), (unsigned)s_ll_ntri, (unsigned)s_ll_sig, (unsigned)s_ll_reads,
                                (unsigned)((s_cutc - cut0) * 8000 / 33869));
                    if (dt > 2000000 && s_listlog > 0 && tgr_frame() >= (uint32_t)s_listlog && tgr_frame() < (uint32_t)s_listlog + 10)
                        fprintf(stderr, "  paths: away %u kept %u cut %u slow %u pieces %u mesh %u\n", (unsigned)(s_path[0] - pa0),
                                (unsigned)(s_path[1] - pa1), (unsigned)(s_path[2] - pa2), (unsigned)(s_path[3] - pa3),
                                (unsigned)(s_n_piece - pc0), (unsigned)((s_st[7] - nmh) >> 16));
                    s_lh_n[cmp][b]++;
                    s_cls_n[cls]++;
                }
                if (done)
                    break;                            /* drawn from its kept form */
            }
            if (((w0 >> 16) & 0xFF) == 0 && sp < 18)
                stack[sp++] = pc;
            pc = seg_addr(w1);
            break;
        case 0xB1: {                                  /* G_TRI2 */
            RG_BEGIN(RG_RUNTRI);
            s_path[4] += 2;
            triangle(((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2);
            triangle(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
            RG_END(RG_RUNTRI);
            break;
        }
        case 0xBF: {                                  /* G_TRI1 */
            RG_BEGIN(RG_RUNTRI);
            triangle(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
            RG_END(RG_RUNTRI);
            break;
        }
        case 0xB8:                                    /* G_ENDDL */
            if (sp == 0)
                return;
            pc = stack[--sp];
            break;
        case 0xBD:                                    /* G_POPMTX */
            if (s_mvn > 0)
                s_mvn--;
            s_mvp_dirty = 1;
            break;
        case 0xE4: case 0xE5: {                       /* G_TEXRECT: the halves follow */
            s_dstamp++;
            uint32_t h2 = rd32(pc + 4), hc = rd32(pc + 12);
            pc += 16;
            rect_tex(w0, w1, h2, hc);
            break;
        }
        default:
            exec_state(op, w0, w1);
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
    uint64_t task_t0 = ps1_time_ns();
    if (s_dlstats < 0)
        s_dlstats = getenv("TGR_DLSTATS") != NULL;
    if (s_dlstats && tgr_frame() >= (uint32_t)atoi(getenv("TGR_DLSTATS")) && s_dlstats == 1) {
        int i;
        fprintf(stderr, "dlstats frame %u:", (unsigned)tgr_frame());
        for (i = 0; i < 256; i++)
            if (s_opcount[i])
                fprintf(stderr, " %02X:%u", i, (unsigned)s_opcount[i]);
        fprintf(stderr, "\n");
        for (i = 0; i < s_ndlt; i++)
            fprintf(stderr, "dlt %06X calls %u cmds %u tris %u vtx %u state %u mtx %u dl %u movemem %u depth %u\n",
                    (unsigned)s_dlt[i].a, (unsigned)s_dlt[i].calls, (unsigned)s_dlt[i].cmds, (unsigned)s_dlt[i].tris,
                    (unsigned)s_dlt[i].vtx, (unsigned)s_dlt[i].state, (unsigned)s_dlt[i].mtx, (unsigned)s_dlt[i].dl,
                    (unsigned)s_dlt[i].movemem, (unsigned)s_dlt[i].depth);
        s_dlstats = 2;
    }
    if (s_dlstats == 1)
        memset(s_opcount, 0, sizeof s_opcount);
    if (!s_inited)
        init();
    if (s_listlog < 0) {
        s_perfops = getenv("TGR_PERFOPS") != NULL || getenv("TGR_DLSTATS") != NULL;
        s_perfcalls = (getenv("TGR_PERF") && atoi(getenv("TGR_PERF")) != 2) || getenv("TGR_LISTLOG") != NULL;   /* (TGR_PERF=2: the frame times only) */
    }
    if (s_listlog < 0)
        s_listlog = getenv("TGR_LISTLOG") ? atoi(getenv("TGR_LISTLOG")) : 0;   /* (list calls over 2 ms, from that frame for 10) */
    s_frame_no++;
    s_mvn = 0;
    s_root_dl = seg_addr(dl);
    if (s_tex_full) {
        /* the shelves are held by textures this frame may not use: all go, and
         * what it does use packs again (the uploads land after the last frame's
         * list has drawn) */
        s_tex_full = 0;
        tex_flush_all();
    }
    {
        uint64_t t1 = ps1_time_ns(), t2;
        run(dl);
        t2 = ps1_time_ns();
        frame_end();
        s_rg[RG_END] += ps1_time_ns() - t2;
        s_rg[RG_TOPTRI] += t2 - t1;
    }
    s_rg[RG_TASK] += ps1_time_ns() - task_t0;
}

void tgr_dump_state(const char *path, uint32_t dl) { (void)path; (void)dl; }

/* osViBlack: the display off (black) or on again */
void tgr_rcp_black(int on)
{
    s_black = on;
    gpu_display(!on);
}

/* the swap: the buffer is shown at the next retrace, once drawn */
void tgr_rcp_swap(uint32_t fb)
{
    int i;
    if (s_log)
        tgr_log("rcp: swap %08X frame %u black %d\n", fb, (unsigned)tgr_frame(), s_black);
    for (i = 0; i < 2; i++)
        if (s_fbaddr[i] == (fb & 0x3FFFFF)) {
            s_show_pending = i;
            return;
        }
    /* an image nothing was drawn into (a loading screen): black, as fresh RAM */
    i = fb_index(fb & 0x3FFFFF);
    if (i >= 0 && s_target != i) {
        int x, y, w, h;
        fb_rect(i, &x, &y, &w, &h);
        gpu_fill(x, y, 640, h, 0);
        s_fbclear[i] = 0;
        s_show_pending = i;
    }
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
        if (s_black)                            /* the display is off: what shows is black */
            memset(row, 0, sizeof row);
        else
            gpu_read(x0, y0 + y, w, 8, row);
        pcdrv_write(fd, row, w * 8 * 2);
    }
    pcdrv_close(fd);
}

/* the whole of VRAM to a file (TGR_VRAMDUMP=1 with a shot): vramNNNNN_1024x512.raw */
static void vram_dump(uint32_t frame)
{
    static uint16_t row[1024 * 4];
    char name[40];
    int fd, y;
    sprintf(name, "vram%05u_1024x512.raw", (unsigned)frame);
    if ((fd = pcdrv_creat(name)) < 0)
        return;
    for (y = 0; y < 512; y += 4) {
        gpu_read(0, y, 1024, 4, row);
        pcdrv_write(fd, row, (int)sizeof row);
    }
    pcdrv_close(fd);
}

void tgr_rcp_present(uint32_t frame)
{
    static int perf = -1;
    static uint64_t t0;
    static uint32_t f0, d0;
    if (perf < 0)
        perf = getenv("TGR_PERF") != NULL;
    if (getenv("TGR_PCSAMPLE_FROM") && frame == (uint32_t)atoi(getenv("TGR_PCSAMPLE_FROM")))
        ps1_pc_sample((uint32_t)atoi(getenv("TGR_PCSAMPLE") ? getenv("TGR_PCSAMPLE") : "1"));
    if (perf && frame % 60 == 0) {              /* TGR_PERF: the console's time per 60 retraces, frames drawn */
        uint64_t t = ps1_time_ns();
        if (t0)
            fprintf(stderr, "perf: retraces %u-%u took %u ms of console time, %u frames drawn; %u vertices, %u triangles, "
                    "%u pieces, %u clip passes, %u by the GTE (flags %08X, ok %d k %d), %u fast (%u culled), rejected %u, slow: state %u, kind %u, edge %u\n", (unsigned)f0, (unsigned)frame, (unsigned)((t - t0) / 1000000),
                    (unsigned)(tgr_rcp_frames - d0), (unsigned)s_n_vtx, (unsigned)s_n_tri, (unsigned)s_n_piece,
                    (unsigned)s_n_clip, (unsigned)s_n_gte, (unsigned)s_gte_flagor, s_gte_ok, s_gte_k,
                    (unsigned)s_n_fast, (unsigned)s_n_cull, (unsigned)s_n_rej, (unsigned)s_n_stamp, (unsigned)s_n_notok,
                    (unsigned)s_n_edge);
        {
            extern uint32_t fm_calls[8];
            fprintf(stderr, "perf: calls catf %u rotvec %u rotvect %u mulvecrows %u sinf %u cosf %u slow3 %u exact3 %u\n",
                    (unsigned)fm_calls[0], (unsigned)fm_calls[1], (unsigned)fm_calls[2], (unsigned)fm_calls[3],
                    (unsigned)fm_calls[4], (unsigned)fm_calls[5], (unsigned)fm_calls[6], (unsigned)fm_calls[7]);
            memset(fm_calls, 0, sizeof fm_calls);
        }
        {
            extern void tgr_perf_marks_report(void);
            tgr_perf_marks_report();
        }
        {
            int i, n = 0;
            uint32_t hw = 0, lost = 0;
            for (i = 0; i < s_ntex; i++)
                if (s_texs[i].key && s_texs[i].used + 1 >= s_frame_no) {
                    n++;
                    hw += (uint32_t)((s_texs[i].w + tex_k(&s_texs[i]) - 1) / tex_k(&s_texs[i])) * s_texs[i].h;
                }
            for (i = 0; i < s_nshelf; i++)
                lost += (uint32_t)(384 - s_shelf[i].x) * (uint32_t)s_shelf[i].h;
            {
                extern uint64_t gpu_wait_ns;
                fprintf(stderr, "perf: waiting for the GPU %u ms\n", (unsigned)(gpu_wait_ns / 1000000));
                gpu_wait_ns = 0;
            }
            {
                int r;
                fprintf(stderr, "perf: regions (ms):");
                for (r = 0; r < RG_N; r++) {
                    fprintf(stderr, " %s %u", s_rg_name[r], (unsigned)(s_rg[r] / 1000000));
                    s_rg[r] = 0;
                }
                fprintf(stderr, "\n");
                fprintf(stderr, "perf: lists by place (ms/calls): <2M %u/%u 2M %u/%u 2F %u/%u 3E %u/%u\n",
                        (unsigned)(s_cls_t[0] / 1000000), (unsigned)s_cls_n[0], (unsigned)(s_cls_t[1] / 1000000), (unsigned)s_cls_n[1],
                        (unsigned)(s_cls_t[2] / 1000000), (unsigned)s_cls_n[2], (unsigned)(s_cls_t[3] / 1000000), (unsigned)s_cls_n[3]);
                memset(s_cls_t, 0, sizeof s_cls_t);
                for (r = 0; r < 2; r++) {
                    int b;
                    fprintf(stderr, "perf: list calls %s by length (ms/calls):", r ? "compiling" : "kept");
                    for (b = 0; b < 5; b++)
                        fprintf(stderr, " %u/%u", (unsigned)(s_lh_t[r][b] / 1000000), (unsigned)s_lh_n[r][b]);
                    fprintf(stderr, "\n");
                }
                memset(s_lh_t, 0, sizeof s_lh_t);
                fprintf(stderr, "perf: texture lookups %u: key %u ms, found %u ms; decodes %u, %u texels\n", (unsigned)s_tq[3],
                        (unsigned)(s_tq[0] * 8 / 33869), (unsigned)(s_tq[1] * 8 / 33869), (unsigned)s_tq[2], (unsigned)s_decx);
                s_decx = 0;
                fprintf(stderr, "perf: decode parts (ms): tmem %u convert %u upload %u\n", (unsigned)(s_decq[0] * 8 / 33869),
                        (unsigned)(s_decq[1] * 8 / 33869), (unsigned)(s_decq[2] * 8 / 33869));
                memset(s_decq, 0, sizeof s_decq);
                memset(s_tq, 0, sizeof s_tq);
                {
                    int o;
                    fprintf(stderr, "perf: mesh state replay (ms/n):");
                    for (o = 0; o < 256; o++)
                        if (s_stopn[o])
                            fprintf(stderr, " %02X:%u/%u", o, (unsigned)(s_stop[o] * 8 / 33869), (unsigned)s_stopn[o]);
                    fprintf(stderr, "\n");
                    memset(s_stop, 0, sizeof s_stop);
                    memset(s_stopn, 0, sizeof s_stopn);
                }
                fprintf(stderr, "perf: rectangles %u, %u two-texture (ms): colours %u draw state %u put %u\n", (unsigned)s_rq[3], (unsigned)s_rqo,
                        (unsigned)(s_rq[0] * 8 / 33869), (unsigned)(s_rq[1] * 8 / 33869), (unsigned)(s_rq[2] * 8 / 33869));
                memset(s_rq, 0, sizeof s_rq);
                s_rqo = 0;
                fprintf(stderr, "perf: dry triangles (ms): colours %u draw state %u\n", (unsigned)(s_dryq[0] * 8 / 33869),
                        (unsigned)(s_dryq[1] * 8 / 33869));
                memset(s_dryq, 0, sizeof s_dryq);
                fprintf(stderr, "perf: recording (ms): vertices %u triangle %u capture %u\n", (unsigned)(s_recq[0] * 8 / 33869),
                        (unsigned)(s_recq[1] * 8 / 33869), (unsigned)(s_recq[2] * 8 / 33869));
                memset(s_recq, 0, sizeof s_recq);
                fprintf(stderr, "perf: mesh triangles (ms): off %u away %u drawn %u (%u)\n", (unsigned)(s_tt[0] * 8 / 33869),
                        (unsigned)(s_tt[1] * 8 / 33869), (unsigned)(s_tt[2] * 8 / 33869), (unsigned)s_tt[3]);
                memset(s_tt, 0, sizeof s_tt);
                fprintf(stderr, "perf: drawn parts (ms): tests %u copy %u lit %u link %u\n", (unsigned)(s_tq2[0] * 8 / 33869),
                        (unsigned)(s_tq2[1] * 8 / 33869), (unsigned)(s_tq2[2] * 8 / 33869), (unsigned)(s_tq2[3] * 8 / 33869));
                memset(s_tq2, 0, sizeof s_tq2);
                fprintf(stderr, "perf: gte setups: one stamp on %u, ysh %u, more %u\n", (unsigned)s_gsw[0], (unsigned)s_gsw[1], (unsigned)s_gsw[2]);
                memset(s_gsw, 0, sizeof s_gsw);
                fprintf(stderr, "perf: setup (ms/calls): matrix %u/%u gte %u/%u lights %u/%u\n", (unsigned)(s_su[0] * 8 / 33869),
                        (unsigned)s_su[3], (unsigned)(s_su[1] * 8 / 33869), (unsigned)s_su[4], (unsigned)(s_su[2] * 8 / 33869),
                        (unsigned)s_su[5]);
                memset(s_su, 0, sizeof s_su);
                memset(s_lh_n, 0, sizeof s_lh_n);
                memset(s_cls_n, 0, sizeof s_cls_n);
                fprintf(stderr, "perf: list stages (ms): find %u check %u key %u variant %u vertices %u triangles %u state %u\n",
                        (unsigned)(s_st[0] * 8 / 33869), (unsigned)(s_st[1] * 8 / 33869), (unsigned)(s_st[2] * 8 / 33869),
                        (unsigned)(s_st[3] * 8 / 33869), (unsigned)(s_st[4] * 8 / 33869), (unsigned)(s_st[5] * 8 / 33869),
                        (unsigned)(s_st[6] * 8 / 33869));
                fprintf(stderr, "perf: mesh cut path %u ms; direct grids %u, too deep %u, failed %u\n", (unsigned)(s_cutc * 8 / 33869),
                        (unsigned)s_gridn[0], (unsigned)s_gridn[1], (unsigned)s_gridn[2]);
                memset(s_gridn, 0, sizeof s_gridn);
                s_cutc = 0;
                fprintf(stderr, "perf: lists as meshes %u, replayed %u (no mesh: fog %u unseen %u slow %u fogprim %u other %u unrecorded %u)\n",
                        (unsigned)(s_st[7] >> 16), (unsigned)(s_st[7] & 0xFFFF), (unsigned)s_nmwhy[0], (unsigned)s_nmwhy[1],
                        (unsigned)s_nmwhy[2], (unsigned)s_nmwhy[3], (unsigned)s_nmwhy[4], (unsigned)s_nmwhy[7]);
                memset(s_nmwhy, 0, sizeof s_nmwhy);
                fprintf(stderr, "perf: meshes all off the screen %u (vertices %u of %u)\n", (unsigned)s_nmoff[0],
                        (unsigned)s_nmoff[1], (unsigned)s_nmoff[2]);
                memset(s_nmoff, 0, sizeof s_nmoff);
                fprintf(stderr, "perf: meshes drawing nothing %u of %u\n", (unsigned)s_nmempty[0], (unsigned)s_nmempty[1]);
                s_nmempty[0] = s_nmempty[1] = 0;
                memset(s_st, 0, sizeof s_st);
                fprintf(stderr, "perf: run commands:");
                for (r = 0; r < 256; r++)
                    if (s_runops[r] > 300 || s_runopt[r] > 33869 / 8 * 10)
                        fprintf(stderr, " %02X:%u/%ums", r, (unsigned)s_runops[r], (unsigned)(s_runopt[r] * 8 / 33869));
                fprintf(stderr, "\n");
                memset(s_runops, 0, sizeof s_runops);
                memset(s_runopt, 0, sizeof s_runopt);
            }
            fprintf(stderr, "perf: lists kept %u, recorded %u, triangles from them %u\n", (unsigned)s_n_dlc_hit,
                    (unsigned)s_n_dlc_rec, (unsigned)s_n_dlc_tpl);
            s_n_dlc_hit = s_n_dlc_rec = s_n_dlc_tpl = 0;
            fprintf(stderr, "perf: list arena %u KB used, %u KB held, %u flushes, %u evicted\n", (unsigned)(s_dlc_used / 1024),
                    (unsigned)(s_dlc_live / 1024), (unsigned)s_dlc_flushes, (unsigned)s_dlc_evicted);
            fprintf(stderr, "perf: lists why: compiled %u changed %u key %u table %u drawn straight %u lights %u\n",
                    (unsigned)s_dlc_why[0], (unsigned)s_dlc_why[1], (unsigned)s_dlc_why[2], (unsigned)s_dlc_why[3],
                    (unsigned)s_dlc_why[4], (unsigned)s_dlc_why[5]);
            memset(s_dlc_why, 0, sizeof s_dlc_why);
            fprintf(stderr, "perf: list triangles without a kept draw: not yet %u, never %u; capture missed %u\n", (unsigned)s_dlc_tw[0],
                    (unsigned)s_dlc_tw[2], (unsigned)s_dlc_tw[3]);
            memset(s_dlc_tw, 0, sizeof s_dlc_tw);
            fprintf(stderr, "perf: triangles: kept off/away %u drawn %u cut %u slow %u; general %u+\n", (unsigned)s_path[0],
                    (unsigned)s_path[1], (unsigned)s_path[2], (unsigned)s_path[3], (unsigned)s_path[4]);
            memset(s_path, 0, sizeof s_path);
            fprintf(stderr, "perf: never kept why: no draw %u, not ok %u, fog %u, prims %u\n", (unsigned)s_k2why[0],
                    (unsigned)s_k2why[1], (unsigned)s_k2why[2], (unsigned)s_k2why[3]);
            memset(s_k2why, 0, sizeof s_k2why);
            fprintf(stderr, "perf: cut path (ms): off %u away %u whole %u behind %u guard %u both %u\n",
                    (unsigned)(s_cwt[0] * 8 / 33869), (unsigned)(s_cwt[1] * 8 / 33869), (unsigned)(s_cwt[2] * 8 / 33869),
                    (unsigned)(s_cwt[3] * 8 / 33869), (unsigned)(s_cwt[4] * 8 / 33869), (unsigned)(s_cwt[5] * 8 / 33869));
            memset(s_cwt, 0, sizeof s_cwt);
            fprintf(stderr, "perf: cut path: off %u away %u; drawn: whole %u behind %u guard %u both %u\n", (unsigned)s_cutw[0],
                    (unsigned)s_cutw[1], (unsigned)s_cutw[2], (unsigned)s_cutw[3], (unsigned)s_cutw[4], (unsigned)s_cutw[5]);
            memset(s_cutw, 0, sizeof s_cutw);
            {
                int bit;
                fprintf(stderr, "perf: mesh vertices: first pass %u cycles a vertex, again %u; ",
                        (unsigned)(s_flagbits[10] ? s_flagbits[8] * 8 / s_flagbits[10] : 0),
                        (unsigned)(s_flagbits[10] ? s_flagbits[9] * 8 / s_flagbits[10] : 0));
                fprintf(stderr, "perf: triples clean %u flagged %u: behind %u near %u far %u screen %u;", (unsigned)s_flagbits[0],
                        (unsigned)s_flagbits[1], (unsigned)s_flagbits[2], (unsigned)s_flagbits[3], (unsigned)s_flagbits[4],
                        (unsigned)s_flagbits[5]);
                for (bit = 12; bit < 31; bit++)
                    if (s_flagbits[bit])
                        fprintf(stderr, " b%d %u", bit, (unsigned)s_flagbits[bit]);
                for (bit = 12; bit < 31; bit++)
                    if (s_flagbits2[1][bit] | s_flagbits2[2][bit])
                        fprintf(stderr, " retry b%d near %u far %u", bit, (unsigned)s_flagbits2[1][bit], (unsigned)s_flagbits2[2][bit]);
                fprintf(stderr, "\n");
                memset(s_flagbits, 0, sizeof s_flagbits);
                memset(s_flagbits2, 0, sizeof s_flagbits2);
            }
            fprintf(stderr, "perf: splits: size %u texels %u\n", (unsigned)s_splitw[1], (unsigned)s_splitw[2]);
            memset(s_splitw, 0, sizeof s_splitw);
            fprintf(stderr, "perf: texture table moved: flush %u merge %u shelf %u redo %u full %u\n", (unsigned)s_epw[0],
                    (unsigned)s_epw[1], (unsigned)s_epw[2], (unsigned)s_epw[3], (unsigned)s_epw[4]);
            memset(s_epw, 0, sizeof s_epw);
            fprintf(stderr, "perf: packets dropped %u, peak words %u of %d\n", (unsigned)s_n_over, (unsigned)s_peak_used, PKT_WORDS);
            s_n_over = s_peak_used = 0;
            fprintf(stderr, "perf: textures %d in use, %u halfwords; %d shelves, %u halfwords free at shelf ends\n", n,
                    (unsigned)hw, s_nshelf, (unsigned)lost);
        }
        s_n_vtx = s_n_tri = s_n_piece = s_n_clip = s_n_gte = s_gte_flagor = s_n_fast = s_n_cull = 0;
        s_n_rej = s_n_stamp = s_n_notok = s_n_edge = 0;
        t0 = t;
        f0 = frame;
        d0 = tgr_rcp_frames;
    }
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
            if (getenv("TGR_VRAMDUMP"))
                vram_dump(frame);
        }
}
