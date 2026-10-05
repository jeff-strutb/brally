#!/usr/bin/env python3
"""printfcheck.py -- the port's sprintf (platform/libc/xprintf.c) against the
ROM's own (libultra's, at 0x80260DD4, run in n64/tools/n64box.py's CPU) over
generated formats and values: every format the game's source uses, with
random and edge-case arguments, and random formats besides.

    printfcheck.py [--n N] [--seed S]

Prints each case that differs (at most 20) and the totals.
"""
import argparse
import glob
import os
import random
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(ROOT, 'n64/tools'))
import tgrbox as n64box  # noqa: E402  (the port's view of the box: tools/tgrbox.py)

SPRINTF = 0x80260DD4
RET = 0x80000200            # a word of zeros (a nop) the call returns to
FMT, DST, STR, SP = 0x807E0000, 0x807E1000, 0x807E2000, 0x807F0000


def rom_sprintf(box, fmt, kind, arg):
    uc, sx = box.uc, n64box.sx
    box.write(FMT, fmt.encode('latin1') + b'\0')
    box.write(DST, b'\xAA' * 256)
    regs = {4: DST, 5: FMT}
    if kind == 'd':
        regs[6] = arg & 0xFFFFFFFF
    elif kind == 'f':
        b = struct.pack('>d', arg)
        regs[6], regs[7] = struct.unpack('>II', b)       # a double: the a2:a3 pair
    else:
        box.write(STR, arg.encode('latin1') + b'\0')
        regs[6] = STR
    for r, v in regs.items():
        uc.reg_write(n64box.GPR[r], sx(v))
    uc.reg_write(n64box.GPR[29], sx(SP))
    uc.reg_write(n64box.GPR[31], sx(RET))
    uc.emu_start(sx(SPRINTF), sx(RET), count=2000000)
    return box.read(DST, 256).split(b'\0')[0].decode('latin1')


def source_formats():
    """the format strings the game's sprintf calls use, with their first
    conversion's kind"""
    out = set()
    for p in glob.glob(os.path.join(ROOT, 'ports/tgrally/src/**/*.c'), recursive=True):
        for m in re.finditer(r'sprintf\([^,]+,\s*"((?:[^"\\]|\\.)*)"', open(p, errors='replace').read()):
            f = m.group(1).encode().decode('unicode_escape')
            convs = re.findall(r'%[-+ #0]*\d*(?:\.\d+)?[hlL]?([diouxXcsfeEgG%])', f)
            convs = [c for c in convs if c != '%']
            if len(convs) == 1:
                out.add((f, convs[0]))
    return sorted(out)


def kind_of(c):
    return 'f' if c in 'feEgG' else 's' if c == 's' else 'd'


def values(kind, rnd, n):
    if kind == 'd':
        edge = [0, 1, -1, 9, 10, 99, 100, -100, 12345, 0x7FFFFFFF, -0x80000000, 0xFFFF, 255]
        return edge + [rnd.randint(-2**31, 2**31 - 1) for _ in range(n)] + [rnd.randint(-1000, 1000) for _ in range(n)]
    if kind == 's':
        return ['', 'a', 'Top Gear Rally', 'x' * 40]
    edge = [0.0, -0.0, 0.5, 0.49999, 0.5000001, 0.6, 0.95, 0.999, 1.0, 1.5, 2.5, 9.5, 9.995, 99.5,
            0.125, 0.0625, 1e-5, 123456.789, 1e10, 1e20, -0.5, -1.5, 3.14159265, 1609.344, 60.0,
            float('inf'), float('-inf'), float('nan')]
    return edge + [rnd.uniform(-1000, 1000) for _ in range(n)] + [rnd.uniform(0, 2) for _ in range(n)] + \
        [rnd.expovariate(0.01) for _ in range(n)] + [round(rnd.uniform(0, 200), 2) for _ in range(n)]


def random_formats(rnd, n):
    out = []
    for _ in range(n):
        c = rnd.choice('diuxXoceEfgGs')
        f = '%' + ''.join(rnd.sample(' +-#0', rnd.randint(0, 2)))
        if rnd.random() < 0.6:
            f += str(rnd.randint(0, 12))
        if rnd.random() < 0.6:
            f += '.' + str(rnd.randint(0, 9))
        if c in 'diuxXo' and rnd.random() < 0.3:
            f += rnd.choice('hl')
        out.append(('<' + f + c + '>', c))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--n', type=int, default=40)
    ap.add_argument('--seed', type=int, default=1)
    a = ap.parse_args()
    rnd = random.Random(a.seed)
    drv = os.path.join(ROOT, 'build/tgrally/printfcheck')
    subprocess.run(['clang', '-O2', '-I', os.path.join(ROOT, 'ports/tgrally/platform/include'),
                    os.path.join(HERE, 'printfcheck/driver.c'),
                    os.path.join(ROOT, 'ports/tgrally/platform/libc/xprintf.c'), '-o', drv], check=True)
    cases = []
    for f, c in source_formats() + random_formats(rnd, 300):
        k = kind_of(c)
        for v in values(k, rnd, a.n):
            cases.append((f, k, v))
    lines = []
    for f, k, v in cases:
        arg = struct.pack('>d', v).hex() if k == 'f' else str(v)
        lines.append('%s\t%s\t%s' % (f, k, arg))
    port = subprocess.run([drv], input='\n'.join(lines) + '\n', capture_output=True, text=True,
                          encoding='latin1').stdout.split('\n')
    port = [bytes.fromhex(x).decode('latin1') for x in port]
    box = n64box.Box()
    bad = 0
    for i, (f, k, v) in enumerate(cases):
        r = rom_sprintf(box, f, k, v)
        if r != port[i]:
            bad += 1
            if bad <= 20:
                print('%r %r: rom %r port %r' % (f, v, r, port[i]))
    print('%d cases, %d differ' % (len(cases), bad))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
