#!/usr/bin/env python3
"""Resolve a matched function's relocations and check the result against the
original image byte for byte -- addresses included.

WHY THIS EXISTS.  The per-function diff masks relocated operands, so today's
"match" means the instruction ENCODING agrees while every address inside the
function is still an unresolved zero.  That is the right first milestone -- it
proves the C is the C the compiler saw -- but it is NOT the claim that these
bytes can go in the binary.  Patching masked bytes into the DLL corrupts it:
running pe_patch.py on the current staging set produces an image differing from
the original in 858 places, every one a relocation slot.

This tool closes that gap for the functions it can.  For each relocation it
looks the symbol up in the tree's own maps (@implements for functions,
config/globals.csv for globals), writes the address the linker would have
written, and compares.  A function that survives this is bit-exact in the
strong sense: right instructions AND right addresses.

Usage:
    python3 tools/reloc_fill.py [file.obj ...]     # default: all staged objs
"""
import csv
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from relocmap import (normalize, classify, load_learned,  # noqa: E402
                      load_learned_full, load_maps, REL_DIR32, REL_REL32)

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
LEARNED = {}


def parse(path):
    """Return (sections, symbols, relocs-by-section)."""
    d = open(path, 'rb').read()
    nsec = struct.unpack_from('<H', d, 2)[0]
    symoff = struct.unpack_from('<I', d, 8)[0]
    nsym = struct.unpack_from('<I', d, 12)[0]
    strtab = symoff + nsym * 18

    def nm(raw):
        if raw[:4] == b'\0\0\0\0':
            s = struct.unpack_from('<I', raw, 4)[0] + strtab
            return d[s:d.index(b'\0', s)].decode('latin1')
        return raw.rstrip(b'\0').decode('latin1')

    secs = {}
    relocs = {}
    for i in range(nsec):
        o = 20 + i * 40
        name = d[o:o + 8].rstrip(b'\0').decode('latin1')
        praw = struct.unpack_from('<I', d, o + 20)[0]
        size = struct.unpack_from('<I', d, o + 16)[0]
        roff = struct.unpack_from('<I', d, o + 24)[0]
        nrel = struct.unpack_from('<H', d, o + 32)[0]
        secs[i + 1] = {'name': name, 'praw': praw, 'size': size}
        rl = []
        for r in range(nrel):
            e = roff + r * 10
            va, si = struct.unpack_from('<II', d, e)
            rt = struct.unpack_from('<H', d, e + 8)[0]
            rl.append((va, si, rt))
        relocs[i + 1] = rl

    syms = []
    i = 0
    while i < nsym:
        o = symoff + i * 18
        name = nm(d[o:o + 8])
        val = struct.unpack_from('<I', d, o + 8)[0]
        sec = struct.unpack_from('<h', d, o + 12)[0]
        naux = d[o + 17]
        syms.append({'name': name, 'val': val, 'sec': sec, 'idx': i})
        i += 1 + naux
    return d, secs, syms, relocs


# load_maps lives in relocmap.py and is re-exported here, NOT reimplemented.
# It used to exist in both files. When the tree switched to Glide-canonical
# addressing only relocmap's copy was repointed at the Glide-space globals,
# this one kept reading the D3D file, and reloc_learn.py imports THIS one --
# so the learner compared Glide image addresses against D3D map addresses and
# reported 87 contradictions with a constant offset per data region. Two
# loaders for one fact is the bug; there is now one.


_LEARNED = None


def resolve(sym, fnmap, glmap, learned=True):
    """Address for a symbol, preferring maps the tree surveyed over addresses
    read back out of the image.

    `learned=False` is what reloc_learn.py's validation pass uses: asking
    whether a learned address reproduces a KNOWN one has to consult only the
    known maps, or the learned value would be checked against itself.
    """
    global _LEARNED
    s = sym.lstrip('_')
    if s in fnmap:
        return fnmap[s]
    if s in glmap:
        return glmap[s]
    n = normalize(s)
    if n in glmap:
        return glmap[n]
    if not learned:
        return None
    if _LEARNED is None:
        _LEARNED = load_learned()
    return _LEARNED.get(s)


