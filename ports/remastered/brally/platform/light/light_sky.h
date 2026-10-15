/* light_sky.h: the sky as a captured HDR map (the Forza Horizon model: a
 * photographed sky dome is both the sky drawn and the light the scene gets
 * from it).  Plain C, no GPU: the renderer uploads what this makes, and
 * ports/brally/tools/lighttest checks it against brute-force integration.
 *
 * A skymap is an equirectangular Radiance .hdr (row 0 the zenith, the
 * bottom row the nadir).  Loading it:
 *
 *   0. with opt.horizon, rebuilds the sky under that elevation, where the
 *      photographer's trees, buildings and hills stand: each column takes
 *      the sky just over the line (a degree's band, averaged over two
 *      degrees of bearing), blended toward that band's mean round the
 *      whole horizon as it nears the horizon (an overcast's horizon
 *      haze), so the track's own backdrop is the land a map shows;
 *   1. finds the sun: the disc of texels far above the sky around the
 *      brightest one, within LIGHT_SKY_SUN_SEARCH degrees of it.  Its light
 *      over the surrounding sky's, integrated over the texels' solid angles,
 *      is the sun's illuminance facing it; its mean direction the sun's.  The
 *      disc is then painted over with the sky around it, so the sun's light
 *      is counted once: as the directional light, which casts shadows;
 *   2. scales the map to physical units: the sun to opt.sun_lux facing it
 *      (a clear day's 100,000 lux), or with no sun in the map the light on
 *      level ground to opt.horiz_lux; with opt.total_lux, the sun's and the
 *      sky's light on level ground together to it (an overcast map, its
 *      sun only what shows through the cloud);
 *   3. replaces the lower hemisphere (the photographer's own ground) with a
 *      ground of albedo opt.ground_albedo lit by the sun and the sky, a
 *      degree of blend at the horizon: the track is the ground here;
 *   1b. with opt.deglare, takes off the photograph's lens glare about the
 *      sun: per ring about it (0.25 degrees wide to 5, then 1), the
 *      ring's median sky less the background's (the sky 45 to 55 degrees
 *      off), a smooth radial profile subtracted from the texels (glare adds
 *      to what is behind it), so the sky's own structure (stars, cloud)
 *      stays.  A night map's moon carries a
 *      halo 165 x the sky at a degree, 9 x at 8, which drawn as sky lit the
 *      night like a flashlight;
 *   4. turns the map about the vertical so its sun's bearing is opt.sun_az
 *      (the track's designed shadows), when given;
 *   5. projects the result on the first nine spherical harmonics and
 *      convolves them with the cosine (Ramamoorthi and Hanrahan 2001): the
 *      sky's irradiance on a surface of any normal, in nine coefficients.
 *
 * Maps over LIGHT_SKY_ANALYSIS texels wide are measured (the sky's light,
 * the harmonics) on a box-filtered copy that wide, which keeps every
 * integral; the sun is found and painted out at full resolution.
 *
 * Directions are world space, Z up.  A map texel (x, y) of a w x h map looks
 * toward elevation pi/2 - pi (y + 0.5) / h and bearing 2 pi (x + 0.5) / w + rot
 * from +X toward +Y. */
#ifndef BR_LIGHT_SKY_H
#define BR_LIGHT_SKY_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIGHT_SKY_SUN_SEARCH 1.5f     /* degrees about the brightest texel the disc may reach */
#define LIGHT_SKY_SUN_RING0  2.0f     /* the sky around the disc: an annulus 2 ... */
#define LIGHT_SKY_SUN_RING1  4.0f     /* ... to 4 degrees from the sun */
#define LIGHT_SKY_SUN_RATIO  30.0f    /* a disc texel is over 30 x the ring's median */
#define LIGHT_SKY_SUN_RADIUS 0.2665f  /* the sun's angular radius, degrees (drawn disc) */
#define LIGHT_SKY_ANALYSIS 2048       /* texels across the copy the light is measured on */
#define LIGHT_SKY_GLARE_OUT 45.0f     /* degrees: the glare's reach; the sky past it (to +10) its background */

typedef struct LightSkyOpt {
    float sun_lux;           /* the sun's illuminance facing it, lux (0: 100,000) */
    float horiz_lux;         /* a map with no sun: the light on level ground (0: 15,000) */
    float total_lux;         /* when set: the map scaled so the sun's and the sky's light on level
                                ground together is this (overcast, fog: the sun only what shows through) */
    float ground_albedo;     /* the ground under the horizon (0: 0.2) */
    float sun_az;            /* the bearing to turn the sun to, radians from +X toward +Y */
    int has_sun_az;
    float horizon;           /* degrees: the photographer's land reaches this high over the horizon;
                                under it the sky is rebuilt (0: a puresky map, nothing to rebuild) */
    int deglare;             /* take out the glare the camera's lens laid about the sun (a night
                                map's moon): the sky's median, ring by ring about it, less the
                                sky's own beyond LIGHT_SKY_GLARE_OUT */
} LightSkyOpt;

typedef struct LightSky {
    int w, h;
    float *rgb;              /* w x h x 3: radiance, cd/m2, the sun painted out, the ground below the horizon */
    float rot;               /* radians added to every texel's bearing */
    int has_sun;
    float sun[3];            /* toward the sun, unit */
    float sun_lux[3];        /* its illuminance on a face toward it, lux, per channel */
    float sun_omega;         /* the solid angle the disc was found over, sr */
    float sky_up[3];         /* the sky's illuminance on level ground (no sun), lux */
    float ground[3];         /* the ground's radiance, cd/m2 */
    float sh[9][3];          /* irradiance: E(n) = sum sh[i] Y_i(n), lux (sun excluded) */
    float scale;             /* what the file's values were multiplied by */
    float clip_ratio;        /* the sun's facing illuminance over the sky's on level ground, before scaling */
} LightSky;

/* 0 on success; on failure a message in err (when given) */
int  light_sky_load(LightSky *s, const char *path, const LightSkyOpt *opt, char *err, int nerr);
/* the same from radiance already in memory (w x h x 3 floats, copied) */
int  light_sky_make(LightSky *s, const float *rgb, int w, int h, const LightSkyOpt *opt, char *err, int nerr);
void light_sky_free(LightSky *s);

/* a texel's direction, and the texel a direction falls in */
void light_sky_dir(const LightSky *s, float x, float y, float d[3]);
/* the first nine real spherical harmonics at unit d */
void light_sky_sh9(const float d[3], float y[9]);
/* the sky's irradiance on a face of unit normal n, lux (sun excluded) */
void light_sky_irr(const LightSky *s, const float n[3], float e[3]);

/* a Radiance RGBE file into w x h x 3 floats (malloc'd); 0 on success */
int  light_hdr_read(const char *path, float **rgb, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif
