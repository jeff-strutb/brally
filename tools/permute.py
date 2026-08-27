#!/usr/bin/env python3
"""Randomized C permuter for VC5 register-allocation walls.

Modeled on simonlindholm/decomp-permuter: mutate semantically-equivalent C,
compile with the original MSVC 5.0, score against original bytes, keep
improvements, anneal (accept-worse) and restart from the best.

Mutations target LIVENESS and TEMP COUNT — the levers that move VC5 coloring.
Decl reordering and pure renames are proven /O2-neutral and are not used.

    python3 tools/permute.py --va 0x1002F380 --iters 20000
    python3 tools/permute.py --batch --iters 2000
    python3 tools/permute.py --va 0x10027E10 --extract-only

Seeds: --src PATH, else build/ghidra_work/<VA>.c, else the tree .c named in
report.csv (extracted into a single-function wrapped TU). A byte-exact result
is written to build/ghidra_work/<VA>.permuted.c. Progress/resume live in
build/ghidra_work/<VA>.permute.json.

Scoring reuses match_sweep.compile_variant + match_sweep.score (same Wine/MSVC
path as the matching pipeline). Default opt is /O2; pass --opts to try more.
"""
from __future__ import print_function

import argparse
import csv
import hashlib
import json
import os
import random
import re
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import ghidra_to_match as gtm  # noqa: E402
import match_diff  # noqa: E402
import match_sweep  # noqa: E402

WORK_DIR = os.path.join(ROOT, 'build', 'ghidra_work')
PERM_TMP = os.path.join(WORK_DIR, '_permute')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
SHARED_CSV = os.path.join(ROOT, 'config', 'shared.csv')

WRAP_HEAD = '''/* permute seed — %s */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <mmsystem.h>
#ifndef true
#define true 1
#define false 0
#endif
typedef int (*funcptr)();

'''

WRAP_FOOT = '\n#endif /* BR_MATCHING_BUILD */\n'

IDENT = r'[A-Za-z_][A-Za-z0-9_]*'
NUM = r'(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?(?:[fF])?)'
CMP_FLIP = {'<': '>', '>': '<', '<=': '>=', '>=': '<='}
TYPES = ('unsigned char', 'unsigned short', 'unsigned int',
         'signed char', 'char', 'short', 'int', 'float', 'double',
         'int32_t', 'uint32_t', 'int16_t', 'uint16_t', 'uint8_t', 'int8_t')

# Default batch: transcribed, non-EH, 2–40 diffs (report.csv 2026-08-26).
BATCH = [
    # closest / biggest first
    '0x100186E0', '0x10027E10', '0x1000CB20', '0x100075C0', '0x1002F380',
    '0x1003CC00', '0x1003FA00', '0x10027290', '0x100297F0', '0x100316D0',
    '0x10007D50', '0x10034FC0', '0x100357E0', '0x10038100', '0x10019840',
    '0x10029CD0', '0x1002F282', '0x10039E10', '0x10027F00',
]


# ---------------------------------------------------------------------------
# C scanning
# ---------------------------------------------------------------------------

def _match_pair(s, i, open_ch, close_ch):
    """s[i] is open_ch. Return index of matching close_ch, or -1."""
    depth = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c in '"\'':
            q = c
            i += 1
            while i < n and s[i] != q:
                if s[i] == '\\':
                    i += 1
                i += 1
        elif c == '/' and i + 1 < n and s[i + 1] == '/':
            i = s.find('\n', i)
            if i < 0:
                return -1
        elif c == '/' and i + 1 < n and s[i + 1] == '*':
            j = s.find('*/', i + 2)
            if j < 0:
                return -1
            i = j + 1
        elif c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _skip_ws_comments(s, i):
    n = len(s)
    while i < n:
        if s[i] in ' \t\r\n':
            i += 1
        elif s[i:i + 2] == '//':
            j = s.find('\n', i)
            i = n if j < 0 else j + 1
        elif s[i:i + 2] == '/*':
            j = s.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            break
    return i


def _find_func_def(src, name=None):
    """Return (start, open_brace, close_brace, name) of the last function def."""
    pat = re.compile(
        r'(?:^|\n)(?:[\w \t\*]+?)(' + (re.escape(name) if name else IDENT)
        + r')\s*\(', re.M)
    last = None
    _kw = set('if for while switch return sizeof do'.split())
    for m in pat.finditer(src):
        if m.group(1) in _kw:
            continue
        paren = m.end() - 1
        close_p = _match_pair(src, paren, '(', ')')
        if close_p < 0:
            continue
        i = _skip_ws_comments(src, close_p + 1)
        if i < len(src) and src[i] == '{':
            close_b = _match_pair(src, i, '{', '}')
            if close_b > 0:
                last = (m.start() if src[m.start()] != '\n' else m.start() + 1,
                        i, close_b, m.group(1))
    return last


def _split_fn(src):
    loc = _find_func_def(src)
    if not loc:
        return None
    start, ob, cb, name = loc
    return {
        'name': name,
        'pre': src[:ob + 1],          # through opening '{'
        'body': src[ob + 1:cb],
        'post': src[cb:],             # from closing '}' (inclusive)
        'ob': ob, 'cb': cb, 'start': start,
    }


def _top_statements(body):
    """Split a compound body into top-level statements (strings)."""
    stmts = []
    i = 0
    n = len(body)
    while i < n:
        i = _skip_ws_comments(body, i)
        if i >= n:
            break
        start = i
        # preprocessor
        if body[i] == '#':
            j = body.find('\n', i)
            i = n if j < 0 else j + 1
            stmts.append(body[start:i])
            continue
        # keyword-controlled statements that own a following block
        kw = re.match(r'(if|for|while|switch|do)\b', body[i:])
        if kw:
            i = _consume_control(body, i)
            stmts.append(body[start:i])
            continue
        # bare block
        if body[i] == '{':
            j = _match_pair(body, i, '{', '}')
            i = n if j < 0 else j + 1
            stmts.append(body[start:i])
            continue
        # statement to semicolon, tracking parens
        depth = 0
        while i < n:
            c = body[i]
            if c in '"\'':
                q = c
                i += 1
                while i < n and body[i] != q:
                    if body[i] == '\\':
                        i += 1
                    i += 1
                i += 1
                continue
            if c == '/' and i + 1 < n and body[i + 1] == '/':
                j = body.find('\n', i)
                i = n if j < 0 else j
                continue
            if c == '/' and i + 1 < n and body[i + 1] == '*':
                j = body.find('*/', i + 2)
                i = n if j < 0 else j + 2
                continue
            if c in '([{':
                depth += 1
            elif c in ')]}':
                depth -= 1
            elif c == ';' and depth <= 0:
                i += 1
                break
            i += 1
        stmts.append(body[start:i])
    return [s for s in stmts if s.strip()]


def _consume_control(body, i):
    """i at 'if'/'for'/...  Return index past the whole statement (with else)."""
    n = len(body)
    m = re.match(r'(if|for|while|switch|do)\b', body[i:])
    kind = m.group(1)
    i += m.end()
    i = _skip_ws_comments(body, i)
    if kind == 'do':
        i = _consume_stmt(body, i)
        i = _skip_ws_comments(body, i)
        if body[i:i + 5] == 'while':
            i += 5
            i = _skip_ws_comments(body, i)
            if i < n and body[i] == '(':
                j = _match_pair(body, i, '(', ')')
                i = n if j < 0 else j + 1
            i = _skip_ws_comments(body, i)
            if i < n and body[i] == ';':
                i += 1
        return i
    if i < n and body[i] == '(':
        j = _match_pair(body, i, '(', ')')
        i = n if j < 0 else j + 1
    i = _consume_stmt(body, i)
    if kind == 'if':
        i2 = _skip_ws_comments(body, i)
        if body[i2:i2 + 4] == 'else':
            i = _consume_stmt(body, i2 + 4)
    return i


def _consume_stmt(body, i):
    i = _skip_ws_comments(body, i)
    n = len(body)
    if i >= n:
        return i
    if re.match(r'(if|for|while|switch|do)\b', body[i:]):
        return _consume_control(body, i)
    if body[i] == '{':
        j = _match_pair(body, i, '{', '}')
        return n if j < 0 else j + 1
    depth = 0
    while i < n:
        c = body[i]
        if c in '"\'':
            q = c
            i += 1
            while i < n and body[i] != q:
                if body[i] == '\\':
                    i += 1
                i += 1
            i += 1
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        elif c == ';' and depth <= 0:
            return i + 1
        i += 1
    return i


def _is_decl(stmt):
    s = stmt.strip()
    return bool(re.match(
        r'(?:(?:const|static|unsigned|signed|volatile)\s+)*'
        r'(?:int32_t|uint32_t|int16_t|uint16_t|uint8_t|int8_t|'
        r'float|double|int|char|short|long|void|unsigned|struct\s+\w+|bool)\b'
        r'[^;{]*=?\s*[^;{]*;?\s*$', s)) and '(' not in s.split('=')[0]


def _decl_names(stmt):
    s = stmt.strip().rstrip(';')
    # drop storage/type words, keep identifiers before '=' or ','
    s = re.sub(
        r'^(?:const|static|unsigned|signed|volatile|int32_t|uint32_t|'
        r'int16_t|uint16_t|uint8_t|int8_t|float|double|int|char|short|'
        r'long|void|struct\s+\w+|bool)\s+', '', s)
    s = s.split('=')[0]
    return re.findall(IDENT, s)


def _idents(s):
    return set(re.findall(r'\b' + IDENT + r'\b', s))


def _lhs_names(stmt):
    names = set()
    for m in re.finditer(r'\b(' + IDENT + r')\s*(?:\+\+|--|[\+\-\*/&|^]?=)', stmt):
        names.add(m.group(1))
    for m in re.finditer(r'(?:\+\+|--)(\s*)\b(' + IDENT + r')\b', stmt):
        names.add(m.group(2))
    return names


def _rebuild(parts, body):
    return parts['pre'] + body + parts['post']


# ---------------------------------------------------------------------------
# Extract / wrap
# ---------------------------------------------------------------------------

def _norm_va(va):
    va = va.lower()
    if not va.startswith('0x'):
        va = '0x' + va
    return '0x%08x' % int(va, 16)


def _report_row(va):
    va_l = _norm_va(va)
    if not os.path.exists(REPORT):
        return None
    with open(REPORT) as f:
        for r in csv.DictReader(f):
            try:
                if _norm_va(r.get('va') or '0') == va_l:
                    return r
            except ValueError:
                continue
    return None


def _shared_d3d_of(glide_va):
    """Inverse of shared.csv: glide_va -> d3d_va (int or None)."""
    g = int(glide_va, 16)
    if not os.path.exists(SHARED_CSV):
        return None
    with open(SHARED_CSV) as f:
        for r in csv.DictReader(f):
            try:
                if int(r['glide_va'], 16) == g:
                    return int(r['d3d_va'], 16)
            except (KeyError, ValueError):
                continue
    return None


def _all_func_defs(src, name):
    """Every definition of `name` as (start, open_brace, close_brace)."""
    pat = re.compile(
        r'(?:^|\n)(?:[\w \t\*]+?)(' + re.escape(name) + r')\s*\(', re.M)
    out = []
    for m in pat.finditer(src):
        paren = m.end() - 1
        close_p = _match_pair(src, paren, '(', ')')
        if close_p < 0:
            continue
        i = _skip_ws_comments(src, close_p + 1)
        if i < len(src) and src[i] == '{':
            close_b = _match_pair(src, i, '{', '}')
            if close_b > 0:
                st = m.start() if src[m.start()] != '\n' else m.start() + 1
                out.append((st, i, close_b))
    return out


def _prefer_matching_build(src, cands):
    """Prefer a def whose preceding text is inside #ifdef BR_MATCHING_BUILD."""
    if not cands:
        return None
    preferred = []
    for st, ob, cb in cands:
        pre = src[max(0, st - 800):st]
        last_if = pre.rfind('#ifdef BR_MATCHING_BUILD')
        last_else = pre.rfind('#else')
        last_endif = pre.rfind('#endif')
        if last_if > last_else and last_if > last_endif:
            preferred.append((st, ob, cb))
    return (preferred or cands)[-1]


def extract_function_text(src_text, name):
    cands = _all_func_defs(src_text, name)
    loc = _prefer_matching_build(src_text, cands)
    if not loc:
        return None
    start, ob, cb = loc
    # include return type: back up to previous newline that's not mid-decl
    line = src_text.rfind('\n', 0, start)
    # skip back over storage-class / return-type lines without ';'
    while line > 0:
        prev = src_text.rfind('\n', 0, line)
        chunk = src_text[prev + 1:line].strip()
        if (chunk.startswith('/*') or chunk.startswith('*')
                or chunk.startswith('//') or not chunk):
            line = prev
            continue
        if chunk.endswith('\\'):
            line = prev
            continue
        # a type-only line: `void` / `int __stdcall` immediately above name
        if re.match(r'^(?:(?:const|static|unsigned|signed|__\w+|BR_\w+)\s+)*'
                    r'(?:\w+\s*\*?)+$', chunk):
            line = prev
            continue
        break
    return src_text[line + 1:cb + 1]


def _file_scope_decls(src_text):
    """Map name -> 'extern <type> <name><array>;' for file-scope data."""
    out = {}
    # strip functions so we don't pick locals
    stripped = src_text
    # crude: remove bodies
    i = 0
    chunks = []
    while True:
        loc = _find_func_def(stripped[i:])
        if not loc:
            chunks.append(stripped[i:])
            break
        start, ob, cb, _ = loc
        chunks.append(stripped[i:i + start])
        i = i + cb + 1
    head = '\n'.join(chunks)
    decl_re = re.compile(
        r'^(?:extern\s+)?'
        r'((?:(?:const|static|unsigned|signed|volatile)\s+)*)'
        r'([A-Za-z_][A-Za-z0-9_]*)'
        r'((?:\s*\*)*)\s*'
        r'([A-Za-z_][A-Za-z0-9_]*)'
        r'(\[[^\]]*\])?\s*(?:=[^;]*)?;', re.M)
    for m in decl_re.finditer(head):
        stor, typ, stars, name, arr = m.group(1), m.group(2), m.group(3) or '', m.group(4), m.group(5) or ''
        if name in ('if', 'for', 'return', 'sizeof'):
            continue
        typ = (stor + typ + stars).replace('static', '').strip()
        out[name] = 'extern %s %s%s;' % (typ or 'int', name, arr)
    return out


def wrap_tree_function(src_path, func_name, va):
    text = open(src_path).read()
    fn = extract_function_text(text, func_name)
    if not fn:
        raise SystemExit('could not extract %s from %s' % (func_name, src_path))
    used = set(re.findall(r'\b' + IDENT + r'\b', fn))
    # macros referenced by the body pull in their expansion idents too
    for m in re.finditer(r'^#define\s+(' + IDENT + r')\b([^\n]*)', text, re.M):
        if m.group(1) in used:
            used.update(re.findall(r'\b' + IDENT + r'\b', m.group(2)))
    decls = _file_scope_decls(text)
    # includes from the source (top of file)
    includes = []
    for line in text.splitlines():
        s = line.strip()
        if s.startswith('#include'):
            includes.append(s)
        elif s.startswith('#ifdef') or s.startswith('#define _CRTIMP'):
            continue
        elif s.startswith('#endif') or s.startswith('#ifndef'):
            continue
        elif s.startswith('/*') or s.startswith('*') or not s:
            continue
        else:
            # stop after the first real code unless still in include-guard land
            if includes and not s.startswith('#'):
                break
    extra = []
    for inc in includes:
        if inc not in extra:
            extra.append(inc)
    # macros used by the function; prefer the BR_MATCHING_BUILD spelling
    macros = {}
    macros_pref = {}
    for m in re.finditer(r'^#define\s+(' + IDENT + r')(\([^)]*\))?[^\n]*', text, re.M):
        if m.group(2):
            continue  # function-like macro; skip (needs continuation lines)
        if m.group(1) not in used:
            continue
        pre = text[max(0, m.start() - 400):m.start()]
        last_if = pre.rfind('#ifdef BR_MATCHING_BUILD')
        last_else = pre.rfind('#else')
        last_endif = pre.rfind('#endif')
        if last_if > last_else and last_if > last_endif:
            macros_pref[m.group(1)] = m.group(0)
        macros[m.group(1)] = m.group(0)
    for name, line in macros.items():
        extra.append(macros_pref.get(name, line))
    header = WRAP_HEAD % va
    for inc in extra:
        if 'windows.h' in inc or 'stdlib.h' in inc or 'stdio.h' in inc \
                or 'string.h' in inc or 'math.h' in inc or 'stdint.h' in inc \
                or 'mmsystem.h' in inc:
            continue
        header += inc + '\n'
    header += '\n'
    # file-scope data used by the function
    KW = set('if for while return sizeof void int char short long float double '
             'unsigned signed const static extern struct typedef sizeof else '
             'do switch case default break continue goto'.split())
    for name in sorted(used):
        if name in decls and name != func_name:
            header += decls[name] + '\n'
    # calls to other functions defined in this file
    called = set(re.findall(r'\b(' + IDENT + r')\s*\(', fn)) - {func_name}
    called -= set('if for while switch return sizeof do'.split())
    for m in re.finditer(
            r'^(extern(?:\s+[\w\*]+)+\s+(' + IDENT + r')\s*\([^;&{]*\)\s*;)',
            text, re.M):
        if m.group(2) in used and m.group(2) != func_name:
            if not re.search(r'\bextern\b[^;]*\b%s\s*\(' % re.escape(m.group(2)),
                             header):
                header += m.group(1).strip() + '\n'
    defined = set()
    for m in re.finditer(r'^[\w \t\*]+?(' + IDENT + r')\s*\([^;]*\)\s*\{',
                         text, re.M):
        if m.group(1) not in set('if for while switch return sizeof do'.split()):
            defined.add(m.group(1))
    for c in sorted(called & defined):
        if re.search(r'#define\s+%s\b' % re.escape(c), header):
            continue
        if (' %s();' % c) not in header and (' %s(' % c) not in header:
            header += 'int %s();\n' % c
    # typedef / struct tags used as types in the function (brace-aware)
    i = 0
    while True:
        m = re.search(r'^typedef\b', text[i:], re.M)
        if not m:
            break
        start = i + m.start()
        j, depth = start, 0
        while j < len(text):
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
            elif text[j] == ';' and depth <= 0:
                block = text[start:j + 1]
                # last identifier before ';' is the typedef name
                tail = re.search(r'\b(' + IDENT + r')\s*;\s*$', block.strip())
                if tail and tail.group(1) in used:
                    if block.strip() not in header:
                        header += block.strip() + '\n'
                i = j + 1
                break
            j += 1
        else:
            break
    for fun in sorted(set(re.findall(r'\b(FUN_[0-9a-fA-F]{8})\b', fn))):
        if fun == func_name:
            continue
        if fun not in header:
            header += gtm.callee_decl(fun)
    for dat in sorted(set(re.findall(r'\b(_?DAT_[0-9a-fA-F]{8})\b', fn))):
        if dat not in header:
            header += 'extern int %s;\n' % dat
    header += '\n'
    return header + fn + WRAP_FOOT


