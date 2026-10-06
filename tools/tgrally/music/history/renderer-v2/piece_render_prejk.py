"""Render any of the N64 pieces from its logged performance with modern instruments, one stereo
track per part. The recipe below says, for every instrument the piece uses, what plays it; chord
shapes are the notes transcribed from the ROM sample, in equal temperament at A440."""
import sys, os, json, collections, numpy as np, soundfile as sf
from render_remaster import *
import engines as E, synthpy, modsamp, ampfit, csv
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
  'desbass':   dict(unison=3, detune_cents=8, width=0.0, saw=1.0, pulse=0.5, pulse_width=0.45, sub=0.3, cutoff_hz=1100, env_oct=1.5, key_track=0.5, reso=0.15, drive=1.5, fa=0.002, fd=0.25, fs=0.6, fr=0.08, aa=0.002, ad=0.3, as_=0.9, ar=0.06),
  'housepad':  dict(unison=3, detune_cents=12, width=0.6, saw=0.9, pulse=0.45, pulse_width=0.45, sub=0.0, cutoff_hz=1300, env_oct=1.0, key_track=0.35, reso=0.15, drive=0.5, fa=0.004, fd=0.3, fs=0.6, fr=0.2, aa=0.004, ad=0.3, as_=0.85, ar=0.2),
  'gritlead':  dict(unison=5, detune_cents=18, width=0.7, saw=1.0, pulse=0.5, pulse_width=0.4, sub=0.15, cutoff_hz=2600, env_oct=1.4, key_track=0.4, reso=0.25, drive=3.0, fa=0.003, fd=0.35, fs=0.7, fr=0.25, aa=0.004, ad=0.5, as_=0.9, ar=0.25),
  'fatbass':   dict(unison=3, detune_cents=9, width=0.0, saw=1.0, pulse=0.6, pulse_width=0.4, sub=0.85, cutoff_hz=420, env_oct=2.2, key_track=0.6, reso=0.2, drive=2.5, fa=0.002, fd=0.2, fs=0.35, fr=0.08, aa=0.001, ad=0.3, as_=0.85, ar=0.06),
  'dxstr':     dict(unison=7, detune_cents=20, width=1.0, saw=1.0, pulse=0.2, pulse_width=0.5, sub=0.0, cutoff_hz=1400, env_oct=1.0, key_track=0.3, reso=0.1, drive=0.2, fa=0.02, fd=0.4, fs=0.6, fr=0.3, aa=0.015, ad=0.4, as_=0.8, ar=0.3),
}

