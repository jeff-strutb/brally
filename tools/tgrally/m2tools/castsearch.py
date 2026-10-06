"""castsearch.py VA base.c [--rounds N] : greedy search wrapping operand occurrences
(identifiers and integer literals in expressions) in casts; keeps an edit when the
(aligned, blind, positional) score improves."""
import os,sys,re
sys.path.insert(0,'tools/tgrally')
import n64search as S, n64t3 as T
from concurrent.futures import ThreadPoolExecutor
va=int(sys.argv[1],16); path,name,_=T.source_of(va)
tree=open(path).read(); tbody=T.function_text(tree,name)
cur=open(sys.argv[2]).read().strip('\n')
rounds=int(sys.argv[sys.argv.index('--rounds')+1]) if '--rounds' in sys.argv else 4
CASTS=['(unsigned char)','(signed char)','(short)','(unsigned short)','(int)','(unsigned int)']
if '--float' in sys.argv: CASTS=['(float)','(double)']
if '--wide' in sys.argv: CASTS=['(int)','(unsigned int)']
work=os.path.join(os.path.dirname(os.path.abspath(__file__)),'tw')
KW={'if','else','while','for','return','int','char','short','unsigned','signed','void','float','struct','do','break','goto','continue','switch','case','default','sizeof','long','double'}
def sites(body):
    i=body.index('{')
    out=[]
    for m in re.finditer(r'\b([A-Za-z_]\w*|\d+|0x[0-9a-fA-F]+)\b',body):
        if m.start()<i: continue
        w=m.group(1)
        if w in KW: continue
        line=body[body.rfind('\n',0,m.start())+1:body.find('\n',m.start())]
        if re.match(r'\s*(int|char|short|unsigned|signed|float|struct|\w+ \*)\b',line) and ';' in line and '=' not in line: continue
        after=body[m.end():m.end()+3]
        if after.lstrip().startswith('(') : continue   # a call
        if re.match(r'(\[[^\]]*\])*\s*(=[^=]|\+\+|--|[-+*/&|^]=)',body[m.end():]): continue  # an lvalue
        if body[m.start()-1:m.start()] in ('.','>') : continue  # a field name
        out.append((m.start(),m.end()))
    return out
def g0(b): return S.grade(tree.replace(tbody,b),va,name,work)
KEY=sys.argv[sys.argv.index('--key')+1] if '--key' in sys.argv else 'aligned'
def g(b):
    r=g0(b)
    if r is None: return None
    if KEY=='sum': return (r[0]+r[2],)+r
    if KEY=='pos': return (r[2],r[0],r[1],r[2])
    return r
best=g(cur); print('start',best,flush=True)
for r in range(rounds):
    cands=[]
    for s,e in sites(cur):
        for c in CASTS:
            cands.append(cur[:s]+c+cur[s:e]+cur[e:])
    with ThreadPoolExecutor(12) as ex: res=list(ex.map(g,cands))
    sc=[(x,b) for x,b in zip(res,cands) if x]
    sc.sort(key=lambda t:t[0])
    print('round',r+1,len(cands),'best',sc[0][0] if sc else None,flush=True)
    if not sc or sc[0][0]>=best: break
    best,cur=sc[0]
    open(sys.argv[2]+'.cast.c','w').write(cur)
    if best[2]==0 or best==(0,0,0): break
print('final',best)
