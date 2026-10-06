#!/usr/bin/env python3
"""Drop a source file's private copy of a type a header now provides.

The decompiled files often carried their own copy of a struct to avoid an
include. When the 64-bit core includes that header, clang reports the copy
as a redefinition. If the two definitions are the same apart from comments
and whitespace, the file's copy is removed; otherwise it is reported.

Usage: dedupetypes.py   (reads build/brally/null-soft/obj/*.err)
"""
import glob
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def norm(t):
    t = re.sub(r'/\*.*?\*/|//[^\n]*', '', t, flags=re.S)
    return re.sub(r'\s+', '', t)


def def_at(path, line):
    """(start, end, text) of the typedef/struct definition starting at line."""
    s = open(os.path.join(ROOT, path), encoding='latin-1').read()
    lines = s.split('\n')
    a = sum(len(l) + 1 for l in lines[:line - 1])
    b = s.find('{', a)
    if b == -1:
        return None
    d, i = 0, b
    while i < len(s):
        if s[i] == '{':
            d += 1
        elif s[i] == '}':
            d -= 1
            if d == 0:
                break
        i += 1
    e = s.find(';', i) + 1
    return a, e, s[a:e], s


def main():
    os.chdir(ROOT)
    pairs = set()
    for ef in glob.glob('build/brally/null-soft/obj/*.err'):
        t = open(ef, errors='replace').read()
        for m in re.finditer(r"^(\S+?):(\d+):\d+: error: redefinition of '(\w+)'\n(?:.*\n){0,3}?"
                             r"(\S+?):(\d+):\d+: note: previous definition is here", t, re.M):
            f1, l1, nm, f2, l2 = m.group(1), int(m.group(2)), m.group(3), m.group(4), int(m.group(5))
            f2 = os.path.normpath(f2)
            if f1.endswith(('.c', '.cpp')) and f2.endswith('.h'):
                pairs.add((f1, l1, nm, f2, l2))
    done = {}
    for f1, l1, nm, f2, l2 in sorted(pairs, key=lambda x: (x[0], -x[1])):
        if (f1, l1) in done:
            continue
        a = def_at(f1, l1)
        b = def_at(f2, l2)
        if not a or not b:
            continue
        if norm(a[2]) == norm(b[2]) or norm(a[2]).replace('typedefstruct' + nm, 'typedefstruct') == \
                norm(b[2]).replace('typedefstruct' + nm, 'typedefstruct'):
            s = open(f1, encoding='latin-1').read()
            s = s[:a[0]] + '/* %s: %s */' % (nm, os.path.basename(f2)) + s[a[1]:]
            open(f1, 'w', encoding='latin-1').write(s)
            done[(f1, l1)] = True
            print('removed %s copy of %s (= %s)' % (f1, nm, os.path.basename(f2)))
        else:
            print('DIFFERENT %s in %s:%d vs %s:%d' % (nm, f1, l1, f2, l2))


if __name__ == '__main__':
    main()
