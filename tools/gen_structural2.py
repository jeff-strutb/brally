#!/usr/bin/env python3
"""Standalone structural refine transforms for three recurring Ghidra→VC5 idioms.

Ghidra splits `return f() != 0` through a temp (setne), prints `n < K+1` for
original `n <= K`, and prints `-1 < x` / `x > -1` for original `x >= 0`.
Each generator yields `(label, mutated_source)` in the `_refine_candidates`
style so the hill-climb can fold them one edit at a time.

This does not write ghidra_work, does not edit ghidra_to_match.py, and does
not commit. Decision logic: docs/gen-structural2-notes.md.

    python3 tools/gen_structural2.py --dry-run
    python3 tools/gen_structural2.py --validate
    python3 tools/gen_structural2.py --validate --from-decomp
    python3 tools/gen_structural2.py --va 0x1006BAA0 --from-decomp --gen retnotemp
"""
from __future__ import print_function

import argparse
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
_VENV_SP = os.path.join(ROOT, '.venv', 'lib')
if os.path.isdir(_VENV_SP):
    import glob
    for _sp in glob.glob(os.path.join(_VENV_SP, 'python*', 'site-packages')):
        if _sp not in sys.path:
            sys.path.insert(0, _sp)

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
WORK_DIR = os.path.join(ROOT, 'build', 'ghidra_work')
GHIDRA_DIR = os.path.join(ROOT, 'build', 'ghidra_decomp')
LEARNINGS_CSV = os.path.join(ROOT, 'build', 'ghidra_learnings.csv')
REPORT_CSV = os.path.join(ROOT, 'build', 'match', 'report.csv')

# Distinguisher: return f() != 0 is `neg eax; sbb eax,eax; neg eax`.
# == 0 is `neg; sbb; inc`. Temp form is `xor; test; setne; mov`.
_NEG_SBB_NEG = b'\xf7\xd8\x1b\xc0\xf7\xd8'
_NEG_SBB_INC = b'\xf7\xd8\x1b\xc0\x40'
_NEG_SBB_ADD1 = b'\xf7\xd8\x1b\xc0\x83\xc0\x01'
_SETNE = b'\x0f\x95'
_SETE = b'\x0f\x94'

_C_TYPES = frozenset([
    'int', 'char', 'short', 'void', 'unsigned', 'long', 'float', 'double',
    'bool', 'undefined', 'undefined1', 'undefined2', 'undefined4',
    'HANDLE', 'HWND', 'LPCSTR', 'UINT', 'DWORD', 'BYTE', 'WORD',
    'uint', 'ulong', 'ushort', 'size_t', 'BOOL', 'HRESULT', 'MCIERROR',
    'struct', 'const', 'volatile', 'signed',
])
_C_KEYWORDS = frozenset([
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'case', 'do',
    'else', 'true', 'false', 'goto', 'break', 'continue',
])


def _split(src):
    """Same head/body cut as `_refine_candidates`."""
    marker = src.find('Forward declarations')
    if marker < 0:
        return src[:0], src
    head_end = src.find('\n\n', marker)
    if head_end < 0:
        return src, ''
    return src[:head_end], src[head_end:]


def _join(head, body):
    return head + body


# ---------------------------------------------------------------------------
# orig-byte distinguishers
# ---------------------------------------------------------------------------

def _orig(va):
    p = os.path.join(ORIG_DIR, va + '.bin')
    return open(p, 'rb').read() if os.path.exists(p) else b''


def _has_neg_sbb(b):
    """Orig `return f() != 0` / `== 0` without a temp."""
    return (_NEG_SBB_NEG in b) or (_NEG_SBB_INC in b) or (_NEG_SBB_ADD1 in b)


def _skip_push_nops(b, k, limit):
    while k < len(b) and k < limit and (0x50 <= b[k] <= 0x57 or b[k] in (0x66, 0x90)):
        k += 1
    return k


def _cmp_imm8_jccs(b, imm):
    """Offsets of `cmp r/m32, imm8` (83 /7 ib) followed by a signed jcc.

    jg/jle (7f/7e) is `n <= K`; jl/jge (7c/7d) is `n < K+1`. A push between
    the cmp and the jcc is legal (0x100378C0: `cmp esi,8; push edi; jg`).
    """
    if not (0 <= imm <= 0xff):
        return []
    out = []
    i = 0
    imm_b = imm & 0xff
    while True:
        j = b.find(b'\x83', i)
        if j < 0 or j + 2 >= len(b):
            break
        modrm = b[j + 1]
        if ((modrm >> 3) & 7) == 7 and b[j + 2] == imm_b:
            k = _skip_push_nops(b, j + 3, j + 10)
            if k < len(b) and b[k] in (0x7c, 0x7d, 0x7e, 0x7f):
                out.append((j, b[k]))
            elif k + 1 < len(b) and b[k] == 0x0f and b[k + 1] in (0x8c, 0x8d, 0x8e, 0x8f):
                out.append((j, 0x100 + b[k + 1]))
        i = j + 1
    return out


