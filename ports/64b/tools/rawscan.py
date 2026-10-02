#!/usr/bin/env python3
"""Inventory every raw-offset memory access in the portable core.

Decompiler-style bodies address memory as `*(T *)(base + K)`,
`((T *)(base + K))[i]`, or pass `base + K` on as an address. Retyping turns
each into a named field of the struct `base` points to. This lists them from
clang's syntax tree, grouped by base, so every base needs one decision
(which struct) rather than every access.

For each access it records whether the loaded value is then used as an
address (an integer-to-pointer conversion of the load): that marks a
pointer field.

Usage: rawscan.py [--jobs N] [FILE...]      (default: every core file)
Output: build/portable/raw.csv, build/portable/raw_bases.csv
"""
import argparse
import concurrent.futures
import csv
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(ROOT, 'build', 'portable')
FLAGS = ['-fsyntax-only', '-D_FORTIFY_SOURCE=0', '-w', '-ferror-limit=0', '-fms-extensions', '-fdeclspec',
         '-Wno-return-mismatch', '-Wno-error=incompatible-pointer-types',
         '-Wno-error=incompatible-function-pointer-types', '-Iports/64b/platform/include', '-Iports/64b/include',
         '-include', 'ports/64b/platform/include/win32.h',
         '-include', 'ports/64b/platform/include/glide.h',
         '-include', 'ports/64b/platform/include/br_x87.h',
         '-include', 'ports/64b/include/br_crt.h',
         '-include', 'ports/64b/include/br_addr32.h',
         '-include', 'ports/64b/platform/include/br_lp64.h',
         '-include', 'ports/64b/include/br_globals.h',
         '-include', 'ports/64b/include/br_funcs.h']


def flags_for(f):
    """The build's flags for one core file, its alias header included."""
    rel = os.path.relpath(os.path.join(ROOT, f), ROOT)
    key = rel[len('ports/64b/src/core/'):].replace('/', '__') if rel.startswith('ports/64b/src/core/') else None
    al = os.path.join(ROOT, 'ports/64b/alias', (key or '') + '.h')
    return FLAGS + (['-include', al] if key and os.path.exists(al) else [])


def lang(f):
    return ['-x', 'c++', '-std=c++98'] if f.endswith('.cpp') else ['-x', 'c', '-std=gnu89']


def ast(f):
    p = subprocess.run(['clang'] + flags_for(f) + lang(f) + ['-Xclang', '-ast-dump=json',
                       '-Xclang', '-ast-dump-filter=', f], capture_output=True, cwd=ROOT)
    txt = p.stdout.decode('utf-8', 'replace')
    return json.loads(txt) if txt.strip().startswith('{') else None


def qt(n):
    return (n.get('type') or {}).get('qualType', '')


def strip_parens(n):
    while n.get('kind') in ('ParenExpr',) or (n.get('kind') == 'ImplicitCastExpr'
                                              and n.get('castKind') in ('LValueToRValue', 'NoOp')):
        n = n['inner'][0]
    return n


def const_of(n):
    n = strip_parens(n)
    while n.get('kind') == 'ImplicitCastExpr' and n.get('castKind') == 'IntegralCast':
        n = strip_parens(n['inner'][0])
    if n.get('kind') == 'IntegerLiteral':
        return int(n['value'])
    if n.get('kind') == 'UnaryOperator' and n.get('opcode') == '-':
        v = const_of(n['inner'][0])
        return -v if v is not None else None
    return None


