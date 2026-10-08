/* main.c -- Top Gear Rally on the GBA, proof of concept: the static world of
 * a recorded race (tools/convert.py), drawn from the race's own camera path,
 * flat shaded, in bitmap mode 5 (160x128, two pages) stretched to the screen.
 *
 * Per frame: the grid cells near the camera and in front of it are chosen;
 * each chosen cell's vertices go through the frame's fixed-point camera
 * matrix; each triangle is clipped against the near plane, back-face and
 * screen culled, projected through a reciprocal table and put into a depth
 * bucket; the buckets are drawn far to near.  The cost of each step is
 * counted with the timers into g_stats (read by the host harness). */
#include <stdint.h>
#include "world.h"

#define IWRAM_CODE __attribute__((section(".iwram"), target("arm"), noinline))
#define EWRAM_BSS __attribute__((section(".ewram_bss")))

#define REG_DISPCNT (*(volatile uint16_t *)0x04000000)
#define REG_VCOUNT (*(volatile uint16_t *)0x04000006)
#define REG_DISPSTAT (*(volatile uint16_t *)0x04000004)
#define REG_IE (*(volatile uint16_t *)0x04000200)
#define REG_IME (*(volatile uint16_t *)0x04000208)
#define REG_BG2PA (*(volatile int16_t *)0x04000020)
#define REG_BG2PB (*(volatile int16_t *)0x04000022)
#define REG_BG2PC (*(volatile int16_t *)0x04000024)
#define REG_BG2PD (*(volatile int16_t *)0x04000026)
#define REG_BG2X (*(volatile int32_t *)0x04000028)
#define REG_BG2Y (*(volatile int32_t *)0x0400002C)
#define REG_WAITCNT (*(volatile uint16_t *)0x04000204)
#define REG_TM2D (*(volatile uint16_t *)0x04000108)
#define REG_TM2C (*(volatile uint16_t *)0x0400010A)
#define REG_TM3D (*(volatile uint16_t *)0x0400010C)
#define REG_TM3C (*(volatile uint16_t *)0x0400010E)
#define REG_DMA3SAD (*(const void *volatile *)0x040000D4)
#define REG_DMA3DAD (*(void *volatile *)0x040000D8)
#define REG_DMA3CNT (*(volatile uint32_t *)0x040000DC)

#define SW 160
#define SH 128
#define NEAR8 128                 /* the near plane, W in 1/256 units: half a unit */
#define CELLR 24                  /* a cell's reach from its centre, rounded up */
#define NBUCKET 256
#define MAXPOLY 2400

typedef struct { int32_t X, Y, W, sx, sy, u, v; } PV;        /* a transformed vertex (Q4 screen), a corner's u, v */
typedef struct { int32_t x, y, u, v; } PolyV;                /* Q4 screen, texels Q4 */
typedef struct { PolyV v[3]; uint16_t col, next, tex, pad; } Poly;

/* the counters the host reads: frame, cycles total, cycles transform/sort,
   cycles raster, triangles drawn, cells drawn, vertices transformed, game frame */
volatile uint32_t g_stats[12];
static int s_lapped;
uint32_t g_cyc[1200] EWRAM_BSS;                        /* each race frame's cycles, the first time round */
/* the host's probe: g_probe[0] = camera frame + 1 to hold the camera there; g_probe[1] set:
   stop once the frame's lists are built (for a look at memory); g_probe[2] set: one recorded camera
   frame a frame drawn, the whole race in turn (for measuring); else real time */
volatile uint32_t g_probe[3];

PV s_pv[128] __attribute__((section(".iwram_bss")));   /* a cell's vertices (convert.py: at most 128) */
Poly s_poly[MAXPOLY] EWRAM_BSS;
uint16_t s_bucket[NBUCKET] EWRAM_BSS;
int s_npoly;
uint32_t s_rec[4096] EWRAM_BSS;     /* 2^24 / w, w < 4096 (recip normalises a larger w) */
uint8_t s_rsh[256] __attribute__((section(".iwram_bss")));   /* w >> 12's bit length: recip's shift for w under 2^20 */
uint16_t *s_back;

