import struct, sys, numpy as np, json
import romsamp, modsamp
from remaster import *

MOD = '0x17fd10'
m = dict(romsamp.rom_modules())[MOD]
head, inst, tail = parse(m)
orig = modsamp.xm(m)

# notes each instrument plays (1-based instrument numbers), from the patterns in song order
hs = struct.unpack_from('<I', m, 60)[0]; songlen, _, nch, npat = struct.unpack_from('<HHHH', m, 64)
order = list(m[80:80 + songlen]); q = 60 + hs; pats = []
for p in range(npat):
    h, = struct.unpack_from('<I', m, q); ps, = struct.unpack_from('<H', m, q + 7); d = m[q + h:q + h + ps]; q += h + ps
    cells = []; i = 0
    while i < len(d):
        b = d[i]; f = [0] * 5
        if b & 0x80:
            i += 1
            for k in range(5):
                if b & (1 << k): f[k] = d[i]; i += 1
        else: f = list(d[i:i + 5]); i += 5
        cells.append(f)
    pats.append(cells)
use = {}
for o in order:
    for f in pats[o]:
        if f[1] and 0 < f[0] < 97: use.setdefault(f[1], []).append(f[0])

def notes_of(ins): return sorted(set(use.get(ins, [])))
def common(ins): return max(set(use[ins]), key=use[ins].count)

SAL = Salamander(); D1 = FSBS('EGuitarFSBS-dist1'); D2 = FSBS('EGuitarFSBS-dist2'); DRUM = DRS()

def rms(x, secs=0.3):
    n = int(secs * SR); return float(np.sqrt(np.mean(x[:n] ** 2)) + 1e-12)
def orig_rms(k, secs=0.3):
    s = orig[k]; n = int(secs * s['c5']); return float(np.sqrt(np.mean(s['pcm'][:n] ** 2)) + 1e-12)

# chord/notes each sample sounds at XM note C-4 (midi, with measured cents)
RECIPES = {
    3:  ('piano',  [57.12, 60.12, 64.13]),         # grandpiano3: A minor
    4:  ('piano',  [55.16, 59.13, 62.13]),         # grandpiano4: G major
    10: ('piano',  [50.20, 62.20]),                # mobypiano: D in octaves
    6:  ('gtr1',   [46.23, 53.23, 58.23]),         # dist chord: Bb power chord
    7:  ('gtr1',   [34.85]),                       # low Bb, distorted
    8:  ('gtr2',   [34.96]),                       # low Bb, distorted (second voice)
    12: ('gtr2',   [60.15]),                       # guitar90: C4 lead note
}
SMP = {3: 0, 4: 1, 5: 2, 6: 3, 7: 4, 8: 5, 9: 6, 10: 7, 11: 8, 12: 9, 16: 10, 17: 11, 18: 12, 19: 13, 20: 14}
DRUMS = {17: 'Kdrum_without_contact', 11: 'Snare', 9: 'Hihat_closed', 18: 'Hihat_open'}

zones_by_ins = {}; report = []
for ins, (kind, chord) in RECIPES.items():
    zones = []
    for n in notes_of(ins):
        shift = n - 49
        parts = []
        for j, mid in enumerate(chord):
            if kind == 'piano': parts.append(fade(SAL.note(mid + shift, vel=12), 4.0, 0.4))
            elif kind == 'gtr1': parts.append(fade(D1.note(mid + shift, rr=j), 3.5, 0.4))
            else: parts.append(fade(D2.note(mid + shift, rr=j), 3.5, 0.4))
        L = max(len(p) for p in parts); y = np.zeros(L)
        for p in parts: y[:len(p)] += p
        zones.append(([n], y, SR * 2 ** (-(shift) / 12), None))
    zones_by_ins[ins] = zones

# synth bass: harmonic resynthesis of xxbass, per note
s = orig[SMP[5]]; f0 = 3 * s['c5'] / s['loop'][1]
zones = []
for n in notes_of(5):
    y, lp = harmonic_resynth(s['pcm'], s['c5'], f0, s['loop'], 2 ** ((n - 49) / 12))
    zones.append(([n], y, SR * 2 ** (-(n - 49) / 12), lp))
zones_by_ins[5] = zones

# drums: one studio hit each, natural pitch at the note the song uses most
for ins, name in DRUMS.items():
    y = fade(DRUM.hit(name), 2.5 if 'open' in name else 1.5, 0.2)
    nb = common(ins)
    zones_by_ins[ins] = [(notes_of(ins), y, SR * 2 ** (-(nb - 49) / 12), None)]

# level: match each instrument's opening RMS to the original sample's, then one common headroom factor
scale = {}
for ins, zones in zones_by_ins.items():
    nb = common(ins); z = next(z for z in zones if nb in z[0])
    scale[ins] = orig_rms(SMP[ins]) / rms(z[1])
peak = max(max(np.abs(z[1]).max() for z in zones) * scale[ins] for ins, zones in zones_by_ins.items())
headroom = min(1.0, 0.98 / peak)
rebuilt = {}
for ins, zones in zones_by_ins.items():
    zz = [(nt, y * scale[ins] * headroom, c5, lp) for nt, y, c5, lp in zones]
    hdr, smp = inst[ins - 1]
    rebuilt[ins - 1] = rebuild_instrument(hdr, zz, smp[0][0])
    report.append((ins, orig[SMP[ins]]['name'].split(' / ')[-1][:22], len(zz), round(scale[ins] * headroom, 3)))
out = assemble(head, inst, tail, rebuilt)
open(sys.argv[1], 'wb').write(out)
open(sys.argv[1] + '.json', 'w').write(json.dumps({'headroom': headroom, 'instruments': report}, indent=1))
print('headroom', round(headroom, 3), 'module', len(out) // 1024, 'KB')
for r in report: print(r)
