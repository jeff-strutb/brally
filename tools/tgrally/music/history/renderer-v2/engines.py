"""Instrument engines beyond the Jungle set: SFZ basses, VSCO strings and brass, measured drum
matching, breakbeat rebuilding and a fitted 909-style kick."""
import os, re, glob, collections, functools, math
import numpy as np, soundfile as sf
from render_remaster import SR, LIB, res, hz, midi_of, Kit, hp, lp

# ------------------------------------------------------------------ SFZ (FreePats basses)
class SFZ:
    def __init__(self, path):
        self.dir = os.path.dirname(path); txt = open(path, encoding='latin-1').read()
        txt = re.sub(r'//[^\n]*', '', txt)
        self.regions = []; group = {}; ctrl = {}; cur = None; section = None
        for tok in re.findall(r'<\w+>|[\w$]+=(?:[^=<]+?)(?=\s+[\w$]+=|\s*<|\s*$)', txt, re.S):
            if tok.startswith('<'):
                if cur is not None: self.regions.append(cur); cur = None
                section = tok
                if tok == '<group>': group = {}
                if tok == '<region>': cur = {}
                continue
            k, v = tok.split('=', 1); v = v.strip()
            {'<control>': ctrl, '<group>': group, '<global>': group}.get(section, cur if cur is not None else group)[k] = v
            if section == '<region>': cur[k] = v
            if section == '<group>': group[k] = v
            self._group = group
        if cur is not None: self.regions.append(cur)
        # merge group opcodes into their regions (regions were built with the group current at the time)
        full = []; group = {}; section = None; cur = None
        for tok in re.findall(r'<\w+>|[\w$]+=(?:[^=<]+?)(?=\s+[\w$]+=|\s*<|\s*$)', txt, re.S):
            if tok.startswith('<'):
                if cur is not None: full.append(cur); cur = None
                section = tok
                if tok == '<group>': group = {}
                if tok == '<region>': cur = dict(group)
                continue
            k, v = tok.split('=', 1); v = v.strip()
            if section == '<region>': cur[k] = v
            elif section in ('<group>', '<global>'): group[k] = v
            elif section == '<control>': ctrl[k] = v
        if cur is not None: full.append(cur)
        dp = ctrl.get('default_path', '')
        self.regions = []
        for r in full:
            if 'sample' not in r: continue
            key = r.get('key'); kc = int(r.get('pitch_keycenter', key if key else 60)) if not str(r.get('pitch_keycenter', '0')).isalpha() else midi_of(r['pitch_keycenter'])
            lo = int(r.get('lokey', key if key else 0)) if str(r.get('lokey', '0')).lstrip('-').isdigit() else midi_of(r['lokey'])
            hi = int(r.get('hikey', key if key else 127)) if str(r.get('hikey', '0')).lstrip('-').isdigit() else midi_of(r['hikey'])
            self.regions.append(dict(file=os.path.join(self.dir, dp, r['sample'].replace('\\', '/')), kc=kc, lo=lo, hi=hi,
                                     lovel=int(r.get('lovel', 0)), hivel=int(r.get('hivel', 127)), tune=float(r.get('tune', 0)) / 100))
        self.cache = {}
    def note(self, m, vel=100, rr=0):
        mm = int(round(m))
        cands = [r for r in self.regions if r['lo'] <= mm <= r['hi'] and r['lovel'] <= vel <= r['hivel']] or \
                sorted(self.regions, key=lambda r: min(abs(mm - r['lo']), abs(mm - r['hi'])))[:1]
        r = cands[rr % len(cands)]
        if r['file'] not in self.cache:
            x, sr = sf.read(r['file'], dtype='float64'); self.cache[r['file']] = (x.mean(1) if x.ndim == 2 else x, sr)
        x, sr = self.cache[r['file']]
        return res(x[:int(4 * sr)], sr, hz(m) / hz(r['kc'] + r['tune']))

def freepats(name):
    return SFZ(glob.glob(os.path.join(LIB, name, '*', '*.sfz'))[0])

