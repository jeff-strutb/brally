/* view.c: Remastered's switches in Top Gear Rally, and the race's lens on a
 * window of any shape (tgr_view.h). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "tgr_view.h"
#include "plat.h"

static const struct {
    const char *name;                   /* TGR_FLAG_<name>=0|1 sets it at start */
    unsigned char on;                   /* until the app sets it (tgr_flag_set) */
} k_flags[TGR_FLAG_COUNT] = {
    [TGR_FLAG_ANY_ASPECT] = { "ANY_ASPECT", 1 },
    [TGR_FLAG_BR_RACES] = { "BR_RACES", 1 },
};

static int s_loaded;
static volatile unsigned char s_value[TGR_FLAG_COUNT];

static void load(void)
{
    char name[64];
    int i;
    for (i = 0; i < TGR_FLAG_COUNT; i++) {
        const char *e;
        snprintf(name, sizeof name, "TGR_FLAG_%s", k_flags[i].name);
        e = getenv(name);
        s_value[i] = e ? atoi(e) != 0 : k_flags[i].on;
    }
    s_loaded = 1;
}

void tgr_flag_set(int flag, int on)
{
    if (!s_loaded)
        load();
    if (flag >= 0 && flag < TGR_FLAG_COUNT)
        s_value[flag] = on != 0;
}

int tgr_flag(int flag)
{
    if (!s_loaded)
        load();
    return flag >= 0 && flag < TGR_FLAG_COUNT ? s_value[flag] : 0;
}

/* ---- the race's lens ------------------------------------------------------ */
void tgr_view_scale(float *kx, float *ky)
{
    int w = 0, h = 0;
    double a, g = 4.0 / 3.0;
    *kx = *ky = 1.0f;
    if (!tgr_flag(TGR_FLAG_ANY_ASPECT))
        return;
    host_window_pixels(&w, &h);
    if (w <= 0 || h <= 0) {
        const char *e = getenv("TGR_WINDOW");
        if (!e || sscanf(e, "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0)
            return;
    }
    a = (double)w / h;
    if (fabs(a / g - 1.0) < 0.004)      /* 4:3 to the pixel: nothing to widen */
        return;
    *kx = (float)fmax(1.0, a / g);
    *ky = (float)fmax(1.0, g / a);
}

static float s_kx = 1, s_ky = 1;        /* the scale the last lens was made for */
static int s_race;
static volatile uint32_t s_race_at;     /* the retrace the race's views were last drawn at, + 1 */

uint32_t tgr_frame(void);
extern int D_8026FF10;                  /* the race is paused */

void tgr_view_race(int on)
{
    s_race = on;
    if (on)
        s_race_at = tgr_frame() + 1;
}

int tgr_view_driving(void)
{
    uint32_t at = s_race_at;
    return at && tgr_frame() + 1 - at < 8 && !D_8026FF10;
}

void tgr_view_lens(float *fovy, float *aspect, int mirror)
{
    s_kx = s_ky = 1.0f;
    if (s_race)
        tgr_view_scale(&s_kx, &s_ky);
    if (mirror || (s_kx <= 1.0f && s_ky <= 1.0f))
        return;
    /* the view is stretched over the window by kx across and ky down: the
     * lens sees that much more (Hor+ across, Vert+ down) */
    if (s_ky > 1.0f)
        *fovy = (float)(2.0 * atan(s_ky * tan(*fovy * M_PI / 360.0)) * 180.0 / M_PI);
    *aspect = *aspect * s_kx / s_ky;
}

void tgr_view_wedge(float *half_w, float *half_h, int mirror)
{
    float kx, ky;
    if (mirror || !s_race)
        return;
    tgr_view_scale(&kx, &ky);
    *half_w *= kx;
    *half_h *= ky;
}

/* The projections the camera made lately, known by their address and their
 * contents (the matrix pool is a ring: an address comes round again holding
 * another camera's matrix, the HUD's) */
#define NPROJ 16
static struct { uint32_t hash; const void *mtx; int kind; float kx, ky; } s_proj[NPROJ];
static int s_nproj;

static uint32_t mtx_hash(const void *mtx)
{
    const uint8_t *p = (const uint8_t *)mtx;
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < 64; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

void tgr_view_proj(const void *mtx, int mirror)
{
    int i = s_nproj++ % NPROJ;
    if (s_kx <= 1.0f && s_ky <= 1.0f) {
        s_proj[i].mtx = NULL;
        return;
    }
    s_proj[i].mtx = mtx;
    s_proj[i].hash = mtx_hash(mtx);
    s_proj[i].kind = mirror ? 2 : 1;
    s_proj[i].kx = s_kx;
    s_proj[i].ky = s_ky;
}

/* the menus' screen camera (frameloop.c, BrScreenCameraSet) on a window of another
 * shape than 4:3, outside a race: the lens sees as much more as the window shows
 * (Hor+ across, Vert+ down, as a race's), and the projection is known as the menus'
 * (kind 3) so what it draws is spread over the window (rcp.c, place_menu): a carousel
 * item keeps its size at the centre and slides in and out from the window's edges */
static float s_mkx = 1, s_mky = 1;

void tgr_view_menu_lens(float *fovy, float *aspect)
{
    s_mkx = s_mky = 1.0f;
    if (s_race)
        return;
    tgr_view_scale(&s_mkx, &s_mky);
    if (s_mkx <= 1.0f && s_mky <= 1.0f)
        return;
    if (s_mky > 1.0f)
        *fovy = (float)(2.0 * atan(s_mky * tan(*fovy * M_PI / 360.0)) * 180.0 / M_PI);
    *aspect = *aspect * s_mkx / s_mky;
}

void tgr_view_proj_menu(const void *mtx)
{
    int i = s_nproj++ % NPROJ;
    if (s_mkx <= 1.0f && s_mky <= 1.0f) {
        s_proj[i].mtx = NULL;
        return;
    }
    s_proj[i].mtx = mtx;
    s_proj[i].hash = mtx_hash(mtx);
    s_proj[i].kind = 3;
    s_proj[i].kx = s_mkx;
    s_proj[i].ky = s_mky;
}

int tgr_view_proj_kind(const void *mtx, float *kx, float *ky)
{
    uint32_t h = 0;
    int i, k;
    for (k = 0; k < NPROJ; k++) {
        i = (s_nproj - 1 - k + NPROJ * 2) % NPROJ;   /* the latest first */
        if (s_proj[i].mtx != mtx)
            continue;
        if (!h)
            h = mtx_hash(mtx);
        if (s_proj[i].hash == h) {
            *kx = s_proj[i].kx;
            *ky = s_proj[i].ky;
            return s_proj[i].kind;
        }
    }
    return 0;
}

/* a check's shots counted from now: TGR_MENUS_SHOTS=N1,N2,... after Boss Rally's
   menus hand the app back (src/racing/handoff.c), whose wait the headless
   retraces run through unpaced; the shots go to --shots, named by retrace, and
   the run ends after the last */
void tgr_shots_from_now(const char *offsets)
{
    static char list[512];
    uint32_t now = tgr_frame(), last = now;
    int n = 0;
    const char *p = offsets;
    list[0] = 0;
    while (p && *p && n < (int)sizeof list - 16) {
        last = now + (uint32_t)strtoul(p, NULL, 10);
        n += snprintf(list + n, sizeof list - (size_t)n, "%s%u", n ? "," : "", last);
        p = strchr(p, ',');
        if (p)
            p++;
    }
    g_tgr.shot_at = list;
    g_tgr.frames = (int)last + 2;
    fprintf(stderr, "tgr: shots at %s\n", list);
}
