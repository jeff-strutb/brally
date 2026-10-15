"""metalsrc.py: the Metal renderer's whole shader source as brr_open builds it
(brr_metal.m's k_src, then BRR_LIGHTS_SRC, BRR_LIT_SRC and BRR_VOL_SRC; and the reflections'
local cube, BRR_RCUBE_SRC, which reads the lighting's), as a C program that
prints it, so the C compiler joins the string literals exactly as the build
does.  Usage: metalsrc.py BRR_METAL_M OUT_C
"""
import sys


def main():
    src, out = sys.argv[1], sys.argv[2]
    lines = open(src).read().split('\n')
    start = next(i for i, l in enumerate(lines) if l.startswith('static NSString *const k_src = @'))
    end = next(i for i in range(start + 1, len(lines)) if lines[i].rstrip().endswith('";'))
    body = '\n'.join(lines[start + 1:end + 1])
    flat = ' '.join(open(src).read().split())     # the order below has to be the renderer's
    if ('[[[k_src stringByAppendingString:@BRR_LIGHTS_SRC] stringByAppendingString:@BRR_LIT_SRC] '
            'stringByAppendingString:@BRR_VOL_SRC]') not in flat:
        sys.exit('metalsrc.py: brr_metal.m no longer assembles k_src + lights + lit + vol')
    with open(out, 'w') as f:
        f.write('#include <stdio.h>\n#include "brr_metal_lights.h"\n#include "brr_metal_lit.h"\n#include "brr_metal_vol.h"\n#include "brr_metal_rcube.h"\n')
        f.write('static const char *const k_src =\n' + body + '\n')
        f.write('int main(void) { fputs(k_src, stdout); fputs(BRR_LIGHTS_SRC, stdout); fputs(BRR_LIT_SRC, stdout); fputs(BRR_VOL_SRC, stdout); fputs(BRR_RCUBE_SRC, stdout); return 0; }\n')


if __name__ == '__main__':
    main()
