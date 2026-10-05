"""Unattended source search toward a byte-exact function (the N64 lane).

WHAT IT DOES: takes a function's C body and walks downhill over
meaning-preserving respellings.  Each round it builds every single-edit
neighbour of the current best body, compiles all of them in parallel (each a
scratch copy of the whole file), and keeps the one with the fewest differing
instructions.  It stops when no neighbour improves, when the function is
byte-exact, or at the round/compile budget.  Respellings:

  stmt-move   move a statement to another place in its block, only past
              statements it commutes with (no shared written name, a call
              never crosses a call or a store through memory, a store
              through memory never crosses a read or write of the same base)
  decl-move   move a local declaration to another place in the list
              (declared order sets the stack homes)
  arms        if (c) {A} else {B}  ->  if (!(c)) {B} else {A}
  zero        a whole call argument or assignment value 0 <-> 0.0f
  (and every n64permute respelling, enumerated rather than sampled:
  commutative operand swaps, x op= y forms, chain rotation, adjacent
  declaration and assignment swaps)

The score is the aligned instruction diff (an insertion counts once); the
T4 count (positional) breaks ties and decides EXACT.  The tree is never
edited unless --apply is given and the result is EXACT; even then the body
must go through the live oracle (n64t3.py) before it is certified, since a
statement move is only as safe as the dependency guard.

    .venv/bin/python n64/tools/n64search.py 0x80233E10
    .venv/bin/python n64/tools/n64search.py 0x80233E10 --file draft.c --rounds 30
    .venv/bin/python n64/tools/n64search.py --batch rows.txt --out build/n64/search
"""
import argparse
import os
import re
import shutil
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n64build as B  # noqa: E402
import n64permute as P  # noqa: E402
import n64t3 as T  # noqa: E402

SCRATCH = os.path.join(B.OUT, 'search')
SCORE = 'aligned'       # or 'positional': rank by the T4 word count first


def key(s):
    return s if SCORE == 'aligned' else (s[2], s[0], s[1])
_ctx = None


def ctx():
    global _ctx
    if _ctx is None:
        _ctx = (B.Rom(), B.function_map(), B.load_symbols())
    return _ctx


def aligned_diff(ours, theirs):
    import difflib
    sm = difflib.SequenceMatcher(None, list(ours), list(theirs), autojunk=False)
    return sum(max(i2 - i1, j2 - j1) for t, i1, i2, j1, j2 in sm.get_opcodes() if t != 'equal')


def regblind(w):
    """an instruction word with its register fields cleared"""
    op = w >> 26
    if op == 0:                       # SPECIAL: keep opcode, shift amount, function
        return w & 0xFC0007FF
    if op == 0x11:                    # COP1: keep the format/function, drop fs/ft/fd
        return w & 0xFFE0003F
    return w & 0xFC00FFFF             # I/J-type: keep opcode and immediate


def grade(src, va, name, workdir):
    """-> (aligned, aligned register-blind, positional) or None when the
    source does not compile"""
    fd, tmp = tempfile.mkstemp(suffix='.c', dir=workdir)
    os.close(fd)
    try:
        open(tmp, 'w').write(src)
        obj, err = B.compile_c(tmp)
    finally:
        os.unlink(tmp)
    if obj is None:
        return None
    pieces = {n: (s, e) for n, s, e in B.carve(obj) if n}
    if name not in pieces:
        return None
    rom, fmap, syms = ctx()
    fnvas = {n: v for v, n, _ in B.tags_in(src)}
    st, nd, notes, ours, theirs = B.grade(obj, rom, name, *pieces[name], va, fmap[va], syms, fnvas)
    if st == 'EXACT':
        return (0, 0, 0)
    rb = aligned_diff([regblind(x) for x in ours], [regblind(x) for x in theirs])
    return (aligned_diff(ours, theirs), rb, max(1, nd))


# ------------------------------------------------------------- statements
CALL = re.compile(r'\b(?!if\b|while\b|for\b|switch\b|return\b|sizeof\b)\w+\s*\(')
IDENT = re.compile(r'[A-Za-z_]\w*')
KEYWORDS = {'if', 'else', 'while', 'for', 'do', 'return', 'switch', 'case', 'break',
            'continue', 'goto', 'sizeof', 'int', 'char', 'short', 'float', 'double',
            'unsigned', 'signed', 'long', 'void', 'struct', 'default'}


