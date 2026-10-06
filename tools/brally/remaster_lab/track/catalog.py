import numpy as np, collections, json, base64, io
from PIL import Image
from trkparse import parse
from render import render
D='build/brally/wasm32/app/extract/disc/tracks/'
TR=[('amazon','Amazon'),('coast','Coast'),('desert','Desert'),('mine','Mine'),('mountain','Mountain'),('race','Race (circuit)'),('bonus','Bonus stage'),('gamewin','Game-win scene')]
def b64(im,q=78):
    f=io.BytesIO(); Image.fromarray(im).save(f,'WEBP',quality=q); return base64.b64encode(f.getvalue()).decode()
out=[]
for tr,title in TR:
    texs,insts=parse(D+tr+'.trk')
    talpha={}
    for k,t in texs.items():
        im=t['img']; talpha[k]=float((im[...,3]<128).mean()) if im is not None else 0.0
    groups=collections.defaultdict(list)
    root=None
    for I in insts:
        if I['i']<0: root=I; continue
        sc=np.linalg.norm(I['M'][0,:3]); A=collections.Counter(); up=xlu=tot=0; pts=[]
        for P,UV,k,rm in I['tris']:
            n=np.cross(P[1]-P[0],P[2]-P[0]); a=np.linalg.norm(n)/2
            if a==0: continue
            A[k]+=a; tot+=a; up+=a*(abs(n[2])/(2*a)>0.8); xlu+=a*bool(rm&0x4000); pts.append(P*sc)
        if not tot: continue
        pts=np.concatenate(pts); dims=pts.max(0)-pts.min(0)
        prim=A.most_common(1)[0][0]
        cut=sum(a*(talpha[k]>0.08 or '_CI' in k) for k,a in A.items())/tot
        groups[prim].append(dict(I=I,sc=sc,tris=len(I['tris']),up=up/tot,xlu=xlu/tot,cut=cut,dims=dims,keys=list(A)))
    items=[]
    for k,G in groups.items():
        n=len(G); tris=sum(g['tris'] for g in G)
        up=np.mean([g['up'] for g in G]); xlu=np.mean([g['xlu'] for g in G]); cut=np.mean([g['cut'] for g in G])
        tpi=tris/n
        if xlu>0.5: cat='overlay'
        elif cut>0.5 and tpi<=24: cat='card'
        elif up>0.7 and cut<=0.5: cat='ground'
        elif cut>0.5: cat='cutstruct'
        else: cat='solid'
        rep=max(G,key=lambda g:g['tris'])
        tris_r=[(P*rep['sc'],UV,kk) for P,UV,kk,rm in rep['I']['tris']]
        thumb=b64(render(tris_r,texs,120))
        ti=texs[k]['img']
        sw=None
        if ti is not None:
            im=Image.fromarray(ti,'RGBA'); s=48/max(im.size); im=im.resize((max(1,round(im.size[0]*s)),max(1,round(im.size[1]*s))),Image.NEAREST)
            bg=Image.new('RGBA',im.size,(255,0,255,255)); bg.alpha_composite(im); sw=b64(np.array(bg.convert('RGB')),90)
        md=np.median([g['dims'] for g in G],axis=0)
        fmt=k.split('_')[1]; wh=k.split('_')[2]
        items.append(dict(cat=cat,tex=k,fmt=fmt,wh=wh,n=n,tris=tris,dims=[round(x,1) for x in md.tolist()],
                          alpha=round(talpha[k],2),thumb=thumb,sw=sw,ids=[g['I']['i'] for g in G][:40]))
    items.sort(key=lambda x:(-x['n']*1.0-x['tris']*0.02))
    rt=[(P,UV,kk) for P,UV,kk,rm in root['tris']] if root else []
    out.append(dict(id=tr,title=title,ninst=sum(len(G) for G in groups.values()),ntex=len(texs),
                    root=dict(tris=len(rt),thumb=b64(render(rt,texs,120,view=(0.3,-1,0.6))) if rt else None),items=items))
    c=collections.Counter(i['cat'] for i in items); print(tr,dict(c))
json.dump(out,open('catalog.json','w'))
