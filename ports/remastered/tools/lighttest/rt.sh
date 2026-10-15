#!/bin/sh
# Builds and runs the lighting's ray tracing tests (no game run):
#   bvh_test  the BVH against brute force on the CPU
#   rt_gpu    the Metal traversal and the hardware intersector against the CPU
#   rt_compile  the shader source at each Metal language version
# Usage: ports/remastered/tools/lighttest/rt.sh [track.obj] [rays per set]
# The track is the whole track as BR_LIGHT_TRACKOBJ writes it (game data:
# kept under build/, never committed); without it only the synthetic scenes run.
set -e
cd "$(dirname "$0")/../../../.."
ports/remastered/tree.sh make build/remastered/tree || exit 2   # the sources as Remastered builds them
T=ports/remastered/tools/lighttest
L=build/remastered/tree/ports/brally/platform/light
M=build/remastered/tree/ports/brally/platform/render/metal
OUT=build/lighttest
mkdir -p $OUT
CF="-O2 -g -std=gnu11 -Wall -ffp-contract=off -I$L -I$M -I$T"
clang $CF $T/bvh_test.c $L/light_bvh.c -o $OUT/bvh_test
clang $CF -x objective-c -fobjc-arc $T/rt_gpu.m $M/brr_metal_rt.m -x c $L/light_bvh.c \
      -framework Metal -framework Foundation -o $OUT/rt_gpu
clang -O2 -Wall -fobjc-arc -I$L -I$M $T/rt_compile.m -framework Metal -framework Foundation -o $OUT/rt_compile
$OUT/rt_compile
OBJ=${1:-$OUT/track.obj}
[ -f "$OBJ" ] || OBJ=""
$OUT/bvh_test "$OBJ" ${2:-1000000}
$OUT/rt_gpu "$OBJ" ${2:-1000000}
