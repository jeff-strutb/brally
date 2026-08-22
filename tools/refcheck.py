#!/usr/bin/env python3
"""Fail loudly if the extracted reference corpus is not keyed to BRGlide.dll.

Rule 0 of CLAUDE.md is that BRGlide is the reference binary. That rule was
written down twice -- in README.md and in commit d98f480 -- and broken twice
anyway, because prose cannot stop a build. This can.

It does not ask what a tool's default says or what a config file claims. It
takes the reference bytes the matching pipeline actually scores against and
asks which binary they came from. Whatever answers is the truth, regardless of
what anything is labelled.

Exit status is the point: 0 only if the corpus is Glide-keyed.

Usage:
    python3 tools/refcheck.py           # check, print, exit 0/1
    python3 tools/refcheck.py --quiet   # exit status only
"""
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from pe_patch import read_pe_text_info  # noqa: E402

REFERENCE = 'BRGlide.dll'          # rule 0
CANDIDATES = ('BRGlide.dll', 'BRD3D.dll')
SAMPLE = 400


def keyed_to(binaries, bins):
    """How many sampled reference functions each binary actually contains."""
    out = {}
    for name in binaries:
        path = os.path.join(ROOT, 'orig', name)
        if not os.path.exists(path):
            out[name] = None
            continue
        base, secs = read_pe_text_info(path)
        text = [s for s in secs if s[0].startswith('.text')]
        if not text:
            out[name] = None
            continue
        _, rva, vsize, raw, _ = text[0]
        img = open(path, 'rb').read()
        hit = 0
        for b in bins:
            va = int(os.path.basename(b)[2:-4], 16)
            d = open(b, 'rb').read()
            off = va - base - rva
            if off < 0 or off + len(d) > vsize:
                continue
            if img[raw + off:raw + off + len(d)] == d:
                hit += 1
        out[name] = hit
    return out


def main():
    quiet = '--quiet' in sys.argv
    bins = sorted(glob.glob(os.path.join(ROOT, 'build', 'match', 'orig',
                                         '*.bin')))[:SAMPLE]
    if not bins:
        if not quiet:
            print('refcheck: no extracted reference bytes; nothing to verify')
        return 0

    res = keyed_to(CANDIDATES, bins)
    best = max((v or -1, k) for k, v in res.items())[1]
    ok = best == REFERENCE and (res[REFERENCE] or 0) > len(bins) * 0.9

    if not quiet:
        print(f'reference corpus: {len(bins)} sampled functions')
        for k, v in res.items():
            mark = '  <-- corpus is keyed to this' if k == best else ''
            print(f'  {k:14s} {"missing" if v is None else f"{v} match"}{mark}')

    if ok:
        if not quiet:
            print(f'\nOK: corpus is keyed to {REFERENCE}, as rule 0 requires.')
        return 0

    if not quiet:
        print(f'\nFAIL: rule 0 of CLAUDE.md requires {REFERENCE}, but the '
              f'corpus is keyed to {best}.')
        print('  Nothing scored against these bytes means what it says it '
              'means.')
        print('  Re-key via config/shared.csv (d3d_va -> glide_va) before '
              'trusting any match count.')
    return 1


if __name__ == '__main__':
    sys.exit(main())