# ------------------------------------------------------------------ VSCO sections
class VSCO:
    def __init__(self, folder, pat=r'_([A-G]#?\d)_v(\d)(?:_r?r?(\d))?\.wav$'):
        self.idx = collections.defaultdict(list)
        DYN = {'ppp': 1, 'pp': 2, 'p': 3, 'mp': 4, 'mf': 5, 'f': 6, 'ff': 7, 'fff': 8}
        for f in glob.glob(os.path.join(LIB, 'vsco', folder, '*.wav')):
            b = os.path.basename(f); m = re.search(pat, b)
            if m: self.idx[midi_of(m.group(1))].append((int(m.group(2)), int(m.group(3) or 1), f)); continue
            # other VSCO spellings: _RR1, _Sum suffixes, dynamics as words (harp)
            n = re.search(r'_([A-G]#?\d)(?=_|\.)', b)
            if not n: continue
            v = re.search(r'_v(\d)', b); d = re.search(r'_(ppp|pp|p|mp|mf|ff|fff|f)(\d?)(?=_|\.)', b); rr = re.search(r'_[rR][rR](\d)', b)
            vel = int(v.group(1)) if v else (DYN[d.group(1)] if d else 1)
            self.idx[midi_of(n.group(1))].append((vel, int(rr.group(1)) if rr else 1, f))
        self.roots = sorted(self.idx); self.cache = {}
        self.lo, self.hi = (self.roots[0], self.roots[-1]) if self.roots else (0, 0)
    def note(self, m, vel=0.8, rr=0, length=None):
        root = min(self.roots, key=lambda r: abs(r - m))
        layers = sorted({v for v, _, _ in self.idx[root]}); v = layers[min(len(layers) - 1, int(vel * len(layers)))]
        fs = sorted(f for vv, _, f in self.idx[root] if vv == v); f = fs[rr % len(fs)]
        if f not in self.cache: self.cache[f] = sf.read(f, dtype='float64', always_2d=True)
        x, sr = self.cache[f]
        y = res(x, sr, hz(m) / hz(root))
        if length and len(y) < length: y = extend(y, length)
        return y

def extend(y, L, xf=0.25):
    """Hold a sustained sample past its end: loop its last second with an equal-power crossfade."""
    n = int(1.0 * SR); f = int(xf * SR)
    if len(y) < n + f: return np.concatenate([y, np.zeros((L - len(y),) + y.shape[1:])])
    seg = y[-n:]; out = y.copy()
    while len(out) < L:
        a = np.sqrt(np.linspace(1, 0, f))[:, None] if y.ndim == 2 else np.sqrt(np.linspace(1, 0, f))
        b = np.sqrt(np.linspace(0, 1, f))[:, None] if y.ndim == 2 else np.sqrt(np.linspace(0, 1, f))
        out[-f:] = out[-f:] * a + seg[:f] * b; out = np.concatenate([out, seg[f:]])
    return out[:L]

class Strings:
    """Violins, violas and cellos, each note given to the section whose range centres it."""
    def __init__(self):
        self.sec = [VSCO('Strings/Cello Section/susvib'), VSCO('Strings/Viola Section/susvib'), VSCO('Strings/Violin Section/susVib')]
    def note(self, m, vel=0.7, rr=0, length=None):
        s = min(self.sec, key=lambda s: 0 if s.lo + 3 <= m <= s.hi - 5 else min(abs(m - s.lo - 3), abs(m - s.hi + 5)) + 1)
        return s.note(m, vel, rr, length)

class Brass:
    def __init__(self):
        self.tp = VSCO('Brass/Trumpet/stac'); self.tb = VSCO('Brass/Tenor Trombone/stac'); self.hn = VSCO('Brass/F Horn/stac')
    def note(self, m, vel=0.8, rr=0, length=None):
        s = self.tp if m >= 57 else (self.hn if m >= 48 else self.tb)
        return s.note(m, vel, rr)

# ------------------------------------------------------------------ measured matching for percussion
BANDS = [50 * 2 ** (k / 3) for k in range(25)]
def bdb(x, rate, secs=0.6):
    x = x[:int(secs * rate)]
    if x.ndim == 2: x = x.mean(1)
    X = np.abs(np.fft.rfft(x, max(len(x), 256))) ** 2; f = np.fft.rfftfreq(max(len(x), 256), 1 / rate)
    return np.array([10 * np.log10(X[(f >= c * 2 ** -(1 / 6)) & (f < c * 2 ** (1 / 6))].sum() + 1e-20) for c in BANDS])
def envdb(x, rate, secs=0.6, step=0.01):
    if x.ndim == 2: x = x.mean(1)
    n = int(step * rate); m = int(secs / step); x = np.concatenate([x, np.zeros(max(0, m * n - len(x)))])
    e = np.array([np.sqrt(np.mean(x[i * n:(i + 1) * n] ** 2) + 1e-12) for i in range(m)]); return np.maximum(20 * np.log10(e / e.max()), -40)
def score(o, orate, c, crate=SR):
    ob = bdb(o, orate); cb = bdb(c, crate); valid = (ob > ob.max() - 35) & (np.array(BANDS) < 0.45 * orate)
    d = (cb - cb[valid].mean()) - (ob - ob[valid].mean()); se = np.sqrt(np.mean(d[valid] ** 2))
    ee = np.sqrt(np.mean((envdb(o, orate) - envdb(c, crate)) ** 2)); return se + ee, se, ee

