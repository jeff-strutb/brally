#!/usr/bin/env python3
"""check_payload.py -- prove the game executables the builder ships carry none
of the original games' data.

A release payload is built with the data left out (ports/brally IMAGE=runtime,
ports/tgrally ROMDATA=file): the code is ours, the bytes come from the
player's dump at build or run time. This samples the original data -- the
initialised range of BRGlide.dll the port lifts (0x10077000..0x100BCE00) and
the cartridge from ROM 0x70AB0 on -- in 32-byte windows, keeps the windows
with enough distinct bytes to be meaningful, and fails if any binary window
appears in an executable. A window that is text (printable ASCII and NULs) is
reported but does not fail: the game's messages and file names are string
literals in the decompiled source, so the code carries them as the original
did.

    check_payload.py EXE...
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DLL = os.path.join(ROOT, 'reference/brally/orig/BRGlide.dll')
ROM = os.path.join(ROOT, 'reference/tgrally/Top Gear Rally (USA).z64')
WIN = 32


def dll_image():
    d = open(DLL, 'rb').read()
    e = struct.unpack_from('<I', d, 0x3C)[0]
    nsec, = struct.unpack_from('<H', d, e + 6)
    optsz, = struct.unpack_from('<H', d, e + 20)
    base, = struct.unpack_from('<I', d, e + 24 + 28)
    lo, hi = 0x10077000, 0x100BCE00
    out = bytearray(hi - lo)
    so = e + 24 + optsz
    for i in range(nsec):
        vsz, va, rsz, rp = struct.unpack_from('<IIII', d, so + 8 + 40 * i)
        a, b = max(lo, base + va), min(hi, base + va + max(vsz, rsz))
        n = max(0, min(b, base + va + rsz) - a)
        if a < b and n:
            out[a - lo:a - lo + n] = d[rp + (a - base - va):rp + (a - base - va) + n]
    return bytes(out)


def windows(blob, step):
    for o in range(0, len(blob) - WIN, step):
        w = blob[o:o + WIN]
        if len(set(w)) >= 12:
            yield o, w


def is_text(w):
    return all(32 <= b < 127 or b in (0, 9, 10, 13) for b in w)


def main():
    exes = sys.argv[1:]
    if not exes:
        sys.exit(__doc__)
    sources = [('BRGlide.dll data', dll_image(), 64), ('Top Gear Rally ROM data', open(ROM, 'rb').read()[0x70AB0:], 512)]
    bad = 0
    for path in exes:
        body = open(path, 'rb').read()
        for name, blob, step in sources:
            n = hits = text = 0
            for o, w in windows(blob, step):
                n += 1
                if w in body:
                    if is_text(w):
                        text += 1
                        continue
                    hits += 1
                    if hits <= 3:
                        print('%s: %s bytes at +0x%X found' % (os.path.basename(path), name, o))
            print('%s: %d of %d windows of the %s found (and %d text windows: string literals of the source)'
                  % (os.path.basename(path), hits, n, name, text))
            bad += hits
    if bad:
        sys.exit('check_payload: game data found in the payload')
    print('check_payload: clean')


if __name__ == '__main__':
    main()
