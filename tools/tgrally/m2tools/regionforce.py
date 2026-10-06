"""regionforce.py VA LO HI [--fixed k,k] [--colors 1,3,4] : for every uopt web, try
split / each colour and score only ROM words LO..HI (VAs), to find which webs'
colouring differs from the ROM in that region."""
import sys
sys.path.insert(0, 'tools/tgrally')
import n64alloc as A, n64build as B  # noqa: E402
from concurrent.futures import ThreadPoolExecutor
args = sys.argv[1:]
va, lo, hi = (int(x, 16) for x in args[:3])
fixed = args[args.index('--fixed') + 1].split(',') if '--fixed' in args else []
colors = [int(c) for c in args[args.index('--colors') + 1].split(',')] if '--colors' in args else [1, 2, 3, 4, 5, 6]
g = A.Grader(va, None)
k0, k1 = (lo - va) // 4, (hi - va) // 4
def score(keys):
    env = {'CDX_PROC': str(g.proc)}
    if keys:
        env['CDX_FORCE'] = ','.join(keys)
    obj, lg = A.compile_traced(g.f, env)
    if obj is None:
        return 10 ** 9, 10 ** 9
    have = {p[0]: p for p in B.carve(obj)}
    _, s, e = have[g.name]
    st, nd, notes, ours, theirs = B.grade(obj, g.rom, g.name, s, e, g.va, g.fmap[g.va], g.syms, g.fnvas)
    return sum(1 for a, b in zip(ours[k0:k1], theirs[k0:k1]) if a != b), nd
nd, log, st = g.grade({'CDX_LOG': '1', **({'CDX_FORCE': ','.join(fixed)} if fixed else {})})
rows = A.decisions(log)
base = score(fixed)
print('base', base)
keys = []
for d in rows:
    w = '%s:w%s' % (d['phase'], d['web'])
    if any(k.startswith(w + '=') for k in fixed):
        continue
    keys.append(w + '=s')
    keys += ['%s=c%d' % (w, c) for c in colors]
keys = sorted(set(keys))
with ThreadPoolExecutor(12) as ex:
    res = list(ex.map(lambda k: (k, score(fixed + [k])), keys))
res.sort(key=lambda r: r[1])
for k, r in res[:15]:
    if r[0] < base[0]:
        print(k, r)
