#!/bin/sh
# night.sh: the night's checks (night_test.m): the moon, the glow's
# gating, the track's lights.  No game run.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
P=$ROOT/ports/brally/platform
OUT=${OUT:-$ROOT/build/brally/lighttest}
mkdir -p "$OUT"
clang -O2 -g -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off \
  -I"$P/include" -I"$P/host" -I"$P/render" -I"$P/common" -I"$P/light" -I"$P/render/metal" \
  -x objective-c -fobjc-arc "$HERE/night_test.m" -x c "$P/light/light_world.c" -o "$OUT/night_test" \
  -framework Metal -weak_framework MetalFX -framework QuartzCore -framework ImageIO -framework CoreGraphics -framework Foundation
# the track fixture (an OBJ with its instance records, light_game.c BR_LIGHT_TRACKOBJ), when present
TRACK_OBJ=${TRACK_OBJ:-$ROOT/build/lighttest/track.obj} "$OUT/night_test"
