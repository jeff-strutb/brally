#!/usr/bin/env python3
"""Write a C program that prints the Metal renderer's shader source as
brr_open() assembles it (k_src from brr_metal.m, then brr_metal_lights.h,
brr_metal_lit.h, brr_metal_sky.h), so the source can be compiled offline.

    shader_src.py BRR_METAL_M OUT_C
"""
import sys

src = open(sys.argv[1]).read()
start = src.index('static NSString *const k_src = @\n') + len('static NSString *const k_src = @\n')
end = src.index('";\n', start) + 1
body = src[start:end]
flat = ' '.join(src.split())          # the order below has to be the renderer's
if ('[[[k_src stringByAppendingString:@BRR_LIGHTS_SRC] stringByAppendingString:@BRR_LIT_SRC] '
        'stringByAppendingString:@BRR_SKY_SRC]') not in flat:
    sys.exit('shader_src.py: brr_metal.m no longer assembles k_src + lights + lit + sky')
with open(sys.argv[2], 'w') as f:
    f.write('#include <stdio.h>\n#include "brr_metal_lights.h"\n#include "brr_metal_lit.h"\n#include "brr_metal_sky.h"\n')
    f.write('static const char *const k_src =\n' + body + ';\n')
    f.write('int main(void) { fputs(k_src, stdout); fputs(BRR_LIGHTS_SRC, stdout); fputs(BRR_LIT_SRC, stdout); '
            'fputs(BRR_SKY_SRC, stdout); return 0; }\n')
