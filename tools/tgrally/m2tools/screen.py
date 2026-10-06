"""screen.py t3.txt : per T3 row, count aligned instruction pairs whose destination
register class differs: ROM uopt-class (v0,v1,a0-a3,s0-s8) vs ours ugen temp (t0-t9), and reverse"""
import os,sys,difflib
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T, n64build as B
from concurrent.futures import ThreadPoolExecutor
t3=open(sys.argv[1]).read().split()
work=os.path.join(os.path.dirname(os.path.abspath(__file__)),'tw')
UOPT=set(range(2,8))|set(range(16,24))|{30}
TEMP=set(range(8,16))|{24,25}
def rd(w):
    op=w>>26
    if op==0: return (w>>11)&31
    if op in (0x23,0x21,0x25,0x24,0x20,0x0f,0x09,0x0d,0x0c,0x0a,0x0b,0x0e,0x08): return (w>>16)&31
    return None
def key(w): return (w>>26, w&0x3f if w>>26==0 else 0)
bysrc={}
for v in t3:
    va=int(v,16); p,n,_=T.source_of(va); bysrc.setdefault(p,[]).append((va,n))
def run(item):
    p,fns=item
    src=open(p).read()
    import tempfile
    fd,tmp=tempfile.mkstemp(suffix='.c',dir=work); os.close(fd); open(tmp,'w').write(src)
    obj,err=B.compile_c(tmp); os.unlink(tmp)
    if obj is None: return []
    pieces={n:(s,e) for n,s,e in B.carve(obj) if n}
    rom,fmap,syms=S.ctx(); fnvas={n:v for v,n,_ in B.tags_in(src)}
    out=[]
    for va,n in fns:
        st,nd,notes,ours,theirs=B.grade(obj,rom,n,*pieces[n],va,fmap[va],syms,fnvas)
        sm=difflib.SequenceMatcher(None,[key(w) for w in ours],[key(w) for w in theirs],autojunk=False)
        a=b=0
        for i,j,k in sm.get_matching_blocks():
            for q in range(k):
                ro,rt=rd(ours[i+q]),rd(theirs[j+q])
                if ro is None or rt is None: continue
                if rt in UOPT and ro in TEMP: a+=1
                if ro in UOPT and rt in TEMP: b+=1
        out.append((va,n,nd,len(theirs),a,b))
    return out
with ThreadPoolExecutor(12) as ex:
    rows=[r for rs in ex.map(run,bysrc.items()) for r in rs]
rows.sort(key=lambda r:r[2])
for r in rows: print('%08X %-26s T4 %5d  len %5d  rom-uopt/ours-temp %3d  ours-uopt/rom-temp %3d'%r)
