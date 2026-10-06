"""staticscreen.py : for each T3 function, the global addresses (lui/%lo pairs) that
no other ROM function references -> function-static candidates; show how the source names them"""
import sys,re,collections
sys.path.insert(0,'tools/tgrally'); import n64build as B, n64t3 as T
rom=B.Rom(); fm=B.function_map()
LS=(0x23,0x2b,0x09,0x21,0x25,0x24,0x20,0x29,0x28,0x31,0x39,0x35,0x3d)
refs=collections.defaultdict(set)
for va,size in fm.items():
    lui={}
    for k in range(0,size,4):
        try: w=rom.word(va+k)
        except Exception: break
        op=w>>26
        if op==0x0f: lui[(w>>16)&31]=w&0xffff
        elif op in LS:
            rs=(w>>21)&31
            if rs in lui:
                hi=lui[rs]; lo=w&0xffff
                if lo&0x8000: lo-=0x10000
                addr=((hi<<16)+lo)&0xffffffff
                if 0x80200000<=addr<0x80400000: refs[addr].add(va)
t3=open(sys.argv[1]).read().split()
for v in t3:
    va=int(v,16)
    own=sorted(a for a,us in refs.items() if us=={va})
    if not own: continue
    p,n,_=T.source_of(va); src=T.function_text(open(p).read(),n)
    names=[]
    for a in own:
        nm='D_%08X'%a
        hit=nm in src or nm.lower() in src
        names.append('%08X%s'%(a,'*' if hit else ''))
    print('%s %-24s %s'%(v,n,' '.join(names)))
