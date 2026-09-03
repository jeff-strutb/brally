"""Recover the original commutative operand order by sweeping it against the ROM.

Where our C and the ROM agree on every opcode but not on the bytes, the usual
cause is operand order: IDO emits `a + b` and `b + a` differently, so the ROM
records which way the original was written.  That makes the order recoverable
by search -- try both spellings of each commutative operator in a function and
keep whichever reproduces the ROM's bytes.

  .venv/bin/python n64/tools/permute.py --report build/n64/report.csv
  .venv/bin/python n64/tools/permute.py --fn BrVec3Add --apply

Without --apply nothing is written; the recovered spelling is only reported.

This is worth more to the PC decomp than to the N64 one.  VC5 canonicalises
commutative operands, so on the PC side operand order is not recoverable from
the bytes at all and has to be guessed.  Here it is a search with a decisive
oracle, and the answer transfers back.
"""
import sys, os, csv, re, argparse, itertools, collections, tempfile, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'n64/tools'))
import n64match as M  # noqa: E402

MAX_OPS = 12                    # 2**12 spellings is already 4096 compiles

TOKEN = re.compile(r"""
      \s+
    | //[^\n]*
    | /\*.*?\*/
    | "(?:\\.|[^"\\])*"
    | '(?:\\.|[^'\\])*'
    | 0[xX][0-9a-fA-F]+[uUlLfF]*
    | \d+\.?\d*(?:[eE][-+]?\d+)?[uUlLfF]*
    | \.\d+(?:[eE][-+]?\d+)?[fF]*
    | [A-Za-z_]\w*
    | ->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^!<>=]=
    | .
""", re.S | re.X)

OPENERS = {')': '(', ']': '['}


def lex(src):
    """-> [(text, start, end)] with whitespace and comments dropped."""
    out = []
    for m in TOKEN.finditer(src):
        t = m.group(0)
        if t.strip() and not t.startswith(('//', '/*')):
            out.append((t, m.start(), m.end()))
    return out


def _term_back(toks, i):
    """Index of the first token of the operand ending at toks[i] inclusive."""
    t = toks[i][0]
    if t in OPENERS:
        depth, k = 0, i
        while k >= 0:
            if toks[k][0] == t:
                depth += 1
            elif toks[k][0] == OPENERS[t]:
                depth -= 1
                if depth == 0:
                    break
            k -= 1
        if k < 0:
            return None
        i = k
        if i > 0 and re.match(r'^[A-Za-z_]\w*$', toks[i - 1][0]):
            i -= 1                                  # call or subscript base
    elif not re.match(r'^[\w.]+$', t):
        return None
    # walk back over a  a->b.c  chain
    while i >= 2 and toks[i - 1][0] in ('->', '.'):
        i -= 2
        if toks[i][0] in OPENERS:
            j = _term_back(toks, i)
            if j is None:
                return None
            i = j
    if i > 0 and toks[i - 1][0] in ('*', '&', '-', '!', '~'):
        return None                                 # unary-prefixed: leave it
    return i


def _term_fwd(toks, i):
    """Index of the last token of the operand starting at toks[i] inclusive."""
    if not re.match(r'^[\w.]+$', toks[i][0]):
        return None
    while i + 1 < len(toks):
        n = toks[i + 1][0]
        if n in ('->', '.') and i + 2 < len(toks):
            i += 2
        elif n in ('(', '['):
            close = ')' if n == '(' else ']'
            depth, k = 0, i + 1
            while k < len(toks):
                if toks[k][0] == n:
                    depth += 1
                elif toks[k][0] == close:
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            if k >= len(toks):
                return None
            i = k
        else:
            break
    return i


# A declaration reads exactly like a multiplication: `V *o` is (ident, *, ident)
# and so is `a * b`.  Without types the two cannot be told apart, so skip any
# `*` that begins a statement-initial `type *name` shape.
STMT_START = {';', '{', '}', ',', '(', ':'}


def commutative_sites(src, lo, hi):
    """-> [(left_start, op_pos, right_end)] char offsets, within [lo, hi)."""
    toks = lex(src[lo:hi])
    out = []
    for i, (t, a, b) in enumerate(toks):
        if t not in '+*' or i == 0 or i + 1 >= len(toks):
            continue
        prev = toks[i - 1][0]
        if not (re.match(r'^[\w.]+$', prev) or prev in OPENERS):
            continue                                # unary, or not an operand
        ls = _term_back(toks, i - 1)
        rt = _term_fwd(toks, i + 1)
        if ls is None or rt is None:
            continue
        if t == '*':
            before = toks[ls - 1][0] if ls > 0 else ';'
            # `type *name` : an identifier at the head of a statement
            if before in STMT_START and ls == i - 1:
                continue
        left = src[lo + toks[ls][1]:lo + toks[i - 1][2]]
        right = src[lo + toks[i + 1][1]:lo + toks[rt][2]]
        if not left.strip() or not right.strip() or left.strip() == right.strip():
            continue
        out.append((lo + toks[ls][1], lo + a, lo + toks[rt][2]))
    out.sort()
    keep, last = [], -1
    for ls, op, re_ in out:
        if ls >= last:
            keep.append((ls, op, re_))
            last = re_
    return keep