def base_of(n, idx=None):
    """(base expression node, constant offset) for `base + K` / `base - K` /
    plain base.  Nested sums fold.  Terms `i * C` (C constant) are collected
    into idx as (index node, C) when idx is a list; otherwise a sum with a
    non-constant term is its own base."""
    n = strip_parens(n)
    if n.get('kind') == 'BinaryOperator' and n.get('opcode') in ('+', '-'):
        a, b = n['inner']
        kb = const_of(b)
        if kb is not None:
            base, k = base_of(a, idx)
            return base, k + (kb if n['opcode'] == '+' else -kb)
        ka = const_of(a)
        if ka is not None and n['opcode'] == '+':
            base, k = base_of(b, idx)
            return base, k + ka
        if idx is not None and n['opcode'] == '+':
            for x, y in ((b, a), (a, b)):
                t = strip_parens(x)
                while t.get('kind') == 'ImplicitCastExpr':
                    t = strip_parens(t['inner'][0])
                if t.get('kind') == 'BinaryOperator' and t.get('opcode') == '*':
                    c = const_of(t['inner'][1])
                    i = t['inner'][0]
                    if c is None:
                        c, i = const_of(t['inner'][0]), t['inner'][1]
                    if c is not None:
                        idx.append((i, c))
                        return base_of(y, idx)
    if n.get('kind') == 'ImplicitCastExpr' and n.get('castKind') in ('BitCast', 'ArrayToPointerDecay'):
        return base_of(n['inner'][0], idx)
    if n.get('kind') == 'CStyleCastExpr' and n.get('castKind') in ('BitCast', 'NoOp'):
        return base_of(n['inner'][0], idx)
    return n, 0


def describe(b):
    """A stable name for a base: the variable it reads, if any."""
    b = strip_parens(b)
    if b.get('kind') == 'DeclRefExpr':
        d = b.get('referencedDecl', {})
        return d.get('kind', ''), d.get('name', ''), d.get('id', '')
    if b.get('kind') == 'UnaryOperator' and b.get('opcode') == '&':
        s = strip_parens(b['inner'][0])
        if s.get('kind') == 'DeclRefExpr':
            d = s.get('referencedDecl', {})
            return 'AddrOf' + d.get('kind', ''), d.get('name', ''), d.get('id', '')
    return b.get('kind', ''), '', ''


class LineTrack:
    """clang's JSON AST writes a location's `line` only when it differs from
    the previous location written (spelling, expansion, range begin and end,
    in dump order). Replaying that order recovers every node's line."""

    def __init__(self):
        self.last = None

    def _bare(self, loc):
        if 'line' in loc:
            self.last = loc['line']
        return self.last

    def _loc(self, loc):
        if not loc:
            return None
        if 'spellingLoc' in loc or 'expansionLoc' in loc:
            self._bare(loc.get('spellingLoc', {}))
            return self._bare(loc.get('expansionLoc', {}))
        return self._bare(loc)

    def visit(self, n):
        """The node's (expansion) begin line, consuming its locations."""
        self._loc(n.get('loc'))
        rg = n.get('range') or {}
        b = self._loc(rg.get('begin'))
        self._loc(rg.get('end'))
        return b