LOCALS = set()          # the current function's locals and parameters (set by search)
ARRAYS = set()          # locals declared as arrays (their element stores are memory)


def mem_root(expr):
    """the object an lvalue or read touches: a local scalar -> None (not
    memory); a named global or local array -> its name; through a pointer ->
    '*' (could be anything)"""
    names = [n for n in IDENT.findall(expr) if n not in KEYWORDS]
    if not names:
        return None
    root = names[0]
    deref = bool(re.search(r'->|^\s*\*|\(\s*\*', expr))
    if root in LOCALS and root not in ARRAYS:
        return '*' if (deref or '[' in expr) else None
    return '*' if deref and root in LOCALS else root


def facts(stmt):
    """(written locals, read locals, memory written, memory read, has call)
    for one simple statement, conservatively"""
    s = stmt.strip().rstrip(';')
    call = bool(CALL.search(s))
    w, r, mw, mr = set(), set(), set(), set()
    m = re.match(r'^(.+?)\s*([+\-*/%&|^]|<<|>>)?=(?!=)\s*(.+)$', s)
    lhss, rhs = [], s
    if m and not call or (m and not CALL.search(m.group(1))):
        if m:
            lhss, rhs = [m.group(1)], m.group(3)
            # x = y = z: every lhs in the chain is written
            while True:
                mm = re.match(r'^([\w\.\->\[\]\s\*]+?)\s*=(?!=)\s*(.+)$', rhs)
                if not mm:
                    break
                lhss.append(mm.group(1))
                rhs = mm.group(2)
    for lhs in lhss:
        root = mem_root(lhs)
        names = {n for n in IDENT.findall(lhs) if n not in KEYWORDS}
        if root is None:
            w |= names & LOCALS
        else:
            mw.add(root)
            r |= names & LOCALS
        if m and m.group(2):
            r |= names & LOCALS
            if root:
                mr.add(root)
    # every local named anywhere on the right (including inside [] and
    # call arguments) is read
    r |= {n for n in IDENT.findall(rhs) if n in LOCALS}
    for lhs in lhss:
        r |= {n for n in IDENT.findall(lhs[lhs.find('['):] if '[' in lhs else '') if n in LOCALS}
    for tok in re.findall(r'[A-Za-z_][\w\.\->\[\]]*', rhs):
        n = IDENT.match(tok).group(0)
        if n in KEYWORDS:
            continue
        if n in LOCALS:
            root = mem_root(tok)
            if root:
                mr.add(root)
        elif not re.match(r'^\w+\s*\(', rhs[rhs.find(tok):]):
            mr.add(n)
    for inc in re.findall(r'(\w+)\s*(?:\+\+|--)|(?:\+\+|--)\s*(\w+)', s):
        n = inc[0] or inc[1]
        (w if n in LOCALS else mw).add(n)
    if call:
        # a call can write anything it is handed and any memory at all
        w |= {n for n in IDENT.findall(s) if n in LOCALS}
        mw.add('*')
        mr.add('*')
    return w, r, mw, mr, call


def alias(x, y):
    return x == y or x == '*' or y == '*'


def commute(a, b):
    wa, ra, mwa, mra, ca = facts(a)
    wb, rb, mwb, mrb, cb = facts(b)
    if wa & (rb | wb) or wb & ra:
        return False
    for x in mwa:
        if any(alias(x, y) for y in mrb | mwb):
            return False
    for x in mwb:
        if any(alias(x, y) for y in mra):
            return False
    return True


def set_locals(src_fn):
    """record the function's parameters and locals for the dependency guard"""
    LOCALS.clear()
    ARRAYS.clear()
    head = src_fn.split('{', 1)[0]
    params = head[head.find('(') + 1:head.rfind(')')]
    for p in params.split(','):
        ns = IDENT.findall(p)
        if ns:
            LOCALS.add(ns[-1])
    for m in re.finditer(r'^\s+(?:unsigned |signed |register |static |const )*[A-Za-z_]\w*(?:\s+|\s*\*+\s*)'
                         r'([A-Za-z_]\w*)\s*(\[[^\]]*\])?[^;(=]*;', src_fn, re.M):
        if m.group(1) in KEYWORDS:
            continue
        LOCALS.add(m.group(1))
        if m.group(2):
            ARRAYS.add(m.group(1))
    # K&R parameter declarations and comma lists
    for m in re.finditer(r'^\s*(?:unsigned |signed )?[A-Za-z_]\w*\s+([^;(]+);', src_fn.split('{', 1)[0] + '\n', re.M):
        for part in m.group(1).split(','):
            ns = IDENT.findall(part)
            if ns:
                LOCALS.add(ns[0])


