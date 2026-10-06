"""tiescreen.py VA... : for each function, pairs of coloured webs with equal save (same phase) whose colour swap lowers the positional diff"""
import sys,itertools,collections
sys.path.insert(0,'tools/tgrally')
import n64alloc as A
from concurrent.futures import ThreadPoolExecutor
for v in sys.argv[1:]:
    va=int(v,16)
    try:
        g=A.Grader(va,None,aligned=False)
        nd,log,st=g.grade({'CDX_LOG':'1'})
    except SystemExit: continue
    ds=[d for d in A.decisions(log) if d['decision']=='color']
    by=collections.defaultdict(list)
    for d in ds: by[(d['phase'],d['save'])].append((int(d['web']),int(d['bestcolor'])))
    pairs=[]
    for (ph,sv),ws in by.items():
        for (a,ca),(b,cb) in itertools.combinations(ws,2):
            if ca!=cb: pairs.append((ph,a,ca,b,cb))
    def run(p):
        ph,a,ca,b,cb=p
        r,_,_=g.grade({'CDX_FORCE':'%s:w%d=c%d,%s:w%d=c%d'%(ph,a,cb,ph,b,ca)})
        return (r if r is not None else 9999,p)
    with ThreadPoolExecutor(12) as ex: res=sorted(ex.map(run,pairs))
    best=res[0] if res else None
    print('%s base %s pairs %d best %s'%(v,nd,len(pairs),best),flush=True)
