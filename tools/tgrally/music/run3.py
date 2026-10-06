import sys,match2,romsamp,modsamp,stload,numpy as np
keys=sys.argv[1].split(',') if len(sys.argv)>1 else None
st=stload.all_st()
blobs=[(f,ss[0],ss[0]['raw'].astype(np.int8).tobytes()) for f,ss in st.items()]
for mo,m in romsamp.rom_modules():
    for k,R in enumerate(modsamp.xm(m)):
        key='%s#%d'%(mo,k)
        if R['len']<=32 or (keys and key not in keys): continue
        rb=R['raw'].astype(np.int8).tobytes()
        ex=[f for f,D,db in blobs if (rb[:64] in db and (rb in db or db in rb))]
        fz=[]
        if not ex and keys:
            rp=R['pcm'][:12000]
            for f,D,db in blobs:
                if D['len']<0.9*min(len(rp),R['len']): continue
                top=0,0
                for rt in {1.0, round(D['len']/R['len'],4)}:
                    if not (0.9<=rt<6.5): continue
                    Dr=D['pcm'] if rt==1.0 else match2.rs(D['pcm'],rt)
                    if len(Dr)<len(rp): continue
                    sc,_=match2.ncc(rp,Dr); top=max(top,(sc,rt))
                if top[0]>0.9: fz.append((round(top[0],3),top[1],f[5:],D['len']))
            fz.sort(reverse=True)
        print(key,R['name'][:36],R['len'],'| EXACT',[f[5:] for f in ex][:6],'| FUZZY',fz[:4],flush=True)