KIT = Kit()
MIX_CLOSE = {'Kdrum_with_contact': {'Kdrum_front': (1, 1), 'Kdrum_back': (0.6, 0.6)}, 'Kdrum_without_contact': {'Kdrum_front': (1, 1), 'Kdrum_back': (0.6, 0.6)},
             'Snare': {'Snare_top': (1, 1), 'Snare_bottom': (0.4, 0.4)}, 'Snare_rim': {'Snare_top': (1, 1)},
             'Tom1': {'Tom1': (1, 1)}, 'Tom2': {'Tom2': (1, 1)}, 'Tom3': {'Tom3': (1, 1)},
             'Hihat_closed': {'Hihat': (0.6, 0.9)}, 'Hihat_open': {'Hihat': (0.6, 0.9)}, 'Hihat_semi_open': {'Hihat': (0.6, 0.9)},
             'Ride_tip': {'Ride': (0.9, 0.6)}, 'Ride_tip_bell': {'Ride': (0.9, 0.6)}, 'Crash_left_tip': {}, 'Crash_right_tip': {}}
OHROOM = {'OHL': (1.0, 0.0), 'OHR': (0.0, 1.0), 'AmbL': (0.6, 0.0), 'AmbR': (0.0, 0.6)}
def drs_mix(inst, room):
    m = dict(MIX_CLOSE.get(inst, {}))
    for k, (l, r) in OHROOM.items():
        g = room if k.startswith('Amb') else max(0.3, room)
        if not MIX_CLOSE.get(inst): g = 1.0 if k.startswith('OH') else room      # cymbals live in the overheads
        m[k] = (l * g, r * g)
    return m

def match_percussion(pcm, rate, low_share, only=None):
    """Try the studio kit's instruments, strengths, mic mixes and pitches against one original hit."""
    insts = (['Kdrum_with_contact', 'Kdrum_without_contact', 'Tom3', 'Tom2', 'Tom1', 'Snare'] if low_share > 0.55 else
             ['Snare', 'Snare_rim', 'Tom1', 'Hihat_closed', 'Hihat_semi_open', 'Hihat_open', 'Ride_tip', 'Ride_tip_bell', 'Crash_left_tip', 'Crash_right_tip'])
    best = None
    for inst in (only or insts):
        for pw in (0.5, 0.85):
            for room in (0.2, 0.7):
                try: y = KIT.hit(inst, pw, 0, drs_mix(inst, room))
                except Exception: continue
                for st in range(-7, 8, 1):
                    yy = res(y, SR, 2 ** (st / 12)) if st else y
                    sc = score(pcm, rate, yy)
                    if best is None or sc[0] < best[0][0]: best = (sc, dict(inst=inst, power=pw, room=room, semis=st))
    return best

# ------------------------------------------------------------------ breakbeats, rebuilt slice by slice
def onsets(x, rate, thr=0.3):
    hop = int(0.005 * rate); w = int(0.02 * rate)
    fr = [np.abs(np.fft.rfft(x[i:i + w] * np.hanning(w))) for i in range(0, len(x) - w, hop)]
    flux = np.array([0] + [np.maximum(fr[i] - fr[i - 1], 0).sum() for i in range(1, len(fr))])
    from scipy.signal import find_peaks
    pk, _ = find_peaks(flux, height=flux.max() * thr, distance=int(0.06 * rate / hop))
    return [max(0, p * hop - hop) for p in pk]

TEMPL = [('Kdrum_with_contact', 0.8), ('Snare', 0.8), ('Hihat_closed', 0.7), ('Hihat_open', 0.7), ('Ride_tip', 0.7), ('Crash_left_tip', 0.8)]
@functools.lru_cache(None)
def template(inst, pw):
    return KIT.hit(inst, pw, 0, drs_mix(inst, 0.4))

