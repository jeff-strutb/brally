/* light_capture.c: the scene as the game drew it, in world space.
 *
 * Every display-list triangle reaches Glide in the game's pixels with its
 * 1/w (br_dltri2.c unclipped, br_dltrim.c clipped; br_dlview.h tells
 * glide.c it is one).  The viewport took clip space to those pixels,
 * x = cx / cw * sx + ox, so cx = (x - ox) / sx * w, cy likewise, cw = w; and
 * clip = [p 1] x VP for the world point p.  The three equations of x, y and w
 * give p exactly (a 3x3 solve), whether or not the game clipped the triangle.
 * Nothing here writes the game's memory.
 *
 *   BR_LIGHT_CAPTURE=1     record even with the flag off (the flag records too)
 *   BR_LIGHT_STATS=1       once a second: triangles a frame by view, the
 *                          largest reprojection error, the world's extent
 *   BR_LIGHT_DUMP=N:PATH   frame N's camera triangles as a Wavefront OBJ
 *                          (vertex colours after the position)
 *   BR_LIGHT_FRAMELOG=PATH a line a frame: frame, camera, mirror and backdrop
 *                          triangles
 *
 * The backdrop: the no-Z vertex routines (br_dlvtx_noz.c, br_dlvtx_nozlit.c)
 * project, then set 1/w to 1/65535 so the painted sky and horizon write the
 * far depth and texture affinely.  Those triangles have no world position; a
 * triangle with that 1/w is the backdrop (view 3 here), kept apart.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "br_flags.h"
#include "light.h"
#include "brr.h"

typedef struct LightTri {
    float p[3][3];               /* world */
    float uv[3][2];              /* texels */
    uint8_t col[3][4];
    uint32_t texture, state;
    uint8_t view;
} LightTri;

#define BACKDROP_OOW (1.0f / 65535.0f)

static LightTri *s_tri;
static int s_ntri, s_cap;
static unsigned s_frame;
static unsigned s_fr[4];                 /* this frame's triangles by view */
static unsigned s_blend[256];            /* camera triangles by blend (src | dst << 4), for the stats */

/* the camera of the triangles now arriving, and the inverse of its x, y, w
 * columns */
static LightCamera s_cam, s_cam_main;  /* the camera now; the main view's, last seen */
static double s_inv[3][3];
static int s_cam_ok;

static struct {
    unsigned frames, tris[4];
    double err;
    float lo[3], hi[3];
} s_st;

int light_capture_on(void)
{
    static int env = -1;
    if (env < 0) {
        const char *e = getenv("BR_LIGHT_CAPTURE");
        env = e && atoi(e) != 0;
    }
    return env || plat_flag(BR_FLAG_LIGHTING);
}

/* the camera: re-read for every triangle (an 80-byte copy), the solve's
 * inverse only when it changed (the mirror and the main view alternate) */
