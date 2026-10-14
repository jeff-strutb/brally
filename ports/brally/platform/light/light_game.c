/* light_game.c: what the lighting reads of the game's state (DESIGN.md
 * section 2.3), read-only.  Built with the core's flags (link64.sh), for its
 * types and globals; light.h is the plain interface the rest of the lighting
 * sees. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "br_addr32.h"
#include "br_coretypes.h"
#include "br_mat.h"
#include "slice3_41.h"     /* BrDriver, BrDriverCar */
#include "br_cartypes.h"   /* BrRaceCtl */
#include "br_trkhdr.h"   /* g_brTrkHdr */
#include "light.h"
#include "light_world.h"

/* the camera the display list draws through: its view x projection (the
 * "projection" slot BrGbiMatrix fills, row vectors) and the viewport that
 * takes clip space to the game's pixels: x = cx / cw * sx + ox, y the same
 * (br_dltrim.c, br_dlproj.c) */
void light_game_camera(LightCamera *c)
{
    memcpy(c->vp, &DAT_105ccd00, sizeof c->vp);
    c->sx = DAT_105ccd48;
    c->ox = DAT_105cd9f8;
    c->sy = DAT_105ccfdc;
    c->oy = DAT_105cd9fc;
}

int light_game_sky_env(void)
{
    static const int env[16] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 6, 6, 0 };
    return env[g_Br0B380C & 15];   /* g_brCfgChosenTrack, 0x100B3014 */
}

int light_game_weather(void)
{
    int w = *(int32_t *)&DAT_104b15e8;
    return w >= 0 && w <= 4 ? w : 0;
}

/* a car model's extent in its own frame (x forward, y the frame's second
 * row, z up; metres), from the vertices of its full-detail body list (model
 * record +0x8024): G_VTX (0x04) loads of 0x20-byte vertices whose first three
 * floats are the position x 255, followed into called lists (0x06) until
 * G_ENDDL (0xB8).  Measured once per model. */
typedef struct { const void *model; float lo[3], hi[3]; int ok; } ModelBox;
static ModelBox s_box[32];
static int s_nbox;

static void box_dl(uint32_t addr, ModelBox *b, int depth)
{
    const struct { uint32_t w0, w1; } *c;
    int k;
    if (!addr || depth > 6 || !(c = br_ptr32(addr)))
        return;
    for (k = 0; k < 4096; k++, c++) {
        unsigned op = c->w0 >> 24;
        if (op == 0xB8)
            return;
        if (op == 0x06) {
            box_dl(c->w1, b, depth + 1);
            if (c->w0 & 0x00010000u)
                return;
            continue;
        }
        if (op == 0x04) {
            int n = (int)((c->w0 >> 10) & 0x3F), v, a;
            const float *p = (const float *)br_ptr32(c->w1);
            for (v = 0; p && v < n; v++)
                for (a = 0; a < 3; a++) {
                    float x = p[v * 8 + a] / 255.0f;
                    if (!(x > -100 && x < 100))
                        continue;
                    if (!b->ok || x < b->lo[a]) b->lo[a] = x;
                    if (!b->ok || x > b->hi[a]) b->hi[a] = x;
                    if (a == 2) b->ok = 1;
                }
        }
    }
}

static const ModelBox *model_box(const void *model)
{
    int i;
    for (i = 0; i < s_nbox; i++)
        if (s_box[i].model == model)
            return &s_box[i];
    if (!model || s_nbox == 32)
        return NULL;
    memset(&s_box[s_nbox], 0, sizeof s_box[s_nbox]);
    s_box[s_nbox].model = model;
    box_dl(*(const uint32_t *)((const char *)model + 0x8024), &s_box[s_nbox], 0);
    return &s_box[s_nbox++];
}

int light_game_cars(LightCar *out, int max)
{
    int n = g_brRaceNDriver, i, k = 0;
    if (n > 20)
        n = 20;
    for (i = 0; i < n && k < max; i++) {
        const BrDriverCar *c = g_aBrRaceDriver[i].pCar;
        const ModelBox *b;
        LightCar *o = &out[k];
        if (!c)
            continue;
        memset(o, 0, sizeof *o);
        memcpy(o->fwd, &c->fwd, 12);
        memcpy(o->side, &c->right, 12);
        memcpy(o->up, &c->up, 12);
        memcpy(o->pos, &c->pos, 12);
        memcpy(o->vel, &c->f1024, 12);
        o->drive = c->fE68;
        o->brake = c->f0E6C;
        o->ctl = c->pCtl ? c->pCtl->ctl : 0;
        if ((b = model_box(c->pModel)) && b->ok) {
            memcpy(o->lo, b->lo, sizeof o->lo);
            memcpy(o->hi, b->hi, sizeof o->hi);
            o->has_box = 1;
        }
        k++;
    }
    return k;
}