static uint32_t udiv(uint32_t n, uint32_t d)
{
    uint32_t q = 0, bit = 1;
    if (!d)
        return 0xFFFFFFFF;
    while (d < n && !(d & 0x80000000)) {
        d <<= 1;
        bit <<= 1;
    }
    while (bit) {
        if (n >= d) {
            n -= d;
            q |= bit;
        }
        d >>= 1;
        bit >>= 1;
    }
    return q;
}

static inline uint32_t clock32(void) { return REG_TM2D | (uint32_t)REG_TM3D << 16; }

#if defined RASTER_C || defined FRONT_C
/* 1/w: a table below 4096, a normalised lookup above */
static inline __attribute__((always_inline)) uint32_t recip(int32_t w)
{
    int sh = 0;
    while (w >= 4096) {
        w >>= 1;
        sh++;
    }
    return s_rec[w] >> sh;
}
#endif

uint16_t s_tag[128] __attribute__((section(".iwram_bss")));   /* s_pv[i] is this cell's */
uint16_t s_stamp;
uint32_t s_ct;                                        /* front.s: the triangle's col | tex << 16 */

#ifdef FRONT_C
static inline __attribute__((always_inline)) void project(PV *v)
{
    uint32_t r = recip(v->W);
    int64_t x = (((int64_t)v->X * r) >> 20) + SW / 2 * 16, y = (((int64_t)v->Y * r) >> 20) + SH / 2 * 16;
    /* a vertex at the near plane can land far off the screen: keep it in range */
    v->sx = (int32_t)(x < -(1 << 18) ? -(1 << 18) : x > (1 << 18) ? (1 << 18) : x);
    v->sy = (int32_t)(y < -(1 << 18) ? -(1 << 18) : y > (1 << 18) ? (1 << 18) : y);
}


/* a cell vertex through the frame's camera, the first time a drawn triangle needs it */
static inline __attribute__((always_inline)) const PV *vertex(const int32_t *m, const V3 *vs, int i, int *n)
{
    PV *o = &s_pv[i];
    if (s_tag[i] != s_stamp) {
        int32_t x = vs[i].x, y = vs[i].y, z = vs[i].z;
        s_tag[i] = s_stamp;
        o->X = (int32_t)(((int64_t)m[0] * x + (int64_t)m[1] * y + (int64_t)m[2] * z) >> 12) + m[3];
        o->Y = (int32_t)(((int64_t)m[4] * x + (int64_t)m[5] * y + (int64_t)m[6] * z) >> 12) + m[7];
        o->W = (int32_t)(((int64_t)m[8] * x + (int64_t)m[9] * y + (int64_t)m[10] * z) >> 12) + m[11];
        if (o->W >= NEAR8)
            project(o);
        ++*n;
    }
    return o;
}

static IWRAM_CODE void emit(const PV *a, const PV *b, const PV *c, uint16_t col, uint16_t tex);

/* a textured triangle spanning much depth, its edges halved in clip space
   (where a midpoint's u, v are exact under perspective) until each piece is
   nearly flat in depth: an affine texture across it then barely bends.  An
   edge is halved by what its own two ends say (a neighbour sharing it agrees,
   so no crack opens at a T): when its far end is more than twice as deep as
   its near end and it is over 16 pixels long, GMAX times at most (g: the
   edge's halvings so far). */
#define GMAX 3
static inline __attribute__((always_inline)) int halve(const PV *p, const PV *q, int g)
{
    int32_t lo = p->W < q->W ? p->W : q->W, hi = p->W ^ q->W ^ lo, dx = p->sx - q->sx, dy = p->sy - q->sy;
    return g < GMAX && hi - lo > lo && (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) > 16 * 16;
}

static inline __attribute__((always_inline)) void mid(const PV *p, const PV *q, PV *m)
{
    m->X = (p->X >> 1) + (q->X >> 1);
    m->Y = (p->Y >> 1) + (q->Y >> 1);
    m->W = (p->W + q->W) >> 1;
    m->u = (p->u + q->u) >> 1;
    m->v = (p->v + q->v) >> 1;
    project(m);
}