def _orig_wants_le(orig, k):
    """True if orig compares against K (`n <= K`) not K+1 (`n < K+1`)."""
    if k < 0:
        return False
    le = _cmp_imm8_jccs(orig, k)
    lt = _cmp_imm8_jccs(orig, k + 1)
    le_hit = any(jcc in (0x7e, 0x7f, 0x18e, 0x18f) for _off, jcc in le)
    lt_hit = any(jcc in (0x7c, 0x7d, 0x18c, 0x18d) for _off, jcc in lt)
    return le_hit and not lt_hit


def _has_test_jl(b):
    """`test r,r; jl` — orig `if (x >= 0)`. Pushes may sit between test and jl
    (0x1006E0A0: `test eax,eax; push esi; push edi; jl`)."""
    for i in range(len(b) - 2):
        if b[i] != 0x85:
            continue
        k = _skip_push_nops(b, i + 2, i + 10)
        if k < len(b) and b[k] == 0x7c:
            return True
        if k + 1 < len(b) and b[k] == 0x0f and b[k + 1] == 0x8c:
            return True
    return False


def _has_cmp_m1_jle(b):
    """`cmp r, -1; jle` — Ghidra's `x > -1` / `-1 < x` lowering."""
    hits = _cmp_imm8_jccs(b, 0xff)
    return any(jcc in (0x7e, 0x7c, 0x7f, 0x7d, 0x18e, 0x18c) for _off, jcc in hits)


# ---------------------------------------------------------------------------
# 1. i = f(...); return i != 0;  →  return f(...) != 0;
# ---------------------------------------------------------------------------

# Optional Ghidra/wrap cast around the compare: `(uint)(i != 0)`,
# `(unsigned int)(i != 0)`, `(int)(i != 0)`.
_RET_CMP = re.compile(
    r'return\s+'
    r'(?:\(\s*(?:unsigned\s+)?(?:int|uint)\s*\)\s*)?'
    r'\(?\s*'
    r'(?P<tmp>\w+)\s*(?P<op>!=|==)\s*0'
    r'\s*\)?'
    r'\s*;'
)


def _is_call_expr(expr):
    """True if expr is a function call, not a cast-deref or a load."""
    s = expr.strip()
    if not s:
        return False
    if s.startswith('*') and not s.startswith('(*'):
        return False
    for m in re.finditer(r'([A-Za-z_]\w*)\s*\(', s):
        if m.group(1) not in _C_TYPES and m.group(1) not in _C_KEYWORDS:
            return True
    if re.search(r'\(\s*\*', s):
        return True
    return False


def _stmt_before(body, pos):
    """Return (start, semi, stmt_text) of the statement ending at the `;`
    immediately before pos, or None."""
    i = pos
    while i > 0 and body[i - 1] in ' \t\n':
        i -= 1
    if i == 0 or body[i - 1] != ';':
        return None
    semi = i - 1
    j = semi - 1
    depth = 0
    start = 0
    while j >= 0:
        ch = body[j]
        if ch in ')}':
            depth += 1
        elif ch in '({':
            if depth == 0:
                start = j + 1
                break
            depth -= 1
        elif ch == ';' and depth == 0:
            start = j + 1
            break
        j -= 1
    stmt = body[start:semi].strip()
    if not stmt:
        return None
    return start, semi, stmt


def _split_assign(stmt):
    """`tmp = expr` at depth 0. Returns (tmp, expr) or None."""
    depth = 0
    for i, ch in enumerate(stmt):
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        elif ch == '=' and depth == 0:
            if i > 0 and stmt[i - 1] in '=!<>':
                return None
            if i + 1 < len(stmt) and stmt[i + 1] == '=':
                return None
            lhs = stmt[:i].strip()
            rhs = stmt[i + 1:].strip()
            if re.match(r'^[A-Za-z_]\w*$', lhs) and rhs:
                return lhs, rhs
            return None
    return None


def _iter_ret_temp(body):
    """Yield dicts for `tmp = call(...); return tmp != 0;` (and `== 0`,
    and Ghidra's `(uint)(tmp != 0)` wrapper). Adjacent statements only."""
    for m in _RET_CMP.finditer(body):
        prev = _stmt_before(body, m.start())
        if prev is None:
            continue
        start, _semi, stmt = prev
        parts = _split_assign(stmt)
        if parts is None:
            continue
        tmp, expr = parts
        if tmp != m.group('tmp'):
            continue
        if not _is_call_expr(expr):
            continue
        # Start of the assignment line so a multiline call is consumed with it.
        asgn_i = body.find(tmp, start, _semi)
        if asgn_i < 0:
            asgn_i = start
        line_start = body.rfind('\n', 0, asgn_i) + 1
        indent = re.match(r'[ \t]*', body[line_start:]).group(0)
        yield {
            'start': line_start,
            'end': m.end(),
            'tmp': tmp,
            'op': m.group('op'),
            'call': expr,
            'indent': indent,
        }


