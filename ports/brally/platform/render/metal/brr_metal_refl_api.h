/* brr_metal_refl_api.h: the reflection pass's entry points (brr_metal_refl.m) */
#ifndef BRR_METAL_REFL_API_H
#define BRR_METAL_REFL_API_H
#import <Metal/Metal.h>
#include "brr_metal_rt.h"

/* the tier gate (LU.wet.z): BR_REFL, default 2 (screen space, the BVH, the sky); 0 off */
int  brr_refl_tier(void);
/* the reflections added into hdr (the lit target, pre-exposed); lu the frame's
 * LU, scroff the byte offset of its scr (world to the target's pixels) */
void brr_refl_pass(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const void *lu, size_t lusz, size_t scroff, const BrrRtWorld *rt,
                   id<MTLTexture> alb, id<MTLTexture> pos, id<MTLTexture> nrm, id<MTLTexture> hdr,
                   id<MTLTexture> sky, id<MTLTexture> bE);
#endif
