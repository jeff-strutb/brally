/* light_night.h: the night's light and the eye's response to it, as plain C
 * (no state of the game's): the moon, the meter's band and compensation, the
 * Purkinje shift (the post shader's copy is brr_metal_night.h) and which
 * textures may glow.  Header-only, so the renderer and the tests share one
 * copy.  Numbers: MEASUREMENTS.md, "Night: the moon, the eye, the glow". */
#ifndef BR_LIGHT_NIGHT_H
#define BR_LIGHT_NIGHT_H

#include <math.h>
#include <stdint.h>
#include <string.h>

/* ---- the moon ------------------------------------------------------------------
 * A full moon at mean distance is magnitude -12.7: 0.27 lux on a surface
 * facing it above the atmosphere (Krisciunas and Schaefer 1991).  The air
 * takes 0.172 magnitudes an air mass in V (their k_V for a good site); the
 * air mass at elevation h is Kasten and Young's (1989).  light_moon_normal is
 * the illuminance on a surface facing the moon, light_moon_ground on level
 * ground (times sin h): about 0.16 lux at 45 degrees, 0.21 overhead, the
 * published 0.1 to 0.3 lux of a full moon high in the sky. */
#define LIGHT_MOON_TOA   0.27f    /* lux, normal, above the atmosphere */
#define LIGHT_MOON_KV    0.172f   /* magnitudes an air mass */
#define LIGHT_MOON_ELEV  42.0f    /* degrees: Coastline's moon, high (the project lead sets each track's later) */

static inline float light_airmass(float elev_deg)
{
    float h = elev_deg < 0.0f ? 0.0f : elev_deg;
    return 1.0f / (sinf(h * 0.017453293f) + 0.50572f * powf(h + 6.07995f, -1.6364f));
}

static inline float light_moon_normal(float elev_deg)
{
    return LIGHT_MOON_TOA * powf(10.0f, -0.4f * LIGHT_MOON_KV * light_airmass(elev_deg));
}

static inline float light_moon_ground(float elev_deg)
{
    return light_moon_normal(elev_deg) * sinf(elev_deg * 0.017453293f);
}

/* ---- the meter -----------------------------------------------------------------
 * The histogram (16 bins a stop, bin i holding log2 Y in [i/16 - 10, (i+1)/16
 * - 10)): the mean log luminance of the band between percentiles plo and phi
 * as an EV100 (K = 12.5), less the compensation (stops brighter), kept to
 * [evlo, evhi].  No samples: the middle of the range. */
static inline float light_meter_ev(const uint32_t *c, int nbins, float plo, float phi,
                                   float evlo, float evhi, float comp)
{
    unsigned ns = 0, lo, hi, run = 0;
    float mlog = 0.0f, wsum = 0.0f, t;
    int i;
    for (i = 0; i < nbins; i++)
        ns += c[i];
    lo = (unsigned)(ns * plo);
    hi = (unsigned)(ns * phi);
    for (i = 0; i < nbins; i++) {
        unsigned a0 = run, a1 = run + c[i], b0 = a0 > lo ? a0 : lo, b1 = a1 < hi ? a1 : hi;
        float in = b1 > b0 ? (float)(b1 - b0) : 0.0f;
        mlog += in * ((i + 0.5f) / 16.0f - 10.0f);
        wsum += in;
        run = a1;
    }
    t = (evlo + evhi) * 0.5f;
    if (wsum > 0)
        t = log2f(exp2f(mlog / wsum) * 100.0f / 12.5f) - comp;
    return fminf(fmaxf(t, evlo), evhi);
}

/* ---- the Purkinje shift ----------------------------------------------------------
 * The eye's rods take over below the cones' range: colour fades and what is
 * left looks blue (Jensen, Premoze, Shirley et al. 2000, "A Physically-Based
 * Night Sky Model"; Kirk and O'Brien 2011).  The rods' share at luminance L
 * (cd/m2): 0 in photopic light (3 cd/m2 and up), 1 when scotopic (0.001 and
 * under), between them a smoothstep in log L (mesopic).  The blend: toward
 * the scotopic luminance V = Y (1.33 (1 + (Y + Z) / X) - 1.68) (Larson et al.
 * 1997, as Jensen 2000 uses it), carried by a blue of luminance 1. */
#define LIGHT_PURK_LO   0.001f
#define LIGHT_PURK_HI   3.0f
static inline float light_purkinje_w(float L)
{
    float t = (log10f(fmaxf(L, 1e-9f)) - log10f(LIGHT_PURK_LO)) / (log10f(LIGHT_PURK_HI) - log10f(LIGHT_PURK_LO));
    t = fminf(fmaxf(t, 0.0f), 1.0f);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

/* ---- which textures glow ------------------------------------------------------
 * A texture's lamplight (its emissive mask, brr_metal.m lit_emask) glows only
 * where the track's scene draws it: a 3D draw whose triangle lies on the
 * track (light_capture_ontrack) marks its texture the track's; one that does
 * not (a car, what the race adds) marks it never to glow, for as long as the
 * texture lives.  The cars' lamps are light_scene.c's lights, not their
 * paint. */
#define LIGHT_GLOW_TEX 4096
enum { GLOW_UNSEEN = 0, GLOW_TRACK = 1, GLOW_NEVER = 2 };
typedef struct { uint8_t f[LIGHT_GLOW_TEX]; } LightGlow;
static inline void light_glow_new(LightGlow *g, uint32_t tid)
{
    if (tid < LIGHT_GLOW_TEX)
        g->f[tid] = GLOW_UNSEEN;
}
static inline void light_glow_drawn(LightGlow *g, uint32_t tid, int ontrack)
{
    if (tid >= LIGHT_GLOW_TEX)
        return;
    if (!ontrack)
        g->f[tid] = GLOW_NEVER;
    else if (g->f[tid] == GLOW_UNSEEN)
        g->f[tid] = GLOW_TRACK;
}
static inline int light_glow_on(const LightGlow *g, uint32_t tid)
{
    return tid < LIGHT_GLOW_TEX && g->f[tid] == GLOW_TRACK;
}

#endif
