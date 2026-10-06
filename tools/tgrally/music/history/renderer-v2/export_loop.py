import os
"""Seamless-loop file for one piece from its two-pass mix: the song once plus the looping section,
with the file's last 120 ms crossfaded into the audio just before the loop point."""
import sys, json, numpy as np, soundfile as sf
from mixp import kpow
def desert_slam(x, ls):
    """A brief lull before the loop downbeat: the band draws back to a hush for a moment, then comes
    straight back in at full strength on the downbeat. No added sounds."""
    g = np.ones(len(x)); a, b = ls - int(1.1 * SR), ls - int(0.3 * SR)
    g[a:b] = np.linspace(1, 0.12, b - a) ** 1.5; g[b:ls] = 0.12; r = int(0.003 * SR); g[ls - r:ls] = np.linspace(0.12, 1, r)
    return x * g[:, None]
SLAM = {'desert': desert_slam}
SR = 48000; OUT = os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'ports/common/music')
for p in sys.argv[1:]:
    j = json.load(open('loop2/%s.json' % p)); ls, le = j['loop_start_frame'], j['frames']
    x, sr = sf.read('loop2/%s_mix.wav' % p); assert sr == SR; x = x[:le].copy()
    if p in SLAM: x = SLAM[p](x, ls)
    X = int(0.12 * SR); w = np.linspace(0, 1, X)[:, None]
    x[le - X:le] = x[le - X:le] * (1 - w) + x[ls - X:ls] * w
    x = np.clip(x, -1, 1)
    sf.write('%s/remastered_%s.flac' % (OUT, p), x, SR, subtype='PCM_24')
    lufs = -0.691 + 10 * np.log10(kpow(x) + 1e-20)
    m = json.load(open('%s/remastered.json' % OUT))
    for e in [m['title']] + m['race']:
        if e['file'] == 'remastered_%s.flac' % p: e.update(loop_start=ls, loop_end=le, gain_db=round(-15.6 - lufs, 2))
    json.dump(m, open('%s/remastered.json' % OUT, 'w'), indent=1)
    print(p, 'loop', ls, le, 'lufs %.2f' % lufs, 'seam jump', float(np.abs(x[le - 1] - x[ls]).max()))
