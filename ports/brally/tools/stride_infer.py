#!/usr/bin/env python3
"""Record stride and pointer slots of an undeclared region, from the trace.

For a region (regions.csv base name) take every measured (offset, width,
held-address). For each candidate stride P (4..0x4000, multiples of 4) the
pointer offsets fold to residues mod P; P fits when no non-pointer 4-byte
access lands on a pointer residue and the pointer residues recur in at least
two records. The smallest fitting P with the fewest residues is printed with
its layout: residue -> width and pointer-ness, and the records seen.

Usage: stride_infer.py BASE...
"""
import collections
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def main():
    os.chdir(ROOT)
    want = set(sys.argv[1:])
    meas = collections.defaultdict(dict)
    for r in csv.DictReader(open('build/brally/null-soft/trace/objects.csv')):
        b = r['object'][8:] if r['object'].startswith('OVERRUN:') else r['object']
        if b not in want:
            continue
        o, w = int(r['offset'], 16), int(r['width'])
        h = r['holds_address'] == 'yes'
        cur = meas[b].get(o)
        meas[b][o] = (max(w, cur[0]) if cur else w, h or (cur[1] if cur else False))
    for b in sys.argv[1:]:
        m = meas[b]
        ptr = sorted(o for o, (w, h) in m.items() if h)
        non = sorted(o for o, (w, h) in m.items() if not h and w == 4)
        best = None
        for P in range(4, 0x4001, 4):
            res = {o % P for o in ptr}
            if any(o % P in res for o in non):
                continue
            recs = {o // P for o in ptr}
            if len(recs) < 2:
                continue
            score = (len(res), P)
            if best is None or score < best[0]:
                best = (score, P, res, recs)
            if len(res) == 1:
                break
        print('== %s: %d offsets, %d hold addresses' % (b, len(m), len(ptr)))
        if not best:
            print('   no stride fits')
            continue
        _, P, res, recs = best
        lay = collections.defaultdict(set)
        for o, (w, h) in m.items():
            lay[o % P].add((w, h))
        print('   stride 0x%X; records touched %d..%d (%d); pointer residues %s' % (
            P, min(o // P for o in m), max(o // P for o in m), len({o // P for o in m}),
            ' '.join('0x%X' % x for x in sorted(res))))
        print('   layout: ' + ' '.join('+0x%X:%s%s' % (k, '/'.join(str(w) for w, _ in sorted(v)),
                                                  '*' if any(h for _, h in v) else '')
                                     for k, v in sorted(lay.items())[:40]))


if __name__ == '__main__':
    main()
