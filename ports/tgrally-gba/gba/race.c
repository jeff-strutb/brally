/* race.c -- the race the GBA plays: the cars on the grid as the game set them, each
 * game frame their physics (the game's own, sim/, in 32.32 fixed point) from the pad,
 * and the race camera (BrCamChaseStep's cams[0]) as the renderer's Frame: the game's
 * guLookAtF and guPerspectiveF (frameloop.c BrCameraSet: its lens, near 2.4, far 400)
 * to the GBA's 160x128 the way the game's viewport takes it to its 640x480 frame
 * buffer; the cells in view the way BrTrackDrawSetup takes them (the view's span of the
 * 32-unit grid, out to the far plane). */
#include <stdint.h>
#include "world.h"
#include "race.h"
#include "../sim/simload.h"

#define SW 160
#define SH 128
#define FAR 400                          /* D_8028AAC8: the race view's far plane */

static Car s_car[2];
volatile uint32_t g_phase_cyc[6];        /* the cycles of each phase, of the overlays' copies, ticks */
static inline uint32_t clock32(void) { return *(volatile uint16_t *)0x04000108 | (uint32_t)*(volatile uint16_t *)0x0400010C << 16; }

#ifdef SIM_PROF
uint32_t g_simprof[24];
uint32_t sim_clock(void) { return *(volatile uint16_t *)0x04000108 | (uint32_t)*(volatile uint16_t *)0x0400010C << 16; }
#endif
static Pad s_pad[2];
static fx s_cprev[17], s_ccur[17];
static int s_settle;                     /* ticks every car runs whatever (the start) */
#define ABS(x) ((x) < 0 ? -(x) : (x))       /* the view's camera (rows, fov) a tick ago and now */

void race_start(void)
{
    int k, i;
    g_track = &g_rt_track;
    for (i = 0; i < 72; i++)
        g_grip[i] = g_rt_grip[i];
    g_world.walkBack = 0;
    g_world.dt = FX(1.0 / 30);
    g_world.weather = 1;
    g_world.mode = 1;                    /* arcade */
    g_world.players = 1;
    g_world.ncars = 2;
    g_world.lens = g_rt_lens;
    g_world.replay = 0;
    g_world.views = 1;
    g_world.viewCar = 0;
    for (k = 0; k < 2; k++) {
        sl_car_load(&s_car[k], &s_pad[k], g_rt_cars[k], g_rt_pads[k], 0);
        for (i = 0; i < 3; i++)
            s_car[k].camView[i] = g_rt_camView[k][i];
        g_world.cars[k] = &s_car[k];
    }
    sim_camera = BrCamChaseStep;
    s_settle = 20;
    g_sim_dt = FX(1.0 / 30);             /* settled on the grid in the game's ticks, half a second */
    g_sim_dtk = FX(1.0);
    for (k = 0; k < 15; k++)
        race_tick(0x3FF);
    g_sim_dt = FX(RACE_TICKS / 30.0);    /* then a tick every RACE_TICKS of the game's */
    g_sim_dtk = FX(RACE_TICKS);
    race_tick(0x3FF);
}

/* the pad as BrPadMapRead makes it in a race (control layout 0: A accelerates, B brakes,
   R changes up, L down; the d-pad is the stick, full lock either way) */
static void pad_from_keys(Pad *p, uint32_t keys)
{
    uint32_t down = ~keys & 0x3FF, f = 0;
    if (down & 1)
        f |= 0x10 | 0x10000;
    if (down & 2)
        f |= 0x20 | ((f & 0x10000) ? 0x80000 : 0x40000);
    if (down & 0x100)
        f |= 0x2000 | 0x100000;
    if (down & 0x200)
        f |= 0x200000;
    p->flags = f;
    p->steer = (down & 0x10) ? FX(1.0) : (down & 0x20) ? FX(-1.0) : FX(0.0);
}

