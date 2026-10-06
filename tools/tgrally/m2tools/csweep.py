"""csweep.py VA [file] : greedy force sweep over caller-saved colours (c1-c12) for every web"""
import sys,os,re,itertools
sys.path.insert(0,'tools/tgrally')
import n64alloc as A
from concurrent.futures import ThreadPoolExecutor
va=int(sys.argv[1],16); f=sys.argv[2] if len(sys.argv)>2 and sys.argv[2]!='-' else None
g=A.Grader(va,f,aligned=False)
nd,log,st=g.grade({'CDX_LOG':'1'})
webs=sorted({int(d['web']) for d in A.decisions(log) if d['phase']=='p1'})
fixed=[]
base=nd
print('base',base,len(webs),'webs',flush=True)
for rnd in range(int(sys.argv[3]) if len(sys.argv)>3 else 3):
    keys=['p1:w%d=c%d'%(w,c) for w in webs for c in list(range(1,13))]+['p1:w%d=s'%w for w in webs]
    def run(k):
        r,_,_=g.grade({'CDX_FORCE':','.join(fixed+[k])})
        return (r if r is not None else 9999,k)
    with ThreadPoolExecutor(12) as ex: res=sorted(ex.map(run,keys))
    if res[0][0]>=base: print('no improvement'); break
    base,k=res[0]; fixed.append(k); print('+',k,'->',base,flush=True)
    if base==0: print('EXACT',fixed); break
