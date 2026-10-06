import sys,glob,match2,romsamp,modsamp,pickle
want=set(sys.argv[2].split(',')) if len(sys.argv)>2 else None
donors=match2.load_donors(sorted(glob.glob(sys.argv[1])))
print(len(donors),'donor files',sum(len(v) for v in donors.values()),'samples',flush=True)
for mo,m in romsamp.rom_modules():
    for k,R in enumerate(modsamp.xm(m)):
        key='%s#%d'%(mo,k)
        if R['len']<=32 or (want and key not in want): continue
        b=match2.match(R,donors)
        print(key,R['name'][:36],R['len'],'|',' ; '.join('%s %.3f %s#%d %s %db %dHz len%d'%(t[1],t[0],t[2].split('/')[-1],t[3],t[4]['name'][:28],t[4]['bits'],t[4]['c5'] or 0,t[4]['len']) for t in b[:4]),flush=True)
