#!/usr/bin/env python3
"""Treemap SVG for the Top Gear Rally (N64) lane -- the sibling of the PC
lane's tools/progressmap.py, in the same visual language.

Every ROM function is one box, sized by its bytes and coloured by the tier its
transcription has reached (green T4 / blue T3 / amber T2 / gray T1 / purple
fenced library / teal excluded).  The classification is n64tiers.classify() --
the same source the N64 milestone bars are computed from -- so the map and the
bars can never disagree.  Rendering reuses tools/progressmap.render_svg
verbatim, so both lanes' maps stay pixel-for-pixel consistent; this file only
translates N64 tiers into the shapes that renderer expects.

    python3 tools/tgrally/n64map.py --svg docs/tgrally/progress-map.svg
    python3 tools/tgrally/n64map.py --svg <path> --no-build   # skip the fresh build
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'brally'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'tgrally'))
import progressmap as pm            # noqa: E402  -- squarify/layout/render_svg
import n64tiers as nt               # noqa: E402  -- classify()

# N64 tier -> the status vocabulary render_svg colours by.
TIER_STATUS = {'T4': 'match', 'T3': 'codegen', 'T2': 'diff', 'T1': 'todo',
               'FENCED': 'fenced'}


def _status(tier):
    return 'excluded' if tier.startswith('EXCLUDED') else TIER_STATUS.get(tier,
                                                                          'todo')


def build_funcs(fresh=True):
    """The N64 functions shaped exactly like progressmap's PC ones.

    file/name come from build/tgrally/n64/report.csv where a row has a resolved ROM
    address; a not-started (T1) function has no row, so it falls to the
    renderer's `unfiled 0x80xxxxxx` region grouping -- the honest state while
    853 of 883 are still Ghidra drafts.
    """
    fmap, tier = nt.classify(fresh=fresh)
    meta = {}
    rep = os.path.join(ROOT, 'build', 'tgrally', 'n64', 'report.csv')
    if os.path.exists(rep):
        for r in csv.DictReader(open(rep)):
            v = (r.get('n64_va') or '').upper().replace('0X', '').strip()
            if v:
                meta[v] = (r.get('file') or '', r.get('fn') or '')
    funcs = []
    for va, size in fmap.items():
        k = '%08X' % va
        f, name = meta.get(k, ('', ''))
        funcs.append({'file': f, 'va': va, 'size': size, 'name': name,
                      'status': _status(tier[k]), 'diffs': 0})
    return funcs


def main():
    argv = sys.argv[1:]
    out = argv[argv.index('--svg') + 1] if '--svg' in argv \
        else os.path.join(ROOT, 'docs', 'tgrally', 'progress-map.svg')
    funcs = build_funcs(fresh='--no-build' not in argv)
    pm.render_svg(funcs, out)


if __name__ == '__main__':
    main()
