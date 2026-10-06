#!/usr/bin/env python3
"""Which raw-offset accesses are wrong on a 64-bit build.

A raw access (`*(T *)(base + K)`, build/brally/null-soft/raw.csv from rawscan.py)
is correct on LP64 exactly when the object it reaches keeps its 32-bit
byte layout there: a file image, a packet, a pixel buffer -- anything that
never holds an address. The trace (build/brally/null-soft/trace/, trace_report.py)
says, for every access site of the running 32-bit game, which objects it
touched and whether each object ever held an address at any offset.

Each raw access line in a ports/brally core file is mapped to the line of
the original source (src/brally/core/..., what the traced build compiled) by a
diff of the two files, then classified:

  TYPE      a touched object held an address somewhere: its 64-bit layout
            differs, the access must name a field
  GLOBAL    a touched global whose 64-bit type holds a pointer (layout
            differs even if the trace saw no address there)
  SAFE      every touched object is pointer-free: the byte offset holds
  UNTRACED  the trace never ran this line (or it could not be mapped)
A trailing ~ marks a line inside a block the port rewrote, matched to the
original by position: confirm by reading both.

Usage: rawclass.py
Output: build/brally/null-soft/rawclass.csv, summary on stdout
"""
import collections
import csv
import difflib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import imagemap  # noqa: E402
import recspec  # noqa: E402

ROOT = recspec.ROOT


def line_map(port, orig):
    """port line number (1-based) -> (original line numbers, exact?). Equal
    lines map exactly; a line inside a changed block maps to the original
    block's lines around its proportional position."""
    a = open(port, encoding='latin-1').read().split('\n')
    b = open(orig, encoding='latin-1').read().split('\n')
    m = {}
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal' or (tag == 'replace' and i2 - i1 == j2 - j1):
            for k in range(i2 - i1):
                m[i1 + k + 1] = ([j1 + k + 1], tag == 'equal')
        elif tag == 'replace':
            for k in range(i2 - i1):
                c = j1 + (k * (j2 - j1)) // (i2 - i1)
                m[i1 + k + 1] = (list(range(max(j1, c - 2), min(j2, c + 3))), False)
    return m


def main():
    os.chdir(ROOT)
    held = set()
    for r in csv.DictReader(open('build/brally/null-soft/trace/objects.csv')):
        if r['holds_address'] == 'yes':
            held.add(re.sub(r'<\w+>$', '', r['object']))
    canon = {}
    for r in csv.DictReader(open('build/brally/null-soft/canon.csv')):
        canon[r['name']] = r['type']
    gptr = {}

    def global_has_ptr(nm):
        if nm not in gptr:
            ty = canon.get(nm)
            if ty is None:
                gptr[nm] = None
            else:
                sz = imagemap.size_of(ty)
                po = imagemap.pointer_offsets(ty, sz or 0)
                gptr[nm] = None if po is None else bool(po)
        return gptr[nm]

    by_line = collections.defaultdict(set)     # (src file, line) -> touched objects
    for r in csv.DictReader(open('build/brally/null-soft/trace/sites.csv')):
        m = re.match(r'^(.*?):(\d+):', r['loc'])
        if not m:
            continue
        for t in r['touches'].split():
            t = t.replace('OVERRUN:', '')
            by_line[(m.group(1), int(m.group(2)))].add(re.sub(r'\+0x[0-9A-Fa-f]+$', '', t))
    maps = {}
    out = []
    for r in csv.DictReader(open('build/brally/null-soft/raw.csv')):
        f = r['file']
        rel = f[len('ports/brally/'):]
        cls, why = 'UNTRACED', ''
        if r['line'] and os.path.exists(rel):
            if f not in maps:
                maps[f] = line_map(f, rel)
            ols, exact = maps[f].get(int(r['line']), ([], True))
            objs = set()
            for ol in ols:
                objs |= by_line.get((rel, ol), set())
            if objs:
                hp = sorted(o for o in objs if o in held)
                gp = sorted(o for o in objs if not o.startswith(('heap@', '0x', 'stack')) and global_has_ptr(o))
                if hp:
                    cls, why = 'TYPE', ' '.join(hp[:4])
                elif gp:
                    cls, why = 'GLOBAL', ' '.join(gp[:4])
                else:
                    cls, why = 'SAFE', ' '.join(sorted(objs)[:4])
                if not exact:
                    cls += '~'
        out.append(dict(r, cls=cls, why=why))
    keys = list(out[0].keys()) if out else []
    with open('build/brally/null-soft/rawclass.csv', 'w', newline='') as fh:
        w = csv.DictWriter(fh, keys)
        w.writeheader()
        w.writerows(out)
    c = collections.Counter(o['cls'] for o in out)
    print('raw accesses: %s' % dict(c))
    per = collections.Counter(o['file'].split('core/')[1] for o in out if o['cls'].rstrip('~') in ('TYPE', 'GLOBAL'))
    for k, v in per.most_common(25):
        print('  %4d %s' % (v, k))


if __name__ == '__main__':
    main()
