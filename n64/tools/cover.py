"""Coverage of the Top Gear Rally (N64) .text by the PC decomp's source.

Every number here carries its denominator: the ROM's own .text is
0x001000-0x070AB0 = 457,392 bytes across 883 functions, and that is what a
percentage is a percentage OF.

  .venv/bin/python tools/tgr/cover.py [build/n64_work/n64report.csv]
"""
import sys, os, csv, collections

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
os.environ.setdefault('TGR_ROM', os.path.join(ROOT, 'reference/tgrally/Top Gear Rally (USA).z64'))
import n64rom  # noqa: E402

TEXT = n64rom.TEXT_E - n64rom.TEXT_S


def sizes():
    F, end = n64rom.F, n64rom.r2v(n64rom.TEXT_E)
    return {v: (F[i + 1] if i + 1 < len(F) else end) - v for i, v in enumerate(F)}


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        ROOT, 'build/n64_work/n64report.csv')
    sz = sizes()
    rows = list(csv.DictReader(open(path)))
    byst = collections.defaultdict(lambda: [0, 0])     # status -> [fns, bytes]
    seen = set()
    for r in rows:
        st = r['status']
        if st in ('CCFAIL',):
            byst[st][0] += 1
            continue
        v = int(r['n64_va'], 16) if r['n64_va'] else None
        b = sz.get(v, 0) if v is not None and v not in seen else 0
        if v is not None:
            seen.add(v)
        byst[st][0] += 1
        byst[st][1] += b

    print("Top Gear Rally (USA) .text: %d bytes / %d functions" % (TEXT, len(n64rom.F)))
    print()
    print("  %-8s %6s  %10s  %7s" % ('status', 'fns', 'bytes', 'of .text'))
    for st in ('EXACT', 'SHAPE', 'MISS', 'CCFAIL'):
        n, b = byst[st]
        if not n:
            continue
        print("  %-8s %6d  %10d  %6.2f%%" % (st, n, b, 100.0 * b / TEXT))
    e, s = byst['EXACT'], byst['SHAPE']
    print()
    print("  byte-exact now          : %d fns  %d B  %.2f%% of .text"
          % (e[0], e[1], 100.0 * e[1] / TEXT))
    print("  + located, not yet exact: %d fns  %d B  %.2f%% of .text"
          % (e[0] + s[0], e[1] + s[1], 100.0 * (e[1] + s[1]) / TEXT))


if __name__ == '__main__':
    main()
