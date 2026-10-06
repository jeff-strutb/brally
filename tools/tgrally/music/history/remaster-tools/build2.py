"""Jungle remaster, analysis-driven: voicings fitted to each original sample's spectrum with the
replacement's own notes, then an iterative stem-matching loop sets each instrument's level and EQ."""
import struct, sys, json, os, numpy as np
import romsamp, modsamp
from remaster import *
from transcribe import transcribe
import stems as ST

MOD = '0x17fd10'
m = dict(romsamp.rom_modules())[MOD]
head, inst, tail = parse(m)
orig = modsamp.xm(m)
SMP = {3: 0, 4: 1, 5: 2, 6: 3, 7: 4, 8: 5, 9: 6, 10: 7, 11: 8, 12: 9, 16: 10, 17: 11, 18: 12, 19: 13, 20: 14}

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
notes_of = lambda ins: sorted(set(use.get(ins, [])))
common = lambda ins: max(set(use[ins]), key=use[ins].count)

SAL = Salamander(); D1 = FSBS('EGuitarFSBS-dist1'); D2 = FSBS('EGuitarFSBS-dist2'); DRUM = DRS()
KIND = {3: 'piano', 4: 'piano', 10: 'piano', 6: 'gtr1', 7: 'gtr1', 8: 'gtr2', 12: 'gtr2'}
CH = json.load(open('choices.json')) if os.path.exists('choices.json') else {}
CHD = json.load(open('choices_drums.json')) if os.path.exists('choices_drums.json') else {}
PVEL = {3: 12, 4: 15, 10: 16}
SRC = {'piano': lambda mid, rr=0, vel=12: fade(SAL.note(mid, vel=vel), 4.0, 0.4),
       'gtr1': lambda mid, rr=0, vel=0: fade(D1.note(mid, rr=rr), 3.5, 0.4),
       'gtr2': lambda mid, rr=0, vel=0: fade(D2.note(mid, rr=rr), 3.5, 0.4)}
MICS = {  # which microphones make each drum: close mics first, room only where the original rings
    17: ('Kdrum_without_contact', {'Kdrum_front': 1.0, 'Kdrum_back': 0.7, 'OHL': 0.15, 'OHR': 0.15}, 0),
    11: ('Snare', {'Snare_top': 1.0, 'Snare_bottom': 0.35, 'OHL': 0.35, 'OHR': 0.35, 'AmbL': 0.6, 'AmbR': 0.6}, 0),
    9:  ('Hihat_closed', {'Hihat': 1.0, 'OHL': 0.25, 'OHR': 0.25}, 300),
    18: ('Hihat_open', {'Hihat': 1.0, 'OHL': 0.35, 'OHR': 0.35}, 300),
}

def match_envelope(y, o, orate, win=0.02, lo_db=-30, hi_db=12):
    """Shape a replacement hit's loudness over time to the original sample's (the decay is part of the part)."""
    n = int(win * SR); t = np.arange(len(y)) / SR
    ey = np.sqrt(np.convolve(y ** 2, np.ones(n) / n, 'same')) + 1e-9
    no = max(1, int(win * orate)); eo = np.sqrt(np.convolve(o ** 2, np.ones(no) / no, 'same')) + 1e-9
    eo_t = np.interp(t, np.arange(len(o)) / orate, eo, right=eo[-1] * 1e-3)
    r = 20 * np.log10((eo_t / eo_t.max()) / (ey / ey.max()))
    r = np.clip(np.convolve(r, np.ones(n) / n, 'same'), lo_db, hi_db)
    return y * 10 ** (r / 20)

def voicing(ins):
    k = SMP[ins]; s = orig[k]
    c, top = transcribe(s['pcm'], s['c5'])
    mx = max(w for _, w in top)
    cand = sorted({round(mm, 2) for mm, w in top if w / mx >= 0.08})
    return fit_voicing(s['pcm'], s['c5'], cand, lambda mid: SRC[KIND[ins]](mid, vel=PVEL.get(ins, 12)))

def base_zones():
    """Unscaled, unequalised zones per instrument (the expensive part, computed once)."""
    Z = {}; info = {}
    for ins, kind in KIND.items():
        v = voicing(ins); info[ins] = [(round(a, 2), round(b, 4)) for a, b in v]
        zones = []
        for n in notes_of(ins):
            sh = n - 49; parts = [w * SRC[kind](mid + sh, rr=j, vel=PVEL.get(ins, 12)) for j, (mid, w) in enumerate(v)]
            L = max(len(p) for p in parts); y = np.zeros(L)
            for p in parts: y[:len(p)] += p
            zones.append(([n], y, SR * 2 ** (-sh / 12), None))
        Z[ins] = zones
    s = orig[SMP[5]]; f0 = 3 * s['c5'] / s['loop'][1]
    zz = []
    for n in notes_of(5):
        y, lp = harmonic_resynth(s['pcm'], s['c5'], f0, s['loop'], 2 ** ((n - 49) / 12))
        zz.append(([n], y, SR * 2 ** (-(n - 49) / 12), lp))
    Z[5] = zz
    for ins in (19, 20):
        k = SMP[ins]; s = orig[k]; c, top = transcribe(s['pcm'], s['c5']); mx = max(w for _, w in top)
        notes = [(mm, w / mx) for mm, w in top if w / mx >= 0.18]; info[ins] = notes
        zz = []
        for n in notes_of(ins):
            y, lp = chord_resynth(s['pcm'], s['c5'], notes, s['loop'], 2 ** ((n - 49) / 12))
            zz.append(([n], y, SR * 2 ** (-(n - 49) / 12), lp))
        Z[ins] = zz
    s = orig[SMP[16]]                                   # wack10: the original, carried at SR
    Z[16] = [(notes_of(16), resample_to(s['pcm'], s['c5'], 1.0), SR, None)]
    for ins, (name, mics, hp) in MICS.items():
        c = CHD.get(str(ins))
        if c:
            name = c['variant']; mics = dict(mics); mics.update({'OHL': c['oh'], 'OHR': c['oh'], 'AmbL': c['amb'], 'AmbR': c['amb']})
            y = DRUM.hit(name, pct=c['pct'], mics=mics)
        else:
            y = DRUM.hit(name, mics=mics)
        if hp: y = highpass(y, hp)
        s = orig[SMP[ins]]; orate = s['c5'] * 2 ** ((common(ins) - 49) / 12)
        y = match_envelope(y, s['pcm'], orate)
        y = fade(y, 2.5 if 'open' in name else 1.5, 0.2)
        Z[ins] = [(notes_of(ins), y, SR * 2 ** (-(common(ins) - 49) / 12), None)]
    return Z, info

def build(Z, params):
    rebuilt = {}; scaled = {}
    for ins, zones in Z.items():
        p = params.get(str(ins), {'gain_db': 0.0, 'eq_f': [100, 1000], 'eq_db': [0, 0]})
        g = 10 ** (p['gain_db'] / 20)
        scaled[ins] = [(nt, g * zero_phase_eq(y, SR, np.array(p['eq_f']), np.array(p['eq_db'])), c5, lp)
                       for nt, y, c5, lp in zones]
    peak = max(max(np.abs(z[1]).max() for z in zz) for zz in scaled.values())
    hr = min(1.0, 0.98 / peak)
    for ins, zz in scaled.items():
        hdr, smp = inst[ins - 1]
        rebuilt[ins - 1] = rebuild_instrument(hdr, [(nt, y * hr, c5, lp) for nt, y, c5, lp in zz], smp[0][0])
    return assemble(head, inst, tail, rebuilt), hr
