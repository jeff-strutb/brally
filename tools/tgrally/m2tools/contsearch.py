"""contsearch.py VA [--file body.c] : loop-tail continue search.

Equal-save webs colour in web-number order, which follows each value's first
appearance in the IR, and a `continue` moves a loop's latch (bound, step)
ahead of the code it skips (BrSchedInit).  For every `if (X) {` whose block
is the last statement of a loop body, tries `if (!(X)) continue;` with the
block's statements lifted out, and for an `if (A && B) {` also the split
`if (A) { if (!(B)) continue; ... }` at each && (one site at a time, then the
best pairs).  Writes build/tgrally/n64/search/<VA>/cont_best.c."""
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


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    if not isinstance(g, tuple):
        return (10 ** 9,)
    return (g[2], g[0], g[1])


def match_close(lines, k):
    depth = 0
    for j in range(k, len(lines)):
        depth += lines[j].count('{') - lines[j].count('}')
        if depth == 0:
            return j
    return None


def split_and(cond):
    """top-level && operands of cond"""
    parts, depth, cur_ = [], 0, ''
    i = 0
    while i < len(cond):
        ch = cond[i]
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if depth == 0 and cond.startswith('&&', i):
            parts.append(cur_.strip())
            cur_ = ''
            i += 2
            continue
        cur_ += ch
        i += 1
    parts.append(cur_.strip())
    return parts


def cands(text):
    lines = text.split('\n')
    out = []
    for k, l in enumerate(lines):
        m = re.match(r'^(\s*)if \((.*)\) \{$', l)
        if not m:
            continue
        ind, cond = m.groups()
        e = match_close(lines, k)
        if e is None or e + 1 >= len(lines):
            continue
        # the if block (no else) must end its loop body, possibly through
        # enclosing ifs that also end right there
        if lines[e].strip() != '}':
            continue
        ok, j = False, e + 1
        while j < len(lines) and re.match(r'^\s*\}\s*(while.*)?$', lines[j]):
            depth, h = 0, None
            for q in range(j, -1, -1):
                depth += lines[q].count('}') - lines[q].count('{')
                if depth == 0 and q < j:
                    h = q
                    break
            if h is None:
                break
            if re.match(r'^\s*(for|while|do)\b', lines[h]):
                ok = True
                break
            if 'else' in lines[h] or 'switch' in lines[h] or not lines[h].rstrip().endswith(') {'):
                break
            j += 1
        if not ok:
            continue
        inner = [x[2:] if x.startswith(ind + '  ') else x for x in lines[k + 1:e]]
        new = lines[:k] + [ind + 'if (!(%s)) {' % cond, ind + '  continue;', ind + '}'] + inner + lines[e + 1:]
        out.append(('cont@%d' % k, '\n'.join(new)))
        ps = split_and(cond)
        for s in range(1, len(ps)):
            a = ' && '.join(ps[:s])
            b = ' && '.join(ps[s:])
            new = (lines[:k] + [ind + 'if (%s) {' % a, ind + '  if (!(%s)) {' % b, ind + '    continue;',
                                ind + '  }'] + lines[k + 1:e] + lines[e:])
            out.append(('split@%d/%d' % (k, s), '\n'.join(new)))
    return out


best = grade(cur)
print('base', best)
rounds = 0
while best[0] > 0 and rounds < 3:
    rounds += 1
    cs = cands(cur)
    if not cs:
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
open(os.path.join(d, 'cont_best.c'), 'w').write(cur + '\n')
print('->', os.path.join(d, 'cont_best.c'))