def main():
    # Default to every live variant obj, NOT build/match/obj. That directory
    # is a hand-staged set of 10; defaulting to it is what made this tool
    # report 22 confirmed functions when the answer over the real tree was 67.
    from reloc_learn import live_objs
    objs = sys.argv[1:] or live_objs()[0]
    fnmap, glmap = load_maps()
    global LEARNED
    LEARNED = load_learned_full()

    strong = []      # bit-exact including addresses
    encoding_only = []   # all relocs resolved but bytes still differ
    blocked = []     # at least one reloc unresolvable
    nomatch = 0

    for path in objs:
        try:
            d, secs, syms, relocs = parse(path)
        except Exception:
            continue
        # function symbols living in a .text section
        for sy in syms:
            if sy['sec'] <= 0 or sy['sec'] not in secs:
                continue
            if not secs[sy['sec']]['name'].startswith('.text'):
                continue
            name = sy['name'].lstrip('_')
            if name not in fnmap:
                continue
            va = fnmap[name]
            ob = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
            if not os.path.exists(ob):
                continue
            orig = open(ob, 'rb').read()
            sec = secs[sy['sec']]
            start = sec['praw'] + sy['val']
            code = bytearray(d[start:start + len(orig)])
            if len(code) != len(orig):
                nomatch += 1
                continue

            # fill this function's relocations
            unres = []
            self_taught = []   # learned addresses this function itself taught
            for rva, si, rt in relocs[sy['sec']]:
                off = rva - sy['val']
                if not (0 <= off < len(orig) - 3):
                    continue
                target = resolve(syms[si]['name'] if si < len(syms) else '',
                                 fnmap, glmap) if si < len(syms) else None
                # symbol index is into the raw table; find by idx
                tsym = next((s for s in syms if s['idx'] == si), None)
                target = resolve(tsym['name'], fnmap, glmap) if tsym else None
                if target is None:
                    unres.append(tsym['name'] if tsym else '?')
                    continue
                # Was this address read out of THIS function?  If so, matching
                # here proves nothing -- the byte was copied from the original
                # and then compared against the original.
                ls = LEARNED.get(tsym['name'].lstrip('_'))
                if ls and resolve(tsym['name'], fnmap, glmap,
                                  learned=False) is None:
                    if ls[1] and ls[1] <= {va}:
                        self_taught.append(tsym['name'])
                addend = struct.unpack_from('<i', code, off)[0]
                if rt == REL_DIR32:
                    val = target + addend
                elif rt == REL_REL32:
                    val = target + addend - (va + off + 4)
                else:
                    unres.append('reloctype:%#x' % rt)
                    continue
                struct.pack_into('<i', code, off, val & 0xFFFFFFFF
                                 if val >= 0 else val)
            if unres:
                blocked.append((name, hex(va), len(unres)))
            elif bytes(code) == orig:
                strong.append((name, hex(va), len(orig),
                               len(set(self_taught))))
            else:
                nd = sum(1 for i in range(len(orig)) if code[i] != orig[i])
                encoding_only.append((name, hex(va), nd, len(orig)))

    # One row per ADDRESS: a function extracted into a named module still has a
    # copy in its old slice file, so the same VA turns up in two objs.
    us, ue, ub = {}, {}, {}
    for n, v, sz, st in strong:
        us[v] = (n, sz, st)
    for n, v, nd, sz in encoding_only:
        if v not in us:
            ue[v] = (n, nd, sz)
    for n, v, k in blocked:
        if v not in us and v not in ue:
            ub[v] = (n, k)

    # How many of the encoding-level matches survive address resolution?
    claimed = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'build', 'match',
                                              'report.csv'))):
        if r.get('va') and r.get('status'):
            claimed[r['va'].lower()] = r['status']
    m_strong = sum(1 for v in us if claimed.get(v) == 'match')
    m_total = sum(1 for v in claimed.values() if v == 'match')

    # A function scored on an address only IT taught us is proving nothing.
    circular = {v for v, (_, _, st) in us.items() if st}
    indep = len(us) - len(circular)

    print(f"distinct addresses checked: {len(us) + len(ue) + len(ub)}\n")
    print(f"  BIT-EXACT incl. addresses         : {len(us)}")
    print(f"    of which independently checked  : {indep}")
    print(f"    scored on a self-taught address : {len(circular)}  "
          f"(circular -- not evidence)")
    print(f"  relocs resolved, bytes still differ: {len(ue)}")
    print(f"  blocked on an unresolvable symbol  : {len(ub)}")
    print(f"\n  of the {m_total} functions the report calls 'match' "
          f"(encoding only), {m_strong} are confirmed bit-exact WITH addresses.")
    m_indep = sum(1 for v in us
                  if claimed.get(v) == 'match' and v not in circular)
    print(f"  discounting circular cases, {m_indep} stand on evidence.")
    if us:
        print("\nbit-exact including every address:")
        for v, (n, sz, st) in sorted(us.items(), key=lambda kv: -kv[1][1])[:20]:
            flag = '  [self-taught]' if st else ''
            print(f"   {sz:5d}b  {v}  {n}{flag}")


if __name__ == '__main__':
    main()
