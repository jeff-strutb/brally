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
static Pad s_pad[2];

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
    pad_from_keys(&s_pad[0], keys);
    s_pad[1].flags = 0;
    s_pad[1].steer = FX(0.0);
    g_world.walkBack ^= 1;               /* BrRaceTick: the collision cells walked the other way */
    BrCarPhysTick(&s_car[0]);
    BrCarPhysTick(&s_car[1]);
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

void race_view(void *frame)
{
    Frame *f = (Frame *)frame;
    const Car *c = &s_car[g_world.viewCar];
    const fx (*m)[4] = (const fx (*)[4])c->cams[c->cam].mtx;
    fx look[3], right[3], up[3], rel[3], cot, half, kx, ky, rows[3][3], org[3];
    int32_t ri[3][4];
    int i, k;

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
    half = FMUL(c->cams[c->cam].fov, FX(0.5));
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

/* ---- the cells in view: each cell with a triangle, its sphere inside the four edges and
   nearer than the far plane, all its triangles (render_visible's list) ---- */
static uint16_t s_list[12288] __attribute__((section(".ewram_bss")));

const uint16_t *race_cells(const void *frame)
{
    const Frame *f = (const Frame *)frame;
    int n = 0, ci, k, cx, cy, x0, x1, y0, y1;
    int32_t far2;
    /* the cells within the far plane's reach of the camera (32 units = 256 vertex units) */
    cx = f->cam[0] >> 8;
    cy = f->cam[1] >> 8;
    x0 = cx - FAR / 32 - 1;
    x1 = cx + FAR / 32 + 1;
    y0 = cy - FAR / 32 - 1;
    y1 = cy + FAR / 32 + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= g_grid_w) x1 = g_grid_w - 1;
    if (y1 >= g_grid_h) y1 = g_grid_h - 1;
    for (cy = y0; cy <= y1; cy++)
    for (cx = x0; cx <= x1; cx++) {
        const Cell *c = &g_cells[ci = cy * g_grid_w + cx];
        int32_t dx, dy, r;
        if (c->nt == 0)
            continue;
        dx = c->c[0] - f->cam[0];
        dy = c->c[1] - f->cam[1];
        r = c->r + FAR * 8;
        far2 = r * r;
        if (dx * dx + dy * dy > far2)
            continue;
        for (k = 0; k < 4; k++) {
            const int32_t *e = f->edge[k];
            if (e[0] * c->c[0] + e[1] * c->c[1] + e[2] * c->c[2] < (-e[3] - c->r - 2) * 4096)
                break;
        }
        if (k < 4)
            continue;
        if (n + 2 + (int)c->nt >= (int)(sizeof s_list / 2) - 1)
            goto full;
        s_list[n++] = (uint16_t)ci;
        s_list[n++] = (uint16_t)c->nt;
        for (k = 0; k < (int)c->nt; k++)
            s_list[n++] = (uint16_t)k;
    }
full:
    s_list[n] = 0xFFFF;
    return s_list;
}