static int camera(void)
{
    LightCamera c;
    double a[3][3], det;
    int i, j;
    light_game_camera(&c);
    if (s_cam_ok && !memcmp(&c, &s_cam, sizeof c))
        return 1;
    s_cam = c;
    /* rows: the equations for cx, cy, cw; columns: px, py, pz */
    for (i = 0; i < 3; i++) {
        int k = i == 2 ? 3 : i;
        for (j = 0; j < 3; j++)
            a[i][j] = c.vp[j * 4 + k];
    }
    det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
        - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
        + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (fabs(det) < 1e-30 || c.sx == 0.0f || c.sy == 0.0f) {
        s_cam_ok = 0;
        return 0;
    }
    s_inv[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) / det;
    s_inv[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) / det;
    s_inv[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) / det;
    s_inv[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) / det;
    s_inv[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / det;
    s_inv[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / det;
    s_inv[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) / det;
    s_inv[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / det;
    s_inv[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / det;
    s_cam_ok = 1;
    return 1;
}

/* one vertex back into the world; the error of projecting it again, pixels */
static double unproject(const LightVtx *v, float p[3])
{
    const float *m = s_cam.vp;
    double w = 1.0 / v->oow, rhs[3], cl[4], e;
    int i;
    rhs[0] = (v->x - s_cam.ox) / s_cam.sx * w - m[12];
    rhs[1] = (v->y - s_cam.oy) / s_cam.sy * w - m[13];
    rhs[2] = w - m[15];
    for (i = 0; i < 3; i++)
        p[i] = (float)(s_inv[i][0] * rhs[0] + s_inv[i][1] * rhs[1] + s_inv[i][2] * rhs[2]);
    for (i = 0; i < 4; i++)
        cl[i] = p[0] * m[i] + p[1] * m[4 + i] + p[2] * m[8 + i] + m[12 + i];
    e = fabs(cl[0] / cl[3] * s_cam.sx + s_cam.ox - v->x);
    return fmax(e, fabs(cl[1] / cl[3] * s_cam.sy + s_cam.oy - v->y));
}

int light_capture_tri(int view, const LightVtx *a, const LightVtx *b, const LightVtx *c,
                      uint32_t texture, uint32_t state, float world[3][3])
{
    const LightVtx *v[3] = { a, b, c };
    LightTri *t;
    int i, k;
    if (!a->oow || !b->oow || !c->oow || !camera())
        return 0;
    int main_view = view == 0;
    if (a->oow == BACKDROP_OOW || b->oow == BACKDROP_OOW || c->oow == BACKDROP_OOW)
        view = 3;
    if (s_ntri == s_cap) {
        int n = s_cap ? s_cap * 2 : 16384;
        LightTri *p = realloc(s_tri, (size_t)n * sizeof *p);
        if (!p)
            return 0;
        s_tri = p;
        s_cap = n;
    }
    if (view == 0)
        s_cam_main = s_cam;
    t = &s_tri[s_ntri++];
    t->texture = texture;
    t->state = state;
    t->view = (uint8_t)view;
    for (i = 0; i < 3; i++) {
        double e = unproject(v[i], t->p[i]);
        if (e > s_st.err)
            s_st.err = e;
        t->uv[i][0] = v[i]->s;
        t->uv[i][1] = v[i]->t;
        t->col[i][0] = (uint8_t)fminf(fmaxf(v[i]->r, 0), 255);
        t->col[i][1] = (uint8_t)fminf(fmaxf(v[i]->g, 0), 255);
        t->col[i][2] = (uint8_t)fminf(fmaxf(v[i]->b, 0), 255);
        t->col[i][3] = (uint8_t)fminf(fmaxf(v[i]->a, 0), 255);
        if (view == 0)
            for (k = 0; k < 3; k++) {
                if (!s_st.tris[0] && !i && s_st.lo[k] == 0 && s_st.hi[k] == 0)
                    s_st.lo[k] = s_st.hi[k] = t->p[i][k];
                s_st.lo[k] = fminf(s_st.lo[k], t->p[i][k]);
                s_st.hi[k] = fmaxf(s_st.hi[k], t->p[i][k]);
            }
    }
    if (view == 0)
        s_blend[state & 0xFF]++;
    s_st.tris[view & 3]++;
    s_fr[view & 3]++;
    if (view == 3)
        return main_view ? 2 : 0;   /* the main view's backdrop: 2 */
    memcpy(world, t->p, sizeof t->p);
    return 1;
}

int light_capture_camera(LightCamera *c)
{
    *c = s_cam_main;
    return s_cam_main.sx != 0.0f;
}

/* where the main camera is: the point its view x projection takes to
 * x = y = w = 0 */
int light_capture_eye(float eye[3])
{
    const float *m = s_cam_main.vp;
    double a[3][3], b[3], det;
    int i, j;
    for (i = 0; i < 3; i++) {
        int k = i == 2 ? 3 : i;
        for (j = 0; j < 3; j++)
            a[i][j] = m[j * 4 + k];
        b[i] = -m[12 + k];
    }
    det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
        + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (fabs(det) < 1e-30)
        return 0;
    for (i = 0; i < 3; i++) {
        double c[3][3];
        memcpy(c, a, sizeof c);
        for (j = 0; j < 3; j++)
            c[j][i] = b[j];
        eye[i] = (float)((c[0][0] * (c[1][1] * c[2][2] - c[1][2] * c[2][1]) - c[0][1] * (c[1][0] * c[2][2] - c[1][2] * c[2][0])
                          + c[0][2] * (c[1][0] * c[2][1] - c[1][1] * c[2][0])) / det);
    }
    return 1;
}

static void dump(const char *path)
{
    FILE *f = fopen(path, "w");
    char png[1100];
    int i, j, n = 0;
    if (!f)
        return;
    fprintf(f, "# frame %u: camera triangles in world space (x y z, then colour 0..1)\n", s_frame);
    fprintf(f, "# camera vp");
    for (i = 0; i < 16; i++)
        fprintf(f, " %.9g", s_cam_main.vp[i]);
    fprintf(f, "\n# camera viewport %.9g %.9g %.9g %.9g\n", s_cam_main.sx, s_cam_main.ox, s_cam_main.sy, s_cam_main.oy);
    for (i = 0; i < s_ntri; i++) {
        if (s_tri[i].view != 0)
            continue;
        for (j = 0; j < 3; j++)
            fprintf(f, "v %.4f %.4f %.4f %.3f %.3f %.3f\n", s_tri[i].p[j][0], s_tri[i].p[j][1], s_tri[i].p[j][2],
                    s_tri[i].col[j][0] / 255.0, s_tri[i].col[j][1] / 255.0, s_tri[i].col[j][2] / 255.0);
        fprintf(f, "f %d %d %d\n", n + 1, n + 2, n + 3);
        n += 3;
    }
    fclose(f);
    snprintf(png, sizeof png, "%s.png", path);       /* the frame as the game drew it */
    brr_shot(png);
    fprintf(stderr, "light: frame %u, %d camera triangles -> %s (and %s)\n", s_frame, n / 3, path, png);
}

/* BR_LIGHT_TRACKCHECK=1: does the track (light_game_track) hold what the
 * camera drew?  Every camera vertex is tested for lying on a track
 * triangle (within 0.05 units: clipping makes corners that are no track
 * vertex, but they lie on its surface); once a second, the share that do.
 * The cars and their effects are not track, so they are the expected rest. */
#define TC_CELL LIGHT_TC_CELL
#define TC_HASH LIGHT_TC_HASH
static struct { unsigned gen; int *head, *next, *tri, n, cap; double on, all, car, miss[5]; unsigned mstate[256]; } s_tc;

static unsigned tc_key(int x, int y, int z) { return ((unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)z * 83492791u) & (TC_HASH - 1); }

static void tc_build(const LightTrack *t)
{
    int i;
    s_tc.gen = t->gen;
    s_tc.n = 0;
    if (!s_tc.head)
        s_tc.head = malloc(TC_HASH * sizeof *s_tc.head);
    for (i = 0; i < TC_HASH; i++)
        s_tc.head[i] = -1;
    for (i = 0; i < t->ntri; i++) {
        const float *p = &t->tri[9 * i];
        int lo[3], hi[3], a, x, y, z;
        for (a = 0; a < 3; a++) {
            lo[a] = (int)floorf(fminf(fminf(p[a], p[3 + a]), p[6 + a]) / TC_CELL);
            hi[a] = (int)floorf(fmaxf(fmaxf(p[a], p[3 + a]), p[6 + a]) / TC_CELL);
            if (hi[a] - lo[a] > 32)
                hi[a] = lo[a] + 32;
        }
        for (x = lo[0]; x <= hi[0]; x++)
            for (y = lo[1]; y <= hi[1]; y++)
                for (z = lo[2]; z <= hi[2]; z++) {
                    unsigned k = tc_key(x, y, z);
                    if (s_tc.n == s_tc.cap) {
                        int c = s_tc.cap ? s_tc.cap * 2 : 1 << 20;
                        s_tc.next = realloc(s_tc.next, (size_t)c * sizeof *s_tc.next);
                        s_tc.tri = realloc(s_tc.tri, (size_t)c * sizeof *s_tc.tri);
                        s_tc.cap = c;
                    }
                    s_tc.tri[s_tc.n] = i;
                    s_tc.next[s_tc.n] = s_tc.head[k];
                    s_tc.head[k] = s_tc.n++;
                }
    }
}

/* the distance from p to triangle abc (Ericson, Real-Time Collision Detection 5.1.5) */
static float tc_dist(const float *p, const float *a, const float *b, const float *c)
{
    float ab[3], ac[3], ap[3], bp[3], cp[3], q[3], d1, d2, d3, d4, d5, d6, va, vb, vc, v, w, den;
    int k;
    for (k = 0; k < 3; k++) { ab[k] = b[k] - a[k]; ac[k] = c[k] - a[k]; ap[k] = p[k] - a[k]; bp[k] = p[k] - b[k]; cp[k] = p[k] - c[k]; }
#define D(u, v) ((u)[0] * (v)[0] + (u)[1] * (v)[1] + (u)[2] * (v)[2])
    d1 = D(ab, ap); d2 = D(ac, ap); d3 = D(ab, bp); d4 = D(ac, bp); d5 = D(ab, cp); d6 = D(ac, cp);
#undef D
    if (d1 <= 0 && d2 <= 0) { memcpy(q, a, 12); goto out; }
    if (d3 >= 0 && d4 <= d3) { memcpy(q, b, 12); goto out; }
    vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { v = d1 / (d1 - d3); for (k = 0; k < 3; k++) q[k] = a[k] + v * ab[k]; goto out; }
    if (d6 >= 0 && d5 <= d6) { memcpy(q, c, 12); goto out; }
    vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { w = d2 / (d2 - d6); for (k = 0; k < 3; k++) q[k] = a[k] + w * ac[k]; goto out; }
    va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        for (k = 0; k < 3; k++) q[k] = b[k] + w * (c[k] - b[k]);
        goto out;
    }
    den = 1.0f / (va + vb + vc);
    v = vb * den; w = vc * den;
    for (k = 0; k < 3; k++) q[k] = a[k] + ab[k] * v + ac[k] * w;
out:
    for (k = 0; k < 3; k++) q[k] -= p[k];
    return sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
}

/* 1 when a triangle lies on one of the track's triangles: every corner
 * within 0.05 of the same one (what the camera drew of the track, clipped
 * into pieces).  Its shadow is the track's own, which the cascades' static
 * cache already holds; brr_metal.m draws only the rest (the cars, the start
 * lights, what the race adds) as the moving casters.  A triangle within 0.05
 * of a track triangle at its corners is within 0.05 of it everywhere (both
 * are convex), so leaving it out changes no shadow by more than that.  The
 * candidates are those of the grid cell holding its centroid; a miss only
 * keeps it as a moving caster. */
int light_capture_trackhash(const int **head, const int **next, const int **tri, int *n, unsigned *gen)
{
    const LightTrack *t = light_game_track();
    if (!t || !t->ntri)
        return 0;
    if (t->gen != s_tc.gen || !s_tc.head)
        tc_build(t);
    *head = s_tc.head;
    *next = s_tc.next;
    *tri = s_tc.tri;
    *n = s_tc.n;
    *gen = s_tc.gen;
    return 1;
}

int light_capture_ontrack(const float tri[9])
{
    const LightTrack *t = light_game_track();
    float m[3];
    int k, e;
    if (!t || !t->ntri)
        return 0;
    if (t->gen != s_tc.gen || !s_tc.head)
        tc_build(t);
    for (k = 0; k < 3; k++)
        m[k] = (tri[k] + tri[3 + k] + tri[6 + k]) / 3.0f;
    for (e = s_tc.head[tc_key((int)floorf(m[0] / TC_CELL), (int)floorf(m[1] / TC_CELL), (int)floorf(m[2] / TC_CELL))]; e >= 0; e = s_tc.next[e]) {
        const float *q = &t->tri[9 * s_tc.tri[e]];
        if (tc_dist(tri, q, q + 3, q + 6) < 0.05f && tc_dist(tri + 3, q, q + 3, q + 6) < 0.05f && tc_dist(tri + 6, q, q + 3, q + 6) < 0.05f)
            return 1;
    }
    return 0;
}

int light_capture_oncar(const float tri[9])
{
    static LightCar car[20];
    static int nc;
    static unsigned at = ~0u;
    float m[3];
    int c, k;
    if (at != s_frame) {                /* the cars once a frame */
        at = s_frame;
        nc = light_game_cars(car, 20);
    }
    for (k = 0; k < 3; k++)
        m[k] = (tri[k] + tri[3 + k] + tri[6 + k]) / 3.0f;
    for (c = 0; c < nc; c++)
        if (light_car_inbox(&car[c], m, LIGHT_CAR_MARGIN))
            return !light_capture_ontrack(tri);
    return 0;
}

static void tc_frame(void)
{
    const LightTrack *t = light_game_track();
    LightCar car[20];
    int i, j, nc = light_game_cars(car, 20);
    if (!t || !t->ntri)
        return;
    if (nc && s_frame == 1000)
        fprintf(stderr, "light: car 0 pos %.2f %.2f %.2f fwd %.2f %.2f %.2f up %.2f %.2f %.2f box %d %.2f..%.2f %.2f..%.2f %.2f..%.2f\n",
                car[0].pos[0], car[0].pos[1], car[0].pos[2], car[0].fwd[0], car[0].fwd[1], car[0].fwd[2], car[0].up[0], car[0].up[1], car[0].up[2],
                car[0].has_box, car[0].lo[0], car[0].hi[0], car[0].lo[1], car[0].hi[1], car[0].lo[2], car[0].hi[2]);
    if (t->gen != s_tc.gen)
        tc_build(t);
    for (i = 0; i < s_ntri; i++) {
        if (s_tri[i].view != 0)
            continue;
        for (j = 0; j < 3; j++) {
            const float *p = s_tri[i].p[j];
            int c, oncar = 0;
            for (c = 0; c < nc && !oncar; c++) {   /* a car is not track: inside its box, in its own frame */
                float d[3] = { p[0] - car[c].pos[0], p[1] - car[c].pos[1], p[2] - car[c].pos[2] };
                float l[3] = { d[0] * car[c].fwd[0] + d[1] * car[c].fwd[1] + d[2] * car[c].fwd[2],
                               d[0] * car[c].side[0] + d[1] * car[c].side[1] + d[2] * car[c].side[2],
                               d[0] * car[c].up[0] + d[1] * car[c].up[1] + d[2] * car[c].up[2] };
                int a;
                oncar = car[c].has_box;
                for (a = 0; a < 3 && oncar; a++)
                    oncar = l[a] > car[c].lo[a] - 0.5f && l[a] < car[c].hi[a] + 0.5f;
            }
            if (oncar) {
                s_tc.car++;
                continue;
            }
            unsigned k = tc_key((int)floorf(p[0] / TC_CELL), (int)floorf(p[1] / TC_CELL), (int)floorf(p[2] / TC_CELL));
            int e, on = 0;
            float best = 1e30f;
            for (e = s_tc.head[k]; e >= 0 && !on; e = s_tc.next[e]) {
                const float *q = &t->tri[9 * s_tc.tri[e]];
                best = fminf(best, tc_dist(p, q, q + 3, q + 6));
                on = best < 0.05f;
            }
            s_tc.on += on;
            s_tc.all++;
            s_tc.miss[best < 0.05f ? 0 : best < 0.5f ? 1 : best < 5.0f ? 2 : best < 1e29f ? 3 : 4]++;
            if (!on && j == 0)
                s_tc.mstate[s_tri[i].state & 0xFF]++;
        }
    }
}

void light_capture_swap(void)
{
    static int stats = -1, dump_at = -1;
    static char dump_path[1024];
    static uint64_t t0;
    if (stats < 0) {
        const char *e = getenv("BR_LIGHT_STATS");
        stats = e && atoi(e) != 0;
        if ((e = getenv("BR_LIGHT_DUMP")))
            sscanf(e, "%d:%1023s", &dump_at, dump_path);
    }
    {
        static FILE *log;
        static int init;
        if (!init) {
            const char *e = getenv("BR_LIGHT_FRAMELOG");
            init = 1;
            log = e ? fopen(e, "w") : NULL;
        }
        if (log && s_ntri)
            fprintf(log, "%u %u %u %u\n", s_frame, s_fr[0], s_fr[2], s_fr[3]);
        memset(s_fr, 0, sizeof s_fr);
    }
    {
        static int tc = -1;
        if (tc < 0)
            tc = getenv("BR_LIGHT_TRACKCHECK") != NULL;
        if (tc && s_ntri)
            tc_frame();
    }
    s_frame++;
    if (s_ntri)
        s_st.frames++;
    if (dump_at >= 0 && (int)s_frame == dump_at)
        dump(dump_path);
    if (stats) {
        uint64_t now = host_ticks_ns();
        if (!t0)
            t0 = now;
        if (now - t0 >= 1000000000ull && s_st.frames) {
            fprintf(stderr, "light: weather %d, %u frames, %.0f camera + %.0f mirror + %.0f backdrop triangles a frame, reprojection error %.4f px max,"
                            " world x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f\n",
                    light_game_weather(), s_st.frames, (double)s_st.tris[0] / s_st.frames, (double)s_st.tris[2] / s_st.frames,
                    (double)s_st.tris[3] / s_st.frames, s_st.err,
                    s_st.lo[0], s_st.hi[0], s_st.lo[1], s_st.hi[1], s_st.lo[2], s_st.hi[2]);
            {
                int b;
                fprintf(stderr, "light: camera triangles by blend (src/dst):");
                for (b = 0; b < 256; b++)
                    if (s_blend[b])
                        fprintf(stderr, " %d/%d x%u", b & 15, b >> 4, s_blend[b]);
                fprintf(stderr, "\n");
                memset(s_blend, 0, sizeof s_blend);
            }
            if (s_tc.all > 0) {
                int b;
                fprintf(stderr, "light: %.2f%% of %.0f camera vertices off the cars lie on the track (%.0f on cars); nearest track triangle"
                                " <0.05 %.0f, <0.5 %.0f, <5 %.0f, farther %.0f, none in the cell %.0f\n",
                        100.0 * s_tc.on / s_tc.all, s_tc.all, s_tc.car, s_tc.miss[0], s_tc.miss[1], s_tc.miss[2], s_tc.miss[3], s_tc.miss[4]);
                s_tc.car = 0;
                fprintf(stderr, "light: triangles off the track by blend (src/dst):");
                for (b = 0; b < 256; b++)
                    if (s_tc.mstate[b])
                        fprintf(stderr, " %d/%d x%u", b & 15, b >> 4, s_tc.mstate[b]);
                fprintf(stderr, "\n");
                memset(s_tc.miss, 0, sizeof s_tc.miss);
                memset(s_tc.mstate, 0, sizeof s_tc.mstate);
                s_tc.on = s_tc.all = 0;
            }
            memset(&s_st, 0, sizeof s_st);
            t0 = now;
        }
    }
    s_ntri = 0;
}
