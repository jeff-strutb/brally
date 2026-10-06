"""flipsearch.py VA [--file body.c] [--max 12] [--only LINE,LINE..]

Commutative operand-order search.  ugen runs every function twice and the
second pass starts from the float free list the first pass left, and a
memory operand's position decides load order, so the written order of each
`a * b` / `a + b` can rename every temporary.  Finds every simple binary
term (two plain operands: identifiers, members, constant-indexed elements)
in the function body and grades flips: exhaustive up to --max sites, then
greedy (single flips, keep the best, repeat).  Writes the best body to
build/tgrally/n64/search/<VA>/flip_best.c.  Run from the repo root."""
import itertools
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, 'tools/tgrally')
import n64search as S  # noqa: E402
import n64t3 as T  # noqa: E402

IDX = r'\[(?:[^\[\]]|\[[^\[\]]*\])+\]'
PAREN = r'\((?:[^()]|\([^()]*\))*\)'
ATOM = (r'(?:(?:\(\w+(?: \*)?\))?-?[A-Za-z_]\w*(?:(?:->|\.)\w+|' + IDX + r')*'
        r'|0x[0-9A-Fa-f]+|\d+(?:\.\d*)?f?|' + PAREN + r')')
OPS = r'[*+&|^]|==|!='
PREC = {'*': 10, '/': 10, '%': 10, '+': 9, '-': 9, '<<': 8, '>>': 8, '<': 7, '>': 7, '<=': 7, '>=': 7,
        '==': 6, '!=': 6, '&': 5, '^': 4, '|': 3, '&&': 2, '||': 1, '=': 0}
TERM = re.compile(r'(?<![\w\]\)\.>*/+<-])(' + ATOM + r') (' + OPS + r') (' + ATOM + r')(?![\w\[(.]| ?[*/])')

args = sys.argv[1:]
va = int(args[0], 16)
mx = int(args[args.index('--max') + 1]) if '--max' in args else 12
path, name, _ = T.source_of(va)
tree = open(path).read()
body = T.function_text(tree, name)
if '--file' in args:
    body0 = open(args[args.index('--file') + 1]).read().strip('\n')
else:
    body0 = body
work = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tw')
os.makedirs(work, exist_ok=True)

# sites: (start, end, a, op, b) over body0, statements only (skip decls/for headers)
sites = []
for m in TERM.finditer(body0):
    line_start = body0.rfind('\n', 0, m.start()) + 1
    line = body0[line_start:body0.find('\n', m.start())]
    if line.lstrip().startswith(('for', 'float', 'int', 'unsigned', 'short', 'char', 'double', '/*', '*')):
        continue
    if '=' not in line and 'return' not in line and 'if' not in line:
        continue
    op = m.group(2)
    before = body0[:m.start()].rstrip()
    after = body0[m.end():].lstrip()
    pt = re.search(r'(\|\||&&|==|!=|<=|>=|<<|>>|[-+*/%&|^<>=(,?:!~]|return|if|\{|;)$', before)
    nt = re.match(r'(\|\||&&|==|!=|<=|>=|<<|>>|[-+*/%&|^<>)=,?:;\]])', after)
    pv = pt.group(1) if pt else None
    nv = nt.group(1) if nt else None
    if pv is not None and pv not in ('=', '(', ',', '?', ':', 'return', 'if', '{', ';', '!') \
            and not (PREC.get(pv, 99) < PREC[op]):
        continue
    if pv == '!' and op not in ('*',):
        continue
    if nv is not None and nv not in (')', ';', ',', '?', ':', ']') and not (PREC.get(nv, 99) < PREC[op] or (nv == op and op in '*+')):
        continue
    sites.append((m.start(), m.end(), m.group(1), op, m.group(3)))
print('%d sites' % len(sites))
for s in sites:
    print('  %s %s %s' % (s[2], s[3], s[4]))


def build(bits):
    out, pos = [], 0
    for on, (st, en, a, op, b) in zip(bits, sites):
        out.append(body0[pos:st])
        out.append('%s %s %s' % (b, op, a) if on else body0[st:en])
        pos = en
    out.append(body0[pos:])
    return ''.join(out)


def grade(bits):
    nb = build(bits)
    try:
        g = S.grade(tree.replace(body, nb), va, name, work)
    except Exception:
        g = None
    if not isinstance(g, tuple):
        return (10 ** 9,), bits
    return (g[2], g[0], g[1]), bits


def run(cands):
    with ThreadPoolExecutor(12) as ex:
        return sorted(ex.map(grade, cands))


n = len(sites)
base = grade((0,) * n)
print('base', base[0])
if n <= mx:
    res = run(list(itertools.product((0, 1), repeat=n)))
    best = res[0]
else:
    best = base
    while True:
        cands = []
        for k in range(n):
            b = list(best[1])
            b[k] ^= 1
            cands.append(tuple(b))
        res = run(cands)
        if res[0][0] >= best[0]:
            break
        best = res[0]
        print('  ->', best[0])
print('best (T4, aligned, blind)', best[0], ''.join(map(str, best[1])))
d = os.path.join('build/tgrally/n64/search', '%08X' % va)
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'flip_best.c'), 'w').write(build(best[1]) + '\n')
print('->', os.path.join(d, 'flip_best.c'))
