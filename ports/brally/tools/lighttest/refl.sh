#!/bin/sh
# refl.sh: the reflection pass's shader source (brr_metal_refl.h, with the
# BVH's traversal as brr_rt_library compiles it) compiled offline for macOS
# and iOS.  Exit status: 0 when both compile.
set -e
cd "$(dirname "$0")/../../../.."
OUT=build/brally/lighttest
T=ports/brally/tools/lighttest
P=ports/brally/platform
mkdir -p "$OUT"
clang -fobjc-arc -I"$P/render/metal" -I"$P/include" -I"$P/light" -I"$P/common" -I"$P/render" -I"$P/host" \
  "$T/reflsrc.m" -o "$OUT/reflsrc" -framework Metal -framework Foundation
"$OUT/reflsrc" > "$OUT/refl.metal"
if ! xcrun -sdk macosx metal --version > /dev/null 2>&1; then
    echo "SKIP  reflection shaders: no offline Metal compiler"
    exit 0
fi
for sdk in macosx iphoneos; do
    xcrun -sdk "$sdk" metal -c "$OUT/refl.metal" -o "$OUT/refl_$sdk.air" 2> "$OUT/refl_$sdk.log"
    echo "PASS  reflection shaders, $sdk: $(grep -c 'error:' "$OUT/refl_$sdk.log" || true) errors, $(grep -c 'warning:' "$OUT/refl_$sdk.log" || true) warnings"
done
