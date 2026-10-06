"""Render any of the N64 pieces from its logged performance with modern instruments, one stereo
track per part. The recipe below says, for every instrument the piece uses, what plays it; chord
shapes are the notes transcribed from the ROM sample, in equal temperament at A440."""
import sys, os, json, collections, numpy as np, soundfile as sf
from render_remaster import *
import engines as E, synthpy, modsamp
from classify import inst_samples

PRE = {
  'bass':      dict(unison=3, detune_cents=10, width=0.0, saw=1.0, pulse=0.6, pulse_width=0.4, sub=0.45, cutoff_hz=320, env_oct=3.0, key_track=0.7, reso=0.3, drive=2.2, fa=0.002, fd=0.18, fs=0.25, fr=0.08, aa=0.001, ad=0.3, as_=0.85, ar=0.06),
  'distbass':  dict(unison=3, detune_cents=14, width=0.0, saw=1.0, pulse=0.8, pulse_width=0.35, sub=0.4, cutoff_hz=600, env_oct=2.5, key_track=0.6, reso=0.2, drive=5.0, fa=0.002, fd=0.25, fs=0.5, fr=0.08, aa=0.001, ad=0.3, as_=0.9, ar=0.06),
  'acid':      dict(unison=1, detune_cents=0, width=0.0, saw=1.0, pulse=0.0, pulse_width=0.5, sub=0.35, cutoff_hz=260, env_oct=4.5, key_track=0.5, reso=0.72, drive=2.5, fa=0.001, fd=0.16, fs=0.15, fr=0.06, aa=0.001, ad=0.2, as_=0.85, ar=0.05),
  'pluckbass': dict(unison=2, detune_cents=6, width=0.0, saw=0.8, pulse=0.7, pulse_width=0.3, sub=0.5, cutoff_hz=220, env_oct=4.0, key_track=0.6, reso=0.25, drive=1.5, fa=0.001, fd=0.12, fs=0.1, fr=0.05, aa=0.001, ad=0.25, as_=0.6, ar=0.05),
  'stab':      dict(unison=7, detune_cents=28, width=1.0, saw=1.0, pulse=0.25, pulse_width=0.5, sub=0.0, cutoff_hz=900, env_oct=3.2, key_track=0.3, reso=0.2, drive=0.6, fa=0.003, fd=0.45, fs=0.45, fr=0.35, aa=0.004, ad=0.5, as_=0.8, ar=0.35),
  'pad':       dict(unison=7, detune_cents=22, width=1.0, saw=0.9, pulse=0.4, pulse_width=0.5, sub=0.0, cutoff_hz=700, env_oct=1.5, key_track=0.3, reso=0.15, drive=0.3, fa=0.6, fd=1.5, fs=0.6, fr=0.8, aa=0.35, ad=1.0, as_=0.9, ar=0.8),
  'lead':      dict(unison=5, detune_cents=16, width=0.6, saw=1.0, pulse=0.3, pulse_width=0.5, sub=0.2, cutoff_hz=1100, env_oct=2.2, key_track=0.4, reso=0.25, drive=1.0, fa=0.005, fd=0.35, fs=0.55, fr=0.25, aa=0.004, ad=0.4, as_=0.85, ar=0.2),
  'trance':    dict(unison=9, detune_cents=35, width=1.0, saw=1.0, pulse=0.15, pulse_width=0.5, sub=0.25, cutoff_hz=2200, env_oct=1.6, key_track=0.3, reso=0.15, drive=0.8, fa=0.003, fd=0.3, fs=0.7, fr=0.3, aa=0.003, ad=0.3, as_=0.9, ar=0.3),
  'bell':      dict(unison=3, detune_cents=8, width=0.5, saw=0.3, pulse=1.0, pulse_width=0.25, sub=0.0, cutoff_hz=2500, env_oct=1.8, key_track=0.5, reso=0.1, drive=0.3, fa=0.001, fd=0.5, fs=0.2, fr=0.4, aa=0.001, ad=0.6, as_=0.25, ar=0.4),
  'pluck':     dict(unison=3, detune_cents=12, width=0.7, saw=1.0, pulse=0.4, pulse_width=0.4, sub=0.0, cutoff_hz=700, env_oct=4.2, key_track=0.4, reso=0.25, drive=0.6, fa=0.001, fd=0.14, fs=0.08, fr=0.12, aa=0.001, ad=0.25, as_=0.35, ar=0.12),
  'mellow':    dict(unison=5, detune_cents=12, width=0.7, saw=0.4, pulse=0.8, pulse_width=0.5, sub=0.2, cutoff_hz=900, env_oct=1.2, key_track=0.4, reso=0.1, drive=0.2, fa=0.02, fd=0.8, fs=0.6, fr=0.5, aa=0.02, ad=0.6, as_=0.85, ar=0.5),
  'sub808':    dict(unison=1, detune_cents=0, width=0.0, saw=0.0, pulse=0.0, pulse_width=0.5, sub=1.0, cutoff_hz=400, env_oct=1.0, key_track=0.5, reso=0.0, drive=1.2, fa=0.001, fd=0.5, fs=0.5, fr=0.15, aa=0.002, ad=1.2, as_=0.55, ar=0.15),
  'blip1':     dict(unison=2, detune_cents=6, width=0.2, saw=0.7, pulse=0.8, pulse_width=0.35, sub=0.4, cutoff_hz=500, env_oct=2.5, key_track=0.6, reso=0.2, drive=1.2, fa=0.001, fd=0.06, fs=0.2, fr=0.04, aa=0.001, ad=0.1, as_=0.5, ar=0.04),
  'blip2':     dict(unison=2, detune_cents=6, width=0.2, saw=0.9, pulse=0.5, pulse_width=0.4, sub=0.3, cutoff_hz=700, env_oct=2.5, key_track=0.6, reso=0.25, drive=1.2, fa=0.001, fd=0.06, fs=0.25, fr=0.04, aa=0.001, ad=0.1, as_=0.5, ar=0.04),
  'blip3':     dict(unison=2, detune_cents=8, width=0.3, saw=1.0, pulse=0.6, pulse_width=0.3, sub=0.3, cutoff_hz=900, env_oct=3.0, key_track=0.6, reso=0.3, drive=1.4, fa=0.001, fd=0.1, fs=0.25, fr=0.05, aa=0.001, ad=0.18, as_=0.4, ar=0.05),
  'swellbass': dict(unison=3, detune_cents=12, width=0.0, saw=1.0, pulse=0.8, pulse_width=0.35, sub=0.45, cutoff_hz=520, env_oct=1.0, key_track=0.6, reso=0.15, drive=3.5, fa=0.2, fd=0.5, fs=0.8, fr=0.12, aa=0.21, ad=0.4, as_=0.95, ar=0.12),
  'swellxx':   dict(unison=3, detune_cents=10, width=0.0, saw=1.0, pulse=0.6, pulse_width=0.4, sub=0.5, cutoff_hz=380, env_oct=0.8, key_track=0.7, reso=0.2, drive=2.0, fa=0.3, fd=0.6, fs=0.8, fr=0.15, aa=0.32, ad=0.5, as_=0.95, ar=0.15),
  'drone':     dict(unison=5, detune_cents=14, width=0.8, saw=1.0, pulse=0.3, pulse_width=0.5, sub=0.5, cutoff_hz=320, env_oct=0.8, key_track=0.3, reso=0.1, drive=0.8, fa=0.05, fd=2.5, fs=0.3, fr=0.8, aa=0.08, ad=3.0, as_=0.25, ar=0.8),
  'dxstr':     dict(unison=7, detune_cents=20, width=1.0, saw=1.0, pulse=0.2, pulse_width=0.5, sub=0.0, cutoff_hz=1400, env_oct=1.0, key_track=0.3, reso=0.1, drive=0.2, fa=0.02, fd=0.4, fs=0.6, fr=0.3, aa=0.015, ad=0.4, as_=0.8, ar=0.3),
}