def load_seed(va, src_path=None):
    """Return (wrapped_src, func_name, origin_label)."""
    va = _norm_va(va)
    va_u = '0x%08X' % int(va, 16)
    if src_path:
        src = open(src_path).read()
        loc = _find_func_def(src)
        name = loc[3] if loc else 'unknown'
        if '#ifdef BR_MATCHING_BUILD' not in src:
            src = gtm.wrap_for_compile(src, va_u)
            loc = _find_func_def(src)
            name = loc[3] if loc else name
        return src, name, src_path
    row = _report_row(va)
    tree = None
    name = None
    if row and row.get('file') and os.path.exists(os.path.join(ROOT, row['file'])):
        tree = os.path.join(ROOT, row['file'])
        name = row.get('name')
    gw = os.path.join(WORK_DIR, va_u + '.c')
    refined = os.path.join(WORK_DIR, va_u + '.refined.c')
    # Prefer the tree matching-build body (that's what report.csv scored).
    if tree and name:
        try:
            return wrap_tree_function(tree, name, va_u), name, tree
        except SystemExit:
            pass
        # d3d-tagged implements: name is right, try anyway already failed
    if os.path.exists(refined):
        src = open(refined).read()
        loc = _find_func_def(src)
        return src, (loc[3] if loc else name or 'fn'), refined
    if os.path.exists(gw):
        src = open(gw).read()
        loc = _find_func_def(src)
        return src, (loc[3] if loc else name or 'fn'), gw
    raise SystemExit('no seed for %s (no --src, no ghidra_work, no tree row)' % va_u)


# ---------------------------------------------------------------------------
# Mutations
# ---------------------------------------------------------------------------

_tmp_i = 0


def _fresh(prefix='p'):
    global _tmp_i
    _tmp_i += 1
    return '%s%d' % (prefix, _tmp_i)


def _fail():
    raise _Skip()


class _Skip(Exception):
    pass


def mut_intro_temp(body, rng):
    """Cache a repeated subexpression in a named local (or a single mem deref)."""
    # *(T *)(...)  or  DAT_xxx  appearing more than once
    cands = []
    for m in re.finditer(
            r'\*\(\s*((?:unsigned\s+)?(?:int|short|char|float|double))\s*\*\s*\)\s*\(',
            body):
        open_p = m.end() - 1
        close = _match_pair(body, open_p, '(', ')')
        if close < 0:
            continue
        expr = body[m.start():close + 1]
        if len(expr) < 8 or len(expr) > 80:
            continue
        cands.append((expr, m.group(1)))
    for m in re.finditer(r'\b((?:DAT_|_DAT_)[0-9a-fA-F]{8})\b', body):
        cands.append((m.group(1), 'int'))
    # ident[idx] and ident->field / ident.field
    for m in re.finditer(r'\b(' + IDENT + r')\s*(?:->|\.)\s*(' + IDENT + r')', body):
        cands.append((m.group(0), 'int'))
    rng.shuffle(cands)
    for expr, typ in cands:
        n = body.count(expr)
        # a single mem-deref temp still changes the register web
        min_n = 1 if expr.startswith('*') else 2
        if n < min_n:
            continue
        # don't wrap an lhs-only occurrence as the sole use
        tmp = _fresh('t')
        decl = '  %s %s;\n' % (typ, tmp)
        assign = '  %s = %s;\n' % (tmp, expr)
        # insert after declarations
        stmts = _top_statements(body)
        di = 0
        while di < len(stmts) and _is_decl(stmts[di]):
            di += 1
        new_stmts = stmts[:di] + [decl, assign] + stmts[di:]
        new_body = ''.join(new_stmts)
        new_body = new_body.replace(expr, tmp)
        # restore the assignment rhs
        new_body = new_body.replace('%s = %s' % (tmp, tmp),
                                    '%s = %s' % (tmp, expr), 1)
        if new_body.count(tmp) < 3:
            continue
        return new_body, 'intro_temp:%s' % expr[:40]
    _fail()


def mut_inline_temp(body, rng):
    """Re-deref: substitute a temp back to its RHS."""
    assigns = list(re.finditer(
        r'\b(' + IDENT + r')\s*=\s*([^;]+);', body))
    rng.shuffle(assigns)
    for m in assigns:
        name, rhs = m.group(1), m.group(2).strip()
        if name.startswith('p') and len(name) <= 2:
            continue
        if re.search(r'\+\+|--', rhs):
            continue
        if not re.match(r'(?:DAT_|_DAT_|param_|local_|iVar|uVar|g_)', name):
            # still allow iVar/local
            if not re.match(r'(iVar|uVar|local_|lVar|pT|t\d)', name):
                if name not in ('sMask', 'tMask', 'w', 'h', 'id', 'd', 'cb'):
                    pass
        uses = len(re.findall(r'\b%s\b' % re.escape(name), body))
        # decl + assign + uses
        if uses < 3:
            continue
        # don't inline if name is assigned again
        rest = body[m.end():]
        if re.search(r'\b%s\s*=' % re.escape(name), rest):
            # only replace uses until next assignment
            nxt = re.search(r'\b%s\s*=' % re.escape(name), rest)
            region = rest[:nxt.start()]
            if region.count(name) < 1:
                continue
            new_region = re.sub(r'\b%s\b' % re.escape(name), '(%s)' % rhs, region)
            new_body = body[:m.end()] + new_region + rest[nxt.start():]
            return new_body, 'inline_temp:%s' % name
        new_body = body[:m.start()] + body[m.end():]
        new_body = re.sub(r'\b%s\b' % re.escape(name), '(%s)' % rhs, new_body)
        # drop a now-unused decl
        new_body = re.sub(
            r'^\s*(?:unsigned\s+)?(?:int|short|char|float|double|int32_t|'
            r'uint32_t)\s+%s\s*;\s*\n' % re.escape(name),
            '', new_body, count=1, flags=re.M)
        return new_body, 'inline_temp:%s' % name
    _fail()


