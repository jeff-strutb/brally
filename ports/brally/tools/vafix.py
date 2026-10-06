#!/usr/bin/env python3
"""Correct the global inventory's addresses from the original's relocations.

ports/brally/types/globals.csv takes an address from the 32-bit lane's symbol
map, which mixes verified placements with addresses copied from header
comments -- some of them the D3D build's. build/brally/wasm32/sites.csv is the
ground truth: every relocation in every placed Glide function, with the
address the ORIGINAL's dword holds there. For each inventory row:

  * the address the original gives that name in the row's own source file;
  * else the one address the original gives that name anywhere;
  * else the row is left as it was.

Usage: vafix.py [--dry]
"""
import collections
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def main():
    os.chdir(ROOT)
    per_file = collections.defaultdict(set)
    anywhere = collections.defaultdict(set)
    for r in csv.DictReader(open('build/brally/wasm32/sites.csv')):
        if r['addend'] != '0' or r['name'].startswith(('$', '_imp_')):
            continue
        va = int(r['va'], 16)
        per_file[(r['src'], r['name'])].add(va)
        anywhere[r['name']].add(va)
    p = 'ports/brally/types/globals.csv'
    rows = list(csv.DictReader(open(p)))
    # initialised file-private objects (types/private_globals.csv): their
    # file keeps them; they are no global
    pp = 'ports/brally/types/private_globals.csv'
    private = {r['name'] for r in csv.DictReader(open(pp))} if os.path.exists(pp) else set()
    dropped = len(rows)
    rows = [r for r in rows if r['name'] not in private]
    dropped -= len(rows)
    # one name, several original objects: the files that reach a minority
    # address name their object uniquely (name_VA), in the source and here
    renamed = []
    fn_names = {r['name'] for r in csv.DictReader(open('build/brally/wasm32/placement.csv'))}
    for nm, vas in list(anywhere.items()):
        if len(vas) < 2 or nm in fn_names or any(0x10001000 <= v < 0x10077000 for v in vas):
            continue
        count = collections.Counter(v for (src, n), vs in per_file.items() if n == nm for v in vs)
        major = count.most_common(1)[0][0]
        for (src, n), vs in list(per_file.items()):
            if n != nm or len(vs) != 1 or next(iter(vs)) == major:
                continue
            v = next(iter(vs))
            new = '%s_%08X' % (nm, v)
            f = 'ports/brally/' + src
            if os.path.exists(f) and '--dry' not in sys.argv:
                t = open(f, encoding='latin-1').read()
                t2 = re.sub(r'\b%s\b' % re.escape(nm), new, t)
                if t2 != t:
                    open(f, 'w', encoding='latin-1').write(t2)
            for r in rows:
                if r['name'] == nm and r['decl_file'] == f:
                    r['name'] = new
            per_file[(src, new)] = {v}
            anywhere[new] = {v}
            renamed.append((nm, new, src))
        anywhere[nm] = {major}
    fixed, ambiguous = [], set()
    for r in rows:
        src = r['decl_file'][len('ports/brally/'):] if r['decl_file'].startswith('ports/brally/') else r['decl_file']
        cand = per_file.get((src, r['name']))
        if not cand or len(cand) != 1:
            cand = anywhere.get(r['name'])
            if cand and len(cand) > 1:
                ambiguous.add(r['name'])
                continue
        if not cand or len(cand) != 1:
            continue
        va = '0x%08X' % next(iter(cand))
        if r['va'] != va:
            fixed.append((r['name'], r['va'], va, r['decl_file']))
            r['va'] = va
    if '--dry' not in sys.argv and (fixed or renamed or dropped):
        with open(p, 'w', newline='') as fh:
            w = csv.DictWriter(fh, list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
    for r in renamed:
        print('  renamed %s -> %s in %s' % r)
    print('inventory addresses corrected from the original: %d rows (%d names); %d names reach '
          'several addresses' % (len(fixed), len({f[0] for f in fixed}), len(ambiguous)))
    for f in fixed[:60]:
        print('  %-28s %s -> %s  (%s)' % f)
    for n in sorted(ambiguous):
        print('  several: %s %s' % (n, sorted('0x%08X' % v for v in anywhere[n])))


if __name__ == '__main__':
    main()
