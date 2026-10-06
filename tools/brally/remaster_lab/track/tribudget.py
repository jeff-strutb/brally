import sys, json, numpy as np, collections
sys.path.insert(0,"ports/brally-wasm/tools")
import remaster_env_place as P
env=sys.argv[1]
b=open("build/brally/wasm32/app/extract/disc/tracks/mountain.trk","rb").read()
rc,_=P.road_points(b)
A={}; puts=[]
J={}
def info(a,v):
    if a not in J: J[a]=json.load(open(f"ports/common/models/{a}/bake.json"))
    j=J[a]; vv=[x for x in j['variants'] if x['name']==v][0]
    bmin,bmax=np.array(vv['bounds'][0]),np.array(vv['bounds'][1])
    r=np.linalg.norm(bmax-bmin)/2; h=bmax[2]-bmin[2]
    imp=any('impostor' in x for x in j['variants']) or 'impostor' in j
    return [l['tris'] for l in vv['lods']], r, h, imp
for l in open(env):
    f=l.split()
    if not f: continue
    if f[0]=="asset": A[f[1]]=(f[2],f[3])
    elif f[0]=="put":
        a,v=A[f[2]]; M=[float(x) for x in f[3:19]]; s=np.linalg.norm(M[0:3])
        maxd=float(f[19]) if len(f)>19 else 0
        puts.append((a,v,np.array(M[12:15]),s,maxd))
I={}
for a,v,_,_,_ in puts:
    if (a,v) not in I: I[(a,v)]=info(a,v)
pos=np.array([p[2] for p in puts]); 
tot=collections.Counter(); N=0
for e in rc[::150]:
    eye=e+np.array([0,0,2.0]); N+=1
    d=np.linalg.norm(pos-eye,axis=1)
    for k in np.where(d<400)[0]:
        a,v,c,s,maxd=puts[k]
        if maxd>0 and d[k]>maxd: continue
        tris,r,h,imp=I[(a,v)]; r*=s; c2=c+np.array([0,0,h*s*0.5])
        dd=np.linalg.norm(c2-eye)/max(r,1.0)
        lod=0 if dd<2 else 1 if dd<3.5 else 2 if dd<5 else 3
        if lod==3:
            if imp: tot[a]+=2; continue
            lod=2
        tot[a]+=tris[lod]
print(N,"eye points; mean triangles per view (all directions):", round(sum(tot.values())/N/1e6,2),"M")
for a,t in tot.most_common(12): print(f"  {a:34s} {t/N/1e6:.2f} M")