void race_tick(uint32_t keys)
{
    int k, awake;
    pad_from_keys(&s_pad[0], keys);
    s_pad[1].flags = 0;
    s_pad[1].steer = FX(0.0);
    g_world.walkBack ^= 1;               /* BrRaceTick: the collision cells walked the other way */
    {   /* a car at rest with nothing to do, the player's well away, sleeps (its tick skipped) */
        const Body *o = &s_car[1].body;
        fx dx = o->st.pos[0] - s_car[0].body.st.pos[0], dy = o->st.pos[1] - s_car[0].body.st.pos[1];
        awake = s_settle > 0 || s_pad[1].flags != 0 || s_pad[1].steer != 0 || (ABS(dx) < FX(16.0) && ABS(dy) < FX(16.0)) ||
                ABS(o->st.vel[0]) + ABS(o->st.vel[1]) + ABS(o->st.vel[2]) > FX(0.05) ||
                ABS(o->st.omega[0]) + ABS(o->st.omega[1]) + ABS(o->st.omega[2]) > FX(0.05);
        if (s_settle > 0)
            s_settle--;
    }
    for (k = 0; k < 4; k++) {            /* BrCarPhysTick a phase at a time, each phase's code in */
        uint32_t t0 = clock32(), t1;     /* IWRAM for both cars */
        race_phase_code(k);
        t1 = clock32();
        sim_tick_phase(&s_car[0], k);
        if (awake)
            sim_tick_phase(&s_car[1], k);
        g_phase_cyc[4] += t1 - t0;
        g_phase_cyc[k] += clock32() - t1;
    }
    g_phase_cyc[5]++;
    if (g_sim_dtk == FX(2.0) && sim_camera)   /* the chase camera at the game's rate: once more */
        sim_camera(&s_car[g_world.viewCar]);
    for (k = 0; k < 16; k++) {                /* the camera now and a tick before (race_view between) */
        s_cprev[k] = s_ccur[k];
        s_ccur[k] = (&s_car[g_world.viewCar].cams[s_car[g_world.viewCar].cam].mtx[0][0])[k];
    }
    s_cprev[16] = s_ccur[16];
    s_ccur[16] = s_car[g_world.viewCar].cams[s_car[g_world.viewCar].cam].fov;
}

/* ---- the camera to the renderer's Frame (convert.py's rows, made here) ---- */
static fx dot3(const fx *a, const fx *b) { return FMUL(a[0], b[0]) + FMUL(a[1], b[1]) + FMUL(a[2], b[2]); }

static void norm3(fx *v)
{
    fx l = FSQRT(dot3(v, v));
    if (l) {
        v[0] = FDIV(v[0], l);
        v[1] = FDIV(v[1], l);
        v[2] = FDIV(v[2], l);
    }
}

void race_view(void *frame, int alpha)
{
    Frame *f = (Frame *)frame;
    fx cm[17];
    const fx (*m)[4] = (const fx (*)[4])cm;
    fx look[3], right[3], up[3], rel[3], cot, half, kx, ky, rows[3][3], org[3];
    int32_t ri[3][4];
    int i, k;

    for (i = 0; i < 17; i++)             /* between the last two ticks: alpha 0..256 */
        cm[i] = s_cprev[i] + (((s_ccur[i] - s_cprev[i]) >> 8) * alpha);

    /* guLookAtF: the eye at m[3], looking along m[0], m[2] up */
    for (i = 0; i < 3; i++)
        look[i] = -m[0][i];
    norm3(look);
    right[0] = FMUL(m[2][1], look[2]) - FMUL(m[2][2], look[1]);
    right[1] = FMUL(m[2][2], look[0]) - FMUL(m[2][0], look[2]);
    right[2] = FMUL(m[2][0], look[1]) - FMUL(m[2][1], look[0]);
    norm3(right);
    up[0] = FMUL(look[1], right[2]) - FMUL(look[2], right[1]);
    up[1] = FMUL(look[2], right[0]) - FMUL(look[0], right[2]);
    up[2] = FMUL(look[0], right[1]) - FMUL(look[1], right[0]);
    norm3(up);
    /* guPerspectiveF: fovy = the lens x 4/3 x (h / w) = the lens (radians), aspect 4/3; the
       viewport: x = 80 ndc + 80, y = 64 - 64 ndc on the GBA's screen */
    half = FMUL(cm[16], FX(0.5));
    cot = FDIV(FCOS(half), FSIN(half));
    kx = FMUL(FDIV(cot, FX(4.0 / 3.0)), FX(SW / 2));
    ky = FMUL(cot, FX(SH / 2));
    for (i = 0; i < 3; i++) {
        rows[0][i] = FMUL(right[i], kx);
        rows[1][i] = -FMUL(up[i], ky);
        rows[2][i] = -look[i];
    }
    org[0] = ITOF(g_org[0]);
    org[1] = ITOF(g_org[1]);
    org[2] = ITOF(g_org[2]);
    for (i = 0; i < 3; i++)
        rel[i] = org[i] - m[3][i];
    for (k = 0; k < 3; k++) {             /* coefficients Q12 for a vertex in 1/8 units, results 1/256 */
        for (i = 0; i < 3; i++)
            ri[k][i] = (int32_t)(rows[k][i] >> 15);
        ri[k][3] = (int32_t)(dot3(rows[k], rel) >> 24);
        for (i = 0; i < 4; i++)
            f->m[k * 4 + i] = ri[k][i];
    }
    f->pos[0] = (int16_t)FTOI(m[3][0] - org[0]);
    f->pos[1] = (int16_t)FTOI(m[3][1] - org[1]);
    f->fwd[0] = (int16_t)(look[0] >> 24);
    f->fwd[1] = (int16_t)(look[1] >> 24);
    for (i = 0; i < 3; i++)
        f->cam[i] = (int32_t)((m[3][i] - org[i]) >> 29);
    /* the screen's four edges as planes (vertex units, normals Q12), as convert.py makes them */
    {
        static const int8_t ab[4][2] = { { 0, SW / 2 }, { 0, -SW / 2 }, { 1, SH / 2 }, { 1, -SH / 2 } };
        int e;
        for (e = 0; e < 4; e++) {
            int a = ab[e][0], b = ab[e][1], sg = b > 0 ? 1 : -1, ab_ = b > 0 ? b : -b;
            fx g[3], gl;
            int64_t h = (int64_t)sg * ri[a][3] + (int64_t)ab_ * ri[2][3];
            for (i = 0; i < 3; i++)
                g[i] = ((int64_t)sg * ri[a][i] + (int64_t)ab_ * ri[2][i]) << 20;
            gl = FSQRT(dot3(g, g));
            if (gl == 0)
                gl = 1;
            for (i = 0; i < 3; i++)
                f->edge[e][i] = (int32_t)(FDIV(g[i], gl) >> 20);
            f->edge[e][3] = (int32_t)(FDIV(h << 32, gl) >> 32);
        }
    }
}