def mut_split_local(body, rng):
    """Split a local at its second assignment into a fresh name."""
    names = []
    for stmt in _top_statements(body):
        if _is_decl(stmt):
            names.extend(_decl_names(stmt))
    rng.shuffle(names)
    for name in names:
        assigns = list(re.finditer(r'\b%s\s*=' % re.escape(name), body))
        if len(assigns) < 2:
            continue
        # split at a random assignment after the first
        a = rng.choice(assigns[1:])
        new = _fresh(name[:4] or 's')
        # replace name from this assignment onward
        head, tail = body[:a.start()], body[a.start():]
        tail = re.sub(r'\b%s\b' % re.escape(name), new, tail)
        # declare new next to old
        head2 = re.sub(
            r'(^\s*(?:unsigned\s+)?(?:int|short|char|float|double|int32_t|'
            r'uint32_t)\s+%s\s*;)' % re.escape(name),
            r'\1\n  int %s;' % new, head, count=1, flags=re.M)
        if head2 == head:
            head2 = head + '  int %s;\n' % new
        return head2 + tail, 'split_local:%s->%s' % (name, new)
    _fail()


def mut_merge_locals(body, rng):
    """Merge two same-type locals (b replaced by a)."""
    decls = []
    for stmt in _top_statements(body):
        if _is_decl(stmt):
            t = re.match(
                r'\s*((?:unsigned\s+)?(?:int|short|char|float|double|'
                r'int32_t|uint32_t))\s+(' + IDENT + r')\s*;', stmt)
            if t:
                decls.append((t.group(1), t.group(2)))
    same = {}
    for t, n in decls:
        same.setdefault(t, []).append(n)
    cands = [(t, ns) for t, ns in same.items() if len(ns) >= 2]
    if not cands:
        _fail()
    t, ns = rng.choice(cands)
    a, b = rng.sample(ns, 2)
    new = re.sub(r'\b%s\b' % re.escape(b), a, body)
    new = re.sub(
        r'^\s*%s\s+%s\s*;\s*\n' % (re.escape(t), re.escape(b)),
        '', new, count=1, flags=re.M)
    return new, 'merge_locals:%s+%s' % (a, b)


def mut_reassoc(body, rng):
    """Swap commutative operands or reassociate (a+b)+c."""
    ops = list(re.finditer(
        r'\b((?:' + IDENT + r'|' + NUM + r'|\([^()]{0,40}\)))\s*'
        r'([\+\*\|\&\^])\s*'
        r'((?:' + IDENT + r'|' + NUM + r'|\([^()]{0,40}\)))', body))
    comm = [m for m in ops if m.group(2) in '+*|&^' and m.group(1) != m.group(3)]
    if comm:
        m = rng.choice(comm)
        a, op, b = m.group(1), m.group(2), m.group(3)
        repl = '%s %s %s' % (b, op, a)
        return body[:m.start()] + repl + body[m.end():], 'reassoc:swap:%s' % op
    # (a+b)+c -> a+(b+c)
    m = re.search(
        r'\(\s*([^()]+?)\s*([\+\*])\s*([^()]+?)\s*\)\s*\2\s*(' + IDENT + r'|' + NUM + r')',
        body)
    if m:
        a, op, b, c = m.group(1), m.group(2), m.group(3), m.group(4)
        if rng.random() < 0.5:
            repl = '%s %s (%s %s %s)' % (a, op, b, op, c)
        else:
            repl = '(%s %s %s) %s %s' % (a, op, c, op, b)
        return body[:m.start()] + repl + body[m.end():], 'reassoc:assoc:%s' % op
    _fail()


def mut_reorder_stmts(body, rng):
    """Swap two adjacent independent statements."""
    stmts = _top_statements(body)
    idxs = [i for i, s in enumerate(stmts)
            if not _is_decl(s) and s.strip() not in ('{', '}')]
    rng.shuffle(idxs)
    for i in idxs:
        if i + 1 >= len(stmts):
            continue
        a, b = stmts[i], stmts[i + 1]
        if _is_decl(a) or _is_decl(b):
            continue
        if a.strip().startswith(('if', 'for', 'while', 'switch', 'do', 'return',
                                 'case', 'default', 'break', 'continue', '#')):
            continue
        if b.strip().startswith(('if', 'for', 'while', 'switch', 'do', 'return',
                                 'case', 'default', '#')):
            continue
        # independence: lhs of one not in the other
        la, lb = _lhs_names(a), _lhs_names(b)
        ia, ib = _idents(a), _idents(b)
        if la & ib or lb & ia:
            continue
        stmts[i], stmts[i + 1] = b, a
        return ''.join(stmts), 'reorder_stmts'
    _fail()


def mut_hoist_sink(body, rng):
    """Move an assignment one statement up or down if deps allow."""
    stmts = _top_statements(body)
    assigns = [i for i, s in enumerate(stmts)
               if re.match(r'\s*' + IDENT + r'\s*=', s) and not _is_decl(s)]
    if not assigns:
        _fail()
    i = rng.choice(assigns)
    direction = rng.choice((-1, 1))
    j = i + direction
    if j < 0 or j >= len(stmts) or _is_decl(stmts[j]):
        _fail()
    a, b = stmts[i], stmts[j]
    la = _lhs_names(a)
    if la & _idents(b):
        _fail()
    stmts[i], stmts[j] = stmts[j], stmts[i]
    return ''.join(stmts), 'hoist_sink:%+d' % direction


