"""pairsweep.py VA [file] : try swapping colours of web pairs (A->B's colour, B->A's colour) for caller-saved webs"""
import sys,itertools
sys.path.insert(0,'tools/tgrally')
import n64alloc as A
from concurrent.futures import ThreadPoolExecutor
va=int(sys.argv[1],16); f=sys.argv[2] if len(sys.argv)>2 and sys.argv[2]!='-' else None
g=A.Grader(va,f,aligned=False)
nd,log,st=g.grade({'CDX_LOG':'1'})
col={}
for d in A.decisions(log):
    if d['decision']=='color':
        c=int(d.get('bestcolor','-1'))
        if 1<=c<=6: col[(d['phase'],int(d['web']))]=c
print('base',nd,len(col),'caller webs',flush=True)
pairs=[(a,b) for a,b in itertools.combinations(sorted(col),2) if col[a]!=col[b] and a[0]==b[0]]
fixed=sys.argv[3].split(',') if len(sys.argv)>3 and sys.argv[3] else []
def run(p):
    a,b=p
    r,_,_=g.grade({'CDX_FORCE':','.join(fixed+['%s:w%d=c%d'%(a[0],a[1],col[b]),'%s:w%d=c%d'%(b[0],b[1],col[a])])})
    return (r if r is not None else 9999,p)
with ThreadPoolExecutor(12) as ex: res=sorted(ex.map(run,pairs))
for r,p in res[:8]: print(r,'%s:w%d=c%d,%s:w%d=c%d'%(p[0][0],p[0][1],col[p[1]],p[1][0],p[1][1],col[p[0]]))
