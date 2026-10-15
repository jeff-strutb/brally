/* light_cluster.h: many lights (DESIGN.md 7.4): clustered light culling and
 * the lamps' shadow atlas.  Plain C, no GPU calls: the renderer copies a
 * LightSet into a buffer the shaders read (brr_metal_lights.h mirrors its
 * layout), and draws the atlas tiles the plan asks for.
 *
 * Clusters: the camera's view cut into LC_TX x LC_TY screen tiles (equal
 * steps of its clip x/w and y/w) and LC_TZ depth slices (exponential in the
 * view depth, Persson 2015), each listing the lights whose influence (where
 * the shader's light can be non-zero: its range, and for a cone its outer
 * angle) reaches it.  The outermost tiles and slices are open (they reach to
 * infinity, behind the near slice and past the far one), so every point in
 * front of the camera has a cluster that holds every light reaching it.
 *
 * Shadows: each shadowed spot gets a square perspective tile of the atlas,
 * 1024 to 128 texels by its importance, packed as aligned power-of-two
 * blocks. */
#ifndef BR_LIGHT_CLUSTER_H
#define BR_LIGHT_CLUSTER_H

#include <stdint.h>

#include "light.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LC_TX 16
#define LC_TY 9
#define LC_TZ 24
#define LC_N (LC_TX * LC_TY * LC_TZ)
#define LC_LIGHTS 128            /* lights a frame */
#define LC_IDS 256               /* light identities the shadow cache follows */
#define LC_NEAR 2.0f             /* metres: the first slice's far end (it reaches back to the eye) */
#define LC_FAR 600.0f            /* the last slice's near end (it reaches on without end) */

/* the header the shaders read: the view's clip transform (column-major, as
 * Metal's float4x4: clip = vp * float4(p, 1)), 1 / |the w row's gradient| (so
 * the view depth is clip.w times it), the slices' boundaries in view depth,
 * and the atlas */
typedef struct LightClusterHdr {
    float vp[16];
    float eye[4];                /* the eye; w: the number of lights */
    float zk[4];                 /* x: 1 / |grad w|; y: (LC_TZ - 2) / log2(LC_FAR / LC_NEAR); z: log2(LC_NEAR); w: 0 */
    float zb[28];                /* slice k is [zb[k], zb[k + 1]) for k = 1..LC_TZ - 2, zb[1] = LC_NEAR,
                                    zb[LC_TZ - 1] = LC_FAR; slice 0 is everything nearer, the last everything farther */
    float atl[4];                /* the atlas's size in texels, its reciprocal, the shadowed lights, 0 */
} LightClusterHdr;

/* a lamp's shadow tile: its world to clip transform (column-major; clip.w is
 * the depth along the lamp's axis), where it is in the atlas (u0, v0, size, 1
 * when it holds the lamp's depth else 0) and its depth mapping (z/w = A + B/w;
 * tan of the tile's half angle; the near plane) */
typedef struct LightShadowRec {
    float m[16];
    float rect[4];
    float prm[4];                /* A, B, tan, near */
} LightShadowRec;

/* all of it, one buffer: the header, the lights, their shadow tiles, each
 * cluster's list (offset << 8 | count) and the lists' light numbers */
typedef struct LightSet {
    LightClusterHdr h;
    LightSpot l[LC_LIGHTS];
    LightShadowRec s[LC_LIGHTS];
    uint32_t cl[LC_N];
    uint8_t idx[LC_N * LC_LIGHTS];
} LightSet;

/* what a build found */
typedef struct LightClusterStats {
    int lights;                  /* lights given */
    int visible;                 /* lights in at least one cluster */
    long entries;                /* list entries over all clusters */
    int maxlist;                 /* the longest list */
    int cand, refined;           /* (light, cluster) pairs passing the planes; of them dropped by the sphere test */
    float cov[LC_LIGHTS];        /* each light's screen coverage: its tiles over all tiles */
} LightClusterStats;

/* the lights' influence, as the shaders compute it: lamp() x lwin() (the
 * intensity toward p, cd, times the range and cone window), 0 where the light
 * cannot reach (the reference the culling is tested against) */
float light_influence(const LightSpot *l, const float p[3]);

