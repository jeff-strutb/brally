/* light_world.c: the track's own lights at night (DESIGN.md 7.1): its glowing
 * surfaces (lit windows, street lamps; the glow mask, brr_metal.m lit_emask)
 * as point lights of the clustered set, and the lighthouse's turning beam.
 *
 * An emitter is a patch of glowing surface: its centre, normal, area (m2),
 * luminance (cd/m2: the glow's 100 times the share of its texels that glow)
 * and colour.  A Lambertian patch sends pi L A lumens into its half space;
 * the cluster's point light carries the same flux, I = sum(L A) / 4 cd in
 * every direction (4 pi I = pi sum(L A)), from the area-weighted centre moved
 * LIGHT_WORLD_OFF along the mean normal (in front of the wall, not inside
 * it).  Emitters cluster by a grid of LIGHT_WORLD_CELL metres and which way
 * they face; the brightest clusters are kept when there are more than the
 * set holds. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "light.h"
#include "light_world.h"

static int face_of(const float n[3])
{
    float ax = fabsf(n[0]), ay = fabsf(n[1]), az = fabsf(n[2]);
    if (ax >= ay && ax >= az) return n[0] >= 0 ? 0 : 1;
    if (ay >= az) return n[1] >= 0 ? 2 : 3;
    return n[2] >= 0 ? 4 : 5;
}

typedef struct { int key[4]; double la, p[3], n[3], rgb[3]; } Clu;

static int by_flux(const void *a, const void *b)
{
    double x = ((const Clu *)a)->la, y = ((const Clu *)b)->la;
    return x < y ? 1 : x > y ? -1 : 0;
}

int light_world_cluster(const LightEmitter *e, int ne, LightSpot *out, int max)
{
    Clu *c;
    int nc = 0, i, j, k;
    if (ne <= 0 || max <= 0 || !(c = calloc((size_t)ne, sizeof *c)))
        return 0;
    for (i = 0; i < ne; i++) {
        int key[4];
        double la = (double)e[i].lum * e[i].area;
        if (!(la > 0))
            continue;
        for (k = 0; k < 3; k++)
            key[k] = (int)floorf(e[i].p[k] / LIGHT_WORLD_CELL);
        key[3] = face_of(e[i].n);
        for (j = 0; j < nc && memcmp(c[j].key, key, sizeof key); j++)
            ;
        if (j == nc)
            memcpy(c[nc++].key, key, sizeof key);
        c[j].la += la;
        for (k = 0; k < 3; k++) {
            c[j].p[k] += la * e[i].p[k];
            c[j].n[k] += la * e[i].n[k];
            c[j].rgb[k] += la * e[i].rgb[k];
        }
    }
    qsort(c, (size_t)nc, sizeof *c, by_flux);
    for (j = 0; j < nc && j < max; j++) {
        LightSpot *o = &out[j];
        double nl = sqrt(c[j].n[0] * c[j].n[0] + c[j].n[1] * c[j].n[1] + c[j].n[2] * c[j].n[2]), y;
        memset(o, 0, sizeof *o);
        for (k = 0; k < 3; k++)
            o->pos[k] = (float)(c[j].p[k] / c[j].la + (nl > 0 ? c[j].n[k] / nl : 0) * LIGHT_WORLD_OFF);
        o->kind = LIGHT_POINT;
        o->axis[2] = 1.0f;
        o->side[0] = 1.0f;
        o->candela = (float)(c[j].la / 4.0);
        /* where it falls to 0.001 lux (starlight's level), at most 80 m */
        o->range = fminf(sqrtf(o->candela / 0.001f), 80.0f);
        y = 0.2126 * c[j].rgb[0] + 0.7152 * c[j].rgb[1] + 0.0722 * c[j].rgb[2];
        for (k = 0; k < 3; k++)
            o->rgb[k] = y > 0 ? (float)(c[j].rgb[k] / y) : 1.0f;
        o->cosout = 0.0f;
    }
    free(c);
    return j;
}

float light_world_flux(const LightSpot *s, int n)
{
    double f = 0;
    int i;
    for (i = 0; i < n; i++)
        f += s[i].kind == LIGHT_POINT ? 4.0 * M_PI * s[i].candela : 0.0;
    return (float)f;
}

/* the lighthouse: a spot turning about the lantern's vertical, one turn a
 * period (Coastline's: 1.25 degrees a race step, the beam instance's
 * special, remaster-lighthouse-2026-10-01.md), its optic about 300,000 cd (a
 * small coastal light's fourth-order lens), a narrow 3 degree beam (cosine
 * of the inner 1.5, outer 3 degrees), reaching 2 km.  t: seconds; deg_s: its
 * turn in degrees a second. */
void light_world_lighthouse(LightSpot *o, const float pos[3], float t, float deg_s)
{
    float a = t * deg_s * 0.017453293f;
    memset(o, 0, sizeof *o);
    memcpy(o->pos, pos, 12);
    o->kind = LIGHT_SPOT;
    o->axis[0] = cosf(a);
    o->axis[1] = sinf(a);
    o->axis[2] = -0.02f;     /* a hair under the horizon: the sea and the coast */
    {
        float l = sqrtf(o->axis[0] * o->axis[0] + o->axis[1] * o->axis[1] + o->axis[2] * o->axis[2]);
        o->axis[0] /= l; o->axis[1] /= l; o->axis[2] /= l;
    }
    o->side[0] = 0.99965732f;  /* cos 1.5 degrees: the inner cone (light.h LIGHT_SPOT) */
    o->candela = 300000.0f;
    o->range = 2000.0f;
    o->cosout = 0.99862953f;   /* cos 3 degrees */
    o->rgb[0] = 1.0f; o->rgb[1] = 0.93f; o->rgb[2] = 0.80f;
    {
        float y = 0.2126f * o->rgb[0] + 0.7152f * o->rgb[1] + 0.0722f * o->rgb[2];
        o->rgb[0] /= y; o->rgb[1] /= y; o->rgb[2] /= y;
    }
}

