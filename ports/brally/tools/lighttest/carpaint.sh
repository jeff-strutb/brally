#!/bin/sh
# carpaint.sh: car paint as a clear coat.  The capture's car classification
# (carpaint_test.c) here; the layered BRDF's furnace, its Fresnel split and
# GPU against double run in maths.sh (lighttest.m: test_carpaint), and the
# reflection pass's coat in refl.sh's offline compiles.
set -e
cd "$(dirname "$0")/../../../.."
OUT=build/brally/lighttest
T=ports/brally/tools/lighttest
P=ports/brally/platform
mkdir -p "$OUT"
clang -O2 -std=gnu11 -Wall -ffp-contract=off -I"$P/light" "$T/carpaint_test.c" -o "$OUT/carpaint_test" -lm
"$OUT/carpaint_test"
