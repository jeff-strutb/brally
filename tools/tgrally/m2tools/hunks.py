"""hunks.py VA draft.c [--regs] [--ctx N] : structural hunks of a long function.

Aligns our listing against the ROM's on the opcode sequence (registers
masked unless --regs) with difflib and prints every non-equal hunk with
ROM addresses, the nearest preceding call in each listing as an anchor, and
both sides' instructions.  For walking a giant region by region."""
import difflib
import os
import re
import subprocess
import sys

args = sys.argv[1:]
va, draft = args[0], args[1]
regs = '--regs' in args
loose = '--loose' in args
ctx = int(args[args.index('--ctx') + 1]) if '--ctx' in args else 2
out = subprocess.run([sys.executable, os.environ.get("DVTOOL", os.path.join(os.path.dirname(__file__), "dv.py")), va, draft] + os.environ.get("DVARGS", "").split(),
                     capture_output=True, text=True).stdout.splitlines()[1:]
ours, rom = [], []
for l in out:
    if '|' not in l:
        continue
    a, b = l.split('|', 1)
    addr = a[:8]
    o = a[12:].strip()
    r = b.strip()
    if o:
        ours.append(o)
    if r:
        rom.append((addr, r))
base = int(va, 16)
rom = [(('%08X' % (base + 4 * i)), r) for i, (_, r) in enumerate(rom)]


def key(s):
    s = re.sub(r'\s+', ' ', s)
    if not regs:
        s = re.sub(r'\$\w+', '$', s)
    s = re.sub(r'0x[0-9a-f]{6,8}', 'ADDR', s)      # branch/jal targets move with length
    if loose and s.split()[0] in ('lui', 'lw', 'lwc1', 'addiu', 'sw', 'swc1', 'lbu', 'lhu', 'sb', 'sh', 'lb', 'lh', 'ldc1'):
        s = re.sub(r'-?0x[0-9a-f]+|\b\d+\b', 'N', s)
    return s


A = [key(x) for x in ours]
B = [key(x) for _, x in rom]
sm = difflib.SequenceMatcher(None, A, B, autojunk=False)


def anchor(lst, i, getter):
    for k in range(i, -1, -1):
        if getter(lst[k]).startswith('jal'):
            return '%d:%s' % (k, getter(lst[k]))
    return '-'


n = 0
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'equal':
        continue
    n += 1
    print('=== hunk %d %s ours[%d:%d] rom[%d:%d] @%s  anchor rom %s' % (
        n, tag, i1, i2, j1, j2, rom[j1][0] if j1 < len(rom) else 'end', anchor(rom, max(j1 - 1, 0), lambda x: x[1])))
    lo_o, lo_r = max(i1 - ctx, 0), max(j1 - ctx, 0)
    hi = max(i2 - lo_o, j2 - lo_r) + ctx
    for k in range(hi):
        o = ours[lo_o + k] if lo_o + k < min(i2 + ctx, len(ours)) else ''
        r = '%s %s' % rom[lo_r + k] if lo_r + k < min(j2 + ctx, len(rom)) else ''
        print('  %-40s | %s' % (o, r))
print('hunks', n, 'ours', len(ours), 'rom', len(rom))
