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

ndiv = 0
first_shown = False
while ia < len(A) and ib < len(B):
    if ieq(A[ia], B[ib]):
        ia += 1; ib += 1
        continue
    ndiv += 1
    show = nctx if ndiv <= 1 else 6
    lo = max(0, ia - 2)
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
    for tot in range(1, 400):
        for da in range(0, tot + 1):
            db = tot - da
            if ia + da + 6 <= len(An) and ib + db + 6 <= len(Bn) and \
               all(ieq(A[ia+da+k], B[ib+db+k]) for k in range(6)):
                best = (da, db)
                break
        if best: break
    if best:
        ia += best[0]; ib += best[1]
        found = True
    if not found:
        print(f"  ... lost sync at orig+{A[ia][0]:#x}")
        break

print(f"\ntotal divergence regions from offset {start_at:#x}: {ndiv}")
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
