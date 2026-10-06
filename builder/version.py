#!/usr/bin/env python3
"""version.py -- the release version: the lower of the two decompilations' M2
progress (byte-exact bytes over the hand-written target), as a fraction with
two decimals, rounded down. M2 at 73.6% for one game and 75.7% for the other
is release 0.73; both complete is 1.00.

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


def main():
    t3_fns, t3_b, t4_fns, t4_b, target, target_b = progressbar.tier_counts()[:6]
    n3f, n3b, n4f, n4b, ntf, ntb = progressbar.n64_counts()
    br, tgr = t4_b / target_b, n4b / ntb
    low = min(br, tgr)
    hundredths = int(low * 100 + 1e-9)
    version = '%d.%02d' % (hundredths // 100, hundredths % 100)
    if '--detail' in sys.argv:
        print('Boss Rally M2      %.2f%%  (%d / %d B)' % (100 * br, t4_b, target_b))
        print('Top Gear Rally M2  %.2f%%  (%d / %d B)' % (100 * tgr, n4b, ntb))
    print(version)


if __name__ == '__main__':
    main()
