#!/usr/bin/env python3
"""abicheck.py -- every declaration of a core function agrees with its
definition at the native ABI.

The decomp declares callees locally in each file, and IDO's 32-bit ABI made
int, long and pointers interchangeable.  Natively they are not: an int passed
where a pointer is expected arrives with undefined high bits.  This compares,
for every function, each TU's declaration with the definition by ABI class
(i8, i16, i32, i64, ptr, f32, f64, void, struct:N; char and short apart
because Apple arm64 packs stack arguments by their declared size), and lists every mismatch and every
unprototyped declaration of a function that takes or returns a pointer,
64-bit integer or floating value.  Calls to functions with no definition in
the core (the platform's libultra surface) are checked against
platform/include/ultra64.h by the compiler itself.

    .venv/bin/python ports/tgrally/tools/abicheck.py
"""
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SRC = 'ports/tgrally/src'
FLAGS = ['-fsyntax-only', '-std=gnu89', '-Wno-everything', '-Iports/tgrally/include',
         '-Iports/tgrally/platform/include', '-include', 'ports/tgrally/platform/include/ultra64.h',
         '-include', 'ports/tgrally/platform/include/tgr_core.h', '-Xclang', '-ast-dump=json']


def tus():
    out = []
    for dp, dn, fn in os.walk(os.path.join(ROOT, SRC)):
        out += [os.path.relpath(os.path.join(dp, f), ROOT) for f in fn if f.endswith('.c')]
    return sorted(out)


def cls(t):
    t = t.strip()
    if t == 'void':
        return 'void'
    if '*' in t or '[' in t or '(' in t:
        return 'ptr'
    if t in ('float',):
        return 'f32'
    if t in ('double', 'long double'):
        return 'f64'
    if t in ('long long', 'unsigned long long', 'long', 'unsigned long'):
        return 'i64'
    if t.startswith(('struct ', 'union ')):
        return 'agg:' + t
    # on Apple arm64 the arguments past the eighth go on the stack packed by
    # their declared size, so a char or short parameter is its own class
    if t in ('char', 'signed char', 'unsigned char', '_Bool'):
        return 'i8'
    if t in ('short', 'unsigned short', 'short int', 'unsigned short int'):
        return 'i16'
    return 'i32'


def sig(d):
    ty = d['type']
    q = ty.get('desugaredQualType', ty['qualType'])
    params = [c for c in d.get('inner', []) if c.get('kind') == 'ParmVarDecl']
    ret = q.split('(')[0]
    ps = [cls(p['type'].get('desugaredQualType', p['type']['qualType'])) for p in params]
    proto = '(void)' in q or bool(params) or ('...' in q)
    noproto = q.endswith('()')
    return cls(ret), ps, ('...' in q), noproto


def calls(node, out, line=[0]):
    """every call by name with its argument count: [(callee, nargs, line)]"""
    loc = node.get('loc', {}) or {}
    if 'line' in loc:
        line[0] = loc['line']
    r = node.get('range', {}).get('begin', {})
    if 'line' in r:
        line[0] = r['line']
    if node.get('kind') == 'CallExpr':
        inner = node.get('inner', [])
        if inner:
            c = inner[0]
            while c.get('kind') in ('ImplicitCastExpr', 'ParenExpr') and c.get('inner'):
                c = c['inner'][0]
            if c.get('kind') == 'DeclRefExpr' and c.get('referencedDecl', {}).get('kind') == 'FunctionDecl':
                out.append((c['referencedDecl']['name'], len(inner) - 1, line[0]))
    for c in node.get('inner', []):
        calls(c, out, line)


def scan(tu):
    p = subprocess.run(['clang'] + FLAGS + [tu], capture_output=True, text=True, cwd=ROOT)
    ast = json.loads(p.stdout)
    cl = []
    for d in ast.get('inner', []):
        if d.get('kind') == 'FunctionDecl':
            calls(d, cl)
    CALLS[tu] = cl
    rows = []
    for d in ast.get('inner', []):
        if d.get('kind') != 'FunctionDecl' or d.get('isImplicit'):
            continue
        body = any(c.get('kind') == 'CompoundStmt' for c in d.get('inner', []))
        line = d.get('loc', {}).get('line') or d.get('loc', {}).get('expansionLoc', {}).get('line')
        rows.append((d['name'], body, sig(d), line, d.get('storageClass', '')))
    return tu, rows


CALLS = {}


def scan2(tu):
    r = scan(tu)
    return r, CALLS.get(tu, [])


def main():
    defs, decls = {}, {}
    allcalls = {}
    with ThreadPoolExecutor(14) as ex:
        for (tu, rows), cl in ex.map(scan2, tus()):
            allcalls[tu] = cl
            for name, body, s, line, st in rows:
                if body and st != 'static':
                    defs[name] = (tu, s)
                elif body:
                    defs.setdefault(name, (tu, s))
                else:
                    decls.setdefault(name, []).append((tu, line, s))
    bad = 0
    for name in sorted(decls):
        if name not in defs:
            continue
        dtu, (dr, dp, dv, dn) = defs[name]
        for tu, line, (r, p, v, noproto) in decls[name]:
            if noproto:
                risky = dr not in ('i32', 'void') or any(x != 'i32' for x in dp)
                if risky:
                    print('%s:%s: %s unprototyped; defined (%s) -> %s in %s' % (
                        tu, line, name, ', '.join(dp), dr, dtu))
                    bad += 1
                continue
            if r != dr or (p != dp and not (v or dv)):
                print('%s:%s: %s declared (%s) -> %s; defined (%s) -> %s in %s' % (
                    tu, line, name, ', '.join(p), r, ', '.join(dp), dr, dtu))
                bad += 1
    for tu in sorted(allcalls):
        for name, n, line in allcalls[tu]:
            if name in defs:
                dtu, (dr, dp, dv, dn) = defs[name]
                if n != len(dp) and not dv and not (len(dp) == 0 and n == 0):
                    print('%s:%s: %s called with %d arguments; defined with %d in %s' % (
                        tu, line, name, n, len(dp), dtu))
                    bad += 1
    print('abicheck: %d mismatches' % bad, file=sys.stderr)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
