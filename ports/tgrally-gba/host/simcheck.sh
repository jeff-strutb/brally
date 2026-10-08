#!/bin/sh
# Build host/simcheck twice -- the float transcription (simcheck) and the fixed
# point the GBA runs (simcheck_fx) -- and run both against tools/simref.py's DIR.
#   host/simcheck.sh [DIR [free]]
set -e
cd "$(dirname "$0")/.."
OUT=../../build/tgrally/gba
SIM="sim/geom.c sim/rigid.c sim/coll.c sim/car.c sim/camera.c sim/simload.c"
cc -O1 -g -DFX_FLOAT -ffp-contract=off -Wall -Isim host/simcheck.c $SIM -o $OUT/simcheck
cc -O1 -g -ffp-contract=off -Wall -Isim host/simcheck.c $SIM sim/fxmath.c -o $OUT/simcheck_fx
DIR=${1:-$OUT/simref}
echo "float:"; $OUT/simcheck $DIR $2 | tail -${TAILN:-1}
echo "fixed:"; $OUT/simcheck_fx $DIR $2 | tail -${TAILN:-1} || true