def rebuild_break(pcm, rate):
    """Each slice of the original loop becomes the mix of kit hits whose spectra best explain it."""
    from scipy.optimize import nnls
    on = onsets(pcm, rate); on = sorted(set([0] + on)); edges = on + [len(pcm)]
    out = np.zeros((int(len(pcm) / rate * SR) + SR, 2)); hits = []
    T = {k: template(k, p) for k, p in TEMPL}
    for a, b in zip(edges[:-1], edges[1:]):
        seg = pcm[a:min(b, a + int(0.15 * rate))]
        if len(seg) < int(0.01 * rate) or np.sqrt(np.mean(seg ** 2)) < 0.01: continue
        ob = 10 ** (bdb(seg, rate, 0.15) / 20); valid = np.array(BANDS) < 0.45 * rate
        A = np.array([10 ** (bdb(T[k], SR, 0.15) / 20)[valid] for k, _ in TEMPL]).T
        w, _ = nnls(A, ob[valid]); lvl = np.sqrt(np.mean(seg ** 2))
        at = int(a / rate * SR); chosen = []
        for (k, _), wi in zip(TEMPL, w):
            if wi > 0.15 * w.max():
                y = T[k] * wi; e = min(len(out), at + len(y)); out[at:e] += y[:e - at]; chosen.append(k.split('_')[0])
        hits.append((round(a / rate, 3), chosen))
    return out, hits

# ------------------------------------------------------------------ 909-style kick, fitted to the original
def kick909(f_start, f_end, pitch_tau, decay_tau, click=0.3, length=0.8):
    t = np.arange(int(length * SR)) / SR
    f = f_end + (f_start - f_end) * np.exp(-t / pitch_tau)
    ph = 2 * np.pi * np.cumsum(f) / SR
    y = np.sin(ph) * np.exp(-t / decay_tau)
    n = np.random.default_rng(3).standard_normal(len(t)) * np.exp(-t / 0.004) * click
    y = y + hp(n[:, None], 2000, 2)[:, 0]
    y = np.tanh(1.6 * y) / np.tanh(1.6)
    return np.stack([y, y], 1)

def fit_kick(pcm, rate):
    best = None
    for fs in (120, 160, 220, 300):
        for fe in (38, 45, 52, 60):
            for pt in (0.02, 0.04, 0.07):
                for dt in (0.12, 0.2, 0.3, 0.45, 0.65):
                    y = kick909(fs, fe, pt, dt)
                    sc = score(pcm, rate, y)
                    if best is None or sc[0] < best[0][0]: best = (sc, (fs, fe, pt, dt))
    return best

