#!/usr/bin/env python3
"""inventory.py -- every top-level declaration the core's TUs make.

Runs clang's AST dump over each TU of ports/tgrally/src (N64 target, so types
are read as the original compiler saw them) and writes
build/tgrally/null-null/inventory.json:

  vars:  name -> {decl: [(file, type, storage, used)], defined: [file]}
  funcs: name -> {decl: [(file, type)], defined: [file]}

The globals generator and the platform layer's surface checks read it.
"""
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SRC = 'ports/tgrally/src'
OUT = os.path.join(ROOT, 'build/tgrally/null-null/inventory.json')
FLAGS = ['-target', 'mips-linux-gnu', '-fsyntax-only', '-std=gnu89', '-Wno-everything',
         '-Iports/tgrally/include', '-Xclang', '-ast-dump=json']


def tus():
    out = []
    for dp, dn, fn in os.walk(os.path.join(ROOT, SRC)):
        out += [os.path.relpath(os.path.join(dp, f), ROOT) for f in fn if f.endswith('.c')]
    return sorted(out)


def scan(tu):
    p = subprocess.run(['clang'] + FLAGS + [tu], capture_output=True, text=True, cwd=ROOT)
    ast = json.loads(p.stdout)
    rows = []
    for d in ast.get('inner', []):
        if d.get('isImplicit'):
            continue
        loc = d.get('loc', {})
        if 'includedFrom' in loc or ('file' in loc and loc['file'] != tu):
            inc = True
        else:
            inc = False
        k = d.get('kind')
        if k == 'VarDecl':
            rows.append(('var', d['name'], d['type']['qualType'], d.get('storageClass', ''),
                         bool(d.get('isUsed') or d.get('isReferenced')), 'init' in d, inc))
        elif k == 'FunctionDecl':
            body = any(c.get('kind') == 'CompoundStmt' for c in d.get('inner', []))
            rows.append(('func', d['name'], d['type']['qualType'], d.get('storageClass', ''),
                         bool(d.get('isUsed') or d.get('isReferenced')), body, inc))
    return tu, rows


def main():
    res = {'vars': {}, 'funcs': {}}
    with ThreadPoolExecutor(14) as ex:
        for tu, rows in ex.map(scan, tus()):
            for kind, name, ty, st, used, defn, inc in rows:
                e = res['vars' if kind == 'var' else 'funcs'].setdefault(
                    name, {'decl': [], 'defined': [], 'used': []})
                if kind == 'var':
                    is_def = defn or st not in ('extern',)
                else:
                    is_def = defn
                if is_def:
                    e['defined'].append([tu, ty, st])
                else:
                    e['decl'].append([tu, ty])
                if used:
                    e['used'].append(tu)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(res, open(OUT, 'w'), indent=0, sort_keys=True)
    v, f = res['vars'], res['funcs']
    print('vars %d (defined %d), funcs %d (defined %d)' % (
        len(v), sum(1 for x in v.values() if x['defined']),
        len(f), sum(1 for x in f.values() if x['defined'])))


if __name__ == '__main__':
    main()
