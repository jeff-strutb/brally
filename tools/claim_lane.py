#!/usr/bin/env python3
"""Hand each parallel closer a disjoint lane of still-diff functions.
  claim [N]                    -> print TOKEN then N unclaimed diff functions (va name file)
  release <TOKEN> [wallVA ...] -> park the listed walls (never re-handed), release the rest
Matched functions drop out on their own (they leave status=diff). Stale claims (>90 min) are reclaimed."""
import csv, os, sys, time, uuid, fcntl
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT=os.path.join(ROOT,'build','match','report.csv')
CLAIMS=os.path.join(ROOT,'build','match','lane_claims.csv')
STALE=90*60; FIELDS=['va','name','file','token','ts','status']
def diffs():
    with open(REPORT) as f: return [r for r in csv.DictReader(f) if r.get('status')=='diff']
def load():
    if not os.path.exists(CLAIMS): return []
    with open(CLAIMS) as f: return list(csv.DictReader(f))
def save(rows):
    os.makedirs(os.path.dirname(CLAIMS),exist_ok=True)
    with open(CLAIMS,'w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=FIELDS); w.writeheader()
        for r in rows: w.writerow({k:r.get(k,'') for k in FIELDS})
def lock():
    os.makedirs(os.path.dirname(CLAIMS),exist_ok=True)
    lk=open(CLAIMS+'.lock','w'); fcntl.flock(lk,fcntl.LOCK_EX); return lk
def claim(n,big=False):
    n=int(n); lk=lock()
    try:
        now=time.time(); dr=diffs(); dvas={r['va'] for r in dr}; keep=[]; held=set()
        for c in load():
            if c['status']=='parked' and c['va'] in dvas: keep.append(c); held.add(c['va'])
            elif c['status']=='claimed' and (now-float(c['ts']))<STALE and c['va'] in dvas: keep.append(c); held.add(c['va'])
        pool=[r for r in dr if r['va'] not in held]
        # Ranked lanes: tools/fnmatch/triage.py publishes triage_rank.csv
        # (lower score = better target: SHAPE < mixed < missing-code < coloring
        # wall). When present, hand out best-first; the held-set already keeps
        # parallel lanes disjoint. Without it, fall back to rotation.
        rank_csv=os.path.join(ROOT,'build','match','triage_rank.csv')
        if big:
            # --big: hand out the LARGEST still-diff functions (by original
            # bytes). The giants carry dossiers/ordering rules -- see the
            # "Large functions" section of docs/STRUCTURAL-PLAYBOOK.md.
            pool.sort(key=lambda r:-int(r.get('orig_size') or 0))
            pick=pool[:n]
        elif os.path.exists(rank_csv):
            with open(rank_csv) as f:
                score={r['va'].lower():int(r['score']) for r in csv.DictReader(f)}
            pool.sort(key=lambda r:score.get(r['va'].lower(),50000))
            pick=pool[:n]
        else:
            offset=(len([c for c in keep if c['status']=='claimed'])*n)%max(len(pool),1)
            pick=(pool[offset:]+pool[:offset])[:n]
        tok=uuid.uuid4().hex[:8]
        for r in pick: keep.append({'va':r['va'],'name':r['name'],'file':r['file'],'token':tok,'ts':str(now),'status':'claimed'})
        save(keep); print('TOKEN',tok)
        for r in pick: print(r['va'],r['name'],r['file'])
    finally: fcntl.flock(lk,fcntl.LOCK_UN)
def release(tok,walls):
    walls=set(walls); lk=lock()
    try:
        out=[]
        for c in load():
            if c['token']!=tok: out.append(c); continue
            if c['va'] in walls: c['status']='parked'; c['ts']=str(time.time()); out.append(c)
        save(out)
    finally: fcntl.flock(lk,fcntl.LOCK_UN)
if __name__=='__main__':
    cmd=sys.argv[1] if len(sys.argv)>1 else 'claim'
    if cmd=='claim':
        rest=[a for a in sys.argv[2:] if a!='--big']
        claim(rest[0] if rest else 12, big='--big' in sys.argv)
    elif cmd=='release': release(sys.argv[2], sys.argv[3:])