# ------------------------------------------------------------------ beat rebuild on the song's grid
def band_env(x, rate, hop_s=0.001):
    """Band levels in dB every millisecond, centred: filtered, squared, smoothed (longer for the low band)."""
    from scipy.signal import butter, sosfiltfilt
    ny = rate / 2; out = {}; hop = max(1, int(hop_s * rate))
    spec = dict(lo=(None, 150, 0.012), mid=(150, 2000, 0.004), snr=(2000, 5000, 0.003), hi=(5000, None, 0.003))
    for k, (a, b, sm) in spec.items():
        if a is not None and a >= ny * 0.95: out[k] = np.full(len(x) // hop + 1, -99.0); continue
        b = None if b is None or b >= ny * 0.95 else b
        sos = butter(4, [a / ny, b / ny], 'band', output='sos') if a and b else butter(4, (a or b) / ny, 'high' if a else 'low', output='sos')
        y = sosfiltfilt(sos, x) ** 2; w = max(1, int(sm * rate))
        e = np.convolve(y, np.ones(w) / w, 'same')[::hop]; out[k] = 10 * np.log10(e + 1e-12)
    return out, hop / rate

def transcribe_beat(pcm, rate, grid_s):
    pts = grid_s if np.ndim(grid_s) else np.arange(0, len(pcm) / rate - 0.02, grid_s)
    """Hits of a drum loop, one test per grid point: which bands jump there, and by how much."""
    E_, dt = band_env(pcm, rate); n = len(E_['lo']); hits = []
    top = {k: v.max() for k, v in E_.items()}
    for g in pts:
        if g > len(pcm) / rate - 0.02: continue
        i0 = int(g / dt)
        pre = lambda k: E_[k][max(0, i0 - int(0.03 / dt)):max(0, i0 - int(0.006 / dt))].max() if i0 > int(0.01 / dt) else E_[k].max() - 16
        post = lambda k: E_[k][max(0, i0 - int(0.004 / dt)):min(n, i0 + int((0.045 if k == 'lo' else 0.025) / dt))].max()
        rise = {k: post(k) - pre(k) for k in E_}; lvl = {k: post(k) - top[k] for k in E_}
        h = []
        if rise['lo'] >= 10 and lvl['lo'] > -18: h.append(('Kdrum_with_contact', lvl['lo']))
        if rise['mid'] >= 9 and rise['snr'] >= 8 and lvl['mid'] > -20 and lvl['snr'] > -20: h.append(('Snare', max(lvl['mid'], lvl['snr'])))
        elif rate < 12000 and rise['snr'] >= 9 and lvl['snr'] > -18:
            h.append(('Hihat_closed', lvl['snr']))
        elif rise['hi'] >= 9 and lvl['hi'] > -22:
            j = i0 + int(0.03 / dt); k2 = min(n, i0 + int(0.12 / dt)); sus = E_['hi'][j:k2].min() if k2 > j else -99
            h.append(('Hihat_open' if sus > post('hi') - 10 else 'Hihat_closed', lvl['hi']))
        if h: hits.append((round(float(g), 4), h))
    return hits

def rebuild_beat(pcm, rate, grid_s, kick=None, big=False, orch=False):
    """The loop as kit hits placed exactly on the grid; `kick` replaces the kit kick (a fitted 909 body)."""
    hits = transcribe_beat(pcm, rate, grid_s)
    out = np.zeros((int(len(pcm) / rate * SR) + SR, 2))
    for j, (g, hs) in enumerate(hits):
        at = int(round(g * SR))
        for inst, lv in hs:
            pw = float(np.clip(1.0 + lv / 30, 0.35, 1.0))
            if JUNGLE_KIT:
                y = jhit(inst, float(np.clip(1.0 + lv / 30, 0.35, 1.0)), j)
            elif big and inst.startswith('Kdrum'):
                # heavy kick: the fitted body (or the kit kick) plus a long 50 Hz sub under it
                body = kick if kick is not None else KIT.hit(inst, 1.0, j % 3, drs_mix(inst, 0.3))
                sub = kick909(110, 50, 0.03, 0.25 if orch else 0.42, click=0.0, length=0.9) * 0.9
                clk = KIT.hit(inst, 1.0, j % 3, drs_mix(inst, 0.3)) * 0.6
                L_ = max(len(body), len(sub), len(clk)); y = np.zeros((L_, 2)); y[:len(body)] += body * 10 ** (lv / 20); y[:len(sub)] += sub * 10 ** (lv / 20); y[:len(clk)] += clk
            elif big and inst == 'Snare':
                y = KIT.hit(inst, min(1.0, pw + 0.2), j % 3, drs_mix(inst, 0.8)) * 1.3
            elif inst.startswith('Kdrum') and kick is not None:
                a_, b_ = kick * 10 ** (lv / 20), KIT.hit(inst, pw, j % 3, drs_mix(inst, 0.3)) * 0.35; y = np.zeros((max(len(a_), len(b_)), 2)); y[:len(a_)] += a_; y[:len(b_)] += b_
            else:
                y = KIT.hit(inst, pw, j % 3, drs_mix(inst, 0.35))
            if orch and inst.startswith('Kdrum') and not JUNGLE_KIT:
                # orchestral weight under the kit kick: a timpani stroke and the concert bass drum
                dk = lambda z: z[:int(0.6 * SR)] * np.exp(-np.maximum(np.arange(min(len(z), int(0.6 * SR))) / SR - 0.04, 0) / 0.14)[:, None]
                tp = dk(TIMP.hit(j)) * 0.7 * 10 ** (lv / 20); bd = dk(BDRUM.hit(j)) * 0.8 * 10 ** (lv / 20)
                L_ = max(len(y), len(tp), len(bd)); z = np.zeros((L_, 2)); z[:len(y)] += y; z[:len(tp)] += tp; z[:len(bd)] += bd; y = z
            e = min(len(out), at + len(y)); out[at:e] += y[:e - at]
    return out, [(g, [k.split('_')[0] for k, _ in hs]) for g, hs in hits]

class OneShots:
    """A folder of unpitched hits (round robins), loudest takes first."""
    def __init__(self, folder, pick, semis=0.0):
        self.semis = semis; self.fs = sorted(f for f in glob.glob(os.path.join(LIB, 'vsco', folder, '*.wav')) if re.search(pick, os.path.basename(f))); self.c = {}
    def hit(self, k):
        f = self.fs[k % len(self.fs)]
        if f not in self.c:
            x, sr = sf.read(f, always_2d=True); x = res(x, sr, 2 ** (self.semis / 12))
            e = np.convolve(np.abs(x.mean(1)), np.ones(48) / 48, 'same'); k = int(np.argmax(e > 0.3 * e.max())); x = x[max(0, k - 96):]
            self.c[f] = x / max(np.abs(x).max(), 1e-9)
        return self.c[f]
TIMP = OneShots('Percussion/Timpani', r'Timpani1_Hit_v3|Timpani1_Hit_v1', semis=45 - 41.43)
BDRUM = OneShots('VSCO 1 Percussion/drums/bass', r'_ff|_f_')

# ------------------------------------------------------------------ analog drum voices, fitted to the original
def hat808(decay_tau, tone_hz=8000, length=None, seed=0):
    """808-style hat: six detuned square oscillators at the classic metallic ratios, band-passed, with an
    exponential decay; a little noise for air. Stereo with a slight width."""
    from scipy.signal import butter, sosfilt
    length = length or min(1.5, decay_tau * 7 + 0.02); n = int(length * SR); t = np.arange(n) / SR
    fr = np.array([205.3, 304.4, 369.6, 522.7, 540.0, 800.0]) * 1.0
    x = sum(np.sign(np.sin(2 * np.pi * f * t + k)) for k, f in enumerate(fr)) / 6.0
    x = x + np.random.default_rng(seed).standard_normal(n) * 0.25
    sos = butter(2, [tone_hz * 0.7 / (SR / 2), min(0.95, tone_hz * 1.6 / (SR / 2))], 'band', output='sos'); y = sosfilt(sos, x)
    y = sosfilt(butter(2, 6000 / (SR / 2), 'high', output='sos'), y)
    env = np.exp(-t / decay_tau) * np.minimum(1, t / 0.0008); y = y * env; y /= max(np.abs(y).max(), 1e-9)
    d = int(0.0004 * SR); return np.stack([y, np.concatenate([np.zeros(d), y[:-d]]) * 0.9 + y * 0.1], 1)

def fit_hat(pcm, rate):
    best = None
    for dt in (0.012, 0.02, 0.035, 0.05, 0.08, 0.12, 0.18, 0.26, 0.38, 0.55):
        for tone in (6000, 7500, 9000, 11000):
            if tone * 0.7 > 0.45 * max(rate, 22050) * 2: continue
            sc = score(pcm, rate, hat808(dt, tone))
            if best is None or sc[0] < best[0][0]: best = (sc, (dt, tone))
    return best

def tom909(f_start, f_end, pitch_tau, decay_tau, noise=0.25, length=None):
    """909-style tom: a sine that falls from f_start to f_end, a noise burst through a band-pass around
    the tone, gentle saturation."""
    from scipy.signal import butter, sosfilt
    length = length or min(1.5, decay_tau * 6 + 0.05); t = np.arange(int(length * SR)) / SR
    f = f_end + (f_start - f_end) * np.exp(-t / pitch_tau); y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / decay_tau)
    nz = np.random.default_rng(5).standard_normal(len(t)) * np.exp(-t / min(0.05, decay_tau / 3))
    nz = sosfilt(butter(2, [min(0.9, 1500 / (SR / 2)), min(0.95, 5000 / (SR / 2))], 'band', output='sos'), nz)
    y = np.tanh(1.3 * (y + noise * nz / max(np.abs(nz).max(), 1e-9))) / np.tanh(1.3); return np.stack([y, y], 1)

