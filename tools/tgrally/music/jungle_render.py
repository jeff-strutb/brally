"""Jungle, remastered: the XM performance played by modern instruments, mixed in stereo."""
import sys, os, json, numpy as np, soundfile as sf, collections
from render_remaster import *
import romsamp, modsamp

notes, TOTAL = load_events(os.environ.get('JUNGLE_EVENTS', 'jungle_events.csv'))
byi = collections.defaultdict(list)
for n in notes: byi[n['instr']].append(n)
C5 = {i: float(np.median([n['ticks'][0][1] / 2 ** ((n['note'] - 49) / 12) for n in ns])) for i, ns in byi.items()}
rng = np.random.default_rng(11)

# chord shapes at XM note C-4, in equal temperament (transcribed from each ROM sample, detuning removed)
PIANO = {3: [(57, 1.0), (60, 1.0), (64, 1.0)], 4: [(43, 0.5), (55, 1.0), (59, 1.0), (62, 1.0)], 10: [(50, 1.0)]}
GUITAR = {6: [46, 53, 58, 62], 7: [34, 46], 8: [34, 46], 12: [60]}
SYNTH_CHORD = {19: [34, 46, 49, 53], 20: [34, 46, 50, 53]}

def vel_layer(v): return int(np.clip(round(5 + v * 11), 1, 16))

tracks = {}
def track(name): return tracks.setdefault(name, np.zeros((TOTAL + SR * 4, 2)))

# ---------------- piano
P = Piano()
for i, shape in PIANO.items():
    for n in byi[i]:
        shift, gain = curves(n, C5[i]); s0 = int(round(shift[0]))
        v = vel_layer(n['ticks'][0][2]); y = None
        for m, w in shape:
            x = P.note(m + s0, v) * w
            y = x if y is None else (np.pad(y, ((0, max(0, len(x) - len(y))), (0, 0))) + np.pad(x, ((0, max(0, len(y) - len(x))), (0, 0))))
        L = len(shift); rel = int(0.12 * SR)
        y = y[:L + rel].copy()
        g = np.concatenate([gain, np.full(rel, gain[-1]) * np.linspace(1, 0, rel)])[:len(y)]
        place(track('piano'), y * g[:, None] / max(n['ticks'][0][2], 1e-3) * n['ticks'][0][2], n['start'])

# ---------------- guitars: clean DI played as chord shapes, then a real amp capture and a V30 cab, double-tracked
DI = DIGuitar()
def di_track(instrs, take):
    out = np.zeros(TOTAL + SR * 4)
    for i in instrs:
        for n in byi[i]:
            shift, gain = curves(n, C5[i]); s0 = shift[0]
            y = None
            for k, m in enumerate(GUITAR[i]):
                x = DI.note(m + int(round(s0)), rr=take * 2 + k, soft=n['ticks'][0][2] < 0.6)
                x = np.concatenate([np.zeros(int((0.004 * k + (0.006 if take else 0)) * SR)), x])     # downstroke strum
                y = x if y is None else np.pad(y, (0, max(0, len(x) - len(y)))) + np.pad(x, (0, max(0, len(y) - len(x))))
            if np.ptp(shift) > 0.01:                                              # slides and vibrato
                y = varispeed(y[:, None], 2 ** ((shift - int(round(s0))) / 12))[:, 0]
            L = len(shift); rel = int(0.03 * SR); y = y[:L + rel]
            g = np.concatenate([gain, np.full(rel, gain[-1]) * np.linspace(1, 0, rel)])[:len(y)]
            e = min(len(out), n['start'] + len(y)); out[n['start']:e] += (y * g)[:e - n['start']]
    return out
CAB, csr = sf.read([f for f in __import__('glob').glob(os.path.join(HERE, 'ir', 'science', '**', '*V30 SM57*Brighter*.wav'), recursive=True)][0])
CAB = res(CAB.mean(1) if CAB.ndim == 2 else CAB, csr)[:int(0.2 * SR)]
def amp(di, model, drive_db):
    x = di / (np.abs(di).max() + 1e-9) * 10 ** (drive_db / 20)
    y = NAM(os.path.join(HERE, 'nam', model))(x.astype(np.float32))
    y = fftconvolve(y, CAB)[:len(y)]
    return hp(y[:, None], 70, 2)[:, 0]
