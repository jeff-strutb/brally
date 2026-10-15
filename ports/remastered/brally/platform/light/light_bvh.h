/* light_bvh.h: the world for ray-traced lighting (DESIGN.md 8.1): a
 * bounding volume hierarchy over the whole track's triangles
 * (light_game_track), built once a track generation, laid out for the GPU's
 * traversal (brr_metal_rt.h) and traced here on the CPU by the same
 * arithmetic.  Plain C, no game types. */
#ifndef BR_LIGHT_BVH_H
#define BR_LIGHT_BVH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 32 bytes, depth first.  n > 0: a leaf, triangles a .. a + n - 1 of the
 * leaf-ordered arrays.  n == 0: an interior node, its children this + 1 and
 * a. */
typedef struct LightBvhNode {
    float lo[3];
    int32_t a;
    float hi[3];
    int32_t n;
} LightBvhNode;

/* a triangle's surface record, indexed by its original number (the track's
 * order, which is also the hardware structure's primitive id): what a ray
 * shades on hit without texture lookups.  obj from the track; the rest
 * zero until the materials fill them (albedo and emission rgba8, mat the
 * material class). */
typedef struct LightBvhPrim {
    int32_t obj;
    uint32_t mat;
    uint32_t albedo;
    uint32_t emit;
} LightBvhPrim;

typedef struct LightBvh {
    LightBvhNode *node;
    float *tri;              /* ntri x 9, leaf order: the corners as the track has them */
    uint32_t *id;            /* leaf order -> original triangle */
    LightBvhPrim *prim;      /* by original triangle */
    int nnode, ntri, nleaf, depth;
    float sah;               /* the tree's SAH cost (traversal 1, triangle 1, by the root's area) */
} LightBvh;

/* builds b over tri (ntri x 9 floats) with obj per triangle (may be NULL);
 * 0 on failure (out of memory).  Binned SAH, 16 bins, leaves of at most 4. */
int  light_bvh_build(LightBvh *b, const float *tri, const int *obj, int ntri);
void light_bvh_free(LightBvh *b);

/* a hit: the distance along d (d need not be unit; t is in its units), the
 * barycentric weights of the second and third corners (as Metal's
 * intersector reports them) and the original triangle, -1 for none */
typedef struct LightHit {
    float t, u, v;
    int tri;
} LightHit;
/* what a traversal visited (tests and reports) */
typedef struct LightBvhStats {
    uint64_t nodes, tris;
} LightBvhStats;

/* the closest hit with tmin < t < tmax; 1 when there is one */
int  light_bvh_closest(const LightBvh *b, const float o[3], const float d[3], float tmin, float tmax,
                       LightHit *h, LightBvhStats *st);
/* any hit with tmin < t < tmax (shadow and visibility rays) */
int  light_bvh_any(const LightBvh *b, const float o[3], const float d[3], float tmin, float tmax,
                   LightBvhStats *st);
/* one triangle (v: 9 floats) against a ray, the watertight test the
 * traversals use (Woop, Benthin and Wald 2013); 1 with *t, *u, *v set when
 * it is hit at tmin < t < tmax.  Exposed for the tests' brute force. */
int  light_tri_hit(const float o[3], const float d[3], const float *v, float tmin, float tmax,
                   float *t, float *u, float *w);

#ifdef __cplusplus
}
#endif
#endif
