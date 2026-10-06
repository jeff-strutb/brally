/* host_marks.m -- Remastered: what the cars' tyres leave on the ground.
 *
 * Dirt takes ruts, pressed in where the wheels roll and deepening lap after
 * lap, with the loose soil pushed up into low lips beside them; asphalt takes
 * rubber where a car slides.  Drawing only: the game's physics never read any
 * of it, and Original (~) draws none of it.
 *
 *   the mark map   the ground under the cars, 6 cm a texel, in 64-texel tiles
 *                  (3.84 m) allocated where marks fall: a page grid over the
 *                  track says which atlas tile holds each square (and the
 *                  height of the ground there, so a road crossing over
 *                  another does not take its marks); R the surface's height
 *                  (0.5 untouched, below pressed in, above heaped up), G how
 *                  dark the mark is (rubber, compacted or crushed)
 *   the wheels     every driver's car (g_aBrRaceDriver, 0x10AF07F8, stride
 *                  0x80, car at +0x60): wheel i's centre at car+0x70+0x40*i,
 *                  its record at +0x994/0x57C/0x370/0x788 holding the surface
 *                  (+0x1A0) and whether it touches it (+0x1B4), as
 *                  BrCarWheelFx (src/brally/core/drawing/br_carwheelfx.c) reads them;
 *                  the car's axes at +0x00 (forward) +0x10 (side) +0x20 (up),
 *                  its velocity at +0x1024
 *   worn ruts      a dirt road has been driven before the race: at the race's
 *                  start its two wheel tracks are pressed in along the racing
 *                  line (header +0x70's ring, src/brally/include/br_ai.h) wherever the
 *                  ground is loose (host_fx.m's road map)
 *
 * The lighting pass (host_fx.m) reads the map: relief from its height, the
 * mark's darkness, water standing in the ruts in the rain.
 */
#import <Metal/Metal.h>
#include "host.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TRK_HDR   0x106EECD8u
#define DRIVERS   0x10AF07F8u
#define NENTRANT  0x100B3858u
#define PAUSED    0x105CCB5Cu

enum { TT = 64, ATW = 64, ATH = 128, NTILE = ATW * ATH };   /* atlas 4096 x 8192 texels */
static const float CELL = 0.06f;                            /* metres a texel */
#define TILEM (CELL * TT)

int hfx_on(void);
int hfx_road_sealed(float x, float y);   /* host_fx.m's road map: 1 sealed, 0 loose, -1 off it */

typedef struct { int gx, gy; u32 used; float z; } tile;
static tile g_tile[NTILE];
static int g_ntile;
static u8 *g_at;                         /* the atlas, RG8, on the CPU */
static unsigned short *g_page;           /* tile index + 1 per page cell, 0 none */
static float g_x0, g_y0;
static int g_pw, g_ph;
static u32 g_key, g_serial;
static unsigned char *g_dirty;           /* per tile: changed since uploaded */
static int g_page_dirty;
static id<MTLTexture> t_at, t_pg;
static float g_last[16][4][3];
static int g_have[16][4];
static int g_seeded;
static unsigned g_nrub, g_nrut;            /* passes laid (BR_MARKS_LOG) */

static void reset(void)
{
    int i;
    free(g_page); g_page = NULL;
    g_ntile = 0; g_seeded = 0; g_pw = g_ph = 0;
    memset(g_have, 0, sizeof g_have);
    if (g_at) for (i = 0; i < ATW * TT * ATH * TT; i++) { g_at[2 * i] = 128; g_at[2 * i + 1] = 0; }
    if (g_dirty) memset(g_dirty, 1, NTILE);
    g_page_dirty = 1;
}

/* the track's extent, from the collision mesh's vertices (as host_fx.m's road map) */
static int bind_track(void)
{
    u32 faces = H32(TRK_HDR + 0x0C), verts = H32(TRK_HDR + 0x14), flags = H32(TRK_HDR + 0x94);
    u32 key = faces ^ (verts * 31u) ^ (flags * 131u);
    int nv, i;
    float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
    if (!faces || !verts || !flags) return 0;
    if (key == g_key && g_page) return 1;
    nv = (int)H32(TRK_HDR + 0x10);
    if (nv <= 0 || nv > 200000) return 0;
    if (!g_at) {
        g_at = malloc((size_t)ATW * TT * ATH * TT * 2);
        g_dirty = malloc(NTILE);
    }
    reset();
    for (i = 0; i < nv; i++) {
        float x = W_LD(f32, verts, i * 12), y = W_LD(f32, verts, i * 12 + 4);
        if (x < lo[0]) lo[0] = x; if (x > hi[0]) hi[0] = x;
        if (y < lo[1]) lo[1] = y; if (y > hi[1]) hi[1] = y;
    }
    g_x0 = lo[0] - 8; g_y0 = lo[1] - 8;
    g_pw = (int)((hi[0] + 8 - g_x0) / TILEM) + 1;
    g_ph = (int)((hi[1] + 8 - g_y0) / TILEM) + 1;
    if (g_pw > 4096 || g_ph > 4096) { g_pw = g_ph = 0; return 0; }
    g_page = calloc((size_t)g_pw * g_ph, sizeof *g_page);
    g_key = key;
    return 1;
}

