#!/bin/sh
# skymask.sh: the backdrop's structural sky mask (brr_skymask.h) on synthetic
# panoramas with a known skyline, and the cloud layer's phase and Beer's law
# (brr_metal_clouds.h).  Masks of the first synthetic panoramas go to
# build/brally/lighttest/skymask as PGMs.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
OUT=${OUT:-$ROOT/build/brally/lighttest}
mkdir -p "$OUT/skymask"
clang -O2 -std=gnu11 -Wall -Wno-unused-function -I"$ROOT/ports/brally/platform/render/metal" -I"$ROOT/ports/brally/platform/light" \
  "$HERE/skymask_test.c" -o "$OUT/skymask_test" -lm || exit 2
"$OUT/skymask_test" "$OUT/skymask" "$@"
st=$?
# the sun's glare and flare constants (brr_metal_glare.h)
clang -O2 -std=gnu11 -Wall -Wno-unused-function -I"$ROOT/ports/brally/platform/render/metal" \
  "$HERE/glare_test.c" -o "$OUT/glare_test" -lm || exit 2
"$OUT/glare_test" || st=1
exit $st