RECIPES = {
 'title': {
   1: dict(t='piano', shape=[56], vel_scale=0.75, attack_env=True, ch_gain={5: 0.3}), 2: dict(t='perc', track='perc'), 3: dict(t='synth', p='pad', shape=[68], track='pad'),
   5: dict(t='orig', track='fx'), 7: dict(t='revcrash', track='fx'), 8: dict(t='kick909', track='kick'),
   9: dict(t='guitar', shape=[32], track='rhythm1'), 10: dict(t='guitar', shape=[32, 44], track='rhythm2'),
   11: dict(t='drum', inst='Hihat_open', choke=True, track='hats'), 12: dict(t='guitar', shape=[32, 39, 44], track='rhythm1'),
   13: dict(t='choir', shape=[56], track='choir', splice='splice_micah_au.state', splice_octave=-12, splice_double=True, splice_ensemble=True), 16: dict(t='perc', track='perc'), 19: dict(t='synth', p='drone', shape=[44], track='pad2'),
   20: dict(t='piano', shape=[32, 39], attack_env=True), 21: dict(t='synth', p='swellbass', shape=[32], track='bass'), 22: dict(t='synth', p='swellxx', shape=[32], track='bass')},
 'desert': {
   1: dict(t='beat', kick909=True, big=True, track='break'), 2: dict(t='synth', p='desbass', shape=[29], track='bass'),
   3: dict(t='brass', shape=[41, 44, 48], track='brass'), 4: dict(t='brass', shape=[41, 45, 48], track='brass'),
   5: dict(t='beat', kick909=True, big=True, track='break'), 6: dict(t='synth', p='gritlead', shape=[53], track='lead', amp='Helga B 5150 BlockLetter - NoBoost.nam', expressive=True),
   7: dict(t='ebass', lib='PickedBassYR', shape=[29], track='bass2'), 9: dict(t='guitar', shape=[41], track='rhythm1'),
   10: dict(t='guitar', shape=[41, 48, 53], track='rhythm1'), 11: dict(t='guitar', shape=[41], track='rhythm2'),
   12: dict(t='guitar', shape=[41, 48, 53], track='rhythm2', sustain=True), 13: dict(t='drum', inst='Hihat_closed', choke=True, track='hats'),
   14: dict(t='drum', inst='Ride_tip', track='cym'), 15: dict(t='synth', p='dxstr', shape=[41, 44, 48, 53], track='pad'),
   16: dict(t='synth', p='dxstr', shape=[41, 57, 60, 65], track='pad')},
 'mountain': {
   1: dict(t='strings', shape=[45, 57], track='strings_pad', also=[dict(t='vsco', layers=[('Strings/Violin Section/susVib', [69, 76])], sus=True, rel=0.8, gains={'Strings/Violin Section/susVib': 0.15})]),
   9: dict(t='ebass', lib='FingerBassYR', shape=[45], track='bass',
           also=[dict(t='vsco', layers=[('Strings/Solo Contrabass/Pizz', [33])], gains={'Strings/Solo Contrabass/Pizz': 0.45})]),
   15: dict(t='splice', splice='splice_cello_au.state', shape=[45], merge=0.2, chord_cell=0.88, track='strings_cello'),
   18: dict(t='synth', p='desbass', shape=[33], stepped=True, track='bass2'),
   26: dict(t='guitar', shape=[45, 52, 57, 61], track='rhythm1'), 27: dict(t='guitar', shape=[45], track='rhythm1'),
   28: dict(t='guitar', shape=[45], track='rhythm2'), 29: dict(t='drum', inst='Hihat_closed', choke=True, track='hats'),
   **{i: dict(t='none') for i in range(73, 80)},
   81: dict(t='splice', splice='splice_strings_au.state', shape=[45, 52, 61, 64, 69], chord=True, track='stab'),
   82: dict(t='splice', splice='splice_strings_au.state', shape=[45, 52, 60, 64, 69], chord=True, track='stab')},
 'coastline': {
   1: dict(t='synth', p='bell', shape=[60], track='lead'), 2: dict(t='synth', p='housepad', shape=[36, 43, 51], track='stab'),
   3: dict(t='synth', p='lead', shape=[48], track='lead2'), 4: dict(t='synth', p='housepad', shape=[48, 51, 55], track='lead2'),
   5: dict(t='synth', p='housepad', shape=[48, 52, 57], track='lead2'), 6: dict(t='synth', p='housepad', shape=[48, 53, 56], track='lead2'),
   7: dict(t='drum', inst='Hihat_open', choke=True, track='hats'), 8: dict(t='kick909', track='kick'),
   9: dict(t='synth', p='blip1', shape=[36], track='bass2'), 10: dict(t='synth', p='blip2', shape=[36], track='bass2'), 11: dict(t='synth', p='blip3', shape=[36], track='bass2'),
   12: dict(t='synth', p='housepad', shape=[36, 43, 52, 62], track='pad'), 13: dict(t='tom909', track='perc'),
   14: dict(t='synth', p='fatbass', shape=[36], track='bass'), 15: dict(t='synth', p='housepad', shape=[36, 48, 52, 55], track='pad')},
 'stripmine': {
   1: dict(t='swell', track='fx'), 2: dict(t='break', track='break'), 3: dict(t='guitar', shape=[36, 43, 48], track='rhythm1'),
   4: dict(t='perc', layer909=True, track='kick'), 6: dict(t='splice', splice='splice_umbriel_au.state', shape=[48], track='arp'), 7: dict(t='perc', layer909=True, track='kick'),
   8: dict(t='perc', layer909=True, track='kick'), 9: dict(t='ebass', lib='FingerBassYR', shape=[36], track='bass3'), 10: dict(t='ebass', lib='FingerBassYR', shape=[36], track='bass3'), 11: dict(t='ebass', lib='FingerBassYR', shape=[36], track='bass3'),
   12: dict(t='splice', splice='splice_lapsteel_au.state', shape=[48, 55, 60], chord=True, track='steel'), 13: dict(t='splice', splice='splice_lapsteel_au.state', shape=[48, 55, 60], chord=True, track='steel'),
   14: dict(t='splice', splice='splice_exprstr_au.state', shape=[48, 55, 60], chord=True, track='strings_expr'), 17: dict(t='ebass', lib='PickedBassYR', shape=[36], track='bass'),
   18: dict(t='ebass', lib='FingerBassYR', shape=[36], track='bass2'), 19: dict(t='perc', track='perc2'),
   20: dict(t='guitar', shape=[48], track='lead2', stepped=True), 21: dict(t='splice', splice='splice_strings_au.state', shape=[36, 48, 55, 64], chord=True, track='strings')},
}

KITGEN = {'stripmine': dict(kicks=(4, 7, 8), bar_rows=32, snare=(8, 24), hat=4), 'mountain': dict(style='edm', active_from=tuple(range(73, 80)))}
FIT_AMP = {'title', 'desert', 'mountain', 'coastline', 'stripmine'}
def tight(y, pre_ms=1.5, thr=0.1):
    """Trim a sampled note's lead-in so its attack starts pre_ms after the note start: the recordings
    begin 0-50 ms early by varying amounts, which smears riffs against the beat."""
    a = np.abs(y if y.ndim == 1 else y.mean(1)); e = np.convolve(a, np.ones(48) / 48, 'same')
    k = int(np.argmax(e > thr * e.max())); cut = max(0, k - int(pre_ms * SR / 1000))
    return y[cut:]

