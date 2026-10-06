"""Mix any piece: track levels from the original's K-weighted arrangement balance (original stems
rendered per part), a chain per track role, shared reverb, master tonal target, glue, limiter."""
import sys, os, json, collections, numpy as np, soundfile as sf
from concurrent.futures import ThreadPoolExecutor
from scipy.signal import fftconvolve
from scipy.ndimage import maximum_filter1d
from render_remaster import hp, shelf_eq, compress, reverb_ir, SR
from scipy.signal import lfilter
def kweight(x):
    b1, a1 = [1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585]
    b2, a2 = [1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621]
    return lfilter(b2, a2, lfilter(b1, a1, x, axis=0), axis=0)
def kpow(x):
    y = kweight(x); return float(np.mean(np.sum(y ** 2, axis=1) if y.ndim == 2 else y ** 2))
import stems as ST
from piece_render import RECIPES

CHAIN = {  # role: high-pass, eq (Hz, dB), compression (threshold, ratio), reverb send
    'piano':   dict(hp=70,  eq=([60, 250, 400, 3000, 10000], [0, -2.5, -1.5, 1.5, 2.0]), comp=(-20, 2.0), send=0.22),
    'rhythm':  dict(hp=90,  eq=([80, 300, 500, 3500, 9000, 14000], [0, -2.5, -1.0, 1.0, -2.0, -6.0]), comp=None, send=0.06),
    'lead':    dict(hp=140, eq=([150, 400, 2500, 9000], [0, -1.5, 1.5, -1.0]), comp=(-18, 2.5), send=0.25),
    'bass':    dict(hp=30,  eq=([40, 80, 250, 1000, 3000], [1.5, 1.0, -2.0, 0.5, 1.0]), comp=(-16, 4.0), send=0.0),
    'sub':     dict(hp=25,  eq=([30, 60, 150, 500], [1.0, 0.5, -2.0, -3.0]), comp=(-16, 3.0), send=0.0),
    'stab':    dict(hp=150, eq=([100, 300, 1000, 5000, 12000], [0, -2.0, 0.5, 1.5, 2.0]), comp=(-18, 2.0), send=0.25),
    'pad':     dict(hp=120, eq=([100, 300, 1000, 5000, 12000], [0, -2.5, 0.0, 1.0, 1.5]), comp=(-20, 1.8), send=0.35),
    'strings': dict(hp=60,  eq=([80, 300, 2000, 8000], [0, -1.5, 1.0, 1.5]), comp=(-22, 1.6), send=0.35),
    'brass':   dict(hp=80,  eq=([100, 400, 2500, 8000], [0, -1.5, 1.5, 1.0]), comp=(-18, 2.5), send=0.2),
    'pluck':   dict(hp=120, eq=([150, 500, 3000, 10000], [0, -1.0, 1.5, 1.5]), comp=(-18, 2.0), send=0.2),
    'kick':    dict(hp=30,  eq=([50, 100, 400, 3500, 8000], [2.0, 0.5, -3.0, 2.0, 1.0]), comp=(-14, 3.0), send=0.0),
    'hats':    dict(hp=350, eq=([400, 3000, 10000], [0, 0.5, 1.5]), comp=None, send=0.05),
    'cym':     dict(hp=300, eq=([400, 3000, 10000], [0, 0.5, 1.0]), comp=None, send=0.1),
    'perc':    dict(hp=60,  eq=([200, 800, 5000, 10000], [0.5, -1.0, 1.5, 1.0]), comp=(-16, 2.5), send=0.12),
    'break':   dict(hp=35,  eq=([60, 400, 3500, 9000], [1.0, -2.0, 1.5, 1.0]), comp=(-15, 3.0), send=0.08),
    'fx':      dict(hp=80,  eq=([100, 1000], [0, 0]), comp=None, send=0.3),
}
BIAS = {'kick': 1.5, 'bass': 1.0, 'hats': -1.5}
def role(t):
    for r in sorted(CHAIN, key=len, reverse=True):
        if t.startswith(r): return r
    return 'perc'

TARGET = {31: 2.0, 63: 6.0, 125: 5.0, 250: 2.0, 500: 0.5, 1000: 0.0, 2000: -2.0, 4000: -4.5, 8000: -7.5, 16000: -13.0}
def oct_share(x):
    X = np.abs(np.fft.rfft(x.mean(1))) ** 2; f = np.fft.rfftfreq(len(x), 1 / SR)
    return {c: 10 * np.log10(X[(f >= c / np.sqrt(2)) & (f < c * np.sqrt(2))].sum() + 1e-20) for c in TARGET}

def orig_stems(piece):
    path = 'pieces/%s_origstems.npz' % piece
    if os.path.exists(path): return dict(np.load(path))
    m = open('pieces/%s.xm' % piece, 'rb').read()
    def one(i): return str(i), ST.render(ST.solo(m, i), 'pieces/_solo_%s_%d.xm' % (piece, i)).astype(np.float32)
    with ThreadPoolExecutor(8) as ex: d = dict(ex.map(one, sorted(RECIPES[piece])))
    np.savez_compressed(path, **d); return d

