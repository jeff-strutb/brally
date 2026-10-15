"""surfparity.py DUMPDIR SURFTEST OUTDIR: texregion.py against light_surf.c (surftest) on every
texture BR_LIGHT_TEXOUT dumped under DUMPDIR (PAMs named by hash): the same textures given a
map, the same surfaces and colours, and per map texel the same region drawn (allowed: 0.1% of
texels, float-order ties where three regions meet), region weights and variation within 1.5/255.
Exit 1 when any map differs beyond that."""
import os, sys, glob, subprocess
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import texregion as tr
DUMP, SURFTEST, OD = sys.argv[1], sys.argv[2], sys.argv[3]
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
mat = {}
for l in open(ROOT + '/ports/common/models/texmat.csv'):
    if l.startswith('#') or not l.strip(): continue
    h, m = l.strip().split(',')[:2]; mat[h.upper()] = m
names = sorted(set(mat.values()))
files = {}
for p in glob.glob(DUMP + '/**/*.pam', recursive=True):
    if os.path.getsize(p) > 80: files[os.path.basename(p)[:-4]] = p
L = [(h, names.index(mat[h]) if h in mat else -1, p) for h, p in sorted(files.items())]
open(OD + '/list.txt', 'w').write(''.join('%s %d %s\n' % x for x in L))
od = OD + '/maps'; os.makedirs(od, exist_ok=True)
for f in glob.glob(od + '/*'): os.remove(f)
print(subprocess.run([SURFTEST, OD + '/list.txt', od], capture_output=True, text=True).stdout.strip())
def rd(p):
    b = open(p, 'rb').read(); i = b.index(b'ENDHDR\n') + 7; hdr = b[:i].decode().split('\n')
    w = int([x for x in hdr if x.startswith('WIDTH')][0].split()[1]); h = int([x for x in hdr if x.startswith('HEIGHT')][0].split()[1])
    return np.frombuffer(b[i:i + w * h * 4], np.uint8).reshape(h, w, 4)
bad = n = 0; worst = []
for h, m, p in L:
    r = tr.region_map(mat.get(h, '?'), rd(p))
    c = od + '/%s.bin' % h
    if (r is None) != (not os.path.exists(c)):
        bad += 1; print('presence differs', h, mat.get(h)); continue
    if r is None: continue
    n += 1
    d = open(c, 'rb').read(); W, H, k = np.frombuffer(d[:12], np.int32); surf = list(d[12:20][:k]); lin = np.frombuffer(d[20:116], np.float32).reshape(8, 3)[:k]
    mp = np.frombuffer(d[116:116 + W * H * 4], np.uint8).reshape(H, W, 4); vr = np.frombuffer(d[116 + W * H * 4:], np.uint8).reshape(H, W, 2)
    pm, pv, pal = r
    ps = [s for s, _ in pal]
    if ps != surf or not np.allclose(lin, np.array([c for _, c in pal]), rtol=1e-5, atol=1e-6):
        bad += 1; print('palette differs', h, mat.get(h), ps, surf, float(np.abs(lin - np.array([c for _, c in pal])).max()) if ps == surf else ''); continue
    def eff(m, v):   # each region's weight as drawn, and the variation drawn
        t = m[..., 2] / 255.0; Wt = np.zeros(m.shape[:2] + (8,))
        np.put_along_axis(Wt, m[..., 0:1].astype(int), (1 - t)[..., None], 2)
        lo = np.take_along_axis(Wt, m[..., 1:2].astype(int), 2)[..., 0]
        np.put_along_axis(Wt, m[..., 1:2].astype(int), (lo + t)[..., None] * (m[..., 1:2] != m[..., 0:1]) + lo[..., None] * (m[..., 1:2] == m[..., 0:1]), 2)
        return Wt, (1 - t) * v[..., 0] + t * v[..., 1]
    wc, vc = eff(mp, vr); wp, vp = eff(pm, pv)
    lab_frac = float((wc.argmax(2) != wp.argmax(2)).mean())
    tfrac = float((np.abs(wc - wp).max(2) * 255 > 1.5).mean()); vfrac = float((np.abs(vc - vp) > 1.5).mean())
    cdiff = int(np.abs(mp[..., 3].astype(int) - pm[..., 3].astype(int)).max())
    worst.append((max(lab_frac, tfrac, vfrac), lab_frac, tfrac, vfrac, h, cdiff))
    if max(lab_frac, tfrac, vfrac) > 0.001 or cdiff > 1: bad += 1
worst.sort(reverse=True)
print(n, 'maps compared,', bad, 'beyond 0.1% of texels with another region drawn, a region weight or the variation off by more than 1.5/255 (or coverage by more than 1)')
for w in worst[:5]: print('  texels off: region %.4f%%  weight %.4f%%  variation %.4f%%  coverage max %d  %s' % (w[1] * 100, w[2] * 100, w[3] * 100, w[5], w[4]))
sys.exit(1 if bad else 0)
