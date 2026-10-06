"""Stereo remaster renderer: plays the performance logged from the XM (note starts, per-tick pitch,
volume and pan) with modern instruments, one track per part, then mixes them."""
import csv, json, math, os, sys, collections
import numpy as np, soundfile as sf
from fractions import Fraction
from scipy.signal import resample_poly, fftconvolve, butter, sosfiltfilt, sosfilt
import xml.etree.ElementTree as ET

SR = 48000
HERE = os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'build', 'tgrally', 'music'); LIB = os.path.join(HERE, 'lib')
sys.path.insert(0, HERE)
import synthpy
from nam_infer import NAM

# ------------------------------------------------------------------ the performance
def load_events(path):
    """Per channel, a list of notes: start/end frames, instrument, note, and per-tick curves."""
    rows = list(csv.DictReader(open(path)))
    by_ch = collections.defaultdict(list)
    for r in rows: by_ch[int(r['ch'])].append(r)
    total = int(rows[-1]['frame']) + SR
    notes = []
    for ch, rr in by_ch.items():
        cur = None
        for r in rr:
            fr = int(r['frame']); act = int(r['active']); trig = int(r['trig'])
            if trig == 1 or (cur and not act):
                if cur: cur['end'] = fr; notes.append(cur); cur = None
            if trig == 1 and act:
                cur = dict(ch=ch, start=fr, instr=int(r['instr']), sample=int(r['sample']), note=int(r['note']), ticks=[])
            if cur is not None:
                cur['ticks'].append((fr, float(r['rate_hz']), float(r['vol']), int(r['pan'])))
        if cur: cur['end'] = int(rr[-1]['frame']) + SR // 50; notes.append(cur)
    return notes, total

def curves(n, c5):
    """Semitone shift from the instrument's C-4 and linear gain, per output sample over the note."""
    fr = np.array([t[0] for t in n['ticks']]) - n['start']; rate = np.array([t[1] for t in n['ticks']]); vol = np.array([t[2] for t in n['ticks']])
    L = n['end'] - n['start']; x = np.arange(L)
    sh = 12 * np.log2(np.maximum(rate, 1e-3) / c5)
    shift = np.interp(x, fr, sh); gain = np.interp(x, fr, vol)
    k = max(1, int(0.004 * SR)); gain = np.convolve(gain, np.ones(k) / k, 'same')      # de-zipper
    return shift, gain

# ------------------------------------------------------------------ helpers
def res(x, sr_in, ratio=1.0):
    r = Fraction(SR / (sr_in * ratio)).limit_denominator(4000)
    return resample_poly(x, r.numerator, r.denominator, axis=0)

def varispeed(x, ratio):
    """Read x (N x C) with a time-varying speed curve (one value per output sample)."""
    pos = np.cumsum(np.concatenate([[0.0], ratio[:-1]]))
    pos = pos[pos < len(x) - 1]
    i = pos.astype(int); f = (pos - i)[:, None]
    return x[i] * (1 - f) + x[i + 1] * f

def hp(x, hz, order=2):
    return sosfilt(butter(order, hz / (SR / 2), 'high', output='sos'), x, axis=0)

def lp(x, hz, order=2):
    return sosfilt(butter(order, hz / (SR / 2), 'low', output='sos'), x, axis=0)

def shelf_eq(x, freqs, gains_db):
    n = len(x); N = 1 << int(np.ceil(np.log2(n + 1)))
    f = np.fft.rfftfreq(N, 1 / SR)
    g = 10 ** (np.interp(np.log(np.maximum(f, 1)), np.log(freqs), gains_db) / 20)
    return np.fft.irfft(np.fft.rfft(x, N, axis=0) * g[:, None], N, axis=0)[:n]

def compress(x, thr_db=-18, ratio=3.0, att=0.01, rel=0.15, makeup_db=0.0, block=48):
    """Feed-forward compressor; the level detector runs on 1 ms blocks with attack/release smoothing."""
    n = len(x); nb = (n + block - 1) // block
    p = np.mean(x ** 2, axis=1); p = np.pad(p, (0, nb * block - n))
    lvl = np.sqrt(p.reshape(nb, block).mean(1) + 1e-12)
    a, r = math.exp(-block / (att * SR)), math.exp(-block / (rel * SR))
    env = np.empty(nb); e = 0.0
    for i in range(nb):
        v = lvl[i]; e = a * e + (1 - a) * v if v > e else r * e + (1 - r) * v; env[i] = e
    db = 20 * np.log10(env + 1e-12); over = np.maximum(0, db - thr_db)
    gb = 10 ** ((-over * (1 - 1 / ratio) + makeup_db) / 20)
    g = np.interp(np.arange(n), np.arange(nb) * block + block / 2, gb)
    return x * g[:, None]

