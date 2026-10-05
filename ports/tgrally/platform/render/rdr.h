/* rdr.h: what the RCP draws, for a renderer (platform/render/<api>/).
 *
 * platform/gfx/ runs the game's display lists as the RSP (F3DEX 1.21) and
 * the RDP would: the RSP's transform, lighting, texture-coordinate
 * generation and fog happen there, and what reaches a renderer is the RDP's
 * work, described without reference to any graphics API: triangles in clip
 * space with their shade colour and texture coordinates, under the RDP state
 * that was current (the colour combiner's equation, the blender's mode, the
 * depth test, the two texture tiles as decoded images with their wrap
 * rules), and screen rectangles.
 *
 * Coordinates are the N64's framebuffer pixels (fb_w x fb_h, the colour
 * image's size); a renderer scales them to its output. */
#ifndef TGR_RDR_H
#define TGR_RDR_H
#include <stdint.h>

/* ---- the colour combiner: (A - B) * C + D per cycle, colour and alpha ---- */
enum {                      /* combiner inputs (each slot takes a subset) */
    RDR_CC_COMBINED, RDR_CC_TEXEL0, RDR_CC_TEXEL1, RDR_CC_PRIM, RDR_CC_SHADE, RDR_CC_ENV,
    RDR_CC_ONE, RDR_CC_ZERO, RDR_CC_NOISE, RDR_CC_KEYCENTER, RDR_CC_KEYSCALE,
    RDR_CC_COMBINED_A, RDR_CC_TEXEL0_A, RDR_CC_TEXEL1_A, RDR_CC_PRIM_A, RDR_CC_SHADE_A,
    RDR_CC_ENV_A, RDR_CC_LOD_FRAC, RDR_CC_PRIM_LOD_FRAC, RDR_CC_K4, RDR_CC_K5
};

typedef struct RdrCombine {
    uint8_t rgb[2][4];      /* [cycle][a, b, c, d] */
    uint8_t a[2][4];
} RdrCombine;

/* ---- the blender, reduced to what a GPU blend does ---------------------- */
enum {
    RDR_BLEND_OPAQUE,       /* the pixel replaces the framebuffer */
    RDR_BLEND_ALPHA,        /* src * a + dst * (1 - a) */
    RDR_BLEND_ADD,          /* src * a + dst */
    RDR_BLEND_MEM           /* the framebuffer is kept (coverage-only pass) */
};

typedef struct RdrTile {    /* one texture tile, decoded */
    int tex;                /* the renderer's texture handle (rdr_texture), 0: none */
    int w, h;               /* the decoded image's size */
    float s0, t0;           /* the tile's origin (its SETTILESIZE ul), texels */
    float sscale, tscale;   /* the tile's shift as a scale on the coordinates */
    uint8_t clamp_s, clamp_t, mirror_s, mirror_t;
    int16_t mask_s, mask_t; /* wrap period, texels (0: clamp to the image) */
    int16_t clamp_w, clamp_h; /* a clamped axis clamps to 0..clamp_w-1 (the tile's size) first,
                                 then masks and mirrors, as the RDP does */
} RdrTile;

typedef struct RdrState {
    int cycle;              /* 1 or 2 (copy and fill modes become rectangles) */
    RdrCombine cc;
    float prim[4], env[4], fog[4], blend[4];
    float prim_lod_frac, k4, k5;
    RdrTile tile[2];        /* texel 0 and texel 1 */
    int filter;             /* 0 point, 1 bilinear */
    int blend_mode;         /* RDR_BLEND_* */
    int blend_alpha;        /* the blend's alpha: 0 the combined alpha, 1 the fog colour's,
                               2 the shade's (the blender's A input) */
    int fog_blend;          /* the blender mixes in the fog colour by shade alpha */
    int alpha_compare;      /* 0 none, 1 against blend alpha, 2 dither */
    int z_test, z_write, z_decal;
    int cull;               /* (already done by the RSP: informational) */
    int scissor[4];         /* x0, y0, x1, y1 in framebuffer pixels */
} RdrState;

typedef struct RdrVtx {
    float x, y, z, w;       /* clip space: x/w, y/w in -1..1 of the viewport */
    float s, t;             /* texture coordinates, texels (the RSP's s/32, t/32 scaled) */
    float r, g, b, a;       /* shade, 0..1 (a: fog when the blender fogs) */
} RdrVtx;

/* ---- what a renderer implements ----------------------------------------- */
int  rdr_init(void);                                   /* 1 on success */
void rdr_window(void);      /* the host's window is open (called on the main thread) */
int  rdr_presents(void);    /* 1: frames reach the window by themselves (rdr_frame_end);
                               0: the platform presents rdr_frame_pixels */
void rdr_frame_begin(int fb_w, int fb_h);              /* a new colour image */
int  rdr_texture(const uint8_t *rgba, int w, int h);   /* an RGBA8 image; a handle */
void rdr_texture_free(int tex);
void rdr_triangles(const RdrState *st, const RdrVtx *v, int n);   /* n vertices, n/3 triangles */
/* a screen rectangle (fill mode, texture rectangles, copy mode) in
 * framebuffer pixels: x0,y0 inclusive .. x1,y1 exclusive; s,t at the top
 * left and their steps per pixel; a fill when st->tile[0].tex == 0 and
 * fill != 0 (then rgba is the fill colour) */
void rdr_rect(const RdrState *st, float x0, float y0, float x1, float y1,
              float s, float t, float dsdx, float dtdy, int fill, const float rgba[4]);
void rdr_clear_depth(void);
void rdr_frame_end(void);                              /* the finished frame */
/* the last finished frame, 0xAARRGGBB, top row first (for screenshots and
 * hosts that present pixels); NULL if the renderer cannot read it back */
const uint32_t *rdr_frame_pixels(int *w, int *h);
#endif
