/* brr_metal_taa_api.h: what brr_metal.m calls of brr_metal_taa.m */
#ifndef BRR_METAL_TAA_API_H
#define BRR_METAL_TAA_API_H
#import <Metal/Metal.h>

int brr_taa_mode(void);
int brr_taa_metalfx_supported(id<MTLDevice> dev);
id brr_taa_metalfx_make(id<MTLDevice> dev, int iw, int ih, int ow, int oh);
/* this frame's jittered copy of the screen map table (nil: TAA off) */
id<MTLBuffer> brr_taa_begin(id<MTLDevice> dev, id<MTLBuffer> xt, int slot, int nxf, int tw, int th, float *jit_px);
/* the resolved HDR (pre-exposed) at ow x oh (MetalFX's upscale from hdr's size when fx) for bloom and post; hdr itself when off.
 * S: this frame's world-to-pixel matrix unjittered (brr_screen_matrix), vp its view x projection */
id<MTLTexture> brr_taa_resolve(id<MTLDevice> dev, id<MTLCommandBuffer> cb, id<MTLTexture> hdr, id<MTLTexture> gpos,
                               id<MTLTexture> depth, const float S[16], const float vp[16], const float eye[3],
                               int ow, int oh, int fx);
/* the lit 3D's internal scale (brr_metal_taa.m); *up: through MetalFX */
float brr_taa_scale(id<MTLDevice> dev, int tw, int th, float gpu_ms, int *up);
void brr_taa_reset(void);

#endif