/* the set the renderer gathers (brr_metal.m: the glowing draws on the track) */
static LightSpot s_set[LIGHT_WORLD_MAX];
static int s_n;
static float s_lh[3];
static int s_has_lh;

void light_world_set(const LightSpot *s, int n)
{
    s_n = n < LIGHT_WORLD_MAX ? n : LIGHT_WORLD_MAX;
    memcpy(s_set, s, (size_t)s_n * sizeof *s);
}

void light_world_set_lighthouse(const float *pos)
{
    s_has_lh = pos != NULL;
    if (pos)
        memcpy(s_lh, pos, 12);
}

int light_world_spots(LightSpot *out, int max, float t)
{
    int n = s_n < max ? s_n : max;
    memcpy(out, s_set, (size_t)n * sizeof *out);
    if (s_has_lh && n < max)
        light_world_lighthouse(&out[n++], s_lh, t, LIGHT_LH_DEG_S);
    return n;
}

/* ---- gathering the track's glow ---------------------------------------------------
 * The renderer hands over each glowing draw of the track's scene (brr_metal.m:
 * its world triangles, the share of its texture's texels that glow and their
 * mean colour).  Each triangle becomes an emitter once a track generation
 * (its centroid to 5 cm is its identity), facing the camera that saw it (a
 * lit window is seen from outside), its luminance the glow's 100 cd/m2 times
 * the share.  The set is clustered again when it has grown, at most every
 * 60th call (light_world_refresh). */
#define GATHER_MAX 16384
static LightEmitter s_em[GATHER_MAX];
static uint64_t s_ekey[2 * GATHER_MAX];
static int s_ne, s_dirty;
static unsigned s_egen;

static int seen(uint64_t k)
{
    unsigned h = (unsigned)((k * 0x9E3779B97F4A7C15ull) >> 49) & (2 * GATHER_MAX - 1);
    while (s_ekey[h]) {
        if (s_ekey[h] == k)
            return 1;
        h = (h + 1) & (2 * GATHER_MAX - 1);
    }
    s_ekey[h] = k;
    return 0;
}

void light_world_gather(unsigned gen, const float *tri, int n, float share, const float rgb[3], const float eye[3])
{
    int i, k;
    if (gen != s_egen) {
        s_egen = gen;
        s_ne = 0;
        memset(s_ekey, 0, sizeof s_ekey);
        s_dirty = 1;
    }
    if (!(share > 0))
        return;
    for (i = 0; i < n && s_ne < GATHER_MAX; i++) {
        const float *a = tri + 9 * i, *b = a + 3, *c = a + 6;
        float e1[3], e2[3], nn[3], m[3], l, d = 0;
        uint64_t key;
        LightEmitter *o;
        for (k = 0; k < 3; k++) {
            e1[k] = b[k] - a[k];
            e2[k] = c[k] - a[k];
            m[k] = (a[k] + b[k] + c[k]) / 3.0f;
        }
        nn[0] = e1[1] * e2[2] - e1[2] * e2[1];
        nn[1] = e1[2] * e2[0] - e1[0] * e2[2];
        nn[2] = e1[0] * e2[1] - e1[1] * e2[0];
        l = sqrtf(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
        if (!(l > 1e-6f))
            continue;
        key = ((uint64_t)(uint32_t)(int32_t)lrintf(m[0] * 20.0f) << 42) ^ ((uint64_t)(uint32_t)(int32_t)lrintf(m[1] * 20.0f) << 21)
              ^ (uint64_t)(uint32_t)(int32_t)lrintf(m[2] * 20.0f) ^ 1ull;
        if (seen(key))
            continue;
        o = &s_em[s_ne++];
        for (k = 0; k < 3; k++) {
            nn[k] /= l;
            d += nn[k] * (eye[k] - m[k]);
        }
        for (k = 0; k < 3; k++) {
            o->p[k] = m[k];
            o->n[k] = d < 0 ? -nn[k] : nn[k];
            o->rgb[k] = rgb[k];
        }
        o->area = 0.5f * l;
        o->lum = 100.0f * share;
        s_dirty = 1;
    }
}

int light_world_refresh(void)
{
    static LightSpot s[LIGHT_WORLD_MAX];
    static int calls;
    int n;
    calls++;
    if (!s_dirty || (calls % 60 && s_n))
        return s_n;
    n = light_world_cluster(s_em, s_ne, s, LIGHT_WORLD_MAX);
    light_world_set(s, n);
    s_dirty = 0;
    return n;
}

int light_world_emitters(void)
{
    return s_ne;
}

/* a lighthouse beam as the race turns it: its instance's position and its
 * model's x axis (the beam; the special turns it about the model's y, which
 * is the world's up on Coastline) */
void light_world_beam(LightSpot *o, const float pos[3], const float xaxis[3])
{
    light_world_lighthouse(o, pos, atan2f(xaxis[1], xaxis[0]) / 0.017453293f, 1.0f);
}
