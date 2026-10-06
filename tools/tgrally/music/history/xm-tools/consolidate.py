import glob,json,re,pickle,os,numpy as np,modsamp,romsamp,stload
NAMES=json.load(open('ma_names.json'))
RIPS={'134660','142976','142098','140031','142685','142382','141885'}
def label(path):
    b=os.path.basename(path)
    if path.startswith('stxx/'): return 'Amiga ST-XX disks: '+path[5:]
    if path.startswith('modland/files/'): return 'Modland: '+b.replace('_',' ',0)
    if path.startswith('modland:'): return 'Modland: '+path[8:]
    i=b.split('.')[0]
    if i in NAMES: return 'Mod Archive #%s %s'%(i,NAMES[i][0])
    return b
# ROM samples
R=[]
for mo,m in romsamp.rom_modules():
    for k,s in enumerate(modsamp.xm(m)):
        if s['len']>32: R.append(('%s#%d'%(mo,k),s))
# local donors
files=[f for f in glob.glob('donors/*.bin')+glob.glob('d4/*.bin')+glob.glob('d5/*.bin')+glob.glob('modland/files/*') if os.path.basename(f).split('.')[0] not in RIPS and 'Barry_Leitch_tgr' not in f]
D=[]
for f in sorted(set(files)):
    try:
        for j,s in enumerate(modsamp.load(open(f,'rb').read())):
            if s['len']>32: D.append((f,j,s))
    except Exception: pass
for f in glob.glob('stxx/**/*',recursive=True):
    if os.path.isfile(f) and os.path.getsize(f)>32:
        try: D.append((f,0,stload.load_st(f)))
        except Exception: pass
Db=[(f,j,s,((np.asarray(s['raw'])>>8) if s['bits']==16 else np.asarray(s['raw'])).astype(np.int8).tobytes()) for f,j,s in D]
out={}
for key,s in R:
    rb=s['raw'].astype(np.int8).tobytes()
    ex=[]
    for f,j,d,db in Db:
        if rb[:64] in db and (rb in db or db in rb):
            ex.append(dict(src=label(f),j=j,name=d['name'][:40],bits=d['bits'],len=d['len'],rate=round(d['c5'] or 0),kind='exact' if len(db)==len(rb) else ('exact, source longer' if len(db)>len(rb) else 'exact, source shorter')))
    out[key]=dict(name=s['name'],len=s['len'],rate=round(s['c5']),loop=s['loop'] is not None,exact=ex,fuzzy=[])
# fuzzy from earlier runs
for mo,k,name,ln,b in pickle.load(open('match.pkl','rb')):
    for sc,kind,did,j,dn,bits,c5,dl in b:
        if kind!='EXACT' and sc>=0.9 and did not in RIPS and dl>=0.4*ln:
            out['%s#%d'%(mo,k)]['fuzzy'].append(dict(src=label(did+'.bin'),j=j,name=dn[:40],bits=bits,len=dl,rate=round(c5 or 0),corr=round(sc,3),ratio=float(kind[1:])))
pat=re.compile(r'x([\d.]+) ([\d.]+) (\S+)#(\d+) (.*?) (\d+)b (\d+)Hz len(\d+)')
for f in glob.glob('out[2456]/*.txt'):
    for line in open(f):
        if ' | ' not in line: continue
        key=line.split()[0]
        for part in line.split(' | ',1)[1].split(' ; '):
            m=pat.search(part)
            if not m or m.group(3).split('.')[0] in RIPS or 'tgr' in m.group(3): continue
            ratio,sc,did,j,dn,bits,rate,dl=m.groups()
            if float(sc)>=0.9 and int(dl)>=0.4*out[key]['len']:
                src=label(('modland/files/' if 'Fasttracker' in did or 'Protracker' in did else '')+did)
                out[key]['fuzzy'].append(dict(src=src,j=int(j),name=dn.strip()[:40],bits=int(bits),len=int(dl),rate=int(rate),corr=round(float(sc),3),ratio=float(ratio)))
for f in glob.glob('out3/*.txt'):
    for line in open(f):
        key=line.split()[0]; m=re.search(r"FUZZY \[(.*)\]",line)
        if m and m.group(1):
            for sc,rt,p,dl in re.findall(r"\(([\d.]+), ([\d.]+), '([^']+)', (\d+)\)",m.group(1)):
                out[key]['fuzzy'].append(dict(src='Amiga ST-XX disks: '+p,j=0,name=p,bits=8,len=int(dl),rate=8363,corr=float(sc),ratio=float(rt)))
# scan hits
for f in glob.glob('scanout/*.jsonl')+glob.glob('amout/*.jsonl'):
    for line in open(f):
        try: d=json.loads(line)
        except: continue
        for h in d.get('hits',[]):
            if 'tgr' in d['path'].lower(): continue
            e=out[h['rom']]
            P=d['path']
            src=('Amiga disc: '+P.split('/amiga/x_',1)[1]) if '/amiga/x_' in P else ('Aminet: '+P.split('/aminet/x/',1)[1]) if '/aminet/x/' in P else 'Modland: '+P
            if h['kind']=='win' and h['n']>=0.95*min(e['len'],h['len']) and h['len']>=e['len']*0.98:
                e['exact'].append(dict(src=src,j=h['j'],name=h['name'][:40],bits=h['bits'],len=h['len'],rate=round(h['c5'] or 0),kind='exact (scan)'))
            elif h['kind']=='env':
                e['fuzzy'].append(dict(src=src,j=h['j'],name=h['name'][:40],bits=h['bits'],len=h['len'],rate=round(h['c5'] or 0),corr=h['ncc'],ratio=h['ratio']))
json.dump(out,open('provenance.json','w'),indent=1)
n_ex=sum(1 for v in out.values() if v['exact']); n_fz=sum(1 for v in out.values() if not v['exact'] and v['fuzzy'])
print(len(out),'samples;',n_ex,'with exact;',n_fz,'fuzzy only;',len(out)-n_ex-n_fz,'none')
for k,v in out.items():
    if not v['exact']: print(k,v['name'][:30],v['len'],'| best fuzzy:',sorted(v['fuzzy'],key=lambda x:-x['corr'])[:1])
