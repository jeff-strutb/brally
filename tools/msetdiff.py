#!/usr/bin/env python3
"""Register-blind INSTRUCTION-MULTISET diff between original bytes and a recompile.

    .venv/bin/python3 tools/msetdiff.py \
        build/match/orig/<VA>.bin build/match/t3d/<v>.obj <SymbolName> [rows] \
        [--orig-range LO-HI] [--recomp-range LO-HI]

‼ THE RANGE FLAGS ARE HOW YOU SEE INSIDE A LOST-SYNC GAP.  divergence.py
cannot compare a block that holds no `--key` consecutive matching
instructions; on 0x100250D0 that hid 1,093 bytes (12.9%) behind one
"NEVER COMPARED" line for five sessions.  A windowed multiset over the same
two offsets reads it straight off: pass the ORIGINAL's offsets and the
RECOMPILE's (they differ by the accumulated delta -- divergence.py's
re-anchor line prints both).  The header line reports each side's
instruction count and their difference, which is where a missing-code
verdict actually comes from.  Proven 2026-09-03: orig 0x15b8-0x19fd against
recomp 0x15b4-0x19ce gave 274 vs 270 insns and 11 MISSING / 7 EXTRA, ten of
which were a known allocation swap and one an extra `jmp`.

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
def norm(i, relocd, tail_reloc=None):
    """`relocd`: some byte of the instruction is a reloc.  `tail_reloc`: the
    reloc sits in the LAST FOUR BYTES (the imm32 when the instruction has an
    immediate operand, else the disp32).  None = unknown, old behaviour."""
    s = i.op_str
    # A rel8/rel32 target is a pure BYTE-OFFSET artefact: any earlier region
    # that changes size rotates every later target and floods the diff.  The
    # aligned comparator (divergence.py) is what judges control flow.
    # capstone prints small targets as bare decimal (`call 8`); accept both
    # forms or an intra-obj call to a file-static pairs with nothing.
    if BRANCH.match(i.mnemonic) and re.fullmatch(r'(0x[0-9a-f]+|\d+)', s):
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
        #
        # 2026-09-09: `mov dword ptr [ebx + 0xffc], <reloc>` fell through the
        # `'A' not in s` guard below -- the DISPLACEMENT had already become A,
        # so the reloc'd immediate stayed `0` and the row paired with nothing
        # (0x1005FF00: a phantom 1+1 that failed T3 gate A3 on identical
        # bytes).  When the caller says the reloc is in the tail and the
        # instruction has an immediate operand, that immediate IS the reloc.
        if relocd and tail_reloc and re.search(r'(^|, )0$', s):
            s = re.sub(r'(^|, )0$', r'\1A', s)
        elif 'A' not in s:
            def _abs(m):
                inner = re.sub(r'\s*\+\s*(0x[0-9a-f]+|\d+)$', '', m.group(1))
                return '[' + inner + ' + A]'
            # 2026-09-09 (second): the mirror of the tail case above.  In
            # `mov dword ptr [R + <reloc disp32>], 0` the reloc sits in the
            # DISPLACEMENT (not the last four bytes), the addend is 0 so
            # capstone prints a bare `[R]`, and the old imm-first order
            # rewrote the true immediate 0 to A instead -- the row then
            # paired with nothing and failed gate A3 on identical bytes
            # (0x1006C290, unpaired `mov [M], A` vs `mov [A], 0`).  When
            # the caller SAYS the reloc is not in the tail, the memory
            # operand is the reloc'd field.
            if tail_reloc is False and '[' in s:
                s = re.sub(r'\[([^]]*)\]', _abs, s, count=1)
            elif re.fullmatch(r'0', s):
                s = 'A'
            elif re.search(r',\s*0$', s):
                s = re.sub(r',\s*0$', ', A', s)
            elif '[' in s:
                s = re.sub(r'\[([^]]*)\]', _abs, s, count=1)
    return i.mnemonic + ' ' + s
def load(p, sym, lo=0, hi=None):
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
    n = 0
    for i in ins:
        if i.address < lo or (hi is not None and i.address >= hi):
            continue
        rd = any(o in rel for o in range(i.address, i.address + i.size))
        tail = rd and i.size >= 4 and (i.address + i.size - 4) in rel
        c[norm(i, rd, tail)] += 1
        n += 1
    return c, n
def _range(arg):
    """`0x15b8-0x19fd` -> (0x15b8, 0x19fd); `0x15b8-` -> (0x15b8, None)."""
    a, _, b = arg.partition('-')
    return int(a, 0), (int(b, 0) if b else None)
if __name__ == '__main__':
    argv = sys.argv[1:]
    orng = rrng = (0, None)
    if '--orig-range' in argv:
        k = argv.index('--orig-range'); orng = _range(argv[k+1]); del argv[k:k+2]
    if '--recomp-range' in argv:
        k = argv.index('--recomp-range'); rrng = _range(argv[k+1]); del argv[k:k+2]
    sys.argv = [sys.argv[0]] + argv
    o, no = load(sys.argv[1], None, *orng)
    r, nr = load(sys.argv[2], sys.argv[3], *rrng)
    n = int(sys.argv[4]) if len(sys.argv) > 4 else 18
    if orng != (0, None) or rrng != (0, None):
        print("windowed: orig %d insns, recomp %d insns (%+d)" % (no, nr, nr - no))
    miss, extra = o - r, r - o
    print("orig-only (MISSING), %d:" % sum(miss.values()))
    for k,v in miss.most_common(n): print("   %2d  %s" % (v,k))
    print("recomp-only (EXTRA), %d:" % sum(extra.values()))
    for k,v in extra.most_common(n): print("   %2d  %s" % (v,k))
