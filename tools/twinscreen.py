import csv,re,subprocess,os
ROOT="/Users/jeffreywilbur/projects/strutb/brally"
g2d={}
with open(os.path.join(ROOT,"config/shared.csv"),newline='') as f:
    for r in csv.DictReader(f): g2d[r['glide_va'].lower()]=(r['d3d_va'].lower(),int(r['size']),r['class'],r['matched_by'])
gsize={}
with open(os.path.join(ROOT,"config/functions_glide.csv"),newline='') as f:
    for r in csv.DictReader(f): gsize[r['va'].lower()]=int(r['size'])
tags={}
for ln in subprocess.run(["grep","-rn","@implements","src/"],cwd=ROOT,capture_output=True,text=True).stdout.splitlines():
    m=re.search(r'@implements\s+(0x[0-9A-Fa-f]{8})\s+(\w+)\s+(\S+)',ln)
    if m: tags.setdefault((m.group(1).lower(),m.group(2)),[]).append((m.group(3),ln.split(':')[0]))
scored=set()
with open(os.path.join(ROOT,"build/match/report.csv"),newline='') as f:
    for r in csv.DictReader(f): scored.add(r['va'].lower())
for p in ("build/match/report_cpp.csv","build/match/report_exe.csv"):
    fp=os.path.join(ROOT,p)
    if os.path.exists(fp):
        for r in csv.DictReader(open(fp,newline='')):
            for v in r.values():
                if v and re.fullmatch(r'0x[0-9A-Fa-f]{8}',v): scored.add(v.lower())
hits=[]
for gva,(d3d,ssize,cls,mb) in g2d.items():
    if (gva,'glide') in tags or gva in scored: continue
    if (d3d,'d3d') not in tags: continue
    if gsize.get(gva)!=ssize: continue
    for name,fil in tags[(d3d,'d3d')]: hits.append((gva,ssize,d3d,cls,mb,name,fil))
byfile={}
for h in hits: byfile.setdefault(h[6],[]).append(h)
print(f"UNSCORED glide VAs with a d3d-tagged twin, sizes agreeing: {len(hits)} in {len(byfile)} files, {sum(h[1] for h in hits)} B")
for fil in sorted(byfile,key=lambda k:-sum(h[1] for h in byfile[k])):
    hs=byfile[fil]
    print(f"\n{fil}   {len(hs)} fn / {sum(h[1] for h in hs)} B")
    for h in sorted(hs,key=lambda x:-x[1]): print(f"   glide={h[0]} {h[1]:5d}B  d3d={h[2]} {h[3]}/{h[4]}  {h[5]}")
