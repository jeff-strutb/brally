#!/bin/sh
# taa.sh: the lit frame's TAA and MetalFX upscaling (brr_metal_taa.h, .m): jitter,
# reprojection, variance clipping and convergence (GPU against CPU), MetalFX's
# availability; then brr_metal_taa.m and its shaders compiled for macOS and iOS.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
"$ROOT/ports/remastered/tree.sh" make "$ROOT/build/remastered/tree" || exit 2   # the sources as Remastered builds them
P=$ROOT/build/remastered/tree/ports/brally/platform
M=$P/render/metal
OUT=${OUT:-$ROOT/build/brally/lighttest}
mkdir -p "$OUT"
fail=0
clang -O2 -g -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off -I"$P/render" -I"$M" \
  -x objective-c -fobjc-arc "$HERE/taatest.m" "$M/brr_metal_taa.m" -o "$OUT/taatest" \
  -framework Metal -weak_framework MetalFX -framework Foundation || exit 2
"$OUT/taatest" || fail=1
# the shaders offline, and brr_metal_taa.m for iOS (arm64, MetalFX behind its availability check)
printf '#include <stdio.h>\n#include "brr_metal_taa.h"\nint main(void){fputs(BRR_TAA_SRC,stdout);return 0;}\n' > "$OUT/taasrc.c"
clang -I"$M" "$OUT/taasrc.c" -o "$OUT/taasrc" && "$OUT/taasrc" > "$OUT/taa.metal" || exit 2
for sdk in macosx iphoneos; do
  if xcrun -sdk $sdk metal -c "$OUT/taa.metal" -o "$OUT/taa.$sdk.air" 2>"$OUT/taa.$sdk.log"; then
    echo "PASS  TAA shaders offline, $sdk ($(grep -c warning "$OUT/taa.$sdk.log") warnings)"
  else
    echo "FAIL  TAA shaders offline, $sdk"; cat "$OUT/taa.$sdk.log"; fail=1
  fi
done
if xcrun -sdk iphoneos clang -arch arm64 -miphoneos-version-min=15.0 -fobjc-arc -Wall -Werror -Wno-unused-function -I"$P/render" -I"$M" \
     -c "$M/brr_metal_taa.m" -o "$OUT/taa_ios.o" 2>"$OUT/taa_ios.log"; then
  echo "PASS  brr_metal_taa.m for iOS arm64 (MetalFX gated at iOS 16)"
else
  echo "FAIL  brr_metal_taa.m for iOS arm64"; cat "$OUT/taa_ios.log"; fail=1
fi
exit $fail
