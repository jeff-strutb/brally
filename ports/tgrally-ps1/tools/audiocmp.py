#!/usr/bin/env python3
"""audiocmp.py PS1_WAV MAC_RAW -- the port's sound against the native port's
(TGR_WAVDUMP raw 16-bit stereo at 22 kHz): the loudness envelopes aligned by
their best correlation, the level ratio, and the spectral centroid (below 10 kHz) of
2-second stretches."""
import sys
import wave

import numpy as np

w = wave.open(sys.argv[1])
d = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, 2).astype(float)
m = np.frombuffer(open(sys.argv[2], 'rb').read(), dtype='<i2').reshape(-1, 2).astype(float)
MR = int(sys.argv[3]) if len(sys.argv) > 3 else 22000


def env(x, rate, win=0.05):
    k = int(rate * win)
    n = len(x) // k
    return np.abs(x[:n * k]).reshape(n, k, 2).mean(axis=(1, 2))


a, b = env(d, 44100), env(m, MR)
best = (-2, 0)
for lag in range(-300, 300):
    x, y = (a[lag:], b) if lag >= 0 else (a, b[-lag:])
    n = min(len(x), len(y))
    if n < 100:
        continue
    c = np.corrcoef(x[:n], y[:n])[0, 1]
    if c > best[0]:
        best = (c, lag)
c, lag = best
print('envelope correlation %.3f at a lag of %.2f s' % (c, lag * 0.05))
x, y = (a[lag:], b) if lag >= 0 else (a, b[-lag:])
n = min(len(x), len(y))
print('level ratio %.2f' % (x[:n].mean() / y[:n].mean()))


def centroid(s, rate):
    s = s[:, 0] + s[:, 1]
    f = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    fr = np.fft.rfftfreq(len(s), 1 / rate)
    keep = fr < 10000                           # (the band both can hold)
    return (f[keep] * fr[keep]).sum() / max(f[keep].sum(), 1)


for t in range(2, int(n * 0.05) - 2, 4):
    i0 = int((t + max(lag, 0) * 0.05) * 44100)
    j0 = int((t + max(-lag, 0) * 0.05) * MR)
    print('t=%2d s  centroid %5.0f Hz (mac %5.0f Hz)' % (t, centroid(d[i0:i0 + 2 * 44100], 44100),
                                                         centroid(m[j0:j0 + 2 * MR], MR)))