def curves(n, c5):
    """Semitone shift and linear gain per output sample. The tracker sets volume once per tick, so gain
    holds each tick's value (a 3 ms ramp removes clicks); pitch glides linearly within slides but jumps
    (arpeggios, retuned notes) are held like the volume."""
    fr = np.array([t[0] for t in n['ticks']]) - n['start']; rate = np.array([t[1] for t in n['ticks']]); vol = np.array([t[2] for t in n['ticks']])
    L = n['end'] - n['start']; x = np.arange(L)
    sh = 12 * np.log2(np.maximum(rate, 1e-3) / c5)
    k = np.clip(np.searchsorted(fr, x, 'right') - 1, 0, len(fr) - 1)
    gain = vol[k]; w = max(1, int(0.003 * SR)); gain = np.convolve(np.concatenate([np.full(w, gain[0]), gain]), np.ones(w) / w, 'valid')[1:L + 1]
    lin = np.interp(x, fr, sh); held = sh[k]
    jump = np.concatenate([np.abs(np.diff(sh)) > 0.5, [False]])[k]
    shift = np.where(jump, held, lin)
    return shift, gain

def shape_to(hit, pcm, rate):
    """Shorten a kit hit so it decays no slower than the original sample (never lengthens it)."""
    def env(x, r):
        w = max(2, int(0.01 * r)); h = max(1, int(0.002 * r)); e = np.sqrt(np.convolve(x ** 2, np.ones(w) / w, 'same')[::h]); return e / max(e.max(), 1e-9), h / r
    eo, ho = env(np.asarray(pcm, float), rate); ek, hk = env(hit.mean(1) if hit.ndim == 2 else hit, SR)
    po, pk = int(np.argmax(eo)), int(np.argmax(ek))
    t = np.arange(len(hit)) / SR; tk = t - pk * hk
    o = np.interp(tk / ho + po, np.arange(len(eo)), eo, right=1e-4); k = np.interp(t / hk, np.arange(len(ek)), ek, right=1e-4)
    g = np.where(tk <= 0, 1.0, np.clip(o / np.maximum(k, 1e-6), 0, 1)); w = int(0.004 * SR)
    g = np.convolve(g, np.ones(w) / w, 'same'); g[:pk * int(hk * SR) + 1] = 1.0
    return hit * (g[:, None] if hit.ndim == 2 else g)

def row_grid(path, starts):
    """Row length in seconds around each of the given note starts (median), from the event log."""
    ch = [int(r['frame']) for r in csv.DictReader(open(path)) if r['ch'] == '0' and r['tick'] == '0']
    ch = np.array(sorted(set(ch))); d = []
    for st in starts:
        k = np.searchsorted(ch, st, 'right')
        if 0 < k < len(ch): d.append(ch[k] - ch[k - 1])
    return float(np.median(d)) / SR