def gen_ret_notemp(src):
    """Ghidra `i = f(...); return i != 0;` → `return f(...) != 0;`.

    Distinguisher: orig `f7 d8 1b c0 f7 d8` (neg; sbb; neg). The temp form
    is `xor r,r; test eax; setne r; mov eax,r` (+3). `return f(...) == 0`
    is `neg; sbb; inc` (`f7 d8 1b c0 40`). Proven MATCH 0x1006BAA0 /
    0x1006B6E0 / 0x1006BB10 (sound wrappers). Also fires on the uint-cast
    spelling `return (uint)(i != 0)` (0x10002EB0 / 0x10002F10).

    Yields `retnotemp:ne:TMP` / `retnotemp:eq:TMP`.
    """
    head, body = _split(src)
    for i, site in enumerate(_iter_ret_temp(body)):
        op = 'ne' if site['op'] == '!=' else 'eq'
        repl = '%sreturn %s %s 0;' % (
            site['indent'], site['call'], site['op'])
        nb = body[:site['start']] + repl + body[site['end']:]
        yield ('retnotemp:%s:%s' % (op, site['tmp'][:20]), _join(head, nb))


def transform_ret_notemp(src, orig=None):
    """Apply every adjacent `i = f(); return i != 0;` fold."""
    labels = []
    changed = True
    while changed:
        changed = False
        for label, cand in gen_ret_notemp(src):
            if cand != src:
                src = cand
                labels.append(label)
                changed = True
                break
    return src, labels


# ---------------------------------------------------------------------------
# 2. n < K+1  →  n <= K
# ---------------------------------------------------------------------------

# Same shape as `_refine_candidates` (i) `X > C` → `X >= C+1`, the dual.
# Requires wrapping parens, so `for (i = 0; i < 8; i++)` (no extra parens
# around the test) does not fire; `if (param_1 < 9)` does.
_LEBOUND = re.compile(
    r'\(([^()<>=!&|]+?)\s*<\s*(0x[0-9a-fA-F]+|\d+)\)'
)


def _fmt_imm(tok, val):
    tok = tok.strip()
    if tok.lower().startswith('0x'):
        return '0x%x' % val
    return '%d' % val


def _is_narrow_temp(name):
    """Ghidra char/short temps (`cVar4`, `sVar2`). Their `-1 <` / `< 0x80`
    is an ASCII window, not the int `>= 0` / `<= K` idiom — orig is often
    `cmp r8, -1` even when a dword `test; jl` lives elsewhere in the
    function (0x100541B0 / 0x10054280 scored worse)."""
    return bool(re.match(r'[cs]Var\d+$', name.strip()))


def gen_lebound(src):
    """Ghidra `if (n < K+1)` → `if (n <= K)`.

    Distinguisher: orig `cmp n, K; jg` (`83 xx K; 7f`) vs `cmp n, K+1; jge`.
    Proven MATCH 0x100378C0 (`param_1 < 9` → `param_1 <= 8`). Dual of
    `_refine_candidates` (i) `X > C` → `X >= C+1`. Yields `lebound:X<=K`.
    """
    head, body = _split(src)
    for m in _LEBOUND.finditer(body):
        x, tok = m.group(1).strip(), m.group(2)
        c = int(tok, 0)
        if c < 1:
            continue
        if _is_narrow_temp(x):
            continue
        new_c = _fmt_imm(tok, c - 1)
        nb = (body[:m.start()] + '(%s <= %s)' % (x, new_c) + body[m.end():])
        yield ('lebound:%s<=%s' % (x[:16], new_c), _join(head, nb))


def transform_lebound(src, orig=None):
    """Apply every `X < C` → `X <= C-1`. If orig bytes are given, keep only
    sites whose orig is `cmp C-1; jg/jle` (not `cmp C; jl/jge`)."""
    labels = []
    changed = True
    while changed:
        changed = False
        for label, cand in gen_lebound(src):
            if cand == src:
                continue
            if orig is not None:
                # label is lebound:X<=K — recover K
                ktok = label.split('<=', 1)[-1]
                try:
                    k = int(ktok, 0)
                except ValueError:
                    k = None
                if k is None or not _orig_wants_le(orig, k):
                    continue
            src = cand
            labels.append(label)
            changed = True
            break
    return src, labels


# ---------------------------------------------------------------------------
# 3. -1 < x  /  x > -1  →  x >= 0
# ---------------------------------------------------------------------------

_GT_M1 = re.compile(
    r'\((?:\((?:unsigned )?int\)\s*)?([^()<>=!&|]+?)\s*>\s*-1\)'
)


