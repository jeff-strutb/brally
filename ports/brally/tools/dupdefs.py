#!/usr/bin/env python3
"""One body per original function: the placed one.

A name defined (with a body, at file scope) in more than one core file is
kept only in the file build/wasm/placement.csv places it from; the other
bodies -- earlier or D3D transcriptions of the same function -- are removed,
file-statics included, with their static prototypes. This is what the
32-bit lane does (w2c.py func_ref): a reference by name binds to the placed
body, and an unplaced local never stands in for it.

Usage: dupdefs.py [--dry]
"""
import collections
import csv
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubaudit  # noqa: E402

ROOT = stubaudit.ROOT


def bodies(s):
    c = stubaudit.scrub(s)
    for m in re.finditer(r'^[A-Za-z_][^\n;{}#=]*?(?<![:~\w])(\w+)\s*\(([^;{}]*)\)\s*(?:const\s*)?\{', c, re.M):
        nm = m.group(1)
        if nm in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
            continue
        enc = stubaudit.enclosing(s, m.start())
        if enc is not None and not re.search(r'extern\s+"C"\s*$', enc.rstrip()):
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
        yield nm, m.start(), i + 1


def main():
    os.chdir(ROOT)
    place = {r['name']: 'ports/brally/' + r['src'] for r in csv.DictReader(open('build/wasm/placement.csv'))}
    where = collections.defaultdict(list)
    texts = {}
    for f in sorted(glob.glob('ports/brally/src/core/**/*.c*', recursive=True)):
        s = open(f, encoding='latin-1').read()
        texts[f] = s
        for nm, a, z in bodies(s):
            where[nm].append((f, a, z))
    edits = collections.defaultdict(list)
    for nm, locs in where.items():
        files = {f for f, _, _ in locs}
        if len(files) < 2 or nm not in place or place[nm] not in files:
            continue
        for f, a, z in locs:
            if f != place[nm]:
                edits[f].append((a, z, '/* %s: the placed body is %s */' % (nm, os.path.basename(place[nm]))))
    n = 0
    for f, es in edits.items():
        s = texts[f]
        for a, z, t in sorted(es, reverse=True):
            s = s[:a] + t + s[z:]
            n += 1
        for nm in {t.split(':')[0][3:] for _, _, t in es}:
            s = re.sub(r'^static\b[^;{}#\n]*\b%s\s*\([^;{}]*\)\s*;[ \t]*\n' % re.escape(nm), '', s, flags=re.M)
        if '--dry' not in sys.argv:
            open(f, 'w', encoding='latin-1').write(s)
        print('%s: %s' % (f, ', '.join(sorted({t.split(':')[0][3:] for _, _, t in es}))))
    print('duplicate bodies removed: %d' % n)


if __name__ == '__main__':
    main()
