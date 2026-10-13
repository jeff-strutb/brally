/* light_game.c: what the lighting reads of the game's state (DESIGN.md
 * section 2.3), read-only.  Built with the core's flags (link64.sh), for its
 * types and globals; light.h is the plain interface the rest of the lighting
 * sees. */
#include <string.h>
#include "br_addr32.h"
#include "br_coretypes.h"
#include "br_mat.h"
#include "slice3_41.h"     /* BrDriver, BrDriverCar */
#include "br_cartypes.h"   /* BrRaceCtl */
#include "light.h"

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
