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
#define LIGHT_MOON_ELEV  11.0f    /* degrees: low enough to be seen (the camera's vertical half-angle is 15 degrees,
                                     from the game's projection: cot = 3.732); a full moon stands opposite
                                     the sun, so on Coastline it is ahead of the start, over the straight */

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

/* ---- the night's exposure ---------------------------------------------------------
 * At night the eye adapts to the scene's ambient light, not to the lamps:
 * the exposure puts an 18% grey surface under the moon and the moonlit sky
 * (Yamb: lux on level ground) at LIGHT_NIGHT_KEY on the display (linear,
 * before the tonemap); the lamps' pools are brought down by the local
 * exposure (light_local_exposure).  EV100 from the camera's exposure
 * 1 / (1.2 2^EV): EV = log2(L / (1.2 key)), L = 0.18 Yamb / pi. */
#define LIGHT_NIGHT_KEY   0.12f
#define LIGHT_LOCAL_EDGE0 1.5f
#define LIGHT_LOCAL_EDGE1 3.0f
#define LIGHT_LOCAL_KNEE 2.0f
#define LIGHT_NIGHT_LOCAL_K 0.85f   
static inline float light_night_ev(float Yamb, float key)
{
    return log2f(0.18f * Yamb / 3.14159265f / (1.2f * key));
}

/* ---- local exposure ---------------------------------------------------------------
 * A region brighter than the key (its local mean Yloc, pre-exposed: a blur
 * of the frame) is brought down by (Yloc / key)^-k: its contrast with the
 * rest of the frame compressed by k, the detail inside it kept (the same
 * factor over the whole region), as Unreal Engine 5's local exposure
 * ("highlight contrast" 1 - k).  Regions at or under the key unchanged. */