/* g packs the three edges' halvings: a-b in bits 0-2, b-c in 3-5, c-a in 6-8 */
static IWRAM_CODE void persp(const PV *a, const PV *b, const PV *c, uint16_t col, uint16_t tex, int g)
{
    int g0 = g & 7, g1 = g >> 3 & 7, g2 = g >> 6 & 7, gi;
    int h;
    PV m[3];
    if ((a->sx < 0 && b->sx < 0 && c->sx < 0) || (a->sx >= SW * 16 && b->sx >= SW * 16 && c->sx >= SW * 16) ||
        (a->sy < 0 && b->sy < 0 && c->sy < 0) || (a->sy >= SH * 16 && b->sy >= SH * 16 && c->sy >= SH * 16))
        return;                                      /* off the screen, and so every piece of it */
    h = halve(a, b, g0) | halve(b, c, g1) << 1 | halve(c, a, g2) << 2;
    if (!h) {
        emit(a, b, c, col, tex);
        return;
    }
    /* turn the triangle so the halved edges come first: a-b, then b-c */
    while (h != 1 && h != 3 && h != 7) {
        const PV *t = a;
        a = b; b = c; c = t;
        g = (g >> 3 | g << 6) & 511;
        h = (h >> 1 | h << 2) & 7;
    }
    g0 = (g & 7) + 1; g1 = (g >> 3 & 7) + 1; g2 = (g >> 6 & 7) + 1;
    gi = h == 1 ? g0 : h == 3 ? (g0 > g1 ? g0 : g1) : (g0 > g1 ? g0 : g1) > g2 ? (g0 > g1 ? g0 : g1) : g2;
    /* an edge across the inside: as many halvings as the most halved edge cut */
    mid(a, b, &m[0]);
    if (h == 1) {                                    /* a-b: two */
        persp(a, &m[0], c, col, tex, g0 | gi << 3 | (g2 - 1) << 6);
        persp(&m[0], b, c, col, tex, g0 | (g1 - 1) << 3 | gi << 6);
        return;
    }
    mid(b, c, &m[1]);
    if (h == 3) {                                    /* a-b and b-c: three */
        persp(&m[0], b, &m[1], col, tex, g0 | g1 << 3 | gi << 6);
        persp(a, &m[0], &m[1], col, tex, g0 | gi << 3 | gi << 6);
        persp(a, &m[1], c, col, tex, gi | g1 << 3 | (g2 - 1) << 6);
        return;
    }
    mid(c, a, &m[2]);                                /* all three: four */
    persp(a, &m[0], &m[2], col, tex, g0 | gi << 3 | g2 << 6);
    persp(&m[0], b, &m[1], col, tex, g0 | g1 << 3 | gi << 6);
    persp(&m[2], &m[1], c, col, tex, gi | g1 << 3 | g2 << 6);
    persp(&m[0], &m[1], &m[2], col, tex, gi | gi << 3 | gi << 6);
}
static IWRAM_CODE void emit(const PV *a, const PV *b, const PV *c, uint16_t col, uint16_t tex)
{
    int32_t area, w, minx, maxx, miny, maxy;
    Poly *p;
    int64_t ar = (int64_t)(b->sx - a->sx) * (c->sy - a->sy) - (int64_t)(b->sy - a->sy) * (c->sx - a->sx);
    area = ar > 0 ? 1 : ar < 0 ? -1 : 0;
    if (area >= 0)                                   /* facing away (or edge on) */
        return;
    minx = a->sx < b->sx ? a->sx : b->sx;
    minx = minx < c->sx ? minx : c->sx;
    maxx = a->sx > b->sx ? a->sx : b->sx;
    maxx = maxx > c->sx ? maxx : c->sx;
    miny = a->sy < b->sy ? a->sy : b->sy;
    miny = miny < c->sy ? miny : c->sy;
    maxy = a->sy > b->sy ? a->sy : b->sy;
    maxy = maxy > c->sy ? maxy : c->sy;
    if (maxx < 0 || minx >= SW * 16 || maxy < 0 || miny >= SH * 16 || s_npoly >= MAXPOLY)
        return;
    /* no row centre within it, or no column centre (a sixteenth's grace for the edge steps'
       rounding): it covers no pixel */
    if ((miny + 7) >> 4 >= (maxy + 7) >> 4 || (minx + 6) >> 4 >= (maxx + 8) >> 4)
        return;
    w = (a->W + b->W + c->W) >> 8;                   /* the depth key: three times mean W, in units */
    w = w >= NBUCKET ? NBUCKET - 1 : w;
    p = &s_poly[s_npoly];
    p->v[0].x = a->sx; p->v[0].y = a->sy; p->v[0].u = a->u; p->v[0].v = a->v;
    p->v[1].x = b->sx; p->v[1].y = b->sy; p->v[1].u = b->u; p->v[1].v = b->v;
    p->v[2].x = c->sx; p->v[2].y = c->sy; p->v[2].u = c->u; p->v[2].v = c->v;
    p->col = col;
    p->tex = tex;
    p->next = s_bucket[w];
    s_bucket[w] = (uint16_t)++s_npoly;
}

