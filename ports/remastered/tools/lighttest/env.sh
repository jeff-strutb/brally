#!/bin/sh
# env.sh: the sky's environment on the GPU (brr_metal_env.h, .m; env_test.m):
# the cube's faces against Metal's sampling, the prefilter's normalisation
# and its GGX lobe against the CPU's.  macOS.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
"$ROOT/ports/remastered/tree.sh" make "$ROOT/build/remastered/tree" || exit 2   # the sources as Remastered builds them
OUT=${OUT:-$ROOT/build/brally/lighttest}
P=$ROOT/build/remastered/tree/ports/brally/platform
mkdir -p "$OUT"
clang -O2 -fobjc-arc -Wall -I"$P/light" -I"$P/render/metal" -x objective-c "$HERE/env_test.m" "$P/render/metal/brr_metal_env.m" \
  -x c "$P/light/light_sky.c" -framework Metal -framework Foundation -lm -o "$OUT/env_test" || exit 2
"$OUT/env_test"
