"""permsearch.py VA [--file body.c] [--max 6]

Statement-order search.  Finds runs of consecutive simple statements at one
indentation (assignments / compound assignments / increments ending in ';',
no calls, no control flow) and tries every order of each run (runs longer
than --max are windowed), keeping the best, run by run, until nothing
improves.  A permutation is only tried when no statement in the run reads or
writes a name another statement in it writes (names = identifiers outside
member/field position), so the result keeps the semantics as long as the
run's lvalues do not alias through different pointers -- check any EXACT
result by eye.  Writes build/tgrally/n64/search/<VA>/perm_best.c."""
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
mx = int(args[args.index('--max') + 1]) if '--max' in args else 6
path, name, _ = T.source_of(va)
tree = open(path).read()
body = T.function_text(tree, name)
cur = open(args[args.index('--file') + 1]).read().strip('\n') if '--file' in args else body
work = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tw')
os.makedirs(work, exist_ok=True)

SIMPLE = re.compile(r'^(\s+)([^;{}()]*?(?:\+\+|--|[-+*/&|^]?=)[^;{}]*);\s*$')
CALL = re.compile(r'[A-Za-z_]\w*\s*\(')
KEYWORDS = {'if', 'for', 'while', 'return', 'switch', 'case', 'else', 'do', 'goto'}


def names(s):
    s = re.sub(r'(->|\.)\s*\w+', '', s)          # drop member names
    return set(re.findall(r'\b[A-Za-z_]\w*\b', s)) - {'float', 'int', 'unsigned', 'short', 'char'}


def written(stmt):
    m = re.match(r'\s*(.*?)\s*(\+\+|--|[-+*/&|^]?=)(?!=)', stmt)
    lhs = m.group(1) if m else stmt
    return names(lhs), lhs.strip()


def runs(text):
    lines = text.split('\n')
    out, i = [], 0
    while i < len(lines):
        m = SIMPLE.match(lines[i])
        if not m or CALL.search(lines[i]) or lines[i].split()[0] in KEYWORDS:
            i += 1
            continue
        ind = m.group(1)
        j = i
        while j < len(lines):
            mm = SIMPLE.match(lines[j])
            if not mm or mm.group(1) != ind or CALL.search(lines[j]) or lines[j].split()[0] in KEYWORDS:
                break
            j += 1
        if j - i >= 2:
            out.append((i, j))
        i = j if j > i else i + 1
    return lines, out


def independent(stmts):
    ws = [written(s) for s in stmts]
    for a in range(len(stmts)):
        for b in range(len(stmts)):
            if a == b:
                continue
            wa_names, wa_lhs = ws[a]
            # b reads or writes the exact lvalue a writes, or a plain name a writes
            if wa_lhs in stmts[b]:
                return False
            plain = {n for n in wa_names if re.fullmatch(r'\s*' + n + r'\s*', wa_lhs)}
            if plain & names(stmts[b]):
                return False
    return True


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    if not isinstance(g, tuple):
        return (10 ** 9,)
    return (g[2], g[0], g[1])


best = grade(cur)
print('base', best)
improved = True
while improved and best[0] > 0:
    improved = False
    lines, rs = runs(cur)
    for (a, b) in rs:
        for w0 in range(a, max(a + 1, b - mx + 1)):
            w1 = min(b, w0 + mx)
            block = lines[w0:w1]
            if len(block) < 2 or not independent(block):
                continue
            cands = []
            for p in itertools.permutations(range(len(block))):
                if list(p) == list(range(len(block))):
                    continue
                cands.append('\n'.join(lines[:w0] + [block[k] for k in p] + lines[w1:]))
            with ThreadPoolExecutor(12) as ex:
                res = list(ex.map(grade, cands))
            k = min(range(len(res)), key=lambda q: res[q]) if res else None
            if k is not None and res[k] < best:
                best, cur = res[k], cands[k]
                improved = True
                print('  lines %d-%d ->' % (w0, w1), best)
                break
        if improved:
            break
print('best (T4, aligned, blind)', best)
d = os.path.join('build/tgrally/n64/search', '%08X' % va)
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'perm_best.c'), 'w').write(cur + '\n')
print('->', os.path.join(d, 'perm_best.c'))