def spell(src, sites, mask):
    """Rewrite `src`, swapping the operands of the sites selected by `mask`."""
    parts, prev = [], 0
    for bit, (ls, op, re_) in enumerate(sites):
        if not (mask >> bit) & 1:
            continue
        parts.append(src[prev:ls])
        parts.append('%s %s %s' % (src[op + 1:re_].strip(), src[op],
                                   src[ls:op].strip()))
        prev = re_
    parts.append(src[prev:])
    return ''.join(parts)


def find_function(text, name):
    """-> (body_open, body_close) char range of the function's BODY braces."""
    for m in re.finditer(r'(^|\n)([A-Za-z_][^\n;{}]*?\b%s\s*\()' % re.escape(name),
                         text):
        i = text.find('{', m.end())
        if i < 0 or ';' in text[m.end():i]:
            continue
        depth, j = 0, i
        while j < len(text):
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0:
                    return (i, j + 1)
            j += 1
    return None


# --------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--report', default=os.path.join(ROOT, 'build/n64/report.csv'))
    ap.add_argument('--fn', help='only this function')
    ap.add_argument('--apply', action='store_true', help='write winners back')
    ap.add_argument('--max-ops', type=int, default=MAX_OPS)
    args = ap.parse_args()

    rows = [r for r in csv.DictReader(open(args.report))
            if r['status'] == 'SHAPE' and r['dist'] == '0']
    if args.fn:
        rows = [r for r in rows if r['fn'] == args.fn]
    if not rows:
        sys.exit("nothing to permute (want SHAPE rows at distance 0)")

    rom = M.Rom()
    romfn = {v: b for v, b in rom.fns}
    byfile = collections.defaultdict(list)
    for r in rows:
        byfile[r['file']].append(r)

    stat = collections.Counter()
    wins = []
    for rel, rs in sorted(byfile.items()):
        path = os.path.join(ROOT, rel)
        original = open(path).read()
        text = original
        for r in rs:
            stat['tried'] += 1
            span = find_function(text, r['fn'])
            if not span:
                stat['no_definition'] += 1
                continue
            lo, hi = span
            sites = commutative_sites(text, lo, hi)
            if not sites:
                stat['no_commutative_op'] += 1
                continue
            if len(sites) > args.max_ops:
                sites = sites[:args.max_ops]
            target = romfn.get(int(r['n64_va'], 16))
            variants = [tuple(r['opt'].split())] if r.get('opt') else [('-O2',)]

            won = None
            for mask in range(1 << len(sites)):
                trial = spell(text, sites, mask) if mask else text
                tf = tempfile.NamedTemporaryFile('w', suffix='.c', delete=False,
                                                 dir=os.path.dirname(path))
                tf.write(trial)
                tf.close()
                try:
                    for v in variants:
                        fns, err = M.compile_file(tf.name, extra=v)
                        if not fns or r['fn'] not in fns:
                            continue
                        b, rl = fns[r['fn']]
                        if M.exact(M.trim(b), rl, target):
                            won = (mask, trial)
                            break
                finally:
                    os.unlink(tf.name)
                if won:
                    break

            if won:
                stat['solved'] += 1
                mask, newtext = won
                wins.append((rel, r['fn'], r['n64_va'], bin(mask).count('1'),
                             len(sites)))
                print("  SOLVED %-28s %s  %d of %d operand(s) swapped"
                      % (r['fn'], r['n64_va'], bin(mask).count('1'), len(sites)))
                if mask:
                    text = newtext
            else:
                stat['unsolved'] += 1

        if args.apply and text != original:
            open(path, 'w').write(text)

    print()
    print("distance-0 rows tried : %d" % stat['tried'])
    print("  SOLVED (now exact)  : %d" % stat['solved'])
    print("  no commutative op   : %d" % stat['no_commutative_op'])
    print("  definition not found: %d" % stat['no_definition'])
    print("  still unsolved      : %d" % stat['unsolved'])
    if wins and not args.apply:
        print("\n(nothing written -- re-run with --apply)")


if __name__ == '__main__':
    main()
