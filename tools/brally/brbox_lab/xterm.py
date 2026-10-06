import itertools, os, re, subprocess, sys, concurrent.futures as cf
sys.path.insert(0, 'tools'); sys.path.insert(0, 'build/brally/win32/brbox')
from match_diff import parse_coff_obj
import xdiff
SRC, SYM, VA, OPT = sys.argv[1:5]
base = open(SRC).read(); orig = open('build/brally/win32/match/orig/%s.bin' % VA, 'rb').read()
COLS = ['DAT_105d1760', 'DAT_105d1764', 'DAT_105d1768', 'DAT_105d176c']
def gen(cast, order, form):
    t = base
    for c in COLS:
        colx = c if form == 'ext' else '(*(float *)0x%s)' % c[4:]
        xv = {'none': 'pSrc->x', 'double': '(double)pSrc->x', 'float': '(float)pSrc->x'}[cast]
        term = '%s * %s' % (xv, colx) if order == 0 else '%s * %s' % (colx, xv)
        t = t.replace('(double)pSrc->x * %s' % c, term)
    return t
def score(cfg, idx):
    t = gen(*cfg)
    d = 'build/brally/win32/match/t3d/xt/%d' % idx; os.makedirs(d, exist_ok=True)
    src = os.path.join(d, os.path.basename(SRC)); open(src, 'w').write(t)
    obj = os.path.join(d, 'o.obj')
    if os.path.exists(obj): os.unlink(obj)
    subprocess.run(['sh', 'tools/toolchains/wine.sh', 'tools/toolchains/msvc5/bin/cl.exe', '/nologo'] + OPT.split() + ['/W3', '/I', os.path.dirname(SRC), '/I', 'include', '/I', 'tools/toolchains/msvc5-compat', '/I', 'tools/toolchains/msvc5/include', '/DBR_MATCHING_BUILD', '/c', src, '/Fo' + obj.replace('/', '\\')], capture_output=True)
    if not os.path.exists(obj): return 9999, cfg
    return xdiff.score(orig, parse_coff_obj(obj)[SYM][0].rstrip(b'\x90')), cfg
cfgs = list(itertools.product(['none', 'double', 'float'], [0, 1], ['ext', 'abs']))
with cf.ThreadPoolExecutor(12) as ex:
    res = sorted(ex.map(lambda a: score(a[1], a[0]), enumerate(cfgs)))
for r in res: print(r)
open('build/brally/win32/match/t3d/xt/best.c', 'w').write(gen(*res[0][1]))
