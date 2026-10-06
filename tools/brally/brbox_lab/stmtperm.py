"""stmtperm.py SRC SYM VA OPT START_MARK N  -- try every order of the N statements
that start at the line containing START_MARK; score by instruction diff."""
import itertools, os, re, subprocess, sys, difflib, concurrent.futures as cf
sys.path.insert(0, 'tools'); sys.path.insert(0, 'build/brally/win32/brbox')
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
SRC, SYM, VA, OPT, MARK, N = sys.argv[1:7]
N = int(N)
base = open(SRC).read().split('\n')
k0 = next(i for i, l in enumerate(base) if MARK in l)
stmts = base[k0:k0 + N]
orig = open('build/brally/win32/match/orig/%s.bin' % VA, 'rb').read()
MD = Cs(CS_ARCH_X86, CS_MODE_32)
NRM = lambda s: re.sub(r'0x[0-9a-f]{3,}', 'A', s)
O = ['%s %s' % (i.mnemonic, NRM(i.op_str)) for i in MD.disasm(orig, 0)]

def score(perm, idx):
    L = base[:k0] + [stmts[i] for i in perm] + base[k0 + N:]
    d = 'build/brally/win32/match/t3d/sp/%d' % idx
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, os.path.basename(SRC))
    open(src, 'w').write('\n'.join(L))
    obj = os.path.join(d, 'o.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    subprocess.run(['sh', 'tools/toolchains/wine.sh', 'tools/toolchains/msvc5/bin/cl.exe', '/nologo'] + OPT.split() +
                   ['/W3', '/I', os.path.dirname(SRC), '/I', 'include', '/I', 'tools/toolchains/msvc5-compat',
                    '/I', 'tools/toolchains/msvc5/include', '/DBR_MATCHING_BUILD', '/c', src, '/Fo' + obj.replace('/', '\\')],
                   capture_output=True)
    if not os.path.exists(obj):
        return (9999,9999), perm
    code = parse_coff_obj(obj)[SYM][0].rstrip(b'\x90')
    b = ['%s %s' % (i.mnemonic, NRM(i.op_str)) for i in MD.disasm(code, 0)]
    sm = difflib.SequenceMatcher(None, O, b, autojunk=False)
    import xdiff; return xdiff.split_score(orig, code), perm

perms = list(itertools.permutations(range(N)))
with cf.ThreadPoolExecutor(12) as ex:
    res = list(ex.map(lambda a: score(a[1], a[0]), enumerate(perms)))
res.sort(); import collections; print(collections.Counter(s for s,p in res))
for s, p in res[:8]:
    print(s, p)
best = res[0][1]
open('build/brally/win32/match/t3d/sp/best.txt', 'w').write('\n'.join(stmts[i] for i in best))
