#!/usr/bin/env python3
"""Structural-signature audit of all diff rows: frame size, epilogue count,
first-push offset, instruction count. Flags rows where these mismatch
(actionable via mid-return / variable-merge idioms) vs pure register residue."""
import csv, os, sys
ROOT='/Users/jeffreywilbur/projects/strutb/brally'
sys.path.insert(0, ROOT+'/tools')
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md=Cs(CS_ARCH_X86,CS_MODE_32); md.skipdata=True

def sig(b):
    frame=0; first_push=None; rets=0; pushes=0; n=0
    for i in md.disasm(bytes(b),0):
        n+=1
        if i.mnemonic=='sub' and i.op_str.startswith('esp, '):
            v=i.op_str.split(', ')[1]
            if i.address<8: frame=int(v,0)
        if i.mnemonic=='push' and i.op_str in ('ebx','ebp','esi','edi'):
            pushes+=1
            if first_push is None: first_push=i.address
        if i.mnemonic=='ret': rets+=1
    return frame, first_push, rets, pushes, n

objcache={}
rows=list(csv.DictReader(open(ROOT+'/build/match/report.csv')))
flagged=[]
for r in rows:
    if r['status']!='diff': continue
    d=int(r['diffs'] or 0)
    if d==0 or d>600: continue
    va=r['va']; f=r['file']
    orig_p=ROOT+f'/build/match/orig/{va}.bin'
    if not os.path.exists(orig_p): continue
    base=os.path.splitext(os.path.basename(f))[0]
    obj=ROOT+'/build/match/obj_O2/'+base+'.obj'
    if not os.path.exists(obj): continue
    if obj not in objcache:
        try: objcache[obj]=parse_coff_obj(obj)
        except Exception: objcache[obj]=None
    funcs=objcache[obj]
    if not funcs or r['name'] not in funcs: continue
    rec,_=funcs[r['name']]
    o=sig(open(orig_p,'rb').read()); c=sig(rec)
    tags=[]
    if o[0]!=c[0]: tags.append(f'FRAME {o[0]:#x}vs{c[0]:#x}')
    if o[2]!=c[2]: tags.append(f'EPILOGUES {o[2]}vs{c[2]}')
    if o[3]!=c[3]: tags.append(f'NPUSH {o[3]}vs{c[3]}')
    if (o[1] is not None and c[1] is not None and abs(o[1]-c[1])>8): tags.append(f'PUSHPOS {o[1]:#x}vs{c[1]:#x}')
    if tags:
        flagged.append((d, va, r['name'], f, ';'.join(tags), o[4], c[4]))
flagged.sort()
for x in flagged: print(*x)
print(len(flagged),'flagged of', sum(1 for r in rows if r['status']=='diff'))
