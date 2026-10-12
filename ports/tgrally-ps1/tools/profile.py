#!/usr/bin/env python3
"""profile.py RUNDIR [ELF] -- the PC samples a run took (TGR_PCSAMPLE=n in
run.cfg: pcsamples.bin) attributed to functions, by the ELF's symbols:
the share of samples in each function, and of each function's callers."""
import collections
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
d = sys.argv[1]
elf = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'build/tgrally/ps1/tgrally.elf')
nm = subprocess.run([os.path.join(ROOT, 'build/toolchains/ps1/bin/mipsel-none-elf-nm'), '-n', elf],
                    capture_output=True, text=True).stdout.split('\n')
syms = []
for line in nm:
    f = line.split()
    if len(f) == 3 and f[1] in 'tTwW':
        syms.append((int(f[0], 16), f[2]))
addrs = [a for a, _ in syms]


def name(pc):
    import bisect
    i = bisect.bisect_right(addrs, pc) - 1
    return syms[i][1] if i >= 0 else '?'


data = open(os.path.join(d, 'pcsamples.bin'), 'rb').read()
pairs = [struct.unpack_from('<II', data, i) for i in range(0, len(data), 8)]
fn = collections.Counter(name(pc) for pc, _ in pairs)
n = len(pairs)
print('%d samples' % n)
for f, c in fn.most_common(40):
    print('%6.2f%%  %s' % (100.0 * c / n, f))

# callers (the sampled ra) of the functions named after the run dir
for target in sys.argv[3:]:
    c = collections.Counter(name(ra) for pc, ra in pairs if name(pc) == target)
    print('callers of %s:' % target, ', '.join('%s %d' % kv for kv in c.most_common(8)))
