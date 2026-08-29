#!/bin/sh
# vmake.sh <tag> [src.c] -- private variant copy for one prober
cd "$(git rev-parse --show-toplevel)"
SRC="${2:-src/core/drawing/br_tex3d_expand.c}"
mkdir -p build/match/t3d
cp "$SRC" "build/match/t3d/v_$1.c"
echo "made build/match/t3d/v_$1.c from $SRC"
