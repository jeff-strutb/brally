"""capuc.py VA draft.c FROM TO : read-only; uopt's output ucode (ugen input) for draft source lines FROM..TO (draft-relative)."""
import glob, os, re, subprocess, sys, tempfile
sys.path.insert(0, 'tools/tgrally'); sys.path.insert(0, 'build/tgrally/ext/n64-decomp-workbench/src')
import n64t3 as T, n64build as B
from decomp_workbench.ucode import parse_ucode
CAP = os.environ.get('CAPDIR', 'build/tgrally/n64/capture')
va = int(sys.argv[1], 16); path, name, _ = T.source_of(va)
tree = open(path).read(); body = T.function_text(tree, name)
nb = open(sys.argv[2]).read().strip('\n'); src = tree.replace(body, nb)
off = src[:src.index(nb)].count('\n')
lo, hi = int(sys.argv[3]) + off, int(sys.argv[4]) + off
d = tempfile.mkdtemp(); p = os.path.join(d, 'x.c'); open(p, 'w').write(src)
before = set(glob.glob(CAP + '/captures/*'))
subprocess.run([CAP + '/toolchain/cc'] + B.BASE_FLAGS + B.cflags_for(src) + ['-o', 'x.o', 'x.c'], cwd=d, capture_output=True)
new = sorted(set(glob.glob(CAP + '/captures/*')) - before)
f = sorted(glob.glob(new[-1] + '/' + os.environ.get('UCFILE', 'before-8-*')))[0]
recs = parse_ucode(open(f, 'rb').read())
on = False
for r in recs:
    if r.name == 'loc':
        on = lo <= r.words[1] <= hi
    if on: print(r.index, r.name, 'dt', r.dtype, 'mt', r.mtype, r.detail)
