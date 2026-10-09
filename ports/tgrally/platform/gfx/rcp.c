/* rcp.c: the game's display lists, run as the N64's RSP (F3DEX 1.21) and
 * RDP would, onto a renderer (render/rdr.h).
 *
 * The RSP half keeps the microcode's state -- the matrix stacks, the 32
 * vertex slots, the lights and look-at directions, the segment table, the
 * geometry mode, fog and the viewport -- transforms and lights each vertex
 * as it loads, and turns triangles into clip-space vertices.  The RDP half
 * keeps the other modes, the combiner, the colour registers, the eight tiles
 * and a 4 KB TMEM filled by the loads exactly as the hardware fills it (odd
 * rows' words swapped), and decodes a tile's texels from TMEM when a
 * primitive uses it.
 *
 * Display lists and everything they point at are big-endian in game memory
 * (the arena), read here as the RSP read them. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "tgr_touch.h"
#include "tgr_addr.h"
#include "tgr_view.h"
#include "../render/rdr.h"

/* ---- debugging switches (environment, read once) ------------------------------ *
 *   TGR_RCPLOG=1         log each primitive's state to stderr
 *   TGR_RAMDUMP=FILE@N   write the segment table and game memory at frame N (tools)
 *   TGR_WORLDDUMP=FILE   record the world-placed triangles, the camera and the textures (tools)
 *   TGR_HUDSTATE=FILE    record what the race HUD reads, each frame (tools)
 *   TGR_TEXDUMP=DIR      write every decoded texture as a PNG
 *   TGR_PIXEL=X,Y        (soft renderer) report each write to that pixel with
 *                        the triangle number the log gives
 *   TGR_ONLYTRIS=N / TGR_SKIPTRIS=N   draw only the first N / skip the first N
 *                        triangles of each frame (to find what draws a region) */
static int s_log, s_only = -1, s_skip;
int tgr_rcp_tri;
uint32_t tgr_vi_ctrl(void);                         /* platform/os/io.c */
uint64_t host_ticks_ns(void);
uint32_t tgr_rcp_frames;                            /* frames finished (TGR_STATS) */
uint64_t tgr_rcp_end_ns, tgr_rcp_end_max_ns;        /* time in rdr_frame_end: the present */

/* TGR_HUDSTATE=FILE: what the race HUD (racing/racehud.c) reads, once a finished
 * frame, for tools: 'R' retrace, the mode and layout words, the laps, the view and
 * panel, then the player car's fields the HUD prints (raw), its two messages
 * (24 bytes each), and whether the view is the car's third camera; 'D' once: the
 * car's model record (its dial) */
extern void *D_8028AAF0[1], *D_8028AAF4[1];
static void hud_state(void)
{
    static FILE *f;
    static int init, dialed;
    const uint8_t *car;
    uint32_t w[24];
    int i;
    if (!init) {
        init = 1;
        if (getenv("TGR_HUDSTATE"))
            f = fopen(getenv("TGR_HUDSTATE"), "wb");
    }
    car = (const uint8_t *)D_8028AAF0[0];
    if (!f || !car)
        return;
    if (!dialed) {
        int kind = *(const int *)(car + 0x205C);
        dialed = 1;
        fputc('D', f);
        fwrite(TGR_PTR(uint8_t *, 0x8028AE0Cu + kind * 0x60), 1, 0x60, f);
    }
    w[0] = tgr_frame();
    w[1] = *TGR_PTR(uint32_t *, 0x8026FF18u);     /* race mode */
    w[2] = *TGR_PTR(uint32_t *, 0x8026FF10u);
    w[3] = *TGR_PTR(uint32_t *, 0x802723D8u);     /* mph */
    w[4] = *TGR_PTR(uint32_t *, 0x8028AB0Cu);     /* players' layout */
    w[5] = *TGR_PTR(uint32_t *, 0x8028AA80u);     /* night */
    w[6] = *TGR_PTR(uint32_t *, 0x8028B304u);     /* laps in the race */
    for (i = 0; i < 4; i++)
        w[7 + i] = TGR_PTR(uint32_t *, 0x8031B2C8u)[i];          /* view 0: x y w h */
    w[11] = *TGR_PTR(uint16_t *, 0x8028C7B4u + 2);                /* its panel's height */
    w[12] = *(const uint32_t *)(car + 0xF78);     /* laps */
    w[13] = *(const uint32_t *)(car + 0xF80);     /* raceTime (the lap time shown) */
    w[14] = *(const uint32_t *)(car + 0xF98);     /* best lap */
    w[15] = *(const uint32_t *)(car + 0xFA0);     /* lapTime (the total shown) */
    w[16] = *(const uint32_t *)(car + 0xFA4);     /* time left */
    w[17] = *(const uint32_t *)(car + 0xFAC);     /* position */
    w[18] = *(const uint32_t *)(car + 0xFE4);     /* speed */
    w[19] = *(const uint32_t *)(car + 0xDF4);     /* revs */
    w[20] = *(const uint32_t *)(car + 0xE38);
    w[21] = *(const uint32_t *)(car + 0xE40);     /* rev lamp frame */
    w[22] = *(const uint32_t *)(car + 0x205C);    /* kind */
    w[23] = D_8028AAF4[0] == (const void *)(car + 0x1DF0 + 2 * 0x44);
    fputc('R', f);
    fwrite(w, 4, 24, f);
    for (i = 0; i < 2; i++) {
        uint32_t a = *(const uint32_t *)(car + (i ? 0xFB8 : 0xFB0));
        char m[24];
        memset(m, 0, sizeof m);
        if (a)
            strncpy(m, TGR_PTR(const char *, a), sizeof m - 1);
        fwrite(m, 1, sizeof m, f);
    }
}

static void rec_flush(void);
static void touch_shown(void);

static void frame_end(void)
{
    uint64_t t0 = host_ticks_ns(), t;
    touch_shown();
    rec_flush();
    hud_state();
    rdr_vi(tgr_vi_ctrl());
    rdr_frame_end();
    t = host_ticks_ns() - t0;
    tgr_rcp_frames++;
    tgr_rcp_end_ns += t;
    if (t > tgr_rcp_end_max_ns)
        tgr_rcp_end_max_ns = t;
}                 /* the frame's triangle number (TGR_PIXEL reports it) */
static const char *s_texdump;

/* ---- memory ---------------------------------------------------------------- */
static uint32_t s_seg[16];

static uint32_t seg_addr(uint32_t a)
{
    return ((s_seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x7FFFFF);
}
static const uint8_t *mem(uint32_t phys) { return tgr_rdram + (phys & 0x7FFFFF); }
static uint32_t rd32(uint32_t phys)
{
    const uint8_t *p = mem(phys);
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static uint16_t rd16(uint32_t phys) { const uint8_t *p = mem(phys); return (uint16_t)(p[0] << 8 | p[1]); }

/* ---- the RSP's state ----------------------------------------------------------- */
#define G_ZBUFFER            0x00000001
#define G_SHADE              0x00000004
#define G_SHADING_SMOOTH     0x00000200
#define G_CULL_FRONT         0x00001000
#define G_CULL_BACK          0x00002000
#define G_FOG                0x00010000
#define G_LIGHTING           0x00020000
#define G_TEXTURE_GEN        0x00040000
#define G_TEXTURE_GEN_LINEAR 0x00080000

typedef struct { float m[4][4]; } M4;

static M4 s_mv[10];
static int s_mvn;
static M4 s_proj, s_mvp;
static int s_mvp_forced;
static uint32_t s_geom;
static struct { float sx, sy, sz, tx, ty, tz; } s_vp;
static struct { uint8_t col[3]; int8_t dir[3]; } s_light[8];
static int s_nlights = 1;
static int8_t s_lookat[2][3];
static int s_fog_mul, s_fog_ofs;
static struct { int on, tile, level; uint16_t ss, ts; } s_tex;

typedef struct { RdrVtx v; float nx, ny, nz; int clip; float wx, wy, wz, ww; int world; uint32_t addr; } Vtx;
static Vtx s_vtx[32];

static void m4_mul(M4 *r, const M4 *a, const M4 *b)
{
    M4 t;
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            t.m[i][j] = a->m[i][0] * b->m[0][j] + a->m[i][1] * b->m[1][j] + a->m[i][2] * b->m[2][j] +
                        a->m[i][3] * b->m[3][j];
    *r = t;
}

/* a fixed-point Mtx (16 integer halves, then 16 fraction halves) */
static void m4_load(M4 *r, uint32_t a)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            int32_t hi = (int16_t)rd16(a + (i * 4 + j) * 2);
            uint32_t lo = rd16(a + 32 + (i * 4 + j) * 2);
            r->m[i][j] = (float)((hi * 65536 + (int32_t)lo) / 65536.0);
        }
}

/* the RSP keeps the MVP in DMEM as s15.16 halves (integer parts, then
 * fractions), and a forced matrix overwrites those halves a quarter at a
 * time: kept here as the fixed-point words so a quarter replaces exactly
 * the halves it carries */
static int32_t s_mvp_fx[16];

static void mvp_update(void)
{
    int i;
    if (s_mvp_forced)
        return;
    m4_mul(&s_mvp, &s_mv[s_mvn], &s_proj);
    for (i = 0; i < 16; i++) {
        double f = floor((double)s_mvp.m[i / 4][i % 4] * 65536.0);
        s_mvp_fx[i] = f >= 2147483647.0 ? INT32_MAX : f <= -2147483648.0 ? INT32_MIN : (int32_t)f;
    }
}

/* a light's direction in model space (the modelview's transpose applied) */
static void model_dir(const int8_t d[3], float out[3])
{
    const M4 *m = &s_mv[s_mvn];
    float x = d[0] / 127.0f, y = d[1] / 127.0f, z = d[2] / 127.0f, l;
    int i;
    for (i = 0; i < 3; i++)
        out[i] = x * m->m[i][0] + y * m->m[i][1] + z * m->m[i][2];
    l = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (l > 0) {
        out[0] /= l;
        out[1] /= l;
        out[2] /= l;
    }
}