def fit_tom(pcm, rate):
    """Measure the tone's settling frequency first, then fit the sweep, decay and noise around it."""
    x = np.asarray(pcm, float); seg = x[int(0.03 * rate):int(0.2 * rate)]
    N = 1 << 16; X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N)); f = np.fft.rfftfreq(N, 1 / rate)
    band = (f > 40) & (f < 1000); f0 = float(f[band][np.argmax(X[band])]); best = None
    for up in (1.0, 1.15, 1.35, 1.7):
        for pt in (0.01, 0.03, 0.06):
            for dt in (0.08, 0.15, 0.25, 0.4, 0.7):
                for nz in (0.1, 0.3, 0.6):
                    sc = score(pcm, rate, tom909(f0 * up, f0, pt, dt, nz))
                    if best is None or sc[0] < best[0][0]: best = (sc, (f0 * up, f0, pt, dt, nz))
    return best

def low_env(x, rate, secs=0.45):
    from scipy.signal import butter, sosfiltfilt
    x = np.asarray(x, float); x = x.mean(1) if x.ndim == 2 else x
    y = sosfiltfilt(butter(4, 150 / (rate / 2), 'low', output='sos'), x)[:int(secs * rate)]; h = int(0.01 * rate)
    e = np.array([20 * np.log10(np.sqrt(np.mean(y[k:k + h] ** 2)) + 1e-9) for k in range(0, len(y) - h, h)]); return np.maximum(e - e.max(), -40)

