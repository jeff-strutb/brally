#!/usr/bin/env python3
"""ovlcheck.py PLACE OBJ... -- check gba/place.txt against the call graph: a function in an
overlay may be reached only from its own phase's entry (and what it calls, through calls the
objects' relocations show), never from another phase or from code outside the phases.

  ENTRY lines in PLACE ("# entry OVERLAY FUNCTION ...") name each overlay's entry points;
  "# calls CALLER CALLEE" adds an edge the relocations cannot show (a call through a pointer);
  "# dispatch FUNCTION" names code that calls the entries only with their overlay in;
  "# ok CALLER CALLEE" a reference that is not a call (a pointer taken).
Prints the functions each phase reaches that sit in the cartridge, hottest first by size."""
import struct
import sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
from gbalink import Obj, SHT_REL


def main():
    place_p, objs = sys.argv[1], [Obj(p) for p in sys.argv[2:]]
    place, entry, extra, dispatch, ok = {}, {}, [], set(), set()
    for line in open(place_p):
        f = line.split()
        if line.startswith('# entry'):
            entry.setdefault(f[2], []).extend(f[3:])
        elif line.startswith('# calls'):
            extra.append((f[2], f[3]))
        elif line.startswith('# dispatch'):
            dispatch.update(f[2:])
        elif line.startswith('# ok'):
            ok.add((f[2], f[3]))
        elif f and not f[0].startswith('#') and len(f) >= 2:
            place[f[0]] = f[1]
    calls, size = {}, {}
    for o in objs:
        for i, sh in enumerate(o.sh):
            n = o.names[i]
            if n.startswith('.text.'):
                size[n[6:]] = sh[5]
            if sh[1] != SHT_REL or not o.names[sh[7]].startswith('.text.'):
                continue
            fn = o.names[sh[7]][6:]
            for k in range(sh[5] // 8):
                off, info = struct.unpack_from('<II', o.d, sh[4] + k * 8)
                if info & 0xFF not in (1, 28, 29, 2):
                    continue
                name, value, sz, sinfo, shndx = o.syms[info >> 8]
                if not name and shndx < 0xFF00 and shndx:      # a section symbol: the section's function
                    name = o.names[shndx][6:] if o.names[shndx].startswith('.text.') else ''
                if name:
                    calls.setdefault(fn, set()).add(name)
    for a, b in extra:
        calls.setdefault(a, set()).add(b)

    def reach(starts):
        seen, todo = set(), list(starts)
        while todo:
            f = todo.pop()
            if f in seen:
                continue
            seen.add(f)
            todo += calls.get(f, ())
        return seen
    bad = 0
    phases = {ov: reach(fs) for ov, fs in entry.items()}
    for ov, seen in phases.items():
        for f in seen:
            p = place.get(f)
            if p and p != 'iwram' and p != ov:
                print('%s: reaches %s, which is in %s' % (ov, f, p))
                bad += 1
    for f, p in place.items():                      # callers outside any phase
        if p == 'iwram':
            continue
        for g, cs in calls.items():
            if g in dispatch or (g, f) in ok:
                continue
            if f in cs and place.get(g) != p and not any(g in seen for ov, seen in phases.items() if ov == p):
                print('%s (in %s) is called from %s (%s)' % (f, p, g, place.get(g, 'the cartridge')))
                bad += 1
    for ov, seen in sorted(phases.items()):
        rom = sorted((size.get(f, 0), f) for f in seen if f not in place and f in size)
        print('%s: %d B placed; in the cartridge: %s' % (ov, sum(size.get(f, 0) for f in seen if place.get(f) == ov),
              ' '.join('%s %d' % (f, s) for s, f in reversed(rom[-12:]))))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