/* the tile under a ground point, made if need be (the least recently
 * marked one is reused once the atlas is full) */
static int tile_at(int gx, int gy, float z)
{
    unsigned short *p;
    int t, i;
    if (gx < 0 || gy < 0 || gx >= g_pw || gy >= g_ph) return -1;
    p = &g_page[gy * g_pw + gx];
    if (*p) {
        t = *p - 1;
        if (fabsf(g_tile[t].z - z) > 4.0f) return -1;      /* another road over or under this one */
        g_tile[t].used = g_serial;
        return t;
    }
    if (g_ntile < NTILE) t = g_ntile++;
    else {
        t = 0;
        for (i = 1; i < NTILE; i++) if (g_tile[i].used < g_tile[t].used) t = i;
        g_page[g_tile[t].gy * g_pw + g_tile[t].gx] = 0;
        {
            int ax = (t % ATW) * TT, ay = (t / ATW) * TT, y;
            for (y = 0; y < TT; y++) {
                u8 *r = g_at + ((size_t)(ay + y) * ATW * TT + ax) * 2;
                for (i = 0; i < TT; i++) { r[2 * i] = 128; r[2 * i + 1] = 0; }
            }
        }
    }
    g_tile[t].gx = gx; g_tile[t].gy = gy; g_tile[t].z = z; g_tile[t].used = g_serial;
    *p = (unsigned short)(t + 1);
    g_dirty[t] = 1;
    g_page_dirty = 1;
    return t;
}

/* One texel's channels: height eased toward `h` (deeper only, or higher
 * only, as asked) by `k`, darkness raised toward `g` */
static void texel(float x, float y, float z, float h, float k, float g)
{
    float vx = (x - g_x0) / CELL, vy = (y - g_y0) / CELL;
    int ix = (int)floorf(vx), iy = (int)floorf(vy), t;
    u8 *c;
    if (ix < 0 || iy < 0) return;
    t = tile_at(ix / TT, iy / TT, z);
    if (t < 0) return;
    c = g_at + ((size_t)((t / ATW) * TT + iy % TT) * ATW * TT + (t % ATW) * TT + ix % TT) * 2;
    {
        float cur = c[0] / 255.0f, nh = cur;
        if (h < 0.5f && h < cur) nh = cur + (h - cur) * k;
        else if (h > 0.5f && h > cur) nh = cur + (h - cur) * k;
        c[0] = (u8)(nh * 255.0f + 0.5f);
        if (g > 0) { float cg = c[1] / 255.0f; cg += (g - cg) * k; c[1] = (u8)(cg * 255.0f + 0.5f); }
    }
    g_dirty[t] = 1;
}

/* A tyre's pass from a to b: across it the tread's trough (depth `dep`, 0..1
 * of the map's range) with lips of pushed-out soil either side, its
 * darkness `g`; `k` how much of the way one pass goes */
static void stamp(const float *a, const float *b, float hw, float dep, float g, float k)
{
    float dx = b[0] - a[0], dy = b[1] - a[1], L = sqrtf(dx * dx + dy * dy);
    float nx, ny, reach = hw * 2.2f, s, t;
    if (L < 1e-4f) return;
    nx = -dy / L; ny = dx / L;
    for (s = 0; s <= L; s += CELL * 0.7f) {
        float px = a[0] + dx * s / L, py = a[1] + dy * s / L, pz = a[2] + (b[2] - a[2]) * s / L;
        for (t = -reach; t <= reach; t += CELL * 0.7f) {
            float u = fabsf(t) / hw;
            /* a trough across the tread, lips just outside it */
            float trough = expf(-u * u * 1.6f), lip = 0.45f * expf(-(u - 1.45f) * (u - 1.45f) * 6.0f);
            float h = 0.5f - 0.5f * dep * trough + 0.5f * dep * lip;
            texel(px + nx * t, py + ny * t, pz, h, k, g * trough);
        }
    }
}