for name, instrs, model, drive, width in [('rhythm1', [6, 7], 'Helga B 5150 BlockLetter - Boosted.nam', -6, 0.9),
                                          ('rhythm2', [8], 'Helga B 5150 BlockLetter - Boosted.nam', -6, 0.6),
                                          ('lead', [12], 'Helga B 5150 BlockLetter - NoBoost.nam', -10, 0.15)]:
    L = amp(di_track(instrs, 0), model, drive); R = amp(di_track(instrs, 1), model, drive) if width > 0.2 else L
    t = track(name)
    a = (width + 1) * np.pi / 4
    t[:len(L), 0] += L * np.cos(np.pi / 4 - width * np.pi / 4) * 1.2; t[:len(R), 1] += R * np.cos(np.pi / 4 - width * np.pi / 4) * 1.2

# ---------------- synths
BASS = dict(unison=3, detune_cents=10, width=0.0, saw=1.0, pulse=0.6, pulse_width=0.4, sub=0.45, cutoff_hz=320, env_oct=3.0, key_track=0.7, reso=0.3, drive=2.2,
            fa=0.002, fd=0.18, fs=0.25, fr=0.08, aa=0.001, ad=0.3, as_=0.85, ar=0.06)
STAB = dict(unison=7, detune_cents=28, width=1.0, saw=1.0, pulse=0.25, pulse_width=0.5, sub=0.0, cutoff_hz=900, env_oct=3.2, key_track=0.3, reso=0.2, drive=0.6,
            fa=0.003, fd=0.45, fs=0.45, fr=0.35, aa=0.004, ad=0.5, as_=0.8, ar=0.35)
for n in byi[5]:
    shift, gain = curves(n, C5[5]); f = hz(34 + shift) ; L = len(shift) + int(0.1 * SR)
    f = np.concatenate([f, np.full(L - len(f), f[-1])]); y = synthpy.render(BASS, f, len(shift) / SR, seed=n['start'] % 997)
    g = np.concatenate([gain, np.full(L - len(gain), gain[-1])]); place(track('bass'), y * g[:, None], n['start'])
for i, shape in SYNTH_CHORD.items():
    for n in byi[i]:
        shift, gain = curves(n, C5[i]); L = len(shift) + int(0.4 * SR); y = np.zeros((L, 2))
        for k, m in enumerate(shape):
            f = hz(m + shift); f = np.concatenate([f, np.full(L - len(f), f[-1])])
            p = dict(STAB);
            if m < 40: p.update(unison=3, width=0.0, sub=0.0, cutoff_hz=500)   # chord root: no sub, the bass carries that
            y += synthpy.render(p, f, len(shift) / SR, seed=n['start'] + k)
        g = np.concatenate([gain, np.full(L - len(gain), gain[-1])]); place(track('synth'), y * g[:, None], n['start'])

# ---------------- drums (studio kit, stereo from the kit's own overheads and room)
K = Kit()
OH = {'OHL': (1.0, 0.0), 'OHR': (0.0, 1.0), 'AmbL': (0.5, 0.0), 'AmbR': (0.0, 0.5)}
DRUM = {17: ('kick', 'Kdrum_with_contact', dict(OH, Kdrum_front=(1.0, 1.0), Kdrum_back=(0.6, 0.6)), False),
        11: ('snare', 'Snare', dict(OH, Snare_top=(1.0, 1.0), Snare_bottom=(0.4, 0.4)), False),
        9:  ('hats', 'Hihat_closed', {'Hihat': (0.55, 0.85), 'OHL': (0.3, 0.0), 'OHR': (0.0, 0.3)}, True),
        18: ('hats', 'Hihat_open', {'Hihat': (0.55, 0.85), 'OHL': (0.4, 0.0), 'OHR': (0.0, 0.4)}, True)}
last_hat_end = 0
for i, (tname, inst, mics, choke) in DRUM.items():
    for j, n in enumerate(byi[i]):
        v = n['ticks'][0][2]; y = K.hit(inst, min(1.0, 0.55 + 0.45 * v), j, mics) * v
        if choke:
            L = n['end'] - n['start'] + int(0.04 * SR); y = y[:L].copy(); f = min(len(y), int(0.04 * SR)); y[-f:] *= np.linspace(1, 0, f)[:, None]
        place(track(tname), y, n['start'])

# ---------------- wack10: the original sample, cleanly resampled
m = dict(romsamp.rom_modules())['0x17fd10']; W = modsamp.xm(m)[10]
for n in byi[16]:
    shift, gain = curves(n, C5[16]); y = res(W['pcm'], W['c5'], 2 ** (shift[0] / 12))[:len(shift)]
    place(track('wack'), np.stack([y, y], 1) * gain[:len(y), None], n['start'])

np.savez_compressed(os.environ.get('JUNGLE_OUT', 'jungle_tracks.npz'), **tracks)
print('tracks', {k: round(float(np.sqrt(np.mean(v ** 2))), 5) for k, v in tracks.items()})
