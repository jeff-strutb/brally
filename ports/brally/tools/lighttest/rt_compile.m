/* rt_compile.m: brr_metal_rt.h's shader source compiled by this Mac's Metal
 * compiler at each language version the builds can meet (macOS 12 and iOS 16
 * up: Metal 2.4 and 3.0 on), the compute traversal alone and with the
 * hardware intersector, without fast math as brr_rt_library compiles it.
 * Each with a kernel that calls every function, so nothing is left out as
 * unused.  Usage: rt_compile   (run.sh) */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "brr_metal_rt.h"

static const char *k_use =
"kernel void k_use(device const RtNode *node [[buffer(0)]], device const float *tri [[buffer(1)]],\n"
"                  device const uint *id [[buffer(2)]], device RtHit *out [[buffer(3)]],\n"
"#if BRR_RT_HW\n"
"                  raytracing::primitive_acceleration_structure as [[buffer(4)]],\n"
"#endif\n"
"                  uint i [[thread_position_in_grid]]) {\n"
"  float3 o = float3(i, 0, 0), d = float3(0, 0, 1);\n"
"  out[i] = rt_closest(node, tri, id, o, d, 0.0, INFINITY);\n"
"  out[i].tri += rt_any(node, tri, id, o, d, 0.0, 10.0) ? 1 : 0;\n"
"#if BRR_RT_HW\n"
"  out[i].t += rt_hw_closest(as, o, d, 0.0, INFINITY).t;\n"
"  out[i].tri += rt_hw_any(as, o, d, 0.0, 10.0) ? 1 : 0;\n"
"#endif\n"
"}\n";

int main(void)
{
    @autoreleasepool {
        static const struct { MTLLanguageVersion v; const char *name; } ver[] = {
            { MTLLanguageVersion2_4, "2.4" }, { MTLLanguageVersion3_0, "3.0" },
            { MTLLanguageVersion3_1, "3.1" }, { MTLLanguageVersion3_2, "3.2" },
        };
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        int k, hw, fails = 0;
        for (k = 0; k < (int)(sizeof ver / sizeof ver[0]); k++)
            for (hw = 0; hw < 2; hw++) {
                MTLCompileOptions *o = [MTLCompileOptions new];
                NSError *err = nil;
                NSString *src = [NSString stringWithFormat:@"#include <metal_stdlib>\nusing namespace metal;\n#define BRR_RT_HW %d\n%s%s%s",
                                 hw, BRR_RT_SRC, hw ? BRR_RT_HW_SRC : "", k_use];
                id<MTLLibrary> lib;
                id<MTLFunction> f;
                o.languageVersion = ver[k].v;
                if (@available(macOS 15.0, *))
                    o.mathMode = MTLMathModeSafe;
                lib = [dev newLibraryWithSource:src options:o error:&err];
                f = lib ? [lib newFunctionWithName:@"k_use"] : nil;
                if (f && ![dev newComputePipelineStateWithFunction:f error:&err])
                    f = nil;
                printf("Metal %s, %s: %s\n", ver[k].name, hw ? "compute BVH and intersector" : "compute BVH",
                       f ? (err ? "compiled, with warnings" : "compiled, no warnings") : "FAILED");
                if (err)
                    printf("%s\n", [[err localizedDescription] UTF8String]);
                if (!f)
                    fails++;
            }
        return fails != 0;
    }
}