def mut_width(body, rng):
    """Change a local's width/signedness."""
    mlist = list(re.finditer(
        r'^(\s+)((?:unsigned\s+)?(?:int|short|char)|int32_t|uint32_t|'
        r'int16_t|uint16_t|uint8_t|int8_t) (\w+);', body, re.M))
    if not mlist:
        _fail()
    m = rng.choice(mlist)
    cur = m.group(2)
    options = [t for t in ('int', 'unsigned int', 'short', 'unsigned short',
                           'char', 'unsigned char', 'int32_t', 'uint32_t')
               if t != cur]
    nxt = rng.choice(options)
    nb = body[:m.start()] + '%s%s %s;' % (m.group(1), nxt, m.group(3)) + body[m.end():]
    return nb, 'width:%s:%s->%s' % (m.group(3), cur, nxt)


def mut_cmp_flip(body, rng):
    cands = list(re.finditer(
        r'\(\s*([^()<>=!&|]{1,40}?)\s*(<=|>=|<|>)\s*([^()<>=!&|]{1,40}?)\s*\)',
        body))
    if not cands:
        _fail()
    m = rng.choice(cands)
    a, op, b = m.group(1).strip(), m.group(2), m.group(3).strip()
    repl = '(%s %s %s)' % (b, CMP_FLIP[op], a)
    return body[:m.start()] + repl + body[m.end():], 'cmp_flip:%s' % op


def mut_loop_shape(body, rng):
    """for(;;) <-> while, do-while <-> while for simple cases."""
    # for (; cond; ) { body } -> while (cond) { body }
    m = re.search(
        r'\bfor\s*\(\s*;\s*([^;]+?)\s*;\s*\)\s*\{', body)
    if m and rng.random() < 0.5:
        nb = body[:m.start()] + 'while (%s) {' % m.group(1).strip() + body[m.end():]
        return nb, 'loop:for->while'
    m = re.search(r'\bwhile\s*\(([^)]+)\)\s*\{', body)
    if m:
        nb = body[:m.start()] + 'for (; %s; ) {' % m.group(1).strip() + body[m.end():]
        return nb, 'loop:while->for'
    # do { } while (cond) -> for(;cond;)
    m = re.search(r'\bdo\s*\{', body)
    if m:
        # find matching close + while
        j = _match_pair(body, m.end() - 1, '{', '}')
        if j > 0:
            rest = body[j + 1:]
            wm = re.match(r'\s*while\s*\(([^)]+)\)\s*;', rest)
            if wm:
                cond = wm.group(1)
                inner = body[m.end():j]
                nb = (body[:m.start()] + 'for (; %s; ) {\n' % cond + inner
                      + '}\n' + rest[wm.end():])
                return nb, 'loop:do->for'
    _fail()


def mut_block_scope(body, rng):
    """Wrap a run of statements in a nested block (changes /Od slots; /O2
    packing of block-scoped locals onto dead slots)."""
    stmts = _top_statements(body)
    di = 0
    while di < len(stmts) and _is_decl(stmts[di]):
        di += 1
    rest = stmts[di:]
    if len(rest) < 3:
        _fail()
    lo = rng.randint(0, len(rest) - 2)
    hi = rng.randint(lo + 1, min(lo + 6, len(rest) - 1))
    wrapped = stmts[:di] + rest[:lo] + ['{\n'] + rest[lo:hi + 1] + ['}\n'] + rest[hi + 1:]
    return ''.join(wrapped), 'block_scope:%d-%d' % (lo, hi)


def mut_if_invert(body, rng):
    """if (c) A else B  ->  if (!(c)) B else A."""
    m = re.search(r'\bif\s*\(', body)
    tries = 0
    pos = 0
    cands = []
    while True:
        m = re.search(r'\bif\s*\(', body[pos:])
        if not m:
            break
        p = pos + m.end() - 1
        q = _match_pair(body, p, '(', ')')
        if q < 0:
            break
        cond = body[p + 1:q]
        r = _skip_ws_comments(body, q + 1)
        then_end = _consume_stmt(body, r)
        s = _skip_ws_comments(body, then_end)
        if body[s:s + 4] == 'else':
            else_end = _consume_stmt(body, s + 4)
            cands.append((pos + m.start(),
                          p, q, r, then_end, s, else_end, cond))
        pos = q + 1
        tries += 1
        if tries > 40:
            break
    if not cands:
        _fail()
    start, p, q, r, then_end, s, else_end, cond = rng.choice(cands)
    then_b = body[r:then_end]
    else_b = body[s + 4:else_end]
    repl = 'if (!(%s)) %s else %s' % (cond, else_b, then_b)
    return body[:start] + repl + body[else_end:], 'if_invert'


def mut_copy_cond(body, rng):
    """Duplicate a used condition into a temp: t = cond; if (t)."""
    mlist = list(re.finditer(r'\bif\s*\(', body))
    if not mlist:
        _fail()
    m = rng.choice(mlist)
    p = m.end() - 1
    q = _match_pair(body, p, '(', ')')
    if q < 0:
        _fail()
    cond = body[p + 1:q].strip()
    if len(cond) < 3 or '=' in cond:
        _fail()
    tmp = _fresh('c')
    decl = '  int %s;\n  %s = %s;\n' % (tmp, tmp, cond)
    stmts = _top_statements(body)
    di = 0
    while di < len(stmts) and _is_decl(stmts[di]):
        di += 1
    new_body = ''.join(stmts[:di]) + decl + ''.join(stmts[di:])
    # replace this if's cond
    # re-find after insert
    new_body = new_body.replace('if (%s)' % cond, 'if (%s)' % tmp, 1)
    return new_body, 'copy_cond'


def mut_split_add(body, rng):
    """a = b + c  ->  t = b; a = t + c  (extra live value)."""
    mlist = list(re.finditer(
        r'\b(' + IDENT + r')\s*=\s*(' + IDENT + r')\s*([\+\-\*])\s*('
        + IDENT + r'|' + NUM + r')\s*;', body))
    if not mlist:
        _fail()
    m = rng.choice(mlist)
    t = _fresh('s')
    assign = '%s = %s; %s = %s %s %s;' % (
        t, m.group(2), m.group(1), t, m.group(3), m.group(4))
    body2 = body[:m.start()] + assign + body[m.end():]
    stmts = _top_statements(body2)
    di = 0
    while di < len(stmts) and _is_decl(stmts[di]):
        di += 1
    stmts.insert(di, '  int %s;\n' % t)
    return ''.join(stmts), 'split_add:%s' % m.group(1)


