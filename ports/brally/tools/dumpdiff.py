#!/usr/bin/env python3
"""Compare two BR_DUMP data-area dumps global by global.

    dumpdiff.py OURS REFERENCE [--all] [--grep RE] [--rows N] [--map BR_DATA_C]
    dumpdiff.py OURS REFERENCE --state [--map BR_DATA_C]

OURS is the 64-bit build's dump (BR_DUMP=F:PATH, platform/common/script.c);
REFERENCE is the wasm lane's dump of the same frame of the same script. Only
the globals the 64-bit build can place at their original addresses are
compared -- the map is the g_brDumpMap table datalift.py generates into
build/brally/null-soft/gen/br_data.c (--map for another build's). A word inside a
block of neighbouring globals (globfold.py) is shown with the global the
original has there. Each differing global prints with its words
shown as integers and as floats, ours first.

--state compares game state only: one line per global (differing words,
first word, ours then the reference's), exit status 1 when any differ. It
leaves out a word where the reference holds an address and ours holds 0 or
an address of its own (pointers live in their own 64-bit objects here), and
the globals in NOT_STATE.
"""
import bisect
import re
import struct
import sys

LO = 0x10077000
# Not game state, by global name: what the 32-bit lane's native code owns
# (frame drawing, display lists, CD/EAR, the Glide state; native/*.m
# @replaces), host-side values (paths, DirectPlay sessions, texture memory,
# handles), the save and zero-region tables the data lift fills with the
# core's own sizes, and pointer tables. A name matches as a prefix.
NOT_STATE = (
    'BrGlDepthFuncShadow',
    'BrGlHw',
    'BrGlNavLast6748',
    'BrGlRectMode',
    'BrPathSegCount',
    'DAT_100ad7d0',
    'DAT_100b22d8',
    'DAT_100b2f08',
    'DAT_100b32b0',
    'DAT_100b51e4',
    'DAT_1021c7',
    'DAT_10273340',
    'DAT_1035fba8',
    'DAT_104b1688',
    'DAT_105b972c',
    'DAT_105ccfe8',
    'DAT_105cd9fc',
    'DAT_105d17ec',
    'DAT_105e1828',
    'DAT_106b80a8',
    'DAT_10ac5c54',
    'DAT_118ef1',
    'g_0B39B0',
    'g_0B3A68',
    'g_18ABDE0_64',
    'g_5BCAF8',
    'g_A9BFD0',
    'g_BrCarLightSlot',
    'g_BrCarMtxSlot',
    'g_BrDPlay',
    'g_BrDikEdge',
    'g_BrDrawScale',
    'g_BrSndAA3470',
    'g_aBrCfg',
    'g_aBrCfgBaseDir',
    'g_aBrCfgIniPath',
    'g_aBrDlVtxPool',
    'g_aBrEntRecs',
    'g_aBrNetSession',
    'g_aBrRacePlaceMsg',
    'g_ab0C12A0',
    'g_abBrTrkImage',
    'g_brEarEvent',
    'g_brFfb',
    'g_brKeyBias',
    'g_brRace6EECC8',
    'g_brRaceBeginFxCount',
    'g_brRaceBeginPathScale',
    'g_brRb6F44',
    'g_brTrkHdr',
    'g_navArg',
    'g_szBrAC5DD0',
    's17_',
    's_aClipPool',
)
MAP = 'build/brally/null-soft/gen/br_data.c'


def is_addr(v):
    u = v & 0xFFFFFFFF
    return 0x02000000 <= u < 0x08000000 or 0x10000000 <= u < 0x11900000


def state(rows, ours, ref, names, nvas):
    import collections
    count, first = collections.Counter(), {}
    for va, name, n in rows:
        a, b = ours[va - LO:va - LO + n], ref[va - LO:va - LO + n]
        if a == b:
            continue
        for i in range(0, n - n % 4, 4):
            if a[i:i + 4] == b[i:i + 4]:
                continue
            ia, ib = struct.unpack('<i', a[i:i + 4])[0], struct.unpack('<i', b[i:i + 4])[0]
            if is_addr(ib) and (ia == 0 or (ia & 0xFFFFFFFF) >= 0x20000000 or is_addr(ia)):
                continue
            g = name
            if name.startswith('br_block'):
                k = bisect.bisect_right(nvas, va + i) - 1
                g = names[k][1] if k >= 0 else name
            if g.startswith(NOT_STATE) or name.startswith(NOT_STATE):
                continue
            # a replay record's never-written velocity words, mirrored into
            # the car's shadow states: the 32-bit lane's stack held 1.0
            if ia == 0 and ib == 0x3F800000 and name.startswith(('g_aBrRaceCar', 'g_aBrSnap')):
                continue
            count[g] += 1
            first.setdefault(g, '+0x%X %d %d' % (va + i - LO, ia, ib))
    for g, c in count.most_common():
        print('%5d %-32s %s' % (c, g, first[g]))
    return 1 if count else 0


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    show_all = '--all' in sys.argv
    pat = None
    if '--grep' in sys.argv:
        pat = re.compile(sys.argv[sys.argv.index('--grep') + 1])
        args = [a for a in args if a != sys.argv[sys.argv.index('--grep') + 1]]
    mp = MAP
    if '--map' in sys.argv:
        mp = sys.argv[sys.argv.index('--map') + 1]
        args = [a for a in args if a != mp]
    names = sorted((int(va, 16), n) for n, va in re.findall(
        r'^BR_BLOCK_AT\(\w+, (\w+), 0x[0-9A-F]+\);\s*/\*\s*0x([0-9A-Fa-f]{8})',
        open('ports/brally/src/core/data/br_globals.c').read(), re.M))
    nvas = [v for v, _ in names]
    nrows = 6       # differing words shown per global
    if '--rows' in sys.argv:
        nrows = int(sys.argv[sys.argv.index('--rows') + 1])
        args = [a for a in args if a != sys.argv[sys.argv.index('--rows') + 1]]
    ours, ref = (open(p, 'rb').read() for p in args[:2])
    rows = [(va, '%s+0x%s' % (nm, o) if o != '0' else nm, n)
            for va, nm, n, o in re.findall(r'\{ 0x([0-9A-F]{8})u, br_sym_(\w+) \+ \d+, (\d+)u \},\s*/\* \w+\+0x([0-9A-F]+) \*/', open(mp).read())]
    if '--state' in sys.argv:
        sys.exit(state([(int(va, 16), nm, int(n)) for va, nm, n in rows], ours, ref, names, nvas))
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
        for i in diffs[:nrows]:
            ia, ib = struct.unpack('<i', a[i:i + 4])[0], struct.unpack('<i', b[i:i + 4])[0]
            fa, fb = struct.unpack('<f', a[i:i + 4])[0], struct.unpack('<f', b[i:i + 4])[0]
            where = ''
            if name.startswith('br_block'):
                k = bisect.bisect_right(nvas, va + i) - 1
                if k >= 0:
                    where = '  %s+0x%X' % (names[k][1], va + i - nvas[k])
            print('    +0x%04X  %11d %11d   %-12.6g %-12.6g%s' % (i, ia, ib, fa, fb, where))
    print('%d of %d globals differ' % (ndiff, len(rows)))


if __name__ == '__main__':
    main()
