#!/bin/sh
# Build the instrumented IDO 5.3 that tools/tgrally/n64alloc.py drives.
#
# WHAT IT DOES: recompiles IDO 5.3 from the original IRIX binaries with
# ido-static-recomp (pinned commit), applies the n64-decomp-workbench uopt
# profiles (globalcolor + alias: allocator decision records and CDX_FORCE
# register forcing) and ugen profiles (temp free-list and emit-order records),
# links them with a libc shim that implements ecvt/fcvt (so uopt's
# -Wo,-zdbug:2 listing works), and gates the result: with tracing off, every
# N64 source must compile to the same sections, relocations and symbols as
# tools/ido53.  Everything lands in build/tgrally/ext (gitignored).
#
#     sh tools/tgrally/build_ido_trace.sh
#
# Output: build/tgrally/ext/instr/out/cc (a full IDO 5.3 driver with traced passes).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
EXT=$ROOT/build/tgrally/ext
RECOMP_COMMIT=9c242adc890beef098020149d9554f48208f699d   # uopt.c sha256 b0058f15... (the workbench profile pin)
WB_COMMIT=3f58a68db5d4cf343c76b8361dfc1786c501e521
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
mkdir -p "$EXT"
cd "$EXT"

[ -d ido-static-recomp ] || git clone -q https://github.com/decompals/ido-static-recomp.git
(cd ido-static-recomp && git fetch -q --depth 1 origin $RECOMP_COMMIT && git checkout -q $RECOMP_COMMIT -- . )
[ -d n64-decomp-workbench ] || git clone -q https://github.com/akratch/n64-decomp-workbench.git
(cd n64-decomp-workbench && git fetch -q --depth 1 origin $WB_COMMIT && git checkout -q $WB_COMMIT)
[ -x wbvenv/bin/decomp-workbench ] || { python3 -m venv wbvenv && wbvenv/bin/pip install -q -e n64-decomp-workbench; }

cd ido-static-recomp
make -C tools/rabbitizer static CC=gcc CXX=g++ DEBUG=1 -j8 >/dev/null
make -j8 VERSION=5.3 RELEASE=1 >/dev/null
echo "b0058f1559441c1a194d649271eb43b8637ec255682cfdd629031340b915b13f  build/5.3/uopt.c" | shasum -a 256 -c -

mkdir -p ../instr
rm -f ../instr/uopt.c ../instr/ugen.c
../wbvenv/bin/decomp-workbench instrument-uopt build/5.3/uopt.c ../instr/uopt.c --profile alias --profile globalcolor
../wbvenv/bin/decomp-workbench instrument-ugen --emit-provenance build/5.3/ugen.c ../instr/ugen.c
python3 "$ROOT/tools/tgrally/patches/uopt_saveocc.py" ../instr/uopt.c     # per-occurrence save records

# the libc shim with ecvt/fcvt, built aside so the stock build stays stock
cp libc_impl.c ../instr/libc_impl.c
(cd ../instr && patch -s -p1 < "$ROOT/tools/tgrally/patches/ido-static-recomp-ecvt.patch")
F="-std=c11 -Os -fno-strict-aliasing -I. -DPACKAGE_VERSION=\"trace\" -DDATETIME=\"trace\""
rm -rf ../instr/out && cp -R build/5.3/out ../instr/out
eval gcc -c $F -DIDO53 -Wno-deprecated-declarations -o ../instr/libc_impl.o ../instr/libc_impl.c
for p in uopt ugen; do
    eval gcc -c $F -o ../instr/$p.o ../instr/$p.c
    eval gcc $F -o ../instr/out/$p ../instr/$p.o ../instr/libc_impl.o build/5.3/version_info.o -lm
done
../wbvenv/bin/decomp-workbench check-drop-in ../instr/out/uopt ../instr/out/ugen

# identity gate: tracing off, the traced compiler must equal tools/ido53 on every source
cd "$ROOT"
.venv/bin/python - <<'EOF'
import os, sys, hashlib, subprocess
sys.path.insert(0, 'tools/tgrally')
import n64build as B
def digest(cc, f):
    o = f.replace('/', '_') + cc.replace('/', '_') + '.o'
    o = os.path.join('build/tgrally/ext/instr', 'gate_' + hashlib.sha1(o.encode()).hexdigest()[:12] + '.o')
    flags = B.cflags_for(open(f).read())
    base = [x for x in B.BASE_FLAGS if x != '-mips2'] if '-mips3' in flags else B.BASE_FLAGS
    subprocess.run([cc] + base + flags + ['-o', o, f], check=True, capture_output=True)
    obj = B.Obj(o); os.unlink(o)
    h = hashlib.sha256()
    for s in ('.text', '.rodata', '.data', '.sdata', '.bss'):
        i, blob = obj.sec(s)
        h.update(s.encode() + bytes(blob or b'') + (repr(obj.rels.get(i, [])).encode() if i is not None else b''))
    h.update(repr([(x['name'], x['shndx'], x.get('value')) for x in obj.syms]).encode())
    return h.hexdigest()
from concurrent.futures import ThreadPoolExecutor
srcs = [f for f in B.all_sources() if f.endswith('.c')]
def both(f):
    return f, digest(B.CC, f) == digest('build/tgrally/ext/instr/out/cc', f)
bad = [f for f, ok in ThreadPoolExecutor(8).map(both, srcs) if not ok]
print('identity gate: %d of %d sources identical' % (len(srcs) - len(bad), len(srcs)))
sys.exit(1 if bad else 0)
EOF
echo "built build/tgrally/ext/instr/out/cc"