static void load_vertices(uint32_t a, int n, int v0)
{
    float ldir[8][3], lk[2][3];
    int i, k;
    if (s_geom & G_LIGHTING) {
        for (k = 0; k < s_nlights; k++)
            model_dir(s_light[k].dir, ldir[k]);
        model_dir(s_lookat[0], lk[0]);
        model_dir(s_lookat[1], lk[1]);
    }
    for (i = 0; i < n && v0 + i < 32; i++) {
        uint32_t p = a + i * 16;
        float x = (int16_t)rd16(p), y = (int16_t)rd16(p + 2), z = (int16_t)rd16(p + 4);
        float s = (int16_t)rd16(p + 8), t = (int16_t)rd16(p + 10);
        const uint8_t *c = mem(p + 12);
        Vtx *v = &s_vtx[v0 + i];
        const M4 *m = &s_mvp;
        float cx = x * m->m[0][0] + y * m->m[1][0] + z * m->m[2][0] + m->m[3][0];
        float cy = x * m->m[0][1] + y * m->m[1][1] + z * m->m[2][1] + m->m[3][1];
        float cz = x * m->m[0][2] + y * m->m[1][2] + z * m->m[2][2] + m->m[3][2];
        float cw = x * m->m[0][3] + y * m->m[1][3] + z * m->m[2][3] + m->m[3][3];
        v->v.x = cx * s_vp.sx + cw * s_vp.tx;         /* framebuffer pixels, times w */
        v->v.y = -cy * s_vp.sy + cw * s_vp.ty;
        v->v.z = cz;
        v->v.w = cw;
        v->clip = (cx < -cw) | (cx > cw) << 1 | (cy < -cw) << 2 | (cy > cw) << 3 | (cw < 0.001f) << 4;
        v->world = !s_mvp_forced;                     /* the modelview places it in the world */
        v->addr = p;
        if (v->world) {
            const M4 *w = &s_mv[s_mvn];
            v->wx = x * w->m[0][0] + y * w->m[1][0] + z * w->m[2][0] + w->m[3][0];
            v->wy = x * w->m[0][1] + y * w->m[1][1] + z * w->m[2][1] + w->m[3][1];
            v->wz = x * w->m[0][2] + y * w->m[1][2] + z * w->m[2][2] + w->m[3][2];
            v->ww = 1.0f;
        } else {                                      /* a forced matrix: its clip position */
            v->wx = cx;
            v->wy = cy;
            v->wz = cz;
            v->ww = cw;
        }
        if (s_geom & G_LIGHTING) {
            float n[3] = { (int8_t)c[0] / 127.0f, (int8_t)c[1] / 127.0f, (int8_t)c[2] / 127.0f };
            float r = s_light[s_nlights].col[0], g = s_light[s_nlights].col[1], b = s_light[s_nlights].col[2];
            for (k = 0; k < s_nlights; k++) {
                float d = n[0] * ldir[k][0] + n[1] * ldir[k][1] + n[2] * ldir[k][2];
                if (d > 0) {
                    r += d * s_light[k].col[0];
                    g += d * s_light[k].col[1];
                    b += d * s_light[k].col[2];
                }
            }
            v->v.r = (r > 255 ? 255 : r) / 255.0f;
            v->v.g = (g > 255 ? 255 : g) / 255.0f;
            v->v.b = (b > 255 ? 255 : b) / 255.0f;
            if (s_geom & G_TEXTURE_GEN) {
                float dx = n[0] * lk[0][0] + n[1] * lk[0][1] + n[2] * lk[0][2];
                float dy = n[0] * lk[1][0] + n[1] * lk[1][1] + n[2] * lk[1][2];
                if (s_geom & G_TEXTURE_GEN_LINEAR) {  /* the microcode's arc-cosine form */
                    dx = acosf(-(dx < -1 ? -1 : dx > 1 ? 1 : dx)) / 4.0f;
                    dy = acosf(-(dy < -1 ? -1 : dy > 1 ? 1 : dy)) / 4.0f;
                } else {
                    dx = (dx + 1.0f) / 4.0f;
                    dy = (dy + 1.0f) / 4.0f;
                }
                s = dx * s_tex.ss;
                t = dy * s_tex.ts;
                v->v.s = s / 32.0f;
                v->v.t = t / 32.0f;
            }
        } else {
            v->v.r = c[0] / 255.0f;
            v->v.g = c[1] / 255.0f;
            v->v.b = c[2] / 255.0f;
        }
        v->v.a = c[3] / 255.0f;
        if (!((s_geom & G_LIGHTING) && (s_geom & G_TEXTURE_GEN))) {
            v->v.s = s * s_tex.ss / 65536.0f / 32.0f;
            v->v.t = t * s_tex.ts / 65536.0f / 32.0f;
        }
        if (s_geom & G_FOG) {
            float winv = cw > 0 ? 1.0f / cw : 32767.0f, f = cz * winv * s_fog_mul + s_fog_ofs;
            v->v.a = (f < 0 ? 0 : f > 255 ? 255 : f) / 255.0f;
        }
    }
}

/* ---- the RDP's state ------------------------------------------------------------ */
static uint32_t s_omh, s_oml, s_fill;
static uint32_t s_cimg, s_zimg, s_timg;
static int s_cimg_w, s_timg_fmt, s_timg_siz, s_timg_w;
static uint32_t s_cc0, s_cc1;
static float s_prim[4], s_env[4], s_fogc[4], s_blendc[4], s_prim_lod;
static int s_scissor[4];
static uint8_t s_tmem[4096];
static struct {
    int fmt, siz, line, tmem, pal, cmt, maskt, shiftt, cms, masks, shifts;
    int uls, ult, lrs, lrt;           /* 10.2 */
} s_tile[8];
static uint32_t s_half1, s_half2;
static int s_fb_w, s_fb_h, s_frame_open;

static void rgba32f(uint32_t c, float out[4])
{
    out[0] = (c >> 24) / 255.0f;
    out[1] = ((c >> 16) & 0xFF) / 255.0f;
    out[2] = ((c >> 8) & 0xFF) / 255.0f;
    out[3] = (c & 0xFF) / 255.0f;
}

/* TMEM loads, as the hardware does them: odd rows have their 32-bit words
 * swapped within each 64-bit word */
static void tmem_put64(int addr, const uint8_t *src, int odd)
{
    int k;
    for (k = 0; k < 8; k++)
        s_tmem[((addr * 8) + (k ^ (odd ? 4 : 0))) & 0xFFF] = src[k];
}

static void load_block(int tile, int uls, int ult, int lrs, int dxt)
{
    int bpp = s_timg_siz == 0 ? 0 : 4 << s_timg_siz;  /* bits per texel (4-bit: 0 -> handled) */
    int texels = lrs - uls + 1;
    int bytes = s_timg_siz == 0 ? (texels + 1) / 2 : texels * (bpp / 8);
    int words = (bytes + 7) / 8, i;
    uint32_t src = (s_timg & 0x7FFFFF) + (uint32_t)((ult * s_timg_w + uls) * (bpp ? bpp / 8 : 1) / (bpp ? 1 : 2));
    int t = s_tile[tile].tmem;
    for (i = 0; i < words; i++) {
        int odd = ((dxt * i) >> 11) & 1;
        tmem_put64(t + i, mem(src + i * 8), odd);
    }
}

static void load_tile(int tile, int uls, int ult, int lrs, int lrt)
{
    int bps = s_timg_siz == 0 ? 0 : (1 << s_timg_siz) / 2;   /* bytes per texel: 4b handled apart */
    int x0 = uls >> 2, y0 = ult >> 2, x1 = lrs >> 2, y1 = lrt >> 2, y;
    int line = s_tile[tile].line;
    for (y = y0; y <= y1; y++) {
        int rowbytes = s_timg_siz == 0 ? (x1 - x0 + 2) / 2 : (x1 - x0 + 1) * bps;
        uint32_t src = (s_timg & 0x7FFFFF) +
                       (uint32_t)(s_timg_siz == 0 ? (y * s_timg_w + x0) / 2 : (y * s_timg_w + x0) * bps);
        int w, words = (rowbytes + 7) / 8;
        for (w = 0; w < words; w++) {
            uint8_t buf[8];
            memcpy(buf, mem(src + w * 8), 8);
            tmem_put64(s_tile[tile].tmem + (y - y0) * line + w, buf, (y - y0) & 1);
        }
    }
}

static void load_tlut(int tile, int lrs)
{
    int n = (lrs >> 2) + 1, i;
    int t = s_tile[tile].tmem;               /* 0x100.. (the upper half) in 64-bit words */
    for (i = 0; i < n; i++) {                /* each entry quadrupled: 64 bits an entry */
        uint16_t c = rd16((s_timg & 0x7FFFFF) + i * 2);
        int k;
        for (k = 0; k < 4; k++) {
            s_tmem[(t * 8 + i * 8 + k * 2) & 0xFFF] = (uint8_t)(c >> 8);
            s_tmem[(t * 8 + i * 8 + k * 2 + 1) & 0xFFF] = (uint8_t)c;
        }
    }
}

/* a texel of tile t at (x, y) inside its line layout, as RGBA8 */
static uint8_t tm8(int a) { return s_tmem[a & 0xFFF]; }