def _expr_end(s, i):
    """End index of a comparison operand starting at i (paren-aware)."""
    n = len(s)
    depth = 0
    while i < n:
        ch = s[i]
        if ch == '(' or ch == '[':
            depth += 1
            i += 1
            continue
        if ch == ')' or ch == ']':
            if depth == 0:
                return i
            depth -= 1
            i += 1
            continue
        if depth == 0:
            if s.startswith('&&', i) or s.startswith('||', i):
                return i
            if ch in ',;?':
                return i
            if s.startswith('==', i) or s.startswith('!=', i):
                return i
            if s.startswith('<=', i) or s.startswith('>=', i):
                return i
            if s.startswith('<<', i) or s.startswith('>>', i):
                i += 2
                continue
            if ch in '<>':
                return i
        i += 1
    return i


def _iter_neg1_lt(body):
    """Yield (start, end, rhs) for a token `-1 < rhs` (not `+ -1`, not `<<`)."""
    i = 0
    n = len(body)
    while True:
        j = body.find('-1', i)
        if j < 0:
            return
        if j > 0 and (body[j - 1].isalnum() or body[j - 1] in '._xX'):
            i = j + 1
            continue
        k = j - 1
        while k >= 0 and body[k] in ' \t':
            k -= 1
        if k >= 0 and body[k] in '+-*/%&|^~':
            i = j + 1
            continue
        rest = j + 2
        while rest < n and body[rest] in ' \t':
            rest += 1
        if rest >= n or body[rest] != '<':
            i = j + 1
            continue
        if rest + 1 < n and body[rest + 1] in '<=':
            i = j + 1
            continue
        rhs_i = rest + 1
        while rhs_i < n and body[rhs_i] in ' \t':
            rhs_i += 1
        rhs_end = _expr_end(body, rhs_i)
        rhs = body[rhs_i:rhs_end].strip()
        if rhs:
            yield j, rhs_end, rhs
        i = max(rhs_end, j + 2)


def gen_ge0(src):
    """Ghidra `-1 < x` / `x > -1` → `x >= 0`.

    Distinguisher: orig `test r,r; jl` (`85 xx 7c`) vs `cmp r, -1; jle`
    (`83 xx ff 7e`). Proven as the opening of 0x1006E130 (`(-1 < param_1)
    && (param_1 < 8)` → `(param_1 >= 0) && (param_1 < 8)`). Yields
    `ge0:lt:X` / `ge0:gt:X`.
    """
    head, body = _split(src)
    for start, end, rhs in _iter_neg1_lt(body):
        ident = re.match(r'(?:\((?:unsigned )?\w+\)\s*)?(\w+)', rhs.strip())
        if ident and _is_narrow_temp(ident.group(1)):
            continue
        repl = '(%s >= 0)' % rhs
        nb = body[:start] + repl + body[end:]
        yield ('ge0:lt:%s' % re.sub(r'\s+', '', rhs)[:24], _join(head, nb))
    for m in _GT_M1.finditer(body):
        x = m.group(1).strip()
        if _is_narrow_temp(x):
            continue
        nb = body[:m.start()] + '(%s >= 0)' % x + body[m.end():]
        yield ('ge0:gt:%s' % x[:24], _join(head, nb))


def transform_ge0(src, orig=None):
    """Apply every `-1 < x` / `x > -1` → `x >= 0`."""
    labels = []
    changed = True
    while changed:
        changed = False
        for label, cand in gen_ge0(src):
            if cand != src:
                src = cand
                labels.append(label)
                changed = True
                break
    return src, labels


GENERATORS = [
    ('retnotemp', gen_ret_notemp, transform_ret_notemp),
    ('lebound', gen_lebound, transform_lebound),
    ('ge0', gen_ge0, transform_ge0),
]


# ---------------------------------------------------------------------------
# Prey filters (residue + orig-byte distinguisher)
# ---------------------------------------------------------------------------

def residue_rows():
    if not os.path.exists(LEARNINGS_CSV):
        return []
    tree_matched = set()
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            tree_matched = {r['va'].lower() for r in csv.DictReader(f)
                            if r.get('status') == 'match'}
    out = []
    with open(LEARNINGS_CSV) as f:
        for r in csv.DictReader(f):
            if not r.get('divergence') or r['divergence'] in ('', 'match'):
                continue
            if r['va'].lower() in tree_matched:
                continue
            out.append(r)
    return out


def prey_ret_notemp(src, orig, va=None):
    if any(True for _ in gen_ret_notemp(src)):
        return True
    # orig has the sbb form but the wrap may already have folded — still
    # count as prey for --from-decomp extras.
    if orig and _has_neg_sbb(orig) and re.search(
            r'return\s+(?:\([^;]*\)\s*)?\w+\s*(!=|==)\s*0', src):
        return any(True for _ in gen_ret_notemp(src))
    return False


