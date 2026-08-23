#!/usr/bin/env python3
"""Scan the ORIGINAL bytes corpus and derive each global's access profile:
width (1/2/4/8), signedness hint (movsx/movzx), and floatness (x87 dword/
qword loads). Writes build/orig_global_widths.csv — ground truth for the
matching pipeline's extern declarations (better than Ghidra's undefined4)."""
import glob, os, csv, collections
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM, X86_OP_IMM

DATA_LO, DATA_HI = 0x10077000, 0x11900000
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True

prof = collections.defaultdict(lambda: collections.Counter())

FLOAT_MNEMS = {'fld','fst','fstp','fadd','fsub','fsubr','fmul','fdiv','fdivr','fcom','fcomp'}
INT_X87 = {'fild','fistp','fist','fisttp'}

for path in glob.glob('build/match/orig/*.bin'):
    b = open(path,'rb').read()
    va = int(os.path.basename(path)[:-4], 16)
    for i in md.disasm(b, va):
        for op in i.operands:
            if op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.segment == 0:
                addr = op.mem.disp & 0xffffffff
                if not (DATA_LO <= addr < DATA_HI):
                    continue
                p = prof[addr]
                m = i.mnemonic
                if m in FLOAT_MNEMS:
                    p['f%d' % op.size] += 1
                elif m in INT_X87:
                    p['i%d' % op.size] += 1
                elif m == 'movsx':
                    p['s%d' % op.size] += 1
                elif m == 'movzx':
                    p['z%d' % op.size] += 1
                else:
                    p['w%d' % op.size] += 1
                if op.mem.index != 0:
                    p['indexed'] += 1
            elif op.type == X86_OP_IMM:
                a = op.imm & 0xffffffff
                if DATA_LO <= a < DATA_HI:
                    prof[a]['addrof'] += 1

def decide(p):
    f4 = p.get('f4',0); f8 = p.get('f8',0)
    if f8 and f8 >= f4: return 'double'
    if f4: return 'float'
    sizes = {sz for sz in (1,2,4)
             if any(p.get(k % sz,0) for k in ('w%d','s%d','z%d','i%d'))}
    if sizes == {1}: return 'char' if p.get('s1') else 'unsigned char'
    if sizes == {2}: return 'short' if p.get('s2') else 'unsigned short'
    if not sizes: return ''            # address-taken only
    if sizes == {4}: return 'int'
    return 'int'                        # mixed — widest wins

os.makedirs('build', exist_ok=True)
with open('build/orig_global_widths.csv','w',newline='') as fh:
    w = csv.writer(fh)
    w.writerow(['addr','ctype','profile'])
    for addr in sorted(prof):
        p = prof[addr]
        w.writerow(['0x%08X' % addr, decide(p),
                    ' '.join('%s:%d' % kv for kv in sorted(p.items()))])
print(len(prof), 'global addresses profiled')
import collections as C
c=C.Counter(decide(p) for p in prof.values())
print(c.most_common())
