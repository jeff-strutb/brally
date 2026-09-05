#!/usr/bin/env python3
"""Find UNMATCHED originals that are byte-close to an already-MATCHED one.

Rule 7's cadence needs cause groups, and the cheapest cause group there is is
"the same function again with two facts changed".  Twice on 2026-09-05 this
screen paid immediately: 0x10020460 is 0x1001FF60 with 37 bytes different and
0x10020900 is 0x1001ECF0 with 37 -- both went byte-exact from the matched
sibling's source in one pass, where the raw drafts had been sitting in T1.

A pair only means anything at EQUAL SIZE (a different size is a different
function), so the screen groups by exact byte length and reports, for every
unmatched original, its nearest matched same-size neighbour and the count of
differing bytes.  Call displacements and absolute operands differ by
construction, so a real twin still shows a few dozen: the useful threshold is
"tens", not "zero".

    .venv/bin/python tools/twinscreen.py            # every hit, closest first
    .venv/bin/python tools/twinscreen.py --max 80   # only very close pairs
"""
import csv, os, re, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
ORIG = os.path.join(ROOT, 'build', 'match', 'orig')

maxdiff = 200
minsize = 48
if '--max' in sys.argv:
    maxdiff = int(sys.argv[sys.argv.index('--max') + 1])
if '--min-size' in sys.argv:
    minsize = int(sys.argv[sys.argv.index('--min-size') + 1])

# linker/CRT output and data rows are not decomp targets; rule 0's fence list
# is the record of that, and a 6-byte import thunk matching another 6-byte
# import thunk is noise, not a twin.
fenced = set()
fp = os.path.join(ROOT, 'config', 'fenced.csv')
if os.path.exists(fp):
    for m in re.finditer(r'0x([0-9A-Fa-f]{8})', open(fp).read()):
        fenced.add(int(m.group(1), 16))

matched, tagged = {}, set()
for path, keyed in ((REPORT, True),):
    with open(path, newline='') as fh:
        for row in csv.reader(fh):
            if len(row) < 4 or not row[1].startswith('0x'):
                continue
            va = int(row[1], 16)
            tagged.add(va)
            if row[3] == 'match':
                matched[va] = (row[2], row[0])

# every extracted original, keyed by exact size
bysize = {}
for f in glob.glob(os.path.join(ORIG, '0x*.bin')):
    va = int(os.path.basename(f)[:10], 16)
    b = open(f, 'rb').read()
    bysize.setdefault(len(b), []).append((va, b))

hits = []
for size, group in bysize.items():
    mine = [(va, b) for va, b in group if va in matched]
    if not mine:
        continue
    if size < minsize:
        continue
    for va, b in group:
        if va in tagged or va in fenced:
            continue
        best = None
        for mva, mb in mine:
            d = sum(1 for x, y in zip(b, mb) if x != y)
            if best is None or d < best[1]:
                best = (mva, d)
        if best and best[1] <= maxdiff:
            hits.append((best[1], va, size, best[0], matched[best[0]][0],
                         matched[best[0]][1]))

hits.sort()
print('%-6s %-12s %-7s %-12s %-28s %s' %
      ('diff', 'unmatched', 'bytes', 'twin', 'twin name', 'twin file'))
for d, va, size, mva, name, f in hits:
    print('%-6d 0x%08X %-7d 0x%08X %-28s %s' % (d, va, size, mva, name, f))
print('\n%d unmatched originals sit within %d bytes of a MATCHED same-size '
      'function.' % (len(hits), maxdiff))