def mix_piece(piece, out_wav):
    T = dict(np.load(os.environ.get('TRACKS', 'pieces/%s_tracks.npz') % piece)); OS = orig_stems(piece)
    groups = collections.defaultdict(list)
    for i, r in RECIPES[piece].items(): groups[r.get('track', r['t'])].append(i)
    L = min(len(v) for v in T.values())
    op = {}
    for g, ins in groups.items():
        if g not in T: continue
        o = sum(OS[str(i)].astype(float) for i in ins if str(i) in OS); op[g] = kpow(np.stack([o, o], 1))
    proc = {}
    match = {r.get('track', r['t']) for r in RECIPES[piece].values() if r.get('match_tone')}
    for g, x in T.items():
        c = CHAIN[role(g)]; y = hp(x[:L].astype(float), c['hp'], 2)
        y = shelf_eq(y, np.array(c['eq'][0], float), np.array(c['eq'][1], float))
        if g in match:
            o = sum(OS[str(i)].astype(float) for i in groups[g] if str(i) in OS)[:L]
            B = [100 * 2 ** (k / 3) for k in range(19)]
            def bd(v):
                V = np.abs(np.fft.rfft(v)) ** 2; f = np.fft.rfftfreq(len(v), 1 / SR)
                return np.array([10 * np.log10(V[(f >= b * 2 ** -(1 / 6)) & (f < b * 2 ** (1 / 6))].sum() + 1e-20) for b in B])
            do = bd(o[:SR * 60]); dy = bd(y.mean(1)[:SR * 60]); ok = do > do.max() - 30
            diff = np.where(ok, (do - do[ok].mean()) - (dy - dy[ok].mean()), 0); last = np.nonzero(ok)[0][-1]; diff[last + 1:] = diff[last]
            y = shelf_eq(y, np.array(B, float), np.clip(np.convolve(diff, [0.25, 0.5, 0.25], 'same'), -9, 9))
        if c['comp']: y = compress(y, c['comp'][0], c['comp'][1])
        proc[g] = y
    # Balance is judged in the finished mix: the master tonal correction is linear, so each stem gets
    # the same curve before its share is compared with the original's, and the gains are re-solved.
    tot_o = sum(op[g] for g in proc if g in op); want = {g: op[g] / tot_o for g in proc if g in op}
    gain = {g: np.sqrt(want[g] / max(kpow(proc[g]), 1e-20)) * 10 ** (BIAS.get(role(g), 0) / 20) for g in want}
    def master_curve(stems):
        m = sum(stems[g] * gain[g] for g in gain); m = m + fftconvolve(sum(stems[g] * gain[g] * CHAIN[role(g)]['send'] for g in gain), IR, axes=0)[:L] * 0.9
        sh = oct_share(m); return np.array([np.clip(TARGET[c] - (sh[c] - sh[1000]), -6, 6) for c in TARGET])
    IR = reverb_ir(); F = np.array(list(TARGET), float); curve = np.zeros(len(F))
    for it in range(4):
        eqd = {g: shelf_eq(proc[g], F, curve) for g in gain}
        curve = curve + master_curve(eqd) * 0.8
        eqd = {g: shelf_eq(proc[g], F, curve) for g in gain}
        pw = {g: kpow(eqd[g]) * gain[g] ** 2 for g in gain}; tp = sum(pw.values())
        for g in gain: gain[g] *= np.sqrt(want[g] / max(pw[g] / tp, 1e-20)) ** 0.8 * 10 ** (BIAS.get(role(g), 0) / 20 * 0.2)
    eqd = {g: shelf_eq(proc[g], F, curve) for g in gain}
    pw = {g: kpow(eqd[g]) * gain[g] ** 2 for g in gain}; tp = sum(pw.values())
    print('  share orig/mix %:', {g: '%.1f/%.1f' % (100 * want[g], 100 * pw[g] / tp) for g in gain}, 'master curve', dict(zip(TARGET, np.round(curve, 1))), flush=True)
    mix = np.zeros((L, 2)); send = np.zeros((L, 2))
    for g in gain:
        mix += eqd[g] * gain[g]; send += eqd[g] * gain[g] * CHAIN[role(g)]['send']
    mix += fftconvolve(send, IR, axes=0)[:L] * 0.9
    mix = compress(mix, thr_db=-14, ratio=2.0, att=0.02, rel=0.25)
    sh = oct_share(mix); corr = {c: np.clip(TARGET[c] - (sh[c] - sh[1000]), -2, 2) for c in TARGET}
    mix = shelf_eq(mix, np.array(list(corr), float), np.array([corr[c] for c in corr]))
    lufs = -0.691 + 10 * np.log10(kpow(mix) + 1e-20); mix *= 10 ** ((-14 - lufs) / 20)
    look = int(0.005 * SR); env = maximum_filter1d(np.max(np.abs(mix), axis=1), size=look * 2 + 1)
    g = np.minimum(1.0, 0.891 / np.maximum(env, 1e-9)); g = np.minimum(np.convolve(g, np.ones(look) / look, 'same'), 1.0)
    mix = mix * g[:, None]
    # trim the tail of silence the renderer pads with
    if os.environ.get('NOTRIM'): nz = []
    else: nz = np.nonzero(np.max(np.abs(mix), axis=1) > 1e-4)[0]; mix = mix[:nz[-1] + int(0.5 * SR)] if len(nz) else mix
    sf.write(out_wav, mix.astype(np.float32), SR, subtype='FLOAT')
    return {g: round(20 * np.log10(v), 1) for g, v in gain.items()}

if __name__ == '__main__':
    for p in sys.argv[1:]:
        print(p, mix_piece(p, 'pieces/%s_mix.wav' % p), flush=True)