/* the clusters for the camera vp (LightCamera's: row vectors, clip = [p 1] vp,
 * w > 0 in front) and its eye, over s->l[0..n): fills s->h (but atl), s->cl
 * and s->idx */
void light_cluster_build(LightSet *s, int n, const float vp[16], const float eye[3], LightClusterStats *st);
/* the clusters' geometry alone (the camera's; light_cluster_build's first
 * part): the header, the planes (normal, offset) and the tile corners' rays,
 * which brr_metal_cluster.h's kernels take to build the lists on the GPU */
typedef struct LightClusterGeom {
    float px[LC_TX + 1][4], py[LC_TY + 1][4], pz[LC_TZ + 1][4];
    float ray[LC_TY + 1][LC_TX + 1][4];
    float org[4];
    float zb[LC_TZ + 4];
} LightClusterGeom;
void light_cluster_geom(LightSet *s, int n, const float vp[16], const float eye[3], LightClusterGeom *g);
/* the cluster a world point falls in, exactly as the shaders find it */
int  light_cluster_of(const LightSet *s, const float p[3]);

/* ---- shadows ------------------------------------------------------------------- */
#define LS_TILE_MAX 1024
#define LS_TILE_MIN 128
#define LS_NEARP 0.25f           /* metres: a lamp's shadow near plane (its own lens and bumper are nearer) */

/* the track's triangles, the static casters: binned into a grid on the
 * ground at load (gen: the track's generation, rebinned when it changes) */
void light_shadow_track(const float *tri, int ntri, unsigned gen);
/* a spot's shadow transform: tile tan(half angle) from its outer cone */
int  light_shadow_matrix(const LightSpot *l, LightShadowRec *r);
/* the track's triangles that can cast into the lamp's tile (its frustum): their
 * vertex numbers (3 per triangle, into the track's array), at most max; the count */
int  light_shadow_track_casters(const LightShadowRec *r, const LightSpot *l, uint32_t *out, int max);
/* the grid itself, for the GPU's copy (brr_metal_cast.h cast_tiles): cells of
 * LS_TRACK_CELL metres from lo, nx by ny; cell c's triangles list[start[c]] to
 * list[start[c + 1]], their extent box[6 c] (lo xyz, hi xyz); build counts the
 * rebuilds.  0 when there is no grid */
#define LS_TRACK_CELL 16.0f
typedef struct LightTrackGrid {
    unsigned build;
    int ntri, nx, ny;
    float lo[2];
    const int *start, *list;
    const float *box;
} LightTrackGrid;
int  light_shadow_track_grid(LightTrackGrid *g);

typedef struct LightShadowTile {
    int light;                   /* in the set */
    int x, y, size;              /* atlas texels */
    int render;                  /* draw it this frame (else its cached depth stands) */
} LightShadowTile;
typedef struct LightShadowPlan {
    int n;                       /* tiles */
    LightShadowTile t[LC_LIGHTS];
    int rendered, cached, stale, dropped, repacked;
    long texels;                 /* texels drawn this frame */
    float imp[LC_LIGHTS];        /* each light's importance */
} LightShadowPlan;
/* choose, size, place and schedule the shadow tiles: candidates are the
 * cones (head lamps, spots) seen in some cluster; importance = candela x
 * screen coverage / distance^2 from the eye (a light that held a tile last
 * frame counts 1.25 times: no flicker between near equals); sizes from
 * coverage (1024 over a quarter of the screen down to 128), halved from the
 * least important until all fit the atlas; a tile is drawn again when its
 * light or place changed or the moving casters near it did (dyn, ndyn
 * vertices: hashed), the most important first, within budget texels a frame;
 * a tile with no depth yet that misses the budget is unshadowed this frame.
 * Fills s->s and s->h.atl.  ids: each light's identity (LC_IDS), stable
 * across frames. */
void light_shadow_plan(LightSet *s, const int *ids, const LightClusterStats *st, const float eye[3],
                       const float *dyn, int ndyn, int atlas, long budget, LightShadowPlan *p);
/* forget every cached tile (the atlas was remade) */
void light_shadow_reset(void);

#ifdef __cplusplus
}
#endif
#endif
