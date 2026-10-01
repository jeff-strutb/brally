#!/usr/bin/env python3
"""Put the original byte arithmetic back where a retype turned it into
element arithmetic (the sites arithcheck.py lists).

For a variable that was `int` in the original and is `T *` in the port:

    v + E     ->  ((T *)((char *)(v) + (E)))
    v - E     ->  ((T *)((char *)(v) - (E)))
    v += E    ->  v = (T *)((char *)(v) + (E))
    v -= E    ->  v = (T *)((char *)(v) - (E))
    v++ / v-- ->  v = (T *)((char *)(v) + 1) / - 1

E stays in bytes, as the original computed. This is exact where T is a
scalar (the memory is the original's 32-bit layout: buffers, file images,
pointer-free records). Where T is a record with a spec the site is listed
instead -- its offset has to become a field (rewrite.py).

Usage: arithfix.py [--dry]      (reads build/portable/arithcheck.csv)
"""
import collections
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402
import rewrite  # noqa: E402
import stridefix  # noqa: E402

ROOT = rawscan.ROOT
SCALAR = re.compile(r'^(const\s+)?(unsigned\s+|signed\s+)?(int|short|long|float|double|char|u?int(8|16|32|64)_t|'
                    r'DWORD|WORD|BYTE|LONG|UINT|USHORT)$')


def strip(n):
    while n.get('kind') in ('ImplicitCastExpr', 'ParenExpr'):
        n = n['inner'][0]
    return n


def main():
    os.chdir(ROOT)
    rows = list(csv.DictReader(open('build/portable/arithcheck.csv')))
    by_file = collections.defaultdict(set)
    for r in rows:
        by_file[r['file']].add((r['func'], r['var']))
    total, listed = 0, []
    for f, targets in sorted(by_file.items()):
        a = rawscan.ast(f)
        if a is None:
            continue
        s = open(f, encoding='latin-1').read()
        fabs = os.path.abspath(f)
        decls = {}
        edits = []

        def text(n):
            rg = stridefix.tokrange(s, n)
            return (rg, s[rg[0]:rg[1]]) if rg else (None, None)

        def target(v):
            v = strip(v)
            if v.get('kind') != 'DeclRefExpr':
                return None
            d = decls.get(v.get('referencedDecl', {}).get('id'))
            if d and (d[0], d[1]) in targets:
                return d
            return None

        def walk(n, fn):
            k = n.get('kind')
            if k in ('FunctionDecl', 'CXXMethodDecl') and 'inner' in n:
                fn = n.get('name')
            if k in ('ParmVarDecl', 'VarDecl'):
                decls[n.get('id')] = (fn, n.get('name'), n.get('type', {}).get('qualType', ''))
            done = False
            if k in ('BinaryOperator', 'CompoundAssignOperator') and n.get('opcode') in ('+', '-', '+=', '-='):
                l, r = n['inner']
                op = n['opcode']
                for side, other in ((l, r), (r, l)):
                    d = target(side)
                    if not d or (op == '-' and side is r):
                        continue
                    t = d[2].strip()
                    pointee = re.sub(r'\b(const|volatile)\b', '', t[:-1]).strip()
                    rg, whole = text(n)
                    _, vt = text(strip(side))
                    _, et = text(other)
                    if not rg or vt is None or et is None:
                        break
                    if not SCALAR.match(pointee):
                        listed.append((f, s.count('\n', 0, rg[0]) + 1, d[1], t, whole))
                        break
                    sign = '-' if op in ('-', '-=') else '+'
                    if op in ('+=', '-='):
                        new = '%s = (%s)((char *)(%s) %s (%s))' % (vt, t, vt, sign, et)
                    else:
                        new = '((%s)((char *)(%s) %s (%s)))' % (t, vt, sign, et)
                    edits.append((rg[0], rg[1], new))
                    done = True
                    break
            if k == 'UnaryOperator' and n.get('opcode') in ('++', '--'):
                d = target(n['inner'][0])
                if d:
                    t = d[2].strip()
                    pointee = re.sub(r'\b(const|volatile)\b', '', t[:-1]).strip()
                    rg, whole = text(n)
                    _, vt = text(strip(n['inner'][0]))
                    if rg and vt and SCALAR.match(pointee):
                        edits.append((rg[0], rg[1], '(%s = (%s)((char *)(%s) %s 1))' % (
                            vt, t, vt, '+' if n['opcode'] == '++' else '-')))
                        done = True
                    elif rg:
                        listed.append((f, s.count('\n', 0, rg[0]) + 1, d[1], t, whole))
            if not done:
                for c in n.get('inner', []) or []:
                    walk(c, fn)

        for n in a.get('inner', []):
            loc = n.get('loc', {})
            fl = loc.get('file') or loc.get('spellingLoc', {}).get('file') or loc.get('expansionLoc', {}).get('file')
            if fl and os.path.abspath(os.path.join(ROOT, fl)) != fabs:
                continue
            walk(n, None)
        keep, last = [], -1
        for e in sorted(set(edits), key=lambda e: (e[0], -e[1])):
            if e[0] >= last:
                keep.append(e)
                last = e[1]
        for a0, z, t in reversed(keep):
            s = s[:a0] + t + s[z:]
        if keep and '--dry' not in sys.argv:
            open(f, 'w', encoding='latin-1').write(s)
        total += len(keep)
    print('byte arithmetic restored: %d sites' % total)
    for r in listed:
        print('  RECORD (needs a field): %s:%d %s (%s): %s' % r)


if __name__ == '__main__':
    main()