def prey_lebound(src, orig, va=None):
    if orig is not None and orig != b'':
        for label, _cand in gen_lebound(src):
            ktok = label.split('<=', 1)[-1]
            try:
                k = int(ktok, 0)
            except ValueError:
                continue
            if _orig_wants_le(orig, k):
                return True
        return False
    return any(True for _ in gen_lebound(src))


def prey_ge0(src, orig, va=None):
    if not any(True for _ in gen_ge0(src)):
        return False
    if orig is None or orig == b'':
        return True
    # Ghidra prints `-1 < x` for BOTH `x >= 0` (test; jl) and `x > -1`
    # (cmp -1; jle). Only the test; jl spelling is this idiom.
    if _has_test_jl(orig):
        return True
    return False


PREY_FN = {
    'retnotemp': prey_ret_notemp,
    'lebound': prey_lebound,
    'ge0': prey_ge0,
}

EXTRAS = {
    'retnotemp': [
        '0x1006BAA0', '0x1006B6E0', '0x1006BB10', '0x1006B4F0',
        '0x10058F90', '0x10002830', '0x10002F70', '0x10002EB0',
        '0x10002F10',
    ],
    'lebound': ['0x100378C0'],
    'ge0': ['0x1006E130', '0x1006E0A0', '0x100031D0', '0x10006460'],
}


# ---------------------------------------------------------------------------
# Load / score
# ---------------------------------------------------------------------------

def load_wrapped(va_hex, from_decomp=False):
    """Return (src, func_name). Never writes ghidra_work."""
    import ghidra_to_match as g
    work = os.path.join(WORK_DIR, va_hex + '.c')
    if not from_decomp and os.path.exists(work):
        src = open(work).read()
        m = re.search(
            r'\b(FUN_[0-9a-fA-F]+|THUNK_[0-9a-fA-F]+)\s*\([^;]*\)\s*\n?\s*\{',
            src)
        name = m.group(1) if m else 'FUN_' + va_hex[2:]
        return src, name
    funcs = g.load_functions()
    target = None
    for f in funcs:
        if int(f['va'], 16) == int(va_hex, 16):
            target = f
            break
    if target is None:
        raise SystemExit('VA %s not in functions_glide.csv' % va_hex)
    gmap = g.load_globals()
    fn_names = g.load_fn_names()
    func_name, cleaned = g.prepare_function(target, gmap, fn_names)
    orig_file = os.path.join(ORIG_DIR, va_hex + '.bin')
    if os.path.exists(orig_file):
        import match_sweep
        orig_bytes = match_sweep.load_orig(orig_file, va_hex)
        cleaned = g.fix_calling_convention(cleaned, func_name, orig_bytes)
    src = g.wrap_for_compile(cleaned, va_hex)
    return src, func_name


def score_src(src, func_name, va_hex, tag):
    import ghidra_to_match as g
    import match_sweep
    orig = match_sweep.load_orig(
        os.path.join(ORIG_DIR, va_hex + '.bin'), va_hex)
    r = g._score_source(src, func_name, orig,
                        ['/O2', '/Od', '/O2 /Oy-'], tag)
    return r[0], r[1]


def _norm_va(va):
    if not va.lower().startswith('0x'):
        va = '0x' + va
    return '0x%08X' % int(va, 16)


def _apply(transform, src, orig):
    return transform(src, orig)


def run_one(va_hex, gen_name=None, from_decomp=False, verbose=True,
            score=True):
    va_hex = _norm_va(va_hex)
    src, fname = load_wrapped(va_hex, from_decomp=from_decomp)
    orig = _orig(va_hex)
    gens = GENERATORS
    if gen_name:
        gens = [g for g in GENERATORS if g[0] == gen_name]
        if not gens:
            raise SystemExit('unknown generator %s' % gen_name)
    report = []
    new_src = src
    for name, gen, transform in gens:
        if not PREY_FN[name](new_src, orig, va_hex) and gen_name is None:
            continue
        out, labels = _apply(transform, new_src, orig)
        n_yield = sum(1 for _ in gen(new_src))
        report.append((name, labels, n_yield, out != new_src))
        if verbose:
            print('  %s: %d yields, applied %s' % (
                name, n_yield, labels or '(no-op)'))
        new_src = out
    if not score:
        return {
            'va': va_hex, 'before': None, 'after': None,
            'report': report, 'src': new_src, 'fname': fname,
        }
    tag = ('g2d' if from_decomp else 'g2w') + va_hex[-6:]
    before = score_src(src, fname, va_hex, tag + 'b')
    after = score_src(new_src, fname, va_hex, tag + 'a')
    if verbose:
        print('  diffs  %s → %s   opt %s → %s' % (
            before[0], after[0], before[1], after[1]))
        if after[0] == 0:
            print('  MATCH')
    return {
        'va': va_hex,
        'before': before[0],
        'after': after[0],
        'bopt': before[1],
        'aopt': after[1],
        'report': report,
        'match': after[0] == 0,
        'moved': (after[0] is not None and before[0] is not None
                  and after[0] < before[0]),
        'fname': fname,
    }


