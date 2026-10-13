#!/bin/sh
# Builds and runs the irradiance probes' tests (gi_test.m; no game run) and
# compiles their shader source offline for macOS and iOS.
set -e
cd "$(dirname "$0")/../../../.."
T=ports/brally/tools/lighttest
L=ports/brally/platform/light
M=ports/brally/platform/render/metal
OUT=build/lighttest
mkdir -p $OUT
CF="-O2 -g -std=gnu11 -Wall -ffp-contract=off -I$L -I$M -I$T"
clang $CF -x objective-c -fobjc-arc $T/gi_test.m $M/brr_metal_gi.m $M/brr_metal_rt.m -x c $L/light_bvh.c \
      -framework Metal -framework Foundation -o $OUT/gi_test
# the source as brr_rt_library assembles it, compiled offline (no ray tracing header: the compute path)
cat > $OUT/gi_src.c <<'EOF'
#include <stdio.h>
#include "brr_metal_rt.h"
#include "brr_metal_lights.h"
#include "brr_metal_gi.h"
int main(void) { printf("#include <metal_stdlib>\nusing namespace metal;\n#define BRR_RT_HW 0\n%s%s%s%s", BRR_RT_SRC, BRR_GI_DEFS, BRR_LIGHTS_SRC, BRR_GI_SRC); return 0; }
EOF
clang -I$L -I$M $OUT/gi_src.c -o $OUT/gi_src
$OUT/gi_src > $OUT/gi.metal
xcrun -sdk macosx metal -c $OUT/gi.metal -o $OUT/gi_macos.air
xcrun -sdk iphoneos metal -c $OUT/gi.metal -o $OUT/gi_ios.air
echo "gi: shader source compiles for macOS and iOS"
$OUT/gi_test
