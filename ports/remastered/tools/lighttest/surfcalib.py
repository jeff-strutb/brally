"""surfcalib.py RUNDIR [PREFIX]: the Remastered textures' grade (docs/brally/remaster/textures/DESIGN.md
section 4).  Under RUNDIR, pairs of headless runs PREFIX<name>_on (the lit frames) and PREFIX<name>_id
(the same script with BR_SURF_VIEW=2 BR_LIGHT_VIEW=1: each surface's albedo in its id's colour, hue
id * 0.618034 turns, everything else black): per surface, the lit frames' L* (median, upper quartile)
and the chroma and hue of its mean where the id frames show it, against Forza Horizon 6's clean ground
patches (FH6 below, section 2), and the luminance and chroma factors that would match them.

Both measures leave the exposure out: a surface's luminance over its own frame's median (FH6's
patches over their screenshots'), and its saturation as the chroma of its mean over its lightness."""
import sys, os, glob, json
import numpy as np
from PIL import Image
def lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def lab(rgb):
    l = lin(rgb); M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    xyz = l @ M.T / np.array([0.9505, 1.0, 1.089])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)
R = sys.argv[1]; pre = sys.argv[2] if len(sys.argv) > 2 else 'k_'
IDS = {'asphalt': 1, 'paint_white': 2, 'gravel': 4, 'dirt': 5, 'dirt_red': 6, 'sand': 7, 'grass': 11, 'grass_dry': 12,
       'rock_grey': 14, 'rock_brown': 15, 'rock_red': 16, 'concrete': 17, 'plaster': 18, 'snow': 24}
REF = {'asphalt': 'asphalt', 'gravel': 'dirt', 'dirt': 'dirt', 'dirt_red': 'dirt', 'sand': 'sand', 'grass': 'grass', 'grass_dry': 'grass',
       'rock_grey': 'rock', 'rock_brown': 'rock', 'rock_red': 'rock', 'snow': 'snow'}
# Forza Horizon 6's clean ground patches (22,510 Steam screenshots; DESIGN.md section 2)
FH6 = {"asphalt": {"rel50": 1.036, "rel75": 1.321, "cl50": 0.158, "n": 1377}, "dirt": {"rel50": 1.125, "rel75": 1.346, "cl50": 0.354, "n": 35}, "grass": {"rel50": 0.924, "rel75": 1.049, "cl50": 0.727, "n": 89}, "sand": {"rel50": 1.148, "rel75": 1.301, "cl50": 0.395, "n": 35}, "rock": {"rel50": 1.006, "rel75": 1.258, "cl50": 0.161, "n": 165}, "snow": {"rel50": 1.481, "rel75": 2.928, "cl50": 0.045, "n": 78}}
fh = FH6
def hsv(a):
    mx, mn = a.max(-1), a.min(-1); d = mx - mn; h = np.zeros_like(mx); r, g, b = a[..., 0], a[..., 1], a[..., 2]
    m = d > 1e-6; rr = m & (mx == r); gg = m & (mx == g) & ~rr; bb = m & ~rr & ~gg
    h[rr] = (60 * (g - b)[rr] / d[rr]) % 360; h[gg] = 60 * (b - r)[gg] / d[gg] + 120; h[bb] = 60 * (r - g)[bb] / d[bb] + 240
    return h, np.where(mx > 0, d / np.maximum(mx, 1e-6), 0), mx
def erode(m, k):
    o = m.copy()
    for dy in range(-k, k + 1):
        for dx in range(-k, k + 1):
            o &= np.roll(np.roll(m, dy, 0), dx, 1)
    return o
acc = {s: [] for s in IDS}; hh = []; per = {}
for idd in sorted(glob.glob(R + '/%s*_id' % pre)):
    on = idd[:-3] + '_on'
    for f in glob.glob(idd + '/shots/*.png'):
        g = on + '/shots/' + os.path.basename(f)
        if not os.path.exists(g): continue
        A = np.asarray(Image.open(f).convert('RGB'), float)[60:-60] / 255   # (the HUD's rows out)
        B = np.asarray(Image.open(g).convert('RGB'), float)[60:-60] / 255
        h, s, v = hsv(lin(A)); ok = (s > 0.6) & (v > 0.02); hh.append(h[ok])   # the id colours are linear
        lb = lab(B); Y = lin(B) @ [0.2126, 0.7152, 0.0722]; ymed = max(float(np.median(Y)), 1e-3)
        for n, i in IDS.items():
            hc = (i * 0.618034) % 1 * 360
            m = ok & (np.minimum(abs(h - hc), 360 - abs(h - hc)) < 5)
            m = erode(m, 3)
            if m.sum() < 400: continue
            acc[n].append((Y[m] / ymed, lb[m])); per.setdefault(n, set()).add(os.path.basename(idd))
H = np.concatenate(hh) if hh else np.zeros(0)
hist = np.histogram(H, 72, (0, 360))[0]
print('id hue histogram (5 deg bins, counts over 1%):', [(i * 5, int(c)) for i, c in enumerate(hist) if c > 0.01 * hist.sum()])
out = {}
for n in IDS:
    if not acc[n]: continue
    rel = np.concatenate([r for r, _ in acc[n]]); x = np.concatenate([l for _, l in acc[n]])
    cl = float(np.hypot(x[:, 1].mean(), x[:, 2].mean()) / max(x[:, 0].mean(), 1))
    r50, r75 = np.percentile(rel, [50, 75]); h = float(np.degrees(np.arctan2(x[:, 2].mean(), x[:, 1].mean())) % 360)
    line = '%-11s %8d px  lum/frame %.2f/%.2f  C/L %.3f  h %3.0f' % (n, len(rel), r50, r75, cl, h)
    if n in REF:
        t = fh[REF[n]]
        lf = t['rel50'] / r50; cf = t['cl50'] / max(cl, 1e-3)
        line += '   FH6 %.2f/%.2f C/L %.3f  -> lum x%.2f chroma x%.2f' % (t['rel50'], t['rel75'], t['cl50'], lf, cf)
        out[n] = dict(rel50=float(r50), cl=cl, lum=lf, chroma=cf, n=int(len(rel)))
    print(line)
json.dump(out, open(R + '/calib_%s.json' % pre.strip('_'), 'w'), indent=1)
