#!/usr/bin/env python3
"""Compare the ARGUMENT STREAM of every call, original against recompile.

    .venv/bin/python3 tools/pushcensus.py \
        build/match/orig/<VA>.bin build/match/t3d/<v>.obj <SymbolName>

Why this exists: the other two comparators are BLIND to a wrong constant in
a call argument.  `divergence.py` wildcards imm32 when it normalises, and
`msetdiff.py` compares multisets, so a PERMUTATION of the right tokens
passes both.  On an emit-heavy function -- anything that builds a display
list -- that is exactly the defect you have: sixteen combiner tokens in the
wrong slots emit a wrong display list while every tool reports shape noise.

Proven 2026-09-03 on 0x1000A110 (BrCarDrawVehicle).  Two of its thirty-four
calls had their tokens permuted; fixing them closed FOUR divergence regions
(29 -> 25) and, more to the point, fixed the display list the function
actually emits.  A third call was short two pushes, which is how the
model-DL hook's per-arm structure was found.

Reading the output: cdecl pushes right-to-left, so within one call the Nth
push is argument (nargs + 1 - N) and the LAST push is argument 1.  A push of
a register is printed <R> -- in these functions that is usually the zero
register, so <R> normally means a literal 0 in the source.  A push whose
field carries a relocation, or an absolute 0x10xxxxxx, is printed <A>: those
cannot be compared across a linked original and an object file, so they are
deliberately collapsed to one token on both sides.

Grouping is by call boundary and is naive -- an argument stream interrupted
by a nested call is split.  Both sides are grouped the same way, so a
mismatch in the GROUP COUNT means the two builds do not agree on their call
structure and the per-group report below it is not meaningful; fix that
first.
"""
import sys, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True


def groups(data, relocs=None):
    """[(first push address, [normalised operand, ...]), ...], one per call."""
    out, cur, start = [], [], None
    for i in md.disasm(data, 0):
        if i.mnemonic == 'push':
            if start is None:
                start = i.address
            op = i.op_str
            relocd = relocs is not None and any(
                k in relocs for k in range(i.address, i.address + i.size))
            if relocd or (op.startswith('0x1') and len(op) == 10):
                op = '<A>'
            elif not op.startswith('0x'):
                op = '<R>'
            cur.append(op)
        elif i.mnemonic == 'call':
            if cur:
                out.append((start, cur))
            cur, start = [], None
    if cur:
        out.append((start, cur))
    return out


def load(path, sym):
    if path.endswith('.bin'):
        return groups(open(path, 'rb').read())
    data, rel = parse_coff_obj(path)[sym]
    return groups(data, set(rel))


def main():
    go = load(sys.argv[1], None)
    gr = load(sys.argv[2], sys.argv[3])
    print("call groups: orig %d  recomp %d" % (len(go), len(gr)))
    if len(go) != len(gr):
        print("‼ group counts differ -- the two builds disagree on call "
              "structure; the per-group report below is not meaningful.")
    bad = 0
    for k, (a, b) in enumerate(zip(go, gr)):
        if a[1] == b[1]:
            continue
        bad += 1
        kind = ("COUNT %d vs %d" % (len(a[1]), len(b[1]))
                if len(a[1]) != len(b[1]) else
                "PERMUTED" if collections.Counter(a[1]) == collections.Counter(b[1])
                else "VALUES")
        print("\ngroup %-3d orig@%05x recomp@%05x   %s" % (k, a[0], b[0], kind))
        print("   orig   " + ' '.join(a[1]))
        print("   recomp " + ' '.join(b[1]))
        if len(a[1]) == len(b[1]):
            n = len(a[1])
            for idx, (x, y) in enumerate(zip(a[1], b[1])):
                if x != y:
                    print("     arg %-2d  orig %-8s recomp %s" % (n - idx, x, y))
    print("\ngroups differing: %d of %d" % (bad, len(go)))


if __name__ == '__main__':
    main()
