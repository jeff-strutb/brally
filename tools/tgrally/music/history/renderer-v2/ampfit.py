"""Amp envelope of an original sample as the tracker plays it (loop included), and the linear ADSR
of the synth that reproduces it best within the note lengths the piece actually uses."""
import numpy as np
def played(s, secs):
    x = s['pcm'].astype(float); n = int(secs * s['c5'])
    if s['loop'] and len(x) < n:
        ls, ll = s['loop']; body = x[ls:ls + ll]; reps = int(np.ceil((n - len(x)) / max(ll, 1))) + 1
        x = np.concatenate([x[:ls + ll]] + [body] * reps)
    return x[:n]
def env_db(x, rate, hop_s=0.005, win_s=0.02):
    h = max(1, int(hop_s * rate)); w = int(win_s * rate)
    e = np.array([np.sqrt(np.mean(x[i:i + w] ** 2)) for i in range(0, max(1, len(x) - w), h)])
    return 20 * np.log10(e / max(e.max(), 1e-9) + 1e-4), hop_s
def adsr(t, a, d, s):
    return np.where(t < a, t / max(a, 1e-6), np.where(t < a + d, 1 - (1 - s) * (t - a) / max(d, 1e-6), s))
def fit(s, horizon):
    horizon = float(np.clip(horizon, 0.15, 4.0))
    e, dt = env_db(played(s, horizon), s['c5']); t = np.arange(len(e)) * dt
    best = None
    for a in (0.001, 0.004, 0.01, 0.02, 0.04, 0.07, 0.12, 0.2, 0.35, 0.6, 1.0, 2.0):
        for d in (0.03, 0.06, 0.1, 0.18, 0.3, 0.5, 0.8, 1.3, 2.0, 3.5):
            for sl in (0.02, 0.08, 0.15, 0.25, 0.4, 0.55, 0.7, 0.85, 1.0):
                m = 20 * np.log10(adsr(t, a, d, sl) + 1e-4)
                err = np.mean((np.maximum(m, -40) - np.maximum(e, -40)) ** 2)
                if best is None or err < best[0]: best = (err, a, d, sl)
    return dict(aa=best[1], ad=best[2], as_=best[3]), best[0]

def sample_env(s, secs=12.0, hop_s=0.002, win_s=0.008):
    """Linear amplitude envelope of the sample as the tracker plays it at its C-4 rate (loop included),
    peak 1, sampled every hop_s of sample time."""
    x = played(s, secs); r = s['c5']; h = max(1, int(hop_s * r))
    # the window spans at least two periods of the sample's lowest strong partial, so a bass note's own
    # waveform does not ripple the envelope
    seg = x[:int(0.5 * r)] - np.mean(x[:int(0.5 * r)]); ac = np.correlate(seg, seg, 'full')[len(seg) - 1:]
    lo, hi = int(r / 2000), min(len(ac) - 1, int(r / 20)); f0 = r / (lo + np.argmax(ac[lo:hi])) if hi > lo else 200.0
    w = max(2, int(min(0.05, max(win_s, 2.2 / f0)) * r))
    p = np.convolve(x ** 2, np.ones(w) / w, 'same')[::h]; e = np.sqrt(p); e /= max(e.max(), 1e-9)
    return np.maximum(e, 1e-3), hop_s

def note_env(senv, hop_s, rate_ratio, n_out, sr=48000):
    """The sample envelope along a note: rate_ratio (per output sample) is playback rate / C-4 rate."""
    pos = np.cumsum(rate_ratio[:n_out]) / sr            # seconds of sample time elapsed
    if len(pos) < n_out: pos = np.concatenate([pos, pos[-1] + (np.arange(1, n_out - len(pos) + 1) * rate_ratio[-1] / sr)])
    return np.interp(pos / hop_s, np.arange(len(senv)), senv, right=senv[-1])
