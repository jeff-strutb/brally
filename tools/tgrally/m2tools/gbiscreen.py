"""gbiscreen.py t3.txt [--all] : per T3 row, rewrite gbi macro statements (gSP*/gDP*/gRaw/...) whose expansion is
one Gfx (w0, w1) as explicit blocks, one word per line (w0 first, and w1 first); try each site singly and all at once"""
import os,sys,re,subprocess,tempfile
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T
from concurrent.futures import ThreadPoolExecutor
t3=open(sys.argv[1]).read().split()
work=os.path.join(os.path.dirname(os.path.abspath(__file__)),'tw'); os.makedirs(work,exist_ok=True)
GBI=open('src/tgrally/include/tgr/gbi.h').read()
CALL=re.compile(r'^(\s*)(g(?:SP|DP|Raw|MoveWd|Dma1p)\w*)\((.*)\);\s*$')
def expand(stmt):
    fd,p=tempfile.mkstemp(suffix='.c',dir=work); os.close(fd)
    open(p,'w').write(GBI+'\n@@@\n'+stmt+'\n')
    out=subprocess.run(['clang','-E','-P','-x','c',p],capture_output=True,text=True).stdout
    os.unlink(p)
    return out.split('@@@',1)[1] if '@@@' in out else None
def block(ind,exp):
    m=re.search(r'Gfx \*_g = (.*?);\s*_g->words\.w0 = (.*?);\s*_g->words\.w1 = (.*?);\s*\}',exp,re.S)
    if not m or exp.count('_g->words.w0')!=1: return None
    pk,w0,w1=[re.sub(r'\s+',' ',x).strip() for x in m.groups()]
    a=ind+'{\n'+ind+'  Gfx *_g = %s;\n\n'%pk
    return [a+ind+'  _g->words.w0 = %s;\n'%w0+ind+'  _g->words.w1 = %s;\n'%w1+ind+'}',
            a+ind+'  _g->words.w1 = %s;\n'%w1+ind+'  _g->words.w0 = %s;\n'%w0+ind+'}']
jobs=[]; base={}
for v in t3:
    va=int(v,16)
    try: p,n,_=T.source_of(va)
    except Exception: continue
    tree=open(p).read(); body=T.function_text(tree,n)
    if len(body.splitlines())>500: continue
    lines=body.split('\n'); sites=[]
    for i,l in enumerate(lines):
        m=CALL.match(l)
        if not m: continue
        e=expand(l.strip())
        if not e: continue
        bl=block(m.group(1),e)
        if bl: sites.append((i,bl))
    if not sites: continue
    for i,bl in sites:
        for k,b in enumerate(bl):
            L=list(lines); L[i]=b
            jobs.append((va,n,tree,body,'\n'.join(L),'line %d form %d'%(i,k)))
    L=list(lines)
    for i,bl in sites: L[i]=bl[0]
    jobs.append((va,n,tree,body,'\n'.join(L),'all w0-first (%d sites)'%len(sites)))
def run(j):
    va,n,tree,body,nb,tag=j
    return j,S.grade(tree.replace(body,nb),va,n,work)
def runb(va):
    p,n,_=T.source_of(va); return va,S.grade(open(p).read(),va,n,work)
vas=sorted({j[0] for j in jobs})
print(len(jobs),'variants over',len(vas),'rows',flush=True)
with ThreadPoolExecutor(12) as ex:
    for va,g in ex.map(runb,vas): base[va]=g
    for j,g in ex.map(run,jobs):
        b=base[j[0]]
        if g and b and g[2]<b[2]:
            print('%08X %-24s %s -> %s  %s'%(j[0],j[1],b[2],g[2],j[5]),flush=True)