def scan(f):
    a = ast(f)
    if a is None:
        return []
    rows = []
    fabs = os.path.join(ROOT, f)
    lt = LineTrack()

    def infile(n):
        loc = n.get('loc') or n.get('range', {}).get('begin', {})
        fl = loc.get('file') or loc.get('expansionLoc', {}).get('file')
        return fl is None or os.path.abspath(os.path.join(ROOT, fl)) == fabs

    def walk(n, fn, parents):
        k = n.get('kind')
        if k in ('FunctionDecl', 'CXXMethodDecl', 'CXXConstructorDecl', 'CXXDestructorDecl') and 'inner' in n:
            fn = n.get('name', '?')
        # an int-to-pointer (or pointer-to-pointer) cast whose operand is base+K
        # (NoOp: a same-type cast, `(uint8_t *)((uint8_t *)p + K)`)
        if k in ('CStyleCastExpr', 'ImplicitCastExpr') and n.get('castKind') in ('IntegralToPointer', 'BitCast', 'NoOp') \
                and qt(n).rstrip().endswith('*') and (k == 'CStyleCastExpr' or n.get('castKind') != 'NoOp'):
            op = n['inner'][0]
            base, off = base_of(op)
            bt = qt(base)
            BYTEP = ('char*', 'unsignedchar*', 'uint8_t*', 'constchar*', 'constunsignedchar*', 'BYTE*', 'void*',
                     'constvoid*', 'constuint8_t*', 'int8_t*', 'signedchar*')
            # the sum's own type: `(uint8_t *)p + K` on a typed p is byte arithmetic too
            ot = qt(strip_parens(op)).replace(' ', '')
            is_raw = n.get('castKind') == 'IntegralToPointer' or bt.replace(' ', '') in BYTEP or ot in BYTEP
            if is_raw and (off != 0 or n.get('castKind') == 'IntegralToPointer'):
                use = 'addr'
                par = parents[-1] if parents else {}
                if par.get('kind') == 'UnaryOperator' and par.get('opcode') == '*':
                    use = 'deref'
                elif par.get('kind') == 'ParenExpr' and len(parents) > 1 and parents[-2].get('kind') == 'ArraySubscriptExpr':
                    use = 'index'
                # is the loaded value then used as an address?
                ptrval = ''
                if use == 'deref' and len(parents) >= 3:
                    up = parents[-2]
                    if up.get('kind') == 'ImplicitCastExpr' and up.get('castKind') == 'LValueToRValue':
                        up2 = parents[-3]
                        if up2.get('kind') in ('CStyleCastExpr', 'ImplicitCastExpr') and \
                                up2.get('castKind') == 'IntegralToPointer':
                            ptrval = 'ptr'
                    if qt(parents[-1]).rstrip().endswith('*'):
                        ptrval = 'ptr'
                bk, bn, bid = describe(base)
                line = n.get('_line') or ''
                rows.append({'file': f, 'func': fn, 'line': line, 'base_kind': bk, 'base': bn,
                             'base_type': bt, 'off': off, 'type': qt(n), 'use': use, 'value': ptrval})
        for c in n.get('inner', []) or []:
            if isinstance(c, dict) and (parents or infile(c) or c.get('kind') not in (
                    'FunctionDecl', 'VarDecl', 'RecordDecl', 'TypedefDecl', 'CXXRecordDecl')):
                walk(c, fn, parents + [n])
    def lines(n):
        n['_line'] = lt.visit(n)
        for c in n.get('inner', []) or []:
            if isinstance(c, dict):
                lines(c)
    lines(a)
    for top in a.get('inner', []):
        if infile(top):
            walk(top, None, [])
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=os.cpu_count())
    ap.add_argument('files', nargs='*')
    a = ap.parse_args()
    os.chdir(ROOT)
    files = a.files or sorted(
        os.path.join(dp, fn)[len(ROOT) + 1:] if os.path.isabs(dp) else os.path.join(dp, fn)
        for dp, _, fns in os.walk('ports/64b/src/core') for fn in fns if fn.endswith(('.c', '.cpp')))
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        rows = [r for rs in ex.map(scan, files) for r in rs]
    os.makedirs(OUT, exist_ok=True)
    keys = ['file', 'func', 'line', 'base_kind', 'base', 'base_type', 'off', 'type', 'use', 'value']
    with open(os.path.join(OUT, 'raw.csv'), 'w', newline='') as fh:
        w = csv.DictWriter(fh, keys)
        w.writeheader()
        w.writerows(rows)
    bases = {}
    for r in rows:
        k = (r['file'], r['func'] if r['base_kind'] in ('ParmVarDecl', 'VarDecl') and r['base'] else '', r['base_kind'], r['base'])
        b = bases.setdefault(k, {'n': 0, 'offs': set(), 'ptr': set(), 'type': r['base_type']})
        b['n'] += 1
        b['offs'].add(r['off'])
        if r['value']:
            b['ptr'].add(r['off'])
    with open(os.path.join(OUT, 'raw_bases.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['file', 'func', 'base_kind', 'base', 'base_type', 'accesses', 'max_off', 'ptr_offs'])
        for k, b in sorted(bases.items(), key=lambda kv: -kv[1]['n']):
            w.writerow(list(k) + [b['type'], b['n'], hex(max(b['offs'])),
                                  ' '.join(hex(o) for o in sorted(b['ptr']))])
    kinds = {}
    for (f, fn, bk, bn) in bases:
        kinds[bk] = kinds.get(bk, 0) + 1
    print('raw accesses: %d in %d files; distinct bases: %d %s' % (
        len(rows), len({r['file'] for r in rows}), len(bases), kinds))


if __name__ == '__main__':
    main()