/* ---- the track: every scene object's triangles in the world -----------------
 * The scene is the track's instance table (g_brTrkHdr.aInstances,
 * cInstances records of 0x54 bytes, br_scenedl.c): a 4x4 model matrix (row vectors, floats 0..15)
 * loaded with G_MTX, and at +0x44 the display list both the near and far
 * draws call (br_objdl.c); an unrotated object (+0x4D bit 0x20) has only
 * its scale (float 0) and position (floats 12..14).  Its
 * lists were fixed up at load (br_vtxcache.c): G_VTX (0x04) points at
 * 8-float vertices (x y z s t nx ny nz), its first slot is byte 1 of w0 / 2
 * and its count (w0 >> 10) & 0x3F; G_TRI1 (0xBF) and G_TRI2 (0xB1) hold
 * plain slot numbers.  The triangles here are world positions the same way
 * the capture's are (clip = world x the camera's view x projection). */

static struct {
    const void *recs;
    int count;
    LightTrack t;
    int cap, lists, unknown;
} s_trk;

static void trk_add(const float a[3], const float b[3], const float c[3], int obj)
{
    float *o;
    if (s_trk.t.ntri == s_trk.cap) {
        int n = s_trk.cap ? s_trk.cap * 2 : 65536;
        float *p = realloc(s_trk.t.tri, (size_t)n * 9 * sizeof *p);
        int *q = realloc(s_trk.t.obj, (size_t)n * sizeof *q);
        if (p) s_trk.t.tri = p;
        if (q) s_trk.t.obj = q;
        if (!p || !q)
            return;
        s_trk.cap = n;
    }
    o = &s_trk.t.tri[9 * s_trk.t.ntri];
    memcpy(o, a, 12);
    memcpy(o + 3, b, 12);
    memcpy(o + 6, c, 12);
    s_trk.t.obj[s_trk.t.ntri++] = obj;
}

static void trk_dl(uint32_t addr, const float *m, float (*vb)[3], int obj, int depth)
{
    const struct { uint32_t w0, w1; } *c;
    int k;
    if (!addr || depth > 8 || !(c = br_ptr32(addr)))
        return;
    s_trk.lists++;
    for (k = 0; k < 65536; k++, c++) {
        unsigned op = c->w0 >> 24;
        if (op == 0xB8)
            return;
        if (op == 0x06) {
            trk_dl(c->w1, m, vb, obj, depth + 1);
            if (c->w0 & 0x00010000u)
                return;
        } else if (op == 0x04) {
            int n = (int)((c->w0 >> 10) & 0x3F), v0 = (int)((c->w0 >> 16) & 0xFF) / 2, v;
            const float *p = (const float *)br_ptr32(c->w1);
            for (v = 0; p && v < n && v0 + v < 64; v++) {
                const float *q = &p[v * 8];
                int a;
                for (a = 0; a < 3; a++)
                    vb[v0 + v][a] = q[0] * m[a] + q[1] * m[4 + a] + q[2] * m[8 + a] + m[12 + a];
            }
        } else if (op == 0xBF) {
            trk_add(vb[(c->w1 >> 16) & 63], vb[(c->w1 >> 8) & 63], vb[c->w1 & 63], obj);
        } else if (op == 0xB1) {
            trk_add(vb[(c->w0 >> 16) & 63], vb[(c->w0 >> 8) & 63], vb[c->w0 & 63], obj);
            trk_add(vb[(c->w1 >> 16) & 63], vb[(c->w1 >> 8) & 63], vb[c->w1 & 63], obj);
        } else if (op == 0x01) {
            s_trk.unknown++;     /* a matrix inside an object's list: none expected */
        }
    }
}

/* the lighthouse beams: the instance records whose flag bytes are the beam's
 * (light_world.h light_beam_record; on Coastline 64 and 142 only), their
 * live matrices (BrRaceStepSpecials turns them in place) */
