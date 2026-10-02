/* brr.h: the renderer backend interface, neutral between graphics APIs.
 *
 * The game draws through Glide 2 (platform/common/glide.c). Glide's model is
 * small and fixed: screen-space triangles, one texture, a colour and an
 * alpha combiner, alpha blending and testing, a depth buffer, table fog.
 * glide.c keeps Glide's state and textures and hands each draw here with
 * that state attached; a backend (Metal, OpenGL, a future Windows API, or
 * the null one) only has to rasterise it. The combiner parameters are
 * Glide's own values, so every backend evaluates the same equations, from
 * one description, in its own shading language.
 */
#ifndef BR_BRR_H
#define BR_BRR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct brr_vertex {
    float x, y;          /* pixels, origin top left */
    float z;             /* depth, 0 near .. 1 far (from Glide's ooz or oow) */
    float oow;           /* 1/w, for perspective-correct texturing and fog */
    float r, g, b, a;    /* 0..255, as Glide gives them */
    float s, t;          /* texel coordinates already divided by w (sow, tow) */
} brr_vertex;

typedef struct brr_state {
    /* grColorCombine / grAlphaCombine: function, factor, local, other, invert */
    int32_t cc_fn, cc_factor, cc_local, cc_other, cc_invert;
    int32_t ac_fn, ac_factor, ac_local, ac_other, ac_invert;
    /* grTexCombine on TMU0 */
    int32_t tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;
    uint32_t constant;   /* grConstantColorValue, ARGB */
    /* grAlphaBlendFunction */
    int32_t blend_rgb_src, blend_rgb_dst, blend_a_src, blend_a_dst;
    /* grAlphaTestFunction / ReferenceValue */
    int32_t atest_fn;
    uint8_t atest_ref;
    /* depth */
    int32_t depth_mode, depth_fn, depth_mask;
    int32_t cull;
    /* fog */
    int32_t fog_mode;
    uint32_t fog_color;
    uint8_t  fog_table[64];
    /* the texture (0: none) and how it is sampled */
    uint32_t texture;
    int32_t  tex_w, tex_h;
    int32_t  min_filter, mag_filter, clamp_s, clamp_t;
    /* scissor: grClipWindow */
    int32_t clip_x0, clip_y0, clip_x1, clip_y1;
} brr_state;

int      brr_open(int width, int height);
void     brr_close(void);
/* a texture of RGBA8 pixels; replaces id's contents when id != 0 */
uint32_t brr_texture(uint32_t id, const uint8_t *rgba, int w, int h);
void     brr_texture_free(uint32_t id);
void     brr_clear(uint32_t argb, float depth, int colour, int depthbuf, const brr_state *clip);
/* triangles: n vertices, n a multiple of 3 */
void     brr_draw(const brr_state *st, const brr_vertex *v, int n);
/* pixels written straight to the back buffer (16-bit RGB565, Glide's LFB) */
void     brr_lfb_write(int x, int y, int w, int h, const uint16_t *rgb565, int stride);
void     brr_present(void);
/* the frame last presented, to a PNG; 0 when this backend cannot */
int      brr_shot(const char *path);

#ifdef __cplusplus
}
#endif
#endif
