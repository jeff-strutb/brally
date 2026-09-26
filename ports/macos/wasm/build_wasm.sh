#!/bin/sh
# The macOS full-boot build, 32-bit lane (see w2c.py for why wasm32).
#
#   1. every src/core TU, Windows-build arm (-DBR_MATCHING_BUILD), -> wasm32
#      object.  The decomp source is never edited: MSVC-isms clang rejects are
#      handled here with flags, the port include overlay, and per-file
#      pre-includes (ports/macos/wasm/pre/<basename>.h).
#   2. symmap.py: every referenced symbol -> its original address
#   3. w2c.py: objects -> C, linked against the original layout
#   4. native clang: generated C + runtime + host layer -> build/wasm/brally
#
# Toolchain: emscripten's LLVM (clang with the wasm backend, no emcc driver).
set -e
cd "$(dirname "$0")/../../.."
ROOT=$(pwd)
LLVM=${BR_WASM_LLVM:-/opt/homebrew/opt/emscripten/libexec/llvm/bin}
[ -x "$LLVM/clang" ] || { echo "build_wasm: no wasm-capable clang at $LLVM" >&2; exit 1; }
OUT=build/wasm
mkdir -p $OUT/obj $OUT/c $OUT/inc
JOBS=${JOBS:-14}

python3 ports/macos/wasm/gen_inc.py tools/msvc5/include $OUT/inc

export LLVM OUT

# Only TUs that carry game functions: @implements (C) or the C++ lane.
find src/core -name '*.c' -o -name '*.cpp' | sort | while read f; do
    grep -q '@implements' "$f" && echo "$f"
done > $OUT/tus.txt
rm -f $OUT/obj/*.o
xargs -P $JOBS -n 1 ports/macos/wasm/wcc.sh < $OUT/tus.txt | sort > $OUT/compile.txt
grep '^FAIL' $OUT/compile.txt > $OUT/fails.txt || true
echo "wasm32: $(ls $OUT/obj/*.o | wc -l | tr -d ' ') of $(wc -l < $OUT/tus.txt | tr -d ' ') TUs compiled ($(grep -c '^LAX' $OUT/compile.txt) via msvc_lax, $(wc -l < $OUT/fails.txt | tr -d ' ') failed: $OUT/fails.txt)"

.venv/bin/python ports/macos/wasm/symmap.py
python3 - <<'EOF'
import csv
rows = {}
for r in csv.DictReader(open('build/match/report.csv')):
    import os
    rows[r['name']] = os.path.splitext(os.path.basename(r['file']))[0]
with open('build/wasm/owners.csv', 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['name', 'base'])
    for k, v in sorted(rows.items()): w.writerow([k, v])
EOF
rm -f $OUT/c/*.c
python3 ports/macos/wasm/w2c.py --out $OUT/c --symmap $OUT/symmap.csv \
    --owners $OUT/owners.csv $OUT/obj/*.o

# ---- native: generated C + runtime + host layer -> build/wasm/brally ------
mkdir -p $OUT/nat
# the runtime header is in every generated file: a change rebuilds them all
[ $OUT/nat/.stamp -nt ports/macos/wasm/rt/w2c_rt.h ] || rm -f $OUT/nat/*.o
touch $OUT/nat/.stamp
# drop objects of generated files that no longer exist (object ids shift)
for o in $OUT/nat/o*.o $OUT/nat/w2c_*.o; do
    [ -f "$o" ] || continue
    c=$OUT/c/$(basename "$o" .o).c
    [ -f "$c" ] || rm -f "$o"
done
NCF="-std=gnu11 -O2 -g -w -Iports/macos/wasm/rt -I$OUT/c -Iports/macos/wasm/host"
ls $OUT/c/*.c | xargs -P $JOBS -I{} sh -c \
  'n=$(basename {} .c); [ $OUT/nat/$n.o -nt {} ] || clang '"$NCF"' -c {} -o $OUT/nat/$n.o || echo "NATIVE FAIL $n"'
for f in ports/macos/wasm/rt/w2c_rt.c ports/macos/wasm/host/*.c; do
    clang $NCF -c $f -o $OUT/nat/rt_$(basename $f .c).o
done
for f in ports/macos/wasm/host/*.m; do
    clang $NCF -fobjc-arc -c $f -o $OUT/nat/rt_$(basename $f .m).o
done
clang $OUT/nat/*.o -framework Cocoa -framework Metal -framework QuartzCore -o $OUT/brally
echo "built: $OUT/brally"