/* the racing line's centre points, every metre (br_ai.h's ring: next +0x00,
 * sibling +0x04, point count +0x14, points from +0x40 every 0x28, centre at
 * +0x0C), and the worn wheel tracks pressed into the loose ground along it */
static void seed(void)
{
    u32 stack[256], seen[1024];
    int ns = 0, nseen = 0, nruts = 0;
    stack[ns++] = H32(TRK_HDR + 0x70);
    stack[ns++] = H32(TRK_HDR + 0x74);
    while (ns) {
        u32 n = stack[--ns];
        int i, k, cnt, dup = 0;
        if (!n) continue;
        for (i = 0; i < nseen; i++) if (seen[i] == n) dup = 1;
        if (dup || nseen == 1024) continue;
        seen[nseen++] = n;
        cnt = W_LD(u16, n, 0x14);
        if (cnt > 4096) continue;
        for (k = 0; k < cnt; k++) {
            u32 p0 = n + 0x40 + 0x28 * (u32)k, p1 = p0 + 0x28;
            float c0[3] = { W_LD(f32, p0, 0x0C), W_LD(f32, p0, 0x10), W_LD(f32, p0, 0x14) };
            float c1[3] = { W_LD(f32, p1, 0x0C), W_LD(f32, p1, 0x10), W_LD(f32, p1, 0x14) };
            float dx = c1[0] - c0[0], dy = c1[1] - c0[1], L = sqrtf(dx * dx + dy * dy);
            int j, m;
            if (L < 0.5f || L > 200.0f) continue;
            m = (int)(L / 0.5f) + 1;
            for (j = 0; j < m; j++) {
                float t0 = (float)j / m, t1 = (float)(j + 1) / m;
                float a[3], b[3], nx = -dy / L, ny = dx / L;
                int w;
                a[0] = c0[0] + dx * t0; a[1] = c0[1] + dy * t0; a[2] = c0[2] + (c1[2] - c0[2]) * t0;
                b[0] = c0[0] + dx * t1; b[1] = c0[1] + dy * t1; b[2] = c0[2] + (c1[2] - c0[2]) * t1;
                if (hfx_road_sealed(a[0], a[1]) != 0) continue;
                /* two wheel tracks either side of the line, wandering a
                 * little; fainter ones where cars ran wide of it */
                for (w = 0; w < 4; w++) {
                    float sx = (a[0] + b[0]) * 0.5f, sy = (a[1] + b[1]) * 0.5f;
                    float wander = 0.25f * sinf(sx * 0.031f + sy * 0.017f) + 0.15f * sinf(sx * 0.11f - sy * 0.07f);
                    float off = (w == 0 ? -0.78f : w == 1 ? 0.78f : w == 2 ? -2.3f : 2.4f) + wander;
                    float dep = w < 2 ? 0.75f : 0.35f;
                    float aa[3] = { a[0] + nx * off, a[1] + ny * off, a[2] }, bb[3] = { b[0] + nx * off, b[1] + ny * off, b[2] };
                    stamp(aa, bb, w < 2 ? 0.13f : 0.11f, dep, w < 2 ? 0.2f : 0.08f, 1.0f);
                }
                nruts++;
            }
        }
        if (ns < 254) { stack[ns++] = W_LD(u32, n, 0); stack[ns++] = W_LD(u32, n, 4); }
    }
    if (getenv("BR_MARKS_LOG"))
        fprintf(stderr, "marks: %d path nodes, %d m of worn ruts, %d tiles\n", nseen, nruts / 2, g_ntile);
}

