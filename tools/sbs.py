#!/usr/bin/env python3
"""Side-by-side disassembly of a recompiled function against its original.

    .venv/bin/python tools/sbs.py <obj> <symbol> <VA>

<obj> is the sweep's object (build/match/obj_<opt>/<file>.obj, opt = the
report.csv column for that function; fn.py writes obj_O2), <symbol> the C
name (matched exactly first, then as a substring -- a port twin named
`<sym>_port` will otherwise win), <VA> the original's address, which keys
build/match/orig/<VA>.bin.  Rows are paired by index, so after the first
divergence the pairing is positional; read the flagged lines for WHERE the
shape differs, not for what.  Reloc'd operands show as [0] on the recomp
side; ignore those rows.

This is the instrument that closed nine T1 functions in one lane where
fn.py's EXTRA/MISSING summary alone had misled twice.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj            # noqa: E402
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        sys.exit(2)
    obj, sym, va = sys.argv[1:4]
    d = parse_coff_obj(obj)
    keys = [x for x in d if x == sym or x == '_' + sym] or [x for x in d if sym in x]
    if not keys:
        sys.exit(f'{sym}: not in {obj}; symbols: {sorted(d)}')
    code = d[keys[0]][0]
    orig = open(os.path.join(ROOT, 'build', 'match', 'orig', f'{va}.bin'), 'rb').read()
    print(len(code), len(orig), keys[0])
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.skipdata = True
    ins = list(md.disasm(code, 0))
    oi = list(md.disasm(orig, 0))
    for i in range(max(len(ins), len(oi))):
        a = ins[i] if i < len(ins) else None
        b = oi[i] if i < len(oi) else None
        left = f"{a.address:#05x}: {a.mnemonic:8s}{a.op_str:30s}" if a else ' ' * 46
        right = f"{b.address:#05x}: {b.mnemonic:8s}{b.op_str}" if b else ''
        same = a and b and a.mnemonic == b.mnemonic and a.op_str == b.op_str
        print(left + ' | ' + right + ('' if same else '  <<<'))


if __name__ == '__main__':
    main()