/* the point where edge a-b meets the near plane */
static IWRAM_CODE void cut(const PV *a, const PV *b, PV *o)
{
    if (a->W > b->W) {                               /* from the same end either way round: */
        const PV *t = a;                             /* neighbours sharing the edge get the */
        a = b;                                       /* same point, to the bit (no crack)   */
        b = t;
    }
    int32_t d = b->W - a->W;                         /* the planes are crossed: d != 0 */
    int32_t t = (int32_t)(((int64_t)(NEAR8 - a->W) * (int32_t)recip(d < 0 ? -d : d)) >> 8);
    if (d < 0)
        t = -t;
    o->X = a->X + (int32_t)(((int64_t)(b->X - a->X) * t) >> 16);
    o->Y = a->Y + (int32_t)(((int64_t)(b->Y - a->Y) * t) >> 16);
    o->u = a->u + (int32_t)(((int64_t)(b->u - a->u) * t) >> 16);
    o->v = a->v + (int32_t)(((int64_t)(b->v - a->v) * t) >> 16);
    o->W = NEAR8;
    project(o);
}

static IWRAM_CODE void triangle(const PV *p0, const PV *p1, const PV *p2, const Tri *t)
{
    PV c[3], q[4];
    int i, n = 0, in = (p0->W >= NEAR8) + (p1->W >= NEAR8) + (p2->W >= NEAR8);
    if (in == 0)
        return;
    if (in == 3) {                                   /* all in front: facing away, or off the screen? */
        int64_t ar = (int64_t)(p1->sx - p0->sx) * (p2->sy - p0->sy) - (int64_t)(p1->sy - p0->sy) * (p2->sx - p0->sx);
        if (ar >= 0)                                 /* (every piece of it would be) */
            return;
        if ((p0->sx < 0 && p1->sx < 0 && p2->sx < 0) || (p0->sx >= SW * 16 && p1->sx >= SW * 16 && p2->sx >= SW * 16) ||
            (p0->sy < 0 && p1->sy < 0 && p2->sy < 0) || (p0->sy >= SH * 16 && p1->sy >= SH * 16 && p2->sy >= SH * 16))
            return;
    }
    if (in == 3) {                                   /* its u, v into the cell's corners themselves */
        PV *q0 = (PV *)p0, *q1 = (PV *)p1, *q2 = (PV *)p2;   /* (each triangle sets them as it uses them) */
        q0->u = t->uv[0]; q0->v = t->uv[1];
        q1->u = t->uv[2]; q1->v = t->uv[3];
        q2->u = t->uv[4]; q2->v = t->uv[5];
        if (t->tex != 0xFFFF)
            persp(q0, q1, q2, t->col, t->tex, 0);
        else
            emit(q0, q1, q2, t->col, t->tex);
        return;
    }
    c[0] = *p0; c[1] = *p1; c[2] = *p2;               /* this triangle's corners: its own u, v */
    for (i = 0; i < 3; i++) {
        c[i].u = t->uv[i * 2];
        c[i].v = t->uv[i * 2 + 1];
    }
    for (i = 0; i < 3; i++) {                        /* clip the polygon against W = near */
        const PV *a = &c[i], *b = &c[(i + 1) % 3];
        if (a->W >= NEAR8)
            q[n++] = *a;
        if ((a->W >= NEAR8) != (b->W >= NEAR8))
            cut(a, b, &q[n++]);
    }
    if (t->tex != 0xFFFF) {
        persp(&q[0], &q[1], &q[2], t->col, t->tex, 0);
        if (n == 4)
            persp(&q[0], &q[2], &q[3], t->col, t->tex, 0);
    } else {
        emit(&q[0], &q[1], &q[2], t->col, t->tex);
        if (n == 4)
            emit(&q[0], &q[2], &q[3], t->col, t->tex);
    }
}


