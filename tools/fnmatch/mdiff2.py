#!/usr/bin/env python3
"""Multiset diff with selectable register normalization.
usage: mdiff2.py <obj> <sym> [mode] [n]   mode: raw|regnorm|widthnorm"""
import sys, re, os
from collections import Counter
sys.path.insert(0,'tools')
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

obj, sym = sys.argv[1], sys.argv[2]
mode = sys.argv[3] if len(sys.argv)>3 else 'regnorm'
orig = open(os.environ.get('BR_ORIG','build/match/orig/0x100250D0.bin'),'rb').read()
recomp = parse_coff_obj(obj)[sym][0]
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata=True

R32 = r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b'
R16 = r'\b(ax|bx|cx|dx|si|di|bp)\b'
R8  = r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b'

def norm(txt, mode):
    t = re.sub(r'esp [+-] 0x[0-9a-f]+', 'esp+S', txt)
    t = re.sub(r'0x[0-9a-f]+', 'I', t)
    t = re.sub(r'\b\d+\b', 'I', t)
    if mode in ('regnorm','widthnorm'):
        t = re.sub(R32, 'R', t); t = re.sub(R16, 'W', t); t = re.sub(R8, 'B', t)
    if mode == 'widthnorm':
        t = re.sub(r'\b[WB]\b', 'R', t)
        t = re.sub(r'\b(byte|word|dword) ptr\b', 'ptr', t)
        t = re.sub(r'\bmovzx\b|\bmovsx\b', 'mov', t)
    return t

def bag(code):
    c = Counter()
    for i in md.disasm(code, 0):
        c[norm(f"{i.mnemonic} {i.op_str}", mode)] += 1
    return c

O, Rr = bag(orig), bag(recomp)
extra, miss = Rr - O, O - Rr
print(f"mode={mode}  orig={sum(O.values())} recomp={sum(Rr.values())}  "
      f"EXTRA={sum(extra.values())} MISSING={sum(miss.values())} "
      f"NET={sum(Rr.values())-sum(O.values())}")
n = int(sys.argv[4]) if len(sys.argv)>4 else 25
if n:
    print("--- EXTRA ---")
    for k,v in extra.most_common(n): print(f"  +{v:3}  {k}")
    print("--- MISSING ---")
    for k,v in miss.most_common(n): print(f"  -{v:3}  {k}")
