#!/bin/sh
# vol.sh: the volumetric fog's tests (DESIGN.md 9), no game run: the
# integration against closed forms, the energy-conserving step against the
# naive sum, the phase function's normalisation (vol_test.m, CPU); the
# renderer's own kernels against the CPU reference on the GPU (macOS); the
# shader source with the volume's kernels compiled offline for macOS and iOS
# (lights.sh compile builds and compiles the same source).
#   ports/remastered/tools/lighttest/vol.sh
set -e
cd "$(dirname "$0")/../../../.."
ports/remastered/tree.sh make build/remastered/tree || exit 2   # the sources as Remastered builds them
T=ports/remastered/tools/lighttest
P=build/remastered/tree/ports/brally/platform
OUT=build/brally/lighttest
mkdir -p $OUT
CFLAGS="-O2 -g -std=gnu11 -Wall -ffp-contract=off -I$P/light -I$P/render/metal -I$T"
python3 $T/metalsrc.py $P/render/metal/brr_metal.m $OUT/metalsrc_vol.c
clang $CFLAGS $OUT/metalsrc_vol.c -o $OUT/metalsrc_vol
$OUT/metalsrc_vol > $OUT/vol.metal
clang $CFLAGS -x objective-c -fobjc-arc $T/vol_test.m -framework Metal -framework Foundation -lm -o $OUT/vol_test
$OUT/vol_test $OUT/vol.metal
if xcrun -sdk macosx metal -v >/dev/null 2>&1; then
  for sdk in macosx iphoneos; do
    xcrun -sdk $sdk metal -c $OUT/vol.metal -o $OUT/vol-$sdk.air
    echo "compiled the volume's kernels offline for $sdk"
  done
fi