/* ---- the cells in view: those in the view's wedge on the ground (26 degrees either side, out to
   the far plane, 64 units wider), each with a triangle, its sphere inside the four edges
   and nearer than the far plane, all its triangles (render_visible's list); the few cells whose
   sphere reaches further than that past their square are tried every frame ---- */
static uint16_t s_list[12288] __attribute__((section(".ewram_bss")));
#define M8 512                           /* the margin: 64 units, in eighths (a cell's sphere reaches out
                                            that far, which the edges' test lets in) */
static uint16_t s_big[128];
static int s_nbig = -1;
static uint16_t s_rowat[130] __attribute__((section(".ewram_bss")));   /* each row's first in s_rowx */
static uint8_t s_rowx[4422] __attribute__((section(".ewram_bss")));   /* the rows' cells with triangles, by x */
static uint8_t s_isbig[(4096 * 2) / 8] __attribute__((section(".ewram_bss")));

static void find_big(void)
{
    int ci, gx, gy, nx = 0;
    s_nbig = 0;
    for (gy = 0; gy < g_grid_h; gy++)
    for (gx = 0; gx < g_grid_w; gx++) {
        const Cell *c = &g_cells[ci = gy * g_grid_w + gx];
        int32_t x0 = gx * 256, y0 = gy * 256;
        if (gx == 0)
            s_rowat[gy] = (uint16_t)nx;
        if (c->nt == 0)
            continue;
        s_rowx[nx++] = (uint8_t)gx;
        if (c->c[0] - c->r < x0 - M8 || c->c[0] + c->r > x0 + 256 + M8 ||
            c->c[1] - c->r < y0 - M8 || c->c[1] + c->r > y0 + 256 + M8) {
            if (s_nbig < (int)(sizeof s_big / sizeof s_big[0])) {
                s_big[s_nbig++] = (uint16_t)ci;
                s_isbig[ci >> 3] |= (uint8_t)(1 << (ci & 7));
            }
        }
    }
    s_rowat[g_grid_h] = (uint16_t)nx;
}

static inline __attribute__((always_inline)) int cell_seen(const Frame *f, const Cell *c)
{
    int32_t dx, dy, r;
    int k;
    if (c->nt == 0)
        return 0;
    dx = c->c[0] - f->cam[0];
    dy = c->c[1] - f->cam[1];
    r = c->r + FAR * 8;
    if (dx * dx + dy * dy > r * r)
        return 0;
    for (k = 0; k < 4; k++) {
        const int32_t *e = f->edge[k];
        if (e[0] * c->c[0] + e[1] * c->c[1] + e[2] * c->c[2] < (-e[3] - c->r - 2) * 4096)
            return 0;
    }
    return 1;
}

