/* brr_metal_env_api.h: the sky's environment on the GPU (brr_metal_env.h,
 * brr_metal_env.m): made once a skymap from light_sky.c's radiance. */
#ifndef BRR_METAL_ENV_API_H
#define BRR_METAL_ENV_API_H

#import <Metal/Metal.h>
#include "light_sky.h"

typedef struct BrrEnv {
    id<MTLTexture> cube;     /* the sky, ENV_CUBE a face, mipmapped (RGBA16F, cd/m2) */
    id<MTLTexture> spec;     /* the specular environment, ENV_SPEC a face, ENV_SPEC_MIPS mips */
    float rot;               /* the map's bearing turn (LightSky.rot) */
    int ready;
} BrrEnv;

/* the cube and the specular from the skymap s, through lib (BRR_ENV_PASS_SRC
 * and the full-screen vertex stage lvs), on queue q; waits for the GPU (once a
 * map).  0 on success; the old textures kept on failure */
int brr_env_build(BrrEnv *e, id<MTLDevice> dev, id<MTLLibrary> lib, id<MTLCommandQueue> q, const LightSky *s);

#endif
