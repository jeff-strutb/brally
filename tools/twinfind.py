#!/usr/bin/env python3
"""Find a D3D function's Glide twin by reloc-masked structural search.

Exact byte search fails on any function containing a call or a global
reference, because the linker wrote different addresses into the two builds.
A real twin is identical once those slots are masked AND carries its
relocations at the same relative offsets. This uses both DLLs' own base
relocation tables to test exactly that, so a hit is structural, not a
coincidence of opcodes.

Usage:
    python3 tools/twinfind.py 0x1007C8A0 39      # one d3d VA + size
    python3 tools/twinfind.py --batch            # the untwinned @implements set
"""
import csv
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from pe_patch import read_pe_text_info  # noqa: E402


def relocs_in_text(path):
    """Set of RVAs that the base-reloc table patches (HIGHLOW only)."""
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    opt = pe + 24
    rva, size = struct.unpack_from('<II', d, opt + 96 + 5 * 8)
    base, secs = read_pe_text_info(path)

    def r2o(r):
        for nm, srva, vs, ro, rs in secs:
            if srva <= r < srva + max(vs, rs):
                return ro + (r - srva)
        return None
    out = set()
    off = r2o(rva)
    if off is None:
        return out, base, secs
    end = off + size
    while off < end:
        page, blk = struct.unpack_from('<II', d, off)
        if blk < 8:
            break
        for i in range(off + 8, off + blk, 2):
            e = struct.unpack_from('<H', d, i)[0]
            if e >> 12 == 3:
                out.add(page + (e & 0xFFF))
        off += blk
    return out, base, secs


def text_of(path):
    base, secs = read_pe_text_info(path)
    t = [s for s in secs if s[0].startswith('.text')][0]
    _, rva, vs, raw, rs = t
    return base, rva, raw, open(path, 'rb').read()[raw:raw + rs]


def find_twin(dva, size, d3d, glide, drel, grel, dbase, gbase, drva, grva):
    doff = dva - dbase - drva
    body = d3d[doff:doff + size]
    # reloc offsets relative to the function. drel holds RVAs (offset from
    # image base), so the function's RVA window is [dva-dbase, +size).
    drva0 = dva - dbase
    rel = sorted(o - drva0 for o in drel if drva0 <= o < drva0 + size)
    hits = []
    # scan Glide .text on 1-byte granularity
    b0 = body[0]
    for i in range(len(glide) - size + 1):
        if glide[i] != b0:
            continue
        gva = gbase + grva + i
        gva0 = gva - gbase
        ok = True
        for j in range(size):
            # mask a 4-byte slot at each D3D reloc, or where Glide relocates
            if any(r <= j < r + 4 for r in rel):
                continue
            if any((gva0 + j - k) in grel for k in range(4)):
                continue
            if glide[i + j] != body[j]:
                ok = False
                break
        if not ok:
            continue
        # structural: every D3D reloc offset must also be a Glide reloc
        if all((gva0 + r) in grel for r in rel):
            hits.append(gva)
    return hits, rel


def main():
    drel, dbase, _ = relocs_in_text(os.path.join(ROOT, 'orig', 'BRD3D.dll'))
    grel, gbase, _ = relocs_in_text(os.path.join(ROOT, 'orig', 'BRGlide.dll'))
    _, drva, _, d3d = text_of(os.path.join(ROOT, 'orig', 'BRD3D.dll'))
    _, grva, _, glide = text_of(os.path.join(ROOT, 'orig', 'BRGlide.dll'))

    if '--batch' in sys.argv:
        # untwinned d3d @implements tags
        fwd = set()
        for r in csv.DictReader(open(os.path.join(ROOT, 'config',
                                                  'shared.csv'))):
            if r['d3d_va'].strip() and r['glide_va'].strip():
                fwd.add(int(r['d3d_va'], 16))
        TAG = re.compile(r'@implements\s+0x([0-9A-Fa-f]+)\s+(\w+)\s+(\w+)')
        import glob
        targets = []
        for p in sorted(glob.glob(os.path.join(ROOT, 'src', '**', '*.c'),
                                  recursive=True)):
            for ln, line in enumerate(open(p, errors='replace'), 1):
                m = TAG.search(line)
                if m and m.group(2) == 'd3d':
                    va = int(m.group(1), 16)
                    if va not in fwd:
                        targets.append((va, m.group(3), p, ln))
        for va, name, p, ln in targets:
            b = os.path.join(ROOT, 'build', 'match', 'orig_d3d',
                             '0x%08X.bin' % va)
            size = os.path.getsize(b) if os.path.exists(b) else 0
            if not size:
                print(f"{name:22s} 0x{va:08X}  NO D3D REF")
                continue
            hits, rel = find_twin(va, size, d3d, glide, drel, grel, dbase,
                                  gbase, drva, grva)
            uniq = sorted(set(hits))
            tag = (f"UNIQUE glide 0x{uniq[0]:08X}" if len(uniq) == 1
                   else f"{len(uniq)} candidates" if uniq else "ABSENT")
            print(f"{name:22s} d3d 0x{va:08X} {size:4d}b relocs={len(rel)}  "
                  f"{tag}"
                  + (f"  {os.path.relpath(p,ROOT)}:{ln}"
                     if len(uniq) != 1 else ''))
        return

    dva = int(sys.argv[1], 16)
    size = int(sys.argv[2])
    hits, rel = find_twin(dva, size, d3d, glide, drel, grel, dbase, gbase,
                          drva, grva)
    print(f"relocs={rel}")
    for h in sorted(set(hits)):
        print(f"  glide 0x{h:08X}")


if __name__ == '__main__':
    main()