RECIPES = {
 'title': {
   1: dict(t='piano', shape=[56], layer_strings=0.55, vel_scale=0.75), 2: dict(t='perc', track='perc'), 3: dict(t='synth', p='pad', shape=[68], track='pad'),
   5: dict(t='orig', track='fx'), 7: dict(t='revcrash', track='fx'), 8: dict(t='kick909', track='kick'),
   9: dict(t='guitar', shape=[32], track='rhythm1'), 10: dict(t='guitar', shape=[32, 44], track='rhythm2'),
   11: dict(t='drum', inst='Hihat_open', choke=True, track='hats'), 12: dict(t='guitar', shape=[32, 39, 44], track='rhythm1'),
   13: dict(t='strings', shape=[56], track='strings', match_tone=True), 16: dict(t='perc', track='perc'), 19: dict(t='synth', p='drone', shape=[43], track='pad2'),
   20: dict(t='piano', shape=[32, 39]), 21: dict(t='synth', p='swellbass', shape=[32], track='bass'), 22: dict(t='synth', p='swellxx', shape=[32], track='bass')},
 'desert': {
   1: dict(t='break', track='break'), 2: dict(t='ebass', lib='FingerBassYR', shape=[29], track='bass'),
   3: dict(t='brass', shape=[41, 44, 48], track='brass'), 4: dict(t='brass', shape=[41, 45, 48], track='brass'),
   5: dict(t='break', track='break'), 6: dict(t='synth', p='mellow', shape=[53], track='lead'),
   7: dict(t='ebass', lib='PickedBassYR', shape=[29], track='bass2'), 9: dict(t='guitar', shape=[41], track='rhythm1'),
   10: dict(t='guitar', shape=[41, 48, 53], track='rhythm1'), 11: dict(t='guitar', shape=[41], track='rhythm2'),
   12: dict(t='guitar', shape=[41, 48, 53], track='rhythm2', sustain=True), 13: dict(t='drum', inst='Hihat_closed', choke=True, track='hats'),
   14: dict(t='drum', inst='Ride_tip', track='cym'), 15: dict(t='synth', p='dxstr', shape=[41, 44, 48, 53], track='pad'),
   16: dict(t='synth', p='dxstr', shape=[41, 57, 60, 65], track='pad')},
 'mountain': {
   1: dict(t='synth', p='pad', shape=[33], track='pad'), 9: dict(t='ebass', lib='FingerBassYR', shape=[45], track='bass'),
   15: dict(t='synth', p='pluck', shape=[45], track='pluck'), 18: dict(t='synth', p='acid', shape=[33], track='bass2'),
   26: dict(t='guitar', shape=[45, 52, 57, 61], track='rhythm1'), 27: dict(t='guitar', shape=[45], track='rhythm1'),
   28: dict(t='guitar', shape=[45], track='rhythm2'), 29: dict(t='drum', inst='Hihat_closed', choke=True, track='hats'),
   **{i: dict(t='break', track='break') for i in range(73, 80)},
   81: dict(t='synth', p='stab', shape=[45, 52, 61, 64, 69], track='stab'), 82: dict(t='synth', p='stab', shape=[45, 52, 60, 64, 69], track='stab')},
 'coastline': {
   1: dict(t='synth', p='bell', shape=[60], track='lead'), 2: dict(t='synth', p='stab', shape=[43, 48, 51, 55, 60, 63], track='stab'),
   3: dict(t='synth', p='lead', shape=[48], track='lead2'), 4: dict(t='synth', p='lead', shape=[48, 51, 55], track='lead2'),
   5: dict(t='synth', p='lead', shape=[48, 52, 57], track='lead2'), 6: dict(t='synth', p='lead', shape=[48, 53, 56], track='lead2'),
   7: dict(t='drum', inst='Hihat_open', choke=True, track='hats'), 8: dict(t='kick909', track='kick'),
   9: dict(t='synth', p='blip1', shape=[36], track='bass2'), 10: dict(t='synth', p='blip2', shape=[36], track='bass2'), 11: dict(t='synth', p='blip3', shape=[36], track='bass2'),
   12: dict(t='synth', p='pad', shape=[36, 43, 52, 60, 62], track='pad'), 13: dict(t='break', track='break'),
   14: dict(t='synth', p='bass', shape=[36], track='bass'), 15: dict(t='synth', p='pad', shape=[36, 43, 48, 52, 55, 60, 64], track='pad')},
 'stripmine': {
   1: dict(t='orig', track='fx'), 2: dict(t='break', track='break'), 3: dict(t='guitar', shape=[36, 43, 48], track='rhythm1'),
   4: dict(t='kick909', track='kick'), 6: dict(t='synth', p='pluck', shape=[48], track='lead'), 7: dict(t='kick909', track='kick'),
   8: dict(t='kick909', track='kick'), 9: dict(t='synth', p='blip1', shape=[36], track='bass3'), 10: dict(t='synth', p='blip2', shape=[36], track='bass3'), 11: dict(t='synth', p='blip3', shape=[36], track='bass3'),
   12: dict(t='synth', p='stab', shape=[43, 48, 51, 55, 60, 63], track='stab'), 13: dict(t='synth', p='stab', shape=[36, 43, 48, 52, 55, 60, 64], track='stab'),
   14: dict(t='synth', p='sub808', shape=[36], track='sub'), 17: dict(t='synth', p='pluckbass', shape=[36], track='bass'),
   18: dict(t='synth', p='bass', shape=[36], track='bass2'), 19: dict(t='perc', track='perc2'),
   20: dict(t='synth', p='trance', shape=[48], track='lead2'), 21: dict(t='strings', shape=[36, 48, 55, 64], track='strings')},
}

