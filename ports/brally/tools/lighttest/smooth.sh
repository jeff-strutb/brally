#!/bin/sh
# smooth.sh: smooth shading normals (light_smooth.c; smooth_test.c) on a
# prism, the same with flipped windings, a sphere, and a degenerate triangle.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
OUT=${OUT:-$ROOT/build/brally/lighttest}
mkdir -p "$OUT"
clang -O2 -std=gnu11 -Wall -I"$ROOT/ports/brally/platform/light" "$HERE/smooth_test.c" "$ROOT/ports/brally/platform/light/light_smooth.c" \
  -o "$OUT/smooth_test" -lm || exit 2
"$OUT/smooth_test"