def _prey_vas(gen_name, from_decomp=False):
    """Residue VAs that look like prey for this generator."""
    vas = []
    for r in residue_rows():
        va = _norm_va(r['va'])
        work = os.path.join(WORK_DIR, va + '.c')
        decomp = os.path.join(GHIDRA_DIR, va + '.c')
        if from_decomp:
            if not os.path.exists(decomp):
                continue
            try:
                src, _ = load_wrapped(va, from_decomp=True)
            except Exception:
                continue
        else:
            if not os.path.exists(work):
                continue
            src = open(work).read()
        orig = _orig(va)
        if PREY_FN[gen_name](src, orig, va):
            vas.append(va)
    return vas


def _score_pair(args):
    va, gen_name, from_decomp = args
    import ghidra_to_match as g  # noqa: F401
    src, fname = load_wrapped(va, from_decomp=from_decomp)
    orig = _orig(va)
    transform = dict((n, t) for n, _g, t in GENERATORS)[gen_name]
    gen = dict((n, g) for n, g, _t in GENERATORS)[gen_name]
    new_src, labels = _apply(transform, src, orig)
    if new_src == src:
        return {
            'va': va, 'before': None, 'after': None, 'noop': True,
            'labels': labels, 'yields': sum(1 for _ in gen(src)),
        }
    tag = ('g2d' if from_decomp else 'g2w') + va[-6:] + gen_name[:2]
    b = score_src(src, fname, va, tag + 'b')
    a = score_src(new_src, fname, va, tag + 'a')
    return {
        'va': va,
        'before': b[0], 'after': a[0],
        'bopt': b[1], 'aopt': a[1],
        'noop': False,
        'labels': labels,
        'yields': sum(1 for _ in gen(src)),
        'match': a[0] == 0,
        'moved': (a[0] is not None and b[0] is not None and a[0] < b[0]),
        'worse': (a[0] is not None and b[0] is not None and a[0] > b[0]),
    }


def run_validate(from_decomp=False, gens=None, max_prey=None, workers=4):
    from concurrent.futures import ProcessPoolExecutor, as_completed
    gens = gens or [g[0] for g in GENERATORS]
    print('residue unmatched: %d' % len(residue_rows()), flush=True)
    print('source: %s' % ('decomp-wrap' if from_decomp else 'ghidra_work'),
          flush=True)
    summary = []
    for gen_name in gens:
        vas = _prey_vas(gen_name, from_decomp=from_decomp)
        for e in EXTRAS.get(gen_name, []):
            e = _norm_va(e)
            if e not in vas:
                work = os.path.join(WORK_DIR, e + '.c')
                decomp = os.path.join(GHIDRA_DIR, e + '.c')
                if from_decomp and os.path.exists(decomp):
                    vas.append(e)
                elif not from_decomp and os.path.exists(work):
                    vas.append(e)
        if max_prey:
            vas = vas[:max_prey]
        print('\n======== %s  prey %d ========' % (gen_name, len(vas)),
              flush=True)
        rows = []
        jobs = [(va, gen_name, from_decomp) for va in vas]
        if workers <= 1:
            for job in jobs:
                print('  scoring %s' % job[0], flush=True)
                try:
                    rows.append(_score_pair(job))
                except Exception as e:
                    print('  ERROR', job[0], type(e).__name__, e, flush=True)
                    rows.append({'va': job[0], 'error': str(e)})
        else:
            with ProcessPoolExecutor(max_workers=workers) as pool:
                futs = {pool.submit(_score_pair, job): job[0] for job in jobs}
                for fut in as_completed(futs):
                    va = futs[fut]
                    try:
                        r = fut.result()
                    except Exception as e:
                        print('  ERROR', va, type(e).__name__, e, flush=True)
                        r = {'va': va, 'error': str(e)}
                    rows.append(r)
                    if r.get('error'):
                        continue
                    mark = ''
                    if r.get('noop'):
                        mark = ' no-op'
                    elif r.get('match'):
                        mark = ' MATCH'
                    elif r.get('moved'):
                        mark = ' MOVE'
                    elif r.get('worse'):
                        mark = ' worse'
                    print('  %-12s %8s → %8s  %s%s' % (
                        r['va'], r.get('before'), r.get('after'),
                        ' '.join(r.get('labels') or [])[:40], mark),
                          flush=True)
        n_prey = len(vas)
        n_fire = sum(1 for r in rows if not r.get('noop') and not r.get('error')
                     and r.get('labels'))
        n_move = sum(1 for r in rows if r.get('moved'))
        n_match_abs = sum(1 for r in rows if r.get('match'))
        n_worse = sum(1 for r in rows if r.get('worse'))
        n_err = sum(1 for r in rows if r.get('error'))
        examples_move = [r['va'] for r in rows if r.get('moved')]
        examples_match = [r['va'] for r in rows if r.get('match')]
        examples_worse = [r['va'] for r in rows if r.get('worse')]
        summary.append({
            'gen': gen_name, 'prey': n_prey, 'fired': n_fire,
            'moved': n_move, 'matched': n_match_abs, 'worse': n_worse,
            'err': n_err, 'ex_move': examples_move, 'ex_match': examples_match,
            'ex_worse': examples_worse, 'rows': rows,
        })
        print('  prey=%d fired=%d moved=%d matched=%d worse=%d err=%d' % (
            n_prey, n_fire, n_move, n_match_abs, n_worse, n_err), flush=True)
        if examples_move:
            print('  moved VAs: %s' % ' '.join(examples_move), flush=True)
        if examples_match:
            print('  MATCH VAs: %s' % ' '.join(examples_match), flush=True)
        if examples_worse:
            print('  worse VAs: %s' % ' '.join(examples_worse), flush=True)
    print('\n======== summary ========')
    print('%-12s %5s %5s %5s %5s  hit-rate' % (
        'gen', 'prey', 'move', 'MATCH', 'worse'))
    for s in summary:
        rate = (s['moved'] / s['prey']) if s['prey'] else 0
        print('%-12s %5d %5d %5d %5d  %d/%d = %.0f%%' % (
            s['gen'], s['prey'], s['moved'], s['matched'], s['worse'],
            s['moved'], s['prey'], 100 * rate))
    return summary


