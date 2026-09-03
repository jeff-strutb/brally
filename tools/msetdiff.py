#!/usr/bin/env python3
"""Register-blind INSTRUCTION-MULTISET diff between original bytes and a recompile.

    .venv/bin/python3 tools/msetdiff.py \
        build/match/orig/<VA>.bin build/match/t3d/<v>.obj <SymbolName> [rows]

Complements tools/divergence.py rather than duplicating it.  divergence.py
aligns the two streams and reports REGIONS, comparing "mnemonic + operand
SHAPE with imm32 wildcarded" -- so it is structurally BLIND to
immediate-operand defects and to code that is simply absent.  This tool
normalises registers (32/16/8-bit), esp displacements and reloc'd operands
but KEEPS small immediates, then diffs the two multisets.  What survives is
either a real constant defect or genuinely missing/extra code.

Proven 2026-09-03 on 0x1000EAF0: it surfaced `cmp R,0x1f4` x4 against our
`cmp R,0x1f3` x4 -- the ring-wrap test was `head >= 500`, not `499 < head`.
Those four sites had scored as MATCHING for eight passes of region-based
grinding; fixing them dropped the reloc-masked byte diff 3,855 -> 3,727.
Run this on any function that has stalled: a region count that will not move
while the instruction count is off is exactly the signature it catches.

2026-09-03, ninth pass on 0x1000EAF0: the first version drowned its own
signal.  Branch/call rel32 targets were compared literally (every later
target rotates when any earlier region changes size) and, worse, the
recompile stores the ADDEND in a reloc'd field, so capstone prints
`push 0` / `[R]` / `[R + 4]` where the linked original prints
`push 0x106e9a38` / `[R + 0x1035faf0]` -- every reloc'd instruction paired
as MISSING+EXTRA.  Both are normalised now: the same comparison fell from
76 to 37 real differences, and two apparent "we pass 0 where the original
passes a pointer" defects turned out to be artefacts of the old spelling.
Never act on a bare `push 0` / zero-displacement row without checking the
reloc list first.
"""
import sys, os, re, collections
sys.path.insert(0, os.path.join(os.getcwd(),'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
BRANCH = re.compile(r'^(j[a-z]+|call|loop[a-z]*)$')
def norm(i, relocd):
    s = i.op_str
    # A rel8/rel32 target is a pure BYTE-OFFSET artefact: any earlier region
    # that changes size rotates every later target and floods the diff.  The
    # aligned comparator (divergence.py) is what judges control flow.
    if BRANCH.match(i.mnemonic) and re.fullmatch(r'0x[0-9a-f]+', s):
        return i.mnemonic + ' T'
    s = re.sub(r'esp \+ 0x[0-9a-f]+', 'esp+S', s)
    s = re.sub(r'\*(1|2|4|8)\b', '*K', s)
    s = re.sub(r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b', 'R', s)
    s = re.sub(r'\b(ax|bx|cx|dx|si|di|bp)\b', 'W', s)
    s = re.sub(r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b', 'B', s)
    if relocd or re.search(r'0x1[0-9a-f]{7}', s):
        s = re.sub(r'0x[0-9a-f]+', 'A', s)
        s = re.sub(r'\[(\d+)\]', '[A]', s)
        s = re.sub(r'\[([A-Z]\*K) \+ \d+\]', r'[\1 + A]', s)
        # In the RECOMPILE the reloc'd field holds the ADDEND, so capstone
        # prints `push 0` / `[R]` / `[R + 4]` where the linked original
        # prints an absolute 0x10xxxxxx.  Without this the two sides never
        # pair up and every reloc'd instruction shows as MISSING+EXTRA.
        if 'A' not in s:
            if re.fullmatch(r'0', s):
                s = 'A'
            elif re.search(r',\s*0$', s):
                s = re.sub(r',\s*0$', ', A', s)
            elif '[' in s:
                def _abs(m):
                    inner = re.sub(r'\s*\+\s*(0x[0-9a-f]+|\d+)$', '', m.group(1))
                    return '[' + inner + ' + A]'
                s = re.sub(r'\[([^]]*)\]', _abs, s, count=1)
    return i.mnemonic + ' ' + s
def load(p, sym):
    if p.endswith('.bin'):
        d, rel = open(p,'rb').read(), set()
    else:
        d, rel = parse_coff_obj(p)[sym]
        rel = set(rel)
    c = collections.Counter()
    ins = list(md.disasm(d, 0))
    # trailing inter-function alignment padding is not part of the function
    while ins and ins[-1].mnemonic in ('nop', 'int3'):
        ins.pop()
    for i in ins:
        rd = any(o in rel for o in range(i.address, i.address + i.size))
        c[norm(i, rd)] += 1
    return c
if __name__ == '__main__':
    o = load(sys.argv[1], None); r = load(sys.argv[2], sys.argv[3])
    n = int(sys.argv[4]) if len(sys.argv) > 4 else 18
    miss, extra = o - r, r - o
    print("orig-only (MISSING), %d:" % sum(miss.values()))
    for k,v in miss.most_common(n): print("   %2d  %s" % (v,k))
    print("recomp-only (EXTRA), %d:" % sum(extra.values()))
    for k,v in extra.most_common(n): print("   %2d  %s" % (v,k))
