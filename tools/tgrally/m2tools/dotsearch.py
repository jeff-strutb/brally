"""dotsearch.py VA [--file body.c] : three-product sum search.

A dot product's term order, factor order and grouping decide which operand
IDO loads first, which temporary each product lands in and the add chain
(BrCrRespWalk: planeD and the push-out distance).  Finds every expression
that is a sum of exactly three products of plain operands (members,
indexed elements, identifiers), in any grouping, and tries all 96 forms per
site, greedily site by site.  Writes build/tgrally/n64/search/<VA>/dot_best.c."""
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
os.makedirs(work, exist_ok=True)

OP = r'(?:[A-Za-z_]\w*(?:(?:->|\.)\w+|\[[^\[\]]+\])*)'
PR = r'(%s) \* (%s)' % (OP, OP)
# a + b + c, (a + b) + c, a + (b + c)
PAT = re.compile(r'\(?\(?%s \+ \(?%s\)? \+ %s\)?\)?' % (PR, PR, PR))


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    if not isinstance(g, tuple):
        return (10 ** 9,)
    return (g[2], g[0], g[1])


def forms(m):
    t = [(m.group(1), m.group(2)), (m.group(3), m.group(4)), (m.group(5), m.group(6))]
    out = []
    for perm in itertools.permutations(range(3)):
        for fl in itertools.product((0, 1), repeat=3):
            terms = ['%s * %s' % ((t[i][0], t[i][1]) if not fl[k] else (t[i][1], t[i][0]))
                     for k, i in enumerate(perm)]
            out.append('(%s + %s) + %s' % tuple(terms))
            out.append('%s + (%s + %s)' % tuple(terms))
    return out


def sites(text):
    res = []
    for m in PAT.finditer(text):
        s, e = m.start(), m.end()
        # keep balanced: only whole parenthesised spans
        frag = text[s:e]
        if frag.count('(') != frag.count(')'):
            continue
        res.append((s, e, m))
    return res


best = grade(cur)
print('base', best)
k = 0
while True:
    ss = sites(cur)
    if k >= len(ss):
        break
    s, e, m = ss[k]
    cands = [cur[:s] + '(' + f + ')' + cur[e:] for f in forms(m)]
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(grade, cands))
    j = min(range(len(res)), key=lambda q: res[q])
    if res[j] < best:
        best, cur = res[j], cands[j]
        print('   site %d -> %s' % (k, best))
    k += 1
print('best (T4, aligned, blind)', best)
d = os.path.join('build/tgrally/n64/search', '%08X' % va)
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'dot_best.c'), 'w').write(cur + '\n')
print('->', os.path.join(d, 'dot_best.c'))
