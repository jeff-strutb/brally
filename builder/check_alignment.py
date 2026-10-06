#!/usr/bin/env python3
"""check_alignment.py -- prove x86-64 game code never assumes 16-byte alignment
of memory it reaches through a pointer.

Both ports keep the original's data layout (Top Gear Rally's N64 addresses in
the arena, Boss Rally's blocks at the original's offsets), which promises only
natural alignment. Legacy SSE instructions with a 16-byte memory operand
(movaps, movdqa, packed arithmetic) fault on a real x86 CPU when the address
is not 16-aligned; Rosetta, and so Wine on a Mac, does not check. The ports
stop clang assuming that alignment of the game's externs (br_crt.h, cc64.sh,
tgr_libc.h); this checks the result. It lists, per object, every such
instruction whose memory operand is a register other than the stack pointer
or frame pointer (stack and RIP-relative constants are aligned by the
compiler), and fails if there is any.

    check_alignment.py OBJ_DIR...    the core's objects (OUT/obj of a build)
"""
import glob
import os
import re
import subprocess
import sys

OPS = re.compile(r'^\s+[0-9a-f]+:\s+(movaps|movapd|movdqa|movntps|movntpd|movntdq|(?:add|sub|mul|div|min|max|and|andn|or|xor|sqrt|cmp|unpckl|unpckh|shuf|round|blendv)p[sd]|cvtdq2ps|cvtps2dq|cvttps2dq|p[a-z0-9]+)\s+(.*)$')
NARROW = re.compile(r'^p(extr|insr|movzx|movsx)')         # scalar-sized memory operands
FN = re.compile(r'^[0-9a-f]+ <(.*)>:$')


def scan(obj):
    out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', obj], capture_output=True, text=True).stdout
    fn, hits = None, []
    for line in out.split('\n'):
        m = FN.match(line)
        if m:
            fn = m.group(1)
            continue
        m = OPS.match(line)
        if not m or NARROW.match(m.group(1)) or '(' not in m.group(2):
            continue
        mem = re.search(r'\([^)]*\)', m.group(2)).group(0)
        if '%rsp' in mem or '%rbp' in mem or '%rip' in mem:
            continue
        hits.append('%s: %s %s' % (fn, m.group(1), m.group(2).strip()))
    return hits


def main():
    dirs = sys.argv[1:]
    if not dirs:
        sys.exit(__doc__)
    bad = n = 0
    for d in dirs:
        for obj in sorted(glob.glob(os.path.join(d, '*.o'))):
            n += 1
            for h in scan(obj):
                bad += 1
                if bad <= 20:
                    print('%s: %s' % (os.path.basename(obj), h))
    print('check_alignment: %d objects, %d alignment-dependent accesses through a pointer' % (n, bad))
    if bad:
        sys.exit(1)


if __name__ == '__main__':
    main()