/* what the game drew from the camera's grid cell (its visibility list), each
   triangle only at the distances the game drew it at (its detail levels) */
static IWRAM_CODE void render_visible(const Frame *f, int *ncells, int *nverts)
{
    const int32_t *m = f->m;
    int32_t px = f->pos[0], py = f->pos[1];
    int ax = ((px + g_org[0]) >> 5) - g_pvs_x0, ay = ((py + g_org[1]) >> 5) - g_pvs_y0;   /* 32-unit cells */
    const uint16_t *p;
    uint32_t at;
    if (ax < 0 || ay < 0 || ax >= g_pvs_w || ay >= g_pvs_h)
        return;
    at = g_pvs_at[ay * g_pvs_w + ax];
    if (at == 0xFFFFFFFF)
        return;
    p = g_pvs + at;
    while (*p != 0xFFFF) {
        const Cell *c = &g_cells[*p++];
        int n = *p++, k;
        const V3 *vs = &g_verts[c->vstart];
        const Tri *tb = &g_tris[c->tstart];
        for (k = 0; k < 4; k++) {                    /* the cell wholly past an edge of the screen? */
            const int32_t *e = f->edge[k];
            if (e[0] * c->c[0] + e[1] * c->c[1] + e[2] * c->c[2] < (-e[3] - c->r - 2) * 4096)
                break;
        }
        if (k < 4) {
            p += n;
            continue;
        }
        s_stamp++;
        while (n--) {
            const Tri *t = &tb[*p++];
            int32_t ex = t->cx - px, ey = t->cy - py, e2 = ex * ex + ey * ey;
            int32_t a = t->dlo * 8, b = t->dhi * 8;
            if (e2 < a * a || e2 > b * b)
                continue;
            /* the camera clearly behind its plane: it faces away (the screen's test, exact,
               decides the rest) */
            if (t->n[0] * f->cam[0] + t->n[1] * f->cam[1] + t->n[2] * f->cam[2] - t->d < -2 * 4096)
                continue;
            triangle(vertex(m, vs, t->a, nverts), vertex(m, vs, t->b, nverts), vertex(m, vs, t->c, nverts), t);
        }
        ++*ncells;
    }
}
#else
void render_visible(const Frame *f, int *ncells, int *nverts);                       /* front.s */
#endif

#ifdef RASTER_C
/* a flat triangle, vertices in Q4 pixels, filled at pixel centres */
static IWRAM_CODE void raster(const Poly *p)
{
    int32_t x0 = p->v[0].x, y0 = p->v[0].y, x1 = p->v[1].x, y1 = p->v[1].y, x2 = p->v[2].x, y2 = p->v[2].y, t;
    int32_t y, ye, xl, xr, xs, xo;
    uint32_t col = p->col | (uint32_t)p->col << 16;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y0 > y2) { t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y2 - y0 < 1)
        return;
    /* long edge 0-2, short edges 0-1 then 1-2; slopes in Q16 pixels a row */
    {
        int32_t d02 = (int32_t)(((int64_t)(x2 - x0) * (int32_t)recip(y2 - y0)) >> 8);
        int half;
        for (half = 0; half < 2; half++) {
            int32_t ya = half ? y1 : y0, yb = half ? y2 : y1, xa = half ? x1 : x0, xb = half ? x2 : x1, ds;
            if (yb - ya < 1)
                continue;
            ds = (int32_t)(((int64_t)(xb - xa) * (int32_t)recip(yb - ya)) >> 8);
            y = (ya + 7) >> 4;                       /* the rows whose centre 16y+8 is in [ya, yb) */
            ye = (yb + 7) >> 4;
            if (y < 0)
                y = 0;
            if (ye > SH)
                ye = SH;
            {
                int32_t yc = y * 16 + 8;
                xs = (xa << 12) + (int32_t)(((int64_t)ds * (yc - ya)) >> 4);   /* Q16 pixels */
                xo = (x0 << 12) + (int32_t)(((int64_t)d02 * (yc - y0)) >> 4);
            }
            for (; y < ye; y++, xs += ds, xo += d02) {
                int32_t a, b;
                uint16_t *row;
                if (xs < xo) { xl = xs; xr = xo; } else { xl = xo; xr = xs; }
                a = (xl - (1 << 15) + 0xFFFF) >> 16;   /* the pixels whose centre x + 0.5 is in [xl, xr) */
                b = (xr - (1 << 15) + 0xFFFF) >> 16;
                if (a < 0) a = 0;
                if (b > SW) b = SW;
                if (a >= b)
                    continue;
                row = s_back + y * SW;
                if (a & 1)
                    row[a++] = (uint16_t)col;
                {
                    uint32_t *w = (uint32_t *)(row + a);
                    int n = (b - a) >> 1;
                    while (n >= 4) {
                        w[0] = col; w[1] = col; w[2] = col; w[3] = col;
                        w += 4;
                        n -= 4;
                    }
                    while (n--)
                        *w++ = col;
                    if ((b - a) & 1)
                        row[b - 1] = (uint16_t)col;
                }
            }
        }
    }
}
#else
void raster(const Poly *p);                                                           /* raster.s */
#endif