def row_points(path, starts, dur):
    """Row starts (and the midpoints between them) after each note start, in seconds, the most common
    pattern among the notes: the song's real grid, swing included."""
    ch = np.array(sorted({int(r['frame']) for r in csv.DictReader(open(path)) if r['ch'] == '0' and r['tick'] == '0'}))
    pats = collections.Counter()
    for st in starts:
        k = np.searchsorted(ch, st); rows = ch[k:k + 64]; rows = rows[rows < st + dur * SR + SR // 10] - st
        if len(rows) > 1: pats[tuple(np.round(rows / SR, 3))] += 1
    rows = np.array(pats.most_common(1)[0][0]); mids = (rows[:-1] + rows[1:]) / 2
    return np.sort(np.concatenate([rows, mids, [rows[-1] + (rows[-1] - rows[-2]) / 2]]))

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
    SPIC = [E.VSCO('Strings/Cello Section/spic'), E.VSCO('Strings/Viola Section/spic'), E.VSCO('Strings/Violin Section/Spic')] if any(r.get('short_spic') for r in R.values()) else None
    basses = {}
    di = collections.defaultdict(lambda: [np.zeros(TOTAL + SR * 6), np.zeros(TOTAL + SR * 6)])
    # a part may carry layers ('also'): more instruments playing the same notes into the same track
    parts = [(i, r) for i, r in R.items()] + [(i, dict(sub, track=sub.get('track', r.get('track', r['t'])))) for i, r in R.items() for sub in r.get('also', [])]
    for i, r in parts:
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
                if r.get('attack_env'):
                    if 'aenv' not in r: e_, h_ = ampfit.sample_env(smp(i), 1.0); k_ = int(np.argmax(e_ >= 0.89)); r['aenv'] = (e_[:k_ + 1], h_)
                    e_, h_ = r['aenv']; ratio = C5[i] * 2 ** (sh[0] / 12) / smp(i)['c5']; n_ = int(len(e_) * h_ / ratio * SR)
                    a_ = np.interp(np.arange(n_) * ratio / SR / h_, np.arange(len(e_)), e_); y = y.copy(); y[:n_] *= a_[:len(y), None] if y.ndim == 2 else a_[:len(y)]
                put('piano', y * r.get('ch_gain', {}).get(n['ch'], 1.0), n, g, r.get('rel', 0.15))
        elif t == 'guitar':
            for take in (0, 1):
                for n in ns:
                    sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None
                    for k, mm in enumerate(r['shape']):
                        x = tight(DI.note(mm + s0, rr=take * 2 + k, soft=n['ticks'][0][2] < 0.6))
                        x = np.concatenate([np.zeros(int((0.004 * k + (0.006 if take else 0)) * SR)), x])
                        y = x if y is None else _add(y, x)
                    if r.get('stepped'):
                        q = np.round(sh); w_ = int(0.012 * SR); sh = np.convolve(np.concatenate([np.full(w_, q[0]), q, np.full(w_, q[-1])]), np.ones(w_) / w_, 'same')[w_:-w_]
                    if r.get('sustain') and len(y) < len(sh): y = E.extend(y, len(sh))
                    if np.ptp(sh) > 0.01: y = varispeed(y[:, None], 2 ** ((sh - s0) / 12))[:, 0]
                    L = len(sh); rel = int(0.03 * SR); y = y[:L + rel]
                    gg = np.concatenate([g, np.full(max(0, len(y) - L), g[-1]) * np.linspace(1, 0, max(0, len(y) - L))])[:len(y)]
                    buf = di[tn][take]; e = min(len(buf), n['start'] + len(y)); buf[n['start']:e] += (y * gg)[:e - n['start']]
        elif t == 'ebass':
            if r['lib'] not in basses: basses[r['lib']] = E.freepats(r['lib'])
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = tight(basses[r['lib']].note(r['shape'][0] + s0, int(40 + 87 * n['ticks'][0][2]), j))
                if np.ptp(sh) > 0.01: y = varispeed(y[:, None], 2 ** ((sh - s0) / 12))[:, 0]
                put(tn, y, n, g, 0.04)
        elif t == 'synth':
            p = dict(PRE[r['p']]); senv = None
            if piece in FIT_AMP and r.get('amp') != 'preset':
                # the original sample's own amplitude shape (attack, decay, loop pulses) drives each note
                horizon = np.percentile([(n['end'] - n['start']) / SR for n in ns], 90) * 1.3
                fa, err = ampfit.fit(smp(i), horizon); p.update(fa); p['ar'] = min(p['ar'], max(0.05, fa['ad'])); report[i] = dict(amp=fa, err_db=round(float(np.sqrt(err)), 1))
            for n in ns:
                sh, g = curves(n, C5[i]); L = len(sh) + int(p['ar'] * SR * 1.2); y = np.zeros((L, 2))
                if r.get('stepped'):
                    # slides become clean steps between the notes they pass through (no filter "yowp")
                    q = np.round(sh); w_ = int(0.015 * SR); sh = np.convolve(np.concatenate([np.full(w_, q[0]), q, np.full(w_, q[-1])]), np.ones(w_) / w_, 'same')[w_:-w_]
                if r.get('expressive'):
                    tt = np.arange(len(sh)) / SR; dep = np.clip((tt - 0.28) / 0.6, 0, 1) * 0.22
                    sh = sh + dep * np.sin(2 * np.pi * 5.3 * tt + (n['start'] % 997))
                    dur = len(sh) / SR
                    if dur > 0.35:
                        sw = np.minimum(1, tt / 0.09) ** 1.5; fade = 1 - 0.35 * np.clip((tt - 0.3) / max(dur, 1.5), 0, 1)
                        g = g * (sw * fade)[:len(g)]
                for k, mm in enumerate(r['shape']):
                    f = hz(mm + sh); f = np.concatenate([f, np.full(L - len(f), f[-1])]); pp = dict(p)
                    if mm < 40 and len(r['shape']) > 1: pp.update(unison=2, width=0.0, sub=0.0)
                    y += synthpy.render(pp, f, len(sh) / SR, seed=(n['start'] + k) % 100003) * r.get('level', 1.0)
                if senv is not None:
                    ratio = C5[i] * 2 ** (np.concatenate([sh, np.full(L - len(sh), sh[-1])]) / 12) / s_['c5']
                    y *= ampfit.note_env(senv, ehop, ratio, L)[:, None]
                put(tn, y, n, g, p['ar'], cut=False)
        elif t == 'vsco':
            # orchestral layers: each (folder, notes) layer plays the note's pitch plus its offsets
            libs = {f: E.VSCO(f) for f, _ in r['layers']}
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None; sus = r.get('sus', False)
                for f, offs in r['layers']:
                    for k, mm in enumerate(offs):
                        x = libs[f].note(mm + s0, 0.45 + 0.55 * n['ticks'][0][2], j + k, length=(len(sh) + int(0.3 * SR)) if sus else None)
                        x = (tight(x) if sus else tight(x, 8.0, 0.5)) * r.get('gains', {}).get(f, 1.0); y = x if y is None else _add(y, x)
                if r.get('glide') and np.ptp(sh) > 0.01: y = varispeed(y, 2 ** ((np.concatenate([sh, np.full(max(0, len(y) - len(sh)), sh[-1])])[:len(y)] - s0) / 12))
                put(tn, y, n, g, r.get('rel', 0.2), cut=sus)
        elif t == 'splice':
            # a part played by a Splice INSTRUMENT patch; repeated notes on one pitch can be merged into
            # one held note (a bowed line instead of a pulse)
            ev = []; env_ = np.zeros(TOTAL + SR * 6)
            for n in sorted(ns, key=lambda n: n['start']):
                sh, g = curves(n, C5[i]); v_ = 30 + 90 * n['ticks'][0][2]
                for sm_ in (r['shape'] if r.get('chord') else r['shape'][:1]):
                    m_ = int(round(sh[0])) + sm_
                    if r.get('merge') and ev and ev[-1][2] == m_ and n['start'] - ev[-1][1] < r['merge'] * SR:
                        ev[-1] = (ev[-1][0], n['end'], m_, max(ev[-1][3], v_))
                    else: ev.append((n['start'], n['end'], m_, v_))
                e0 = min(len(env_), n['start'] + len(g)); env_[n['start']:e0] = np.maximum(env_[n['start']:e0], g[:e0 - n['start']])
            if r.get('chord_cell'):
                # an arpeggio becomes its harmony: per cell, its most-played pitches bowed together
                cell = int(r['chord_cell'] * SR); cells = collections.defaultdict(collections.Counter); vel = collections.defaultdict(float)
                for a_, b_, m_, v_ in ev: cells[a_ // cell][m_] += 1; vel[a_ // cell] = max(vel[a_ // cell], v_)
                ev2 = []
                for c_ in sorted(cells):
                    top = [m_ for m_, _ in cells[c_].most_common(3)]
                    for m_ in top: ev2.append((c_ * cell, (c_ + 1) * cell - int(0.02 * SR), m_, vel[c_] * 0.85))
                # a pitch that carries on into the next cell is held, not re-bowed
                held = {}; ev = []
                for a_, b_, m_, v_ in sorted(ev2):
                    if m_ in held and held[m_][1] >= a_ - int(0.03 * SR): held[m_] = (held[m_][0], b_, m_, max(held[m_][3], v_))
                    else:
                        if m_ in held: ev.append(held[m_])
                        held[m_] = (a_, b_, m_, v_)
                ev += list(held.values())
            import splice_render
            out = splice_render.render(r['splice'], ev, len(env_))
            w_ = int(0.25 * SR); sm = np.convolve(np.maximum.accumulate(env_ * 0) + env_, np.ones(w_) / w_, 'same')
            gg = np.clip(sm / max(np.percentile(sm[sm > 0], 95), 1e-9), 0, 1.2) if (sm > 0).any() else sm
            hold = np.maximum.accumulate(np.where(gg > 0.02, np.arange(len(gg)), 0)); gg = np.maximum(gg, gg[hold] * np.exp(-(np.arange(len(gg)) - hold) / (1.2 * SR)))
            t_ = track(tn); L_ = min(len(t_), len(out)); t_[:L_] += out[:L_] * gg[:L_, None]
            report[i] = dict(notes=len(ns), played=len(ev))
        elif t == 'choir':
            CH = E.Choir(); VC = E.VowelChoir(); ST_ = E.Strings() if r.get('ch_strings') else None
            SPL = []; SPLENV = np.zeros(TOTAL + SR * 6)
            # phrasing like singers: notes that follow each other form a phrase; a phrase lasts at most one
            # breath (~5 s), and the note that runs past that ends early so the section can breathe
            phrase = {}
            for c in {n['ch'] for n in ns if n['ch'] not in r.get('ch_strings', ())}:
                seq = sorted([n for n in ns if n['ch'] == c], key=lambda n: n['start']); cur = []
                def close(cur):
                    if not cur: return
                    a, b = cur[0]['start'], cur[-1]['end']
                    for m_ in cur: phrase[id(m_)] = (a, b, m_ is cur[0], m_ is cur[-1])
                for m_ in seq:
                    if cur and (m_['start'] - cur[-1]['end'] > 0.25 * SR or m_['end'] - cur[0]['start'] > 5.5 * SR):
                        if m_['start'] - cur[-1]['end'] <= 0.25 * SR: cur[-1]['end'] = max(cur[-1]['start'] + int(0.3 * SR), cur[-1]['end'] - int(0.35 * SR))
                        close(cur); cur = []
                    cur.append(m_)
                close(cur)
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None
                if n['ch'] in r.get('ch_strings', ()):
                    # inner chord voices: a quiet string bed underneath, not more choir
                    for k, mm in enumerate(r['shape']):
                        x = ST_.note(mm + s0, 0.35, j + k, length=len(sh) + int(0.4 * SR)) * r.get('strings_gain', 0.4); y = x if y is None else _add(y, x)
                    put(tn, y, n, g, 0.35); continue
                # a sung phrase: swells in from the breath, crests, falls away into the next breath;
                # notes inside a phrase join legato instead of re-attacking
                pa, pb, first, last = phrase.get(id(n), (n['start'], n['end'], True, True))
                L_ = n['end'] - n['start']; g = g[:L_] if len(g) > L_ else np.concatenate([g, np.full(L_ - len(g), g[-1])])
                sh = sh[:L_] if len(sh) >= L_ else np.concatenate([sh, np.full(L_ - len(sh), sh[-1])])
                ta = (n["start"] + np.arange(L_) - pa) / SR; Pl = max((pb - pa) / SR, 0.6)
                arc = 0.72 + 0.28 * np.sin(np.pi * np.clip(ta / Pl, 0, 1)) ** 0.8
                swell = np.minimum(1, ta / 0.45) ** 1.6; fall = np.clip((Pl - ta) / 0.7, 0, 1) ** 0.8
                g = g * arc * swell * fall
                for k, mm in enumerate(r['shape']):
                    if r.get('splice'):
                        # sung by the plugin below (one pass over the whole line); here only the phrase shape is kept
                        SPL.append((n['start'], n['end'], mm + s0 + r.get('splice_octave', 0), 30 + 90 * n['ticks'][0][2]))
                        if r.get('splice_double') and mm + s0 >= 62:
                            SPL.append((n['start'], n['end'], mm + s0 + r.get('splice_octave', 0) - 12, 20 + 60 * n['ticks'][0][2]))
                        x = np.zeros((1, 2))
                        env_ = np.zeros(TOTAL + SR * 6); e0 = min(len(env_), n['start'] + len(g)); env_[n['start']:e0] = g[:e0 - n['start']]
                        SPLENV[:] = np.maximum(SPLENV, env_); continue
                    vw = ('Ah', 'Oh')[(pa // SR // 7) % 2]      # the vowel changes from phrase to phrase
                    x = VC.note(mm + s0, vw, 0.5 + 0.5 * n['ticks'][0][2], length=len(sh) + int(0.6 * SR), attack=0.35 if first else 0.1, legato=not first)
                    if np.ptp(sh) > 0.01: x = varispeed(x, 2 ** ((np.concatenate([sh, np.full(max(0, len(x) - len(sh)), sh[-1])])[:len(x)] - s0) / 12))
                    y = x if y is None else _add(y, x)
                if y is None or len(y) < 2: continue
                put(tn, y, n, g, 0.55 if last else 0.18)
            if SPL:
                import splice_render
                sm = np.convolve(SPLENV, np.ones(int(0.05 * SR)) / int(0.05 * SR), 'same')
                out = splice_render.ensemble(r['splice'], SPL, len(SPLENV)) if r.get('splice_ensemble') else splice_render.render(r['splice'], SPL, len(SPLENV))
                # the phrase shape (swell, crest, breath) rides the plugin's own performance; its release tail is kept
                hold = np.maximum.accumulate(np.where(sm > 0, np.arange(len(sm)), 0)); gg = np.where(sm > 0, sm, sm[hold] * np.exp(-(np.arange(len(sm)) - hold) / (0.8 * SR)))
                t_ = track(tn); t_[:len(out)] += out[:len(t_)] * np.clip(gg / max(np.percentile(gg[gg > 0], 95), 1e-9), 0, 1.2)[:len(out), None]
        elif t == 'strings':
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None
                short = r.get('short_spic') and len(sh) < 0.6 * SR
                for k, mm in enumerate(r['shape']):
                    if short:
                        lib = SPIC[0] if mm + s0 < 48 else (SPIC[1] if mm + s0 < 60 else SPIC[2])
                        x = tight(lib.note(mm + s0, 0.5 + 0.5 * n['ticks'][0][2], j + k), 8.0, 0.5)
                    else:
                        x = ST.note(mm + s0, 0.4 + 0.5 * n['ticks'][0][2], j + k, length=len(sh) + int(0.6 * SR))
                    y = x if y is None else _add(y, x)
                put(tn, y, n, g, 0.35 if short else 0.5, cut=not short)
        elif t == 'brass':
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); s0 = int(round(sh[0])); y = None
                for k, mm in enumerate(r['shape']):
                    x = BR.note(mm + s0, 0.5 + 0.5 * n['ticks'][0][2], j + k); y = x if y is None else _add(y, x)
                put(tn, y, n, g, 0.12)
        elif t in ('perc', 'drum', 'kick909', 'hat808', 'tom909', 'break', 'beat', 'revcrash', 'swell', 'orig'):
            s = smp(i); nb = common(i); orate = s['c5'] * 2 ** ((nb - 49) / 12)
            if t == 'perc':
                best = E.match_percussion(s['pcm'], orate, feats_low(s), ['Kdrum_with_contact', 'Kdrum_without_contact'] if r.get('layer909') else None); cfg = best[1]; report[i] = dict(match=cfg, score=round(best[0][0], 1))
                base = E.KIT.hit(cfg['inst'], cfg['power'], 0, E.drs_mix(cfg['inst'], cfg['room']))
                if cfg['semis']: base = res(base, SR, 2 ** (cfg['semis'] / 12))
                variants = [base] + [res(E.KIT.hit(cfg['inst'], cfg['power'], rr, E.drs_mix(cfg['inst'], cfg['room'])), SR, 2 ** (cfg['semis'] / 12)) for rr in (1, 2)]
                if r.get('layer909'):
                    # an acoustic kick on top, the fitted 909 body underneath for the low end
                    kf = E.fit_kick2(s['pcm'][:int(0.4 * orate)], orate)[1]; kb = E.kick909(*kf) * 0.8
                    variants = [np.pad(v, ((0, max(0, len(kb) - len(v))), (0, 0)))[:max(len(v), len(kb))] + np.pad(kb, ((0, max(0, len(v) - len(kb))), (0, 0))) for v in variants]
            elif t == 'drum':
                variants = [E.KIT.hit(r['inst'], 0.75, rr, E.drs_mix(r['inst'], 0.3)) for rr in range(3)]
            elif t == 'hat808':
                best = E.fit_hat(s['pcm'], orate); report[i] = dict(fit=best[1], score=round(float(best[0][0]), 1))
                variants = [E.hat808(best[1][0], best[1][1], seed=k) for k in range(3)]
            elif t == 'tom909':
                best = E.fit_tom(s['pcm'][:int(0.8 * orate)], orate); report[i] = dict(fit=[round(float(v), 3) for v in best[1]], score=round(float(best[0][0]), 1))
                variants = [E.tom909(*best[1])]
            elif t == 'kick909':
                play = float(np.median([(n['end'] - n['start']) / SR for n in ns])); seg = s['pcm'][:int(max(0.12, min(0.6, play)) * orate)]; best = (E.fit_kick2 if piece in FIT_AMP else E.fit_kick)(seg, orate); report[i] = dict(fit=[round(float(v), 3) for v in best[1]], score=round(float(best[0][0]), 1))
                variants = [E.kick909(*best[1])]
            elif t == 'break':
                y, hits = E.rebuild_break(s['pcm'], orate); report[i] = dict(slices=len(hits)); variants = [y]
            elif t == 'beat':
                grid = row_points(os.path.join(os.environ.get('EVENTS_DIR', 'pieces'), '%s.csv' % piece), [n['start'] for n in ns], len(s['pcm']) / orate)
                kf = E.fit_kick2(s['pcm'][:int(0.25 * orate)], orate)[1] if r.get('kick909') else None
                y, hits = E.rebuild_beat(s['pcm'], orate, grid, E.kick909(*kf) if kf else None, big=r.get('big', False), orch=r.get('orch', False)); report[i] = dict(grid=[round(float(g), 3) for g in grid[:8]], hits=hits, kick=kf); variants = [y]
            elif t == 'swell':
                import soundfile as sf_
                c_, sr_ = sf_.read(os.path.join(LIB, 'vsco', 'VSCO 1 Percussion/varMetal/Cymbals/susp', 'susp_hit_softmall_roll4_fff_long.wav'), always_2d=True)
                c_ = res(c_, sr_); pk_ = int(np.argmax(np.abs(c_).mean(1))); variants = [c_]
                report[i] = dict(peak_s=round(pk_ / SR, 2))
            elif t == 'revcrash':
                c = E.KIT.hit('Crash_left_tip', 0.85, 0, E.drs_mix('Crash_left_tip', 0.8)); c = c[:int(len(s['pcm']) / orate * SR)]
                variants = [c[::-1].copy()]
            else:
                variants = [np.stack([res(s['pcm'], orate)] * 2, 1)]
            if t in ('drum', 'perc') and piece in FIT_AMP:
                variants = [shape_to(v, s['pcm'], orate) for v in variants]
            for j, n in enumerate(ns):
                sh, g = curves(n, C5[i]); rel = (n['note'] - nb) if t in ('perc', 'break', 'beat', 'orig', 'kick909', 'tom909') else 0
                y = variants[j % len(variants)] * 1.0
                if t == 'swell':
                    # the roll builds across the note and crests where it ends; no pitch sweep
                    pk_ = int(np.argmax(np.abs(y).mean(1))); L_ = len(sh); y = y[max(0, pk_ - L_):pk_ + int(1.2 * SR)]
                    put(tn, y, n, np.full(L_, max(g.max(), 1e-3)), 1.0, cut=False); continue
                ratio = 2 ** ((sh - (nb - 49)) / 12)
                if np.ptp(sh) > 0.01 or abs(ratio[0] - 1) > 1e-3: y = varispeed(y, ratio)
                put(tn, y, n, g * (0.85 + 0.15 * n['ticks'][0][2]) / max(n['ticks'][0][2], 1e-3) * n['ticks'][0][2], 0.03,
                    cut=r.get('choke', False) or t in ('break', 'beat', 'orig'))
    for i, r in R.items():
        if r.get('amp') and r['t'] == 'synth' and r.get('track', r['t']) in tracks:
            t_ = tracks[r.get('track', r['t'])]; m_ = t_.mean(1).astype(float)
            if np.abs(m_).max() > 0:
                a_ = amp_chain(m_, r['amp']); a_ *= np.sqrt(np.mean(m_ ** 2)) / max(np.sqrt(np.mean(a_ ** 2)), 1e-12)
                t_[:len(a_)] = t_[:len(a_)] * 0.35 + (a_ * 0.8)[:, None]
    if piece in KITGEN:
        kg = KITGEN[piece]; path = os.path.join(os.environ.get('EVENTS_DIR', 'pieces'), '%s.csv' % piece)
    if piece in KITGEN and kg.get('style') == 'edm':
        # four-on-the-floor on the song's own (swung) rows, wherever the original beat plays
        rows = [(int(q['frame']), int(q['row'])) for q in csv.DictReader(open(path)) if q['ch'] == '0' and q['tick'] == '0']
        spans = [(n['start'], n['end']) for k_ in kg['active_from'] for n in byi.get(k_, [])]
        SA = np.array([a for a, _ in spans]); SB = np.array([b for _, b in spans])
        tk = track('kit'); hit_n = 0; KK = [E.edm_kick(k) for k in range(3)]; CL = [E.clap(k) for k in range(3)]
        dec = lambda z, tau: z[:int(tau * 6 * SR)] * np.exp(-np.arange(min(len(z), int(tau * 6 * SR))) / SR / tau)[:, None]
        for f_, rw in rows:
            if not np.any((SA <= f_) & (SB > f_)): continue
            ph = rw % 8; hits = []
            if ph % 4 == 0: hits.append((KK[hit_n % 3], 1.0))
            if ph == 4: hits.append((CL[hit_n % 3], 0.8))
            if ph % 4 == 2: hits.append((dec(E.KIT.hit('Hihat_open', 0.7, hit_n % 3, E.drs_mix('Hihat_open', 0.3)), 0.09), 0.5))
            else: hits.append((dec(E.KIT.hit('Hihat_closed', 0.6 if ph % 2 == 0 else 0.4, hit_n % 3, E.drs_mix('Hihat_closed', 0.25)), 0.035), 0.32 if ph % 2 == 0 else 0.2))
            for y, gn in hits:
                hit_n += 1; e = min(len(tk), f_ + len(y)); tk[f_:e] += y[:e - f_] * gn
    elif piece in KITGEN:
        rows = [(int(q['frame']), int(q['row'])) for q in csv.DictReader(open(path)) if q['ch'] == '0' and q['tick'] == '0']
        kicks = np.array(sorted(n['start'] for k_ in kg['kicks'] for n in byi.get(k_, [])))
        tk = track('kit'); bar = -1; prev_active = False; active = False; entry = False; hit_n = 0
        for f_, rw in rows:
            ph = rw % kg['bar_rows']
            if ph == 0:
                bar += 1; blen = kg['bar_rows'] * int(np.median(np.diff([a for a, _ in rows[:200]])))
                active = bool(len(kicks)) and np.any((kicks >= f_) & (kicks < f_ + blen))
                entry = active and not prev_active; prev_active = active
            if not active: continue
            hits = []
            if ph == 0 and (entry or bar % 8 == 0): hits.append(('Crash_left_tip' if bar % 16 else 'Crash_right_tip', 0.95, 0.8))
            if ph in kg['snare']: hits.append(('Snare', 0.9, 0.55))
            if ph % kg['hat'] == 0 and not (ph == 0 and (entry or bar % 8 == 0)):
                hits.append(('Hihat_open' if ph == kg['bar_rows'] - kg['hat'] and bar % 2 else 'Hihat_closed', 0.62 if ph % (2 * kg['hat']) == 0 else 0.45, 0.35))
            for inst, pw, room in hits:
                y = E.KIT.hit(inst, pw, hit_n % 3, E.drs_mix(inst, room)); hit_n += 1
                e = min(len(tk), f_ + len(y)); tk[f_:e] += y[:e - f_]
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
        tr, rep = render_piece(p, os.path.join(os.environ.get('EVENTS_DIR', 'pieces'), '%s_tracks.npz' % p))
        print(p, {k: round(float(np.sqrt(np.mean(v ** 2))), 4) for k, v in tr.items()}, rep, flush=True)
