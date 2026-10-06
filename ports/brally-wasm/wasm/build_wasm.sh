#!/bin/sh
# The macOS full-boot build, 32-bit lane (see w2c.py for why wasm32).
#
#   1. every src/brally/core TU, Windows-build arm (-DBR_MATCHING_BUILD), -> wasm32
#      object.  The decomp source is never edited: MSVC-isms clang rejects are
#      handled here with flags, the port include overlay, and per-file
#      pre-includes (ports/brally-wasm/wasm/pre/<basename>.h).
#   2. symmap.py: every referenced symbol -> its original address
#   3. w2c.py: objects -> C, linked against the original layout; a native
#      body in ports/brally-wasm/wasm/native/ tagged `@replaces 0xVA Name` takes
#      that function's direct calls and dispatch slot
#   4. native clang: generated C + runtime + host layer + native overrides
#      -> build/brally/wasm32/brally
#
# Toolchain: emscripten's LLVM (clang with the wasm backend, no emcc driver).
set -e
cd "$(dirname "$0")/../../.."
ROOT=$(pwd)
LLVM=${BR_WASM_LLVM:-/opt/homebrew/opt/emscripten/libexec/llvm/bin}
[ -x "$LLVM/clang" ] || { echo "build_wasm: no wasm-capable clang at $LLVM" >&2; exit 1; }
# BR_TRACE_BUILD=1: the traced build (line tables, every load and store
# reported to rt/w2c_trace.c) in build/brally/wasm32-trace; the address maps stay
# build/brally/wasm32's.
W=build/brally/wasm32
OUT=$W
TRACEFLAG=
if [ -n "$BR_TRACE_BUILD" ]; then
    OUT=build/brally/wasm32-trace; TRACEFLAG=--trace; BR_WASM_G=1; export BR_WASM_G
fi
mkdir -p $OUT/obj $OUT/c $OUT/inc
JOBS=${JOBS:-14}

python3 ports/brally-wasm/wasm/gen_inc.py tools/toolchains/msvc5/include $OUT/inc

export LLVM OUT

# Only TUs that carry game functions: @implements (C) or the C++ lane.
# A leading '_' is a scratch file (a permuter's candidate), never the game.
find src/brally/core \( -name '*.c' -o -name '*.cpp' \) ! -name '_*' | sort | while read f; do
    grep -q '@implements' "$f" && echo "$f"
done > $OUT/tus.txt
rm -f $OUT/obj/*.o
xargs -P $JOBS -n 1 ports/brally-wasm/wasm/wcc.sh < $OUT/tus.txt | sort > $OUT/compile.txt
grep '^FAIL' $OUT/compile.txt > $OUT/fails.txt || true
echo "wasm32: $(ls $OUT/obj/*.o | wc -l | tr -d ' ') of $(wc -l < $OUT/tus.txt | tr -d ' ') TUs compiled ($(grep -c '^LAX' $OUT/compile.txt) via msvc_lax, $(wc -l < $OUT/fails.txt | tr -d ' ') failed: $OUT/fails.txt)"

# The verified T3 build's placement decides every function's address. It is
# derived from src/brally/ and config/brally/, so rebuild it whenever either is newer --
# a stale copy names files that were refiled since and silently gives their
# functions synthetic addresses, and the original's data tables then call
# into nothing.
if [ -z "$BR_TRACE_BUILD" ]; then
if [ ! -f $OUT/placement.csv ] || [ -n "$(find src/brally config/brally -newer $OUT/placement.csv -print -quit)" ]; then
    .venv/bin/python ports/brally-wasm/wasm/t3manifest.py
fi
.venv/bin/python ports/brally-wasm/wasm/symmap.py
python3 - <<'EOF'
import csv
rows = {}
for r in csv.DictReader(open('build/brally/win32/match/report.csv')):
    import os
    rows[r['name']] = os.path.splitext(os.path.basename(r['file']))[0]
with open('build/brally/wasm32/owners.csv', 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['name', 'base'])
    for k, v in sorted(rows.items()): w.writerow([k, v])
EOF
fi
rm -f $OUT/c/*.c
python3 ports/brally-wasm/wasm/w2c.py $TRACEFLAG --out $OUT/c --symmap $W/symmap.csv \
    --owners $W/owners.csv --native ports/brally-wasm/wasm/native $OUT/obj/*.o

# ---- native: generated C + runtime + host layer -> build/brally/wasm32/brally ------
mkdir -p $OUT/nat
# the runtime header is in every generated file: a change rebuilds them all
[ $OUT/nat/.stamp -nt ports/brally-wasm/wasm/rt/w2c_rt.h ] || rm -f $OUT/nat/*.o
touch $OUT/nat/.stamp
# drop objects of generated files that no longer exist (object ids shift)
for o in $OUT/nat/o*.o $OUT/nat/w2c_*.o; do
    [ -f "$o" ] || continue
    c=$OUT/c/$(basename "$o" .o).c
    [ -f "$c" ] || rm -f "$o"
done
# libopenmpt plays the N64 soundtrack (native/music.m); linked statically,
# with the codecs it was built against, so the app carries no library.
BREW=${BR_BREW:-/opt/homebrew/opt}
MPT_LIBS="$BREW/libopenmpt/lib/libopenmpt.a $BREW/mpg123/lib/libmpg123.a $BREW/libvorbis/lib/libvorbisfile.a $BREW/libvorbis/lib/libvorbis.a $BREW/libogg/lib/libogg.a -lz -lc++"
TDEF=; [ -n "$BR_TRACE_BUILD" ] && TDEF=-DBR_TRACE
NCF="$TDEF -std=gnu11 -O2 -g -w -Iports/brally-wasm/wasm/rt -I$OUT/c -Iports/brally-wasm/wasm/host -I$BREW/libopenmpt/include"
# the flags reach sh through the environment: BSD xargs caps an -I command
# at 255 bytes
export NCF
ls $OUT/c/*.c | xargs -P $JOBS -I{} sh -c \
  'n=$(basename {} .c); [ $OUT/nat/$n.o -nt {} ] || clang $NCF -c {} -o $OUT/nat/$n.o || echo "NATIVE FAIL $n"'
TRT=; [ -n "$BR_TRACE_BUILD" ] && TRT=ports/brally-wasm/wasm/rt/w2c_trace.c
for f in ports/brally-wasm/wasm/rt/w2c_rt.c $TRT ports/brally-wasm/wasm/host/*.c; do
    clang $NCF -c $f -o $OUT/nat/rt_$(basename $f .c).o
done
for f in ports/brally-wasm/wasm/host/*.m; do
    clang $NCF -fobjc-arc -c $f -o $OUT/nat/rt_$(basename $f .m).o
done
# native overrides: a removed file must not stay linked
rm -f $OUT/nat/nv_*.o
for f in ports/brally-wasm/wasm/native/*.c ports/brally-wasm/wasm/native/*.m; do
    [ -f "$f" ] || continue
    case $f in *.m) arc=-fobjc-arc ;; *) arc= ;; esac
    clang $NCF $arc -c $f -o $OUT/nat/nv_$(basename $f | tr . _).o
done
clang $OUT/nat/*.o -framework Cocoa -framework Metal -framework MetalFX -framework QuartzCore -framework GameController -framework AVFoundation $MPT_LIBS -o $OUT/brally
echo "built: $OUT/brally"