def blocks(lines):
    """runs of consecutive simple statements at one indentation"""
    simple = re.compile(r'^(\s+)[^\s{}#/][^{}]*;\s*$')
    runs, cur, ind = [], [], None
    for i, l in enumerate(lines):
        m = simple.match(l)
        bad = m and re.match(r'\s*(return|break|continue|goto|case|default)\b', l)
        is_decl = re.match(r'\s+(unsigned |signed |static |register )?(int|short|char|float|double|long|'
                           r'Br\w+|u8|u16|s16|u32|s32|f32)\b[^=;(]*;', l)
        if m and not bad and not is_decl and (ind is None or m.group(1) == ind):
            cur.append(i)
            ind = m.group(1)
        else:
            if len(cur) > 1:
                runs.append(cur)
            cur, ind = ([i], m.group(1)) if (m and not bad and not is_decl) else ([], None)
    if len(cur) > 1:
        runs.append(cur)
    return runs


def stmt_moves(body):
    lines = body.split('\n')
    out = []
    for run in blocks(lines):
        for a in range(len(run)):
            for b in range(len(run)):
                if a == b:
                    continue
                lo, hi = min(a, b), max(a, b)
                mover = lines[run[a]]
                crossed = [lines[run[k]] for k in range(lo, hi + 1) if k != a]
                if not all(commute(mover, c) for c in crossed):
                    continue
                seq = [run[k] for k in range(len(run))]
                x = seq.pop(a)
                seq.insert(b, x)
                new = list(lines)
                for k, idx in enumerate(run):
                    new[idx] = lines[seq[k]]
                out.append(('stmt-move', '\n'.join(new)))
    return out


def decl_moves(body):
    lines = body.split('\n')
    decl = [i for i, l in enumerate(lines)
            if re.match(r'\s+(unsigned |signed |register )?(int|short|char|float|double|long|Br\w+|u8|u16|'
                        r's16|u32|s32|f32|void)\b[^=;(]*;\s*(/\*.*\*/)?\s*$', l)]
    out = []
    # only one contiguous declaration run (the function's top)
    run = []
    for i in decl:
        if not run or i == run[-1] + 1:
            run.append(i)
        else:
            break
    # move a run of 1-3 adjacent declarations to any other gap
    for size in (1, 2, 3):
        for a in range(len(run) - size + 1):
            chunk = run[a:a + size]
            rest = run[:a] + run[a + size:]
            for b in range(len(rest) + 1):
                if b == a:
                    continue
                seq = rest[:b] + chunk + rest[b:]
                new = list(lines)
                for k, idx in enumerate(run):
                    new[idx] = lines[seq[k]]
                out.append(('decl-move', '\n'.join(new)))
    return out


def whole_flips(body):
    """a whole-number float literal <-> the int literal, only where the value
    cannot change: one side of a comparison, or the whole value assigned
    (1000.0f and 1000 are different u-code constants; one is hoisted out of a
    loop where the other is rebuilt at each use)"""
    out = []
    lit = r'(\d+)(?:\.0*)?[fF]?'
    for m in re.finditer(r'(?:(?<=[<>=!]=)|(?<=[<>]))(\s*)(-?)(\d+)(\.0*f|\.0*F|\.0+)?(?=\s*[);&|?]|\s*$)', body):
        num, frac = m.group(3), m.group(4)
        rep = num if frac else num + '.0f'
        out.append(('whole', body[:m.start(3)] + rep + body[m.end(4) if frac else m.end(3):]))
    for m in re.finditer(r'(?<![=!<>])=(?!=)(\s*)(-?)(\d+)(\.0*f|\.0*F|\.0+)?(?=\s*;)', body):
        num, frac = m.group(3), m.group(4)
        if num == '0':
            continue                      # zero_flips covers it
        rep = num if frac else num + '.0f'
        out.append(('whole', body[:m.start(3)] + rep + body[m.end(4) if frac else m.end(3):]))
    # a comparison with the literal on the left
    for m in re.finditer(r'(?<=[(&|!])(\s*)(-?)(\d+)(\.0*f|\.0*F|\.0+)?(?=\s*(?:[<>]=?|[=!]=))', body):
        num, frac = m.group(3), m.group(4)
        rep = num if frac else num + '.0f'
        out.append(('whole', body[:m.start(3)] + rep + body[m.end(4) if frac else m.end(3):]))
    return out


