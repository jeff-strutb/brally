"""Top Gear Rally (N64, USA) ROM analyser.

  TGR_ROM=<path to .z64> .venv/bin/python tools/n64rom.py <cmd>

    stats                 segment/function summary
    func   <vram>         disassemble the function containing <vram>
    dis    <vram> [n]     disassemble n instructions
    xref   <vram>         data xrefs + direct callers
    dumpxref <lo> <hi>    every data xref target in a vram range

Layout (verified): one resident segment, no overlays.
  .text            rom 0x001000-0x070AB0  vram 0x80200000  883 functions
  .data/.rodata    rom 0x070AB0-0x0AD400
  .bss             vram 0x802AC400 + 0xD67B0   (zeroed by the entry stub)
  assets           rom 0x0AD400-0x7DF75A  164 zlib records, loaded by rom offset
"""
import struct,collections,sys,json
from capstone import *
import os
ROM=os.environ.get("TGR_ROM","testdata/Top Gear Rally (USA).z64")
d=open(ROM,'rb').read(); n=len(d)
BASE=0x80200000; ROMOFF=0x1000
TEXT_S,TEXT_E=0x1000,0x70AB0
def v2r(v): return v-BASE+ROMOFF
def r2v(r): return r-ROMOFF+BASE
def W(r): return struct.unpack('>I',d[r:r+4])[0]

# ---------- function map ----------
funcs=set()
calls=collections.defaultdict(set)   # target -> set(callers pc)
for r in range(TEXT_S,TEXT_E,4):
    w=W(r); op=w>>26
    if op==3:
        t=((w&0x03FFFFFF)<<2)|0x80000000
        funcs.add(t); calls[t].add(r2v(r))
# boundaries: jr ra + delay -> next non-nop aligned start
ends=[]
for r in range(TEXT_S,TEXT_E,4):
    if W(r)==0x03E00008: ends.append(r+8)
ends=sorted(set(ends))
starts=set()
for e in ends:
    p=e
    while p<TEXT_E and W(p)==0: p+=4
    if p<TEXT_E: starts.add(r2v(p))
funcs |= starts
funcs.add(BASE)
funcs={f for f in funcs if BASE<=f<r2v(TEXT_E)}
F=sorted(funcs)

def fstart(v):
    import bisect
    i=bisect.bisect_right(F,v)-1
    return F[i] if i>=0 else None

# ---------- data xrefs (lui + lo pair) ----------
xref=collections.defaultdict(list)
lui={}
for r in range(TEXT_S,TEXT_E,4):
    if r2v(r) in funcs: lui={}
    w=W(r); op=w>>26
    rs=(w>>21)&31; rt=(w>>16)&31; imm=w&0xffff
    simm=imm-0x10000 if imm&0x8000 else imm
    if op==0x0F:
        lui[rt]=imm<<16
    elif op==0x09:
        if rs in lui:
            xref[(lui[rs]+simm)&0xffffffff].append(r2v(r))
            if rt==rs: lui.pop(rs,None)
            else: lui.pop(rt,None)
        else: lui.pop(rt,None)
    elif op==0x0D:
        if rs in lui:
            xref[(lui[rs]|imm)&0xffffffff].append(r2v(r))
        lui.pop(rt,None)
    elif op in (0x20,0x21,0x23,0x24,0x25,0x30,0x31,0x35,0x37,0x39,0x3D,0x28,0x29,0x2B,0x2A,0x2E,0x3F):
        if rs in lui: xref[(lui[rs]+simm)&0xffffffff].append(r2v(r))
        if op in (0x20,0x21,0x23,0x24,0x25,0x30,0x37,0x3F): lui.pop(rt,None)
    elif op==0 or op==1 or op==2 or op==3 or (4<=op<=7):
        pass

md=Cs(CS_ARCH_MIPS,CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)
md.detail=False
def dis(v,cnt=40):
    r=v2r(v)
    out=[]
    for i in md.disasm(d[r:r+cnt*4], v):
        out.append("%08X  %-8s %s"%(i.address,i.mnemonic,i.op_str))
    return out

def cmd_stats():
    print("functions: %d"%len(F))
    print("text: %06X-%06X  vram %08X-%08X (%d KB)"%(TEXT_S,TEXT_E,BASE,r2v(TEXT_E),(TEXT_E-TEXT_S)//1024))
    print("distinct data xref targets: %d"%len(xref))

if __name__=="__main__":
    a=sys.argv[1:]
    if not a or a[0]=='stats': cmd_stats()
    elif a[0]=='xref':
        v=int(a[1],16)
        print("xrefs to %08X:"%v)
        for pc in xref.get(v,[]): print("   %08X  (in func %08X)"%(pc,fstart(pc)))
        if v in calls:
            print(" callers:")
            for pc in sorted(calls[v]): print("   %08X  (in func %08X)"%(pc,fstart(pc)))
    elif a[0]=='dis':
        v=int(a[1],16); cnt=int(a[2]) if len(a)>2 else 60
        for l in dis(v,cnt): print(l)
    elif a[0]=='func':
        v=int(a[1],16); s=fstart(v)
        import bisect
        i=bisect.bisect_right(F,s)
        e=F[i] if i<len(F) else r2v(TEXT_E)
        print("func %08X..%08X (%d bytes) callers=%d"%(s,e,e-s,len(calls.get(s,()))))
        for l in dis(s,(e-s)//4): print(l)
    elif a[0]=='dumpxref':
        lo=int(a[1],16); hi=int(a[2],16)
        for v in sorted(xref):
            if lo<=v<hi:
                print("%08X <- %s"%(v,' '.join('%08X'%p for p in xref[v][:8])))