/* a textured triangle: affine u, v across it, the texture wrapping at its
   power-of-two size, a clear texel (bit 15) left undrawn.  This is the
   reference for raster.s (RASTER_C builds it instead): every value below is
   computed the same way there, to the bit.
     vertices: Q4 pixels, within the guard band (emit clips to it)
     u, v: Q16 texels; at pixel (a, y): Cu + dudx * a + dudy * y
   The span steps u and v packed in one word, each 6.10 (u the high half, v
   the low; a carry out of v reaches u's last fraction bit, a 1024th of a texel):
   the texel is (uv >> 26) & wmask across, (uv >> 10) & hmask down, rows 64 apart. */
#define PACK(u, v) (((uint32_t)((u) >> 6) << 16) + (uint32_t)((v) >> 6))
#ifdef RASTER_C
/* (da1 e2 - da2 e1) / den in Q16: rs = 2^24 / (|den| >> sh), den's sign */
static inline __attribute__((always_inline)) int32_t grad(int32_t da1, int32_t da2, int32_t e1, int32_t e2,
                                                          int sh, int32_t rs)
{
    int64_t n = (int64_t)da1 * e2 - (int64_t)da2 * e1;
    return (int32_t)(((int64_t)(int32_t)(n >> sh) * rs) >> 8);
}

