"""variants.py SRC SYM VA OPT PY -- PY is a python file defining VARIANTS = {name: fn(text)->text}."""
import os, subprocess, sys, concurrent.futures as cf
sys.path.insert(0, 'tools'); sys.path.insert(0, 'build/brally/win32/brbox')
from match_diff import parse_coff_obj
import xdiff
SRC, SYM, VA, OPT, PY = sys.argv[1:6]
base = open(SRC).read(); orig = open('build/brally/win32/match/orig/%s.bin' % VA, 'rb').read()
ns = {}; exec(open(PY).read(), ns); V = ns['VARIANTS']
def run(item):
    i, (name, fn) = item
    t = fn(base)
    d = 'build/brally/win32/match/t3d/vv/%d' % i; os.makedirs(d, exist_ok=True)
    src = os.path.join(d, os.path.basename(SRC)); open(src, 'w').write(t)
    obj = os.path.join(d, 'o.obj')
    if os.path.exists(obj): os.unlink(obj)
    subprocess.run(['sh', 'tools/toolchains/wine.sh', 'tools/toolchains/msvc5/bin/cl.exe', '/nologo'] + OPT.split() + ['/W3', '/I', os.path.dirname(SRC), '/I', 'include', '/I', 'tools/toolchains/msvc5-compat', '/I', 'tools/toolchains/msvc5/include', '/DBR_MATCHING_BUILD', '/c', src, '/Fo' + obj.replace('/', '\\')], capture_output=True)
    if not os.path.exists(obj): return 9999, name, d
    c = parse_coff_obj(obj)[SYM][0].rstrip(b'\x90')
    return (xdiff.score(orig, c), xdiff.split_score(orig, c)), name, d
with cf.ThreadPoolExecutor(12) as ex:
    res = sorted(ex.map(run, enumerate(V.items())))
for s, n, d in res: print(s, n, d)
