"""Four-tier coverage for Top Gear Rally -- the N64 lane's tiers.py.

    .venv/bin/python n64/tools/n64tiers.py            # M1 / M2 and the four tiers
    .venv/bin/python n64/tools/n64tiers.py --list T2  # VAs in a tier

Same standard as the PC lane (CLAUDE.md rule 12), measured against the ROM:

    T1  not started   no code in n64/src yet.  Every function has a Ghidra
                      draft (n64/tools/n64ghidra.sh) and many have a PC twin;
                      neither is project code.
    T2  in progress   tagged in n64/src, but the bytes still differ.
    T3  certified     complete, not byte-exact: an `@t3 0x80XXXXXX` tag AND an
                      EQUIVALENT verdict from the live oracle in
                      n64/config/t3_live.csv (n64/tools/n64t3.py --qualify).
    T4  byte-exact    n64/tools/n64build.py grades it EXACT: every word equal
                      with every relocation resolved.

    M1 = T3 + T4 (contract-valid), M2 = T4 (byte-exact).

Denominator: every function in the ROM's .text map, less the FENCED rows in
n64/config/fenced_tgr.csv (Nintendo's libultra and the zlib inflater -- library
code linked into the game, the N64 counterpart of the PC lane's static CRT)
and the EXCLUDED rows in n64/config/excluded_tgr.csv (game code the retail
game provably never runs).  Both are counted on their own lines.

The build is re-run first -- it takes seconds -- so the counts are never stale.
"""
import argparse
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
sys.path.insert(0, os.path.join(N64, 'tools'))
import n64build as B  # noqa: E402

T3TAG = re.compile(r'@t3\s+(0x[0-9A-Fa-f]{8})')


def csv_vas(path):
    if not os.path.exists(path):
        return {}
    return {r['va'].upper().replace('0X', ''): r for r in csv.DictReader(open(path))}


def t3_tags():
    out = set()
    for f in B.all_sources():
        for m in T3TAG.finditer(open(f, errors='replace').read()):
            out.add('%08X' % int(m.group(1), 16))
    return out


def classify(fresh=True):
    if fresh:
        subprocess.run([sys.executable, os.path.join(N64, 'tools/n64build.py'), '-q'],
                       cwd=ROOT, capture_output=True)
    fmap = B.function_map()
    fenced = csv_vas(os.path.join(N64, 'config/fenced_tgr.csv'))
    excluded = csv_vas(os.path.join(N64, 'config/excluded_tgr.csv'))
    live = csv_vas(os.path.join(N64, 'config/t3_live.csv'))
    ver = {}
    p = os.path.join(B.OUT, 'verify.csv')
    if os.path.exists(p):
        for r in csv.DictReader(open(p)):
            if r['va']:
                ver[r['va'].upper()] = r
    t3 = t3_tags()
    tier = {}
    for va, size in fmap.items():
        k = '%08X' % va
        if k in fenced:
            tier[k] = 'FENCED'
            continue
        r = ver.get(k)
        if r is None:
            t = 'T1'
        elif r['status'] == 'EXACT':
            t = 'T4'
        elif k in t3 and live.get(k, {}).get('verdict') == 'EQUIVALENT':
            t = 'T3'
        else:
            t = 'T2'
        tier[k] = ('EXCLUDED (%s)' % t) if k in excluded else t
    return fmap, tier


def bar(frac, width=40):
    n = int(round(frac * width))
    return '█' * n + '░' * (width - n)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--list')
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    fmap, tier = classify(not a.no_build)
    size = {'%08X' % v: s for v, s in fmap.items()}
    if a.list:
        for k in sorted(tier):
            if tier[k] == a.list:
                print(k, size[k])
        return
    agg = {}
    for k, t in tier.items():
        c = agg.setdefault(t, [0, 0])
        c[0] += 1
        c[1] += size[k]
    target = [k for k, t in tier.items() if t in ('T1', 'T2', 'T3', 'T4')]
    tf, tb = len(target), sum(size[k] for k in target)
    allb = sum(size.values())
    g = lambda t: agg.get(t, [0, 0])
    m1 = [g('T3')[0] + g('T4')[0], g('T3')[1] + g('T4')[1]]
    m2 = g('T4')
    print('=' * 64)
    print('  Top Gear Rally (N64) game code: %d functions   (%s B of %s B .text)'
          % (tf, format(tb, ','), format(allb, ',')))
    print('  ' + '-' * 60)
    for t, what in (('T1', 'not started (Ghidra draft only)'),
                    ('T2', 'in progress (not done)'),
                    ('T3', 'certified, not byte-exact'),
                    ('T4', 'done (byte-exact)')):
        print('  %-3s %-32s %5d fns %9s B' % (t, what, g(t)[0], format(g(t)[1], ',')))
    ex = [(t, c) for t, c in agg.items() if t.startswith('EXCLUDED')]
    for t, c in sorted(ex):
        print('  %-36s %5d fns %9s B' % (t, c[0], format(c[1], ',')))
    print('  %-36s %5d fns %9s B   (libultra, zlib: n64/config/fenced_tgr.csv)'
          % ('FENCED library', g('FENCED')[0], format(g('FENCED')[1], ',')))
    print('  ' + '-' * 60)
    for name, c in (('M1  Contract-valid (T3 + T4)', m1), ('M2  Byte-exact (T4)', m2)):
        f = c[1] / tb if tb else 0
        print('  %s' % name)
        print('      %s %5.1f%%   %s / %s B   %d / %d fns'
              % (bar(f), 100 * f, format(c[1], ','), format(tb, ','), c[0], tf))
    print('=' * 64)


if __name__ == '__main__':
    main()
