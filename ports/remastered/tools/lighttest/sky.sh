#!/bin/sh
# sky.sh: the skymap (light_sky.c): the sun found, measured and painted out,
# the map's calibration, the ground and the irradiance harmonics, exact on a
# synthetic sky and against brute-force integration on a real map
# (SKYMAP, default the Remastered sunny map when present).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
"$ROOT/ports/remastered/tree.sh" make "$ROOT/build/remastered/tree" || exit 2   # the sources as Remastered builds them
OUT=${OUT:-$ROOT/build/brally/lighttest}
mkdir -p "$OUT"
clang -O2 -std=gnu11 -Wall -I"$ROOT/build/remastered/tree/ports/brally/platform/light" \
  "$HERE/sky_test.c" "$ROOT/build/remastered/tree/ports/brally/platform/light/light_sky.c" -o "$OUT/sky_test" -lm || exit 2
MAP=${SKYMAP:-$ROOT/ports/common/skies/sunny.hdr}
if [ -f "$MAP" ]; then "$OUT/sky_test" "$MAP"; else echo "sky.sh: no real map at $MAP (synthetic only)"; "$OUT/sky_test"; fi
