"""Mix the remaster tracks: levels from the original's arrangement balance (K-weighted), then a
modern chain per track, a shared reverb, bus glue and a limiter to -14 LUFS."""
import numpy as np, soundfile as sf, json, sys, subprocess, re
from scipy.signal import lfilter, butter, sosfilt, fftconvolve
from render_remaster import hp, lp, shelf_eq, compress, reverb_ir, SR

def kweight(x):
    # ITU-R BS.1770 pre-filter (high shelf) and RLB high-pass at 48 kHz
    b1, a1 = [1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585]
    b2, a2 = [1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621]
    return lfilter(b2, a2, lfilter(b1, a1, x, axis=0), axis=0)
def kpow(x):
    y = kweight(x); return float(np.mean(np.sum(y ** 2, axis=1) if y.ndim == 2 else y ** 2))

GROUPS = {'piano': [3, 4, 10], 'rhythm1': [6, 7], 'rhythm2': [8], 'lead': [12], 'bass': [5], 'synth': [19, 20],
          'kick': [17], 'snare': [11], 'hats': [9, 18], 'wack': [16]}
T = dict(np.load(sys.argv[1] if len(sys.argv) > 1 else 'jungle_tracks.npz'))
L = min(len(v) for v in T.values())
orig = {g: sum(np.load('stems/orig_%02d.npy' % i).astype(float) for i in ins) for g, ins in GROUPS.items()}
op = {g: kpow(np.stack([o, o], 1)) for g, o in orig.items()}

CHAIN = {  # high-pass, eq points (Hz, dB), compression, reverb send
    'piano':   dict(hp=70,  eq=([60, 250, 400, 3000, 10000], [0, -2.5, -1.5, 1.5, 2.0]), comp=(-20, 2.0), send=0.22),
    'rhythm1': dict(hp=90,  eq=([80, 300, 500, 3500, 9000, 14000], [0, -2.5, -1.0, 1.0, -2.0, -6.0]), comp=None, send=0.06),
    'rhythm2': dict(hp=90,  eq=([80, 300, 500, 3500, 9000, 14000], [0, -2.5, -1.0, 1.0, -2.0, -6.0]), comp=None, send=0.06),
    'lead':    dict(hp=140, eq=([150, 400, 2500, 9000], [0, -1.5, 1.5, -2.0]), comp=(-18, 2.5), send=0.25),
    'bass':    dict(hp=30,  eq=([40, 80, 250, 1000, 3000], [1.5, 1.0, -2.0, 0.5, 1.0]), comp=(-16, 4.0), send=0.0),
    'synth':   dict(hp=150, eq=([100, 300, 1000, 5000, 12000], [0, -2.0, 0.5, 1.5, 2.0]), comp=(-18, 2.0), send=0.25),
    'kick':    dict(hp=30,  eq=([50, 100, 400, 3500, 8000], [2.0, 0.5, -3.0, 2.0, 1.0]), comp=(-14, 3.0), send=0.0),
    'snare':   dict(hp=90,  eq=([200, 800, 5000, 10000], [1.0, -1.0, 2.0, 1.5]), comp=(-16, 3.0), send=0.15),
    'hats':    dict(hp=350, eq=([400, 3000, 10000], [0, 0.5, 1.5]), comp=None, send=0.05),
    'wack':    dict(hp=60,  eq=([100, 1000], [0, 0]), comp=None, send=0.1),
}
BIAS = {'kick': 1.5, 'bass': 1.0, 'hats': -1.5}          # modern low-end weight, gentler hats
proc = {}
for g, x in T.items():
    x = x[:L]; c = CHAIN[g]
    y = hp(x, c['hp'], 2); y = shelf_eq(y, np.array(c['eq'][0], float), np.array(c['eq'][1], float))
    if c['comp']: y = compress(y, c['comp'][0], c['comp'][1])
    proc[g] = y
# levels: each track's K-weighted power in the same proportion to the others as in the original
ref = 'synth'
gain = {}
for g, y in proc.items():
    target = op[g] / op[ref]
    have = kpow(y) / kpow(proc[ref])
    gain[g] = np.sqrt(target / max(have, 1e-20)) * 10 ** (BIAS.get(g, 0) / 20)
mix = np.zeros((L, 2)); send = np.zeros((L, 2))
for g, y in proc.items():
    mix += y * gain[g]; send += y * gain[g] * CHAIN[g]['send']
wet = fftconvolve(send, reverb_ir(), axes=0)[:L]
mix += wet * 0.9
TARGET = {31: 2.0, 63: 6.0, 125: 5.0, 250: 2.0, 500: 0.5, 1000: 0.0, 2000: -2.0, 4000: -4.5, 8000: -7.5, 16000: -13.0}
def oct_share(x):
    X = np.abs(np.fft.rfft(x.mean(1))) ** 2; f = np.fft.rfftfreq(len(x), 1 / SR)
    return {c: 10 * np.log10(X[(f >= c / np.sqrt(2)) & (f < c * np.sqrt(2))].sum() + 1e-20) for c in TARGET}
sh = oct_share(mix); corr = {c: np.clip((TARGET[c] - (sh[c] - sh[1000])), -5, 5) for c in TARGET}
mix = shelf_eq(mix, np.array(list(corr), float), np.array([corr[c] for c in corr]))
mix = compress(mix, thr_db=-14, ratio=2.0, att=0.02, rel=0.25)          # bus glue
# loudness to -14 LUFS, then a look-ahead peak limiter at -1 dBFS
lufs = -0.691 + 10 * np.log10(kpow(mix) + 1e-20)
mix *= 10 ** ((-14 - lufs) / 20)
look = int(0.005 * SR); pk = np.max(np.abs(mix), axis=1)
from scipy.ndimage import maximum_filter1d
env = maximum_filter1d(pk, size=look * 2 + 1)
g = np.minimum(1.0, 0.891 / np.maximum(env, 1e-9))
k = look; g = np.convolve(g, np.ones(k) / k, 'same'); g = np.minimum(g, 1.0)
mix = mix * g[:, None]
sf.write(sys.argv[2] if len(sys.argv) > 2 else 'jungle_mix.wav', mix.astype(np.float32), SR, subtype='FLOAT')
json.dump({g: round(20 * np.log10(v), 2) for g, v in gain.items()}, open('mix_gains.json', 'w'), indent=1)
print('gains dB', {g: round(20 * np.log10(v), 1) for g, v in gain.items()}, 'peak', round(float(np.abs(mix).max()), 3))