/* Each frame: the wheels' passes since the last one */
void hmarks_frame(void)
{
    int n, i, w;
    if (!hfx_on() || getenv("BR_MARKS_OFF") || !bind_track()) return;
    g_serial++;
    if (!g_seeded) {
        if (hfx_road_sealed(0, 0) == -2) return;        /* the road map is not built yet */
        seed();
        g_seeded = 1;
    }
    if (H32(PAUSED)) return;
    if (getenv("BR_MARKS_LOG") && g_serial % 600 == 0)
        fprintf(stderr, "marks: %u rut passes, %u rubber passes, %d tiles\n", g_nrut, g_nrub, g_ntile);
    n = (int)H32(NENTRANT);
    if (n < 0 || n > 16) return;
    for (i = 0; i < n; i++) {
        static const u32 REC[4] = { 0x994, 0x57C, 0x370, 0x788 };
        u32 car = W_LD(u32, DRIVERS + 0x80u * (u32)i, 0x60);
        float side[3], up[3], vel[3], lat;
        if (!car) { memset(g_have[i], 0, sizeof g_have[i]); continue; }
        for (w = 0; w < 3; w++) {
            side[w] = W_LD(f32, car, 0x10 + 4 * w); up[w] = W_LD(f32, car, 0x20 + 4 * w);
            vel[w] = W_LD(f32, car, 0x1024 + 4 * w);
        }
        lat = fabsf(side[0] * vel[0] + side[1] * vel[1] + side[2] * vel[2]);
        for (w = 0; w < 4; w++) {
            u32 r = car + REC[w];
            float p[3];
            int live = W_LD(s32, r, 0x1B4) != 0, sealed;
            p[0] = W_LD(f32, car, 0x70 + 0x40 * w) - up[0] * 0.3f;
            p[1] = W_LD(f32, car, 0x74 + 0x40 * w) - up[1] * 0.3f;
            p[2] = W_LD(f32, car, 0x78 + 0x40 * w) - up[2] * 0.3f;
            if (!live) { g_have[i][w] = 0; continue; }
            if (g_have[i][w]) {
                float *q = g_last[i][w];
                float dx = p[0] - q[0], dy = p[1] - q[1], d2 = dx * dx + dy * dy;
                if (d2 > 36.0f) { memcpy(q, p, sizeof p); continue; }   /* put back on the road */
                if (d2 < 0.04f) continue;                               /* not far enough to mark */
                sealed = hfx_road_sealed(p[0], p[1]);
                if (sealed == 1) {
                    /* rubber only where the tyre slides */
                    float slide = fminf(1.0f, fmaxf(0.0f, (lat - 3.0f) / 6.0f));
                    if (W_LD(u8, car, 0x360)) slide = fmaxf(slide, 0.5f);
                    if (slide > 0) { stamp(q, p, 0.1f, 0.0f, 0.75f * slide, 0.35f); g_nrub++; }
                } else {
                    /* loose ground: a rut every pass, deeper with every lap */
                    stamp(q, p, 0.12f, 0.9f, 0.3f, 0.18f); g_nrut++;
                }
            }
            memcpy(g_last[i][w], p, sizeof p);
            g_have[i][w] = 1;
        }
    }
}

/* host_fx.m: the map's textures, up to date, and its placing: x0, y0,
 * 1 / texel size, tiles per atlas row; 0 when there is none */
int hmarks_bind(id<MTLDevice> dev, id<MTLTexture> *atlas, id<MTLTexture> *page, float *u)
{
    int t;
    if (!g_page || !g_pw) return 0;
    if (!t_at) {
        MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
                                                                                     width:ATW * TT height:ATH * TT mipmapped:NO];
        d.usage = MTLTextureUsageShaderRead;
        t_at = [dev newTextureWithDescriptor:d];
        memset(g_dirty, 1, NTILE);
    }
    for (t = 0; t < NTILE; t++) {
        if (!g_dirty[t]) continue;
        g_dirty[t] = 0;
        [t_at replaceRegion:MTLRegionMake2D((NSUInteger)(t % ATW) * TT, (NSUInteger)(t / ATW) * TT, TT, TT) mipmapLevel:0
                  withBytes:g_at + ((size_t)(t / ATW) * TT * ATW * TT + (size_t)(t % ATW) * TT) * 2 bytesPerRow:ATW * TT * 2];
    }
    if (g_page_dirty || !t_pg || t_pg.width != (NSUInteger)g_pw || t_pg.height != (NSUInteger)g_ph) {
        /* per page cell: atlas tile x, y, the ground's height there, 1 if a tile */
        float *px = calloc((size_t)g_pw * g_ph * 4, sizeof *px);
        int i;
        for (i = 0; i < g_pw * g_ph; i++) if (g_page[i]) {
            int k = g_page[i] - 1;
            px[4 * i] = (float)(k % ATW); px[4 * i + 1] = (float)(k / ATW); px[4 * i + 2] = g_tile[k].z; px[4 * i + 3] = 1;
        }
        if (!t_pg || t_pg.width != (NSUInteger)g_pw || t_pg.height != (NSUInteger)g_ph) {
            MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                                                         width:(NSUInteger)g_pw height:(NSUInteger)g_ph mipmapped:NO];
            d.usage = MTLTextureUsageShaderRead;
            t_pg = [dev newTextureWithDescriptor:d];
        }
        [t_pg replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)g_pw, (NSUInteger)g_ph) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)g_pw * 16];
        free(px);
        g_page_dirty = 0;
    }
    *atlas = t_at; *page = t_pg;
    u[0] = g_x0; u[1] = g_y0; u[2] = 1.0f / CELL; u[3] = (float)TT;
    return 1;
}
