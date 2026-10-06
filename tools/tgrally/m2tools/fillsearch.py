"""fillsearch.py VA [--file body.c] : frame-filler search.

Finds the function's declared-but-unused filler locals (names u*, pad*,
spare*, unused*, x0..: any local never mentioned after its declaration),
then grades every combination of their sizes (0..max(orig+2,4) ints each,
capped at 4000 variants) plus, when the ROM frame is larger than ours, one
extra `int padN[k]` (k 1..6) at each declaration slot.  Prints the best few
and writes build/tgrally/n64/search/<VA>/fill_best.c."""
import itertools
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, 'tools/tgrally')
import n64search as S  # noqa: E402
import n64t3 as T  # noqa: E402

args = sys.argv[1:]
va = int(args[0], 16)
path, name, _ = T.source_of(va)
tree = open(path).read()
body = T.function_text(tree, name)
cur = open(args[args.index('--file') + 1]).read().strip('\n') if '--file' in args else body
work = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tw')

lines = cur.split('\n')
brace = next(i for i, l in enumerate(lines) if l.strip() == '{')
decl_re = re.compile(r'^(\s+)(?:unsigned |signed )?(int|short|char|float)\s+(\w+)(?:\[(\d+)\])?;(.*)$')
decls = []
for i in range(brace + 1, len(lines)):
    m = decl_re.match(lines[i])
    if not m:
        if lines[i].strip() == '' or lines[i].strip().startswith(('/*', '*')):
            continue
        if re.match(r'^\s+(\w+\s+)+\**\w+(\[\d+\])*(\s*=[^;]*)?;', lines[i]):
            continue
        break
    decls.append((i, m))
rest = '\n'.join(lines[decls[-1][0] + 1:]) if decls else cur
unused = [(i, m) for i, m in decls if not re.search(r'\b%s\b' % m.group(3), rest)]
print('unused fillers:', [m.group(3) + ('[%s]' % m.group(4) if m.group(4) else '') for i, m in unused])


def render(sizes, extra):
    out = list(lines)
    for (i, m), n in zip(unused, sizes):
        if n == 0:
            out[i] = None
        else:
            out[i] = '%s%s %s%s;%s' % (m.group(1), m.group(2), m.group(3),
                                       '[%d]' % n if (n > 1 or m.group(4)) else '', m.group(5))
    if extra:
        slot, k = extra
        out.insert(slot, '  int padx[%d];' % k)
    return '\n'.join(l for l in out if l is not None)


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    return (g[2], g[0], g[1]) if isinstance(g, tuple) else (10 ** 9,)


ranges = [range(0, max(int(m.group(4) or 1) + 3, 5)) for i, m in unused]
combos = list(itertools.product(*ranges))[:4000] if ranges else [()]
cands = [(c, None) for c in combos]
slots = [i for i, m in decls] + ([decls[-1][0] + 1] if decls else [])
orig = tuple(int(m.group(4) or 1) for i, m in unused)
for s in slots:
    for k in range(1, 7):
        cands.append((orig, (s, k)))
texts = [render(c, x) for c, x in cands]
with ThreadPoolExecutor(12) as ex:
    res = list(ex.map(grade, texts))
order = sorted(range(len(res)), key=lambda k: res[k])
print('base', grade(cur))
for k in order[:5]:
    print(res[k], cands[k])
d = os.path.join('build/tgrally/n64/search', '%08X' % va)
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'fill_best.c'), 'w').write(texts[order[0]] + '\n')