def pan(x, p):
    """Equal-power pan of a stereo (or mono) signal, p in -1..1."""
    if x.ndim == 1: x = np.stack([x, x], 1)
    a = (p + 1) * math.pi / 4
    return np.stack([x[:, 0] * math.cos(a) * 1.414, x[:, 1] * math.sin(a) * 1.414], 1)

def reverb_ir(seconds=2.2, predelay=0.02, damp_hz=5500, seed=5):
    rng = np.random.default_rng(seed); n = int(seconds * SR)
    t = np.arange(n) / SR; env = np.exp(-6.9 * t / seconds)
    ir = rng.standard_normal((n, 2)) * env[:, None]
    ir = lp(ir, damp_hz, 1); ir = hp(ir, 180, 2)
    ir = np.concatenate([np.zeros((int(predelay * SR), 2)), ir])
    return ir / np.sqrt((ir ** 2).sum(0))

def place(track, y, at):
    e = min(len(track), at + len(y))
    if e > at: track[at:e] += y[:e - at]

# ------------------------------------------------------------------ instruments
NOTE = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11}
def midi_of(nm):
    import re; m = re.match(r'([A-G]#?)(-?\d)', nm); return NOTE[m.group(1)] + 12 * (int(m.group(2)) + 1)
def hz(m): return 440.0 * 2 ** ((m - 69) / 12)

class Piano:
    def __init__(self):
        import glob
        self.files = {}
        for f in glob.glob(os.path.join(LIB, 'SalamanderGrandPianoV3_48khz24bit', '48khz24bit', '*v*.wav')):
            b = os.path.basename(f)[:-4]; nm, v = b.split('v'); self.files[(midi_of(nm), int(v))] = f
        self.roots = sorted({k[0] for k in self.files}); self.cache = {}
    def note(self, m, vel):
        root = min(self.roots, key=lambda r: abs(r - m)); key = (root, vel)
        if key not in self.cache: self.cache[key] = sf.read(self.files[key], dtype='float64')
        x, sr = self.cache[key]
        return res(x[:int(4 * sr)], sr, hz(m) / hz(root))

class DIGuitar:
    def __init__(self, lib='EGuitarFSBS-clean'):
        import glob, re
        self.files = collections.defaultdict(list)
        for f in glob.glob(os.path.join(LIB, lib, '**', '*.flac'), recursive=True):
            m = re.match(r'([A-G]#?\d)_s(\d)_(soft_)?(\d+)\.flac', os.path.basename(f))
            if m: self.files[(midi_of(m.group(1)), bool(m.group(3)))].append(f)
        self.roots = sorted({k[0] for k in self.files if not k[1]}); self.cache = {}
    def note(self, m, rr=0, soft=False):
        root = min(self.roots, key=lambda r: abs(r - m))
        fs = sorted(self.files.get((root, soft)) or self.files[(root, False)]); f = fs[rr % len(fs)]
        if f not in self.cache:
            x, sr = sf.read(f, dtype='float64'); self.cache[f] = (x.mean(1) if x.ndim == 2 else x, sr)
        x, sr = self.cache[f]
        return res(x[:int(3 * sr)], sr, hz(m) / hz(root))

class Kit:
    def __init__(self):
        self.base = os.path.join(LIB, 'drs', 'DRSKit'); self.cache = {}
    def hits(self, inst):
        if inst not in self.cache:
            root = ET.parse(os.path.join(self.base, inst, inst + '.xml')).getroot()
            self.cache[inst] = sorted(((float(s.get('power')), s) for s in root.iter('sample')), key=lambda t: t[0])
        return self.cache[inst]
    def hit(self, inst, vel, rr, mics):
        hs = self.hits(inst); target = vel * hs[-1][0]
        near = sorted(hs, key=lambda t: abs(t[0] - target))[:4]; s = near[rr % len(near)][1]
        out = None
        for af in s.iter('audiofile'):
            g = mics.get(af.get('channel'))
            if g is None: continue
            p = os.path.join(self.base, inst, af.get('file'))
            if p not in self.cache: self.cache[p] = sf.read(p, dtype='float64', always_2d=True)
            x, sr = self.cache[p]
            ch = x[:, int(af.get('filechannel')) - 1]
            gl, gr = g
            y = np.stack([ch * gl, ch * gr], 1)
            out = y if out is None else out + y
        return res(out, sr)
