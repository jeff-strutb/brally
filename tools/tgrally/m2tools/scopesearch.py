"""scopesearch.py VA [--file body.c] : block-scope search.

IDO's uopt colours a local VARIABLE as one live range, across disjoint
if/else arms too, so a ROM that gives "one" local different registers in
different arms (or spills it to the temp area instead of its home) was
written with block-scoped locals.  For every function-level local (single
declarator) and every brace block that uses it, tries declaring a shadow of
it at the top of that block, when the block's first mention of it is a plain
assignment and its next mention after the block (if any) is one too, so the
value never crosses the block edge.  Greedy: grade all single moves, keep
the best, repeat; then drops function-level declarations left unused.
Writes build/tgrally/n64/search/<VA>/scope_best.c.  Check an EXACT result by eye."""
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

DECL = re.compile(r'^  ((?:unsigned |signed |const |register )*[A-Za-z_]\w*[\s*]+(\w+)(?:\[\w+\])?|[A-Za-z_]\w*\s*\(\*\s*(\w+)\)\s*\([^;]*\));\s*(?:/\*.*\*/)?$')


def grade(text):
    try:
        g = S.grade(tree.replace(body, text), va, name, work)
    except Exception:
        return (10 ** 9,)
    if not isinstance(g, tuple):
        return (10 ** 9,)
    return (g[2], g[0], g[1])


def fn_decls(lines):
    out = []
    i = next(k for k, l in enumerate(lines) if l.strip() == '{') + 1
    while i < len(lines) and lines[i].strip():
        m = DECL.match(lines[i])
        if m and '[' not in m.group(1) and ',' not in m.group(1) and '=' not in lines[i]:
            out.append((m.group(2) or m.group(3), m.group(1)))
        i += 1
    return out, i


def blocks(lines, start):
    """(open_line, close_line, indent) for every brace block after the decls."""
    out, stack = [], []
    for k in range(start, len(lines)):
        l = lines[k]
        s = l.strip()
        if s.startswith('}') and stack:
            o = stack.pop()
            out.append((o, k))
        if s.endswith('{') and k > start:
            stack.append(k)
    return out


def mentions(lines, v, a, b):
    w = re.compile(r'(?<![\w.>])' + re.escape(v) + r'\b')
    return [k for k in range(a, b) if w.search(re.sub(r'/\*.*?\*/', '', lines[k]))]


def plain_assign(line, v):
    """v = <expr not reading v>; at statement start (a fresh value)."""
    m = re.match(r'\s*' + re.escape(v) + r'\s*=([^=].*)$', line)
    if m is None:
        return False
    rhs = re.sub(r'/\*.*?\*/', '', m.group(1))
    return re.search(r'(?<![\w.>])' + re.escape(v) + r'\b', rhs) is None


def candidates(text):
    lines = text.split('\n')
    decls, dend = fn_decls(lines)
    out = []
    for v, d in decls:
        allm = mentions(lines, v, dend, len(lines))
        for o, c in blocks(lines, dend):
            inside = mentions(lines, v, o + 1, c)
            if not inside or not plain_assign(lines[inside[0]], v):
                continue
            # already shadowed in this block?
            if any(re.match(r'\s*' + re.escape(d) + ';', lines[k]) for k in range(o + 1, c)):
                continue
            if re.search(r'\b' + re.escape(v) + r'\b', lines[o]):
                continue
            after = [k for k in allm if k > c]
            if after and not plain_assign(lines[after[0]], v):
                continue
            ind = re.match(r'\s*', lines[o + 1]).group(0)
            new = lines[:o + 1] + [ind + d + ';', ''] + lines[o + 1:]
            out.append(('%s@%d' % (v, o), '\n'.join(new)))
    return out


def drop_unused(text):
    lines = text.split('\n')
    decls, dend = fn_decls(lines)
    res = []
    for v, d in decls:
        # a declaration still needed fails to compile and grades worst
        k0 = next((k for k, l in enumerate(lines) if l.startswith('  ' + d + ';')), None)
        if k0 is None:
            continue
        res.append(('-' + v, '\n'.join(lines[:k0] + lines[k0 + 1:])))
    return res


best = grade(cur)
print('base', best)
while best[0] > 0:
    cs = candidates(cur)
    if not cs:
        break
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(lambda c: grade(c[1]), cs))
    k = min(range(len(res)), key=lambda q: res[q])
    if res[k] >= best:
        break
    best, cur = res[k], cs[k][1]
    print('  ', cs[k][0], '->', best)
if best[0] > 0:
    ds = drop_unused(cur)
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(lambda c: grade(c[1]), ds))
    for (lab, txt), r in zip(ds, res):
        if r < best:
            best, cur = r, txt
            print('  ', lab, '->', best)
print('best (T4, aligned, blind)', best)
d = os.path.join('build/tgrally/n64/search', '%08X' % va)
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'scope_best.c'), 'w').write(cur + '\n')
print('->', os.path.join(d, 'scope_best.c'))
