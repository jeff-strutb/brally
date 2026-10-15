/* reflsrc.m: prints the reflection pass's whole shader source, as
 * brr_metal_refl.m compiles it (LU, the wet surfaces, the sky's environment,
 * the probes' lookup, the pass; no rays), for refl.sh's offline compiles */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include "brr_metal_lit.h"
#include "brr_metal_rt.h"
#include "brr_metal_gi_src.h"
#include "brr_metal_lights.h"
int main(void)
{
    printf("#include <metal_stdlib>\nusing namespace metal;\n%s%s%s%s%s%s%s", BRR_LU_SRC, BRR_WET_SRC,
           BRR_ENV_SRC, BRR_GI_DEFS, BRR_LIGHTS_SRC, BRR_GI_SAMPLE_SRC, BRR_REFL_SRC);   /* as brr_metal_refl.m builds it */
    return 0;
}
