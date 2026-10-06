import json,glob,collections,sys
UNS=set(open('unsourced.txt').read().strip().split(','))
best=collections.defaultdict(list); nfiles=0; errs=0
for f in glob.glob('scanout/*.jsonl'):
    for l in open(f):
        try: d=json.loads(l)
        except: continue
        nfiles+=1
        if 'err' in d: errs+=1; continue
        for h in d['hits']:
            if 'tgr' in d['path'].lower(): continue
            score = h.get('ncc',0) if h['kind']=='env' else min(1.0,h['n']/max(1,min(h['len'],1e9)))
            best[h['rom']].append((h['kind'],score,h.get('ratio',1.0),h['bits'],h['len'],h.get('c5'),d['path'],h['j'],h['name'][:26],h.get('n')))
print(nfiles,'files scanned',errs,'errors')
want=sys.argv[1] if len(sys.argv)>1 else 'uns'
for rom in sorted(best, key=lambda r:(r not in UNS, r)):
    if want=='uns' and rom not in UNS: continue
    hs=sorted(best[rom],key=lambda t:(-(t[0]=='env' and t[2]>1.05 or t[3]==16), -t[1]))
    print(('** ' if rom in UNS else '   ')+rom, len(hs))
    for t in hs[:6]: print('      %s %.3f x%.3f %db len%d  %s #%d %s'%(t[0],t[1],t[2],t[3],t[4],t[6],t[7],t[8]))
