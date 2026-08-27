#!/usr/bin/env python3
"""Standalone structural refine transforms for four recurring Ghidra→VC5 idioms.

Ghidra explodes /Oi string ops, peels signed jl-back loops, prints a 16-bit
field as a byte pair, and leaves callees as empty `int f();` (float args
promote to double). Each generator yields `(label, mutated_source)` in the
`_refine_candidates` style so the hill-climb can fold them one edit at a time.

This does not write ghidra_work, does not edit ghidra_to_match.py, and does
not commit. Decision logic: docs/gen-structural-notes.md.

    python3 tools/gen_structural.py --dry-run
    python3 tools/gen_structural.py --validate
    python3 tools/gen_structural.py --va 0x1006FF50 --gen strarr
    python3 tools/gen_structural.py --va 0x1006EBC0 --from-decomp --gen floatproto
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

# CRT /Oi string ops. Distinguisher in orig: `or ecx,-1; repne scasb; not ecx;
# shr ecx,2; rep movsd` (VC5-IDIOMS-dll.md, proven 0x1006FF50).
_SCASB = b'\xf2\xae'
_MOVSD = b'\xf3\xa5'
# 16-bit BE reconstruct: `xor r,r; mov al; mov ah; mov word` (0x10031960).
_XOR_EAX = b'\x33\xc0'
_MOV_WORD = b'\x66\x89'
# fstp dword [esp] / [esp+disp8] vs fstp qword — empty `int f();` promotes.
_FSTP_DWORD = (b'\xd9\x1c\x24', b'\xd9\x5c\x24')
_FSTP_QWORD = (b'\xdd\x1c\x24', b'\xdd\x5c\x24')

_STRING_FUNS = frozenset([
    'strcpy', 'strcat', 'strstr', 'strcmp', 'strlen', 'sprintf', 'sscanf',
    'fopen', 'printf', 'fprintf', 'wsprintfA', 'OutputDebugStringA',
])
_C_KEYWORDS = frozenset([
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'case', 'do',
    'void', 'int', 'char', 'short', 'float', 'double', 'long', 'unsigned',
    'struct', 'typedef', 'static', 'const', 'volatile', 'goto', 'break',
    'continue', 'else', 'true', 'false',
])
# Already prototyped via windows.h / math.h / string.h — do not redeclare.
_SKIP_PROTO = frozenset([
    'ftol', 'floor', 'ceil', 'sqrt', 'sin', 'cos', 'tan', 'atan2', 'fabs',
    'strcpy', 'strcat', 'strstr', 'strcmp', 'strlen', 'sprintf', 'sscanf',
    'fopen', 'printf', 'fprintf', 'memcpy', 'memset', 'memcmp', 'malloc',
    'free', 'exit', 'qsort', 'toupper', 'tolower', 'atoi', 'atof',
    'WaitForSingleObject', 'ReleaseMutex', 'CloseHandle',
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


def _brace_block(s, open_idx):
    """Return (inner, end_idx_after_close) for the `{...}` at open_idx."""
    if open_idx < 0 or open_idx >= len(s) or s[open_idx] != '{':
        return None, None
    depth = 0
    for i in range(open_idx, len(s)):
        if s[i] == '{':
            depth += 1
        elif s[i] == '}':
            depth -= 1
            if depth == 0:
                return s[open_idx + 1:i], i + 1
    return None, None


# ---------------------------------------------------------------------------
# 1. exploded repne scasb / rep movsd → strcpy/strcat + extern char s_*[]
# ---------------------------------------------------------------------------

# Ghidra's inlined /Oi copy: dword loop (len>>2) then residual bytes (len&3).
# `_strcpy_sub` in ghidra_to_match requires `src = walker + -len` with no
# intervening statements and no dest-scan; residue still has the walker-rewind
# form (`walker = walker + -len; dest = D;`) and strcat (dest scanned first).
_COPY_LOOPS = re.compile(
    r'for\s*\(\s*(?P<n>\w+)\s*=\s*(?P<u>\w+)\s*>>\s*2\s*;\s*(?P=n)\s*!=\s*0\s*;'
    r'\s*(?P=n)\s*=\s*(?P=n)\s*-\s*1\s*\)\s*\{\s*'
    r'\*\(\s*(?:unsigned\s+)?(?:int|long)\s*\*\s*\)\s*(?P<dst>\w+)\s*=\s*'
    r'\*\(\s*(?:unsigned\s+)?(?:int|long)\s*\*\s*\)\s*(?P<src>\w+)\s*;\s*'
    r'(?P=src)\s*=\s*(?P=src)\s*\+\s*4\s*;\s*'
    r'(?P=dst)\s*=\s*(?P=dst)\s*\+\s*4\s*;\s*'
    r'\}\s*'
    r'for\s*\(\s*(?P=u)\s*=\s*(?P=u)\s*&\s*3\s*;\s*(?P=u)\s*!=\s*0\s*;'
    r'\s*(?P=u)\s*=\s*(?P=u)\s*-\s*1\s*\)\s*\{\s*'
    r'\*(?P=dst)\s*=\s*\*(?P=src)\s*;\s*'
    r'(?P=src)\s*=\s*(?P=src)\s*\+\s*1\s*;\s*'
    r'(?P=dst)\s*=\s*(?P=dst)\s*\+\s*1\s*;\s*'
    r'\}',
    re.S,
)

_SCAN_DO = re.compile(
    r'do\s*\{\s*'
    r'(?:(?P<w>\w+)\s*=\s*(?P<p>\w+)\s*;\s*)?'
    r'if\s*\(\s*(?P<u>\w+)\s*==\s*0\s*\)\s*break\s*;\s*'
    r'(?P=u)\s*=\s*(?P=u)\s*(?:-\s*1|\+\s*-1)\s*;\s*'
    r'(?:(?P<w2>\w+)\s*=\s*(?P<p2>\w+)\s*\+\s*1\s*;\s*)?'
    r'(?P<c>\w+)\s*=\s*\*(?P<pstar>\w+)\s*;\s*'
    r'(?P<pset>\w+)\s*=\s*(?P<next>[^;]+);\s*'
    r'\}\s*while\s*\(\s*(?P=c)\s*!=\s*(?:\'\\0\'|0)\s*\)\s*;',
    re.S,
)

_EXTERN_S = re.compile(
    r'^extern (unsigned )?(int|char|short) (\*?)(s_\w+)\s*;\s*$', re.M)
_EXTERN_DAT = re.compile(
    r'^extern (unsigned )?(int|char|short) (\*?)((?:DAT_|_DAT_)[0-9a-fA-F]+)\s*;\s*$',
    re.M)
# Whole-object string address. `(char *)&DAT + i` is a struct/table walk, not a
# string (0x1006A330). `fopen(path, &DAT_mode)` is a string (0x10008DC0).
_CHAR_CAST_AMP = re.compile(
    r'\(\s*char\s*\*\s*\)\s*&\s*((?:s_|DAT_|_DAT_)[A-Za-z0-9_]+)\b(?!\s*[+\[])')
# DAT_ as a string address: strcpy/strcat/fopen/sprintf. sscanf's 3rd+ args
# are out-params (0x10031030 `&DAT_1186c960`) — those stay int.
_DAT_STRFN_RE = 'strcpy|strcat|strstr|strcmp|strlen|sprintf|fopen|printf|fprintf|wsprintfA'


def _src_expr_from_scan(scan_text, scan_ptr):
    """Prefer `p = EXPR;` immediately before the do, else the starred pointer."""
    inits = list(re.finditer(
        r'\b%s\s*=\s*([^;]+);' % re.escape(scan_ptr), scan_text))
    if inits:
        expr = inits[-1].group(1).strip()
        if expr != '0xffffffff' and ' + 1' not in expr and ' + -' not in expr:
            return expr
    return scan_ptr


def _find_string_copy_sites(body):
    """Yield dicts describing one exploded strcpy/strcat in body."""
    for cm in _COPY_LOOPS.finditer(body):
        u = cm.group('u')
        dst_var = cm.group('dst')
        pre = body[:cm.start()]
        tildes = list(re.finditer(
            r'\b%s\s*=\s*~\s*%s\s*;' % (re.escape(u), re.escape(u)), pre))
        if not tildes:
            continue
        tilde = tildes[-1]
        ffs = list(re.finditer(
            r'\b%s\s*=\s*0xffffffff\s*;' % re.escape(u), pre[:tilde.start()]))
        if not ffs:
            continue
        ff = ffs[-1]
        scan_region = pre[ff.end():tilde.start()]
        sm = None
        for cand in _SCAN_DO.finditer(scan_region):
            if cand.group('u') == u:
                sm = cand
        if sm is None:
            continue
        scan_ptr = sm.group('pstar')
        src_expr = _src_expr_from_scan(scan_region[:sm.start()], scan_ptr)
        mid = pre[tilde.end():]
        # Intervening statements between `U = ~U` and the copy (e.g.
        # `iVar2 = *(int *)(p + N);` on 0x10038DA0) must be kept — they are
        # not part of the inlined strcpy.
        dest_scans = list(_SCAN_DO.finditer(mid))
        kind = 'strcpy'
        dest_expr = dst_var
        keep = []
        rewind_re = re.compile(
            r'\+\s*-%s\b|-\s*%s\b' % (re.escape(u), re.escape(u)))
        dest_m1_re = re.compile(r'\+\s*-1\b')
        if dest_scans:
            kind = 'strcat'
            dsm = dest_scans[0]
            dest_ptr = dsm.group('pstar')
            dest_expr = _src_expr_from_scan(mid[:dsm.start()], dest_ptr)
            # keep statements before the dest-scan that aren't `I = -1`
            # (that's the dest-scan counter init).
            pre_ds = mid[:dsm.start()]
            for am in re.finditer(r'(\w+)\s*=\s*([^;]+);', pre_ds):
                lhs, rhs = am.group(1), am.group(2).strip()
                if rhs in ('-1', '0xffffffff'):
                    continue
                keep.append(am.group(0))
        else:
            for am in re.finditer(r'(\w+)\s*=\s*([^;]+);', mid):
                lhs, rhs = am.group(1), am.group(2).strip()
                if rewind_re.search(rhs):
                    continue
                if lhs == dst_var:
                    dest_expr = rhs
                    continue
                keep.append(am.group(0))
            if dest_m1_re.search(dest_expr or ''):
                kind = 'strcat'
                # `dst = walker + -1` — dest base was assigned earlier in mid
                # (or is the starred pointer of a dest scan we missed).
                bases = list(re.finditer(
                    r'(\w+)\s*=\s*([^;]+);', mid))
                for am in bases:
                    if am.group(1) != dst_var and not rewind_re.search(
                            am.group(2)):
                        dest_expr = am.group(2).strip()
        yield {
            'start': ff.start(),
            'end': cm.end(),
            'kind': kind,
            'src': src_expr,
            'dst': dest_expr,
            'keep': keep,
            'u': u,
        }


def _retype_extern_array(head, body, name):
    """`extern int NAME;` / `extern unsigned char NAME;` → `extern char NAME[]`.

    Address-push (`push offset NAME`) needs the array form; a scalar int
    loads the first dword (`mov r,[NAME]; push r`). Proven 0x10008E60.
    """
    pat = re.compile(
        r'^extern (?:unsigned )?(?:int|char|short) \*?%s\s*;\s*$'
        % re.escape(name), re.M)
    m = pat.search(head)
    if not m:
        return head, body, False
    nh = head[:m.start()] + 'extern char %s[];' % name + head[m.end():]
    nb = _CHAR_CAST_AMP.sub(
        lambda mm: mm.group(1) if mm.group(1) == name else mm.group(0), body)
    # `(char *)&NAME` already handled; bare `&NAME` as a whole-object address
    # of the new array is just `NAME`.
    nb = re.sub(r'&\s*%s\b(?!\s*[\[+])' % re.escape(name), name, nb)
    return nh, nb, True


def _dat_string_names(head, body):
    """DAT_ symbols used as a string *address*, not as an int/table value.

    `(char *)&DAT` is the Ghidra tell. A CRT string call with a bare DAT
    (not `&DAT + N`, which is a table slot) is the same. Skip out-params
    (`sscanf(..., &DAT_106eef08 + i)`) and handle globals.
    """
    names = []
    for m in _EXTERN_DAT.finditer(head):
        name = m.group(4)
        if m.group(3) == '*' and m.group(2) == 'char':
            continue
        if name in _CHAR_CAST_AMP.findall(body):
            names.append(name)
            continue
        # table slot: `&DAT + N` / `&DAT[N]` — not a string
        if re.search(r'&\s*%s\s*[+\[]' % re.escape(name), body):
            # still a string if a CRT call takes `&DAT` as a *whole* arg
            # (`fopen(path, &DAT_mode)`), not `&DAT +`.
            if re.search(
                    r'\b(?:%s)\s*\([^;()]*&\s*%s\s*[,)]'
                    % (_DAT_STRFN_RE, re.escape(name)), body):
                names.append(name)
            continue
        if re.search(
                r'\b(?:%s)\s*\([^;()]*\b%s\s*[,)]'
                % (_DAT_STRFN_RE, re.escape(name)), body):
            names.append(name)
    return names


def gen_strarr(src):
    """Ghidra exploded `repne scasb`/`rep movsd` → strcpy/strcat, plus
    `extern int s_*` → `extern char s_*[]` (address push, not value).

    Distinguisher: orig `or ecx,-1; f2 ae; not ecx; shr ecx,2; f3 a5`.
    Ghidra prints a 0xffffffff scan + dword/byte copy; `_strcpy_sub` at wrap
    time misses walker-rewind and strcat (dest scanned first). Proven MATCH
    0x1006FF50 / 0x10055A40. Prey: residue functions with `s_*` or orig scasb.

    Yields one edit at a time (`strarr:NAME`, `strcpy:N`, `strcat:N`) so the
    refine climb can combine with other generators. Fold recipe: one combined
    candidate — see transform_strarr.
    """
    head, body = _split(src)
    for m in _EXTERN_S.finditer(head):
        name = m.group(4)
        # already an array
        if re.search(r'^extern char %s\[\];' % re.escape(name), head, re.M):
            continue
        nh, nb, ok = _retype_extern_array(head, body, name)
        if ok:
            yield ('strarr:%s' % name[:28], _join(nh, nb))
    for name in _dat_string_names(head, body):
        if re.search(r'^extern char %s\[\];' % re.escape(name), head, re.M):
            continue
        nh, nb, ok = _retype_extern_array(head, body, name)
        if ok:
            yield ('strarr:%s' % name[:28], _join(nh, nb))
    sites = list(_find_string_copy_sites(body))
    for i, site in enumerate(sites):
        keep = ''.join('%s ' % s for s in site.get('keep') or [])
        repl = '%s%s(%s, %s);' % (keep, site['kind'], site['dst'], site['src'])
        nb = body[:site['start']] + repl + body[site['end']:]
        yield ('%s:%d' % (site['kind'], i), _join(head, nb))


def transform_strarr(src):
    """Apply every strarr / strcpy / strcat edit. Returns (new_src, labels)."""
    labels = []
    changed = True
    while changed:
        changed = False
        for label, cand in gen_strarr(src):
            if cand != src:
                src = cand
                labels.append(label)
                changed = True
                break
    return src, labels


# ---------------------------------------------------------------------------
# 2. cmp p,end; jl header  vs  VC5 peel to jge; cmp; jne
# ---------------------------------------------------------------------------

# Table-scan while: Ghidra `while (*p != key) { p += stride; i++; if (p >= end)
# return 0; }`. Orig is `cmp [p],key; je found; add p,imm; inc i; cmp p,end; jl
# header` (0x10039580). VC5 peels every while/do/for spelling to `jge; cmp;
# jne`; the goto form is the remaining C-level lever.
# Bodies are brace-matched (a regex `.*?` stops at the `return 0;` inner `}`).
_WHILE_HEAD = re.compile(
    r'(?P<i>\w+)\s*=\s*0\s*;\s*'
    r'(?P<p>\w+)\s*=\s*(?P<start>[^;]+);\s*'
    r'while\s*\(\s*\*\s*(?P=p)\s*!=\s*(?P<key>[^)]+)\)\s*\{',
)
_DO_HEAD = re.compile(
    r'(?P<i>\w+)\s*=\s*0\s*;\s*'
    r'(?P<p>\w+)\s*=\s*(?P<start>[^;]+);\s*'
    r'do\s*\{',
)

_END_GE = re.compile(
    r'\(\s*int\s*\)\s*(?P<p>\w+)\s*>=\s*(?P<end>0x[0-9a-fA-F]+|\d+)')
_END_GT = re.compile(
    r'\(\s*int\s*\)\s*(?P<p>\w+)\s*>\s*(?P<end>0x[0-9a-fA-F]+|\d+)')
_END_LT_FLIP = re.compile(
    r'(?P<end>0x[0-9a-fA-F]+|\d+)\s*<\s*\(\s*int\s*\)\s*(?P<p>\w+)')
_END_LE_FLIP = re.compile(
    r'(?P<end>0x[0-9a-fA-F]+|\d+)\s*<=\s*\(\s*int\s*\)\s*(?P<p>\w+)')
_END_WHILE_LT = re.compile(
    r'\(\s*int\s*\)\s*(?P<p>\w+)\s*<\s*(?P<end>0x[0-9a-fA-F]+|\d+)')
_DAT_ADDR = re.compile(r'&?\s*(?:DAT_|_DAT_)([0-9a-fA-F]{8})\b')
_INT_CAST_PTR = re.compile(
    r'(?P<p>\w+)\s*=\s*\(\s*int\s*\*\s*\)\s*(0x[0-9a-fA-F]+)\s*;')


def _parse_end(cond, p):
    """Return exclusive end immediate from a Ghidra end-test, or None."""
    cond = cond.strip()
    m = _END_GE.search(cond)
    if m and m.group('p') == p:
        return m.group('end'), '>='
    m = _END_GT.search(cond)
    if m and m.group('p') == p:
        v = int(m.group('end'), 0)
        return hex(v + 1), '>'
    m = _END_LT_FLIP.search(cond)
    if m and m.group('p') == p:
        v = int(m.group('end'), 0)
        return hex(v + 1), 'flip<'
    m = _END_LE_FLIP.search(cond)
    if m and m.group('p') == p:
        return m.group('end'), 'flip<='
    m = _END_WHILE_LT.search(cond)
    if m and m.group('p') == p:
        return m.group('end'), '<'
    return None, None


def _end_from_while_body(body, p):
    """`if ((int)p >= END) return 0;` / Ghidra `if (END-1 < (int)p)`."""
    m = re.search(
        r'if\s*\(\s*(?P<cond>\([^)]*\)[^)]*|[^(){;]+)\s*\)\s*\{\s*return\s+0\s*;\s*\}',
        body, re.S)
    if not m:
        m = re.search(
            r'if\s*\(\s*(?P<cond>\([^)]*\)[^)]*|[^(){;]+)\s*\)\s*return\s+0\s*;',
            body, re.S)
    if not m:
        return None
    end, _ = _parse_end(m.group('cond'), p)
    return end


def _iter_while_scans(body):
    for m in _WHILE_HEAD.finditer(body):
        inner, end_i = _brace_block(body, m.end() - 1)
        if inner is None:
            continue
        after = body[end_i:]
        am = re.match(r'\s*(return\s+%s\s*;|break\s*;)' % re.escape(m.group('i')),
                      after)
        span_end = end_i + (am.end() if am else 0)
        yield {
            'start': m.start(), 'end': span_end, 'i': m.group('i'),
            'p': m.group('p'), 'init': m.group('start'), 'key': m.group('key'),
            'body': inner, 'kind': 'while',
        }


def _iter_do_scans(body):
    for m in _DO_HEAD.finditer(body):
        inner, end_i = _brace_block(body, m.end() - 1)
        if inner is None:
            continue
        wm = re.match(
            r'\s*while\s*\(\s*(?P<cond>[^;]+)\s*\)\s*;', body[end_i:])
        if not wm:
            continue
        if not re.search(r'\*\s*%s\s*==' % re.escape(m.group('p')), inner):
            continue
        yield {
            'start': m.start(),
            'end': end_i + wm.end(),
            'i': m.group('i'), 'p': m.group('p'), 'init': m.group('start'),
            'body': inner, 'cond': wm.group('cond'), 'kind': 'do',
        }


def _goto_scan(i, p, start, key, stride, end, lab):
    """Orig-shaped scan: cmp [p],key at the header, signed jl back-edge."""
    return (
        '%s = 0;\n'
        '    %s = %s;\n'
        '    %s:\n'
        '    if (*%s == %s) return %s;\n'
        '    %s = %s + %s;\n'
        '    %s = %s + 1;\n'
        '    if ((int)%s < %s) goto %s;\n'
        '    return 0;'
        % (i, p, start.strip(), lab, p, key.strip(), i, p, p, stride,
           i, i, p, end, lab)
    )


def _stride_from_body(body, p):
    m = re.search(
        r'\b%s\s*=\s*%s\s*\+\s*([0-9a-fxA-FX]+)\s*;' % (
            re.escape(p), re.escape(p)),
        body)
    return m.group(1) if m else None


def gen_loop_peel(src):
    """Table-scan / pointer-walk whose orig is `cmp p,end; jl header`.

    Ghidra prints `while (*p != key) { p += n; i++; if (p >= end) return 0; }`
    or `do { if (*p == key) return i; p += n; i++; } while (p < end)`. VC5
    peels both (and for/goto of the same shape) to `jge default; cmp [p],key;
    jne`. The remaining C-level spelling is an explicit header label with a
    signed `<` back-edge. Integer table bases (`p = (int *)0xADDR`) give
    `mov ecx,imm` instead of a load. Proven family 0x10039580.

    Yields `loopgoto:N`, `loopdowhile:N`, `loopimm:DAT`.
    """
    head, body = _split(src)
    n = 0
    for scan in _iter_while_scans(body):
        p, i = scan['p'], scan['i']
        stride = _stride_from_body(scan['body'], p)
        end = _end_from_while_body(scan['body'], p)
        if not stride or not end:
            continue
        lab = 'LAB_scan_%d' % scan['start']
        repl = _goto_scan(i, p, scan['init'], scan['key'], stride, end, lab)
        nb = body[:scan['start']] + repl + body[scan['end']:]
        yield ('loopgoto:%d' % n, _join(head, nb))
        dw = (
            '%s = 0;\n'
            '    %s = %s;\n'
            '    do {\n'
            '      if (*%s == %s) return %s;\n'
            '      %s = %s + %s;\n'
            '      %s = %s + 1;\n'
            '    } while ((int)%s < %s);\n'
            '    return 0;'
            % (i, p, scan['init'].strip(), p, scan['key'].strip(), i,
               p, p, stride, i, i, p, end)
        )
        nb = body[:scan['start']] + dw + body[scan['end']:]
        yield ('loopdowhile:%d' % n, _join(head, nb))
        # Integer table base only on the scan's start expression.
        dm = _DAT_ADDR.search(scan['init'])
        if dm:
            addr = '0x' + dm.group(1)
            new_init = '((int *)%s)' % addr
            span = body[scan['start']:scan['end']]
            span2 = span.replace(scan['init'].strip(), new_init, 1)
            if span2 != span:
                nb = body[:scan['start']] + span2 + body[scan['end']:]
                yield ('loopimm:%s' % addr, _join(head, nb))
        n += 1
    for scan in _iter_do_scans(body):
        p, i = scan['p'], scan['i']
        stride = _stride_from_body(scan['body'], p)
        end, _ = _parse_end(scan['cond'], p)
        if not stride or not end:
            continue
        lab = 'LAB_scan_%d' % scan['start']
        inner = scan['body'].rstrip()
        repl = (
            '%s = 0;\n'
            '    %s = %s;\n'
            '    %s:\n'
            '%s\n'
            '    if ((int)%s < %s) goto %s;'
            % (i, p, scan['init'].strip(), lab, inner, p, end, lab)
        )
        nb = body[:scan['start']] + repl + body[scan['end']:]
        yield ('loopgoto:%d' % n, _join(head, nb))
        n += 1


def transform_loop_peel(src):
    """Apply every loop-peel edit that still fires. Prefer goto, then imm."""
    labels = []
    # integer bases first (shape-independent)
    seen_imm = set()
    for label, cand in gen_loop_peel(src):
        if label.startswith('loopimm:') and cand != src:
            if label in seen_imm:
                continue
            src = cand
            labels.append(label)
            seen_imm.add(label)
    # Then one goto per remaining while/do arm. Label numbers restart after
    # each edit (`loopgoto:0` is "the first remaining scan"), so do not key
    # the loop on the label — stop when no goto candidate still changes src.
    changed = True
    while changed:
        changed = False
        for label, cand in gen_loop_peel(src):
            if not label.startswith('loopgoto:'):
                continue
            if cand == src:
                continue
            src = cand
            labels.append(label)
            changed = True
            break
    return src, labels


# ---------------------------------------------------------------------------
# 3. Ghidra byte-pair of a 16-bit field → xor r,r; mov al; mov ah; mov word
# ---------------------------------------------------------------------------

# CONCAT11(p[lo], p[hi]) and the wrap-time leftover `/* CONCAT */(p[lo], p[hi])`
# plus the or-shift form already in some work files. Orig (0x10031960):
#   xor eax,eax; xor ecx,ecx;
#   mov al,[esi+15]; mov cl,[esi+17];
#   mov ah,[esi+14]; mov ch,[esi+16];
#   mov word [esi+14],ax; mov word [esi+16],cx
# That is BrRcaFixupRecord's 16-bit BE reconstruct, not a two-statement
# byte-pair swap (those are the surrounding dword pairs).
_CONCAT11 = re.compile(
    r'\*\(\s*(?:unsigned\s+)?short\s*\*\s*\)\s*\((?P<base>[^)]+)\)\s*=\s*'
    r'(?:CONCAT11|/\*\s*CONCAT\s*\*/)\s*\(\s*(?P<a>[^(),]+),\s*(?P<b>[^(),]+)\)\s*;',
)
_OR_SHIFT16 = re.compile(
    r'\*\(\s*(?:unsigned\s+)?short\s*\*\s*\)\s*\((?P<base>[^)]+)\)\s*=\s*'
    r'(?:\(\s*(?:unsigned\s+)?short\s*\)\s*)?'
    r'(?P<b>[^|;]+?)\s*\|\s*'
    r'\(\s*(?:\(\s*(?:unsigned\s+)?short\s*\)\s*)?(?P<a>[^|;]+?)\s*<<\s*8\s*\)\s*;',
    re.S,
)
# Adjacent byte-pair swap: t = p[n]; p[n] = p[n+1]; p[n+1] = t;
_BYTE_PAIR = re.compile(
    r'(?P<t>\w+)\s*=\s*(?P<p>\w+)\s*\[(?P<lo>[^\]]+)\]\s*;\s*'
    r'(?P=p)\s*\[(?P=lo)\]\s*=\s*(?P=p)\s*\[(?P<hi>[^\]]+)\]\s*;\s*'
    r'(?P=p)\s*\[(?P=hi)\]\s*=\s*(?P=t)\s*;',
    re.S,
)


def _xor_word_store(base, lo_byte, hi_byte, tmp):
    """C that VC5 lowers to xor-zero + mov al + mov ah + mov word.

    `unsigned short t = 0; t = (unsigned char)HI; t |= LO<<8; store`
    is the BrRcaFixupRecord form (VC5-IDIOMS.md, 0x10018B60 MATCH).
    """
    return (
        '{ unsigned short %s = 0; %s = (unsigned char)(%s); '
        '%s |= (unsigned short)((unsigned char)(%s) << 8); '
        '*(unsigned short *)(%s) = %s; }'
        % (tmp, tmp, hi_byte.strip(), tmp, lo_byte.strip(),
           base.strip(), tmp)
    )


def _index_plus_one(a, b):
    """True if b is a+1 (int or `x+1` vs `x`)."""
    def _imm(s):
        s = s.strip()
        try:
            return int(s, 0)
        except ValueError:
            return None
    ia, ib = _imm(a), _imm(b)
    if ia is not None and ib is not None:
        return ib == ia + 1 or ia == ib + 1
    if re.sub(r'\s+', '', b) == re.sub(r'\s+', '', a) + '+1':
        return True
    if re.sub(r'\s+', '', a) == re.sub(r'\s+', '', b) + '+1':
        return True
    return False


def gen_word_bswap(src):
    """Ghidra byte-pair / CONCAT11 of a 16-bit field → xor-zero word store.

    Distinguisher: orig `33 c0 8a 46 15 8a 66 14 66 89 46 14` (0x10031960).
    Ghidra prints `*(ushort *)(p+N) = CONCAT11(p[N], p[N+1])` or a two-byte
    swap of adjacent indices. Yields `wordbswap:N` and, when two stores sit
    next to each other, `wordbswap:pair:N` (orig interleaves the two xors).
    """
    head, body = _split(src)
    n = 0
    sites = []
    for rx in (_CONCAT11, _OR_SHIFT16):
        for m in rx.finditer(body):
            base, a, b = m.group('base'), m.group('a'), m.group('b')
            sites.append((m.start(), m.end(), base, a, b, m.group(0)))
    # Adjacent two-byte swap is the INNER pair of a dword bswap (swaprot
    # already covers those). Only CONCAT11 / or-shift of a 16-bit store is
    # this idiom — Ghidra's 0x10031960 spelling.
    sites.sort(key=lambda s: s[0])
    # drop overlapping (CONCAT and or-shift of the same store)
    filtered = []
    used = set()
    for s in sites:
        span = (s[0], s[1])
        if any(span[0] < u[1] and span[1] > u[0] for u in used):
            continue
        used.add(span)
        filtered.append(s)
    for i, (st, en, base, a, b, _raw) in enumerate(filtered):
        # CONCAT11(p[N], p[N+1]) → store (p[N+1] | p[N]<<8). This is the
        # BrRcaFixupRecord spelling that MATCHED 0x10018B60; the xor-zero
        # local form is a second candidate (orig 0x10031960 interleaves two).
        brrca = (
            '*(unsigned short *)(%s) = (unsigned short)((unsigned char)(%s) | '
            '((unsigned short)(unsigned char)(%s) << 8));'
            % (base.strip(), b.strip(), a.strip())
        )
        nb = body[:st] + brrca + body[en:]
        yield ('wordbswap:brrca:%d' % i, _join(head, nb))
        tmp = '_wbs_%d' % i
        repl = _xor_word_store(base, a, b, tmp)
        nb = body[:st] + repl + body[en:]
        yield ('wordbswap:%d' % i, _join(head, nb))
        n += 1
    # adjacent pair: two 16-bit stores with no statements between
    for i in range(len(filtered) - 1):
        a = filtered[i]
        b = filtered[i + 1]
        gap = body[a[1]:b[0]]
        if gap.strip():
            continue
        t0, t1 = '_wbs_p%d' % i, '_wbs_q%d' % i
        repl = (
            '{ unsigned short %s = 0, %s = 0; '
            '%s = (unsigned char)(%s); %s = (unsigned char)(%s); '
            '%s |= (unsigned short)((unsigned char)(%s) << 8); '
            '%s |= (unsigned short)((unsigned char)(%s) << 8); '
            '*(unsigned short *)(%s) = %s; '
            '*(unsigned short *)(%s) = %s; }'
            % (t0, t1,
               t0, a[4], t1, b[4],
               t0, a[3], t1, b[3],
               a[2], t0, b[2], t1)
        )
        nb = body[:a[0]] + repl + body[b[1]:]
        yield ('wordbswap:pair:%d' % i, _join(head, nb))


def transform_word_bswap(src):
    """CONCAT11 / leftover comma → BrRca or-shift. Do not rewrite an
    already-or-shift store (that is the proven spelling; xor-zero locals
    scored worse from-decomp on 0x10031960)."""
    labels = []
    n = 0
    changed = True
    while changed:
        changed = False
        head, body = _split(src)
        m = _CONCAT11.search(body)
        if not m:
            break
        brrca = (
            '*(unsigned short *)(%s) = (unsigned short)((unsigned char)(%s) | '
            '((unsigned short)(unsigned char)(%s) << 8));'
            % (m.group('base').strip(), m.group('b').strip(),
               m.group('a').strip())
        )
        src = _join(head, body[:m.start()] + brrca + body[m.end():])
        labels.append('wordbswap:brrca:%d' % n)
        n += 1
        changed = True
    return src, labels


# ---------------------------------------------------------------------------
# 4. empty `int f();` + a float arg → `int f(float)` (fstp dword, not qword)
# ---------------------------------------------------------------------------

_EMPTY_PROTO = re.compile(
    r'^(?P<ret>int|void|float|double) (?P<name>[A-Za-z_]\w*)\(\);\s*$', re.M)
_FLOAT_HINT = re.compile(
    r'\*\(\s*float\s*\*\)|\(\s*float\s*\)|\bfloat\b|\d+\.\d+f?\b|_DAT_[0-9a-fA-F]+'
    r'|\bfVar\d+\b|\bfStack_?\w*|\bfparam\w*')


def _call_args(body, name):
    """Yield (start, end, args_text) of identifier calls `name(`."""
    for m in re.finditer(r'\b%s\s*\(' % re.escape(name), body):
        k = m.end()
        depth = 1
        end = k
        while end < len(body) and depth:
            if body[end] == '(':
                depth += 1
            elif body[end] == ')':
                depth -= 1
            end += 1
        yield m.start(), end, body[k:end - 1]


def _split_args(args):
    parts, depth, cur = [], 0, []
    for ch in args:
        if ch == '(':
            depth += 1
            cur.append(ch)
        elif ch == ')':
            depth -= 1
            cur.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append(''.join(cur).strip())
    return [p for p in parts if p]


def _arg_is_float(arg, body):
    if _FLOAT_HINT.search(arg):
        return True
    ident = arg.strip()
    if re.match(r'^[A-Za-z_]\w*$', ident):
        if re.search(r'\bfloat\s+%s\b' % re.escape(ident), body):
            return True
    return False


def _float_sig(name, body):
    """Return a C param list (`float` / `int` mix) or None if no float arg."""
    typed = None
    for _s, _e, args in _call_args(body, name):
        parts = _split_args(args)
        if not parts:
            continue
        flags = [_arg_is_float(p, body) for p in parts]
        if not any(flags):
            continue
        sig = ['float' if f else 'int' for f in flags]
        if typed is None:
            typed = sig
        else:
            # widen to the longest call; float wins a position
            if len(sig) > len(typed):
                typed = sig[:len(typed)] + sig[len(typed):]
            for i, t in enumerate(sig):
                if i < len(typed) and t == 'float':
                    typed[i] = 'float'
    return typed


def gen_float_proto(src):
    """Empty `int f();` called with a float arg → `int f(float)`.

    Unprototyped calls convert float to double (`fstp qword [esp]`,
    `sub esp,8`, enough to force `and esp,-8`). A real `float` parameter
    is `push ecx; fstp dword`. Proven 0x10019A70 / 0x10018990 thunk /
    0x1006EBC0. Yields `floatproto:NAME`.
    """
    head, body = _split(src)
    for m in _EMPTY_PROTO.finditer(head):
        name = m.group('name')
        if name in _SKIP_PROTO or name in _C_KEYWORDS:
            continue
        sig = _float_sig(name, body)
        if not sig:
            continue
        # FUN_10018990 is the 9-byte `fld dword [esp+4]; jmp __ftol` thunk:
        # one float, extra Ghidra args are leftover. Keep them typed so the
        # call still compiles, but the first slot is float.
        if name == 'FUN_10018990' and sig:
            sig[0] = 'float'
        decl = '%s %s(%s);' % (m.group('ret'), name, ', '.join(sig))
        nh = head[:m.start()] + decl + head[m.end():]
        yield ('floatproto:%s' % name[:28], _join(nh, body))


def transform_float_proto(src):
    labels = []
    changed = True
    while changed:
        changed = False
        for label, cand in gen_float_proto(src):
            if cand != src:
                src = cand
                labels.append(label)
                changed = True
                break
    return src, labels


GENERATORS = [
    ('strarr', gen_strarr, transform_strarr),
    ('looppeel', gen_loop_peel, transform_loop_peel),
    ('wordbswap', gen_word_bswap, transform_word_bswap),
    ('floatproto', gen_float_proto, transform_float_proto),
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


def _orig(va):
    p = os.path.join(ORIG_DIR, va + '.bin')
    return open(p, 'rb').read() if os.path.exists(p) else b''


def _jl_back(b):
    for i in range(len(b) - 1):
        if b[i] == 0x7C and b[i + 1] >= 0x80:
            return True
    return False


def _has_word_rebuild(b):
    i = 0
    while True:
        j = b.find(_XOR_EAX, i)
        if j < 0:
            return False
        w = b[j:j + 24]
        if (b'\x8a\x66' in w or b'\x8a\x26' in w or b'\x8a\x6e' in w
                or b'\x8a\x67' in w) and _MOV_WORD in b[j:j + 40]:
            return True
        i = j + 1


def prey_strarr(src, orig, va=None):
    head, body = _split(src)
    if _EXTERN_S.search(head):
        return True
    if list(_find_string_copy_sites(body)):
        return True
    if orig and _SCASB in orig:
        return True
    if _dat_string_names(head, body):
        return True
    return False


def prey_loop_peel(src, orig, va=None):
    _h, body = _split(src)
    if list(_iter_while_scans(body)) or list(_iter_do_scans(body)):
        return True
    if orig and b'\x83\xc1\x24' in orig and _jl_back(orig):
        return True
    return False


def prey_word_bswap(src, orig, va=None):
    _h, body = _split(src)
    if _CONCAT11.search(body) or _OR_SHIFT16.search(body):
        return True
    if orig and _has_word_rebuild(orig):
        return True
    return False


def prey_float_proto(src, orig, va=None):
    if any(True for _ in gen_float_proto(src)):
        return True
    if orig and any(p in orig for p in _FSTP_DWORD):
        # only if the wrap still has an empty proto that could be the callee
        head, body = _split(src)
        return bool(_EMPTY_PROTO.search(head))
    return False


PREY_FN = {
    'strarr': prey_strarr,
    'looppeel': prey_loop_peel,
    'wordbswap': prey_word_bswap,
    'floatproto': prey_float_proto,
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
        out, labels = transform(new_src)
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
    tag = ('gsd' if from_decomp else 'gsw') + va_hex[-6:]
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
    import ghidra_to_match as g  # noqa: F401  (workers need the import path)
    src, fname = load_wrapped(va, from_decomp=from_decomp)
    orig = _orig(va)
    transform = dict((n, t) for n, _g, t in GENERATORS)[gen_name]
    new_src, labels = transform(src)
    if new_src == src:
        return {
            'va': va, 'before': None, 'after': None, 'noop': True,
            'labels': labels, 'yields': sum(1 for _ in
                dict((n, g) for n, g, _t in GENERATORS)[gen_name](src)),
        }
    tag = ('gsd' if from_decomp else 'gsw') + va[-6:] + gen_name[:2]
    b = score_src(src, fname, va, tag + 'b')
    a = score_src(new_src, fname, va, tag + 'a')
    return {
        'va': va,
        'before': b[0], 'after': a[0],
        'bopt': b[1], 'aopt': a[1],
        'noop': False,
        'labels': labels,
        'yields': sum(1 for _ in
            dict((n, g) for n, g, _t in GENERATORS)[gen_name](src)),
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
        # Canonical examples always included (even if already MATCH in work).
        extras = {
            'strarr': ['0x1006FF50', '0x10055A40', '0x100367C0', '0x100038A0',
                       '0x10038DA0', '0x1003FDA0', '0x100087D0'],
            'looppeel': ['0x10039580', '0x10036080'],
            'wordbswap': ['0x10031960'],
            'floatproto': ['0x1006EBC0'],
        }.get(gen_name, [])
        for e in extras:
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
        n_match = sum(1 for r in rows if r.get('match') and r.get('moved'))
        n_match_abs = sum(1 for r in rows if r.get('match'))
        n_worse = sum(1 for r in rows if r.get('worse'))
        n_err = sum(1 for r in rows if r.get('error'))
        examples_move = [r['va'] for r in rows if r.get('moved')][:8]
        examples_match = [r['va'] for r in rows if r.get('match')][:8]
        summary.append({
            'gen': gen_name, 'prey': n_prey, 'fired': n_fire,
            'moved': n_move, 'matched': n_match_abs, 'worse': n_worse,
            'err': n_err, 'ex_move': examples_move, 'ex_match': examples_match,
            'rows': rows,
        })
        print('  prey=%d fired=%d moved=%d matched=%d worse=%d err=%d' % (
            n_prey, n_fire, n_move, n_match_abs, n_worse, n_err), flush=True)
        if examples_move:
            print('  moved VAs: %s' % ' '.join(examples_move), flush=True)
        if examples_match:
            print('  MATCH VAs: %s' % ' '.join(examples_match), flush=True)
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
        extras = {
            'strarr': ['0x1006FF50', '0x10055A40', '0x100367C0', '0x100038A0'],
            'looppeel': ['0x10039580'],
            'wordbswap': ['0x10031960'],
            'floatproto': ['0x1006EBC0'],
        }.get(gen_name, [])
        for e in extras:
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
            yields = list(gen(src))
            out, labels = transform(src)
            print('  %s  yields=%d  applied=%s  changed=%s' % (
                va, len(yields), labels or '-', out != src))
            for lab, cand in yields[:8]:
                print('      - %s  Δ%d chars' % (lab, abs(len(cand) - len(src))))


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
    args = ap.parse_args()

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