static IWRAM_CODE void raster_tex(const Poly *p)
{
    int32_t x0, y0, x1, y1, x2, y2, u0, v0, u1, v1, u2, v2, t;
    int32_t dx1, dy1, dx2, dy2, dudx, dvdx, dudy, dvdy, cu, cv, s02, half, rs;
    uint32_t duvx, duvy, uv0, wm2, hm7;
    int64_t den;
    uint64_t ad;
    int sh = 0, neg;
    const Tex *tx = &g_tex[p->tex];
    x0 = p->v[0].x; y0 = p->v[0].y; u0 = p->v[0].u; v0 = p->v[0].v;
    x1 = p->v[1].x; y1 = p->v[1].y; u1 = p->v[1].u; v1 = p->v[1].v;
    x2 = p->v[2].x; y2 = p->v[2].y; u2 = p->v[2].u; v2 = p->v[2].v;
#define SWAP(a, b) (t = a, a = b, b = t)
    if (y0 > y1) { SWAP(x0, x1); SWAP(y0, y1); SWAP(u0, u1); SWAP(v0, v1); }
    if (y1 > y2) { SWAP(x1, x2); SWAP(y1, y2); SWAP(u1, u2); SWAP(v1, v2); }
    if (y0 > y1) { SWAP(x0, x1); SWAP(y0, y1); SWAP(u0, u1); SWAP(v0, v1); }
#undef SWAP
    dy2 = y2 - y0;
    if (dy2 < 1)
        return;
    dx1 = x1 - x0; dy1 = y1 - y0; dx2 = x2 - x0;
    den = (int64_t)dx1 * dy2 - (int64_t)dx2 * dy1;
    if (!den)
        return;
    neg = den < 0;
    ad = (uint64_t)(neg ? -den : den);
    while (ad >= 4096) {
        ad >>= 1;
        sh++;
    }
    rs = neg ? -(int32_t)s_rec[ad] : (int32_t)s_rec[ad];
    dudx = grad(u1 - u0, u2 - u0, dy1, dy2, sh, rs);
    dvdx = grad(v1 - v0, v2 - v0, dy1, dy2, sh, rs);
    dudy = grad(u2 - u0, u1 - u0, dx2, dx1, sh, rs);         /* (u2-u0) dx1 - (u1-u0) dx2 */
    dvdy = grad(v2 - v0, v1 - v0, dx2, dx1, sh, rs);
    cu = (u0 << 12) + (int32_t)(((int64_t)dudx * (8 - x0) + (int64_t)dudy * (8 - y0)) >> 4);
    cv = (v0 << 12) + (int32_t)(((int64_t)dvdx * (8 - x0) + (int64_t)dvdy * (8 - y0)) >> 4);
    s02 = (int32_t)(((int64_t)dx2 * (int32_t)recip(dy2)) >> 8);
    duvx = PACK(dudx, dvdx);
    duvy = PACK(dudy, dvdy);
    uv0 = PACK(cu, cv);
    wm2 = ((1u << tx->wbits) - 1) << 1;
    hm7 = ((1u << tx->hbits) - 1) << 7;
    for (half = 0; half < 2; half++) {
        int32_t ya = half ? y1 : y0, yb = half ? y2 : y1, xa = half ? x1 : x0, xb = half ? x2 : x1;
        int32_t ss, yy, ye, xs, xo, yc;
        uint32_t uvr;
        if (yb - ya < 1)
            continue;
        ss = (int32_t)(((int64_t)(xb - xa) * (int32_t)recip(yb - ya)) >> 8);
        yy = (ya + 7) >> 4;
        ye = (yb + 7) >> 4;
        if (yy < 0)
            yy = 0;
        if (ye > SH)
            ye = SH;
        if (yy >= ye)
            continue;
        yc = yy * 16 + 8;
        xs = (xa << 12) + (int32_t)(((int64_t)ss * (yc - ya)) >> 4);
        xo = (x0 << 12) + (int32_t)(((int64_t)s02 * (yc - y0)) >> 4);
        uvr = uv0 + duvy * (uint32_t)yy;
        for (; yy < ye; yy++, xs += ss, xo += s02, uvr += duvy) {
            int32_t xl = xs < xo ? xs : xo, xr = xs < xo ? xo : xs, a, b;
            uint16_t *dst;
            uint32_t uv;
            a = (xl - (1 << 15) + 0xFFFF) >> 16;
            b = (xr - (1 << 15) + 0xFFFF) >> 16;
            if (a < 0) a = 0;
            if (b > SW) b = SW;
            if (a >= b)
                continue;
            dst = s_back + yy * SW + a;
            for (uv = uvr + duvx * (uint32_t)a; a < b; a++, uv += duvx, dst++) {
                uint16_t c = *(const uint16_t *)((const uint8_t *)tx->data + (((uv >> 25) & wm2) | ((uv >> 3) & hm7)));
                if (!tx->alpha || !(c & 0x8000))
                    *dst = c;
            }
        }
    }
}
#else
void raster_tex(const Poly *p);                                                        /* raster.s */
#endif

void fill(uint32_t *dst, uint32_t word, int n);         /* span.s */
void irq_vblank(void);                                  /* span.s */
void snd_init(void);                                    /* sound.s */
void hud_init(void);                                    /* hud.s */
volatile uint32_t s_flip;                               /* DISPCNT to show at the next blank, or 0 */
volatile uint32_t s_vbl;                                /* blanks so far (span.s) */

#ifdef RASTER_C
/* the buckets, furthest first, each its list: the triangles drawn */
static int draw(void)
{
    int i, n = 0;
    for (i = NBUCKET - 1; i >= 0; i--) {
        uint16_t k = s_bucket[i];
        while (k) {
            if (s_poly[k - 1].tex != 0xFFFF)
                raster_tex(&s_poly[k - 1]);
            else
                raster(&s_poly[k - 1]);
            n++;
            k = s_poly[k - 1].next;
        }
    }
    return n;
}
#else
int draw(void);                                                                     /* raster.s */
#endif

