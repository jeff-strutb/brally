"""Screen for d3d claims scored against a Glide body they may not share.

    python3 tools/screen_slotpairs.py

Needs the capstone venv (.venv/bin/python3) only for consistency with the other
screens; it reads CSVs alone.

A `d3d`-tagged body is scored against the GLIDE bytes its address maps to
through config/shared.csv.  When that mapping was made by `slot` -- the two
functions merely occupy the same dispatch slot -- the pairing is the weakest
evidence the pairing tool produces, and several of those rows carry the note
"DIFFERENT CODE" in so many words.  Scoring a body against bytes it shares no
code with produces a large permanent diff for a function that may already be
finished under its Glide name, and the lane ranking then offers it as the best
available target.  Two such rows sat at the top of the ranking on 2026-09-03:

  0x1001BE90 -> glide 0x1001E380  (already byte-exact as BrGlRectFill)
  0x10020FA0 -> glide 0x10021270  (already byte-exact as BrGlGbiCall)

Both are now @d3donly, which is the convention this tree already uses.

EVERY HIT NEEDS EYES.  `matched_by == slot` is a weak pairing, not proof of
difference: 0x1001BAE0 opens with the same two function-pointer stores as its
Glide row and may well be the same routine with callees inlined, which is the
MISSING CODE class and wants transcribing, not relabelling.  Check whether the
Glide address is already matched under another name (that settles it), and
otherwise read both bodies before touching a tag.
"""
import csv, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sh = {}
for r in csv.DictReader(open(os.path.join(ROOT, 'config/shared.csv'))):
    g = (r.get('glide_va') or '').strip().lower()
    if g:
        sh.setdefault(g, []).append(r)

# Anything already byte-exact anywhere is the strongest possible signal that a
# still-diffing row at the same address is a duplicate claim, not open work.
matched = {}
for name in ('report.csv', 'report_cpp.csv', 'report_exe.csv'):
    fp = os.path.join(ROOT, 'build/match', name)
    if not os.path.exists(fp):
        continue
    for r in csv.DictReader(open(fp)):
        va = (r.get('va') or '').strip().lower()
        if va and r.get('status') == 'match':
            matched[va] = '%s:%s' % (r.get('file'), r.get('name'))

out = []
for r in csv.DictReader(open(os.path.join(ROOT, 'build/match/report.csv'))):
    if r.get('status') != 'diff':
        continue
    va = (r.get('va') or '').lower()
    for e in sh.get(va, []):
        note = e.get('note') or ''
        if 'DIFFERENT CODE' in note or (e.get('matched_by') or '') == 'slot':
            out.append((va, r['name'], r['file'], e['d3d_va'],
                        matched.get(va, ''), note[:46]))

print('%d diff rows paired to a Glide body by SLOT only\n' % len(out))
print('%-12s %-28s %-30s %-11s %s' % ('glide va', 'name', 'file', 'd3d va',
                                      'already matched as / note'))
for va, name, f, d3d, m, note in out:
    print('%-12s %-28s %-30s %-11s %s' % (va, name, f, d3d, m or note))
print('\nEvery hit needs eyes -- see this file\'s docstring.')
