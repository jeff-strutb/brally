"""deadinit.py VA [--file body.c] : dead-initialiser web-order search.

uopt's p2 sweep colours unconstrained webs in web-number order, and web
numbers follow first appearance in the IR -- including stores that dead-code
elimination later removes (BrModRowRead: `fx = 0; param = 0;` at the loop
top renumbered the webs to the ROM's colour order with no code).  For every
scalar local declared at the top of the function, tries a dead `v = 0;`
right after the declarations (and as the declaration's initialiser), then
greedily stacks the best ones.  Writes build/tgrally/n64/search/<VA>/dead_best.c."""
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
os.makedirs(work, exist_ok=True)

DECL = re.compile(r'^  ((?:unsigned |signed )?(?:char|short|int|long|float|double|u8|s8|u16|s16|u32|s32|f32)'
                  r'|[A-Za-z_]\w* \*+)\s*\**\s*([A-Za-z_]\w*);$')


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    if not isinstance(g, tuple):
        return (10 ** 9,)
    return (g[2], g[0], g[1])


def decls(text):
    lines = text.split('\n')
    out = []
    for k, l in enumerate(lines[2:], 2):
        if not l.strip():
            break
        m = DECL.match(l)
        if m:
            out.append((k, m.group(2), l))
    return lines, out


def cands(text):
    lines, ds = decls(text)
    if not ds:
        return []
    last = max(k for k, _, _ in ds)
    res = []
    for k, v, l in ds:
        # a dead store after the declarations
        new = lines[:last + 1] + ['  %s = 0;' % v] + lines[last + 1:]
        if lines[last + 1].strip():
            new = lines[:last + 1] + ['  %s = 0;' % v, ''] + lines[last + 1:]
        res.append(('store %s' % v, '\n'.join(new)))
        # the declaration's initialiser
        new = lines[:k] + [l[:-1] + ' = 0;'] + lines[k + 1:]
        res.append(('init %s' % v, '\n'.join(new)))
    return res


best = grade(cur)
print('base', best)
for rnd in range(4):
    cs = [c for c in cands(cur) if c[1] != cur]
    if not cs or best[0] == 0:
        break
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(lambda c: grade(c[1]), cs))
    k = min(range(len(res)), key=lambda q: res[q])
    if res[k] >= best:
        break
    best, cur = res[k], cs[k][1]
    print('  ', cs[k][0], '->', best)
print('best (T4, aligned, blind)', best)
d = os.path.join('build/tgrally/n64/search', '%08X' % va)
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'dead_best.c'), 'w').write(cur + '\n')
print('->', os.path.join(d, 'dead_best.c'))
