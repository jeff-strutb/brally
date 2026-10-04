#!/usr/bin/env python3
"""Check the record specs against what the running game did.

build/portable/trace/objects.csv (trace_report.py) says, for every global
object and offset the game touched, the access width and whether the value
there was ever an address. Every global canonical object whose type is an
array of (or one) spec'd record is checked field by field:

  POINTER-MISSING  the game kept addresses at this offset, but the spec's
                   field there is not a pointer (on 64-bit it truncates)
  POINTER-UNSEEN   the spec says pointer, the game wrote non-address values
                   there (possible, e.g. a union -- read it)
  NO-FIELD         the game touched an offset that is padding in the spec
  WIDTH            the game's access width disagrees with the field's size

Also every global that is not a spec'd record but held an address at some
offset is listed: those are pointer slots (or tables of them) whose type
must be a pointer type.

Usage: catalog_check.py [RECORD...]
Output: build/portable/trace/catalog_check.txt
"""
import collections
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recspec  # noqa: E402
import rewrite  # noqa: E402
import stridefix  # noqa: E402

ROOT = recspec.ROOT


def is_ptr(t):
    t = t.strip()
    return t.endswith('*') or t.startswith('fnptr:')


def main():
    os.chdir(ROOT)
    only = set(sys.argv[1:])
    out = []
    per = collections.defaultdict(dict)
    nonrec = collections.defaultdict(list)
    for r in csv.DictReader(open('build/portable/trace/objects.csv')):
        m = re.match(r'^(\w+)<(\w+)>$', r['object'])
        o, w = int(r['offset'], 16), int(r['width'])
        if m:
            rec = m.group(2)
            if only and rec not in only:
                continue
            e = per[rec].setdefault((o, w), {'held': set(), 'sites': set(), 'objs': set()})
            e['held'].add(r['holds_address'])
            e['sites'].update(r['sites'].split())
            e['objs'].add(m.group(1))
        elif r['holds_address'] == 'yes' and w == 4 and not r['object'].startswith(('heap@', '0x', 'stack', 'portdata')):
            nonrec[r['object']].append(o)
    for rec in sorted(per):
        for (o, w), e in sorted(per[rec].items()):
            path, fty = rewrite.resolve(rec, o, 'char')
            sites = ' '.join(sorted(e['sites'])[:4])
            if path is None:
                out.append('%-15s %s+0x%04X w%d held=%s  %s' % ('NO-FIELD', rec, o, w, '/'.join(sorted(e['held'])) or '-', sites))
            elif 'yes' in e['held'] and w == 4 and not is_ptr(fty):
                out.append('%-15s %s+0x%04X %s (%s)  %s' % ('POINTER-MISSING', rec, o, path, fty, sites))
            elif is_ptr(fty) and e['held'] == {''} and w == 4:
                out.append('%-15s %s+0x%04X %s (%s)  %s' % ('POINTER-UNSEEN', rec, o, path, fty, sites))
    for ob, offs in sorted(nonrec.items()):
        out.append('%-15s %s  offsets %s' % ('GLOBAL-PTRS', ob, ' '.join('0x%X' % o for o in sorted(set(offs))[:12])))
    os.makedirs('build/portable/trace', exist_ok=True)
    open('build/portable/trace/catalog_check.txt', 'w').write('\n'.join(out) + '\n')
    c = collections.Counter(l.split()[0] for l in out)
    print('catalog check: %s' % dict(c))


if __name__ == '__main__':
    main()
