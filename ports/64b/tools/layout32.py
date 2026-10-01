#!/usr/bin/env python3
"""The original (i686) layout of a C type, and a check of measured pointer
offsets against it.

    layout32.py HEADER TYPE [GLOBAL]

prints every field's offset, size and type (clang -fdump-record-layouts at
i686), and with GLOBAL, the offsets build/portable/trace/objects.csv says
held addresses, each marked by the field it falls in: a pointer field is
right, anything else is a 64-bit bug in the type.
"""
import collections
import csv
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recspec  # noqa: E402


def layout(header, ty):
    src = '#include "%s"\nint __probe = sizeof(%s);\n' % (header, ty)
    p = subprocess.run(['clang'] + recspec.I686 + recspec.FLAGS + ['-Xclang', '-fdump-record-layouts', '-x', 'c', '-'],
                       input=src.encode(), capture_output=True, cwd=recspec.ROOT)
    tag = re.sub(r'^struct\s+', '', ty)
    blocks = p.stdout.decode().split('*** Dumping AST Record Layout')
    rows, size = [], None
    for b in blocks:
        lines = b.strip().split('\n')
        if not lines or not re.match(r'\s*0 \| (struct|union) %s\b' % re.escape(tag), lines[0]):
            continue
        for l in lines[1:]:
            m = re.match(r'\s*(\d+)(?::\d+-\d+)? \|(\s*)(.*?) (\w+)$', l)
            if m:
                rows.append((int(m.group(1)), len(m.group(2)), m.group(3), m.group(4)))
            m = re.search(r'\[sizeof=(\d+)', l)
            if m:
                size = int(m.group(1))
        break
    return rows, size


def main():
    os.chdir(recspec.ROOT)
    hdr, ty = sys.argv[1], sys.argv[2]
    rows, size = layout(hdr, ty)
    print('%s: sizeof %s' % (ty, size))
    for off, depth, t, n in rows:
        print('  0x%04X %s%s %s' % (off, ' ' * (depth - 1), t, n))
    if len(sys.argv) > 3 and size:
        meas = set()
        for r in csv.DictReader(open('build/portable/trace/objects.csv')):
            if r['object'] == sys.argv[3] and r['holds_address'] == 'yes' and r['width'] == '4':
                meas.add(int(r['offset'], 16) % size)
        for o in sorted(meas):
            leaf = [(off, t, n) for off, d, t, n in rows if off <= o]
            f = max(leaf, key=lambda x: x[0]) if leaf else None
            ok = f and f[0] == o and '*' in f[1]
            print('  %s held an address at +0x%X  -> %s' % ('ok ' if ok else 'BUG', o,
                                                          '%s %s @0x%X' % (f[1], f[2], f[0]) if f else '?'))


if __name__ == '__main__':
    main()
