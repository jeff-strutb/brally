/* view.c: the behaviour flags and their profiles, and the race's lens on a
 * window of any shape (tgr_view.h). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "tgr_view.h"

enum { PROFILE_ORIGINAL, PROFILE_REMASTERED, PROFILE_COUNT };

static const struct {
    const char *name;                   /* TGR_FLAG_<name> overrides it */
    unsigned char value[PROFILE_COUNT]; /* original, remastered */
} k_flags[TGR_FLAG_COUNT] = {
    [TGR_FLAG_ANY_ASPECT] = { "ANY_ASPECT", { 0, 1 } },
};

static const char *const k_profiles[PROFILE_COUNT] = { "original", "remastered" };

static int s_profile = -1;
static volatile unsigned char s_value[TGR_FLAG_COUNT];

static void apply(void)
{
    char name[64];
    int i;
    for (i = 0; i < TGR_FLAG_COUNT; i++) {
        const char *e;
        snprintf(name, sizeof name, "TGR_FLAG_%s", k_flags[i].name);
        e = getenv(name);
        s_value[i] = e ? atoi(e) != 0 : k_flags[i].value[s_profile];
    }
}

static void load(void)
{
    const char *p = getenv("TGR_PROFILE");
    s_profile = p && strcmp(p, "original") == 0 ? PROFILE_ORIGINAL : PROFILE_REMASTERED;
    apply();
}

/* the window's shape follows on the host's thread (main.c) */
void tgr_profile_toggle(void)
{
    if (s_profile < 0)
        load();
    s_profile = s_profile == PROFILE_ORIGINAL ? PROFILE_REMASTERED : PROFILE_ORIGINAL;
    apply();                            /* a TGR_FLAG_<NAME> override still holds */
    fprintf(stderr, "profile: %s\n", k_profiles[s_profile]);
}

int tgr_flag(int flag)
{
    if (s_profile < 0)
        load();
    return flag >= 0 && flag < TGR_FLAG_COUNT ? s_value[flag] : 0;
}

const char *tgr_profile(void)
{
    if (s_profile < 0)
        load();
    return k_profiles[s_profile];
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

void tgr_view_race(int on) { s_race = on; }

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