def render_piece(piece, out_npz):
    R = RECIPES[piece]
    m = open('pieces/%s.xm' % piece, 'rb').read(); im, flat = inst_samples(m)
    notes, TOTAL = load_events(os.path.join(os.environ.get('EVENTS_DIR', 'pieces'), '%s.csv' % piece))
    byi = collections.defaultdict(list)
    for n in notes: byi[n['instr']].append(n)
    C5 = {i: float(np.median([n['ticks'][0][1] / 2 ** ((n['note'] - 49) / 12) for n in ns])) for i, ns in byi.items()}
    tracks = {}; report = {}
    def track(name): return tracks.setdefault(name, np.zeros((TOTAL + SR * 6, 2), np.float32))
    def smp(i):
        s0 = collections.Counter(n['sample'] for n in byi[i]).most_common(1)[0][0]; return flat[im[i][max(0, s0)]]
    def common(i): return collections.Counter(n['note'] for n in byi[i]).most_common(1)[0][0]
    def put(tname, y, n, gain, release, cut=True):
        L = len(gain); rel = int(release * SR)
        if cut: y = y[:L + rel].copy()
        g = np.concatenate([gain, np.full(max(0, len(y) - L), gain[-1])])[:len(y)]
        if cut and len(y) > L:
            k = min(rel, len(y) - L); g[L:L + k] *= np.linspace(1, 0, k); g[L + k:] = 0
        if y.ndim == 1: y = np.stack([y, y], 1)
        place(track(tname), y * g[:, None], n['start'])
    P = Piano() if any(r['t'] == 'piano' for r in R.values()) else None
    DI = DIGuitar(); ST = E.Strings() if any(r['t'] == 'strings' or r.get('layer_strings') for r in R.values()) else None
    BR = E.Brass() if any(r['t'] == 'brass' for r in R.values()) else None
    basses = {}
    di = collections.defaultdict(lambda: [np.zeros(TOTAL + SR * 6), np.zeros(TOTAL + SR * 6)])
    for i, r in R.items():
        if i not in byi: continue
        t = r['t']; tn = r.get('track', t); ns = byi[i]
        if t == 'piano':
            for n in ns:
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); v = int(np.clip(round(5 + n['ticks'][0][2] * 11 * r.get('vel_scale', 1.0)), 1, 16)); y = None
                for mm in r['shape']:
                    x = P.note(mm + s0, v); y = x if y is None else _add(y, x)
                if r.get('layer_strings'):
                    ST2 = ST or E.Strings()
                    for mm in r['shape']:
                        sx = ST2.note(mm + s0, 0.6, n['start'] % 3, length=len(sh) + int(0.5 * SR)) * r['layer_strings']
                        y = _add(y, sx)
                put('piano', y, n, g, 0.15)
        elif t == 'guitar':
            for take in (0, 1):
                for n in ns:
                    sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None
                    for k, mm in enumerate(r['shape']):
                        x = DI.note(mm + s0, rr=take * 2 + k, soft=n['ticks'][0][2] < 0.6)
                        x = np.concatenate([np.zeros(int((0.004 * k + (0.006 if take else 0)) * SR)), x])
                        y = x if y is None else _add(y, x)
                    if r.get('sustain') and len(y) < len(sh): y = E.extend(y, len(sh))
                    if np.ptp(sh) > 0.01: y = varispeed(y[:, None], 2 ** ((sh - s0) / 12))[:, 0]
                    L = len(sh); rel = int(0.03 * SR); y = y[:L + rel]
                    gg = np.concatenate([g, np.full(max(0, len(y) - L), g[-1]) * np.linspace(1, 0, max(0, len(y) - L))])[:len(y)]
                    buf = di[tn][take]; e = min(len(buf), n['start'] + len(y)); buf[n['start']:e] += (y * gg)[:e - n['start']]
        elif t == 'ebass':
            if r['lib'] not in basses: basses[r['lib']] = E.freepats(r['lib'])
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = basses[r['lib']].note(r['shape'][0] + s0, int(40 + 87 * n['ticks'][0][2]), j)
                if np.ptp(sh) > 0.01: y = varispeed(y[:, None], 2 ** ((sh - s0) / 12))[:, 0]
                put(tn, y, n, g, 0.04)
        elif t == 'synth':
            p = PRE[r['p']]
            for n in ns:
                sh, g = curves(n, C5[i]); L = len(sh) + int(p['ar'] * SR * 1.2); y = np.zeros((L, 2))
                for k, mm in enumerate(r['shape']):
                    f = hz(mm + sh); f = np.concatenate([f, np.full(L - len(f), f[-1])]); pp = dict(p)
                    if mm < 40 and len(r['shape']) > 1: pp.update(unison=2, width=0.0, sub=0.0)
                    y += synthpy.render(pp, f, len(sh) / SR, seed=(n['start'] + k) % 100003)
                put(tn, y, n, g, p['ar'], cut=False)
        elif t == 'strings':
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None
                for k, mm in enumerate(r['shape']):
                    x = ST.note(mm + s0, 0.4 + 0.5 * n['ticks'][0][2], j + k, length=len(sh) + int(0.6 * SR)); y = x if y is None else _add(y, x)
                put(tn, y, n, g, 0.5)
        elif t == 'brass':
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None
                for k, mm in enumerate(r['shape']):
                    x = BR.note(mm + s0, 0.5 + 0.5 * n['ticks'][0][2], j + k); y = x if y is None else _add(y, x)
                put(tn, y, n, g, 0.12)
        elif t in ('perc', 'drum', 'kick909', 'break', 'revcrash', 'orig'):
            s = smp(i); nb = common(i); orate = s['c5'] * 2 ** ((nb - 49) / 12)
            if t == 'perc':
                best = E.match_percussion(s['pcm'], orate, feats_low(s)); cfg = best[1]; report[i] = dict(match=cfg, score=round(best[0][0], 1))
                base = E.KIT.hit(cfg['inst'], cfg['power'], 0, E.drs_mix(cfg['inst'], cfg['room']))
                if cfg['semis']: base = res(base, SR, 2 ** (cfg['semis'] / 12))
                variants = [base] + [res(E.KIT.hit(cfg['inst'], cfg['power'], rr, E.drs_mix(cfg['inst'], cfg['room'])), SR, 2 ** (cfg['semis'] / 12)) for rr in (1, 2)]
            elif t == 'drum':
                variants = [E.KIT.hit(r['inst'], 0.75, rr, E.drs_mix(r['inst'], 0.3)) for rr in range(3)]
            elif t == 'kick909':
                play = float(np.median([(n['end'] - n['start']) / SR for n in ns])); seg = s['pcm'][:int(max(0.12, min(0.6, play)) * orate)]; best = E.fit_kick(seg, orate); report[i] = dict(fit=best[1], score=round(best[0][0], 1))
                variants = [E.kick909(*best[1])]
            elif t == 'break':
                y, hits = E.rebuild_break(s['pcm'], orate); report[i] = dict(slices=len(hits)); variants = [y]
            elif t == 'revcrash':
                c = E.KIT.hit('Crash_left_tip', 0.85, 0, E.drs_mix('Crash_left_tip', 0.8)); c = c[:int(len(s['pcm']) / orate * SR)]
                variants = [c[::-1].copy()]
            else:
                variants = [np.stack([res(s['pcm'], orate)] * 2, 1)]
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); rel = (n['note'] - nb) if t in ('perc', 'break', 'orig', 'kick909') else 0
                y = variants[j % len(variants)] * 1.0
                ratio = 2 ** ((sh - (nb - 49)) / 12)
                if np.ptp(sh) > 0.01 or abs(ratio[0] - 1) > 1e-3: y = varispeed(y, ratio)
                put(tn, y, n, g * (0.85 + 0.15 * n['ticks'][0][2]) / max(n['ticks'][0][2], 1e-3) * n['ticks'][0][2], 0.03,
                    cut=r.get('choke', False) or t in ('break', 'orig'))
    # amp the guitar DI tracks (each take through the capture and the cab), double-tracked
    for tn, (a, b) in di.items():
        model = 'Helga B 5150 BlockLetter - NoBoost.nam' if tn == 'lead' else 'Helga B 5150 BlockLetter - Boosted.nam'
        L = amp_chain(a, model); Rr = amp_chain(b, model); t = track(tn)
        t[:len(L), 0] += L * 1.2; t[:len(Rr), 1] += Rr * 1.2
    np.savez_compressed(out_npz, **tracks)
    json.dump({str(k): v for k, v in report.items()}, open(out_npz + '.json', 'w'), indent=1, default=str)
    return tracks, report

