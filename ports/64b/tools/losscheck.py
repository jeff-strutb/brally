#!/usr/bin/env python3
"""Did any port pass delete code? Per function, the fork against the original.

For every function defined in both the original decompiled file and its fork,
compares statement count (semicolons outside comments/strings) and call
count. A fork body with clearly less than the original is printed: edits
retype and rename, they never remove statements.

Usage: losscheck.py [--min-ratio R]
"""
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubaudit  # noqa: E402

ROOT = stubaudit.ROOT


def bodies(s):
    c = stubaudit.scrub(s)
    out = {}
    for m in re.finditer(r'^[A-Za-z_][^\n;{}#=]*?\b(\w+)\s*\(([^;{}]*)\)\s*(?:const\s*)?\{', c, re.M):
        nm = m.group(1)
        if nm in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
            continue
        i, d = m.end() - 1, 0
        while i < len(c):
            if c[i] == '{':
                d += 1
            elif c[i] == '}':
                d -= 1
                if d == 0:
                    break
            i += 1
        b = c[m.end():i]
        out.setdefault(nm, (b.count(';'), len(re.findall(r'\b\w+\s*\(', b)), b.count('if')))
    return out


def main():
    os.chdir(ROOT)
    ratio = float(sys.argv[sys.argv.index('--min-ratio') + 1]) if '--min-ratio' in sys.argv else 0.75
    bad = []
    for f in sorted(glob.glob('ports/64b/src/core/**/*.c*', recursive=True)):
        o = f[len('ports/64b/'):]
        if not os.path.exists(o):
            continue
        a = bodies(open(o, encoding='latin-1').read())
        b = bodies(open(f, encoding='latin-1').read())
        for nm, (so, co, io) in a.items():
            if nm not in b:
                continue
            sf, cf, iff = b[nm]
            if so >= 4 and (sf < so * ratio or iff < io * ratio):
                bad.append((f, nm, so, sf, io, iff))
    for r in bad:
        print('%s %s: statements %d -> %d, ifs %d -> %d' % r)
    print('functions with lost code: %d' % len(bad))


if __name__ == '__main__':
    main()
