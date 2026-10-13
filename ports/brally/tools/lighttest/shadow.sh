#!/bin/sh
# shadow.sh: the sun's shadows checked by arithmetic, without running the game.
#   1. the Metal renderer's whole shader source (as brr_open builds it)
#      compiled offline for macOS and for iOS
#   2. shadowtest.c: the soft shadow against the physics, the static cache
#      against brute force, the world-to-pixel map against the sky's ray,
#      the contact shadow against ray-traced truth (CPU)
#   3. gputest.m: the shader's own shadow functions run on this Mac's GPU
#      over synthetic maps, against their CPU twins; the cache's restore pass
#      checked bit for bit (Metal's validation layer on)
# Outputs under build/brally/lighttest.  Exits non-zero on any failure.
set -e
cd "$(dirname "$0")/../../../.."
OUT=build/brally/lighttest
T=ports/brally/tools/lighttest
M=ports/brally/platform/render/metal
mkdir -p "$OUT"
PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

"$PY" "$T/metalsrc.py" "$M/brr_metal.m" "$OUT/metalsrc.c"
cc -I"$M" -I"$M/../../light" -o "$OUT/metalsrc" "$OUT/metalsrc.c"
"$OUT/metalsrc" > "$OUT/brr.metal"
if xcrun -sdk macosx metal --version > /dev/null 2>&1; then
    for sdk in macosx iphoneos; do
        xcrun -sdk "$sdk" metal -c "$OUT/brr.metal" -o "$OUT/brr_$sdk.air" 2> "$OUT/metal_$sdk.log"
        echo "shader source ($(wc -l < "$OUT/brr.metal" | tr -d ' ') lines), $sdk: compiled offline, $(grep -c 'error:' "$OUT/metal_$sdk.log" || true) errors, $(grep -c 'warning:' "$OUT/metal_$sdk.log" || true) warnings"
    done
else
    # Xcode 26 and later ship the offline compiler separately
    # (xcodebuild -downloadComponent MetalToolchain); gputest still compiles
    # the whole source with the system's compiler, as brr_open does
    echo "shader source: no offline Metal compiler here (the Metal Toolchain component is not installed): offline macOS and iOS compiles skipped"
fi

cc -std=c99 -O2 -Wall -Wextra -ffp-contract=off -o "$OUT/shadowtest" "$T/shadowtest.c" -lm
"$OUT/shadowtest"

clang -fobjc-arc -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -o "$OUT/gputest" "$T/gputest.m" -I"$M" -I"$M/../../light" -framework Metal -framework Foundation
MTL_DEBUG_LAYER=1 "$OUT/gputest" "$OUT/brr.metal"