def arrow_flips(body):
    """X[0].f <-> X->f (they emit different temp-ring orders)"""
    out = []
    for m in re.finditer(r'\b(\w+)\[0\]\.(\w+)', body):
        out.append(('arrow', body[:m.start()] + '%s->%s' % (m.group(1), m.group(2)) + body[m.end():]))
    for m in re.finditer(r'\b(\w+)->(\w+)', body):
        if m.group(1) in LOCALS:          # a pointer local is not an array name
            continue
        out.append(('arrow', body[:m.start()] + '%s[0].%s' % (m.group(1), m.group(2)) + body[m.end():]))
    return out


def match_brace(s, i):
    depth = 0
    for k in range(i, len(s)):
        if s[k] == '{':
            depth += 1
        elif s[k] == '}':
            depth -= 1
            if depth == 0:
                return k
    return -1


def match_paren(s, i):
    depth = 0
    for k in range(i, len(s)):
        if s[k] == '(':
            depth += 1
        elif s[k] == ')':
            depth -= 1
            if depth == 0:
                return k
    return -1


def arm_flips(body):
    out = []
    for m in re.finditer(r'\bif\s*\(', body):
        if body[:m.start()].rstrip().endswith('else'):
            continue
        pe = match_paren(body, m.end() - 1)
        if pe < 0:
            continue
        rest = body[pe + 1:]
        lb = pe + 1 + (len(rest) - len(rest.lstrip()))
        if lb >= len(body) or body[lb] != '{':
            continue
        rb = match_brace(body, lb)
        tail = body[rb + 1:]
        em = re.match(r'\s*else\s*\{', tail)
        if not em:
            continue
        lb2 = rb + 1 + em.end() - 1
        rb2 = match_brace(body, lb2)
        cond = body[m.end():pe]
        a, b = body[lb + 1:rb], body[lb2 + 1:rb2]
        if re.match(r'^\s*!\s*\((.*)\)\s*$', cond, re.S):
            ncond = re.match(r'^\s*!\s*\((.*)\)\s*$', cond, re.S).group(1)
        else:
            ncond = '!(%s)' % cond.strip()
        new = body[:m.start()] + 'if (' + ncond + ') {' + b + '} else {' + a + '}' + body[rb2 + 1:]
        out.append(('arms', new))
    return out


def zero_flips(body):
    out = []
    pat = re.compile(r'(?<=[(,=])(\s*)(0|0\.0f|0\.0)(\s*)(?=[,);])')
    for m in pat.finditer(body):
        before = body[:m.start()].rstrip()
        if before.endswith('==') or before.endswith('!='):
            continue
        for rep in ('0', '0.0f'):
            if rep != m.group(2):
                out.append(('zero', body[:m.start(2)] + rep + body[m.end(2):]))
    return out


def permute_ops(body):
    """every n64permute respelling, enumerated"""
    class Pick:
        def __init__(self, k): self.k, self.n = k, None
        def choice(self, ops):
            self.n = len(ops)
            return ops[self.k]
    out, seen = [], set()
    k = 0
    while True:
        p = Pick(k)
        try:
            new = P.mutate(body, p)
        except IndexError:
            break
        if p.n is None or k >= p.n:
            break
        if new and new != body and new not in seen:
            seen.add(new)
            out.append(('permute', new))
        k += 1
    return out


def neighbours(body):
    out = []
    for gen in (stmt_moves, decl_moves, arm_flips, zero_flips, whole_flips, arrow_flips, permute_ops):
        out += gen(body)
    seen, uniq = set(), []
    for kind, b in out:
        if b != body and b not in seen:
            seen.add(b)
            uniq.append((kind, b))
    return uniq


