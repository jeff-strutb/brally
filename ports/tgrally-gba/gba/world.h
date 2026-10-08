/* world.h -- the proof of concept's data (tools/convert.py writes world_data.c) */
#ifndef WORLD_H
#define WORLD_H
#include <stdint.h>

typedef struct {
    uint32_t vstart; uint16_t nv; uint8_t dlo, dhi;  /* d: 8-unit steps, its triangles' widest */
    uint32_t tstart; uint32_t nt;
    int16_t c[3], r;              /* its bounding sphere, 1/8 world units */
} Cell;
typedef struct { int16_t x, y, z, pad; } V3;                  /* 1/8 world units from the origin */
typedef struct {
    uint16_t a, b, c, col;        /* cell-local vertex indices, RGB555 */
    int16_t cx, cy;               /* its centre on the ground, world units from the origin */
    uint8_t dlo, dhi;             /* the camera distances (8-unit steps) the game drew it at */
    uint16_t tex;                 /* g_tex index, 0xFFFF: flat (col) */
    int16_t uv[6];                /* u, v at a, b, c: texels, Q4 */
    int16_t n[3], pad;            /* its plane: n Q12, the camera in front when n . cam > d */
    int32_t d;
} Tri;
typedef struct {
    const uint16_t *data;         /* RGB555, bit 15: clear; rows 64 texels apart */
    uint16_t wm2, hm7;            /* (1 << wbits) - 1 << 1, (1 << hbits) - 1 << 7: the span's masks */
    uint8_t wbits, hbits, alpha, pad;
} Tex;
extern const Tex g_tex[];
typedef struct {
    int32_t m[12];                /* rows X, Y, W: three coefficients (Q12 to the result) and a Q8 constant */
    int16_t pos[2];               /* the camera, world units from the origin */
    int16_t fwd[2];               /* the way it looks on the ground, Q8 */
    int32_t cam[3];               /* the camera, 1/8 world units */
    int32_t edge[4][4];           /* the screen's edges as planes: n Q12, d; on the screen side n . p >= -d */
} Frame;

extern const int g_grid_w, g_grid_h, g_nframes, g_maxv;
extern const Cell g_cells[];
extern const V3 g_verts[];
extern const Tri g_tris[];
extern const Frame g_frames[];
extern const int g_org[2];          /* the world origin of the cell grid and the vertices */
extern const int g_pvs_x0, g_pvs_y0, g_pvs_w, g_pvs_h, g_pvs_cell;   /* camera cells, absolute */
extern const uint32_t g_pvs_at[];   /* per camera cell: where its list starts in g_pvs, or ~0 */
extern const uint16_t g_pvs[];      /* cell, n, n triangle indices in it; ...; 0xFFFF */
#endif
