/* brr_metal_gi.h: diffuse global illumination by irradiance probes over the
 * track's BVH (DESIGN.md 8.2; Majercik et al. 2019, "Dynamic Diffuse Global
 * Illumination with Ray-Traced Irradiance Fields").
 *
 * A volume of nx x ny x nz probes, spacing s metres, placed on the world grid
 * (probe w at org + w s) and moved with the camera in whole probes; a probe's
 * storage slot is its world index modulo the counts, so a move only resets
 * the slots that came in.  Each frame:
 *
 *   gi_trace   every probe casts N rays (a spherical Fibonacci set turned by
 *              a random rotation each frame) through the compute BVH; a hit
 *              is shaded with its emission, the sun (shadowed by an any-hit
 *              ray), and last frame's probes at the hit (every bounce after
 *              the first), times its albedo over pi; a miss sees the sky
 *   gi_blend   the rays folded into the probe's irradiance, spherical
 *              harmonics to order 2 (9 coefficients, lux), and an octahedral
 *              atlas of the mean and mean square of the distance to the
 *              nearest surface (16 x 16 texels a probe, 1 texel border;
 *              metres), hysteresis par.x a frame
 *
 * The update is spread over upd.x frames (4): a quarter of the probes, by
 * slot, traced and folded each frame, with the hysteresis raised to the 4th
 * power so the light settles in the same time; a slot new to the volume is
 * updated the frame it comes in.  The hits' lamps come from a light grid
 * (gi_grid, a compute pass a frame): cubes of 16 m or more over the volume
 * and the rays' reach, each listing the lamps whose range overlaps it, at
 * most BRR_GI_CELL, the most intense first.
 *
 * gi_irr(p, n, v) is a surface's indirect irradiance from the 8 probes
 * around it: trilinear, times the backface and Chebyshev visibility weights,
 * from a point pushed off the surface along its normal and toward the eye.
 *
 * Compiled with brr_rt_library (the traversal needs IEEE maths) after
 * BRR_GI_DEFS and BRR_LIGHTS_SRC (the lamps' photometry). */
#ifndef BRR_METAL_GI_H
#define BRR_METAL_GI_H

#include "brr_metal_gi_src.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#include "brr_metal_rt.h"

/* GiU */
typedef struct BrrGiU {
    float org[4];            /* the world grid's origin, spacing (metres) */
    int n[4];                /* probes x, y, z; rays a probe */
    int base[4];             /* the volume's first probe, world grid index */
    int pbase[4];            /* last frame's; w: 1 when there was one */
    float sun[4], sunc[4];   /* toward the sun; its illuminance (lux) */
    float sky[4], gnd[4];    /* radiance of a miss looking up / down (cd/m2); sky.w 1: a miss reads the sky's cube
                                (brr_gi_env) at mip gnd.w instead */
    float par[4];            /* hysteresis, albedo without a record, emission scale (cd/m2), normal bias */
    float rot0[4], rot1[4], rot2[4];   /* this frame's ray rotation, rows */
    float lim[4];            /* ray length (metres), Chebyshev visibility on, lamps in the set, bounces past the first on */
    float gorg[4];           /* the light grid's corner, its cubes' side (metres) */
    int gdim[4];             /* its cubes x, y, z; lamps a cube at most (BRR_GI_CELL at most) */
    int upd[4];              /* frames a probe's update is spread over, this frame's share */
} BrrGiU;

typedef struct BrrGi {
    id<MTLComputePipelineState> trace, bsh, bdst;
    id<MTLBuffer> sh;                /* each slot's irradiance: 9 coefficients (gi_blend_sh), float4 */
    id<MTLTexture> dst[2];           /* ping-pong: [cur] last written */
    id<MTLComputePipelineState> grid;
    id<MTLBuffer> ray, leaf, gcnt, glst;   /* the light grid: a cube's count, its list */
    id<MTLComputePipelineState> offreset, reloc;
    id<MTLBuffer> off;               /* each slot's place moved out of the track's solid (gi_reloc), float4 */
    id<MTLTexture> env, nocube;      /* the sky's prefiltered cube (brr_gi_env), a black one bound without it */
    BrrGiU u;
    int cur, nx, ny, nz, nr, ready;
    unsigned gen;                    /* the world's track generation leaf was made for */
    uint32_t frame;
    size_t bytes;
} BrrGi;

/* pipelines and textures for an nx x ny x nz volume, nr rays a probe; 0 on failure */
int  brr_gi_init(BrrGi *g, id<MTLDevice> dev, int nx, int ny, int nz, int nr);
void brr_gi_release(BrrGi *g);
/* the sky a miss sees: the skymap's prefiltered cube (cd/m2, world axes) at
 * mip lod, so a probe's rays take the sky's light by direction (the sun's
 * side, the horizon's glow); nil for the uniform sky / ground of brr_gi_frame */
void brr_gi_env(BrrGi *g, id<MTLTexture> spec, float lod);
/* centres the volume on eye in whole probes of spacing sp and sets this
 * frame's rotation; sun (unit, toward it), sunc (lux), sky / gnd radiance */
void brr_gi_frame(BrrGi *g, const float eye[3], float sp, const float sun[3], const float sunc[3],
                  const float sky[3], const float gnd[3]);
/* one update (trace, both folds) encoded into cb over world w, the hits lit
 * by the first nlamp lamps of lset (an LSet, brr_metal_lights.h) too */
void brr_gi_encode(BrrGi *g, id<MTLCommandBuffer> cb, const BrrRtWorld *w, id<MTLBuffer> lset, int nlamp);
/* what the lighting reads (gi_irr): the irradiance coefficients, the
 * distance atlas (this frame's after encode) */
id<MTLBuffer>  brr_gi_sh(const BrrGi *g);
id<MTLTexture> brr_gi_dst(const BrrGi *g);
id<MTLBuffer>  brr_gi_off(const BrrGi *g);   /* gi_irr's off: the probes' relocation */
#endif

#endif
