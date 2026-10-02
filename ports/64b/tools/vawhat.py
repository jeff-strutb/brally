#!/usr/bin/env python3
"""What original object holds an address: vawhat.py VA [VA...]

Prints, for each address, the declared objects around it (start, 32-bit
size, every name and type the decompiled files used) -- the object that
contains VA-1 is the one a `p < VA` loop walks."""
import collections
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def main():
    os.chdir(ROOT)
    objs = collections.defaultdict(lambda: {'size': 0, 'names': set()})
    for r in csv.DictReader(open('ports/64b/types/globals.csv')):
        if not r['va']:
            continue
        va = int(r['va'], 16)
        o = objs[va]
        o['size'] = max(o['size'], int(r['size32'] or 0))
        o['names'].add('%s:%s' % (r['name'], r['type']))
    for r in csv.DictReader(open('ports/64b/types/globals_override.csv')):
        va = int(r['va'], 16)
        objs[va]['size'] = max(objs[va]['size'], int(r['size32'] or 0))
        objs[va]['names'].add('%s:%s (override)' % (r['name'], r['type']))
    keys = sorted(objs)
    for a in sys.argv[1:]:
        va = int(a, 16)
        print('== 0x%08X' % va)
        near = [k for k in keys if va - 0x400 <= k <= va + 0x10]
        for k in near:
            o = objs[k]
            tag = ''
            if k <= va - 1 < k + max(o['size'], 1):
                tag = '  <-- holds VA-1'
            print('  0x%08X +0x%-5X %s%s' % (k, o['size'], ' | '.join(sorted(o['names']))[:150], tag))


if __name__ == '__main__':
    main()
