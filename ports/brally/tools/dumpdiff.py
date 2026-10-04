#!/usr/bin/env python3
"""Compare two BR_DUMP data-area dumps global by global.

    dumpdiff.py OURS REFERENCE [--all] [--grep RE]

OURS is the 64-bit build's dump (BR_DUMP=F:PATH, platform/common/script.c);
REFERENCE is the wasm lane's dump of the same frame of the same script. Only
the globals the 64-bit build can place at their original addresses are
compared -- the map is the g_brDumpMap table datalift.py generates into
build/portable/gen/br_data.c. Each differing global prints with its words
shown as integers and as floats, ours first.
"""
import re
import struct
import sys

LO = 0x10077000
MAP = 'build/portable/gen/br_data.c'


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    show_all = '--all' in sys.argv
    pat = None
    if '--grep' in sys.argv:
        pat = re.compile(sys.argv[sys.argv.index('--grep') + 1])
        args = [a for a in args if a != sys.argv[sys.argv.index('--grep') + 1]]
    ours, ref = (open(p, 'rb').read() for p in args[:2])
    rows = [(va, '%s+0x%s' % (nm, o) if o != '0' else nm, n)
            for va, nm, n, o in re.findall(r'\{ 0x([0-9A-F]{8})u, br_sym_(\w+) \+ \d+, (\d+)u \},\s*/\* \w+\+0x([0-9A-F]+) \*/', open(MAP).read())]
    ndiff = 0
    for va, name, n in rows:
        va, n = int(va, 16), int(n)
        if pat and not pat.search(name):
            continue
        a, b = ours[va - LO:va - LO + n], ref[va - LO:va - LO + n]
        if a == b and not show_all:
            continue
        ndiff += 1
        diffs = [i for i in range(0, n - n % 4, 4) if a[i:i + 4] != b[i:i + 4]]
        print('0x%08X %-32s %6d B  %d of %d words differ' % (va, name, n, len(diffs), n // 4))
        for i in diffs[:6]:
            ia, ib = struct.unpack('<i', a[i:i + 4])[0], struct.unpack('<i', b[i:i + 4])[0]
            fa, fb = struct.unpack('<f', a[i:i + 4])[0], struct.unpack('<f', b[i:i + 4])[0]
            print('    +0x%04X  %11d %11d   %-12.6g %-12.6g' % (i, ia, ib, fa, fb))
    print('%d of %d globals differ' % (ndiff, len(rows)))


if __name__ == '__main__':
    main()