def run_dry(from_decomp=False, gens=None, max_prey=None):
    gens = gens or [g[0] for g in GENERATORS]
    for gen_name in gens:
        vas = _prey_vas(gen_name, from_decomp=from_decomp)
        for e in EXTRAS.get(gen_name, []):
            e = _norm_va(e)
            if e not in vas:
                vas.append(e)
        if max_prey:
            vas = vas[:max_prey]
        print('\n======== %s  prey %d (dry) ========' % (gen_name, len(vas)))
        gen = dict((n, g) for n, g, _t in GENERATORS)[gen_name]
        transform = dict((n, t) for n, _g, t in GENERATORS)[gen_name]
        for va in vas:
            try:
                src, _ = load_wrapped(va, from_decomp=from_decomp)
            except Exception as e:
                print('  %s  LOAD %s' % (va, e))
                continue
            orig = _orig(va)
            yields = list(gen(src))
            out, labels = _apply(transform, src, orig)
            print('  %s  yields=%d  applied=%s  changed=%s' % (
                va, len(yields), labels or '-', out != src))
            for lab, cand in yields[:8]:
                print('      - %s  Δ%d chars' % (lab, abs(len(cand) - len(src))))


def _self_test():
    """Snippet checks — no MSVC. Fail loud if a proven spelling stopped matching."""
    fails = []

    def check(name, cond):
        if not cond:
            fails.append(name)

    wrap = ('/* x */\n#ifdef BR_MATCHING_BUILD\n'
            'typedef int (*funcptr)();\n'
            '/* Forward declarations for unknown functions/globals */\n'
            'int FUN_1006b790();\n'
            '\n')
    foot = '\n#endif\n'

    # retnotemp != 0
    body = (
        'int FUN_1006baa0(int param_1)\n{\n  int iVar1;\n'
        '  iVar1 = FUN_1006b790(param_1, 0);\n'
        '  return iVar1 != 0;\n}\n'
    )
    src = wrap + body + foot
    out, labs = transform_ret_notemp(src)
    check('ret-ne-fold',
          'return FUN_1006b790(param_1, 0) != 0;' in out and labs)
    check('ret-ne-no-temp-left',
          'iVar1 = FUN_1006b790' not in out.split('int iVar1;')[-1])

    # retnotemp == 0
    body = (
        'int FUN_x(void)\n{\n  int iVar1;\n'
        '  iVar1 = FUN_1006b790(1);\n'
        '  return iVar1 == 0;\n}\n'
    )
    out, labs = transform_ret_notemp(wrap + body + foot)
    check('ret-eq-fold', 'return FUN_1006b790(1) == 0;' in out)

    # uint-cast spelling
    body = (
        'unsigned int FUN_x(void)\n{\n  int iVar2;\n'
        '  iVar2 = (*DAT_104b162c)(g, 4);\n'
        '  return (unsigned int)(iVar2 != 0);\n}\n'
    )
    out, labs = transform_ret_notemp(wrap + body + foot)
    check('ret-uint-fold', 'return (*DAT_104b162c)(g, 4) != 0;' in out)

    # do not fold a load + intervening store (0x1006B530)
    body = (
        'int FUN_x(void)\n{\n  int iVar1;\n'
        '  iVar1 = DAT_100b55f8[1];\n'
        '  g[0] = iVar1;\n'
        '  return iVar1 != 0;\n}\n'
    )
    out, labs = transform_ret_notemp(wrap + body + foot)
    check('ret-skip-load', out.endswith(body + foot) or not labs)

    # lebound
    body = 'void FUN_x(int param_1)\n{\n  if (param_1 < 9) {\n    return;\n  }\n}\n'
    out, labs = transform_lebound(wrap + body + foot)
    check('le-9-to-8', '(param_1 <= 8)' in out)
    check('le-not-lt9', '(param_1 < 9)' not in out)

    # for-loop `i < 8` has no extra parens — must not fire
    body = (
        'void FUN_x(void)\n{\n  int i;\n'
        '  for (i = 0; i < 8; i = i + 1) {\n  }\n}\n'
    )
    labs = [l for l, _ in gen_lebound(wrap + body + foot)]
    check('le-skip-for', not labs)

    # ge0: -1 < param
    body = (
        'void FUN_x(int param_1)\n{\n'
        '  if ((-1 < param_1) && (param_1 < 8)) {\n    return;\n  }\n}\n'
    )
    out, labs = transform_ge0(wrap + body + foot)
    check('ge0-param', '(param_1 >= 0)' in out)
    check('ge0-no-neg1', '-1 < param_1' not in out)
    check('ge0-keep-lt8', 'param_1 < 8' in out)

    # ge0: (int) cast
    body = (
        'void FUN_x(unsigned int param_1)\n{\n'
        '  if ((-1 < (int)param_1) && ((int)param_1 < 0x40)) {\n'
        '    return;\n  }\n}\n'
    )
    out, labs = transform_ge0(wrap + body + foot)
    check('ge0-cast', '(int)param_1 >= 0' in out)
    check('ge0-cast-keep-upper', 'param_1 < 0x40' in out)

    # ge0: x > -1
    body = 'void FUN_x(int x)\n{\n  if (x > -1) {\n    return;\n  }\n}\n'
    out, labs = transform_ge0(wrap + body + foot)
    check('ge0-gt', '(x >= 0)' in out)

    # ge0: do not eat `+ -1 <`
    body = (
        'void FUN_x(int iVar1, int DAT)\n{\n'
        '  if (iVar1 + -1 < DAT) {\n    return;\n  }\n}\n'
    )
    labs = [l for l, _ in gen_ge0(wrap + body + foot)]
    check('ge0-skip-plus-neg1', not labs)

    # ge0/lebound: skip Ghidra char/short temps (ASCII window, 0x100541B0)
    body = (
        'void FUN_x(char cVar4, short sVar2)\n{\n'
        '  if ((-1 < sVar2) && (sVar2 < 0x80)) {\n    return;\n  }\n'
        '  if (-1 < cVar4) {\n    return;\n  }\n}\n'
    )
    src = wrap + body + foot
    check('ge0-skip-svar', not [l for l, _ in gen_ge0(src)])
    check('le-skip-svar', not [l for l, _ in gen_lebound(src)])

    n_ok = 17 - len(fails)
    if fails:
        raise SystemExit('self-test FAILED: %s' % ', '.join(fails))
    print('self-test: %d checks ok' % n_ok, flush=True)