static void texel(int t, int x, int y, uint8_t out[4])
{
    int fmt = s_tile[t].fmt, siz = s_tile[t].siz, line = s_tile[t].line * 8, base = s_tile[t].tmem * 8;
    int swap = (y & 1) ? 4 : 0, a, v;
    if (siz == 3)
        swap = (y & 1) ? 8 : 0;
    switch (siz) {
    case 0: a = base + y * line + x / 2; v = tm8(a ^ swap); v = (x & 1) ? v & 0xF : v >> 4; break;
    case 1: a = base + y * line + x; v = tm8(a ^ swap); break;
    case 2: a = base + y * line + x * 2; v = tm8(a ^ swap) << 8 | tm8((a ^ swap) + 1); break;
    default: a = base + y * line + x * 2;   /* 32-bit: split across the halves */
        out[0] = tm8(a ^ swap); out[1] = tm8((a ^ swap) + 1);
        out[2] = tm8((a ^ swap) + 0x800); out[3] = tm8((a ^ swap) + 0x801);
        return;
    }
    if (fmt == 2 || (fmt == 0 && siz < 2) ) {      /* colour-indexed (and RGBA4/8, unused, read as CI) */
        int idx = siz == 0 ? (s_tile[t].pal << 4) | v : v;
        uint16_t c = (uint16_t)(tm8(0x800 + idx * 8) << 8 | tm8(0x800 + idx * 8 + 1));
        if ((s_omh >> 14 & 3) == 3) {        /* IA16 palette */
            out[0] = out[1] = out[2] = (uint8_t)(c >> 8);
            out[3] = (uint8_t)c;
        } else {
            out[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
            out[1] = (uint8_t)(((c >> 6) & 31) * 255 / 31);
            out[2] = (uint8_t)(((c >> 1) & 31) * 255 / 31);
            out[3] = (c & 1) ? 255 : 0;
        }
        return;
    }
    switch (fmt << 4 | siz) {
    case 0x02:                                     /* RGBA16 */
        out[0] = (uint8_t)(((v >> 11) & 31) * 255 / 31);
        out[1] = (uint8_t)(((v >> 6) & 31) * 255 / 31);
        out[2] = (uint8_t)(((v >> 1) & 31) * 255 / 31);
        out[3] = (v & 1) ? 255 : 0;
        break;
    case 0x30:                                     /* IA4 */
        out[0] = out[1] = out[2] = (uint8_t)((v >> 1) * 255 / 7);
        out[3] = (v & 1) ? 255 : 0;
        break;
    case 0x31:                                     /* IA8 */
        out[0] = out[1] = out[2] = (uint8_t)((v >> 4) * 17);
        out[3] = (uint8_t)((v & 15) * 17);
        break;
    case 0x32:                                     /* IA16 */
        out[0] = out[1] = out[2] = (uint8_t)(v >> 8);
        out[3] = (uint8_t)v;
        break;
    case 0x40:                                     /* I4 */
        out[0] = out[1] = out[2] = out[3] = (uint8_t)(v * 17);
        break;
    case 0x41:                                     /* I8 */
    default:
        out[0] = out[1] = out[2] = out[3] = (uint8_t)v;
        break;
    }
}

/* ---- decoded textures, cached by their TMEM contents -------------------------------- */
#define TEXCACHE 512
static struct { uint64_t key; int tex, w, h; uint32_t used; } s_cache[TEXCACHE];
static uint32_t s_cache_clock;
static uint64_t s_cur_key;                            /* the last texture looked up (TGR_WORLDDUMP) */
static FILE *s_wdump;                                 /* TGR_WORLDDUMP: the world geometry, for tools */

static void tex_free(int tex);

static uint64_t fnv(const uint8_t *p, int n, uint64_t h)
{
    int i;
    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

static void tile_dims(int t, int *w, int *h)
{
    /* the RDP's tile coordinates are 12-bit 10.2: the size is the wrapped difference */
    int ww = (((s_tile[t].lrs - s_tile[t].uls) & 0xFFF) >> 2) + 1, hh = (((s_tile[t].lrt - s_tile[t].ult) & 0xFFF) >> 2) + 1;
    if (s_tile[t].masks && (1 << s_tile[t].masks) < ww)
        ww = 1 << s_tile[t].masks;
    if (s_tile[t].maskt && (1 << s_tile[t].maskt) < hh)
        hh = 1 << s_tile[t].maskt;
    if (ww < 1)
        ww = 1;
    if (hh < 1)
        hh = 1;
    if (ww > 1024)
        ww = 1024;
    if (hh > 1024)
        hh = 1024;
    *w = ww;
    *h = hh;
}

static int tile_texture(int t, int *pw, int *ph)
{
    int w, h, x, y, i, slot = 0;
    uint64_t key = 14695981039346656037ull;
    uint32_t oldest = 0xFFFFFFFFu;
    uint8_t *rgba;
    tile_dims(t, &w, &h);
    key = fnv((const uint8_t *)&s_tile[t], sizeof s_tile[t], key);
    key = fnv((const uint8_t *)&s_omh, 4, key);
    key = fnv(s_tmem, sizeof s_tmem, key);
    for (i = 0; i < TEXCACHE; i++) {
        if (s_cache[i].tex && s_cache[i].key == key) {
            s_cache[i].used = ++s_cache_clock;
            s_cur_key = key;
            *pw = s_cache[i].w;
            *ph = s_cache[i].h;
            return s_cache[i].tex;
        }
        if (s_cache[i].used < oldest) {
            oldest = s_cache[i].used;
            slot = i;
        }
    }
    rgba = (uint8_t *)malloc((size_t)w * h * 4);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            texel(t, x, y, rgba + (y * w + x) * 4);
    s_cur_key = key;
    if (s_wdump) {                                    /* 'X' key w h rgba: each texture once per decode */
        int32_t wh[2] = { w, h };
        fputc('X', s_wdump);
        fwrite(&key, 8, 1, s_wdump);
        fwrite(wh, 4, 2, s_wdump);
        fwrite(rgba, 4, (size_t)w * h, s_wdump);
    }
    if (s_cache[slot].tex)
        tex_free(s_cache[slot].tex);
    s_cache[slot].tex = rdr_texture(rgba, w, h);
    if (s_texdump) {
        static int n;
        char path[512];
        int brr_png_write(const char *path, const uint8_t *px, int w, int h, int stride, int order);
        int za = 0, q;
        for (q = 0; q < w * h; q++)
            za += rgba[q * 4 + 3] == 0;
        snprintf(path, sizeof path, "%s/tex%04d_h%d_f%d_s%d_%dx%d_a0-%d_pal%d.png", s_texdump, n++, s_cache[slot].tex, s_tile[t].fmt,
                 s_tile[t].siz, w, h, za, s_tile[t].pal);
        brr_png_write(path, rgba, w, h, w * 4, 0);
    }
    s_cache[slot].key = key;
    s_cache[slot].w = w;
    s_cache[slot].h = h;
    s_cache[slot].used = ++s_cache_clock;
    free(rgba);
    *pw = w;
    *ph = h;
    return s_cache[slot].tex;
}

/* ---- the state a primitive draws under ------------------------------------------------- */
static const uint8_t CC_A[16] = { RDR_CC_COMBINED, RDR_CC_TEXEL0, RDR_CC_TEXEL1, RDR_CC_PRIM, RDR_CC_SHADE,
                                  RDR_CC_ENV, RDR_CC_ONE, RDR_CC_NOISE, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO,
                                  RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO };
static const uint8_t CC_B[16] = { RDR_CC_COMBINED, RDR_CC_TEXEL0, RDR_CC_TEXEL1, RDR_CC_PRIM, RDR_CC_SHADE,
                                  RDR_CC_ENV, RDR_CC_KEYCENTER, RDR_CC_K4, RDR_CC_ZERO, RDR_CC_ZERO,
                                  RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO };
static const uint8_t CC_C[32] = { RDR_CC_COMBINED, RDR_CC_TEXEL0, RDR_CC_TEXEL1, RDR_CC_PRIM, RDR_CC_SHADE,
                                  RDR_CC_ENV, RDR_CC_KEYSCALE, RDR_CC_COMBINED_A, RDR_CC_TEXEL0_A,
                                  RDR_CC_TEXEL1_A, RDR_CC_PRIM_A, RDR_CC_SHADE_A, RDR_CC_ENV_A,
                                  RDR_CC_LOD_FRAC, RDR_CC_PRIM_LOD_FRAC, RDR_CC_K5, RDR_CC_ZERO, RDR_CC_ZERO,
                                  RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO,
                                  RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO, RDR_CC_ZERO,
                                  RDR_CC_ZERO, RDR_CC_ZERO };
static const uint8_t CC_D[8] = { RDR_CC_COMBINED, RDR_CC_TEXEL0, RDR_CC_TEXEL1, RDR_CC_PRIM, RDR_CC_SHADE,
                                 RDR_CC_ENV, RDR_CC_ONE, RDR_CC_ZERO };
static const uint8_t AC_ABD[8] = { RDR_CC_COMBINED, RDR_CC_TEXEL0, RDR_CC_TEXEL1, RDR_CC_PRIM, RDR_CC_SHADE,
                                   RDR_CC_ENV, RDR_CC_ONE, RDR_CC_ZERO };
static const uint8_t AC_C[8] = { RDR_CC_LOD_FRAC, RDR_CC_TEXEL0, RDR_CC_TEXEL1, RDR_CC_PRIM, RDR_CC_SHADE,
                                 RDR_CC_ENV, RDR_CC_PRIM_LOD_FRAC, RDR_CC_ZERO };

static int uses_texel(const RdrState *st, int which)
{
    int c, k;
    int t = which ? RDR_CC_TEXEL1 : RDR_CC_TEXEL0, ta = which ? RDR_CC_TEXEL1_A : RDR_CC_TEXEL0_A;
    for (c = 0; c < st->cycle; c++)
        for (k = 0; k < 4; k++)
            if (st->cc.rgb[c][k] == t || st->cc.rgb[c][k] == ta || st->cc.a[c][k] == t)
                return 1;
    return 0;
}

static void fill_tile(RdrTile *rt, int t)
{
    int w, h;
    memset(rt, 0, sizeof *rt);
    rt->tex = tile_texture(t, &w, &h);
    rt->w = w;
    rt->h = h;
    /* an upper left past the lower right is negative (it wrapped round 1024 texels) */
    rt->s0 = (s_tile[t].uls > s_tile[t].lrs ? s_tile[t].uls - 4096 : s_tile[t].uls) / 4.0f;
    rt->t0 = (s_tile[t].ult > s_tile[t].lrt ? s_tile[t].ult - 4096 : s_tile[t].ult) / 4.0f;
    rt->sscale = s_tile[t].shifts <= 10 ? 1.0f / (1 << s_tile[t].shifts) : (float)(1 << (16 - s_tile[t].shifts));
    rt->tscale = s_tile[t].shiftt <= 10 ? 1.0f / (1 << s_tile[t].shiftt) : (float)(1 << (16 - s_tile[t].shiftt));
    rt->clamp_s = (s_tile[t].cms & 2) != 0 || s_tile[t].masks == 0;
    rt->clamp_t = (s_tile[t].cmt & 2) != 0 || s_tile[t].maskt == 0;
    rt->mirror_s = (s_tile[t].cms & 1) != 0;
    rt->mirror_t = (s_tile[t].cmt & 1) != 0;
    rt->mask_s = (int16_t)(s_tile[t].masks ? 1 << s_tile[t].masks : 0);
    rt->clamp_w = (int16_t)((((s_tile[t].lrs - s_tile[t].uls) & 0xFFF) >> 2) + 1);
    rt->clamp_h = (int16_t)((((s_tile[t].lrt - s_tile[t].ult) & 0xFFF) >> 2) + 1);
    rt->mask_t = (int16_t)(s_tile[t].maskt ? 1 << s_tile[t].maskt : 0);
}

static void state(RdrState *st, int tile0)
{
    int cyc = (s_omh >> 20) & 3, c;
    uint32_t bl;
    memset(st, 0, sizeof *st);
    st->cycle = cyc == 1 ? 2 : 1;
    st->cc.rgb[0][0] = CC_A[(s_cc0 >> 20) & 15];
    st->cc.rgb[0][1] = CC_B[(s_cc1 >> 28) & 15];
    st->cc.rgb[0][2] = CC_C[(s_cc0 >> 15) & 31];
    st->cc.rgb[0][3] = CC_D[(s_cc1 >> 15) & 7];
    st->cc.a[0][0] = AC_ABD[(s_cc0 >> 12) & 7];
    st->cc.a[0][1] = AC_ABD[(s_cc1 >> 12) & 7];
    st->cc.a[0][2] = AC_C[(s_cc0 >> 9) & 7];
    st->cc.a[0][3] = AC_ABD[(s_cc1 >> 9) & 7];
    st->cc.rgb[1][0] = CC_A[(s_cc0 >> 5) & 15];
    st->cc.rgb[1][1] = CC_B[(s_cc1 >> 24) & 15];
    st->cc.rgb[1][2] = CC_C[s_cc0 & 31];
    st->cc.rgb[1][3] = CC_D[(s_cc1 >> 6) & 7];
    st->cc.a[1][0] = AC_ABD[(s_cc1 >> 21) & 7];
    st->cc.a[1][1] = AC_ABD[(s_cc1 >> 3) & 7];
    st->cc.a[1][2] = AC_C[(s_cc1 >> 18) & 7];
    st->cc.a[1][3] = AC_ABD[s_cc1 & 7];
    if (st->cycle == 2) {                             /* the RDP's second cycle sees the texels
                                                         swapped: its TEXEL0 is texel 1 and its
                                                         TEXEL1 texel 0 */
        static const uint8_t sw[] = { [RDR_CC_TEXEL0] = RDR_CC_TEXEL1, [RDR_CC_TEXEL1] = RDR_CC_TEXEL0,
                                      [RDR_CC_TEXEL0_A] = RDR_CC_TEXEL1_A, [RDR_CC_TEXEL1_A] = RDR_CC_TEXEL0_A };
        int k;
        for (k = 0; k < 4; k++) {
            uint8_t *r = &st->cc.rgb[1][k], *a = &st->cc.a[1][k];
            if (*r < sizeof sw && sw[*r])
                *r = sw[*r];
            if (*a < sizeof sw && sw[*a])
                *a = sw[*a];
        }
    }
    for (c = 0; c < 4; c++) {
        st->prim[c] = s_prim[c];
        st->env[c] = s_env[c];
        st->fog[c] = s_fogc[c];
        st->blend[c] = s_blendc[c];
    }
    st->prim_lod_frac = s_prim_lod;
    if (uses_texel(st, 0))
        fill_tile(&st->tile[0], tile0);
    if (st->cycle == 2 && uses_texel(st, 1)) {
        /* the second texel is the next tile, except with texture LOD on and
           no further level (G_TEXTURE's level 0): then the same tile */
        int lod = (s_omh >> 16) & 1;
        fill_tile(&st->tile[1], (lod && s_tex.level == 0) ? tile0 : (tile0 + 1) & 7);
    }
    if (((s_omh >> 16) & 1) && s_tex.level > 0 && (uses_texel(st, 0) || uses_texel(st, 1))) {
        int k;
        st->lod_levels = s_tex.level + 1;             /* tiles t .. t + level */
        if (st->lod_levels > 8)
            st->lod_levels = 8;
        for (k = 0; k < st->lod_levels; k++)
            fill_tile(&st->lod[k], (tile0 + k) & 7);
    }
    st->filter = ((s_omh >> 12) & 3) == 2;
    bl = s_oml >> 16;
    /* cycle 1's blender: P * A + M * B */
    {
        int p = (bl >> 14) & 3, a = (bl >> 10) & 3, m = (bl >> 6) & 3, b = (bl >> 2) & 3;
        int force = (s_oml & 0x4000) != 0;
        if (p == 3 && a == 2)                         /* fog colour by shade alpha */
            st->fog_blend = 1;
        if (st->cycle == 2) {                         /* the second cycle decides the mix */
            p = (bl >> 12) & 3;
            a = (bl >> 8) & 3;
            m = (bl >> 4) & 3;
            b = bl & 3;
        }
        if (p == 0 && m == 1 && a != 3 && b == 0 && force) {
            st->blend_mode = RDR_BLEND_ALPHA;         /* IN * A + MEM * (1 - A) */
            st->blend_alpha = a;                      /* A: 0 IN, 1 FOG, 2 SHADE */
        } else if (p == 0 && m == 1 && b == 2) {
            st->blend_mode = RDR_BLEND_ADD;
            st->blend_alpha = a == 3 ? 0 : a;
        }
        else if (p == 1 && m == 1)
            st->blend_mode = RDR_BLEND_MEM;
        else
            st->blend_mode = RDR_BLEND_OPAQUE;
    }
    st->alpha_compare = (s_oml & 3) == 1 ? 1 : (s_oml & 3) == 3 ? 2 : 0;
    /* coverage times alpha (CVG_X_ALPHA, the texture-edge modes): a texel's alpha
       scales the pixel's coverage, so anti-aliased it blends by that alpha and
       none drops it (poles, foliage, dust); without anti-aliasing it cuts at half.
       ALPHA_CVG_SEL alone (the opaque-surface modes) makes the pixel's alpha its
       coverage: the combiner's alpha does not thin it. */
    if ((s_oml & 0x1000) && (s_oml & 0x8) && st->blend_mode == RDR_BLEND_OPAQUE) {
        st->blend_mode = RDR_BLEND_ALPHA;
        st->blend_alpha = 0;
        st->alpha_compare = 4;
    } else if (s_oml & 0x1000) {
        st->alpha_compare = st->alpha_compare ? st->alpha_compare : 3;
    }
    st->z_test = (s_geom & G_ZBUFFER) && (s_oml & 0x10);
    st->z_write = (s_geom & G_ZBUFFER) && (s_oml & 0x20);
    st->z_decal = ((s_oml >> 10) & 3) == 3;
    st->aa = (s_oml & 0x8) != 0;
    st->force_bl = (s_oml & 0x4000) != 0;
    st->cvg_dst = (s_oml >> 8) & 3;
    st->cvg_x_alpha = (s_oml & 0x1000) != 0;
    st->rgb_dither = (s_omh >> 6) & 3;
    st->scissor[0] = s_scissor[0];
    st->scissor[1] = s_scissor[1];
    st->scissor[2] = s_scissor[2];
    st->scissor[3] = s_scissor[3];
}

/* ---- a window of any shape (tgr_view.h) ---------------------------------------------------- *
 * With TGR_FLAG_ANY_ASPECT on and a window of another shape than the N64's 4:3, a frame is
 * recorded and drawn at its end, when what it was is known.  A race frame (one that loaded a
 * race view's projection, its lens widened by kx across and ky down: drawing/frameloop.c)
 * fills the window: its colour image becomes kx fb_w by ky fb_h, still in the N64's pixels,
 * and the race views are stretched over it; the rear-view mirror keeps its shape, centred
 * along the top; and every piece of 2D (the HUD, text, panels) keeps its shape too,
 * stretched only along an axis it spans its scissor on, else held at the distance from the
 * edge (or the centre) it sat nearest.  2D draws are grouped into the elements they make up
 * (touching boxes, words on one line), so a gauge or a line of text moves as one: the Boss
 * Rally port's placement (ports/brally/platform/common/glide.c).  Any other frame is drawn
 * as it was, and the renderer keeps it 4:3 in the window. */
enum { K_2D, K_RACE, K_MIRROR };
typedef struct { double ax, bx, ay, by; } XMap;     /* target = a * game + b (b times w) */
typedef struct {
    int type;                   /* 0 triangles, 1 a rectangle, 2 a depth clear */
    int st, kind, v0, n, fill;
    float x0, y0, x1, y1, s, t, dsdx, dtdy, rgba[4];
    float box[4], clip[4];      /* the draw's box and its scissor, game pixels */
} Rec;
static int s_kind;              /* what the loaded projection draws (K_*) */
static int s_rec;               /* this frame is recorded */
static int s_race;              /* ... and a race view was drawn in it, its lens kx by ky */
static float s_kx = 1, s_ky = 1;

/* the frame's tap targets are on the screen, in its shape (tgr_touch.h) */
static void touch_shown(void) { tgr_touch_shown((float)s_fb_w, (float)s_fb_h, s_rec && s_race, s_kx, s_ky); }
static float s_mirror_clip[4];
static int s_mirror_ok;
static Rec *s_recs;
static int s_nrec, s_caprec;
static RdrVtx *s_rv;
static int s_nrv, s_caprv;
static RdrState *s_rst;
static int s_nrst, s_caprst;
static int *s_freed;            /* textures let go while recording: freed after the frame */
static int s_nfreed, s_capfreed;

#define GROW(p, n, cap, more)                                                          \
    do {                                                                               \
        if ((n) + (more) > (cap)) {                                                    \
            (cap) = ((n) + (more)) * 2 + 1024;                                         \
            (p) = realloc((p), (size_t)(cap) * sizeof *(p));                           \
        }                                                                              \
    } while (0)

static void tex_free(int tex)
{
    if (!s_rec) {
        rdr_texture_free(tex);
        return;
    }
    GROW(s_freed, s_nfreed, s_capfreed, 1);
    s_freed[s_nfreed++] = tex;
}

static int full_clip(const float *r) { return r[0] <= 0 && r[1] <= 0 && r[2] >= s_fb_w && r[3] >= s_fb_h; }

static Rec *rec_new(int type, const RdrState *st)
{
    Rec *r;
    GROW(s_recs, s_nrec, s_caprec, 1);
    r = &s_recs[s_nrec++];
    memset(r, 0, sizeof *r);
    r->type = type;
    r->kind = K_2D;
    if (st) {
        if (!s_nrst || memcmp(&s_rst[s_nrst - 1], st, sizeof *st)) {
            GROW(s_rst, s_nrst, s_caprst, 1);
            s_rst[s_nrst++] = *st;
        }
        r->st = s_nrst - 1;
        r->clip[0] = fmaxf(st->scissor[0], 0);
        r->clip[1] = fmaxf(st->scissor[1], 0);
        r->clip[2] = fminf(st->scissor[2], (float)s_fb_w);
        r->clip[3] = fminf(st->scissor[3], (float)s_fb_h);
        if (r->clip[2] <= r->clip[0] || r->clip[3] <= r->clip[1]) {
            r->clip[0] = r->clip[1] = 0;
            r->clip[2] = (float)s_fb_w;
            r->clip[3] = (float)s_fb_h;
        }
    }
    return r;
}

static void put_tris(const RdrState *st, const RdrVtx *v, int n)
{
    Rec *r;
    int k;
    if (!s_rec) {
        rdr_triangles(st, v, n);
        return;
    }
    r = rec_new(0, st);
    r->kind = s_kind;
    r->v0 = s_nrv;
    r->n = n;
    GROW(s_rv, s_nrv, s_caprv, n);
    memcpy(s_rv + s_nrv, v, (size_t)n * sizeof *v);
    s_nrv += n;
    r->box[0] = r->box[1] = 1e9f;
    r->box[2] = r->box[3] = -1e9f;
    for (k = 0; k < n; k++) {
        float x, y;
        if (v[k].w <= 1e-6f)
            continue;
        x = v[k].x / v[k].w;
        y = v[k].y / v[k].w;
        r->box[0] = fminf(r->box[0], x);
        r->box[1] = fminf(r->box[1], y);
        r->box[2] = fmaxf(r->box[2], x);
        r->box[3] = fmaxf(r->box[3], y);
    }
    if (r->box[2] < r->box[0])
        memcpy(r->box, r->clip, sizeof r->box);
    if (s_kind == K_MIRROR && !s_mirror_ok) {
        memcpy(s_mirror_clip, r->clip, sizeof s_mirror_clip);
        s_mirror_ok = 1;
    }
}

static void put_rect(const RdrState *st, float x0, float y0, float x1, float y1, float s, float t, float dsdx,
                     float dtdy, int fill, const float rgba[4])
{
    Rec *r;
    if (!s_rec) {
        rdr_rect(st, x0, y0, x1, y1, s, t, dsdx, dtdy, fill, rgba);
        return;
    }
    r = rec_new(1, st);
    r->x0 = x0; r->y0 = y0; r->x1 = x1; r->y1 = y1;
    r->s = s; r->t = t; r->dsdx = dsdx; r->dtdy = dtdy;
    r->fill = fill;
    if (rgba)
        memcpy(r->rgba, rgba, sizeof r->rgba);
    r->box[0] = fmaxf(x0, r->clip[0]);
    r->box[1] = fmaxf(y0, r->clip[1]);
    r->box[2] = fminf(x1, r->clip[2]);
    r->box[3] = fminf(y1, r->clip[3]);
    if (r->box[2] <= r->box[0] || r->box[3] <= r->box[1]) {
        r->box[0] = x0; r->box[1] = y0;
        r->box[2] = x1; r->box[3] = y1;
    }
}

static void put_clear_depth(void)
{
    if (!s_rec)
        rdr_clear_depth();
    else
        rec_new(2, NULL);
}

/* a projection loaded: what it draws */
static void proj_loaded(uint32_t phys)
{
    float kx, ky;
    s_kind = tgr_view_proj_kind(mem(phys), &kx, &ky);
    if (s_kind == K_RACE) {                           /* (the frame may not have opened yet) */
        s_race = 1;
        s_kx = kx;
        s_ky = ky;
    }
}

/* One axis of an element's placement: stretched with the window when it spans its
 * scissor, else its own size, kept at the distance from the edge (or the centre) it sat
 * nearest (k the window's stretch on this axis) */
static void place(double lo, double hi, double r0, double r1, double k, double *a, double *b)
{
    double rw = r1 - r0;
    if (hi - lo >= 0.9 * rw) { *a = k; *b = 0; return; }
    *a = 1;
    if (hi <= r0 + 0.42 * rw) *b = r0 * k - r0;
    else if (lo >= r0 + 0.58 * rw) *b = r1 * k - r1;
    else *b = (r0 + r1) * 0.5 * (k - 1);
}

static void follow(double lo, double hi, double r0, double r1, double k, double *b)
{
    double e = lo + hi < r0 + r1 ? lo : hi;
    *b = e * (k - 1);
}

static int el_root(int *p, int i) { while (p[i] != i) i = p[i] = p[p[i]]; return i; }

static int spans(const Rec *a)
{
    return a->box[2] - a->box[0] >= 0.9f * (a->clip[2] - a->clip[0]) ||
           a->box[3] - a->box[1] >= 0.9f * (a->clip[3] - a->clip[1]);
}

/* every record's map, for a race frame: the 2D ones by the element they belong to (the 2D
 * draws of one scissor whose boxes touch, within 6 game pixels, or that sit on one row a
 * few spaces apart).  A draw spanning its scissor joins none. */
static void place_frame(XMap *map)
{
    static int *par, cap;
    static float (*gb)[4];
    XMap mirror = { 1, (s_fb_w * s_kx - s_fb_w) * 0.5, 1, 0 }, wide = { s_kx, 0, s_ky, 0 };
    int i, j, n = s_nrec;
    if (n > cap) {
        cap = n * 2;
        par = realloc(par, (size_t)cap * sizeof *par);
        gb = realloc(gb, (size_t)cap * sizeof *gb);
    }
    for (i = 0; i < n; i++) {
        Rec *a = &s_recs[i];
        par[i] = i;
        /* a fill, or triangles cut to their own box (a panel, the mirror's frame): an
           element of the whole screen, so it groups with what it borders */
        if (a->type != 2 && a->kind == K_2D && !full_clip(a->clip) &&
            !(s_mirror_ok && !memcmp(a->clip, s_mirror_clip, sizeof a->clip)) &&
            a->box[2] - a->box[0] >= 0.9f * (a->clip[2] - a->clip[0]) &&
            a->box[3] - a->box[1] >= 0.9f * (a->clip[3] - a->clip[1])) {
            a->clip[0] = a->clip[1] = 0;
            a->clip[2] = (float)s_fb_w;
            a->clip[3] = (float)s_fb_h;
        }
    }
    if (n <= 6000)
        for (i = 0; i < n; i++) {
            const Rec *a = &s_recs[i];
            if (a->type == 2 || a->kind != K_2D || spans(a))
                continue;
            for (j = i + 1; j < n; j++) {
                const Rec *b = &s_recs[j];
                if (b->type == 2 || b->kind != K_2D || memcmp(a->clip, b->clip, sizeof a->clip) || spans(b))
                    continue;
                if ((a->box[0] - 6 <= b->box[2] && b->box[0] - 6 <= a->box[2] && a->box[1] - 6 <= b->box[3] &&
                     b->box[1] - 6 <= a->box[3]) ||
                    /* words of one line of text: the same row, a few spaces apart */
                    (fabsf(a->box[1] - b->box[1]) < 2 && fabsf(a->box[3] - b->box[3]) < 2 &&
                     a->box[0] - 40 <= b->box[2] && b->box[0] - 40 <= a->box[2])) {
                    int ra = el_root(par, i), rb = el_root(par, j);
                    if (ra != rb)
                        par[rb] = ra;
                }
            }
        }
    for (i = 0; i < n; i++) {
        gb[i][0] = gb[i][1] = 1e9f;
        gb[i][2] = gb[i][3] = -1e9f;
    }
    for (i = 0; i < n; i++) {
        int r = el_root(par, i);
        const Rec *a = &s_recs[i];
        gb[r][0] = fminf(gb[r][0], a->box[0]);
        gb[r][1] = fminf(gb[r][1], a->box[1]);
        gb[r][2] = fmaxf(gb[r][2], a->box[2]);
        gb[r][3] = fmaxf(gb[r][3], a->box[3]);
    }
    for (i = 0; i < n; i++) {
        const Rec *a = &s_recs[i];
        const float *g = gb[el_root(par, i)];
        if (a->kind == K_RACE || a->type == 2)
            map[i] = wide;
        else if (a->kind == K_MIRROR || (s_mirror_ok && !memcmp(a->clip, s_mirror_clip, sizeof a->clip)))
            map[i] = mirror;
        else {
            int sx = g[2] - g[0] >= 0.9f * (a->clip[2] - a->clip[0]), sy = g[3] - g[1] >= 0.9f * (a->clip[3] - a->clip[1]);
            place(g[0], g[2], a->clip[0], a->clip[2], s_kx, &map[i].ax, &map[i].bx);
            place(g[1], g[3], a->clip[1], a->clip[3], s_ky, &map[i].ay, &map[i].by);
            /* a bar the length of one side (a view's outline, a divider) goes with the
               stretched views across it: its nearer edge where theirs goes */
            if (sy && !sx)
                follow(g[0], g[2], a->clip[0], a->clip[2], s_kx, &map[i].bx);
            if (sx && !sy)
                follow(g[1], g[3], a->clip[1], a->clip[3], s_ky, &map[i].by);
        }
    }
}

/* the scissor in the window's image: a whole-screen one stays whole, the mirror's goes
 * with the mirror, any other is stretched with the views (2D moves within it) */
static void map_scissor(RdrState *st, const Rec *r)
{
    float *sc = st->scissor;
    double vw = s_fb_w * s_kx, vh = s_fb_h * s_ky;
    XMap m = { s_kx, 0, s_ky, 0 };
    if (sc[0] <= 0 && sc[1] <= 0 && sc[2] >= s_fb_w && sc[3] >= s_fb_h) {
        sc[0] = sc[1] = 0;
        sc[2] = (float)vw;
        sc[3] = (float)vh;
        return;
    }
    if (r->kind == K_MIRROR || (s_mirror_ok && !memcmp(r->clip, s_mirror_clip, sizeof r->clip)))
        m = (XMap){ 1, (vw - s_fb_w) * 0.5, 1, 0 };
    sc[0] = (float)(m.ax * sc[0] + m.bx);
    sc[2] = (float)(m.ax * sc[2] + m.bx);
    sc[1] = (float)(m.ay * sc[1] + m.by);
    sc[3] = (float)(m.ay * sc[3] + m.by);
}

/* the recorded frame, drawn */
static void rec_flush(void)
{
    static XMap *map;
    static int mapcap;
    static RdrVtx *tv;
    static int tvcap;
    int i, k, race = s_race;
    if (!s_rec) {
        s_race = 0;
        s_kx = s_ky = 1;
        return;
    }
    s_rec = 0;                                        /* from here, draws and frees are direct */
    if (race) {
        if (s_nrec > mapcap) {
            mapcap = s_nrec * 2;
            map = realloc(map, (size_t)mapcap * sizeof *map);
        }
        place_frame(map);
        rdr_frame_begin(s_fb_w * s_kx, s_fb_h * s_ky);
    } else {
        rdr_frame_begin((float)s_fb_w, (float)s_fb_h);
    }
    for (i = 0; i < s_nrec; i++) {
        const Rec *r = &s_recs[i];
        RdrState st;
        if (r->type == 2) {
            rdr_clear_depth();
            continue;
        }
        st = s_rst[r->st];
        if (!race) {
            if (r->type == 0)
                rdr_triangles(&st, s_rv + r->v0, r->n);
            else
                rdr_rect(&st, r->x0, r->y0, r->x1, r->y1, r->s, r->t, r->dsdx, r->dtdy, r->fill, r->rgba);
            continue;
        }
        map_scissor(&st, r);
        if (r->type == 0) {
            const XMap *m = &map[i];
            if (r->n > tvcap) {
                tvcap = r->n * 2;
                tv = realloc(tv, (size_t)tvcap * sizeof *tv);
            }
            for (k = 0; k < r->n; k++) {
                tv[k] = s_rv[r->v0 + k];
                tv[k].x = (float)(m->ax * tv[k].x + m->bx * tv[k].w);
                tv[k].y = (float)(m->ay * tv[k].y + m->by * tv[k].w);
            }
            rdr_triangles(&st, tv, r->n);
        } else {
            const XMap *m = &map[i];
            rdr_rect(&st, (float)(m->ax * r->x0 + m->bx), (float)(m->ay * r->y0 + m->by),
                     (float)(m->ax * r->x1 + m->bx), (float)(m->ay * r->y1 + m->by), r->s, r->t,
                     (float)(r->dsdx / m->ax), (float)(r->dtdy / m->ay), r->fill, r->rgba);
        }
    }
    for (i = 0; i < s_nfreed; i++)
        rdr_texture_free(s_freed[i]);
    s_nfreed = s_nrec = s_nrv = s_nrst = 0;
    s_race = 0;
    s_kx = s_ky = 1;
    s_mirror_ok = 0;
}

static void frame_open(void)
{
    if (!s_frame_open) {
        float kx, ky;
        tgr_view_scale(&kx, &ky);
        s_rec = kx > 1.0f || ky > 1.0f;               /* drawn at the end (above) */
        s_mirror_ok = 0;
        if (!s_rec)
            rdr_frame_begin((float)s_fb_w, (float)s_fb_h);
        s_frame_open = 1;
    }
}

/* The RDP draws a pixel when any of its 8 coverage samples (4 sub-scanlines,
 * 2 across) falls inside the triangle, so a mesh seam narrower than a pixel
 * never opens.  A renderer that samples pixel centres covers the same pixels
 * with the triangle grown by half a pixel (its edges pushed out) and cut to
 * its bounding box grown by half a pixel (so a sharp corner cannot spike):
 * the result is a convex polygon, its attributes found perspective-correctly
 * from the triangle.  It is drawn just behind the triangle itself, so it fills
 * a seam but never paints over the neighbour sharing an edge.  Triangles of
 * under a pixel are left alone (the RDP's samples mostly miss them).
 * Returns the polygon's vertex count (0: not grown). */
static int cover_like_rdp(const RdrVtx v[3], RdrVtx out[8])
{
    float x[3], y[3], iw[3], nx[3], ny[3], px[16], py[16], qx[16], qy[16];
    float x0, y0, x1, y1, cross;
    int k, n, e;
    for (k = 0; k < 3; k++) {
        if (v[k].w <= 0.001f)
            return 0;                                 /* clipped later: left as is */
        iw[k] = 1.0f / v[k].w;
        x[k] = v[k].x * iw[k];
        y[k] = v[k].y * iw[k];
    }
    cross = (x[1] - x[0]) * (y[2] - y[0]) - (y[1] - y[0]) * (x[2] - x[0]);
    if (fabsf(cross) < 2.0f)
        return 0;
    for (k = 0; k < 3; k++) {                         /* edge k: vertex k to k + 1, outward normal */
        int j = (k + 1) % 3, o = (k + 2) % 3;
        float dx = x[j] - x[k], dy = y[j] - y[k], l = sqrtf(dx * dx + dy * dy);
        nx[k] = -dy / l;
        ny[k] = dx / l;
        if (nx[k] * (x[o] - x[k]) + ny[k] * (y[o] - y[k]) > 0) {   /* away from the third vertex */
            nx[k] = -nx[k];
            ny[k] = -ny[k];
        }
    }
    for (k = 0; k < 3; k++) {                         /* vertex k joins edges k - 1 and k */
        int i = (k + 2) % 3;
        float d = 1.0f + nx[i] * nx[k] + ny[i] * ny[k], f = 0.5f / (d < 1e-4f ? 1e-4f : d);
        px[k] = x[k] + (nx[i] + nx[k]) * f;
        py[k] = y[k] + (ny[i] + ny[k]) * f;
    }
    x0 = fminf(x[0], fminf(x[1], x[2])) - 0.5f;
    x1 = fmaxf(x[0], fmaxf(x[1], x[2])) + 0.5f;
    y0 = fminf(y[0], fminf(y[1], y[2])) - 0.5f;
    y1 = fmaxf(y[0], fmaxf(y[1], y[2])) + 0.5f;
    n = 3;
    for (e = 0; e < 4 && n > 0; e++) {                /* cut to the box, one side at a time */
        int m = 0;
        for (k = 0; k < n; k++) {
            int j = (k + 1) % n;
            float dk = e == 0 ? px[k] - x0 : e == 1 ? x1 - px[k] : e == 2 ? py[k] - y0 : y1 - py[k];
            float dj = e == 0 ? px[j] - x0 : e == 1 ? x1 - px[j] : e == 2 ? py[j] - y0 : y1 - py[j];
            if (dk >= 0) {
                qx[m] = px[k];
                qy[m++] = py[k];
            }
            if ((dk >= 0) != (dj >= 0)) {
                float t = dk / (dk - dj);
                qx[m] = px[k] + (px[j] - px[k]) * t;
                qy[m++] = py[k] + (py[j] - py[k]) * t;
            }
        }
        n = m > 8 ? 8 : m;
        memcpy(px, qx, sizeof px);
        memcpy(py, qy, sizeof py);
    }
    for (k = 0; k < n; k++) {                         /* each corner's attributes, from the triangle */
        float l0 = ((x[1] - px[k]) * (y[2] - py[k]) - (y[1] - py[k]) * (x[2] - px[k])) / cross;
        float l1 = ((x[2] - px[k]) * (y[0] - py[k]) - (y[2] - py[k]) * (x[0] - px[k])) / cross;
        float l2 = 1.0f - l0 - l1, q = l0 * iw[0] + l1 * iw[1] + l2 * iw[2], w;
        RdrVtx *o = &out[k];
        if (q <= 1e-6f)
            return 0;
        w = 1.0f / q;
#define PC(f) ((l0 * v[0].f * iw[0] + l1 * v[1].f * iw[1] + l2 * v[2].f * iw[2]) * w)
        o->w = w;
        o->x = px[k] * w;
        o->y = py[k] * w;
        o->z = PC(z);
        o->s = PC(s);
        o->t = PC(t);
        o->r = PC(r);
        o->g = PC(g);
        o->b = PC(b);
        o->a = PC(a);
#undef PC
    }
    return n;
}

static void triangle(int a, int b, int c)
{
    Vtx *v[3];
    RdrState st;
    RdrVtx out[3];
    int k, skip;
    if (a >= 32 || b >= 32 || c >= 32)
        return;
    v[0] = &s_vtx[a];
    v[1] = &s_vtx[b];
    v[2] = &s_vtx[c];
    if (v[0]->clip & v[1]->clip & v[2]->clip & 15) {
        if (s_log)
            fprintf(stderr, "offscreen clip %d %d %d\n", v[0]->clip, v[1]->clip, v[2]->clip);
        return;                                       /* wholly off one side */
    }
    {
        /* the facing, from the homogeneous coordinates: det(x, y, w) is the
           screen-space cross product times w0 w1 w2, so it is the same test
           in front of the camera, and it still holds for a triangle reaching
           behind it (what the RSP's clipper hands on keeps that facing) */
        const RdrVtx *p = &v[0]->v, *q = &v[1]->v, *r = &v[2]->v;
        float det = p->x * (q->y * r->w - r->y * q->w) - p->y * (q->x * r->w - r->x * q->w) +
                    p->w * (q->x * r->y - r->x * q->y);
        if (((s_geom & G_CULL_BACK) && det >= 0) || ((s_geom & G_CULL_FRONT) && det <= 0)) {
            if (s_log)                                /* screen y runs down: clockwise is < 0 */
                fprintf(stderr, "culled geom %08X det %g w %.2f %.2f %.2f\n", s_geom, det, p->w, q->w, r->w);
            return;
        }
    }
    {
        static int count;
        static uint32_t frame = 0xFFFFFFFF;
        if (frame != tgr_frame()) {
            frame = tgr_frame();
            count = 0;
            if (s_log)
                fprintf(stderr, "frame %u\n", frame);
        }
        count++;
        tgr_rcp_tri = count;
        skip = (s_only >= 0 && count > s_only) || count <= s_skip;
    }
    frame_open();
    state(&st, s_tex.tile);
    if (s_wdump) {
        /* 'C' frame proj[16] vp[6] when the projection changes, then 'T' frame textured key world(bits)
           xyzw[12] (world, or clip under a forced matrix) rgba[12] st[6] vertex addresses[3]
           tile: s0 t0 sscale tscale, clamp s/t mirror s/t, mask s/t clamp w/h */
        static uint32_t cam_frame = 0xFFFFFFFF;
        static M4 cam_proj;
        uint32_t fr = tgr_frame();
        float rec[30];
        int32_t textured = st.tile[0].tex != 0, world = v[0]->world | v[1]->world << 1 | v[2]->world << 2;
        int q;
        if (cam_frame != fr || memcmp(&cam_proj, &s_proj, sizeof s_proj)) {   /* each projection in use */
            cam_frame = fr;
            cam_proj = s_proj;
            fputc('C', s_wdump);
            fwrite(&fr, 4, 1, s_wdump);
            fwrite(&s_proj, 4, 16, s_wdump);
            fwrite(&s_vp, 4, 6, s_wdump);
        }
        for (q = 0; q < 3; q++) {
            rec[q * 4] = v[q]->wx;
            rec[q * 4 + 1] = v[q]->wy;
            rec[q * 4 + 2] = v[q]->wz;
            rec[q * 4 + 3] = v[q]->ww;
            rec[12 + q * 4] = v[q]->v.r;
            rec[13 + q * 4] = v[q]->v.g;
            rec[14 + q * 4] = v[q]->v.b;
            rec[15 + q * 4] = v[q]->v.a;
            rec[24 + q * 2] = v[q]->v.s;
            rec[25 + q * 2] = v[q]->v.t;
        }
        if (textured) {                               /* texel 0's own image, not the last tile looked up */
            for (q = 0; q < TEXCACHE; q++)
                if (s_cache[q].tex == st.tile[0].tex) {
                    s_cur_key = s_cache[q].key;
                    break;
                }
        }
        fputc('T', s_wdump);
        fwrite(&fr, 4, 1, s_wdump);
        fwrite(&textured, 4, 1, s_wdump);
        fwrite(&world, 4, 1, s_wdump);
        fwrite(&s_cur_key, 8, 1, s_wdump);
        fwrite(rec, 4, 30, s_wdump);
        for (q = 0; q < 3; q++)
            fwrite(&v[q]->addr, 4, 1, s_wdump);
        {                                             /* how texel 0's tile maps the coordinates */
            const RdrTile *tl = &st.tile[0];
            float ft[4] = { tl->s0, tl->t0, tl->sscale, tl->tscale };
            uint8_t fl[4] = { tl->clamp_s, tl->clamp_t, tl->mirror_s, tl->mirror_t };
            int16_t mk[4] = { tl->mask_s, tl->mask_t, tl->clamp_w, tl->clamp_h };
            fwrite(ft, 4, 4, s_wdump);
            fwrite(fl, 1, 4, s_wdump);
            fwrite(mk, 2, 4, s_wdump);
        }
    }
    if (s_log)
        fprintf(stderr, "tri #%d geom %08X omh %08X oml %08X cc %06X %08X tex %d %dx%d tex1 %d y %.2f %.2f %.2f w %.1f x %.1f %.1f %.1f zw %.3f %.3f %.3f w %.2f %.2f %.2f\n",
                tgr_rcp_tri, s_geom, s_omh, s_oml, s_cc0, s_cc1, st.tile[0].tex, st.tile[0].w, st.tile[0].h, st.tile[1].tex,
                v[0]->v.y / v[0]->v.w, v[1]->v.y / v[1]->v.w, v[2]->v.y / v[2]->v.w, v[0]->v.w,
                v[0]->v.x / v[0]->v.w, v[1]->v.x / v[1]->v.w, v[2]->v.x / v[2]->v.w,
                v[0]->v.z / v[0]->v.w, v[1]->v.z / v[1]->v.w, v[2]->v.z / v[2]->v.w, v[0]->v.w, v[1]->v.w, v[2]->v.w),
        fprintf(stderr, "    tile%d fmt %d siz %d line %d tmem %d pal %d uls %d ult %d lrs %d lrt %d mask %d/%d shift %d/%d s %.1f t %.1f timg %X\n",
                s_tex.tile, s_tile[s_tex.tile].fmt, s_tile[s_tex.tile].siz, s_tile[s_tex.tile].line,
                s_tile[s_tex.tile].tmem, s_tile[s_tex.tile].pal, s_tile[s_tex.tile].uls, s_tile[s_tex.tile].ult,
                s_tile[s_tex.tile].lrs, s_tile[s_tex.tile].lrt, s_tile[s_tex.tile].masks, s_tile[s_tex.tile].maskt,
                s_tile[s_tex.tile].shifts, s_tile[s_tex.tile].shiftt, v[0]->v.s, v[0]->v.t, s_timg),
        fprintf(stderr, "    tile%d+1 fmt %d siz %d line %d tmem %d pal %d uls %d ult %d lrs %d lrt %d mask %d/%d shift %d/%d tex1 %dx%d\n",
                (s_tex.tile + 1) & 7, s_tile[(s_tex.tile + 1) & 7].fmt, s_tile[(s_tex.tile + 1) & 7].siz,
                s_tile[(s_tex.tile + 1) & 7].line, s_tile[(s_tex.tile + 1) & 7].tmem, s_tile[(s_tex.tile + 1) & 7].pal,
                s_tile[(s_tex.tile + 1) & 7].uls, s_tile[(s_tex.tile + 1) & 7].ult, s_tile[(s_tex.tile + 1) & 7].lrs,
                s_tile[(s_tex.tile + 1) & 7].lrt, s_tile[(s_tex.tile + 1) & 7].masks, s_tile[(s_tex.tile + 1) & 7].maskt,
                s_tile[(s_tex.tile + 1) & 7].shifts, s_tile[(s_tex.tile + 1) & 7].shiftt, st.tile[1].w, st.tile[1].h),
        fprintf(stderr, "    fog %.2f %.2f %.2f %.2f blend %.2f\n", s_fogc[0], s_fogc[1], s_fogc[2], s_fogc[3], s_blendc[3]),
        fprintf(stderr, "    shade %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f\n",
                v[0]->v.r, v[0]->v.g, v[0]->v.b, v[0]->v.a, v[1]->v.r, v[1]->v.g, v[1]->v.b, v[1]->v.a,
                v[2]->v.r, v[2]->v.g, v[2]->v.b, v[2]->v.a),
        fprintf(stderr, "    prim %.2f %.2f %.2f %.2f env %.2f %.2f %.2f %.2f texscale %04X %04X st %.1f,%.1f %.1f,%.1f %.1f,%.1f lookat %d %d %d / %d %d %d\n",
                s_prim[0], s_prim[1], s_prim[2], s_prim[3], s_env[0], s_env[1], s_env[2], s_env[3], s_tex.ss, s_tex.ts,
                v[0]->v.s, v[0]->v.t, v[1]->v.s, v[1]->v.t, v[2]->v.s, v[2]->v.t, s_lookat[0][0], s_lookat[0][1], s_lookat[0][2],
                s_lookat[1][0], s_lookat[1][1], s_lookat[1][2]);
    for (k = 0; k < 3; k++) {
        out[k] = v[k]->v;
        if (!(s_geom & G_SHADING_SMOOTH)) {           /* flat: the first vertex's colour */
            out[k].r = v[0]->v.r;
            out[k].g = v[0]->v.g;
            out[k].b = v[0]->v.b;
        }
    }
    if (skip)
        return;
    if (!rdr_covers() && st.z_test && !st.z_decal && (st.blend_mode == RDR_BLEND_OPAQUE || st.alpha_compare == 4)) {
        RdrVtx poly[8], fan[18];                      /* not translucent: the RDP's coverage first */
        int n = cover_like_rdp(out, poly), m = 0;
        for (k = 0; k < n; k++)
            poly[k].z += 2e-4f * poly[k].w;           /* a hair behind the triangle */
        for (k = 1; k + 1 < n; k++) {
            fan[m++] = poly[0];
            fan[m++] = poly[k];
            fan[m++] = poly[k + 1];
        }
        if (m)
            put_tris(&st, fan, m);
    }
    put_tris(&st, out, 3);
}

/* ---- the display list ----------------------------------------------------------------- */
static void rect_tex(uint32_t w0, uint32_t w1, uint32_t h2, uint32_t hc)
{
    RdrState st;
    float x1 = ((w0 >> 12) & 0xFFF) / 4.0f, y1 = (w0 & 0xFFF) / 4.0f;
    float x0 = ((w1 >> 12) & 0xFFF) / 4.0f, y0 = (w1 & 0xFFF) / 4.0f;
    int tile = (w1 >> 24) & 7, cyc = (s_omh >> 20) & 3;
    float s = (int16_t)(h2 >> 16) / 32.0f, t = (int16_t)h2 / 32.0f;
    float dsdx = (int16_t)(hc >> 16) / 1024.0f, dtdy = (int16_t)hc / 1024.0f;
    frame_open();
    state(&st, tile);
    if (s_log)
        fprintf(stderr, "texrect cyc %d omh %08X oml %08X cc %06X %08X tile%d fmt %d siz %d line %d tmem %d mask %d/%d blend %d ac %d tex %d %dx%d\n",
                cyc, s_omh, s_oml, s_cc0, s_cc1, tile, s_tile[tile].fmt, s_tile[tile].siz, s_tile[tile].line,
                s_tile[tile].tmem, s_tile[tile].masks, s_tile[tile].maskt, st.blend_mode, st.alpha_compare,
                st.tile[0].tex, st.tile[0].w, st.tile[0].h);
    if (cyc == 2) {                                   /* copy mode: inclusive, 4 texels a step */
        dsdx /= 4.0f;
        x1 += 1;
        y1 += 1;
        st.cycle = 1;
        memset(&st.cc, 0, sizeof st.cc);
        st.cc.rgb[0][0] = st.cc.rgb[0][1] = st.cc.rgb[0][2] = RDR_CC_ZERO;
        st.cc.rgb[0][3] = RDR_CC_TEXEL0;
        st.cc.a[0][0] = st.cc.a[0][1] = st.cc.a[0][2] = RDR_CC_ZERO;
        st.cc.a[0][3] = RDR_CC_TEXEL0;
        fill_tile(&st.tile[0], tile);
        st.blend_mode = RDR_BLEND_OPAQUE;
        st.alpha_compare = (s_oml & 3) ? 1 : 0;
        st.filter = 0;                                /* copy mode does not filter */
        st.z_test = st.z_write = 0;
    } else {
        st.z_test = st.z_test && 0;                   /* rectangles take the primitive depth: unused */
        if (!st.tile[0].tex)
            fill_tile(&st.tile[0], tile);
    }
    put_rect(&st, x0, y0, x1, y1, s, t, dsdx, dtdy, 0, NULL);
}

static void rect_fill(uint32_t w0, uint32_t w1)
{
    RdrState st;
    float x1 = ((w0 >> 12) & 0xFFF) / 4.0f, y1 = (w0 & 0xFFF) / 4.0f;
    float x0 = ((w1 >> 12) & 0xFFF) / 4.0f, y0 = (w1 & 0xFFF) / 4.0f;
    int cyc = (s_omh >> 20) & 3;
    float c[4];
    frame_open();
    if (s_log)
        fprintf(stderr, "fillrect cyc %d %.0f,%.0f-%.0f,%.0f fill %08X cimg %X zimg %X cc %06X %08X prim %.2f %.2f %.2f oml %08X\n",
                cyc, x0, y0, x1, y1, s_fill, s_cimg, s_zimg, s_cc0, s_cc1, s_prim[0], s_prim[1], s_prim[2], s_oml);
    if (cyc == 3 && s_cimg == s_zimg) {               /* the depth buffer filled: a depth clear */
        put_clear_depth();
        return;
    }
    if (cyc == 3 && s_cimg != s_zimg) {
        uint16_t p = (uint16_t)(s_fill >> 16);
        c[0] = ((p >> 11) & 31) / 31.0f;
        c[1] = ((p >> 6) & 31) / 31.0f;
        c[2] = ((p >> 1) & 31) / 31.0f;
        c[3] = (float)(p & 1);                        /* the alpha bit: full coverage, or none */
        memset(&st, 0, sizeof st);
        st.scissor[0] = s_scissor[0];
        st.scissor[1] = s_scissor[1];
        st.scissor[2] = s_scissor[2];
        st.scissor[3] = s_scissor[3];
        put_rect(&st, x0, y0, x1 + 1, y1 + 1, 0, 0, 0, 0, 1, c);
        return;
    }
    state(&st, 0);                                    /* 1/2-cycle: the combiner's colour */
    st.tile[0].tex = st.tile[1].tex = 0;
    put_rect(&st, x0, y0, x1, y1, 0, 0, 0, 0, 2, NULL);
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
            if (p & 1) {                              /* projection */
                if (p & 2) {
                    s_proj = m;
                    proj_loaded(seg_addr(w1));        /* a race view's? (a window of any shape) */
                } else {
                    m4_mul(&s_proj, &m, &s_proj);
                    s_kind = K_2D;
                }
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
            s_mvp_forced = 0;
            mvp_update();
            if (s_log)
                fprintf(stderr, "mtx p %d depth %d  %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f\n",
                        p, s_mvn, m.m[0][0], m.m[0][1], m.m[0][2], m.m[0][3], m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3],
                        m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3], m.m[3][0], m.m[3][1], m.m[3][2], m.m[3][3]);
            break;
        }
        case 0x03: {                                  /* G_MOVEMEM */
            int idx = (w0 >> 16) & 0xFF;
            uint32_t a = seg_addr(w1);
            if (idx == 0x80) {                        /* the viewport */
                s_vp.sx = (int16_t)rd16(a) / 4.0f;
                s_vp.sy = (int16_t)rd16(a + 2) / 4.0f;
                s_vp.sz = (int16_t)rd16(a + 4) / 4.0f;
                s_vp.tx = (int16_t)rd16(a + 8) / 4.0f;
                s_vp.ty = (int16_t)rd16(a + 10) / 4.0f;
                s_vp.tz = (int16_t)rd16(a + 12) / 4.0f;
                if (s_log)
                    fprintf(stderr, "viewport scale %g %g %g trans %g %g %g\n", s_vp.sx, s_vp.sy, s_vp.sz, s_vp.tx, s_vp.ty, s_vp.tz);
            } else if (idx == 0x82 || idx == 0x84) {  /* look-at y, x */
                int k = idx == 0x84 ? 0 : 1, j;
                for (j = 0; j < 3; j++)
                    s_lookat[k][j] = (int8_t)mem(a + 8)[j];
            } else if (idx >= 0x86 && idx <= 0x94) {  /* light 1..8 */
                int k = (idx - 0x86) / 2, j;
                for (j = 0; j < 3; j++) {
                    s_light[k].col[j] = mem(a)[j];
                    s_light[k].dir[j] = (int8_t)mem(a + 8)[j];
                }
            } else if (idx >= 0x98 && idx <= 0x9E) {  /* a forced matrix, a quarter at a time */
                static const int quarter[4] = { 1, 2, 3, 0 };     /* 0x98, 0x9A, 0x9C, 0x9E */
                int q = quarter[(idx - 0x98) / 2], j;
                for (j = 0; j < 8; j++) {
                    int e = q * 8 + j;               /* element e of the 32 halves */
                    uint32_t h = rd16(a + j * 2);
                    if (e < 16)
                        s_mvp_fx[e] = (int32_t)((uint32_t)s_mvp_fx[e] & 0xFFFF) | (int32_t)(h << 16);
                    else
                        s_mvp_fx[e - 16] = (int32_t)(((uint32_t)s_mvp_fx[e - 16] & 0xFFFF0000u) | h);
                }
                for (j = 0; j < 16; j++)
                    s_mvp.m[j / 4][j % 4] = (float)(s_mvp_fx[j] / 65536.0);
                s_mvp_forced = 1;
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
        case 0xB3: s_half2 = w1; break;               /* G_RDPHALF_CONT */
        case 0xB4: s_half1 = w1; break;               /* G_RDPHALF_2 */
        case 0xB6: s_geom &= ~w1; break;              /* G_CLEARGEOMETRYMODE */
        case 0xB7: s_geom |= w1; break;               /* G_SETGEOMETRYMODE */
        case 0xB8:                                    /* G_ENDDL */
            if (sp == 0)
                return;
            pc = stack[--sp];
            break;
        case 0xB9: case 0xBA: {                       /* G_SETOTHERMODE_L, _H */
            int sh = (w0 >> 8) & 0xFF, len = w0 & 0xFF;
            uint32_t mask = (len >= 32 ? 0xFFFFFFFFu : ((1u << len) - 1)) << sh;
            uint32_t *m = op == 0xB9 ? &s_oml : &s_omh;
            /* F3DEX 1.x clears the field and ORs in the whole word: bits of
               the data outside the field land too (the dust's render mode,
               0x0F0A0233 at shift 3, sets dithered alpha compare that way) */
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
            if (idx == 0x02)                          /* G_MW_NUMLIGHT */
                s_nlights = (int)((w1 - 0x80000000u) / 32) - 1;
            else if (idx == 0x06)                     /* G_MW_SEGMENT */
                s_seg[(ofs / 4) & 15] = w1 & 0x7FFFFF;
            else if (idx == 0x08) {                   /* G_MW_FOG */
                s_fog_mul = (int16_t)(w1 >> 16);
                s_fog_ofs = (int16_t)w1;
            } else if (idx == 0x0A) {                 /* G_MW_LIGHTCOL */
                int k = ofs / 0x20;
                if ((ofs & 0x1F) == 0 && k < 8) {
                    s_light[k].col[0] = (uint8_t)(w1 >> 24);
                    s_light[k].col[1] = (uint8_t)(w1 >> 16);
                    s_light[k].col[2] = (uint8_t)(w1 >> 8);
                }
            }
            if (s_nlights < 0)
                s_nlights = 0;
            if (s_nlights > 7)
                s_nlights = 7;
            break;
        }
        case 0xBD:                                    /* G_POPMTX */
            if (s_mvn > 0)
                s_mvn--;
            if (s_log)
                fprintf(stderr, "popmtx depth %d\n", s_mvn);
            s_mvp_forced = 0;
            mvp_update();
            break;
        case 0xE4:                                    /* G_TEXRECT: the halves follow */
        case 0xE5: {
            uint32_t h2 = rd32(pc + 4), hc = rd32(pc + 12);
            pc += 16;
            rect_tex(w0, w1, h2, hc);
            break;
        }
        case 0xE6: case 0xE7: case 0xE8: case 0xE9: break;   /* syncs */
        case 0xED:                                    /* G_SETSCISSOR */
            s_scissor[0] = (int)((w0 >> 12) & 0xFFF) / 4;
            s_scissor[1] = (int)(w0 & 0xFFF) / 4;
            s_scissor[2] = (int)((w1 >> 12) & 0xFFF) / 4;
            s_scissor[3] = (int)(w1 & 0xFFF) / 4;
            break;
        case 0xEF: s_omh = w0 & 0xFFFFFF; s_oml = w1; break; /* G_RDPSETOTHERMODE */
        case 0xF0:                                    /* G_LOADTLUT */
            load_tlut((w1 >> 24) & 7, (w1 >> 12) & 0xFFF);
            break;
        case 0xF2: {                                  /* G_SETTILESIZE */
            int t = (w1 >> 24) & 7;
            s_tile[t].uls = (w0 >> 12) & 0xFFF;
            s_tile[t].ult = w0 & 0xFFF;
            s_tile[t].lrs = (w1 >> 12) & 0xFFF;
            s_tile[t].lrt = w1 & 0xFFF;
            break;
        }
        case 0xF3:                                    /* G_LOADBLOCK */
            load_block((w1 >> 24) & 7, (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF);
            break;
        case 0xF4:                                    /* G_LOADTILE */
            load_tile((w1 >> 24) & 7, (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF);
            break;
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
        case 0xF6: rect_fill(w0, w1); break;          /* G_FILLRECT */
        case 0xF7: s_fill = w1; break;                /* G_SETFILLCOLOR */
        case 0xF8: rgba32f(w1, s_fogc); break;
        case 0xF9: rgba32f(w1, s_blendc); break;
        case 0xFA:
            rgba32f(w1, s_prim);
            s_prim_lod = (w0 & 0xFF) / 255.0f;
            break;
        case 0xFB: rgba32f(w1, s_env); break;
        case 0xFC: s_cc0 = w0 & 0xFFFFFF; s_cc1 = w1; break;
        case 0xFD:                                    /* G_SETTIMG */
            s_timg = seg_addr(w1);
            s_timg_fmt = (w0 >> 21) & 7;
            s_timg_siz = (w0 >> 19) & 3;
            s_timg_w = (int)(w0 & 0xFFF) + 1;
            break;
        case 0xFE: s_zimg = seg_addr(w1); break;      /* G_SETZIMG */
        case 0xFF:                                    /* G_SETCIMG */
            s_cimg = seg_addr(w1);
            s_cimg_w = (int)(w0 & 0xFFF) + 1;
            if (s_cimg != s_zimg) {
                int w = s_cimg_w, h = w * 3 / 4;
                if (s_frame_open && (w != s_fb_w || h != s_fb_h)) {
                    frame_end();
                    s_frame_open = 0;
                }
                s_fb_w = w;
                s_fb_h = h;
            }
            break;
        default:
            break;
        }
    }
}

/* one graphics task: its display list, then the frame it drew */
void tgr_rcp_task(uint32_t dl)
{
    static int inited;
    if (!inited) {
        s_log = getenv("TGR_RCPLOG") != NULL;
        s_texdump = getenv("TGR_TEXDUMP");
        if (getenv("TGR_WORLDDUMP"))
            s_wdump = fopen(getenv("TGR_WORLDDUMP"), "wb");
        if (getenv("TGR_ONLYTRIS"))
            s_only = atoi(getenv("TGR_ONLYTRIS"));
        if (getenv("TGR_SKIPTRIS"))
            s_skip = atoi(getenv("TGR_SKIPTRIS"));
        inited = rdr_init();
        s_mvn = 0;
        memset(&s_mv[0], 0, sizeof s_mv[0]);
        s_fb_w = 320;
        s_fb_h = 240;
        s_scissor[2] = 320;
        s_scissor[3] = 240;
        if (!inited)
            return;
    }
    s_mvn = 0;                                        /* each task starts the matrix stack at the
                                                         task's dram_stack base */
    s_kind = K_2D;
    run(dl);
    if (getenv("TGR_RAMDUMP")) {                      /* FILE@FRAME: game memory and the segments, once */
        static int done;
        const char *e = getenv("TGR_RAMDUMP"), *at = strchr(e, '@');
        if (!done && at && tgr_frame() >= (uint32_t)atoi(at + 1)) {
            char path[512];
            FILE *f;
            snprintf(path, sizeof path, "%.*s", (int)(at - e), e);
            if ((f = fopen(path, "wb"))) {
                fwrite(s_seg, 4, 16, f);
                fwrite(tgr_rdram, 1, 0x800000, f);
                fclose(f);
            }
            done = 1;
        }
    }
    if (s_frame_open) {
        frame_end();
        s_frame_open = 0;
    }
}
