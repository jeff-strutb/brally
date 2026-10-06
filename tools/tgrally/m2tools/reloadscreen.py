"""reloadscreen.py t3list : per T3 function, global addresses (lui/%lo) where the ROM has more
lui instructions for that address than ours (ROM re-forms the address per access; ours hoists)."""
import os,sys,tempfile,collections
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T, n64build as B
from concurrent.futures import ThreadPoolExecutor
LS=(0x23,0x2b,0x09,0x21,0x25,0x24,0x20,0x29,0x28,0x31,0x39,0x35,0x3d)
def addr_luis(words):
    lui={}; cnt=collections.Counter(); seen=set()
    for k,w in enumerate(words):
        op=w>>26
        if op==0x0f: lui[(w>>16)&31]=(k,w&0xffff)
        elif op in LS:
            rs=(w>>21)&31
            if rs in lui:
                k0,hi=lui[rs]; lo=w&0xffff
                if lo&0x8000: lo-=0x10000
                a=((hi<<16)+lo)&0xffffffff
                if (k0,a) not in seen: seen.add((k0,a)); cnt[a]+=1
    return cnt
t3=open(sys.argv[1]).read().split()
work=os.path.join(os.path.dirname(os.path.abspath(__file__)),'tw')
bysrc={}
for v in t3:
    va=int(v,16); p,n,_=T.source_of(va); bysrc.setdefault(p,[]).append((va,n))
def run(item):
    p,fns=item
    src=open(p).read()
    fd,tmp=tempfile.mkstemp(suffix='.c',dir=work); os.close(fd); open(tmp,'w').write(src)
    obj,err=B.compile_c(tmp); os.unlink(tmp)
    if obj is None: return []
    pieces={n:(s,e) for n,s,e in B.carve(obj) if n}
    rom,fmap,syms=S.ctx(); fnvas={n:v for v,n,_ in B.tags_in(src)}
    out=[]
    for va,n in fns:
        st,nd,notes,ours,theirs=B.grade(obj,rom,n,*pieces[n],va,fmap[va],syms,fnvas)
        r=addr_luis(theirs); o=addr_luis(ours)
        # ours addresses are relocated? compare by count of lui per address in rom vs total ours luis that resolve
        diffs=[(hex(a),r[a]) for a in r if r[a]>=2 and 0x80200000<=a<0x80400000]
        out.append((va,n,nd,diffs,sum(r.values()),sum(o.values())))
    return out
with ThreadPoolExecutor(12) as ex:
    rows=[r for rs in ex.map(run,bysrc.items()) for r in rs]
for va,n,nd,d,rt,ot in sorted(rows,key=lambda r:r[2]):
    if rt>ot+1: print('%08X %-24s T4 %4d rom-lui %3d ours-lui %3d  multi: %s'%(va,n,nd,rt,ot,' '.join('%s x%d'%x for x in d[:6])))
