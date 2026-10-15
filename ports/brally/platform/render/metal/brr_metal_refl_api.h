/* brr_metal_refl_api.h: the reflection pass's entry points (brr_metal_refl.m) */
#ifndef BRR_METAL_REFL_API_H
#define BRR_METAL_REFL_API_H
#import <Metal/Metal.h>
#include "brr_metal_rt.h"

/* the tier gate (LU.wet.z): BR_REFL, default 2 (screen space, the BVH, the sky); 0 off */
int  brr_refl_tier(void);
/* the reflections added into hdr (the lit target, pre-exposed); lu the frame's
 * LU, scroff the byte offset of its scr (world to the target's pixels); giu
 * (gisz bytes), girr (brr_gi_sh) and gdst the irradiance probes (brr_metal_gi.h), giu
 * NULL when they are off; lset the frame's lights and gcnt, glst the probes'
 * light grid over them (nil: no lamps on the hits), goff the probes'
 * relocation (brr_gi_off) */
void brr_refl_pass(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const void *lu, size_t lusz, size_t scroff, const BrrRtWorld *rt,
                   id<MTLTexture> alb, id<MTLTexture> pos, id<MTLTexture> nrm, id<MTLTexture> hdr,
                   id<MTLTexture> sky, id<MTLTexture> bE, const void *giu, size_t gisz, id<MTLBuffer> girr, id<MTLTexture> gdst,
                   id<MTLBuffer> lset, id<MTLBuffer> gcnt, id<MTLBuffer> glst, id<MTLBuffer> goff);
/* BR_LIGHT_SUNCHECK (diagnostic): the sun's visibility in hdr (the deferred
 * pass's BR_LIGHT_VIEW=3) against rays traced through the track
 * (brr_metal_refl.h svck), the frame marked and the counts logged */
void brr_suncheck_pass(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const void *lu, size_t lusz, const BrrRtWorld *rt,
                       id<MTLTexture> pos, id<MTLTexture> nrm, id<MTLTexture> hdr, unsigned long frame);
#endif
