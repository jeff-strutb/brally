"""Build the Top Gear Rally image from the decomp -- the N64 M2 and M1 gates.

    .venv/bin/python tools/tgrally/n64image.py            # M2: every T4 body placed
    .venv/bin/python tools/tgrally/n64image.py --t3       # M1: T3 bodies placed as well

M2 (byte-exact).  Every function n64build.py grades EXACT is compiled, its
relocations resolved, and written over its own address in a copy of the ROM's
code segment.  The result must equal the original: 0 differing bytes.

M1 (contract-valid).  The same, plus every T3-certified body (`@t3` tag with
an EQUIVALENT verdict in config/tgrally/t3_live.csv, or --all-tagged for a
candidate run).  A T3 body that fits its slot goes in place; one that is
larger goes in the annex above the game's 4 MB (0x80500000) with a jump at
its slot.  Its literals go in the annex's data area.  The differing bytes are
then exactly the T3 bodies, and the image is what n64t3.py --image runs
headless against the original.
"""
import argparse
import csv
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools/tgrally'))
import n64build as B  # noqa: E402
import n64link as L  # noqa: E402

SEG = 0x100000                     # IPL3 loads the first megabyte after the header
ANNEX_CODE = 0x80500000
ANNEX_DATA = 0x80600000


def t3_certified():
    p = os.path.join(ROOT, 'config/tgrally/t3_live.csv')
    if not os.path.exists(p):
        return set()
    return {int(r['va'], 16) for r in csv.DictReader(open(p)) if r['verdict'] == 'EQUIVALENT'}


def build(t3=False, only=None, all_tagged=False):
    """-> (image bytes for 0x80200000.., [(va, bytes)] annex blocks, report)"""
    rom, fmap, syms = B.Rom(), B.function_map(), B.load_symbols()
    img = bytearray(rom.d[B.ROMOFF:B.ROMOFF + SEG])
    extra = []
    cert = t3_certified()
    rep = dict(t4=0, t3_slot=0, t3_annex=0, skipped=[])
    acode, adata = ANNEX_CODE, ANNEX_DATA
    for f in B.all_sources():
        src = open(f).read()
        tags = B.tags_in(src)
        if not tags:
            continue
        t3tags = {int(m, 16) for m in B.T3TAG.findall(src)}
        obj, err = B.compile_c(f)
        if obj is None:
            rep['skipped'].append((os.path.relpath(f, ROOT), 'compile error'))
            continue
        pieces = {n: (s, e) for n, s, e in B.carve(obj) if n}
        fnvas = {n: v for v, n, _ in tags}
        sbase = None
        for va, name, _ in tags:
            if only is not None and va not in only:
                continue
            if name not in pieces or va not in fmap:
                continue
            s, e = pieces[name]
            st, nd, notes, fixed, theirs = B.grade(obj, rom, name, s, e, va, fmap[va], syms, fnvas)
            off = va - B.BASE
            if st == 'EXACT':
                img[off:off + 4 * len(fixed)] = b''.join(struct.pack('>I', w) for w in fixed)
                rep['t4'] += 1
                continue
            if not t3:
                continue
            if not (all_tagged or (va in t3tags and va in cert)):
                continue
            if sbase is None:
                sbase = B.static_bases(obj, fnvas, rom, fmap)
            try:
                code, data = L.link_function(obj, name, va, adata, fnvas, syms, static_va=sbase)
            except L.LinkError as ex:
                rep['skipped'].append(('%08X' % va, str(ex)))
                continue
            if len(code) > fmap[va]:
                code, data = L.link_function(obj, name, acode, adata, fnvas, syms, static_va=sbase)
                extra.append((acode, code))
                j = 0x08000000 | ((acode >> 2) & 0x03FFFFFF)
                img[off:off + 8] = struct.pack('>II', j, 0)
                acode += (len(code) + 15) & ~15
                rep['t3_annex'] += 1
            else:
                img[off:off + fmap[va]] = code.ljust(fmap[va], b'\0')
                rep['t3_slot'] += 1
            for dva, blob in data:
                extra.append((dva, blob))
                adata = max(adata, (dva + len(blob) + 15) & ~15)
    return bytes(img), extra, rep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--t3', action='store_true')
    ap.add_argument('--all-tagged', action='store_true',
                    help='place every tagged non-exact body (candidate runs only)')
    a = ap.parse_args()
    img, extra, rep = build(t3=a.t3 or a.all_tagged, all_tagged=a.all_tagged)
    rom = B.Rom().d[B.ROMOFF:B.ROMOFF + SEG]
    diff = sum(1 for x, y in zip(img, rom) if x != y)
    print('placed: %d byte-exact, %d T3 in slot, %d T3 in the annex (%d annex blocks)'
          % (rep['t4'], rep['t3_slot'], rep['t3_annex'], len(extra)))
    for s in rep['skipped']:
        print('  skipped', s)
    print('%d bytes differ from the ROM%s' % (diff, '' if a.t3 or a.all_tagged else
                                              '   (M2 gate: must be 0)'))
    os.makedirs(B.OUT, exist_ok=True)
    open(os.path.join(B.OUT, 'tgr_m1.bin' if a.t3 else 'tgr_m2.bin'), 'wb').write(img)
    return 1 if (diff and not (a.t3 or a.all_tagged)) else 0


if __name__ == '__main__':
    sys.exit(main())
