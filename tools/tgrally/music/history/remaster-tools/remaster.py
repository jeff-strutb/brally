"""Remaster one N64 module: replace each instrument with modern 24-bit sources rendered per note,
keep the score, envelopes and effects, and write a module libopenmpt can render."""
import struct, glob, os, re, math, json
import numpy as np, soundfile as sf
from fractions import Fraction
from scipy.signal import resample_poly
import xml.etree.ElementTree as ET
import romsamp, modsamp

SR = 48000
LIB = os.path.join(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'build', 'tgrally', 'music'), 'lib')
NOTE = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11}

def midi_of(name):
    m = re.match(r'([A-G]#?)(-?\d)', name)
    return NOTE[m.group(1)] + 12 * (int(m.group(2)) + 1)

def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)

def resample_to(x, sr_in, ratio):
    """Play x (at sr_in) `ratio` times faster and return it at SR."""
    r = Fraction(SR / (sr_in * ratio)).limit_denominator(2000)
    return resample_poly(x, r.numerator, r.denominator, axis=0)

def mono(x):
    return x.mean(axis=1) if x.ndim == 2 else x

def fade(x, secs, fade_s=0.25):
    n = min(len(x), int(secs * SR))
    y = x[:n].copy(); f = min(int(fade_s * SR), n)
    y[n - f:] *= np.linspace(1, 0, f) ** 2
    return y

# ---------------------------------------------------------------- sources
class Salamander:
    def __init__(self):
        self.files = {}
        for f in glob.glob(os.path.join(LIB, 'SalamanderGrandPianoV3_48khz24bit', '48khz24bit', '*v*.wav')):
            b = os.path.basename(f)[:-4]; nm, v = b.split('v')
            self.files[(midi_of(nm), int(v))] = f
        self.roots = sorted({k[0] for k in self.files})
    def note(self, target_midi, vel=11):
        root = min(self.roots, key=lambda r: abs(r - target_midi))
        x, sr = sf.read(self.files[(root, vel)], dtype='float64')
        return resample_to(mono(x), sr, hz(target_midi) / hz(root))

class FSBS:
    def __init__(self, lib):
        self.files = {}
        for f in glob.glob(os.path.join(LIB, lib, '**', '*.flac'), recursive=True):
            b = os.path.basename(f)
            m = re.match(r'([A-G]#?\d)_s(\d)_(soft_)?(\d+)\.flac', b)
            if m: self.files.setdefault((midi_of(m.group(1)), bool(m.group(3))), []).append(f)
        self.roots = sorted({k[0] for k in self.files if not k[1]})
    def note(self, target_midi, rr=0, soft=False):
        root = min(self.roots, key=lambda r: abs(r - target_midi))
        fs = sorted(self.files[(root, soft)] if (root, soft) in self.files else self.files[(root, False)])
        x, sr = sf.read(fs[rr % len(fs)], dtype='float64')
        return resample_to(mono(x), sr, hz(target_midi) / hz(root))

class DRS:
    GAIN = {'OHL': (1.0, 0.0), 'OHR': (0.0, 1.0), 'AmbL': (0.5, 0.0), 'AmbR': (0.0, 0.5),
            'Kdrum_front': (0.8, 0.8), 'Kdrum_back': (0.45, 0.45), 'Snare_top': (0.8, 0.8), 'Snare_bottom': (0.3, 0.3),
            'Hihat': (0.35, 0.55), 'Ride': (0.3, 0.3), 'Tom1': (0.35, 0.35), 'Tom2': (0.35, 0.35), 'Tom3': (0.35, 0.35)}
    def __init__(self):
        self.base = os.path.join(LIB, 'drs', 'DRSKit')
    def hit(self, inst, pct=0.75, mics=None):
        root = ET.parse(os.path.join(self.base, inst, inst + '.xml')).getroot()
        ss = sorted(root.iter('sample'), key=lambda s: float(s.get('power')))
        s = ss[min(len(ss) - 1, int(pct * len(ss)))]
        out = None; sr = SR
        for af in s.iter('audiofile'):
            chn = af.get('channel')
            g = (mics or {}).get(chn, 0.0)
            if not g: continue
            x, sr = sf.read(os.path.join(self.base, inst, af.get('file')), dtype='float64', always_2d=True)
            ch = x[:, int(af.get('filechannel')) - 1]
            out = ch * g if out is None else out + ch * g
        return resample_to(out, sr, 1.0)

