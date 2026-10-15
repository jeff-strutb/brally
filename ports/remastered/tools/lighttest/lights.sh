#!/bin/sh
# lights.sh: the many-lights tests (DESIGN.md 7.4), no game run: the clusters and the
# lamps' shadow atlas against brute force on the CPU (cluster_test.c), the
# shaders' own code on the GPU against the same (metal_test.m, macOS), and the
# lighting shaders compiled offline for macOS and iOS.
#   ports/remastered/tools/lighttest/lights.sh [cpu|gpu|compile]   (default: all)
set -e
cd "$(dirname "$0")/../../../.."
ports/remastered/tree.sh make build/remastered/tree || exit 2   # the sources as Remastered builds them
T=ports/remastered/tools/lighttest
P=build/remastered/tree/ports/brally/platform
OUT=build/brally/lighttest
mkdir -p $OUT
what=${1:-all}
CFLAGS="-O2 -g -std=gnu11 -Wall -ffp-contract=off -I$P/light -I$P/render/metal -I$T"

if [ "$what" = all ] || [ "$what" = cpu ]; then
  clang $CFLAGS $T/cluster_test.c $P/light/light_cluster.c -lm -o $OUT/cluster_test
  $OUT/cluster_test
fi
if [ "$what" = all ] || [ "$what" = gpu ]; then
  clang $CFLAGS -x objective-c -fobjc-arc $T/metal_test.m -x c $P/light/light_cluster.c \
    -framework Metal -framework Foundation -lm -o $OUT/metal_test
  $OUT/metal_test
fi
if [ "$what" = all ] || [ "$what" = compile ]; then
  # the lighting's shader source as the renderer builds it at start-up, compiled offline
  python3 $T/metalsrc.py $P/render/metal/brr_metal.m $OUT/metalsrc_lights.c
  clang $CFLAGS $OUT/metalsrc_lights.c -o $OUT/metalsrc_lights
  $OUT/metalsrc_lights > $OUT/lit.metal
  # as the game compiles it at start-up (this Mac's runtime compiler), with the lighting's pipelines
  clang $CFLAGS -x objective-c -fobjc-arc $T/compile_test.m -framework Metal -framework Foundation -o $OUT/compile_test
  $OUT/compile_test $OUT/lit.metal
  # offline, for macOS and iOS: needs Xcode's Metal Toolchain component
  if xcrun -sdk macosx metal -v >/dev/null 2>&1; then
    for sdk in macosx iphoneos; do
      xcrun -sdk $sdk metal -c $OUT/lit.metal -o $OUT/lit-$sdk.air
      echo "compiled the renderer's shaders offline for $sdk"
    done
  else
    echo "offline compile skipped: no Metal Toolchain (xcodebuild -downloadComponent MetalToolchain installs it)"
  fi
fi
