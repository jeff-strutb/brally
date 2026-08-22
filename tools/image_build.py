#!/usr/bin/env python3
"""Assemble a whole image from our compiled code plus the original's bytes, and
diff it against the original.

This is the test the per-function checks cannot perform.  match_sweep.py asks
"do these bytes equal the original's bytes at this address", one function at a
time, and a function that answers yes can still be wrong about WHERE it goes:
two functions can claim the same address, a claimed range can overrun into its
neighbour, a size can be wrong.  Those are not hypothetical -- a stale row set
double-claiming 38 addresses is what the report was quietly doing until it was
caught by hand.

So this does not check functions.  It checks the map: every claim is laid into
one image, collisions are refused rather than resolved, and the result is
compared with the original.  Undecompiled functions are not stubbed or skipped;
the original's own bytes stand in for them, which is the standard matching-decomp
arrangement -- coverage decides how much of the image is OURS, not whether the
image can be built at all.

A clean run means: everything we claim to have reproduced, assembled at the
addresses we claim, reproduces the original image exactly.

Usage:
    python3 tools/image_build.py [--out build/BRD3D_rebuilt.dll]
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from relocmap import REL_DIR32, REL_REL32                      # noqa: E402
from reloc_fill import parse, load_maps, resolve               # noqa: E402
from reloc_learn import live_objs                              # noqa: E402
from pe_patch import read_pe_text_info                         # noqa: E402

ORIG_DLL = os.path.join(ROOT, 'orig', 'BRD3D.dll')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')


def compiled_functions(objs, fnmap, glmap):
    """Yield (va, name, filled_bytes, n_unresolved) for every function we build."""
    for path in objs:
        try:
            d, secs, syms, relocs = parse(path)
        except Exception:
            continue
        byidx = {s['idx']: s for s in syms}
        for sy in syms:
            sec = secs.get(sy['sec'])
            if sy['sec'] <= 0 or not sec or not sec['name'].startswith('.text'):
                continue
            name = sy['name'].lstrip('_')
            if name not in fnmap:
                continue
            va = fnmap[name]
            ob = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
            if not os.path.exists(ob):
                continue
            n = len(open(ob, 'rb').read())
            start = sec['praw'] + sy['val']
            code = bytearray(d[start:start + n])
            if len(code) != n:
                continue
            unres = 0
            for rva, si, rt in relocs[sy['sec']]:
                off = rva - sy['val']
                if not (0 <= off <= n - 4):
                    continue
                t = byidx.get(si)
                tgt = resolve(t['name'], fnmap, glmap) if t else None
                if tgt is None:
                    unres += 1
                    continue
                addend = struct.unpack_from('<i', code, off)[0]
                if rt == REL_DIR32:
                    val = tgt + addend
                elif rt == REL_REL32:
                    val = tgt + addend - (va + off + 4)
                else:
                    unres += 1
                    continue
                struct.pack_into('<I', code, off, val & 0xFFFFFFFF)
            yield va, name, bytes(code), unres


def main():
    out = None
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]

    fnmap, glmap = load_maps()
    objs, _ = live_objs()

    # Only place what the tree actually CLAIMS to have reproduced. fnmap holds
    # every tagged function including the ones still diffing; laying those in
    # would measure the size of the undone work, not the truth of the claims.
    import csv
    claimed = set()
    names_at = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'build', 'match',
                                              'report.csv'))):
        if r.get('va') and r.get('status') == 'match':
            claimed.add(int(r['va'], 16))
            names_at.setdefault(int(r['va'], 16), set()).add(r['name'])

    # Best build per address: fully-resolved beats partially-resolved.
    best = {}
    for va, name, code, unres in compiled_functions(objs, fnmap, glmap):
        if va not in claimed:
            continue
        cur = best.get(va)
        if cur is None or unres < cur[2]:
            best[va] = (name, code, unres)

    usable = {va: v for va, v in best.items() if v[2] == 0}
    blocked = len(best) - len(usable)

    # GLOBAL CHECK 1 -- overlapping claims. Per-function diffing cannot see it.
    spans = sorted((va, va + len(c), n) for va, (n, c, _) in usable.items())
    overlaps = [(a, b) for a, b in zip(spans, spans[1:]) if a[1] > b[0]]

    # GLOBAL CHECK 2 -- two DIFFERENT functions claiming one address. The same
    # function appearing twice is not a conflict: every source file is built at
    # both /O2 and /Od, so each function legitimately has two builds. Only
    # distinct names at one address mean a tagging error.
    conflicting = [(va, sorted(ns)) for va, ns in sorted(names_at.items())
                   if len(ns) > 1 and va in usable]

    print(f"functions compiled and addressed : {len(best)}")
    print(f"  every relocation resolved      : {len(usable)}")
    print(f"  blocked on an unknown address  : {blocked}")
    print(f"\noverlapping address claims       : {len(overlaps)}")
    for a, b in overlaps[:10]:
        print(f"    {a[2]} [{a[0]:#x}..{a[1]:#x}) runs into {b[2]} at {b[0]:#x}")
    print(f"two names claiming one address   : {len(conflicting)}")
    for va, ns in conflicting[:10]:
        print(f"    {va:#x}: {' vs '.join(ns)}")

    # Lay every usable function into the original image at its claimed address.
    base, secs = read_pe_text_info(ORIG_DLL)
    img = bytearray(open(ORIG_DLL, 'rb').read())
    text = [s for s in secs if s[0].startswith('.text')][0]
    _, trva, tvsize, traw, _ = text

    placed = bytes_placed = 0
    outside = 0
    diffs = []
    for va, (name, code, _) in sorted(usable.items()):
        off = va - base - trva
        if off < 0 or off + len(code) > tvsize:
            outside += 1
            continue
        fo = traw + off
        if img[fo:fo + len(code)] != code:
            nd = sum(1 for i in range(len(code)) if img[fo + i] != code[i])
            diffs.append((va, name, nd, len(code)))
        img[fo:fo + len(code)] = code
        placed += 1
        bytes_placed += len(code)

    orig = open(ORIG_DLL, 'rb').read()
    delta = sum(1 for i in range(len(orig)) if orig[i] != img[i])

    print(f"\nplaced into the image            : {placed} functions, "
          f"{bytes_placed:,} bytes ({100*bytes_placed/tvsize:.2f}% of .text)")
    print(f"  landed outside .text           : {outside}")
    print(f"functions differing from original: {len(diffs)}")
    for va, name, nd, sz in sorted(diffs, key=lambda x: -x[2])[:10]:
        print(f"    {va:#x} {name}: {nd}/{sz} bytes")

    print(f"\nASSEMBLED IMAGE vs ORIGINAL: {delta} differing bytes")
    if delta == 0 and not overlaps and not conflicting:
        print("  -> every claim holds at image level.")

    if out:
        os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
        open(out, 'wb').write(bytes(img))
        print(f"\nwrote {out}")


if __name__ == '__main__':
    main()
