"""addrrefs.py LO HI : ROM functions that reference addresses in [LO,HI) via lui/%lo pairs"""
import sys,collections
sys.path.insert(0,'tools/tgrally'); import n64build as B, n64t3 as T
rom=B.Rom(); fm=B.function_map()
LS=(0x23,0x2b,0x09,0x21,0x25,0x24,0x20,0x29,0x28,0x31,0x39,0x35,0x3d)
lo_,hi_=int(sys.argv[1],16),int(sys.argv[2],16)
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
                a=((hi<<16)+lo)&0xffffffff
                if lo_<=a<hi_: refs[a].add(va)
for a in sorted(refs):
    print('%08X'%a, ' '.join('%08X'%v for v in sorted(refs[a]))[:150])
