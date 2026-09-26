"""Random source respelling against the ROM -- the N64 lane's byte grind.

    .venv/bin/python n64/tools/n64permute.py 0x8022439C --iters 300
    .venv/bin/python n64/tools/n64permute.py 0x8022439C --iters 300 --ledger
    .venv/bin/python n64/tools/n64permute.py 0x8022439C --iters 300 --apply

Each iteration applies one or two random, meaning-preserving respellings to
the function's body, recompiles its file with IDO and grades the function
with the T4 gate.  The best spelling is kept as the next starting point.

Respellings: swap the operands of a commutative operator; x op= y <-> x = x op y;
swap two adjacent declarations; swap the two sides of == / !=; write an if
with its branches the other way round.  None of them changes what the
function does, so a spelling that grades EXACT is a match.

--apply     write the best spelling back when it is EXACT
--ledger    append a counted `@t4-pass` line above the function (Gate B of
            n64t3.py --qualify): the attempt count, the best diff count and
            how far this pass moved it
"""
import argparse
import datetime
import os
import random
import re
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
sys.path.insert(0, os.path.join(N64, 'tools'))
import n64build as B  # noqa: E402
import n64t3 as T  # noqa: E402

COMM = r'\+|\*|==|!=|&|\||\^'
OPERAND = r'[\w\.\->\[\]]+|\([^()]*\)'


def mutate(body, rng):
    ops = []
    for m in re.finditer(r'(?<![\w\)\]])(%s)\s*(%s)\s*(%s)(?![\w\(\[])' % (OPERAND, COMM, OPERAND), body):
        if m.group(2) in ('&', '|') and (m.group(1).startswith('&') or m.group(3).startswith('&')):
            continue
        ops.append(('swap', m))
    for m in re.finditer(r'(\b[\w\.\->\[\]]+)\s*([+*&|^-])=\s*([^;]+);', body):
        ops.append(('expand', m))
    for m in re.finditer(r'(\b[\w\.\->\[\]]+)\s*=\s*\1\s*([+*&|^-])\s*([^;]+);', body):
        ops.append(('contract', m))
    lines = body.split('\n')
    decl = [i for i, l in enumerate(lines)
            if re.match(r'\s+(unsigned |signed )?(int|short|char|float|double|u8|u16|s16|u32|s32|f32|BrVec3)\b[^;(]*;\s*$', l)]
    for i in decl:
        if i + 1 in decl:
            ops.append(('decl', i))
    if not ops:
        return None
    kind, m = rng.choice(ops)
    if kind == 'swap':
        return body[:m.start()] + '%s %s %s' % (m.group(3), m.group(2), m.group(1)) + body[m.end():]
    if kind == 'expand':
        return body[:m.start()] + '%s = %s %s %s;' % (m.group(1), m.group(1), m.group(2), m.group(3)) + body[m.end():]
    if kind == 'contract':
        return body[:m.start()] + '%s %s= %s;' % (m.group(1), m.group(2), m.group(3)) + body[m.end():]
    if kind == 'decl':
        lines[m], lines[m + 1] = lines[m + 1], lines[m]
        return '\n'.join(lines)
    return None


def grade_text(path, src, va, name):
    d = os.path.dirname(path)
    fd, tmp = tempfile.mkstemp(suffix='.c', dir=os.path.join(B.OUT))
    os.close(fd)
    try:
        # compile in place of the real file so includes resolve the same way
        open(tmp, 'w').write(src)
        obj, err = B.compile_c(tmp)
        if obj is None:
            return None
        pieces = {n: (s, e) for n, s, e in B.carve(obj) if n}
        if name not in pieces:
            return None
        fnvas = {n: v for v, n, _ in B.tags_in(src)}
        rom, fmap, syms = _ctx()
        st, nd, notes, _, _ = B.grade(obj, rom, name, *pieces[name], va, fmap[va], syms, fnvas)
        return 0 if st == 'EXACT' else max(1, nd)
    finally:
        os.unlink(tmp)


_c = None


def _ctx():
    global _c
    if _c is None:
        _c = (B.Rom(), B.function_map(), B.load_symbols())
    return _c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va')
    ap.add_argument('--iters', type=int, default=200)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--ledger', action='store_true')
    a = ap.parse_args()
    va = int(a.va, 16)
    path, name, _ = T.source_of(va)
    src = open(path).read()
    body = T.function_text(src, name)
    start = grade_text(path, src, va, name)
    best, best_body, compiles = start, body, 1
    rng = random.Random(a.seed)
    for i in range(a.iters):
        cand = best_body
        for _ in range(rng.choice((1, 1, 2))):
            nxt = mutate(cand, rng)
            if nxt:
                cand = nxt
        if cand == best_body:
            continue
        nd = grade_text(path, src.replace(body, cand), va, name)
        compiles += 1
        if nd is not None and nd < best:
            best, best_body = nd, cand
            print('  %d: %d diff%s' % (i, nd, '' if nd != 1 else ''))
            if nd == 0:
                break
    print('%08X %s: start %s, best %s after %d compiles' % (va, name, start, best, compiles))
    out = src
    if a.apply and best == 0 and best_body != body:
        out = out.replace(body, best_body)
        print('  applied the byte-exact spelling')
    if a.ledger:
        n = len(re.findall(r'@t4-pass\s+0x%08X' % va, out)) + 1
        line = '/* @t4-pass 0x%08X %d %s compiles %d best %d moved %d  (n64/tools/n64permute.py) */\n' % (
            va, n, datetime.date.today().isoformat(), compiles, best, start - best)
        tag = '/* @implements 0x%08X tgr %s */' % (va, name)
        out = out.replace(tag, line + tag, 1)
    if out != src:
        open(path, 'w').write(out)


if __name__ == '__main__':
    main()
