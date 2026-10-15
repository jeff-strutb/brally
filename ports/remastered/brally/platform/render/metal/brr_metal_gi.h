/* brr_metal_gi.h: diffuse global illumination by irradiance probes over the
 * track's BVH (DESIGN.md 8.2; Majercik et al. 2019, "Dynamic Diffuse Global
 * Illumination with Ray-Traced Irradiance Fields").
 *
 * Baked when a track loads (brr_gi_bake) and again when its light changes
 * (the weather, the sun, the sky's map, the track's own lamps): nothing is
 * traced while the race runs.  A volume moved with the camera (16 x 8 x 16
 * probes 8 m apart, updated a quarter a frame) left the light past 64 m to
 * the volume's edge probes and switched a new row of probes on at its far
 * edge every 8 m driven: the sky's occlusion under walls and banks swept in
 * ahead of the car (worst in snow, where nearly all the light is the sky's).
 *
 * The probes: a grid of spacing sp over the whole track, a probe at each
 * grid point at a corner of a cell some surface passes through, or next to
 * one (a lookup's point moves off its surface by up to a spacing); stored
 * sparse (gi_find).  Each pass, in batches of whole atlas rows:
 *
 *   gi_trace   every probe casts N rays (a spherical Fibonacci set turned by
 *              a random rotation each pass) through the compute BVH; a hit
 *              is shaded with its emission, the sun (shadowed by an any-hit
 *              ray), any lamps given, and the probes at the hit (the last
 *              pass's, from the second: each pass one bounce more), times
 *              its albedo over pi; a miss sees the sky
 *   gi_blend   the rays folded into the probe's irradiance, spherical
 *              harmonics to order 2 (9 coefficients, lux), and an octahedral
 *              atlas of the mean and mean square of the distance to the
 *              nearest surface (8 x 8 texels a probe, 1 texel border; metres,
 *              half floats), pass k weighed against the passes before it by
 *              min(k / (k + 1), BRR_GI_KEEP)
 *
 * The hits' lamps come from a light grid (gi_grid): cubes of 16 m or more
 * over the track, each listing the lamps whose range overlaps it, at most
 * BRR_GI_CELL, the most intense first; only the track's own lamps (the
 * cars' move).
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
    float org[4];            /* the grid's origin, spacing (metres) */
    int n[4];                /* grid points x, y, z; rays a probe */
    int base[4];             /* 0 (the grid's first point) */
    int pbase[4];            /* unused */
    float sun[4], sunc[4];   /* toward the sun; its illuminance (lux) */
    float sky[4], gnd[4];    /* radiance of a miss looking up / down (cd/m2); sky.w 1: a miss reads the sky's cube
                                (brr_gi_env) at mip gnd.w instead */
    float par[4];            /* this pass's share of the old, albedo without a record, emission scale (cd/m2), normal bias */
    float rot0[4], rot1[4], rot2[4];   /* this pass's ray rotation, rows */
    float lim[4];            /* ray length (metres), Chebyshev visibility on, lamps in the set, bounces past the first on */
    float gorg[4];           /* the light grid's corner, its cubes' side (metres) */
    int gdim[4];             /* its cubes x, y, z; lamps a cube at most (BRR_GI_CELL at most) */
    int upd[4];              /* unused */
    int bk[4];               /* probes, atlas tiles a row, the batch's first probe, its probes */
    int bx[4];               /* off's grid table and probe grid points (float4 offsets), the pass, 1: the track's lamps only */
} BrrGiU;

typedef struct BrrGi {
    id<MTLComputePipelineState> trace, bsh, bdst;
    id<MTLBuffer> sh;                /* each probe's irradiance: 9 coefficients (gi_blend_sh), float4 */
    id<MTLTexture> dst[2];           /* ping-pong while baking: [cur] the last written */
    id<MTLComputePipelineState> grid;
    id<MTLBuffer> ray, leaf, gcnt, glst;   /* the light grid: a cube's count, its list */
    id<MTLComputePipelineState> offreset, reloc;
    id<MTLBuffer> off;               /* relocations, the grid's probes, the probes' grid points (GiU bx) */
    id<MTLTexture> env, nocube;      /* the sky's prefiltered cube (brr_gi_env), a black one bound without it */
    BrrGiU u;
    int cur, nr, np, ready, baked;
    float keep;                      /* a pass's most share of the passes before it (BRR_GI_KEEP) */
    int keepray;                     /* 1: the last pass's rays and the atlas before it kept (the tests' CPU fold) */
    unsigned gen;                    /* the world's track generation leaf was made for */
    double ms;                       /* the last bake's time */
    size_t bytes;
} BrrGi;

/* the pipelines, nr rays a probe a pass; 0 on failure */
int  brr_gi_init(BrrGi *g, id<MTLDevice> dev, int nr);
void brr_gi_release(BrrGi *g);
/* the sky a miss sees: the skymap's prefiltered cube (cd/m2, world axes) at
 * mip lod, so a probe's rays take the sky's light by direction (the sun's
 * side, the horizon's glow); nil for the uniform sky / ground of brr_gi_bake */
void brr_gi_env(BrrGi *g, id<MTLTexture> spec, float lod);
/* bakes the probes over world w (its triangles, w->bvh.tri) at spacing sp in
 * passes of the pipeline's rays, each pass committed and waited for: sun
 * (unit, toward it), sunc (lux), sky / gnd radiance; the hits lit by the
 * track's own lamps of the first nlamp of lset too (an LSet, brr_metal_lights.h).
 * 1 when baked (g->baked, g->ms) */
int  brr_gi_bake(BrrGi *g, id<MTLCommandQueue> q, const BrrRtWorld *w, float sp, int passes, const float sun[3], const float sunc[3],
                 const float sky[3], const float gnd[3], id<MTLBuffer> lset, int nlamp);
/* what the lighting reads (gi_irr): the irradiance coefficients, the
 * distance atlas */
id<MTLBuffer>  brr_gi_sh(const BrrGi *g);
id<MTLTexture> brr_gi_dst(const BrrGi *g);
id<MTLBuffer>  brr_gi_off(const BrrGi *g);   /* gi_irr's off: the probes' relocation */
#endif

#endif
