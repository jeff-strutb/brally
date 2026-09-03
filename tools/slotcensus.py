#!/usr/bin/env python3
"""Stack-slot census of an original function: who writes each slot, who reads it.

WHY THIS EXISTS.  On 0x1000A110 two display-list appends were both spelled
`specMem` in the transcription.  The original reads TWO DIFFERENT SLOTS at
those two sites -- the first 32-byte pool allocation at one and the second at
the other -- so the source emitted the wrong pointer into the display list.
That defect is invisible to every other tool in the tree:

  * `divergence.py` sees `mov [eax+4],R` in both streams and calls it a match;
  * `msetdiff.py` compares instruction SHAPES, and both sites have the same one;
  * the push census sees nothing, because neither site pushes anything.

Only the slot map sees it.  So: for every value the original homes in a stack
slot, list every read of that slot and check that the source names the SAME
variable at each read.  Run this on any function that allocates or caches more
than one pointer.

    .venv/bin/python3 tools/slotcensus.py build/match/orig/0xADDR.bin
    .venv/bin/python3 tools/slotcensus.py build/match/orig/0xADDR.bin --slot 0x30
    # side by side with the recompile, to see which slots are permuted:
    .venv/bin/python3 tools/slotcensus.py build/match/orig/0xADDR.bin \
        --obj build/match/obj_O2/<file>.obj --sym <SymbolName>

It reports, per `[esp+N]` slot: how many times it is written and read, the
address of each access, and -- the useful part -- the instruction that
PRODUCED the written value (the preceding `call`, or the source operand), so
two allocations of the same size are told apart by which call filled them.

Slots are reported for esp-relative frames only, which is what /O2 emits here
(`ebp` is a general register in these functions).  Argument slots above the
frame are included: the originals reuse dead argument slots as locals, and
that reuse is itself a matching idiom.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

import re

SLOT = re.compile(r'\[esp \+ (0x[0-9a-f]+|\d+)\]')


def disasm(data):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.skipdata = True
    return [(i.address, i.mnemonic, i.op_str) for i in md.disasm(data, 0)]


def census(insns, label):
    """slot -> {'w': [(addr, text, producer)], 'r': [(addr, text)]}"""
    out = {}
    for n, (addr, mn, ops) in enumerate(insns):
        m = SLOT.search(ops)
        if not m:
            continue
        slot = int(m.group(1), 16 if m.group(1).startswith('0x') else 10)
        rec = out.setdefault(slot, {'w': [], 'r': []})
        text = f"{mn} {ops}"
        # a slot is WRITTEN when it is the destination operand
        dst = ops.split(',')[0].strip()
        if SLOT.search(dst) and mn in ('mov', 'movsx', 'movzx', 'fstp', 'fst',
                                       'add', 'sub', 'or', 'and', 'xor', 'inc',
                                       'dec', 'shl', 'shr', 'lea'):
            # what produced it: the nearest preceding call, else the source
            producer = ''
            for k in range(n - 1, max(-1, n - 12), -1):
                if insns[k][1] == 'call':
                    producer = f"after {insns[k][1]} {insns[k][2]} @{insns[k][0]:#x}"
                    break
            rec['w'].append((addr, text, producer))
        else:
            rec['r'].append((addr, text))
    return out


def show(c, label, only=None):
    print(f"=== {label} ===")
    for slot in sorted(c):
        if only is not None and slot != only:
            continue
        w, r = c[slot]['w'], c[slot]['r']
        print(f"[esp+{slot:#04x}]  {len(w)} written, {len(r)} read")
        for addr, text, prod in w:
            print(f"    W {addr:#07x}  {text}" + (f"   <-- {prod}" if prod else ""))
        for addr, text in r:
            print(f"    R {addr:#07x}  {text}")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    orig = open(argv[1], 'rb').read()
    only = None
    if '--slot' in argv:
        only = int(argv[argv.index('--slot') + 1], 16)
    show(census(disasm(orig), 'orig'), 'ORIGINAL ' + os.path.basename(argv[1]), only)
    if '--obj' in argv:
        from match_diff import parse_coff_obj
        obj = argv[argv.index('--obj') + 1]
        sym = argv[argv.index('--sym') + 1]
        rc = parse_coff_obj(obj)[sym][0]
        show(census(disasm(bytes(rc)), 'recomp'), 'RECOMPILE ' + sym, only)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