def main():
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except Exception:
        pass
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--va', help='single VA')
    ap.add_argument('--gen', choices=[g[0] for g in GENERATORS],
                    help='restrict to one generator')
    ap.add_argument('--from-decomp', action='store_true',
                    help='wrap build/ghidra_decomp (isolates the transform)')
    ap.add_argument('--validate', action='store_true',
                    help='score every residue prey of each generator')
    ap.add_argument('--dry-run', action='store_true',
                    help='print yields, no MSVC score')
    ap.add_argument('--no-score', action='store_true')
    ap.add_argument('--max-prey', type=int, default=0)
    ap.add_argument('--workers', type=int, default=4)
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args()

    if args.self_test or args.dry_run or args.validate:
        _self_test()
        if args.self_test and not (args.dry_run or args.validate or args.va):
            return

    if args.dry_run and not args.va:
        run_dry(from_decomp=args.from_decomp,
                gens=[args.gen] if args.gen else None,
                max_prey=args.max_prey or None)
        return
    if args.validate:
        run_validate(from_decomp=args.from_decomp,
                     gens=[args.gen] if args.gen else None,
                     max_prey=args.max_prey or None,
                     workers=args.workers)
        return
    if not args.va:
        ap.print_help()
        return
    run_one(args.va, gen_name=args.gen, from_decomp=args.from_decomp,
            verbose=True, score=not args.no_score)


if __name__ == '__main__':
    main()
