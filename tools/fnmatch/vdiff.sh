#!/bin/sh
# vdiff.sh <tag> [orig.bin] [mode] [n] -- compile the variant and score it
cd "$(git rev-parse --show-toplevel)"
. .venv/bin/activate 2>/dev/null
ORIG="${2:-build/match/orig/0x100250D0.bin}"
OBJ="build/match/t3d/v_$1.obj"; rm -f "$OBJ"
sh tools/wine.sh tools/msvc5/bin/cl.exe /nologo /O2 /W3 /I include \
  /I tools/msvc5-compat /I tools/msvc5/include /DBR_MATCHING_BUILD \
  /c "build/match/t3d/v_$1.c" "/Fobuild\\match\\t3d\\v_$1.obj" 2>&1 \
  | grep -iE "error" | head -10
[ -f "$OBJ" ] || { echo "COMPILE FAILED"; exit 1; }
BR_ORIG="$ORIG" python3 tools/fnmatch/score.py "$OBJ"
[ -n "$3" ] && BR_ORIG="$ORIG" python3 tools/fnmatch/mdiff2.py "$OBJ" BrTex3dExpand "$3" "${4:-25}"
exit 0
