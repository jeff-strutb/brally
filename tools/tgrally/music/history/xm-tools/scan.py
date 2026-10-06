"""Stream a list of Modland module paths; for each sample report exact-window hits and
rate-invariant envelope matches against every ROM sample.  usage: scan.py listfile outjsonl"""
import sys, json, urllib.request, urllib.parse, time, numpy as np, modsamp, romsamp
from numpy.lib.stride_tricks import sliding_window_view as swv

NB = 48
def env(p):
    n = len(p) // NB
    if n < 4: return None
    e = np.sqrt((p[:n*NB].reshape(NB, n) ** 2).mean(1))
    e = e - e.mean(); s = np.sqrt((e*e).sum())
    return e / s if s > 0 else None

def zc(p):
    h=0.02; st=0; n=0
    q=np.where(p>h,1,np.where(p<-h,-1,0)); q=q[q!=0]
    return int(np.count_nonzero(np.diff(q))) if len(q)>1 else 0

def win8(b):
    if len(b) < 8: return np.zeros(0, np.uint64)
    return swv(np.frombuffer(b, np.uint8), 8).copy().view(np.uint64).ravel()

# ROM targets
R = []
for mo, m in romsamp.rom_modules():
    for k, s in enumerate(modsamp.xm(m)):
        if s['len'] > 32: R.append(('%s#%d' % (mo, k), s))
keys, hv = [], []
for i, (key, s) in enumerate(R):
    b = s['raw'].astype(np.int8).tobytes()
    w = win8(b); a = np.frombuffer(b, np.uint8)
    ok = np.array([len(set(a[j:j+8])) >= 5 for j in range(len(w))], bool) if len(w) else np.zeros(0, bool)
    hv.append(w[ok]); keys += [i] * int(ok.sum())
hv = np.concatenate(hv); keys = np.array(keys)
order = np.argsort(hv); hv = hv[order]; keys = keys[order]
renv = [(i, env(s['pcm']), s['len'], zc(s['pcm'])) for i, (key, s) in enumerate(R) if s['len'] >= 1000]
renv = [(i, e, n, z) for i, e, n, z in renv if e is not None]
RE = np.array([e for _, e, _, _ in renv]); RI = [i for i, _, _, _ in renv]; RL = np.array([n for _, _, n, _ in renv]); RZ = np.array([z for *_, z in renv])
import match2

BASES = ['https://ftp.modland.com/pub/modules/', 'https://modland.antarctica.no/pub/modules/']
def fetch(path, k):
    for t in range(3):
        try:
            u = BASES[(k + t) % 2] + urllib.parse.quote(path)
            with urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent': 'Mozilla/5.0'}), timeout=60) as r:
                return r.read()
        except Exception:
            time.sleep(2)
    return None

out = open(sys.argv[2], 'a')
paths = [l.rstrip('\n') for l in open(sys.argv[1]) if l.strip()]
for k, path in enumerate(paths):
    d = fetch(path, k)
    if d is None:
        out.write(json.dumps({'path': path, 'err': 'fetch'}) + '\n'); continue
    try:
        ss = modsamp.load(d)
    except Exception as e:
        out.write(json.dumps({'path': path, 'err': 'parse'}) + '\n'); continue
    hits = []
    for j, s in enumerate(ss):
        if s['len'] < 64: continue
        raw = np.asarray(s['raw'])
        b = ((raw >> 8) if s['bits'] == 16 else raw).astype(np.int8).tobytes()
        w = win8(b)
        if len(w):
            pos = np.searchsorted(hv, w); pos[pos >= len(hv)] = 0
            m = hv[pos] == w
            if m.sum() >= 24:
                ids, cnt = np.unique(keys[pos[m]], return_counts=True)
                for i, c in zip(ids, cnt):
                    if c >= max(64, 0.25 * min(R[i][1]['len'], s['len'])): hits.append(dict(kind='win', rom=R[i][0], n=int(c), j=j, name=s['name'], bits=s['bits'], len=s['len'], c5=s['c5']))
        e = env(s['pcm'])
        if e is not None and len(RE):
            c = RE @ e
            ratio = s['len'] / RL
            sel = np.nonzero((c > 0.95) & (ratio > 0.9) & (ratio < 6.5))[0]
            if len(sel):
                z = zc(s['pcm'])
                for q in sel:
                    if not (abs(z - RZ[q]) <= 0.12 * max(RZ[q], 20)): continue
                    rs = R[RI[q]][1]
                    Dr = match2.rs(s['pcm'], float(ratio[q])) if abs(ratio[q]-1) > 1e-3 else s['pcm']
                    a = rs['pcm'][:min(len(rs['pcm']), len(Dr), 12000)]
                    sc, _ = match2.ncc(a, Dr)
                    if sc > 0.9:
                        hits.append(dict(kind='env', rom=R[RI[q]][0], corr=round(float(c[q]), 4), ncc=round(sc, 4), ratio=round(float(ratio[q]), 4), j=j, name=s['name'], bits=s['bits'], len=s['len'], c5=s['c5']))
    out.write(json.dumps({'path': path, 'n': len(ss), 'hits': hits}) + '\n'); out.flush()
