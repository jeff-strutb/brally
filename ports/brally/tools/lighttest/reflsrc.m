/* reflsrc.m: prints the reflection pass's whole shader source, as
 * brr_metal_refl.m compiles it through brr_rt_library (BRR_RT_SRC, LU, the
 * wet surfaces, the sky's environment, the pass), for refl.sh's offline compiles */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include "brr_metal_lit.h"
#include "brr_metal_rt.h"
int main(void)
{
    printf("#include <metal_stdlib>\nusing namespace metal;\n#define BRR_RT_HW 1\n%s%s%s%s%s%s%s", BRR_RT_SRC, BRR_RT_HW_SRC, BRR_RT_SEL_SRC, BRR_LU_SRC, BRR_WET_SRC, BRR_ENV_SRC, BRR_REFL_SRC);
    return 0;
}