int light_game_beams(float pos[][3], float xaxis[][3], int max)
{
    const char *recs = BR_PTR32(const char *, g_brTrkHdr.aInstances);
    int count = g_brTrkHdr.cInstances, i, n = 0;
    if (!recs || count <= 0 || count > 65536)
        return 0;
    for (i = 0; i < count && n < max; i++) {
        const unsigned char *r = (const unsigned char *)recs + (size_t)i * 0x54;
        const float *f = (const float *)r;
        if (!light_beam_record(r[0x4C], r[0x4D]))
            continue;
        memcpy(pos[n], f + 12, 12);
        memcpy(xaxis[n], f, 12);
        n++;
    }
    return n;
}

const LightTrack *light_game_track(void)
{
    const char *recs = BR_PTR32(const char *, g_brTrkHdr.aInstances);
    int count = g_brTrkHdr.cInstances, i;
    if (!recs || count <= 0 || count > 65536)
        return NULL;
    if (recs == s_trk.recs && count == s_trk.count)
        return &s_trk.t;
    s_trk.recs = recs;
    s_trk.count = count;
    s_trk.t.ntri = 0;
    s_trk.t.nbill = 0;
    s_trk.lists = s_trk.unknown = 0;
    s_trk.t.gen++;
    for (i = 0; i < count; i++) {
        const char *r = recs + (size_t)i * 0x54;
        float vb[64][3];
        memset(vb, 0, sizeof vb);
        const float *f = (const float *)r;
        float m[16];
        if ((unsigned char)r[0x4D] & 0x20) {
            /* unrotated: the scene draw builds scale x the view's rotation
             * and the position through the view (br_scenedl.c), so its
             * model is scale x I moved to the position */
            memset(m, 0, sizeof m);
            m[0] = m[5] = m[10] = f[0];
            m[12] = f[12];
            m[13] = f[13];
            m[14] = f[14];
            m[15] = 1.0f;
            s_trk.t.nbill++;
        } else
            memcpy(m, f, sizeof m);
        trk_dl(*(const uint32_t *)(r + 0x44), m, vb, i, 0);
    }
    s_trk.t.nobj = count;
    {
        float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
        int t, a;
        for (t = 0; t < s_trk.t.ntri * 3; t++)
            for (a = 0; a < 3; a++) {
                lo[a] = fminf(lo[a], s_trk.t.tri[3 * t + a]);
                hi[a] = fmaxf(hi[a], s_trk.t.tri[3 * t + a]);
            }
        const char *e = getenv("BR_LIGHT_TRACKOBJ");   /* the track as a Wavefront OBJ, object number per face group */
        FILE *fo = e ? fopen(e, "w") : NULL;
        if (fo) {
            int last = -1, q;
            for (i = 0; i < count; i++) {   /* each record: flag bytes 0x4C..0x4F, then its 16 floats */
                const unsigned char *r = (const unsigned char *)recs + (size_t)i * 0x54;
                fprintf(fo, "# o%d flags %02x %02x %02x %02x dl %08x m", i, r[0x4C], r[0x4D], r[0x4E], r[0x4F], *(const uint32_t *)(r + 0x44));
                for (q = 0; q < 16; q++)
                    fprintf(fo, " %g", ((const float *)r)[q]);
                fprintf(fo, "\n");
            }
            for (t = 0; t < s_trk.t.ntri * 3; t++)
                fprintf(fo, "v %.4f %.4f %.4f\n", s_trk.t.tri[3 * t], s_trk.t.tri[3 * t + 1], s_trk.t.tri[3 * t + 2]);
            for (t = 0; t < s_trk.t.ntri; t++) {
                if (s_trk.t.obj[t] != last)
                    fprintf(fo, "g o%d\n", last = s_trk.t.obj[t]);
                fprintf(fo, "f %d %d %d\n", 3 * t + 1, 3 * t + 2, 3 * t + 3);
            }
            fclose(fo);
        }
        memcpy(s_trk.t.lo, lo, sizeof lo);
        memcpy(s_trk.t.hi, hi, sizeof hi);
        fprintf(stderr, "light: track %d objects (%d unrotated), %d lists, %d triangles, %d unexpected matrices,"
                        " x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f\n",
                count, s_trk.t.nbill, s_trk.lists, s_trk.t.ntri, s_trk.unknown, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
    }
    return &s_trk.t;
}
