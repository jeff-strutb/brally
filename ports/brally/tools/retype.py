#!/usr/bin/env python3
"""Give a core global one type everywhere it is declared.

The decomp declares an original global in each file that uses it, often as
`int` when it holds a handle or pointer. The 64-bit core needs the real type
in every declaration (and in its one definition).

Usage: retype.py NAME 'TYPE' [NAME 'TYPE' ...]   (run from the repo root)
"""
import glob
import re
import sys

args = sys.argv[1:]
pairs = list(zip(args[0::2], args[1::2]))
files = [p for d in ('ports/brally/src', 'ports/brally/include')
         for p in glob.glob(d + '/**/*', recursive=True) if p.endswith(('.c', '.cpp', '.h'))]
for name, ty in pairs:
    rx = re.compile(r'^(\s*(?:extern\s+(?:"C"\s+)?)?)(?:(?:const|volatile|unsigned|signed|struct)\s+)*[A-Za-z_][\w]*(?:\s*\*+\s*|\s+)'
                    + re.escape(name) + r'(\s*(?:=[^;]*)?;)', re.M)
    n = 0
    for f in files:
        s = open(f, encoding='latin-1').read()
        s2, k = rx.subn(lambda m: m.group(1) + ty + ' ' + name + m.group(2), s)
        if k:
            open(f, 'w', encoding='latin-1').write(s2)
            n += k
    print('%s -> %s: %d declarations' % (name, ty, n))