static inline float light_local_exposure(float Ypix, float Yloc, float key, float k)
{
    /* edge-aware: a pixel LIGHT_LOCAL_EDGE0..1 stops from its surroundings' mean
       is across an edge (the moonlit ground beside a pool): it is compressed by
       its own luminance, not the pool's (no dark ring); inside a region (its
       texture within a stop or so of the mean) by the region's mean */
    float d = fabsf(log2f(fmaxf(Ypix, 1e-9f) / fmaxf(Yloc, 1e-9f)));
    float t = fminf(fmaxf((d - LIGHT_LOCAL_EDGE0) / (LIGHT_LOCAL_EDGE1 - LIGHT_LOCAL_EDGE0), 0.0f), 1.0f);
    float Y = exp2f(log2f(fmaxf(Yloc, 1e-9f)) + (log2f(fmaxf(Ypix, 1e-9f)) - log2f(fmaxf(Yloc, 1e-9f))) * t * t * (3.0f - 2.0f * t));
    /* only well above the key (LIGHT_LOCAL_KNEE stops): the moonlit scene keeps its exposure */
    key *= exp2f(LIGHT_LOCAL_KNEE);
    return Y > key && k > 0.0f ? powf(Y / key, -k) : 1.0f;
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

/* The meter's centre weighting, as the post pass bins each sample: 1 at the
 * frame's edge, 4 at its centre (1 + 3 (1 - r^2), rounded, inside the
 * ellipse touching the frame's edges; uv in 0..1). */
static inline unsigned light_meter_weight(float u, float v)
{
    float qx = (u - 0.5f) * 2.0f, qy = (v - 0.5f) * 2.0f, k = 1.0f - (qx * qx + qy * qy);
    k = fminf(fmaxf(k, 0.0f), 1.0f);
    return 1u + (unsigned)(3.0f * k + 0.5f);
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

/* ---- which draws are lamplight -------------------------------------------------
 * In the Original game the lit windows at night are blended decals drawn
 * over the walls; the walls, rock and sand under them are opaque (Glide
 * blend ONE, ZERO).  So a draw's glow mask, and the track's lights made from
 * it (light_world_gather), come only from a blended draw: an opaque texture
 * never glows, however warm and bright its texels (a sandy rock face). */
static inline int light_draw_opaque(int src, int dst) { return src == 4 && dst == 0; }
static inline int light_draw_may_glow(int src, int dst) { return !light_draw_opaque(src, dst); }

/* ---- the lighthouse beam's polygons ------------------------------------------------
 * The game draws each lighthouse's beam as a cone fanned from its instance's
 * position (Coastline's 64 and 142: every one of the 10 triangles has a
 * vertex within 0.47 m of it and one 77 m out).  A draw whose every
 * triangle touches a beam instance's position (under 1 m) and reaches past
 * 20 m from it is that beam: emitted light, not a lit surface. */
#define LIGHT_BEAM_APEX  1.0f
#define LIGHT_BEAM_REACH 20.0f
/* the beam polygons' weight when the volumetric fog carries the beam (the
 * lighthouse spot's in-scatter, brr_metal_vol.h) */
#define LIGHT_BEAM_VOLW  0.25f
static inline int light_beam_draw(const float *xyz, int ntri, const float pos[][3], int nb)
{
    int b, t, k;
    if (ntri <= 0)
        return 0;
    for (b = 0; b < nb; b++) {
        int all = 1;
        for (t = 0; t < ntri && all; t++) {
            float dmin = 1e30f, dmax = 0.0f;
            for (k = 0; k < 3; k++) {
                const float *p = &xyz[9 * t + 3 * k];
                float dx = p[0] - pos[b][0], dy = p[1] - pos[b][1], dz = p[2] - pos[b][2];
                float d = sqrtf(dx * dx + dy * dy + dz * dz);
                dmin = fminf(dmin, d);
                dmax = fmaxf(dmax, d);
            }
            all = dmin < LIGHT_BEAM_APEX && dmax > LIGHT_BEAM_REACH;
        }
        if (all)
            return 1;
    }
    return 0;
}

/* ---- the eye's adaptation ----------------------------------------------------------
 * The exposure follows the meter's EV as the eye adapts: toward a brighter
 * scene with a time constant of LIGHT_ADAPT_UP seconds, toward a darker one
 * LIGHT_ADAPT_DOWN (the eye's light adaptation is quicker than its dark).
 * An unset EV (ev > 1e8) takes the first valid reading whole; a change of
 * LIGHT_ADAPT_SNAP stops or more (a camera cut, the race starting) too.  A
 * reading with no samples (valid 0: a frame with no lit surface) leaves it
 * as it is.  dt: seconds since the last reading, kept to 0..0.25. */
#define LIGHT_ADAPT_UP    1.0f
#define LIGHT_ADAPT_DOWN  3.0f
#define LIGHT_ADAPT_SNAP  3.0f
static inline float light_adapt_ev(float ev, float target, float dt, int valid)
{
    float tau;
    if (!valid)
        return ev;
    if (ev > 1e8f || fabsf(target - ev) >= LIGHT_ADAPT_SNAP)
        return target;
    dt = fminf(fmaxf(dt, 0.0f), 0.25f);
    tau = target > ev ? LIGHT_ADAPT_UP : LIGHT_ADAPT_DOWN;
    return ev + (target - ev) * (1.0f - expf(-dt / tau));
}

/* ---- the moon's disc and the stars -----------------------------------------------
 * The disc: LIGHT_MOON_R radians (0.26 degrees), limb darkened by the linear
 * law I(mu) = I0 (1 - u (1 - mu)), u = LIGHT_MOON_LIMB (the full moon is
 * nearly flat: a mild darkening), its mean luminance (1 - u/3) I0.  I0 so
 * the disc integrates to the illuminance E reaching the ground facing it:
 * I0 = E / (pi R^2 (1 - u/3)); at Coastline's 42 degrees (0.213 lux) the mean
 * is 3,290 cd/m2 (the published full moon's 2,500 to 4,000).
 * The stars: one at most to a cell of a LIGHT_STAR_N x LIGHT_STAR_N grid on
 * each face of the direction's cube, present with probability LIGHT_STAR_P;
 * its magnitude m = M1 + 2.5 log10(h) for a hash h uniform in (0,1], so the
 * count brighter than m grows as 10^(0.4 m) (faint stars the most); its illuminance
 * 10^(-0.4 (m + 13.99)) lux (a magnitude 0 star 2.54e-6 lux), spread over
 * its cell. */
#define LIGHT_MOON_R      (0.26f * 0.017453293f)
#define LIGHT_MOON_LIMB   0.3f
#define LIGHT_STAR_N      512
#define LIGHT_STAR_P      0.012f
#define LIGHT_STAR_M1     6.5f
static inline float light_moon_I0(float E)
{
    return E / (3.14159265f * LIGHT_MOON_R * LIGHT_MOON_R * (1.0f - LIGHT_MOON_LIMB / 3.0f));
}
static inline float light_moon_disc(float E, float r)   /* r: angle from the centre over the radius */
{
    float mu;
    if (r >= 1.0f)
        return 0.0f;
    mu = sqrtf(1.0f - r * r);
    return light_moon_I0(E) * (1.0f - LIGHT_MOON_LIMB * (1.0f - mu));
}
static inline float light_star_E(float h)   /* lux, above the atmosphere, for the star's hash h in (0,1] */
{
    float m = LIGHT_STAR_M1 + 2.5f * log10f(fmaxf(h, 1e-3f));
    return powf(10.0f, -0.4f * (m + 13.99f));
}

/* ---- the Purkinje tint ------------------------------------------------------------
 * The colour the rods' share carries, of luminance 1: a grey-blue (x 0.286,
 * y 0.297, against D65's 0.313 0.329; it was 0.248 0.251, a saturated blue
 * whose blue channel was 2.9 times its red).  A moonlit road (0.016 cd/m2, a
 * rods' share of 0.72) reads grey-blue: blue 1.36 times red (2.21 before). */
#define LIGHT_PURK_BLUE_R 0.86f
#define LIGHT_PURK_BLUE_G 0.98f
#define LIGHT_PURK_BLUE_B 1.30f

#endif
