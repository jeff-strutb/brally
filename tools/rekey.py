#!/usr/bin/env python3
"""Re-key the reference corpus from BRD3D to BRGlide (rule 0).

The pipeline is D3D-canonical BY DESIGN, not by accident: load_shared_map()
maps glide_va -> d3d_va and parse_implements() translates a `glide`-tagged
address INTO D3D space before scoring. 108 tags in the tree are already written
in Glide addresses and get converted to D3D. So this is not a tag rewrite --
it is inverting which binary the tree treats as canonical.

This tool does the half that is purely mechanical and verifiable: extract the
reference bytes from BRGlide.dll at Glide addresses, and report exactly what
does and does not carry over. It writes to a SEPARATE directory and changes
nothing live, so the result can be inspected before anything is switched.

    python3 tools/rekey.py                 # measure only
    python3 tools/rekey.py --extract       # write build/match/orig_glide/
"""
import csv
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from pe_patch import read_pe_text_info  # noqa: E402

OUT = os.path.join(ROOT, 'build', 'match', 'orig_glide')
TAG = re.compile(r'@implements\s+0x([0-9A-Fa-f]+)\s+(\w+)\s+(\w+)')


def load_twins():
    """d3d_va -> (glide_va, size, class), and the reverse."""
    fwd, rev = {}, {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'config', 'shared.csv'))):
        d, g = r['d3d_va'].strip(), r['glide_va'].strip()
        if not d or not g:
            continue
        sz = int(r['size']) if r['size'].strip() else 0
        fwd[int(d, 16)] = (int(g, 16), sz, r['class'])
        rev[int(g, 16)] = (int(d, 16), sz, r['class'])
    return fwd, rev


def tags():
    out = []
    for p in sorted(glob.glob(os.path.join(ROOT, 'src', '**', '*.c'),
                              recursive=True)):
        for i, line in enumerate(open(p, errors='replace'), 1):
            m = TAG.search(line)
            if m:
                out.append((os.path.relpath(p, ROOT), i, int(m.group(1), 16),
                            m.group(2), m.group(3)))
    return out


def main():
    fwd, rev = load_twins()
    ts = tags()
    print(f"@implements tags: {len(ts)}")
    by = {}
    for _, _, _, b, _ in ts:
        by[b] = by.get(b, 0) + 1
    print(f"  by declared build: {by}")

    # Resolve every tag to a GLIDE address.
    resolved, unmapped = [], []
    for f, ln, va, build, name in ts:
        if build == 'glide':
            gva = va                      # already Glide
            sz = rev.get(va, (0, 0, ''))[1]
        elif va in fwd:
            gva, sz, _ = fwd[va]
        else:
            unmapped.append((f, ln, va, build, name))
            continue
        resolved.append((f, ln, va, build, name, gva, sz))

    print(f"\nresolvable to a Glide address: {len(resolved)}")
    print(f"NO Glide counterpart          : {len(unmapped)}")
    for f, ln, va, b, n in unmapped[:15]:
        print(f"    {va:#010x} {b:5s} {n:28s} {f}:{ln}")

    # Extract from BRGlide at those addresses.
    gp = os.path.join(ROOT, 'orig', 'BRGlide.dll')
    base, secs = read_pe_text_info(gp)
    text = [s for s in secs if s[0].startswith('.text')][0]
    _, rva, vsize, raw, rawsz = text
    img = open(gp, 'rb').read()

    ok, nosize, oob = [], [], []
    for f, ln, va, build, name, gva, sz in resolved:
        if sz <= 0:
            nosize.append((name, gva))
            continue
        off = gva - base - rva
        if off < 0 or off + sz > vsize:
            oob.append((name, gva))
            continue
        ok.append((gva, sz, name))

    print(f"\nextractable from BRGlide.dll  : {len(ok)}")
    print(f"  no size recorded in twin map: {len(nosize)}")
    print(f"  address outside .text       : {len(oob)}")
    print(f"  bytes                       : {sum(s for _, s, _ in ok):,}")

    if '--extract' in sys.argv:
        os.makedirs(OUT, exist_ok=True)
        for f in os.listdir(OUT):
            os.unlink(os.path.join(OUT, f))
        for gva, sz, name in ok:
            off = raw + (gva - base - rva)
            open(os.path.join(OUT, '0x%08X.bin' % gva), 'wb').write(
                img[off:off + sz])
        print(f"\nwrote {len(ok)} reference functions -> "
              f"{os.path.relpath(OUT, ROOT)}")
        print("NOTHING LIVE HAS CHANGED. Switching the corpus also requires "
              "inverting load_shared_map() and parse_implements(), and "
              "re-learning config/globals.csv, which is 2,569 D3D addresses "
              "with no twin map (shared.csv covers functions only).")


if __name__ == '__main__':
    main()
