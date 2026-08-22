#!/usr/bin/env python3
"""Report what a matching-build .obj's relocations point AT, and whether we can
resolve each one to an original VA.

The per-function diff deliberately masks relocated operands, so a "match" today
means the instruction ENCODING is right while the addresses inside it are still
unresolved zeros.  Filling those slots is what turns a matched function into
bytes that can actually go in the binary -- and it needs, for every relocation:
its offset, its type, the symbol it names, and that symbol's address in the
original image.

This tool answers the last part: how much of that map do we currently have?
Run it before trusting any plan to emit a bit-exact image.

Usage:
    python3 tools/relocmap.py build/match/obj/br_vec.obj [more.obj ...]
"""
import csv
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# i386 relocation types we expect from VC5 /O2 code.
REL_DIR32 = 0x0006   # absolute 32-bit address of the symbol
REL_REL32 = 0x0014   # displacement relative to the end of the instruction
REL_NAMES = {REL_DIR32: 'DIR32', 0x0007: 'DIR32NB', REL_REL32: 'REL32'}


def parse_obj(path):
    """Return (symbols, relocs) where relocs is a list of dicts for .text."""
    data = open(path, 'rb').read()
    nsecs, symoff, nsyms = struct.unpack_from('<H', data, 2)[0], \
        struct.unpack_from('<I', data, 8)[0], \
        struct.unpack_from('<I', data, 12)[0]
    strtab = symoff + nsyms * 18

    def symname(idx):
        off = symoff + idx * 18
        raw = data[off:off + 8]
        if raw[:4] == b'\0\0\0\0':
            s = struct.unpack_from('<I', raw, 4)[0] + strtab
            end = data.index(b'\0', s)
            return data[s:end].decode('latin1')
        return raw.rstrip(b'\0').decode('latin1')

    relocs = []
    for i in range(nsecs):
        off = 20 + i * 40
        name = data[off:off + 8].rstrip(b'\0').decode('latin1')
        if not name.startswith('.text'):
            continue
        praw = struct.unpack_from('<I', data, off + 20)[0]
        roff = struct.unpack_from('<I', data, off + 24)[0]
        nrel = struct.unpack_from('<H', data, off + 32)[0]
        for r in range(nrel):
            e = roff + r * 10
            vaddr, symidx = struct.unpack_from('<II', data, e)
            rtype = struct.unpack_from('<H', data, e + 8)[0]
            relocs.append({'sec': name, 'off': vaddr, 'type': rtype,
                           'sym': symname(symidx), 'praw': praw})
    return relocs


def load_maps():
    """name -> VA, from every source the tree actually has."""
    fn, gl = {}, {}
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    if os.path.exists(rep):
        for r in csv.DictReader(open(rep)):
            if r.get('name') and r.get('va'):
                fn[r['name']] = int(r['va'], 16)
    g = os.path.join(ROOT, 'config', 'globals.csv')
    if os.path.exists(g):
        for r in csv.DictReader(open(g)):
            s = (r.get('symbol') or '').strip()
            if s:
                gl[s] = int(r['addr'], 16)
    return fn, gl


HEX = set('0123456789ABCDEFabcdef')


def normalize(sym):
    """Our global names encode the original address; globals.csv spells it
    `g_<HEX>`.  Recover that spelling where the name carries an address.

    Confirmed to agree with a match-by-address on every case tried, so the join
    is the naming convention drifting, not a guess about which global is meant.
    """
    s = sym.lstrip('_')
    for pre in ('BrG_', 'g_br', 'g_f', 'g_'):
        if s.startswith(pre):
            core = s[len(pre):]
            if core and all(c in HEX for c in core):
                return 'g_' + core.upper()
    return s


def classify(sym):
    """Why an unresolved symbol has no address, which decides what fixes it."""
    s = sym.lstrip('_')
    if '$S' in s or s.startswith('s_'):
        return 'file-static stand-in (no 1:1 original global)'
    if s.startswith('$T') or s.startswith('$SG'):
        return 'compiler constant (needs .rdata placement)'
    if s.startswith('__') or s in ('snprintf', 'sprintf', 'memcpy', 'memset',
                                   'strcpy', 'strlen', 'malloc', 'free'):
        return 'CRT / import'
    return 'named global, no address recorded'


def main():
    objs = sys.argv[1:] or sorted(glob.glob(
        os.path.join(ROOT, 'build', 'match', 'obj', '*.obj')))
    fnmap, glmap = load_maps()
    print(f"maps: {len(fnmap)} function names, {len(glmap)} global symbols\n")

    tot = 0
    hit_fn = hit_gl = hit_norm = miss = 0
    unresolved = {}
    buckets = {}
    bytype = {}
    for o in objs:
        for r in parse_obj(o):
            tot += 1
            k = REL_NAMES.get(r['type'], hex(r['type']))
            bytype[k] = bytype.get(k, 0) + 1
            s = r['sym'].lstrip('_')
            if s in fnmap:
                hit_fn += 1
            elif s in glmap:
                hit_gl += 1
            elif normalize(s) in glmap:
                hit_norm += 1
            else:
                miss += 1
                unresolved[s] = unresolved.get(s, 0) + 1
                b = classify(s)
                buckets[b] = buckets.get(b, 0) + 1

    print(f"relocations: {tot}")
    print(f"  by type: {bytype}")
    if not tot:
        return
    res = hit_fn + hit_gl + hit_norm
    print(f"\n  RESOLVABLE {res} ({100*res/tot:.1f}%)")
    print(f"    via function map (@implements): {hit_fn}")
    print(f"    via globals.csv, name as-is:    {hit_gl}")
    print(f"    via globals.csv after renaming: {hit_norm}")
    print(f"\n  UNRESOLVED {miss} ({100*miss/tot:.1f}%), by cause:")
    for b, n in sorted(buckets.items(), key=lambda kv: -kv[1]):
        print(f"    {n:5d}  {b}")
    print(f"\ndistinct unresolved symbols: {len(unresolved)}")
    for s, n in sorted(unresolved.items(), key=lambda kv: -kv[1])[:12]:
        print(f"   {n:4d}  {s}   [{classify(s)}]")


if __name__ == '__main__':
    main()
