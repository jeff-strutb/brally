"""linemove.py VA --file body.c --lines A-B[,C-D...] : statement-move search.

For each given 1-based line range of the draft (independent statements whose
order is free, checked by eye), tries moving every line to every other
position in its range and keeps the best, repeating until nothing improves
(all permutations when the range has <= 6 lines).  For store-ordering
residue the scheduler leaves when statements are in a different source
order.  Writes build/tgrally/n64/search/<VA>/move_best.c."""
import itertools
import os
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
cur = open(args[args.index('--file') + 1]).read().strip('\n')
ranges = [tuple(int(x) for x in r.split('-')) for r in args[args.index('--lines') + 1].split(',')]
work = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tw')
os.makedirs(work, exist_ok=True)


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    if not isinstance(g, tuple):
        return (10 ** 9,)
    return (g[2], g[0], g[1])


def variants(text, a, b):
    lines = text.split('\n')
    blk = lines[a - 1:b]
    out = []
    if len(blk) <= 6:
        for p in itertools.permutations(blk):
            out.append('\n'.join(lines[:a - 1] + list(p) + lines[b:]))
    else:
        for i in range(len(blk)):
            for j in range(len(blk)):
                if i == j:
                    continue
                nb = blk[:]
                x = nb.pop(i)
                nb.insert(j, x)
                out.append('\n'.join(lines[:a - 1] + nb + lines[b:]))
    return out


best = grade(cur)
print('base', best)
changed = True
while changed and best[0] > 0:
    changed = False
    for a, b in ranges:
        cs = [c for c in variants(cur, a, b) if c != cur]
        with ThreadPoolExecutor(12) as ex:
            res = list(ex.map(grade, cs))
        k = min(range(len(res)), key=lambda q: res[q])
        if res[k] < best:
            best, cur = res[k], cs[k]
            changed = True
            print('   lines %d-%d -> %s' % (a, b, best))
print('best (T4, aligned, blind)', best)
d = os.path.join('build/tgrally/n64/search', '%08X' % va)
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'move_best.c'), 'w').write(cur + '\n')
print('->', os.path.join(d, 'move_best.c'))
