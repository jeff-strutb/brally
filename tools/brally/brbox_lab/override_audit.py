#!/usr/bin/env python3
"""override_audit.py -- config/brally/reloc_overrides.csv binds relocation SITES to
addresses by hand.  A site whose compiled symbol differs from what the row
assumed silently binds the wrong variable (the BrCarDrawVehicle rain bug).
For each row, find the symbol our graded object references at that site and
check that every symbol binds to exactly ONE address -- within a function and,
for external (named) symbols, across the whole table.
"""
import collections, csv, glob, os, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'brally'))
from reloc_fill import parse

rep = {}
for f in ('report.csv', 'report_cpp.csv'):
    p = os.path.join(ROOT, 'build', 'brally', 'win32', 'match', f)
    if os.path.exists(p):
        for r in csv.DictReader(open(p)):
            if r.get('va'):
                rep.setdefault(int(r['va'], 16), r)

def obj_for(r):
    src = r.get('file') or r.get('src') or ''
    base = os.path.splitext(os.path.basename(src))[0]
    for d in ('obj_' + r.get('opt', 'O2'), 'obj_O2', 'obj_cpp_' + r.get('opt', 'O2')):
        p = os.path.join(ROOT, 'build', 'brally', 'win32', 'match', d, base + '.obj')
        if os.path.exists(p):
            return p
    hits = glob.glob(os.path.join(ROOT, 'build', 'brally', 'win32', 'match', '*', base + '.obj'))
    return hits[0] if hits else None

rows = [r for r in csv.reader(open(os.path.join(ROOT, 'config', 'brally', 'reloc_overrides.csv'), newline='')) if r and r[0].startswith('0x')]
byfn = collections.defaultdict(list)
for r in rows:
    byfn[int(r[0], 16)].append((int(r[1], 16), int(r[2], 16), r[3] if len(r) > 3 else ''))

cache = {}
local = collections.defaultdict(set); where = collections.defaultdict(list)
glob_ = collections.defaultdict(set); gwhere = collections.defaultdict(list)
missing = []
for va, lst in byfn.items():
    r = rep.get(va)
    ob = obj_for(r) if r else None
    if not ob:
        missing.append(va); continue
    if ob not in cache:
        cache[ob] = parse(ob)
    d, secs, syms, relocs = cache[ob]
    by = {s['idx']: s for s in syms}
    name = r.get('name', '')
    fsym = next((s for s in syms if s['sec'] > 0 and secs[s['sec']]['name'].startswith('.text')
                 and (s['name'].lstrip('_@').split('@')[0] == name or s['name'] == r.get('symbol'))), None)
    if not fsym:
        missing.append(va); continue
    import struct
    sec = secs[fsym['sec']]
    site = {}
    for rva, si, rt in relocs[fsym['sec']]:
        if si in by:
            add = struct.unpack_from('<i', d, sec['praw'] + rva)[0]
            site[rva - fsym['val']] = (by[si]['name'], rt, add)
    for off, addr, note in lst:
        e = site.get(off)
        if e is None:
            local[(va, '<no reloc at +%#x>' % off)].add(addr); continue
        s, rt, add = e
        if rt == 20:     # REL32: stored value is the displacement
            disp = addr - (1 << 32) if addr & 0x80000000 else addr
            addr = (va + off + 4 + disp) & 0xffffffff
        else:
            addr = (addr - add) & 0xffffffff
        if s.startswith('$') or s.startswith('??_C'):
            continue                        # per-object literal: one per site is fine
        local[(va, s)].add(addr); where[(va, s)].append((off, addr, note[:60]))
        glob_[s].add(addr); gwhere[s].append((va, off, addr))

bad = 0
for (va, s), v in sorted(local.items()):
    if len(v) > 1 or s.startswith('<no reloc'):
        bad += 1
        print('FN  %08X %-24s -> %s' % (va, s, ' '.join('%08X' % a for a in sorted(v))))
        for off, addr, note in where.get((va, s), []):
            print('        +%#x = %08X  %s' % (off, addr, note))
for s, v in sorted(glob_.items()):
    fns = {va for va, _, _ in gwhere[s]}
    if len(v) > 1 and len(fns) > 1 and not any(len(local[(va, s)]) > 1 for va in fns):
        bad += 1
        print('XFN %-28s -> %s' % (s, ' | '.join('%08X<-%s' % (a, ','.join('%08X+%x' % (f, o) for f, o, aa in gwhere[s] if aa == a)[:60]) for a in sorted(v))))
print('override rows: %d  functions: %d  objects not found: %d  CONFLICTS: %d' % (len(rows), len(byfn), len(missing), bad))
if missing:
    print('  no graded object for:', ' '.join('%08X' % v for v in missing[:20]))