def _add(a, b):
    L = max(len(a), len(b))
    pa = ((0, L - len(a)),) + ((0, 0),) * (a.ndim - 1); pb = ((0, L - len(b)),) + ((0, 0),) * (b.ndim - 1)
    return np.pad(a, pa) + np.pad(b, pb)

def feats_low(s):
    x = s['pcm']; X = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2; f = np.fft.rfftfreq(len(x), 1 / s['c5'])
    return float(X[f < 150].sum() / X.sum())

CAB = None
def amp_chain(di, model):
    global CAB
    if CAB is None:
        c, csr = sf.read([f for f in glob.glob(os.path.join(HERE, 'ir', 'science', '**', '*V30 SM57*Brighter*.wav'), recursive=True)][0])
        CAB = res(c.mean(1) if c.ndim == 2 else c, csr)[:int(0.2 * SR)]
    if np.abs(di).max() < 1e-9: return di
    x = di / np.abs(di).max() * 10 ** (-6 / 20)
    y = NAM(os.path.join(HERE, 'nam', model))(x.astype(np.float32))
    y = fftconvolve(y, CAB)[:len(y)]
    return hp(y[:, None], 70, 2)[:, 0]

import glob
if __name__ == '__main__':
    for p in sys.argv[1:]:
        tr, rep = render_piece(p, 'pieces/%s_tracks.npz' % p)
        print(p, {k: round(float(np.sqrt(np.mean(v ** 2))), 4) for k, v in tr.items()}, rep, flush=True)