# ------------------------------------------------------------------ search
def search(va, path=None, rounds=20, jobs=12, max_compiles=4000, lookahead=8, log=print):
    if path:
        src = open(path).read()
        name = next(n for v, n, _ in B.tags_in(src) if v == va)
    else:
        path, name, _ = T.source_of(va)
        src = open(path).read()
    body = T.function_text(src, name)
    set_locals(body)
    work = os.path.join(SCRATCH, '%08X' % va)
    os.makedirs(work, exist_ok=True)
    start = grade(src, va, name, work)
    if start is None:
        log('%08X %s: does not compile' % (va, name))
        return None
    best, best_body, compiles, trail = start, body, 1, []
    log('%08X %s: start aligned %d, blind %d, T4 %d' % (va, name, *start))
    tried = {body}
    for rnd in range(rounds):
        if best[-1] == 0 or compiles >= max_compiles:
            break
        cands = [(k, b) for k, b in neighbours(best_body) if b not in tried]
        cands = cands[:max(0, max_compiles - compiles)]
        tried |= {b for _, b in cands}
        with ThreadPoolExecutor(jobs) as ex:
            res = list(ex.map(lambda kb: (grade(src.replace(body, kb[1]), va, name, work), kb), cands))
        compiles += len(cands)
        res = [(s, kb) for s, kb in res if s is not None]
        if not res:
            break
        s, (kind, b) = min(res, key=lambda r: key(r[0]))
        if key(s) >= key(best) and lookahead and compiles < max_compiles:
            # no single edit helps: try two, starting from the closest few
            res.sort(key=lambda r: key(r[0]))
            seeds = [kb for _, kb in res[:lookahead]]
            second = []
            for k1, b1 in seeds:
                second += [('%s+%s' % (k1, k2), b2) for k2, b2 in neighbours(b1) if b2 not in tried]
            second = second[:max(0, max_compiles - compiles)]
            tried |= {b2 for _, b2 in second}
            with ThreadPoolExecutor(jobs) as ex:
                res2 = list(ex.map(lambda kb: (grade(src.replace(body, kb[1]), va, name, work), kb), second))
            compiles += len(second)
            res2 = [(s2, kb) for s2, kb in res2 if s2 is not None]
            if res2:
                s2, kb2 = min(res2, key=lambda r: key(r[0]))
                if key(s2) < key(best):
                    s, (kind, b) = s2, kb2
        if key(s) >= key(best):
            log('  round %d: %d neighbours, none better (best %s)' % (rnd + 1, len(cands), best))
            break
        best, best_body = s, b
        trail.append(kind)
        log('  round %d: %d neighbours, %s -> aligned %d, blind %d, T4 %d' % (rnd + 1, len(cands), kind, *s))
    out = os.path.join(work, 'best.c' if SCORE == 'aligned' else 'best_pos.c')
    open(out, 'w').write(src.replace(body, best_body))
    log('%08X %s: best aligned %d, blind %d, T4 %d after %d compiles (%s) -> %s' % (
        va, name, *best, compiles, ' '.join(trail) or 'no change', out))
    return dict(va=va, name=name, start=start, best=best, compiles=compiles, trail=trail,
                file=out, path=path, body=body, best_body=best_body)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va', nargs='?')
    ap.add_argument('--file', help='search from this draft instead of the tagged tree source')
    ap.add_argument('--batch', help='a file of VAs, one per line (first token)')
    ap.add_argument('--rounds', type=int, default=20)
    ap.add_argument('--jobs', type=int, default=12)
    ap.add_argument('--max-compiles', type=int, default=4000)
    ap.add_argument('--score', choices=('aligned', 'positional'), default='aligned',
                    help='rank by the aligned diff (structure first) or the T4 word count')
    ap.add_argument('--lookahead', type=int, default=8, help='seeds for a two-edit step when no single edit helps')
    ap.add_argument('--apply', action='store_true', help='write an EXACT result into the tree source')
    a = ap.parse_args()
    vas = []
    if a.batch:
        for l in open(a.batch):
            if l.strip() and not l.startswith('#'):
                vas.append(int(l.split()[0], 16))
    elif a.va:
        vas = [int(a.va, 16)]
    else:
        ap.error('give a VA or --batch')
    global SCORE
    SCORE = a.score
    summary = []
    for va in vas:
        r = search(va, a.file, a.rounds, a.jobs, a.max_compiles, a.lookahead)
        if r is None:
            continue
        summary.append(r)
        if a.apply and r['best'][-1] == 0 and not a.file:
            src = open(r['path']).read()
            open(r['path'], 'w').write(src.replace(r['body'], r['best_body']))
            print('  applied to %s: run the live oracle before certifying' % r['path'])
        sys.stdout.flush()
    if len(summary) > 1:
        print('\nsummary')
        for r in summary:
            print('%08X %-28s aligned %4d -> %4d   T4 %4d -> %4d   %s' % (
                r['va'], r['name'], r['start'][0], r['best'][0], r['start'][-1], r['best'][-1],
                'EXACT' if r['best'][-1] == 0 else ''))


if __name__ == '__main__':
    main()
