#!/usr/bin/env python3
"""The original's memory regions the 64-bit core does not declare.

From build/portable/trace/objects.csv: every access that ran past the object
the core declares at that address (OVERRUN:<object>) belongs to an
undeclared region starting there. Each region's extent: from the object's
address through the highest byte the game touched, then on to the next
address any original code references on its own (build/wasm/sites.csv) --
nothing between two referenced addresses can belong to anything else.

Output build/portable/trace/regions.csv: base object, VA, measured end,
bounded end, how many offsets held addresses, and the source files that
touch it (the clue to what it is: loaded file data stays bytes, a pool of
game records needs its record type).

Usage: regions.py
"""
import bisect
import collections
import csv
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def main():
    os.chdir(ROOT)
    va_of = {}
    for r in csv.DictReader(open('ports/64b/types/globals.csv')):
        if r['va']:
            va_of.setdefault(r['name'], int(r['va'], 16))
    refd = sorted({int(r['va'], 16) for r in csv.DictReader(open('build/wasm/sites.csv'))
                   if r['addend'] == '0' and 0x10077000 <= int(r['va'], 16) < 0x118F2000})
    agg = collections.defaultdict(lambda: {'hi': 0, 'ptr': 0, 'n': 0, 'files': collections.Counter()})
    for r in csv.DictReader(open('build/portable/trace/objects.csv')):
        if not r['object'].startswith('OVERRUN:'):
            continue
        a = agg[r['object'][8:]]
        a['hi'] = max(a['hi'], int(r['offset'], 16) + int(r['width']))
        a['n'] += 1
        a['ptr'] += r['holds_address'] == 'yes'
        for st in r['sites'].split():
            a['files'][st.rsplit(':', 2)[0]] += 1
    rows = []
    for nm, a in agg.items():
        va = va_of.get(nm)
        if va is None:
            continue
        k = bisect.bisect_left(refd, va + a['hi'])
        bound = refd[k] if k < len(refd) else va + a['hi']
        inside = sum(1 for x in refd[bisect.bisect_right(refd, va):k])
        rows.append((va, nm, a['hi'], bound - va, inside, a['n'], a['ptr'],
                     ' '.join('%s(%d)' % (f.split('/')[-1], c) for f, c in a['files'].most_common(4))))
    rows.sort()
    with open('build/portable/trace/regions.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['va', 'base', 'touched', 'bounded', 'refs_inside', 'offsets', 'address_offsets', 'files'])
        for va, nm, hi, b, ins, n, p, fl in rows:
            w.writerow(['0x%08X' % va, nm, '0x%X' % hi, '0x%X' % b, ins, n, p, fl])
    print('undeclared regions: %d (%d hold addresses)' % (len(rows), sum(1 for r in rows if r[6])))


if __name__ == '__main__':
    main()
