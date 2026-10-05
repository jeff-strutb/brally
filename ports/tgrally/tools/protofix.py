#!/usr/bin/env python3
"""protofix.py -- make every TU's declaration of a core function agree with
its definition at the native ABI (the mismatches abicheck.py lists).

A declaration is rewritten from the definition's own parameter list: in the
defining TU word for word; elsewhere with each pointer parameter (and a
pointer result) spelled `void *`, which has the same ABI and needs no type
the declaring TU may not have.  A call that passed an integer where the
definition takes a pointer then fails to compile, and errfix.py turns the
integer (an original address) into the pointer.

    abicheck.py > list; protofix.py list
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
FLAGS = ['-fsyntax-only', '-std=gnu89', '-Wno-everything', '-Iports/tgrally/include',
         '-Iports/tgrally/platform/include', '-include', 'ports/tgrally/platform/include/ultra64.h',
         '-include', 'ports/tgrally/platform/include/tgr_core.h', '-Xclang', '-ast-dump=json']


def definition(tu, name):
    """(return type, [param types], variadic) of name's definition in tu"""
    p = subprocess.run(['clang'] + FLAGS + ['-Xclang', '-ast-dump-filter=' + name, tu],
                       capture_output=True, text=True, cwd=ROOT)
    for chunk in re.split(r'\n(?=\{)', p.stdout):
        try:
            d = json.loads(chunk)
        except ValueError:
            continue
        if d.get('kind') != 'FunctionDecl' or d.get('name') != name:
            continue
        if not any(c.get('kind') == 'CompoundStmt' for c in d.get('inner', [])):
            continue
        q = d['type']['qualType']
        ret = q[:q.index('(')].strip()
        params = [c['type']['qualType'] for c in d.get('inner', []) if c.get('kind') == 'ParmVarDecl']
        return ret, params, '...' in q
    return None


def neutral(t):
    t = t.strip()
    if '(*' in t or t.endswith('*') or '[' in t:
        return 'void *'
    return t


def main():
    rows = [l for l in open(sys.argv[1]) if ': ' in l]
    done = 0
    cache = {}
    for l in rows:
        m = re.match(r'^(\S+):(\d+): (\w+) (?:unprototyped|declared .*?); defined .* in (\S+)$', l.strip())
        if not m:
            continue
        tu, line, name, deftu = m.group(1), int(m.group(2)), m.group(3), m.group(4)
        key = (deftu, name)
        if key not in cache:
            cache[key] = definition(deftu, name)
        dfn = cache[key]
        if not dfn:
            print('no definition text for %s' % name)
            continue
        ret, params, var = dfn
        if tu == deftu:
            ps = ', '.join(params) or 'void'
            r = ret
        else:
            ps = ', '.join(neutral(p) for p in params) or 'void'
            r = neutral(ret)
        if var:
            ps += ', ...'
        new = '%s %s(%s);' % (r, name, ps)
        lines = open(os.path.join(ROOT, tu)).read().split('\n')
        s = lines[line - 1]
        mm = re.match(r'^(\s*)(?:extern\s+)?[^;(]*\b%s\s*\([^;]*\)\s*;(.*)$' % re.escape(name), s)
        if not mm:
            print('%s:%d: cannot rewrite %r' % (tu, line, s.strip()))
            continue
        lines[line - 1] = mm.group(1) + new + mm.group(2)
        open(os.path.join(ROOT, tu), 'w').write('\n'.join(lines))
        done += 1
    print('protofix: %d declarations rewritten' % done)


if __name__ == '__main__':
    main()