def fit_kick2(pcm, rate):
    """Kick fit that measures where the body's pitch settles and matches the low band's decay too."""
    x = np.asarray(pcm, float); seg = x[int(0.08 * rate):int(0.3 * rate)]
    N = 1 << 16; X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N)); f = np.fft.rfftfreq(N, 1 / rate)
    band = (f > 30) & (f < 200); fe0 = float(f[band][np.argmax(X[band])]); lo = low_env(pcm, rate); best = None
    for fe in (fe0,):
        for up in (1.6, 2.2, 3.0, 4.0):
            for pt in (0.01, 0.02, 0.04):
                for dt in (0.12, 0.2, 0.3, 0.45, 0.65):
                    y = kick909(fe * up, fe, pt, dt, length=0.9); sc = score(pcm, rate, y)
                    le = low_env(y, SR); n = min(len(le), len(lo)); lt = np.sqrt(np.mean((le[:n] - lo[:n]) ** 2))
                    tot = sc[0] + lt
                    if best is None or tot < best[0][0]: best = ((tot, sc[1], sc[2], lt), (fe * up, fe, pt, dt))
    return best

# ------------------------------------------------------------------ "aah" choir (Sonatina Symphonic Orchestra chorus, via Virtual Playing Orchestra)
class Choir:
    """Female and male "a" sections, sampled chromatically with loop points; each note goes to the
    section that has it (or the nearest sample), looped for as long as the note lasts, faded in softly."""
    NAMES = ['c', 'c#', 'd', 'd#', 'e', 'f', 'f#', 'g', 'g#', 'a', 'a#', 'b']
    def __init__(self, root=os.path.join(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'build', 'tgrally', 'music'), 'lib', 'choir')):
        import struct, re, glob
        self.s = {}
        for f in glob.glob(os.path.join(root, 'chorus-*-PB-loop.wav')):
            mm = re.search(r'chorus-(female|male)-([a-g]#?)(\d)-PB', os.path.basename(f))
            midi = 12 * (int(mm.group(3)) + 1) + self.NAMES.index(mm.group(2))
            x, sr = sf.read(f); b = open(f, 'rb').read(); k = b.find(b'smpl'); lp = None
            if k > 0 and struct.unpack('<I', b[k + 36:k + 40])[0] > 0: lp = struct.unpack('<II', b[k + 52:k + 60])
            self.s[(mm.group(1), midi)] = (x, sr, lp)
    def ensemble(self, m, vel=0.7, length=None, attack=0.12, legato=False, seed=0):
        """A section rather than one sample: three takes from neighbouring samples, each retuned to the
        note with a few cents of spread and a few ms of offset. Legato notes enter from the sustain."""
        rng = np.random.default_rng(seed); out = None
        for k, (nb, gain) in enumerate(((0, 1.0), (-1, 0.7), (1, 0.7))):
            y = self.note(m, vel, length, attack, near=nb, cents=rng.uniform(-7, 7), legato=legato)
            d = int(rng.uniform(0, 0.03) * SR) if k else 0; y = np.concatenate([np.zeros((d, 2)), y]) * gain
            out = y if out is None else (np.pad(out, ((0, max(0, len(y) - len(out))), (0, 0))) + np.pad(y, ((0, max(0, len(out) - len(y))), (0, 0))))
        return out / 1.6
    def note(self, m, vel=0.7, length=None, attack=0.12, near=0, cents=0.0, legato=False):
        sec = 'female' if m >= 67 else 'male'
        keys = sorted([k for k in self.s if k[0] == sec] or list(self.s), key=lambda k: abs(k[1] - m))
        key = keys[0]
        if near:
            cand = [k for k in self.s if k[0] == key[0] and k[1] == key[1] + near]; key = cand[0] if cand else key
        x, sr, lp = self.s[key]
        ratio = 2 ** ((m - key[1] + cents / 100) / 12); need = int((length or 2 * SR) / SR * sr * ratio) + 16
        if legato and lp: x = x[max(0, lp[0] - int(0.08 * sr)):]; lp = (int(0.08 * sr), lp[1] - lp[0] + int(0.08 * sr))
        if lp and len(x) < need:
            a, b = lp; body = x[a:b + 1]; parts = [x[:b + 1]]; tot = b + 1
            while tot < need: parts.append(body); tot += len(body)
            x = np.concatenate(parts)
        y = res(x[:need], sr, ratio)
        if y.ndim == 1: y = np.stack([y, y], 1)
        n = int(attack * SR); y[:n] *= np.linspace(0, 1, min(n, len(y)))[:, None][:len(y[:n])]
        return y * (0.35 + 0.65 * vel)

