"""declsearch.py VA [--file body.c] : local declaration layout search.

IDO gives every declared local a stack home, function-level ones top-down in
declaration order, and spilled temporaries sit below them, so the order of
the declarations (and any local the source declared but never used) decides
every spill offset, and the order can also decide register ties.  Greedy:
grade moving each function-level declaration to every other slot, adding an
unused `int` at every slot and dropping each never-mentioned local; keep the
best, repeat until nothing improves.  Writes build/tgrally/n64/search/<VA>/decl_best.c."""
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

DECL = re.compile(r'^  (?!static\b|extern\b|return\b|if\b|for\b|while\b|do\b|switch\b|goto\b)[A-Za-z_][\w ]*[\s*]+\(?\*?\s*(\w+)\)?(?:\[[^\]]*\])*(?:\([^;]*\))?;(\s*/\*.*\*/)?$')


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    if not isinstance(g, tuple):
        return (10 ** 9,)
    return (g[2], g[0], g[1])


def split(text):
    lines = text.split('\n')
    i = next(k for k, l in enumerate(lines) if l.strip() == '{') + 1
    j = i
    while j < len(lines) and DECL.match(lines[j]):
        j += 1
    return lines[:i], lines[i:j], lines[j:]


def cands(text):
    head, decls, rest = split(text)
    out = []
    n = len(decls)
    for a in range(n):
        for b in range(n):
            if a == b:
                continue
            d = list(decls)
            x = d.pop(a)
            d.insert(b, x)
            out.append(('mv %s %d->%d' % (DECL.match(x).group(1), a, b), '\n'.join(head + d + rest)))
    used = '\n'.join(rest)
    for a in range(n + 1):
        d = list(decls)
        d.insert(a, '  int unused%d;' % a)
        out.append(('add@%d' % a, '\n'.join(head + d + rest)))
    for a in range(n):
        v = DECL.match(decls[a]).group(1)
        if not re.search(r'\b%s\b' % v, used):
            d = decls[:a] + decls[a + 1:]
            out.append(('drop %s' % v, '\n'.join(head + d + rest)))
    return out


best = grade(cur)
print('base', best)
rounds = 0
while best[0] > 0 and rounds < 6:
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
open(os.path.join(d, 'decl_best.c'), 'w').write(cur + '\n')
print('->', os.path.join(d, 'decl_best.c'))
