"""neqscreen.py t3.txt : per T3 row, try each `A != B` (in if/while/ternary) as `A - B` and `B - A`
one site at a time; report sites that lower the positional diff"""
import os,sys,re
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T
from concurrent.futures import ThreadPoolExecutor
t3=open(sys.argv[1]).read().split()
work=os.path.join(os.path.dirname(os.path.abspath(__file__)),'tw'); os.makedirs(work,exist_ok=True)
PAT=re.compile(r'(\((?:[^()]|\([^()]*\))*?)\s!=\s')
def sites(body):
    # find "X != Y" where X and Y are simple operands (identifiers, members, indexes, calls w/o nesting)
    op=r'[A-Za-z_][\w\.\->\[\]]*(?:\[[^\]]*\])*(?:\([^()]*\))?'
    for m in re.finditer(r'(%s) != (%s)'%(op,op),body):
        yield m
jobs=[]
for v in t3:
    va=int(v,16)
    try: p,n,_=T.source_of(va)
    except Exception: continue
    tree=open(p).read(); body=T.function_text(tree,n)
    if len(body.splitlines())>400: continue
    for m in sites(body):
        a,b=m.group(1),m.group(2)
        for rep in ('%s - %s'%(a,b),'%s - %s'%(b,a)):
            nb=body[:m.start()]+rep+body[m.end():]
            jobs.append((va,n,tree,body,nb,m.group(0),rep))
def run(j):
    va,n,tree,body,nb,orig,rep=j
    return j,S.grade(tree.replace(body,nb),va,n,work)
base={}
def runb(v):
    va=int(v,16); p,n,_=T.source_of(va); tree=open(p).read()
    return va,S.grade(tree,va,n,work)
vas=sorted({j[0] for j in jobs})
with ThreadPoolExecutor(12) as ex:
    for va,g in ex.map(runb,['%08X'%x for x in vas]): base[va]=g
    for j,g in ex.map(run,jobs):
        b=base[j[0]]
        if g and b and g[2]<b[2]:
            print('%08X %-24s %s -> %s  [%s] -> [%s]'%(j[0],j[1],b[2],g[2],j[5],j[6]),flush=True)
