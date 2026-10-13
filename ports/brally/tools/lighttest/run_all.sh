#!/bin/bash
# run_all.sh: the Remastered lighting's verification suite.  Builds the
# standalone Metal harness (lighttest.m, which includes the renderer's
# brr_metal.m; no game), runs every check (PASS or FAIL with its numbers), and
# compiles the renderer's whole shader source offline for macOS and iOS.
#
#   ports/brally/tools/lighttest/run_all.sh        (from the repo root or anywhere)
#
# Exit status: 0 when every check passes.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
P=$ROOT/ports/brally/platform
OUT=${OUT:-$ROOT/build/brally/lighttest}
mkdir -p "$OUT"
fail=0

echo "== build"
clang -O2 -g -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off \
  -I"$P/include" -I"$P/host" -I"$P/render" -I"$P/common" -I"$P/light" -I"$P/render/metal" \
  -x objective-c -fobjc-arc "$HERE/lighttest.m" -o "$OUT/lighttest" \
  -framework Metal -framework QuartzCore -framework ImageIO -framework CoreGraphics -framework Foundation || exit 2
echo "built $OUT/lighttest"

"$OUT/lighttest" "$HERE/kernels.metal" || fail=1

echo
echo "== offline compile of the renderer's shaders (xcrun metal)"
"$OUT/lighttest" "$HERE/kernels.metal" --dump "$OUT/brr_lit_all.metal" || exit 2
for sdk in macosx iphoneos; do
  if ! xcrun -sdk $sdk metal --version >/dev/null 2>&1; then
    echo "SKIP  offline compile, $sdk: the Metal Toolchain is not installed (xcodebuild -downloadComponent MetalToolchain)"
    continue
  fi
  if xcrun -sdk $sdk metal -c "$OUT/brr_lit_all.metal" -o "$OUT/brr_lit_all.$sdk.air" 2>"$OUT/metal.$sdk.log"; then
    echo "PASS  offline compile, $sdk ($(grep -c warning "$OUT/metal.$sdk.log") warnings)"
  else
    echo "FAIL  offline compile, $sdk:"; cat "$OUT/metal.$sdk.log"; fail=1
  fi
done
exit $fail