static void clear(uint16_t col)
{
    fill((uint32_t *)s_back, col | (uint32_t)col << 16, SW * SH / 2);
}

int main(void)
{
    int page = 0, fi = 0, i;
    REG_WAITCNT = 0x4317;                            /* ROM 3/1 waitstates, prefetch on */
    for (i = 1; i < 4096; i++)
        s_rec[i] = udiv(1u << 24, (uint32_t)i);
    s_rec[0] = 1u << 24;
    for (i = 1; i < 256; i++)
        s_rsh[i] = (uint8_t)(s_rsh[i >> 1] + 1);
    REG_DISPCNT = 5 | 1 << 10 | 1 << 12 | 1 << 6;                       /* mode 5, BG2 */
    REG_BG2PA = (SW << 8) / 240;                     /* 160x128 stretched to 240x160 */
    REG_BG2PB = 0;
    REG_BG2PC = 0;
    REG_BG2PD = (SH << 8) / 160;
    REG_BG2X = 0;
    REG_BG2Y = 0;
    REG_TM2D = 0;
    REG_TM3D = 0;
    REG_TM3C = 0x84;                                 /* cascade: TM3 counts TM2's overflows */
    REG_TM2C = 0x80;                                 /* TM2 at the full 16.78 MHz */
    snd_init();
    hud_init();
    *(void (*volatile *)(void))0x03007FFC = irq_vblank;
    REG_DISPSTAT = 1 << 3;                           /* the vertical blank interrupt */
    REG_IE = 1;
    REG_IME = 1;
    for (;;) {
        const Frame *f;
        if (g_probe[0])
            fi = (int)g_probe[0] - 1;
        f = &g_frames[fi];
        uint32_t t0 = clock32(), t1, t2, tc;
        int ncells = 0, nverts = 0, ndrawn = 0;
        for (i = 0; i < NBUCKET; i++)
            s_bucket[i] = 0;
        s_npoly = 0;
        render_visible(f, &ncells, &nverts);
        while (g_probe[1])                           /* the host's: hold here, the lists built */
            ;
        t1 = clock32();
        while (s_flip)                               /* the last frame shown: its page is free */
            ;
        tc = clock32();
        s_back = (uint16_t *)(page ? 0x06000000 : 0x0600A000);
        clear(0x7E8C);                               /* the desert sky */
        ndrawn = draw();
        t2 = clock32();
        s_flip = (uint16_t)(5 | 1 << 10 | 1 << 12 | 1 << 6 | (page ? 0 : 1 << 4));   /* shown at the next blank */
        page ^= 1;
        t2 -= tc - t1;                               /* the cycles worked, not waited */
        g_stats[0] = (uint32_t)fi;
        g_stats[1] = t2 - t0;
        g_stats[2] = t1 - t0;
        g_stats[3] = t2 - t1;
        g_stats[4] = (uint32_t)ndrawn;
        g_stats[5] = (uint32_t)ncells;
        g_stats[6] = (uint32_t)nverts;
        g_stats[7] = tc - t1;                            /* waited for the flip */
        if (!s_lapped) {                                 /* over the race once: the worst, the sum, the count */
            if (t2 - t0 > g_stats[8]) {
                g_stats[8] = t2 - t0;
                g_stats[11] = (uint32_t)fi;
            }
            g_stats[9] += (t2 - t0) >> 4;
            g_stats[10]++;
            if (fi < 1200)
                g_cyc[fi] = t2 - t0;
        }
        if (!g_probe[2]) {                           /* real time: the camera at the game's 30 a second */
            static uint32_t lap;                     /* (no divide: the laps counted off) */
            uint32_t t = (s_vbl >> 1) - lap;
            while (t >= (uint32_t)g_nframes) {
                lap += (uint32_t)g_nframes;
                t -= (uint32_t)g_nframes;
            }
            fi = (int)t;
            continue;
        }
        if (++fi >= g_nframes) {
            fi = 0;
            s_lapped = 1;
        }
    }
}
