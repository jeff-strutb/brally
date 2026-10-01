#!/usr/bin/env python3
"""Byte arithmetic on variables the port retyped from int to a pointer.

The decompiled source keeps many pointers in int variables and steps them in
BYTES (`v + 0x10`, `v += 0x2b68`). Once the port gives such a variable its
real pointer type, the same expression steps in ELEMENTS -- it still
compiles, and it is silently wrong. This finds every such site:

  * the variable's declaration in the ORIGINAL decompiled file (src/...),
    same function, is an integer type;
  * in the fork it is a pointer to something wider than a byte;
  * the fork applies + - += -= ++ -- to it with a non-zero operand.

Each site is printed with what the original stepped by, for a person (or
rewrite.py) to turn into a field or an index.

Usage: arithcheck.py [FILE...]      (default: every core file)
Output: build/portable/arithcheck.csv
"""
import collections
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402

ROOT = rawscan.ROOT
INTS = r'(?:unsigned\s+|signed\s+)?(?:int|long|short|char|uint|uint32_t|int32_t|DWORD|LONG|UINT|undefined4|uintptr_t|intptr_t)'


def orig_types(path):
    """{(function, variable): declared type text} from the original file."""
    out = {}
    if not os.path.exists(path):
        return out
    s = open(path, encoding='latin-1').read()
    for m in re.finditer(r'^[^\n;{}#]*?\b(\w+)\s*\(([^;{}]*)\)\s*\{', s, re.M):
        fn = m.group(1)
        if fn in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
            continue
        for p in m.group(2).split(','):
            pm = re.match(r'\s*(.*?)\s*\b(\w+)\s*$', p.strip())
            if pm:
                out[(fn, pm.group(2))] = pm.group(1)
        i, d = m.end() - 1, 0
        j = i
        while j < len(s):
            if s[j] == '{':
                d += 1
            elif s[j] == '}':
                d -= 1
                if d == 0:
                    break
            j += 1
        body = s[i:j]
        for dm in re.finditer(r'^\s*((?:const\s+)?%s(?:\s*\*)*)\s+([^;=()]+?)(?:=[^;]*)?;' % INTS, body, re.M):
            for v in dm.group(2).split(','):
                vm = re.match(r'\s*(\**)\s*(\w+)', v)
                if vm:
                    out.setdefault((fn, vm.group(2)), dm.group(1) + vm.group(1))
    return out


def main():
    os.chdir(ROOT)
    files = sys.argv[1:] or sorted(os.path.relpath(os.path.join(dp, f), ROOT)
                                   for dp, _, fs in os.walk('ports/64b/src/core')
                                   for f in fs if f.endswith(('.c', '.cpp')))
    rows = []
    for f in files:
        orig = orig_types(f[len('ports/64b/'):])
        if not orig:
            continue
        a = rawscan.ast(f)
        if a is None:
            continue
        s = open(f, encoding='latin-1').read()
        fabs = os.path.abspath(f)
        decls = {}

        def walk(n, fn):
            k = n.get('kind')
            if k in ('FunctionDecl', 'CXXMethodDecl') and 'inner' in n:
                fn = n.get('name')
            if k in ('ParmVarDecl', 'VarDecl'):
                decls[n.get('id')] = (fn, n.get('name'), n.get('type', {}).get('qualType', ''))
            if k in ('BinaryOperator', 'CompoundAssignOperator') and n.get('opcode') in ('+', '-', '+=', '-='):
                l, r = n['inner'][0], n['inner'][1]
                for side, other in ((l, r), (r, l)):
                    v = side
                    while v.get('kind') in ('ImplicitCastExpr', 'ParenExpr'):
                        v = v['inner'][0]
                    if v.get('kind') != 'DeclRefExpr':
                        continue
                    did = v.get('referencedDecl', {}).get('id')
                    if did not in decls:
                        continue
                    dfn, name, ty = decls[did]
                    ot = orig.get((dfn, name))
                    if not ot or '*' in ot:
                        continue
                    t = ty.strip()
                    if not t.endswith('*'):
                        continue
                    pointee = re.sub(r'\b(const|volatile)\b', '', t[:-1]).strip()
                    if pointee in ('char', 'unsigned char', 'signed char', 'uint8_t', 'int8_t', 'void', 'BYTE'):
                        continue
                    if n.get('opcode') in ('+', '-') and side is r and n.get('opcode') == '-':
                        continue
                    rg = n.get('range', {})
                    b = rg.get('begin', {}).get('expansionLoc', rg.get('begin', {}))
                    e = rg.get('end', {}).get('expansionLoc', rg.get('end', {}))
                    if 'offset' not in b or 'offset' not in e:
                        continue
                    line = s.count('\n', 0, b['offset']) + 1
                    rows.append((f, line, dfn, name, ot, t, s[b['offset']:e['offset'] + e.get('tokLen', 0)][:80]))
                    break
            if k == 'UnaryOperator' and n.get('opcode') in ('++', '--'):
                v = n['inner'][0]
                while v.get('kind') in ('ImplicitCastExpr', 'ParenExpr'):
                    v = v['inner'][0]
                if v.get('kind') == 'DeclRefExpr':
                    did = v.get('referencedDecl', {}).get('id')
                    if did in decls:
                        dfn, name, ty = decls[did]
                        ot = orig.get((dfn, name))
                        t = ty.strip()
                        if ot and '*' not in ot and t.endswith('*'):
                            pointee = re.sub(r'\b(const|volatile)\b', '', t[:-1]).strip()
                            if pointee not in ('char', 'unsigned char', 'signed char', 'uint8_t', 'int8_t', 'void'):
                                b = n.get('range', {}).get('begin', {})
                                b = b.get('expansionLoc', b)
                                if 'offset' in b:
                                    rows.append((f, s.count('\n', 0, b['offset']) + 1, dfn, name, ot, t, n['opcode']))
            for c in n.get('inner', []) or []:
                walk(c, fn)
        for n in a.get('inner', []):
            loc = n.get('loc', {})
            fl = loc.get('file') or loc.get('spellingLoc', {}).get('file') or loc.get('expansionLoc', {}).get('file')
            if fl and os.path.abspath(os.path.join(ROOT, fl)) != fabs:
                continue
            walk(n, None)
    rows = sorted(set(rows))
    os.makedirs('build/portable', exist_ok=True)
    with open('build/portable/arithcheck.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['file', 'line', 'func', 'var', 'orig_type', 'type', 'expr'])
        w.writerows(rows)
    per = collections.Counter(r[0] for r in rows)
    print('byte arithmetic on retyped pointers: %d sites in %d files' % (len(rows), len(per)))
    for f, c in per.most_common(25):
        print('  %3d %s' % (c, f))


if __name__ == '__main__':
    main()
