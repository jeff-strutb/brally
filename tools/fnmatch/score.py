#!/usr/bin/env python3
"""Scorecard for a BrTex3dExpand variant obj: bytes, insns, first-div, multiset gaps."""
import sys, re, os
from collections import Counter
sys.path.insert(0,'tools')
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
obj = sys.argv[1]
orig = open(os.environ.get('BR_ORIG','build/match/orig/0x100250D0.bin'),'rb').read()
rc = parse_coff_obj(obj)['BrTex3dExpand'][0]
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata=True
fd = next((i for i in range(min(len(orig),len(rc))) if orig[i]!=rc[i]), min(len(orig),len(rc)))
R32=r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b'; R16=r'\b(ax|bx|cx|dx|si|di|bp)\b'; R8=r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b'
def norm(t,m):
    t=re.sub(r'esp [+-] 0x[0-9a-f]+','esp+S',t); t=re.sub(r'0x[0-9a-f]+','I',t); t=re.sub(r'\b\d+\b','I',t)
    if m!='raw': t=re.sub(R32,'R',t); t=re.sub(R16,'W',t); t=re.sub(R8,'B',t)
    return t
def bag(c,m): 
    x=Counter()
    for i in md.disasm(c,0): x[norm(f"{i.mnemonic} {i.op_str}",m)]+=1
    return x
o_i=sum(bag(orig,'raw').values()); r_i=sum(bag(rc,'raw').values())
res={}
for m in ('raw','regnorm'):
    O,R=bag(orig,m),bag(rc,m); res[m]=(sum((R-O).values()),sum((O-R).values()))
print(f"BYTES orig=8480 recomp={len(rc)} (+{len(rc)-8480})  INSNS orig={o_i} recomp={r_i} (+{r_i-o_i})")
print(f"FIRSTDIV=+0x{fd:x}  RAW extra={res['raw'][0]} miss={res['raw'][1]}  "
      f"REGNORM extra={res['regnorm'][0]} miss={res['regnorm'][1]}")
