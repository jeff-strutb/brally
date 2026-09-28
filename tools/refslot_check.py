#!/usr/bin/env python3
"""refslot_check.py -- the relocation slots the image gate cannot name.

tools/image_build.py fills a byte-exact function's relocation slot from the
ORIGINAL's own dword when no map names the target (a per-file static, a string
literal, a jump-table label, an external no map knows).  "0 differing bytes"
is then true by construction for that slot, so on its own it proves nothing
about where the source points.  Measured 2026-09-28: 64 T4 functions matched
only through such slots -- wrong string contents, a float constant one ULP
off, data-table bytes counted as code, one symbol standing for up to thirty
different original targets.

This check is what makes those slots count.  For every slot the gate filled
from the original, it derives what OUR object says the target is and checks
it against what the original's dword points at:

  SAME-TEXT   a label in the function's own .text: the original's target must
              be the same offset into the function
  CONTENT     initialised data (literals, statics, other .text): our bytes at
              the target, relocation fields masked, must equal the original's
              bytes there (strings to their NUL, bare constants at their width)
  BASE        every slot that names the same symbol (per object for statics,
              globally for externals) must decode to ONE original base address
              -- a symbol that reaches two different variables is wrong at
              one of its uses
  BSS         uninitialised targets have no content; BASE consistency only

check() returns a list of findings; image_build.py fails the gate on any.
"""
import collections
import os
import struct


def _relocs_of(d, si):
    o = 20 + (si - 1) * 40
    roff = struct.unpack_from('<I', d, o + 24)[0]
    nrel = struct.unpack_from('<H', d, o + 32)[0]
    return [struct.unpack_from('<IIH', d, roff + r * 10) for r in range(nrel)]


class _Image(object):
    def __init__(self, path):
        b = open(path, 'rb').read()
        pe = struct.unpack_from('<I', b, 0x3c)[0]
        n = struct.unpack_from('<H', b, pe + 6)[0]
        oh = struct.unpack_from('<H', b, pe + 20)[0]
        self.base = struct.unpack_from('<I', b, pe + 52)[0]
        self.secs = [struct.unpack_from('<8sIIII', b, pe + 24 + oh + 40 * k) for k in range(n)]
        self.b = b

    def at(self, va, n):
        """bytes at va (clipped to raw data), None for bss, 'OUT' outside."""
        for _nm, vs, v, rs, ro in self.secs:
            if self.base + v <= va < self.base + v + vs:
                o = va - self.base - v
                if o >= rs:
                    return None
                return self.b[ro + o: ro + min(o + n, rs)]
        return 'OUT'


def check(refslots, yielded, best, orig_path, origdir, rel32):
    """refslots: (path, fsym, tsym, off, rtype, va, plen, addend4, secs, syms, objbytes)
    yielded: (path, va, placed_bytes); best: {va: (name, code, ...)}."""
    img = _Image(orig_path)
    chosen = collections.defaultdict(set)
    for path, va, code in yielded:
        if va in best and best[va][1] == code:
            chosen[va].add(path)
    bad = []
    seen = set()
    bases = collections.defaultdict(lambda: collections.defaultdict(list))
    for (path, sy, t, off, rt, va, plen, a4, secs, syms, d) in refslots:
        if path not in chosen.get(va, ()) or (va, off) in seen:
            continue
        seen.add((va, off))
        ob = open(os.path.join(origdir, '0x%08X.bin' % va), 'rb').read()
        o_dw = struct.unpack_from('<I', ob, plen + off)[0]
        add = struct.unpack_from('<i', a4, 0)[0]
        if rt == rel32:
            tgt = (va + plen + off + 4 + struct.unpack('<i', struct.pack('<I', o_dw))[0]) & 0xffffffff
        else:
            tgt = o_dw
        where = '%08X+%#x %s' % (va, off, t['name'] if t else '?')
        if t is None:
            bad.append((where, 'relocation with no target symbol'))
            continue
        tsec = secs.get(t['sec'])
        if t['sec'] == sy['sec']:
            want = va + plen + (t['val'] + (0 if rt == rel32 else add) - sy['val'])
            if tgt != want & 0xffffffff:
                bad.append((where, 'label in own .text: original targets %08X, ours %08X' % (tgt, want)))
            continue
        if tsec is None or t['sec'] <= 0:
            base = (tgt - add) & 0xffffffff if rt != rel32 else tgt
            bases[('ext', t['name'])][base].append(where)
            continue
        toff = t['val'] + (0 if rt == rel32 else add)
        sbase = (tgt - (toff - t['val'])) & 0xffffffff
        bases[('static', path, t['name'])][sbase].append(where)
        if tsec['flags'] & 0x80:
            continue                                   # bss: consistency only
        nxt = sorted(s['val'] for s in syms if s['sec'] == t['sec'] and s['val'] > t['val'])
        end = min(nxt[0] if nxt else tsec['size'], t['val'] + 256, tsec['size'])
        ours = d[tsec['praw'] + toff: tsec['praw'] + max(toff, end)]
        if t['name'].startswith('$SG') or t['name'].startswith('??_C'):
            z = ours.find(b'\0')
            ours = ours[:z + 1] if z >= 0 else ours
        elif len(ours) > 4 and not any(ours[4:]):
            ours = ours[:4]
        elif len(ours) > 8 and not any(ours[8:]):
            ours = ours[:8]
        theirs = img.at(tgt, len(ours))
        if theirs == 'OUT':
            bad.append((where, 'original dword %08X is outside the image' % tgt))
            continue
        if theirs is None:
            continue       # ours initialised, the original's variable is bss: BASE decides
        mask = set()
        for rv, _si, _rt in _relocs_of(d, t['sec']):
            mask.update(range(rv - toff, rv - toff + 4))
        if len(theirs) < len(ours) or any(i not in mask and ours[i] != theirs[i] for i in range(len(ours))):
            bad.append((where, 'content differs at %08X: ours %s, original %s'
                        % (tgt, ours[:16].hex(), theirs[:16].hex())))
    for key, m in bases.items():
        if len(m) > 1:
            bad.append((key[-1], 'one symbol reaches %d original addresses: %s' % (
                len(m), ' | '.join('%08X<-%s' % (b, ','.join(w.split()[0] for w in ws[:3]))
                                   for b, ws in sorted(m.items())))))
    return len(seen), bad