static inline __attribute__((always_inline)) int cell_take(const Frame *f, int ci, int n)
{
    const Cell *c = &g_cells[ci];
    if (!cell_seen(f, c))
        return n;
    if (n + 2 >= (int)(sizeof s_list / 2) - 1)
        return n;
    s_list[n++] = (uint16_t)ci;
    s_list[n++] = (uint16_t)(c->nt | 0x8000);   /* all its triangles (front.s) */
    return n;
}

const uint16_t *race_cells(const void *frame)
{
    const Frame *f = (const Frame *)frame;
    int32_t px[3], py[3], ylo, yhi, row, i, n = 0;
    fx slope[3];
    if (s_nbig < 0)
        find_big();
    {   /* the wedge: the camera, the far plane's two ends (tan 26 deg = 0.4877) */
        int32_t fx_ = -f->fwd[0], fy = -f->fwd[1], R = FAR * 8, t = 125;   /* Q8 (the view looks down -fwd), unit on the ground */
        fx l = FSQRT(ITOF(fx_ * fx_ + fy * fy));
        if (l > 0) {
            fx_ = FTOI(FDIV(ITOF(fx_ * 256), l));
            fy = FTOI(FDIV(ITOF(fy * 256), l));
        }
        px[0] = f->cam[0] - (fx_ * M8 >> 8);
        py[0] = f->cam[1] - (fy * M8 >> 8);
        px[1] = f->cam[0] + ((fx_ * R - fy * (R * t >> 8)) >> 8);
        py[1] = f->cam[1] + ((fy * R + fx_ * (R * t >> 8)) >> 8);
        px[2] = f->cam[0] + ((fx_ * R + fy * (R * t >> 8)) >> 8);
        py[2] = f->cam[1] + ((fy * R - fx_ * (R * t >> 8)) >> 8);
    }
    ylo = py[0] < py[1] ? py[0] : py[1];
    ylo = ylo < py[2] ? ylo : py[2];
    yhi = py[0] > py[1] ? py[0] : py[1];
    yhi = yhi > py[2] ? yhi : py[2];
    for (i = 0; i < 3; i++) {                /* each edge's x per y, 32.32 */
        int j = i == 2 ? 0 : i + 1;
        slope[i] = py[j] != py[i] ? FDIV(ITOF(px[j] - px[i]), ITOF(py[j] - py[i])) : 0;
    }
    for (row = (ylo - M8) >> 8; row <= (yhi + M8) >> 8; row++) {
        int32_t s0 = row * 256 - M8, s1 = row * 256 + 256 + M8, xlo = 0x7FFFFFFF, xhi = -0x7FFFFFFF, cx;
        if (row < 0 || row >= g_grid_h)
            continue;
        for (i = 0; i < 3; i++) {            /* the wedge within the strip: its corners, its edges' ends */
            int j = i == 2 ? 0 : i + 1, e;
            int32_t ya, yb;
            if (py[i] >= s0 && py[i] <= s1) {
                xlo = px[i] < xlo ? px[i] : xlo;
                xhi = px[i] > xhi ? px[i] : xhi;
            }
            if (py[i] == py[j])
                continue;
            ya = py[i] < py[j] ? py[i] : py[j];
            yb = py[i] < py[j] ? py[j] : py[i];
            ya = ya > s0 ? ya : s0;
            yb = yb < s1 ? yb : s1;
            for (e = 0; e < 2 && ya <= yb; e++) {
                int32_t y = e ? yb : ya, x = px[i] + FTOI(FMUL(slope[i], ITOF(y - py[i])));
                xlo = x < xlo ? x : xlo;
                xhi = x > xhi ? x : xhi;
            }
        }
        if (xlo > xhi)
            continue;
        xlo = (xlo - M8) >> 8;
        xhi = (xhi + M8) >> 8;
        if (xlo < 0)
            xlo = 0;
        if (xhi >= g_grid_w)
            xhi = g_grid_w - 1;
        for (cx = s_rowat[row]; cx < s_rowat[row + 1]; cx++) {   /* the row's cells with triangles */
            int x = s_rowx[cx], ci;
            if (x < xlo)
                continue;
            if (x > xhi)
                break;
            ci = row * g_grid_w + x;
            if (!(s_isbig[ci >> 3] & (1 << (ci & 7))))
                n = cell_take(f, ci, n);
        }
    }
    for (i = 0; i < s_nbig; i++)
        n = cell_take(f, s_big[i], n);
    s_list[n] = 0xFFFF;
    return s_list;
}

