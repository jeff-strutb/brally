#!/usr/bin/env python3
"""Add what the running game touched to the record specs.

For each spec'd record, catalog_check.py's NO-FIELD rows are offsets the
game read or wrote that the spec leaves as padding. Each becomes a field of
the measured width -- a pointer when the game kept addresses there, else an
integer of that width (8 bytes: int64_t) -- named fXXXX. Offsets inside a
wider measured access are folded into it; accesses that straddle an
existing field are listed for a person instead.

Usage: spec_from_trace.py [--dry] RECORD...
"""
import collections
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recspec  # noqa: E402
import rewrite  # noqa: E402

ROOT = recspec.ROOT
TYPE = {1: 'uint8_t', 2: 'uint16_t', 4: 'int32_t', 8: 'uint32_t[2]'}   # 8: no 8-byte alignment


def main():
    os.chdir(ROOT)
    recs = [a for a in sys.argv[1:] if not a.startswith('--')]
    dry = '--dry' in sys.argv
    meas = collections.defaultdict(dict)       # rec -> off -> (width, held)
    for r in csv.DictReader(open('build/portable/trace/objects.csv')):
        m = re.match(r'^(\w+)<(\w+)>$', r['object'])
        if not m or (recs and m.group(2) not in recs):
            continue
        o, w = int(r['offset'], 16), int(r['width'])
        cur = meas[m.group(2)].get(o)
        held = r['holds_address'] == 'yes'
        if cur is None or w > cur[0]:
            meas[m.group(2)][o] = (w, held or (cur[1] if cur else False))
        elif w == cur[0]:
            meas[m.group(2)][o] = (w, held or cur[1])
    for rec, offs in sorted(meas.items()):
        meta, fields = recspec.load(rec)
        hdrs = meta.get('headers', '').split()
        size = int(meta['size'], 16)
        occupied = []
        for f in fields:
            occupied.append((f.off, f.off + recspec.size32(f.ty, hdrs)))
        new, person = [], []
        covered_to = -1
        for o in sorted(offs):
            w, held = offs[o]
            if o < covered_to:
                continue
            if rewrite.resolve(rec, o, 'char')[0] is not None:
                continue
            end = o + w
            clash = [a for a in occupied if a[0] < end and o < a[1]]
            if clash or end > size:
                person.append('%s+0x%04X w%d overlaps a field or the end' % (rec, o, w))
                continue
            ty = 'void *' if held and w == 4 else TYPE.get(w, 'uint8_t[%d]' % w)
            new.append((o, ty))
            occupied.append((o, end))
            covered_to = end
        if not new:
            continue
        print('%s: %d field(s) from the trace' % (rec, len(new)))
        for o, ty in new:
            print('  0x%04X %s' % (o, ty))
        for p in person:
            print('  PERSON %s' % p)
        if dry:
            continue
        path = recspec.spec_path(rec)
        lines = open(path).read().rstrip('\n').split('\n')
        for o, ty in new:
            lines.append('0x%04X  %-29s f%04X               seen in the trace' % (o, ty, o))
        head = [l for l in lines if l.startswith('#')]
        body = sorted((l for l in lines if l.startswith('0x')), key=lambda l: int(l.split()[0], 16))
        open(path, 'w').write('\n'.join(head + body) + '\n')


if __name__ == '__main__':
    main()
