#!/bin/sh
# vmake.sh <tag> [src.c] -- private variant copy for one prober
cd "$(git rev-parse --show-toplevel)"
SRC="${2:-src/brally/core/drawing/br_tex3d_expand.c}"
mkdir -p build/brally/win32/match/t3d
cp "$SRC" "build/brally/win32/match/t3d/v_$1.c"
echo "made build/brally/win32/match/t3d/v_$1.c from $SRC"
