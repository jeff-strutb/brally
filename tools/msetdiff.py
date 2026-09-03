#!/usr/bin/env python3
"""Register-blind INSTRUCTION-MULTISET diff between original bytes and a recompile.

    .venv/bin/python3 tools/msetdiff.py \
        build/match/orig/<VA>.bin build/match/t3d/<v>.obj <SymbolName>

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
"""
import sys, os, re, collections
sys.path.insert(0, os.path.join(os.getcwd(),'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
def norm(i, relocd):
    s = i.op_str
    s = re.sub(r'esp \+ 0x[0-9a-f]+', 'esp+S', s)
    s = re.sub(r'\*(1|2|4|8)\b', '*K', s)
    s = re.sub(r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b', 'R', s)
    s = re.sub(r'\b(ax|bx|cx|dx|si|di|bp)\b', 'W', s)
    s = re.sub(r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b', 'B', s)
    if relocd or re.search(r'0x1[0-9a-f]{7}', s):
        s = re.sub(r'0x[0-9a-f]+', 'A', s)
        s = re.sub(r'\[(\d+)\]', '[A]', s)
        s = re.sub(r'\[([A-Z]\*K) \+ \d+\]', r'[\1 + A]', s)
    return i.mnemonic + ' ' + s
def load(p, sym):
    if p.endswith('.bin'):
        d, rel = open(p,'rb').read(), set()
    else:
        d, rel = parse_coff_obj(p)[sym]
        rel = set(rel)
    c = collections.Counter()
    for i in md.disasm(d, 0):
        rd = any(o in rel for o in range(i.address, i.address + i.size))
        c[norm(i, rd)] += 1
    return c
o = load(sys.argv[1], None); r = load(sys.argv[2], sys.argv[3])
miss, extra = o - r, r - o
print("orig-only (MISSING), %d:" % sum(miss.values()))
for k,v in miss.most_common(18): print("   %2d  %s" % (v,k))
print("recomp-only (EXTRA), %d:" % sum(extra.values()))
for k,v in extra.most_common(18): print("   %2d  %s" % (v,k))
