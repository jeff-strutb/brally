#!/usr/bin/env python3
"""Find C claims that are stale because the Glide match already lives in the
C++ lane (or in another C file).

Why this exists: the port bodies in the slice files are tagged by their D3D
VA. When the Glide match for that same function is split out into its own
TU -- `src/core/cpp/<VA>.cpp` or `src/core/generated/<VA>.c`, the
convention the slice files already use -- the slice's tag keeps claiming
the VA, so the residue carries a `diff` row for a function that is in fact
byte-exact. Those phantom rows make the remaining work look bigger than it
is, and they hide the real targets when you sort by diff count. Fifteen of
them turned up across three files in one session before this was written.

    python3 tools/stale_claims.py            # report
    python3 tools/stale_claims.py --fix      # rewrite the tags in place

`--fix` replaces

    /* @implements <d3dVA> d3d <Name> */

with

    /* port-only body; Glide match is <path of the matching TU> */

which is exactly the wording the hand-converted ones use. It only ever
touches a tag whose VA is ALREADY matched somewhere else, so it cannot
lose a claim. Re-sweep the touched files afterwards -- the report keeps the
old row until you do.
"""
from __future__ import print_function

import argparse
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORTS = ['build/match/report.csv', 'build/match/report_cpp.csv']


def rows():
    out = []
    for rep in REPORTS:
        p = os.path.join(ROOT, rep)
        if not os.path.exists(p):
            continue
        for r in csv.DictReader(open(p)):
            r['_report'] = rep
            out.append(r)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fix', action='store_true',
                    help='rewrite the stale tags in place')
    args = ap.parse_args()

    all_rows = rows()
    matched = {}
    for r in all_rows:
        if r.get('status') == 'match':
            matched.setdefault((r.get('va') or '').lower(), r.get('file'))

    stale = []
    for r in all_rows:
        if r.get('status') == 'match':
            continue
        va = (r.get('va') or '').lower()
        owner = matched.get(va)
        if owner and owner != r.get('file'):
            stale.append((r.get('file'), va, r.get('symbol') or r.get('name'),
                          owner, r.get('diffs')))

    if not stale:
        print('no stale claims: every diff row owns its VA')
        return

    by_file = {}
    for f, va, sym, owner, d in stale:
        by_file.setdefault(f, []).append((va, sym, owner, d))

    print('%d stale claim(s) in %d file(s) -- the VA is already matched '
          'elsewhere:' % (len(stale), len(by_file)))
    for f in sorted(by_file):
        print('  %s' % f)
        for va, sym, owner, d in sorted(by_file[f]):
            print('      %-12s %-32s diffs=%-5s -> %s' % (va, sym, d, owner))

    if not args.fix:
        print('\nre-run with --fix to convert the tags, then re-sweep those '
              'files')
        return

    touched = 0
    for f in sorted(by_file):
        p = os.path.join(ROOT, f)
        if not os.path.exists(p):
            print('  SKIP (no such file) %s' % f)
            continue
        src = open(p).read()
        n = 0
        for va, sym, owner, _ in by_file[f]:
            if not sym:
                continue
            # The slice tags carry the D3D VA, not the Glide one, so match on
            # the symbol name and accept whatever VA the tag names.
            pat = re.compile(
                r'/\* @implements 0x[0-9A-Fa-f]{8} \w+ %s \*/'
                % re.escape(sym))
            new = '/* port-only body; Glide match is %s */' % owner
            src, k = pat.subn(new, src)
            if k == 0:
                print('  no tag found for %s in %s -- leave it alone'
                      % (sym, f))
            n += k
        if n:
            open(p, 'w').write(src)
            touched += n
            print('  %s: converted %d tag(s)' % (f, n))

    print('\nconverted %d tag(s). Now re-sweep the touched files, e.g.\n'
          '  python3 tools/match_sweep.py %s'
          % (touched, ' '.join(sorted(by_file))))


if __name__ == '__main__':
    main()