# ------------------------------------------------------------------ EDM kit voices
def clap(seed=0, decay=0.16):
    """Hand clap: three quick noise bursts then a short tail, band-passed around 1-3 kHz, with a
    little snare from the kit underneath for body."""
    from scipy.signal import butter, sosfilt
    rng = np.random.default_rng(seed); n = int(0.5 * SR); t = np.arange(n) / SR; x = rng.standard_normal(n)
    env = np.zeros(n)
    for k, d in enumerate((0.0, 0.011, 0.022)): env += (t >= d) * np.exp(-np.maximum(t - d, 0) / 0.006) * (0.8 if k < 2 else 1.0)
    env += (t >= 0.022) * np.exp(-np.maximum(t - 0.022, 0) / decay) * 0.5
    y = sosfilt(butter(2, [900 / (SR / 2), 3200 / (SR / 2)], 'band', output='sos'), x * env); y /= np.abs(y).max()
    d_ = int(0.0006 * SR); st = np.stack([y, np.concatenate([np.zeros(d_), y[:-d_]])], 1)
    sn = KIT.hit('Snare', 0.7, seed % 3, drs_mix('Snare', 0.4))[:n] * 0.45
    out = np.zeros((max(n, len(sn)), 2)); out[:n] += st * 0.8; out[:len(sn)] += sn; return out

def edm_kick(seed=0):
    body = kick909(165, 49, 0.028, 0.3, click=0.35, length=0.7)
    clk = KIT.hit('Kdrum_with_contact', 0.9, seed % 3, drs_mix('Kdrum_with_contact', 0.1))[:int(0.08 * SR)] * 0.25
    body[:len(clk)] += clk; return body

# ------------------------------------------------------------------ Mihai Sorohan vowel ensemble (six singers, SATB)
class VowelChoir:
    """A mixed choir recorded singing sustained vowels (Ah, Oh, E, I, U) on the white keys C3-C6. Each note
    uses the nearest recording retuned, held past its end by a crossfaded loop of its steady part."""
    def __init__(self, root=os.path.join(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'build', 'tgrally', 'music'), 'lib', 'mihai')):
        self.s = {}
        for f in glob.glob(os.path.join(root, 'choir * samples', '*_loud.wav')):
            b = os.path.basename(f); m = re.match(r'(Ah|Oh|E|I|U)_([A-G]#?\d)_loud\.wav', b)
            if m: self.s[(m.group(1), midi_of(m.group(2)))] = f
        self.cache = {}
    def note(self, m, vowel='Ah', vel=0.8, length=None, attack=0.15, legato=False):
        keys = [k for k in self.s if k[0] == vowel] or list(self.s)
        key = min(keys, key=lambda k: abs(k[1] - m)); f = self.s[key]
        if f not in self.cache:
            x, sr = sf.read(f, always_2d=True); x = res(x, sr)
            e = np.convolve(np.abs(x.mean(1)), np.ones(96) / 96, 'same'); k = int(np.argmax(e > 0.05 * e.max())); self.cache[f] = x[k:]
        x = self.cache[f]
        if legato: x = x[int(0.18 * SR):]
        y = res(x, SR, 2 ** ((m - key[1]) / 12))
        if length and len(y) < length: y = extend(y, length, xf=0.4)
        n = int(attack * SR); y = y.copy(); y[:n] *= np.linspace(0, 1, min(n, len(y)))[:, None][:len(y[:n])]
        return y * (0.4 + 0.6 * vel)

# ------------------------------------------------------------------ the Jungle drum kit: one recipe for every piece
# DRSKit exactly as the approved Jungle remaster uses it: stereo overheads hard left/right with the room mics,
# the kick's front/back mics, the snare's top/bottom mics, the hats' own mic with a little overhead. Hit strength
# follows the note's volume; every hit takes the next recorded take. Nothing synthesised is layered in.
JUNGLE_KIT = True
_OH = {'OHL': (1.0, 0.0), 'OHR': (0.0, 1.0), 'AmbL': (0.5, 0.0), 'AmbR': (0.0, 0.5)}
def jmics(inst):
    if inst.startswith('Kdrum'): return dict(_OH, Kdrum_front=(1.0, 1.0), Kdrum_back=(0.6, 0.6))
    if inst.startswith('Snare'): return dict(_OH, Snare_top=(1.0, 1.0), Snare_bottom=(0.4, 0.4))
    if inst == 'Hihat_closed': return {'Hihat': (0.55, 0.85), 'OHL': (0.3, 0.0), 'OHR': (0.0, 0.3)}
    if inst.startswith('Hihat'): return {'Hihat': (0.55, 0.85), 'OHL': (0.4, 0.0), 'OHR': (0.0, 0.4)}
    if inst.startswith('Tom'): return dict(_OH, **{inst: (1.0, 1.0)})
    return dict(_OH)                                   # ride, crashes: the overheads and the room
def jhit(inst, v, rr, semis=0.0):
    y = KIT.hit(inst, min(1.0, 0.55 + 0.45 * float(v)), rr, jmics(inst))
    return res(y, SR, 2 ** (semis / 12)) if semis else y
