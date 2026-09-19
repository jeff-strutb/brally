#!/usr/bin/env python3
"""Verify the PLACED bytes of the contract-valid image, function by function.

t3b_verify.py answers "is this transcription's OBJECT equivalent?" -- it
recompiles, resolves and runs.  This tool answers the question the Win98
machine otherwise answers with a register dump: "are the bytes actually IN
BRGlide.T3.dll equivalent to the original's?"  It reads each certified T3
function's span straight out of the built image -- after every placement
layer: identity slots, pairing, audits, hand rows -- and runs the A5
comparison against the original's span.  No recompiling, no resolving, no
object-variant mismatch: what ships is what runs.

A span whose bytes equal the original's byte-for-byte is reported IDENTICAL
without a run (nothing to compare).  Anything else runs both sides over the
seeded image worlds exactly as t3b_verify does.

Usage:
    .venv/bin/python tools/t3image_verify.py [--image PATH] [--seeds N] [va]
"""
import argparse
import csv
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import t3b_verify as V                                     # noqa: E402
import image_build as ib                                   # noqa: E402
from t3 import certified                                   # noqa: E402

DEFAULT_IMAGE = os.path.join(ROOT, 'build', 'image', 'BRGlide.T3.dll.FAILED')


class Pe(object):
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', self.d, 0x3c)[0]
        nsec = struct.unpack_from('<H', self.d, pe + 6)[0]
        opt = struct.unpack_from('<H', self.d, pe + 20)[0]
        self.base = struct.unpack_from('<I', self.d, pe + 24 + 28)[0]
        self.secs = []
        o = pe + 24 + opt
        for _ in range(nsec):
            vsz, va, rsz, ro = struct.unpack_from('<IIII', self.d, o + 8)
            self.secs.append((va, rsz, ro))
            o += 40

    def read(self, va, n):
        rva = va - self.base
        for sva, rsz, ro in self.secs:
            if sva <= rva < sva + rsz:
                return self.d[ro + (rva - sva):ro + (rva - sva) + n]
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va', nargs='?')
    ap.add_argument('--image', default=DEFAULT_IMAGE)
    ap.add_argument('--seeds', type=int, default=48)
    a = ap.parse_args()

    img = Pe(a.image)
    # annexed functions: the span at the VA is a `jmp` thunk and the body
    # lives in the appended .t3x section; the manifest maps one to the other
    annexed = {}
    man = os.path.join(os.path.dirname(a.image), 't3_annex.csv')
    if os.path.exists(man):
        for r in csv.DictReader(open(man)):
            try:
                annexed[int(r['va'], 16)] = (int(r['annex_va'], 16),
                                             int(r['length']))
            except (ValueError, KeyError):
                pass
    cert = {v for v, i in certified().items()
            if not v.startswith('?') and i['ok']}
    rows = []
    seen_vas = set()
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    for r in csv.DictReader(open(rep)):
        if r.get('status') == 'diff' and r.get('va') \
                and r['va'].lower() in cert:
            if a.va and int(r['va'], 16) != int(a.va, 16):
                continue
            rows.append(r)
            seen_vas.add(int(r['va'], 16))
    repc = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
    if os.path.exists(repc):
        for r in csv.DictReader(open(repc)):
            if r.get('status') == 'diff' and r.get('va') \
                    and r['va'].lower() in cert \
                    and int(r['va'], 16) not in seen_vas:
                if a.va and int(r['va'], 16) != int(a.va, 16):
                    continue
                rows.append(r)

    tally = {}
    for r in sorted(rows, key=lambda x: x['va']):
        va = int(r['va'], 16)
        name = r['name']
        ob = os.path.join(ib.ORIG_DIR, '0x%08X.bin' % va)
        if not os.path.exists(ob):
            continue
        orig = open(ob, 'rb').read()
        placed = img.read(va, len(orig))
        if placed is None:
            print('%s %-24s NOT-IN-IMAGE' % (r['va'], name))
            tally['NOT-IN-IMAGE'] = tally.get('NOT-IN-IMAGE', 0) + 1
            continue
        if placed == orig:
            print('%s %-24s IDENTICAL (original bytes -- not placed or '
                  'byte-exact)' % (r['va'], name))
            tally['IDENTICAL'] = tally.get('IDENTICAL', 0) + 1
            continue
        sig = V.parse_signature(name)
        if sig is None:
            print('%s %-24s UNCLASSIFIED (no parsable signature)'
                  % (r['va'], name))
            tally['NO-SIG'] = tally.get('NO-SIG', 0) + 1
            continue
        buried = V.ENV.shadows_a_neighbour(va, len(orig))
        # placed spans are exactly orig-length by construction, so a
        # neighbour can only be shadowed if the map disagrees with itself
        extra = None
        if va in annexed:
            a_va, alen = annexed[va]
            abody = img.read(a_va, alen)
            if abody is None:
                print('%s %-24s ANNEX-MISSING (manifest names 0x%08X but '
                      'the image has no bytes there)' % (r['va'], name, a_va))
                tally['ANNEX-MISSING'] = tally.get('ANNEX-MISSING', 0) + 1
                continue
            extra = (a_va, abody)
        verdict, why = V.verify_img(va, name, orig, placed,
                                    a.seeds, sig, extra_code=extra)
        if extra is not None:
            why = 'annexed body at 0x%08X; %s' % (extra[0], why)
        print('%s %-24s %s (%s)' % (r['va'], name, verdict, why))
        tally[verdict] = tally.get(verdict, 0) + 1
    print('\n== placed-image verdicts ==')
    for k in sorted(tally):
        print('  %-14s %d' % (k, tally[k]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
