#!/usr/bin/env python3
"""Divergence regions between original bytes and a recompiled function.

The working tool for grinding a large function to byte-exact: aligns the
two instruction streams, compares byte-for-byte with the recomp's reloc
positions masked (and rel32 branch targets), resyncs after each mismatch,
and prints every divergence region with context.  Region count is the
progress bar; the raw byte-diff count is useless once slot offsets shift.

Usage:
    .venv/bin/python3 tools/divergence.py \
        build/match/obj_O2/<file>.obj build/match/orig/0xADDR.bin \
        <SymbolName> [context] [--at 0xOFF] [--mask-slots]

--mask-slots additionally treats [esp+X] displacements as equal, which
separates stack-slot-layout noise from real divergence: run both and the
difference between the two counts is the layout cascade.

--key N sets how many consecutive matching instructions count as a resync
(default 6).  ‼ SIX IS TOO SHORT FOR A FUNCTION BUILT FROM REPEATED ARMS.
The key has to be longer than the longest sequence that repeats, or a resync
lands on the WRONG copy and every delta after it is fiction.  Measured
2026-09-03 on 0x100250D0 (twelve near-identical channel arms, all ending in
the same divide-by-255 fixup): at key 6 it reports 32 regions with FOUR
flagged SUSPECT (skews 74/53, 12/34, 26/135, 115/20); at key 10 it reports
TWENTY with one, and the fictional "+393 bytes in one block" is gone.  The
default stays 6 so that every region count quoted in the dossiers keeps its
meaning -- but on any function with repeated arms, read it at --key 10 and
SAY WHICH KEY the number came from.  Going further (14) is too coarse: the
resync starts skipping whole arms and swallows real regions.

‼ AND A SUSPECT RESYNC POISONS THE TWO REGIONS AFTER IT, not just its own
line.  `change` is a difference of two deltas, so once a resync lands on the
wrong copy the next region's delta is wrong and the two `change` values
computed from it are fiction.  Both are now labelled.  Do not grind a block
whose change carries either warning.

VERIFY A BIG CHANGE BEFORE GRINDING IT.  Pick an instruction that occurs once
per unit of work and count it over the WHOLE function in both streams -- on
0x100250D0 the divide-by-255 magic constant (33 in each) and the one-operand
`imul` (24 in each) prove no channel is missing, which is what showed a -73
"missing block" to be an anchor artefact after two sessions had believed it.
An equal census plus an honest instruction total means the residue is
allocation, wherever the region map points.

--deltas replaces the per-region listing with one line per region giving how
far the recompile has drifted behind (negative) or ahead of the original by
that point, and how much of the drift the PRECEDING stretch added.  That
per-region CHANGE is the triage number a region count cannot give: on
0x1000EAF0 it says at a glance that one block loses 56 bytes and the next
gains 58 back (the x87 scale-block batching), and that another single block
loses 15 (the ring-index addressing form) -- while the other eighteen
regions move only a few bytes each.  Start a stalled function here: grind
the block with the largest |change|, not the first region in the list.

Built and proven on 0x1000EAF0 (br_scenedl.c), 2026-08-24.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

obj_path, orig_path, sym = sys.argv[1], sys.argv[2], sys.argv[3]
nctx = int(sys.argv[4]) if len(sys.argv) > 4 and not sys.argv[4].startswith('--') else 12
start_at = 0
if '--at' in sys.argv:
    start_at = int(sys.argv[sys.argv.index('--at') + 1], 16)

orig = open(orig_path, 'rb').read()
funcs = parse_coff_obj(obj_path)
recomp, relocs = funcs[sym]

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.skipdata = True

def dis(data, relocset):
    out = []
    for i in md.disasm(data, 0):
        # normalized text: mask imm32 that sits in a reloc'd byte range
        txt = f"{i.mnemonic} {i.op_str}"
        masked = txt
        if relocset is not None:
            for off in range(i.address, i.address + i.size):
                if off in relocset:
                    # blank out the 4-byte field containing the reloc
                    masked = f"{i.mnemonic} <RELOC>{i.op_str.split(',')[-1] if False else ''}"
                    break
        out.append((i.address, i.size, i.bytes, txt, masked, i.mnemonic))
    return out

A = dis(orig, None)
B = dis(recomp, relocs)

def norm(entry, is_recomp):
    addr, size, bs, txt, masked, mn = entry
    # mask absolute addresses (relocs) and rel32 branch targets:
    # compare mnemonic + operand SHAPE with imm32 wildcarded
    import re
    t = txt
    # call/jmp rel32 -> mask target
    if mn in ('call', 'jmp') and size == 5:
        t = mn + ' <T>'
    # mask 0x10xxxxxx absolute addrs (orig) -- recomp relocs are 0
    t = re.sub(r'0x10[0-9a-f]{6}\b', '<G>', t)
    if is_recomp:
        # recomp reloc fields read as 0; mask "dword ptr [0]" and bare 0 imms
        # only when the instruction contains a reloc
        addr0 = addr
        for off in range(addr0, addr0 + size):
            if off in relocs:
                t = re.sub(r'\[0\]', '[<G>]', t)
                t = re.sub(r'\[(e[a-z]{2}) \+ 0\]', r'[\1 + <G>]', t)
                t = re.sub(r', 0$', ', <G>', t)
                t = re.sub(r'^push 0$', 'push <G>', t)
                break
    return t

MASK_SLOTS = '--mask-slots' in sys.argv

def ieq(ea, eb):
    if MASK_SLOTS:
        import re as _re
        ta = _re.sub(r'\[esp \+ (0x[0-9a-f]+|\d+)\]', '[esp+S]', ea[3])
        tb = _re.sub(r'\[esp \+ (0x[0-9a-f]+|\d+)\]', '[esp+S]', eb[3])
        if '[esp+S]' in ta or '[esp+S]' in tb:
            if ea[5] != eb[5] or ea[1] != eb[1]:
                return False
            # compare bytes except displacement/imm fields: fall back to text
            import re as _re2
            ta2 = _re2.sub(r'0x10[0-9a-f]{6}\b', '<G>', ta)
            tb2 = tb
            for off in range(eb[0], eb[0]+eb[1]):
                if off in relocs:
                    tb2 = _re2.sub(r'\[0\]','[<G>]',tb2); tb2=_re2.sub(r', 0$',', <G>',tb2)
                    tb2 = _re2.sub(r'0x10[0-9a-f]{6}\b','<G>',tb2)
                    break
            tb2 = _re2.sub(r'0x10[0-9a-f]{6}\b', '<G>', tb2)
            if ea[5].startswith('j') or ea[5]=='call':
                return True
            return ta2 == tb2

    # byte equality with recomp reloc positions masked; orig abs-addr bytes
    # are exactly the reloc'd fields of the recomp when code matches
    if ea[1] != eb[1]:
        return False
    ba, bb = ea[2], eb[2]
    for k in range(ea[1]):
        if (eb[0] + k) in relocs:
            continue
        # mask rel32 of call/jmp (targets differ between obj and image)
        if ea[5] in ('call','jmp','ja','jae','jb','jbe','je','jne','jg','jge','jl','jle','js','jns') and k >= ea[1]-4 and ea[1]>=5:
            continue
        if k>0 and ea[1]==2 and ea[5].startswith('j'):
            continue
        if ba[k] != bb[k]:
            return False
    return True

class EqList:
    def __init__(self, lst, other):
        self.lst=lst
    def __len__(self): return len(self.lst)

An = A
Bn = B

ia = ib = 0
# skip to start_at
while ia < len(A) and A[ia][0] < start_at:
    ia += 1
while ib < len(B) and B[ib][0] < start_at:
    ib += 1

DELTAS = '--deltas' in sys.argv
KEY = int(sys.argv[sys.argv.index('--key') + 1]) if '--key' in sys.argv else 6

ndiv = 0
prev_delta = 0
first_shown = False
while ia < len(A) and ib < len(B):
    if ieq(A[ia], B[ib]):
        ia += 1; ib += 1
        continue
    ndiv += 1
    show = nctx if ndiv <= 1 else 6
    lo = max(0, ia - 2)
    if DELTAS:
        # How far the recompile has drifted BEHIND (negative) or AHEAD of the
        # original by this point, and how much of that drift the preceding
        # stretch added.  The per-region CHANGE is the triage number: it says
        # which single block is losing or gaining the bytes, which the region
        # count alone cannot.
        delta = B[ib][0] - A[ia][0]
        pending_delta = (ndiv, A[ia][0], B[ib][0], delta, delta - prev_delta)
        prev_delta = delta
    else:
        print(f"=== divergence #{ndiv} at orig+{A[ia][0]:#x} recomp+{B[ib][0]:#x} ===")
        for k in range(lo, ia):
            print(f"  = {A[k][0]:6x}  {A[k][3]}")
        for k in range(ia, min(ia + show, len(A))):
            print(f"  O {A[k][0]:6x}  {A[k][3]}")
        print("  --")
        for k in range(ib, min(ib + show, len(B))):
            print(f"  R {B[k][0]:6x}  {B[k][3]}")
    # resync: find next place where 6 consecutive normalized insns match,
    # minimizing da+db so we stay tight
    found = False
    best = None
    # The key length and the order candidates are tried both decide whether a
    # resync lands on the RIGHT copy of a repeated arm.  Two rules:
    #   * longer key = more unique.  `--key N` raises it; 6 is the historical
    #     default and every region count quoted in the dossiers assumes it.
    #   * within one total displacement, try the BALANCED splits first.  The
    #     old loop ran da from 0 upward, so it preferred the most lopsided
    #     split available at that total -- exactly the shape that locks onto
    #     the wrong copy (skews like 26/135 and 115/20).
    for tot in range(1, 400):
        cands = sorted(range(0, tot + 1), key=lambda d: (abs(d - (tot - d)), d))
        for da in cands:
            db = tot - da
            if ia + da + KEY <= len(An) and ib + db + KEY <= len(Bn) and \
               all(ieq(A[ia+da+k], B[ib+db+k]) for k in range(KEY)):
                best = (da, db)
                break
        if best: break
    if best:
        # A resync that skips very different numbers of instructions on the
        # two sides has probably locked onto the WRONG copy of a repeated
        # arm -- these functions are full of near-identical blocks, and the
        # six-instruction key is not unique.  The drift it reports is then
        # an artefact, usually visible as a large change immediately undone
        # by a near-equal opposite one.  Say so rather than let it be read
        # as a real block.
        skew = abs(best[0] - best[1])
        if DELTAS:
            n, oa, rb, d, ch = pending_delta
            warn = f"  <-- SUSPECT: resync skew {best[0]}/{best[1]} insns" if skew > 20 else ""
            # A region that FOLLOWS a suspect resync is measured from a bad
            # anchor, so its own change is fiction too even when its own
            # resync is clean.  Say so: this trap cost a session, which
            # believed a -73 that a whole-function instruction census then
            # showed could not exist.
            if not warn and globals().get('suspect_carry', 0) > 0:
                warn = "  <-- change unreliable: a preceding resync was SUSPECT"
            print(f"region {n:2d}  orig+{oa:#07x}  recomp+{rb:#07x}"
                  f"   delta={d:+d}  (change {ch:+d}){warn}")
        # A bad resync corrupts the DELTA at the next region, and `change`
        # is a difference of two deltas -- so the two regions after it are
        # both fiction, not just one.
        if skew > 20:
            globals()['suspect_carry'] = 2
        else:
            globals()['suspect_carry'] = max(0, globals().get('suspect_carry', 0) - 1)
        ia += best[0]; ib += best[1]
        found = True
    if not found:
        if DELTAS:
            n, oa, rb, d, ch = pending_delta
            print(f"region {n:2d}  orig+{oa:#07x}  recomp+{rb:#07x}"
                  f"   delta={d:+d}  (change {ch:+d})")
        print(f"  ... lost sync at orig+{A[ia][0]:#x}")
        break

print(f"\ntotal divergence regions from offset {start_at:#x}: {ndiv}"
      f"  (resync key {KEY} insns)")
# The COFF function extent is padded to a 16-byte boundary, so the recompile
# ends in up to 15 alignment nops that the extracted original does not have.
# Counting them made three different dossiers record "instruction counts are
# EQUAL" for functions that were actually 6 and 15 instructions SHORT --
# report the real code length.
npad = 0
while npad < len(B) and B[len(B) - 1 - npad][5] in ('nop', 'int3'):
    npad += 1
nB = len(B) - npad
print(f"orig insns: {len(A)}  recomp insns: {nB}"
      + (f" (+{npad} pad)" if npad else "")
      + f"  orig bytes {len(orig)} recomp {len(recomp) - npad}"
      + (f" (+{npad} pad)" if npad else ""))
