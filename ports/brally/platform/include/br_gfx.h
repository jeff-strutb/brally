/* br_gfx.h: how the game draws, straight to the renderer (brr.h).
 *
 * The game was written for a 3dfx card and keeps that card's way of
 * drawing: screen-space triangles, one texture at a time, a colour and an
 * alpha combiner, blending, alpha testing, a depth buffer and table fog.
 * The combiner, blend and compare arguments are the values the game always
 * passed; brr_state carries them to every backend unchanged. Textures live
 * in a range of texture memory the game allocates itself (its slot table):
 * an upload stores the game's bytes at an offset in that range, and a bind
 * reads the bytes at an offset with the description it is given.
 *
 * Implemented by platform/common/br_gfx.c; force-included into the core. */
#ifndef BR_GFX_H
#define BR_GFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a vertex as the game builds it: screen pixels, colour 0..255, 1/z scaled
 * to 0..65535, 1/w, and texel coordinates divided by w for each of the two
 * texture units the game's vertex records have room for (it uses the first) */
typedef struct BrGfxTexCoord {
    float sow, tow, oow;
} BrGfxTexCoord;

typedef struct BrGfxVertex {
    float x, y, z;          /* +0x00 */
    float r, g, b;          /* +0x0C */
    float ooz;              /* +0x18 */
    float a;                /* +0x1C */
    float oow;              /* +0x20 */
    BrGfxTexCoord tc[2];    /* +0x24 */
} BrGfxVertex;

/* a texture: its mip chain from largeLod (256 >> largeLod texels on the long
 * side) down to smallLod, the long side's ratio to the short one, and the
 * texel format below */
typedef struct BrGfxTexInfo {
    int32_t smallLod;
    int32_t largeLod;
    int32_t aspectRatio;    /* 0..6: 8x1, 4x1, 2x1, 1x1, 1x2, 1x4, 1x8 */
    int32_t format;
    void   *data;
} BrGfxTexInfo;

/* texel formats */
#define BR_GFX_FMT_RGB_332              0
#define BR_GFX_FMT_ALPHA_8              2
#define BR_GFX_FMT_INTENSITY_8          3
#define BR_GFX_FMT_ALPHA_INTENSITY_44   4
#define BR_GFX_FMT_ARGB_8332            8
#define BR_GFX_FMT_RGB_565              10
#define BR_GFX_FMT_ARGB_1555            11
#define BR_GFX_FMT_ARGB_4444            12
#define BR_GFX_FMT_ALPHA_INTENSITY_88   13

#define BR_GFX_FOG_TABLE_SIZE 64

/* the picture: w x h pixels, y up from the bottom; 0 when no renderer opens */
int  br_gfx_open(int w, int h);
void br_gfx_shutdown(void);
void br_gfx_clip(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
/* colour RGBA (red in the top byte), alpha, depth 0..0xFFFF */
void br_gfx_clear(uint32_t color, uint8_t alpha, uint16_t depth);
void br_gfx_present(void);

/* state: the combiners' function, factor, local, other and invert */
void br_gfx_color_combine(int32_t fn, int32_t factor, int32_t local, int32_t other, int32_t invert);
void br_gfx_alpha_combine(int32_t fn, int32_t factor, int32_t local, int32_t other, int32_t invert);
void br_gfx_blend(int32_t rgb_src, int32_t rgb_dst, int32_t a_src, int32_t a_dst);
void br_gfx_alpha_test(int32_t cmp);
void br_gfx_alpha_ref(uint8_t ref);
void br_gfx_constant(uint32_t color);
void br_gfx_cull(int32_t mode);         /* 0 none, 1 negative area, 2 positive */
void br_gfx_depth_mode(int32_t mode);   /* 0 off, 1 z, 2 w */
void br_gfx_depth_func(int32_t cmp);
void br_gfx_depth_mask(int32_t on);
void br_gfx_fog_mode(int32_t mode);
void br_gfx_fog_color(uint32_t color);
void br_gfx_fog_table(const uint8_t t[BR_GFX_FOG_TABLE_SIZE]);
void br_gfx_fog_linear(uint8_t t[BR_GFX_FOG_TABLE_SIZE], float nearZ, float farZ);
void br_gfx_tex_combine(int32_t unit, int32_t rgb_fn, int32_t rgb_factor, int32_t a_fn,
                        int32_t a_factor, int32_t rgb_invert, int32_t a_invert);
void br_gfx_tex_filter(int32_t unit, int32_t min, int32_t mag);
void br_gfx_tex_clamp(int32_t unit, int32_t s, int32_t t);

/* texture memory: the range the game allocates in, and the bytes a texture
 * of this shape takes there */
uint32_t br_gfx_tex_lo(int32_t unit);
uint32_t br_gfx_tex_hi(int32_t unit);
uint32_t br_gfx_tex_bytes(int32_t smallLod, int32_t largeLod, int32_t aspect, int32_t format);
void     br_gfx_tex_upload(int32_t unit, uint32_t start, const BrGfxTexInfo *info);
void     br_gfx_tex_bind(int32_t unit, uint32_t start, const BrGfxTexInfo *info);

/* drawing */
void br_gfx_triangle(const BrGfxVertex *a, const BrGfxVertex *b, const BrGfxVertex *c);
void br_gfx_polygon(int n, const BrGfxVertex v[]);
/* RGB565 pixels straight onto the picture; stride in bytes */
void br_gfx_pixels(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int32_t stride, const void *data);

#ifdef __cplusplus
}
#endif
#endif
