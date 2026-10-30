#!/usr/bin/env python3
"""version.py -- the release version: the average of the decompilations' M2
progress (byte-exact bytes over the hand-written target) over Top Gear
Rally's three cartridges (USA, Europe, Japan) and Boss Rally, as a fraction
with two decimals, rounded down. The three cartridges complete and Boss Rally
at 78.6% is release 0.94; all four complete is 1.00.

RELEASE, when set, is the version instead, for a release of the builder and
the ports while M2 has not moved (0.89: the Windows fixes, Keys and Pad and
Exit Game in Top Gear Rally, Quit and the views in Remastered, M2 still at
0.88; 0.95: Build Anyway for dumps that do not match, M2 still at 0.94). It
gives way to M2 once M2 passes it.

The counts come from the same tools that draw the README's bars
(tools/brally/tiers.py and tools/tgrally/n64tiers.py, through
tools/brally/progressbar.py), rebuilt fresh, never from the README.

    builder/version.py           print the version
    builder/version.py --detail  and the two percentages it came from
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'brally'))
import progressbar  # noqa: E402

RELEASE = '0.95'


def main():
    t3_fns, t3_b, t4_fns, t4_b, target, target_b = progressbar.tier_counts()[:6]
    shares = [('Boss Rally', t4_b, target_b)]
    for region, label in (('usa', 'USA'), ('eur', 'Europe'), ('jpn', 'Japan')):
        n3f, n3b, n4f, n4b, ntf, ntb = progressbar.n64_counts(region)
        shares.append(('Top Gear Rally (%s)' % label, n4b, ntb))
    avg = sum(done / total for _, done, total in shares) / len(shares)
    hundredths = int(avg * 100 + 1e-9)
    version = '%d.%02d' % (hundredths // 100, hundredths % 100)
    if RELEASE and float(RELEASE) > float(version):
        version = RELEASE
    if '--detail' in sys.argv:
        for name, done, total in shares:
            print('%-24s M2 %6.2f%%  (%d / %d B)' % (name, 100.0 * done / total, done, total))
    print(version)


if __name__ == '__main__':
    main()