# ---------------------------------------------------------------- additive resynthesis of a synthetic sample
def harmonic_resynth(pcm, rate, f0, loop, target_ratio, top_hz=16000):
    """Rebuild a harmonic synth sample cleanly at SR: harmonic amplitudes are measured frame by frame
    below the original's band limit and extended above it along the measured roll-off. The result is
    played target_ratio times faster than the original (tracker semantics: pitch and time scale together)."""
    hop = 64; win = 1024
    nyq = 0.45 * rate
    H0 = int(nyq // f0)
    frames = range(0, max(1, len(pcm) - win), hop)
    t = np.arange(win) / rate; w = np.hanning(win)
    amps = []
    for i in frames:
        seg = pcm[i:i + win] * w
        a = [2 * abs(np.dot(seg, np.exp(-2j * np.pi * h * f0 * t))) / w.sum() for h in range(1, H0 + 1)]
        amps.append(a)
    A = np.array(amps)                                # frames x H0
    fr_t = np.array(list(frames)) / rate + win / 2 / rate
    f_new = f0 * target_ratio
    Hn = int(top_hz // f_new)
    # extend: log-amplitude slope from the top third of measured harmonics, per frame
    hs = np.arange(1, H0 + 1)
    top = hs > H0 * 2 // 3
    ext = np.zeros((len(A), Hn))
    for j, a in enumerate(A):
        la = np.log(a[top] + 1e-9); k = np.polyfit(np.log(hs[top]), la, 1)[0]
        k = min(k, -1.0)                               # never rising
        ext[j, :min(H0, Hn)] = a[:min(H0, Hn)]
        if Hn > H0:
            ext[j, H0:] = a[-1] * (np.arange(H0 + 1, Hn + 1) / H0) ** k
    dur = len(pcm) / rate / target_ratio
    n = int(dur * SR)
    tt = np.arange(n) / SR
    src_t = tt * target_ratio                          # time in the original sample
    y = np.zeros(n)
    rng = np.random.default_rng(1)
    for h in range(Hn):
        env = np.interp(src_t, fr_t, ext[:, h])
        if (h + 1) * f_new >= SR * 0.45: break
        y += env * np.sin(2 * np.pi * (h + 1) * f_new * tt + rng.uniform(0, 2 * np.pi))
    lp = None
    if loop:
        ls, ll = loop
        ls_new = int(ls / rate / target_ratio * SR)
        per = SR / f_new
        k = max(1, int(round((ll / rate / target_ratio * SR) / per)))
        L = int(round(k * per))
        f_fit = k * SR / L                            # exact integer periods in the loop
        # rebuild the loop region as one exact period set, and hold it
        a_loop = ext[np.searchsorted(fr_t, ls / rate)] if np.searchsorted(fr_t, ls / rate) < len(ext) else ext[-1]
        tl = np.arange(L) / SR; cyc = np.zeros(L)
        rng = np.random.default_rng(1)
        for h in range(Hn):
            if (h + 1) * f_fit >= SR * 0.45: break
            cyc += a_loop[h] * np.sin(2 * np.pi * (h + 1) * f_fit * tl + rng.uniform(0, 2 * np.pi))
        # crossfade the free-running attack into the periodic loop
        xf = min(ls_new, int(0.02 * SR))
        y = y[:ls_new]
        if xf:
            y[-xf:] = y[-xf:] * np.linspace(1, 0, xf) + cyc[-xf:] * np.linspace(0, 1, xf) if L >= xf else y[-xf:]
        y = np.concatenate([y, cyc])
        lp = (ls_new, L)
    return y, lp

# ---------------------------------------------------------------- module surgery
def parse(m):
    hs = struct.unpack_from('<I', m, 60)[0]
    npat, nins = struct.unpack_from('<HH', m, 70)
    q = 60 + hs
    for _ in range(npat):
        h, = struct.unpack_from('<I', m, q); ps, = struct.unpack_from('<H', m, q + 7); q += h + ps
    head_end = q
    inst = []
    for i in range(nins):
        isz, = struct.unpack_from('<I', m, q); n, = struct.unpack_from('<H', m, q + 27)
        hdr = bytes(m[q:q + isz]); q += isz
        smp = []
        if n:
            shs, = struct.unpack_from('<I', hdr, 29)
            sh = [bytes(m[q + j * shs:q + (j + 1) * shs]) for j in range(n)]; q += n * shs
            for s in sh:
                ln, = struct.unpack_from('<I', s, 0); smp.append([s, bytes(m[q:q + ln])]); q += ln
        inst.append([hdr, smp])
    return bytes(m[:head_end]), inst, bytes(m[q:])

def sample_header(pcm16_len, loop, vol, pan, rel, ft, name, stereo=False):
    typ = 16 | (1 if loop else 0)
    ls, ll = loop if loop else (0, 0)
    h = struct.pack('<IIIBbBBbB', 2 * pcm16_len, 2 * ls, 2 * ll, vol, ft, typ, pan, rel, 0)
    return h + name.encode('latin-1')[:22].ljust(22, b'\0')

def to16(y):
    y = np.clip(np.asarray(y), -1, 1)
    tpdf = (np.random.default_rng(7).random(len(y)) - np.random.default_rng(8).random(len(y))) / 32768
    v = np.clip(np.round((y + tpdf) * 32767), -32768, 32767).astype(np.int64)
    d = np.diff(np.concatenate([[0], v]))
    return ((d + 32768) % 65536 - 32768).astype('<i2').tobytes(), len(v)

def pitch_fields(c5):
    p = round(12 * math.log2(c5 / 8363) * 128)
    rel = (p + 64) // 128
    return rel, p - rel * 128

def rebuild_instrument(hdr, zones, orig_sample_hdr):
    """zones: list of (notes, pcm float, c5, loop). One sample per zone, keymap to it."""
    hdr = bytearray(hdr)
    if len(hdr) < 243: raise ValueError('instrument header too short')
    struct.pack_into('<H', hdr, 27, len(zones))
    km = [0] * 96
    for zi, (notes, _, _, _) in enumerate(zones):
        for n in notes: km[n - 1] = zi
    # fill gaps in the keymap with the nearest zone so stray notes still sound
    used = [n for z in zones for n in z[0]]
    for n in range(1, 97):
        if n not in used:
            km[n - 1] = min(range(len(zones)), key=lambda zi: min(abs(n - u) for u in zones[zi][0]))
    hdr[33:129] = bytes(km)
    vol = orig_sample_hdr[12]; pan = orig_sample_hdr[15]
    shs = b''; data = b''
    for zi, (notes, pcm, c5, loop) in enumerate(zones):
        rel, ft = pitch_fields(c5)
        d, n = to16(pcm)
        shs += sample_header(n, loop, vol, pan, rel, ft, 'z%d' % zi)
        data += d
    return bytes(hdr) + shs + data

def assemble(head, inst, tail, rebuilt):
    out = bytearray(head)
    for i, (hdr, smp) in enumerate(inst):
        if i in rebuilt: out += rebuilt[i]
        else:
            out += hdr
            for s, d in smp: out += s
            for s, d in smp: out += d
    out += tail
    return bytes(out)

def chord_resynth(pcm, rate, notes, loop, target_ratio, top_hz=16000):
    """Rebuild a sustained synth chord cleanly: the loop's spectrum is split into each transcribed
    note's harmonics, each note's harmonic roll-off is extended above the original band, and the
    chord is resynthesised with the original's loudness envelope and an exact seamless loop."""
    ls, ll = loop
    seg = pcm[ls:ls + ll] * np.hanning(ll)
    N = 1 << int(np.ceil(np.log2(ll * 8))); X = np.abs(np.fft.rfft(seg, N)) * 2 / np.hanning(ll).sum(); f = np.fft.rfftfreq(N, 1 / rate)
    nyq = 0.45 * rate
    parts = []                                        # (freq_at_C4, amp)
    for mid, w in notes:
        f0 = 440 * 2 ** ((mid - 69) / 12); amps = []
        for h in range(1, int(nyq // f0) + 1):
            i = np.argmin(abs(f - h * f0)); lo, hi = max(0, i - 6), i + 7
            share = sum(1 for m2, _ in notes if abs((h * f0) / (440 * 2 ** ((m2 - 69) / 12)) - round((h * f0) / (440 * 2 ** ((m2 - 69) / 12)))) < 0.01)
            amps.append(X[lo:hi].max() / max(1, share))
        amps = np.array(amps); hs = np.arange(1, len(amps) + 1)
        k = np.polyfit(np.log(hs[len(hs) // 2:]), np.log(amps[len(hs) // 2:] + 1e-9), 1)[0] if len(hs) > 3 else -2
        k = min(k, -1.0)
        Hn = int(top_hz // (f0 * target_ratio))
        for h in range(1, Hn + 1):
            a = amps[h - 1] if h <= len(amps) else amps[-1] * (h / len(amps)) ** k
            parts.append((h * f0, a))
    # loudness envelope of the original (attack, then held at the loop level)
    win = int(0.01 * rate)
    env = np.sqrt(np.convolve(pcm ** 2, np.ones(win) / win, 'same'))
    loop_lvl = env[ls:ls + ll].mean() + 1e-9
    n_attack = int(ls / rate / target_ratio * SR)
    L = int(round(1.0 * SR))                          # one-second seamless loop
    t = np.arange(n_attack + L) / SR
    y = np.zeros(len(t)); rng = np.random.default_rng(3)
    for fc, a in parts:
        fr = fc * target_ratio
        if fr >= SR * 0.45: continue
        fr = round(fr * L / SR) * SR / L              # whole cycles over the loop: seamless
        y += a * np.sin(2 * np.pi * fr * t + rng.uniform(0, 2 * np.pi))
    e = np.interp(t[:n_attack] * target_ratio, np.arange(len(env)) / rate, env / loop_lvl) if n_attack else np.array([])
    y[:n_attack] *= e
    return y, (n_attack, L)

# ---------------------------------------------------------------- analysis-driven fitting
def logspec_mag(x, rate, t0=0.08, t1=0.6, lo=40.0, hi=5000.0, cents=10):
    a, b = int(t0 * rate), min(len(x), int(t1 * rate))
    seg = x[a:b] * np.hanning(b - a)
    N = 1 << int(np.ceil(np.log2(max(len(seg), 1) * 4)))
    X = np.abs(np.fft.rfft(seg, N)) / max(1, b - a); f = np.fft.rfftfreq(N, 1 / rate)
    g = lo * 2 ** (np.arange(0, 1200 * np.log2(hi / lo), cents) / 1200)
    return g, np.interp(g, f, X)

def fit_voicing(orig_pcm, orig_rate, cand, render_note):
    """NNLS fit of candidate replacement notes (rendered at the pitch they sound at C-4) to the
    original sample's spectrum, inside the original's band. Returns [(midi, gain)]."""
    from scipy.optimize import nnls
    hi = min(0.45 * orig_rate, 5000.0)
    g, T = logspec_mag(orig_pcm, orig_rate, hi=hi)
    cols = []
    for mid in cand:
        y = render_note(mid)
        cols.append(logspec_mag(y, SR, hi=hi)[1])
    A = np.array(cols).T
    w, _ = nnls(A, T)
    return [(m, float(x)) for m, x in zip(cand, w) if x > 1e-4 * max(w.max(), 1e-12)]

def zero_phase_eq(y, rate, freqs, gains_db):
    """Apply a smooth EQ curve (dB at given freqs) to a sample without shifting it in time."""
    n = len(y); N = 1 << int(np.ceil(np.log2(n * 2)))
    f = np.fft.rfftfreq(N, 1 / rate)
    gd = np.interp(np.log(np.maximum(f, 1)), np.log(freqs), gains_db, left=gains_db[0], right=gains_db[-1])
    Y = np.fft.rfft(y, N) * 10 ** (gd / 20)
    return np.fft.irfft(Y, N)[:n]

def highpass(y, hz, rate=SR):
    from scipy.signal import butter, sosfiltfilt
    return sosfiltfilt(butter(2, hz / (rate / 2), 'high', output='sos'), y)
