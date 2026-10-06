"""ringsearch.py VA draft.c [--seed p:e:r,...] [--rounds N] : greedy search for the
temp-ring edits (move register R to the end of ugen's free list at emit index E)
that carry the first differing word furthest.  Each round tries every GP temp at
every allocation point in a window before the current first diff.  The result is
the ring delta the ROM implies, to be explained by a source construct."""
import os, re, sys, subprocess
from concurrent.futures import ThreadPoolExecutor
os.environ['TGR_TRACE_CC'] = os.path.abspath('build/tgrally/ext/instr2/out/cc')
sys.path.insert(0, 'tools/tgrally')
import n64t3 as T, n64alloc as A, n64build as B  # noqa: E402
args = sys.argv[1:]
va = int(args[0], 16)
seed = args[args.index('--seed') + 1].split(',') if '--seed' in args else []
rounds = int(args[args.index('--rounds') + 1]) if '--rounds' in args else 6
path, name, _ = T.source_of(va)
tree = open(path).read()
body = T.function_text(tree, name)
full = tree.replace(body, open(args[1]).read().strip('\n'))
tmp = os.path.join('build/tgrally/n64/m2tools/tw', 'ring_' + os.path.basename(path))
os.makedirs(os.path.dirname(tmp), exist_ok=True)
open(tmp, 'w').write(full)
rom, fmap, syms = B.Rom(), B.function_map(), B.load_symbols()
fnvas = {n: v for v, n, _ in B.tags_in(full)}
proc = A.Grader.__new__(A.Grader)
objs, log = A.compile_traced(tmp, {})
proc = [p[0] for p in B.carve(objs)].index(name)


def grade(spec):
    env = {'DKWB_INJECT': ','.join(spec)} if spec else {}
    obj, log = A.compile_traced(tmp, env)
    if obj is None:
        return (-1, 10 ** 9)
    have = {p[0]: p for p in B.carve(obj)}
    _, s, e = have[name]
    st, nd, notes, ours, theirs = B.grade(obj, rom, name, s, e, va, fmap[va], syms, fnvas)
    first = next((k for k, (a, b) in enumerate(zip(ours, theirs)) if a != b), len(ours))
    return (first, nd)


def alloc_emits(spec):
    env = {'DKWB_UGEN_TRACE': '1'}
    if spec:
        env['DKWB_INJECT'] = ','.join(spec)
    _, log = A.compile_traced(tmp, env)
    out = []
    for l in log.splitlines():
        m = re.match(r'DKWB-FREELIST ALLOC_GP proc=(\d+) reg=\d+ emitted=(\d+)', l)
        if m and int(m.group(1)) == proc:
            out.append(int(m.group(2)))
    return sorted(set(out))


cur = list(seed)
best = grade(cur)
print('start', cur, best)
REGS = [8, 9, 10, 11, 12, 13, 14, 15, 24, 25]
for rnd in range(rounds):
    first = best[0]
    emits = alloc_emits(cur)
    lo = int(first * 0.80) - 40
    hi = int(first * 0.92) + 10
    last = int(cur[-1].split(':')[1]) if cur else 0
    window = [e for e in emits if max(lo, last) <= e <= hi]
    cands = [cur + ['%d:%d:%d' % (proc, e, r)] for e in window for r in REGS]
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(grade, cands))
    k = max(range(len(res)), key=lambda q: (res[q][0], -res[q][1]))
    if res[k][0] <= first:
        print('round', rnd, 'no improvement over', best, 'window', len(window))
        break
    cur, best = cands[k], res[k]
    print('round', rnd, cur[-1], best, flush=True)
print('final', ','.join(cur), best)