def mut_plus_eq(body, rng):
    """a = a + b  <->  a += b."""
    m = re.search(
        r'\b(' + IDENT + r')\s*=\s*\1\s*([\+\-\*\|\&\^])\s*([^;]+);', body)
    if m:
        repl = '%s %s= %s;' % (m.group(1), m.group(2), m.group(3).strip())
        return body[:m.start()] + repl + body[m.end():], 'plus_eq'
    m = re.search(
        r'\b(' + IDENT + r')\s*([\+\-\*\|\&\^])=\s*([^;]+);', body)
    if m:
        repl = '%s = %s %s %s;' % (m.group(1), m.group(1), m.group(2),
                                   m.group(3).strip())
        return body[:m.start()] + repl + body[m.end():], 'eq_plus'
    _fail()


def mut_volatile_local(body, rng):
    mlist = list(re.finditer(
        r'^(\s+)((?:unsigned\s+)?(?:int|short|char|float))\s+(\w+);', body, re.M))
    if not mlist:
        _fail()
    m = rng.choice(mlist)
    if 'volatile' in m.group(0):
        _fail()
    nb = body[:m.start()] + '%svolatile %s %s;' % (
        m.group(1), m.group(2), m.group(3)) + body[m.end():]
    return nb, 'volatile:%s' % m.group(3)


MUTATIONS = [
    (16, mut_intro_temp),
    (16, mut_inline_temp),
    (8, mut_split_local),
    (7, mut_merge_locals),
    (8, mut_reassoc),
    (8, mut_reorder_stmts),
    (7, mut_hoist_sink),
    (5, mut_width),
    (5, mut_cmp_flip),
    (3, mut_loop_shape),
    (4, mut_block_scope),
    (5, mut_if_invert),
    (5, mut_copy_cond),
    (8, mut_split_add),
    (6, mut_plus_eq),
    (4, mut_volatile_local),
]


def apply_mutations(src, rng, n=1):
    parts = _split_fn(src)
    if not parts:
        return src, []
    body = parts['body']
    labels = []
    weights = [w for w, _ in MUTATIONS]
    fns = [f for _, f in MUTATIONS]
    for _ in range(n):
        order = list(range(len(fns)))
        rng.shuffle(order)
        # biased pick
        f = rng.choices(fns, weights=weights, k=1)[0]
        try:
            body, lab = f(body, rng)
            labels.append(lab)
        except _Skip:
            # try a few others
            for f2 in rng.sample(fns, min(4, len(fns))):
                try:
                    body, lab = f2(body, rng)
                    labels.append(lab)
                    break
                except _Skip:
                    continue
    if not labels:
        return src, []
    return _rebuild(parts, body), labels


# ---------------------------------------------------------------------------
# Score
# ---------------------------------------------------------------------------

def score_src(src, func_name, orig_bytes, opts, tag):
    os.makedirs(PERM_TMP, exist_ok=True)
    path = os.path.join(PERM_TMP, tag + '.c')
    with open(path, 'w') as f:
        f.write(src)
    best = (None, '', None, None)
    last_err = []
    for opt in opts:
        obj, err = match_sweep.compile_variant(path, 'perm_' + tag, opt)
        if obj is None:
            last_err = err
            continue
        try:
            funcs = match_diff.parse_coff_obj(obj)
        except Exception:
            continue
        found = None
        for cand in funcs:
            if match_diff.undecorate(cand) == func_name:
                found = cand
                break
            if cand.lstrip('_@').split('@')[0] == func_name:
                found = cand
                break
        if found is None and len(funcs) == 1:
            found = list(funcs)[0]
        if found is None:
            continue
        rb, relocs = funcs[found]
        ok, nd, _ = match_sweep.score(orig_bytes, rb, relocs)
        if ok:
            return (0, opt, rb, relocs)
        if best[0] is None or nd < best[0]:
            best = (nd, opt, rb, relocs)
    return best


def _src_hash(src):
    return hashlib.sha1(src.encode('utf-8', 'replace')).hexdigest()


# ---------------------------------------------------------------------------
# Anneal
# ---------------------------------------------------------------------------

def anneal(va, src, func_name, orig_bytes, opts, iters, seed=None,
           restart_every=250, muts_per_step=(1, 3), t0=10.0, cool=0.997,
           max_seconds=0):
    rng = random.Random(seed)
    tag = 'p%08x' % int(va, 16)
    t_score = time.time()
    diffs, opt, rb, relocs = score_src(src, func_name, orig_bytes, opts, tag)
    dt = time.time() - t_score
    if diffs is None:
        print('SEED DID NOT COMPILE for %s (%s)' % (va, func_name), flush=True)
        return None
    best_src, best_diff, best_opt = src, diffs, opt
    cur_src, cur_diff = src, diffs
    seen = {_src_hash(src): diffs}
    history = []
    T = float(t0)
    print('seed %s %s  diffs=%d/%d  opt=%s  compile=%.2fs' % (
        va, func_name, diffs, len(orig_bytes), opt, dt), flush=True)
    t0_run = time.time()
    last_log = t0_run
    for i in range(1, iters + 1):
        if max_seconds and (time.time() - t0_run) >= max_seconds:
            print('  time cap %.0fs at iter %d' % (max_seconds, i), flush=True)
            break
        if i % restart_every == 0 and best_src != cur_src:
            cur_src, cur_diff = best_src, best_diff
            T = max(T, t0 * 0.4)
        nmut = rng.randint(muts_per_step[0], muts_per_step[1])
        cand, labs = apply_mutations(cur_src, rng, nmut)
        if not labs or cand == cur_src:
            T *= cool
            continue
        h = _src_hash(cand)
        if h in seen:
            nd = seen[h]
            opt_c = best_opt
        else:
            nd, opt_c, _, _ = score_src(cand, func_name, orig_bytes, opts, tag)
            if nd is None:
                seen[h] = 10 ** 9
                T *= cool
                continue
            seen[h] = nd
        accept = False
        if nd <= cur_diff:
            accept = True
        else:
            dlt = nd - cur_diff
            p = pow(2.718281828, -dlt / max(T, 0.15))
            accept = rng.random() < p
        if accept:
            cur_src, cur_diff = cand, nd
            if labs:
                history.append((i, nd, list(labs)))
        if nd < best_diff:
            best_diff, best_src, best_opt = nd, cand, opt_c
            print('  * iter %d  diffs %d/%d  via %s' % (
                i, nd, len(orig_bytes), ','.join(labs)), flush=True)
            _save_best(va, best_src, best_diff, func_name, history)
            if nd == 0:
                print('MATCH at iter %d  %s' % (i, va), flush=True)
                return {
                    'diffs': 0, 'src': best_src, 'opt': best_opt,
                    'iters': i, 'seq': history, 'func': func_name,
                }
        T *= cool
        if time.time() - last_log > 20:
            ncomp = sum(1 for v in seen.values() if v < 10 ** 9)
            print('  ... iter %d/%d  cur=%d best=%d  compiled=%d  T=%.2f  %.1fs' % (
                i, iters, cur_diff, best_diff, ncomp, T,
                time.time() - t0_run), flush=True)
            last_log = time.time()
    _save_best(va, best_src, best_diff, func_name, history)
    print('floor %s %s  diffs=%d/%d after %d iters (%d compiled)' % (
        va, func_name, best_diff, len(orig_bytes), i if iters else 0,
        sum(1 for v in seen.values() if v < 10 ** 9)), flush=True)
    return {
        'diffs': best_diff, 'src': best_src, 'opt': best_opt,
        'iters': i if iters else 0, 'seq': history, 'func': func_name,
    }


