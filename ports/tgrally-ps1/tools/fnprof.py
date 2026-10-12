#!/usr/bin/env python3
"""fnprof.py STDERR [ELF] -- the last -finstrument-functions report (perf: fn
ADDR CALLS INCL EXCL) with the ELF's names, by exclusive time."""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
elf = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'build/tgrally/ps1/tgrally.elf')
names = {}
for line in subprocess.run([os.path.join(ROOT, 'build/toolchains/ps1/bin/mipsel-none-elf-nm'), elf],
                           capture_output=True, text=True).stdout.split('\n'):
    f = line.split()
    if len(f) == 3 and f[1] in 'tTwW':
        names[int(f[0], 16)] = f[2]
rows = []
for line in open(sys.argv[1], errors='replace'):
    if line.startswith('perf: retraces'):
        rows = []
    f = line.split()
    if line.startswith('perf: fn ') and len(f) == 6:
        a = int(f[2], 16)
        rows.append((int(f[5]), int(f[4]), int(f[3]), names.get(a, hex(a))))
key = 1 if '--incl' in sys.argv else 0
for e, i, n, nm in sorted(rows, key=lambda r: -r[key])[:int(os.environ.get('N', 60))]:
    print('%6d excl %6d incl %7d calls %6.0f us/call  %s' % (e, i, n, 1000.0 * i / max(n, 1), nm))
