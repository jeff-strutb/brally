"""Greedy search over the light-block spellings of a lit vertex handler.
usage: lightsearch.py SRC SYM VA OPT"""
import itertools, os, re, subprocess, sys, concurrent.futures as cf
sys.path.insert(0, 'tools'); sys.path.insert(0, 'build/brally/win32/brbox')
from match_diff import parse_coff_obj
SRC, SYM, VA, OPT = sys.argv[1:5]
base = open(SRC).read()
orig = open('build/brally/win32/match/orig/%s.bin' % VA, 'rb').read()
ROWS = [  # (target, [(m index, f var)] in the association: (t0 + t1) + t2)
    ('DAT_105ce21c', [(1, 'fy'), (2, 'fz'), (0, 'fx')]),
    ('DAT_105ce220', [(4, 'fx'), (6, 'fz'), (5, 'fy')]),
    ('DAT_105ce224', [(10, 'fz'), (9, 'fy'), (8, 'fx')]),
]
rowre = {tg: re.compile(r'            %s = \(\([^\n]*/ DAT_10077420;\n' % tg) for tg, _ in ROWS}

def row_text(tg, terms, swap_inner, flips):
    ts = []
    for (mi, fv), fl in zip(terms, flips):
        ts.append('%s * m[%d]' % (fv, mi) if fl else 'm[%d] * %s' % (mi, fv))
    a, b, c = ts
    if swap_inner:
        a, b = b, a
    return '            %s = ((%s + %s) + %s) / DAT_10077420;\n' % (tg, a, b, c)

from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import difflib
_MD = Cs(CS_ARCH_X86, CS_MODE_32)
_N = lambda s: re.sub(r'0x[0-9a-f]{3,}', 'A', s)
_O = ['%s %s' % (i.mnemonic, _N(i.op_str)) for i in _MD.disasm(orig, 0)]


def insn_diff(code):
    b = ['%s %s' % (i.mnemonic, _N(i.op_str)) for i in _MD.disasm(code, 0)]
    sm = difflib.SequenceMatcher(None, _O, b, autojunk=False)
    return sum(max(i2 - i1, j2 - j1) for op, i1, i2, j1, j2 in sm.get_opcodes() if op != 'equal')


def build(cfg, idx):
    t = base
    for (tg, terms), (sw, fl) in zip(ROWS, cfg['rows']):
        t = rowre[tg].sub(lambda m: row_text(tg, terms, sw, fl), t, count=1)
    t = t.replace('    float fy, fz, fx;\n', '    float %s;\n' % ', '.join(cfg['decl']), 1)
    d = 'build/brally/win32/match/t3d/ls/%d' % idx
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, os.path.basename(SRC))
    open(src, 'w').write(t)
    obj = os.path.join(d, 'o.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    subprocess.run(['sh', 'tools/toolchains/wine.sh', 'tools/toolchains/msvc5/bin/cl.exe', '/nologo'] + OPT.split() +
                   ['/W3', '/I', os.path.dirname(SRC), '/I', 'include', '/I', 'tools/toolchains/msvc5-compat',
                    '/I', 'tools/toolchains/msvc5/include', '/DBR_MATCHING_BUILD', '/c', src, '/Fo' + obj.replace('/', '\\')],
                   capture_output=True)
    if not os.path.exists(obj):
        return 99999, t
    code, rel = parse_coff_obj(obj)[SYM][:2]
    code = code.rstrip(b'\x90')
    import xdiff; return sum(xdiff.split_score(orig, code)), t

cfg = {'rows': [(False, (False, False, False))] * 3, 'decl': ['fy', 'fz', 'fx']}
best, besttext = build(cfg, 0)
print('start', best, flush=True)
k = 1
for rnd in range(2):
    for r in range(3):
        cands = []
        for sw in (False, True):
            for fl in itertools.product((False, True), repeat=3):
                c = {'rows': list(cfg['rows']), 'decl': cfg['decl']}
                c['rows'][r] = (sw, fl)
                cands.append(c)
        with cf.ThreadPoolExecutor(12) as ex:
            res = list(ex.map(lambda a: (build(a[1], k + a[0]), a[1]), enumerate(cands)))
        k += len(cands)
        (n, t), c = min(res, key=lambda x: x[0][0])
        if n < best:
            best, besttext, cfg = n, t, c
        print('round', rnd, 'row', r, 'best', best, cfg['rows'][r], flush=True)
    decls = [list(p) for p in itertools.permutations(['fx', 'fy', 'fz'])]
    cands = [{'rows': cfg['rows'], 'decl': d} for d in decls]
    with cf.ThreadPoolExecutor(12) as ex:
        res = list(ex.map(lambda a: (build(a[1], k + a[0]), a[1]), enumerate(cands)))
    k += len(cands)
    (n, t), c = min(res, key=lambda x: x[0][0])
    if n < best:
        best, besttext, cfg = n, t, c
    print('round', rnd, 'decl best', best, cfg['decl'], flush=True)
open('build/brally/win32/match/t3d/ls/best.c', 'w').write(besttext)
print('FINAL', best, cfg)