def _out_paths(va):
    va_u = '0x%08X' % int(va, 16)
    return (os.path.join(WORK_DIR, va_u + '.permuted.c'),
            os.path.join(WORK_DIR, va_u + '.permute.json'),
            os.path.join(WORK_DIR, va_u + '.permute.log'))


def _save_best(va, src, diffs, func_name, history):
    os.makedirs(WORK_DIR, exist_ok=True)
    permuted, state, _ = _out_paths(va)
    with open(permuted, 'w') as f:
        f.write(src)
    with open(state, 'w') as f:
        json.dump({
            'va': '0x%08X' % int(va, 16),
            'func': func_name,
            'best_diffs': diffs,
            'n_improves': len(history),
            'last_improves': history[-12:],
        }, f, indent=2)


def permute_one(va, args):
    va = _norm_va(va)
    src, func_name, origin = load_seed(va, args.src)
    orig_path = os.path.join(ORIG_DIR, ('0x%08X' % int(va, 16)) + '.bin')
    if not os.path.exists(orig_path):
        print('no orig bytes %s' % orig_path, flush=True)
        return None
    orig = match_sweep.load_orig(orig_path, va)
    opts = args.opts.split(',') if args.opts else ['/O2']
    print('seed from %s  (%d bytes wrapped, fn %s)' % (
        origin, len(src), func_name), flush=True)
    if args.extract_only:
        out = os.path.join(WORK_DIR, ('0x%08X' % int(va, 16)) + '.seed.c')
        os.makedirs(WORK_DIR, exist_ok=True)
        with open(out, 'w') as f:
            f.write(src)
        diffs, opt, rb, _ = score_src(src, func_name, orig, opts,
                                      'seed%08x' % int(va, 16))
        print('wrote %s  seed diffs=%s opt=%s recomp=%s' % (
            out, diffs, opt, len(rb) if rb else None), flush=True)
        if diffs is None:
            obj, err = match_sweep.compile_variant(out, 'perm_seederr', opts[0])
            print('  compile_error: %s' % (err or obj), flush=True)
        return {'diffs': diffs, 'src': src, 'func': func_name, 'iters': 0, 'seq': []}
    result = anneal(va, src, func_name, orig, opts, args.iters,
                    seed=args.seed, restart_every=args.restart,
                    t0=args.temp, cool=args.cool,
                    max_seconds=args.max_seconds)
    if result and result['diffs'] == 0:
        permuted, _, _ = _out_paths(va)
        print('byte-exact -> %s' % permuted, flush=True)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--va', help='function VA (Glide)')
    ap.add_argument('--src', help='wrapped seed .c (overrides discovery)')
    ap.add_argument('--iters', type=int, default=20000)
    ap.add_argument('--opts', default='/O2',
                    help='comma-separated cl flags, default /O2')
    ap.add_argument('--seed', type=int, default=None)
    ap.add_argument('--restart', type=int, default=250)
    ap.add_argument('--temp', type=float, default=10.0)
    ap.add_argument('--cool', type=float, default=0.997)
    ap.add_argument('--batch', action='store_true',
                    help='run the 19-function residue batch')
    ap.add_argument('--extract-only', action='store_true')
    ap.add_argument('--vas', help='comma-separated VA list (with --batch)')
    ap.add_argument('--max-seconds', type=int, default=0,
                    help='stop each function after this many seconds (0=off)')
    args = ap.parse_args()
    vas = []
    if args.batch:
        vas = [v.strip() for v in (args.vas.split(',') if args.vas else BATCH)]
    elif args.va:
        vas = [args.va]
    else:
        ap.error('pass --va or --batch')
    summary = []
    for va in vas:
        print('=' * 60, flush=True)
        r = permute_one(va, args)
        if r is None:
            summary.append((va, None, 0, []))
        else:
            summary.append((va, r['diffs'], r.get('iters', 0), r.get('seq', [])))
    print('\n======== permute summary ========')
    cracked = []
    for va, d, it, seq in summary:
        mark = 'MATCH' if d == 0 else ('FAIL' if d is None else ('%d diffs' % d))
        print('  %s  %s  (%d iters, %d improves)' % (
            va, mark, it, len(seq)))
        if d == 0:
            cracked.append((va, seq))
    if cracked:
        print('\ncracked mutation sequences:')
        for va, seq in cracked:
            labs = []
            for _i, _nd, ls in seq:
                labs.extend(ls)
            print('  %s: %s' % (va, ' -> '.join(labs[-12:])))
    return 0


if __name__ == '__main__':
    sys.exit(main())
