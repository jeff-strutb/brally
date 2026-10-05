"""Find float literals that are one or two ulps away from the ROM's.

A decimal spelling like 3.6f or 2.943f can round to a neighbour of the
constant the original compiler stored (0x40666666 against the ROM's
0x40666667).  The code then matches instruction for instruction and the
grader cannot see it; only A5 can, and only when a covered call happens
to round differently.  This compiles each source, reads every float and
double in its .rodata, and reports any whose exact bits are not in the
ROM's data while a near neighbour is.

    .venv/bin/python tools/tgrally/n64litcheck.py            # every game source
    .venv/bin/python tools/tgrally/n64litcheck.py FILE.c ...

Print the replacement spelling with
    .venv/bin/python -c "import struct;v=struct.unpack('>f',bytes.fromhex('403c5a1e'))[0];print('%.8g'%v)"
"""
import glob
import os
import struct
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n64build as b


def rom_sets():
    rom = open(b.ROM_PATH, 'rb').read()[b.TEXT_E:b.DATA_E]
    w32 = set(struct.unpack_from('>I', rom, i)[0] for i in range(0, len(rom) - 3, 4))
    w64 = set(struct.unpack_from('>Q', rom, i)[0] for i in range(0, len(rom) - 7, 4))
    return w32, w64


def check(path, w32, w64):
    obj, err = b.compile_c(path)
    if not obj:
        return ['compile failed']
    out = []
    _, ro = obj.sec('.rodata')
    for i in range(0, len(ro) - 3, 4):
        w = struct.unpack_from('>I', ro, i)[0]
        if w == 0 or w in w32 or not 100 < (w >> 23) & 0xff < 160:
            continue
        near = ['%08x' % ((w + d) & 0xffffffff) for d in (-2, -1, 1, 2)
                if (w + d) & 0xffffffff in w32]
        if near:
            out.append('float  .rodata+0x%x %08x (%r), ROM has %s' % (
                i, w, struct.unpack('>f', struct.pack('>I', w))[0], ' '.join(near)))
    for i in range(0, len(ro) - 7, 8):
        q = struct.unpack_from('>Q', ro, i)[0]
        if q == 0 or q in w64 or not 900 < (q >> 52) & 0x7ff < 1150:
            continue
        near = ['%016x' % (q + d) for d in range(-4, 5) if d and q + d in w64]
        if near:
            out.append('double .rodata+0x%x %016x (%r), ROM has %s' % (
                i, q, struct.unpack('>d', struct.pack('>Q', q))[0], ' '.join(near)))
    return out


def main():
    files = sys.argv[1:] or [f for f in glob.glob(os.path.join(b.N64, 'src/**/*.c'), recursive=True)
                             if '/libultra/' not in f and '/zlib/' not in f]
    w32, w64 = rom_sets()
    hits = 0
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        for path, out in zip(files, ex.map(lambda f: check(f, w32, w64), files)):
            for line in out:
                print('%s: %s' % (os.path.relpath(path, b.ROOT), line))
                hits += 1
    print('%d near-miss literal(s) in %d file(s)' % (hits, len(files)))


if __name__ == '__main__':
    main()
