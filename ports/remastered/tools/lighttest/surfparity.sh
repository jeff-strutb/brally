#!/bin/sh
# surfparity.sh DUMPDIR: the Remastered textures' surface maps, light_surf.c against its
# reference ports/remastered/tools/texregion.py, on every texture under DUMPDIR (PAMs a run
# with BR_LIGHT_TEXOUT=DUMPDIR wrote).  docs/brally/remaster/textures/DESIGN.md section 5.
set -e
cd "$(dirname "$0")/../../../.."
ports/remastered/tree.sh make build/remastered/tree || exit 2   # the sources as Remastered builds them
OUT=build/brally/lighttest/surf
L=build/remastered/tree/ports/brally/platform/light
mkdir -p "$OUT"
clang -O2 -std=gnu11 -Wall -I"$L" "$L/light_surf.c" ports/remastered/tools/lighttest/surftest.c -o "$OUT/surftest" -lm
PYTHONPATH=ports/remastered/tools .venv/bin/python ports/remastered/tools/lighttest/surfparity.py "$1" "$OUT/surftest" "$OUT"
