"""Mix any piece: track levels from the original's K-weighted arrangement balance (original stems
rendered per part), a chain per track role, shared reverb, master tonal target, glue, limiter."""
import sys, os, json, collections, numpy as np, soundfile as sf
from concurrent.futures import ThreadPoolExecutor
from scipy.signal import fftconvolve
from scipy.ndimage import maximum_filter1d
from render_remaster import hp, lp, shelf_eq, compress, reverb_ir, SR
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
    'choir':   dict(hp=160, eq=([150, 400, 3000, 8000, 12000], [0, -3.0, 1.0, 2.5, 2.0]), comp=(-22, 1.6), send=0.4),
    'strings': dict(hp=60,  eq=([80, 300, 2000, 8000], [0, -1.5, 1.0, 1.5]), comp=(-22, 1.6), send=0.35),
    'brass':   dict(hp=80,  eq=([100, 400, 2500, 8000], [0, -1.5, 1.5, 1.0]), comp=(-18, 2.5), send=0.2),
    'arp':     dict(hp=150, eq=([150, 500, 3000, 10000], [0, -1.0, 1.0, 1.0]), comp=(-18, 2.0), send=0.2),
    'steel':   dict(hp=100, eq=([120, 400, 3000, 9000], [0, -1.5, 1.0, 0.5]), comp=(-18, 2.0), send=0.2),
    'pluck':   dict(hp=120, eq=([150, 500, 3000, 10000], [0, -1.0, 1.5, 1.5]), comp=(-18, 2.0), send=0.2),
    'kick':    dict(hp=30,  eq=([50, 100, 400, 3500, 8000], [2.0, 0.5, -3.0, 2.0, 1.0]), comp=(-14, 3.0), send=0.0),
    'hats':    dict(hp=350, eq=([400, 3000, 10000], [0, 0.5, 1.5]), comp=None, send=0.05),
    'cym':     dict(hp=300, eq=([400, 3000, 10000], [0, 0.5, 1.0]), comp=None, send=0.1),
    'perc':    dict(hp=60,  eq=([200, 800, 5000, 10000], [0.5, -1.0, 1.5, 1.0]), comp=(-16, 2.5), send=0.12),
    'kit':     dict(hp=35,  eq=([60, 400, 3500, 9000], [1.0, -2.0, 1.5, 1.5]), comp=(-16, 2.5), send=0.12),
    'break':   dict(hp=35,  eq=([60, 400, 3500, 9000], [1.0, -2.0, 1.5, 1.0]), comp=(-15, 3.0), send=0.08),
    'fx':      dict(hp=80,  eq=([100, 1000], [0, 0]), comp=None, send=0.3),
}
BIAS = {'kick': 1.5, 'bass': 1.0, 'hats': -1.5}
# stereo image per role: width of the decorrelated side above 300 Hz, and a pan (-1 left .. 1 right)
STEREO = {'arp': (0.5, 0), 'steel': (0.4, 0), 'choir': (1.0, 0), 'pad': (0.8, 0), 'stab': (0.7, 0), 'strings': (0.6, 0), 'lead': (0.35, 0), 'pluck': (0.5, 0), 'brass': (0.4, 0),
          'hats': (0.2, 0.3), 'cym': (0.3, -0.35), 'perc': (0.15, -0.2), 'fx': (0.6, 0)}
def allpass_chain(x, coefs):
    for a in coefs: x = lfilter([a, 1.0], [1.0, a], x)
    return x
def stereo(y, width, pan):
    from scipy.signal import butter, sosfiltfilt
    m = y.mean(1); sd = (y[:, 0] - y[:, 1]) / 2
    if width > 0:
        d = allpass_chain(m, [0.62, -0.37, 0.71, -0.55]) - allpass_chain(m, [-0.48, 0.66, -0.29, 0.53])
        d = sosfiltfilt(butter(2, 300 / (SR / 2), 'high', output='sos'), d)
        need = max(0.0, width * np.sqrt(np.mean(m ** 2)) - np.sqrt(np.mean(sd ** 2)))
        sd = sd + d * need / max(np.sqrt(np.mean(d ** 2)), 1e-12)
    l, r = m + sd, m - sd
    th = (pan + 1) * np.pi / 4; return np.stack([l * np.cos(th) * np.sqrt(2), r * np.sin(th) * np.sqrt(2)], 1)
def role(t):
    for r in sorted(CHAIN, key=len, reverse=True):
        if t.startswith(r): return r
    return 'perc'

PIECE_BIAS = {'desert': {'break': 3.5, 'lead': 2.0}, 'mountain': {'break': 3.0, 'rhythm1': 1.5, 'bass': 1.0}, 'coastline': {'bass': 3.0, 'kick': 1.0},
              'stripmine': {}, 'title': {'choir': -2.0, 'pad2': -3.0, 'piano': -3.5, 'perc': 1.0, 'kick': 0.5}}
PIECE_EXTRA = {'stripmine': {'kit': 0.13}, 'mountain': {'kit': 0.27}}
# pulse: these parts duck under each kick (depth, release s); ethereal: longer space, more send, echoes
DUCK = {'mountain': (('bass', 'bass2', 'stab', 'rhythm1', 'rhythm2'), 0.45, 0.16)}
SPACE = {'title': dict(ir=(4.5, 0.04, 6000), send_mul={'perc': 2.6, 'hats': 3.0, 'piano': 1.4, 'fx': 1.5, 'choir': 2.2}, send_add={'kick': 0.12},
                       tilt={'piano': ([2000, 4000, 8000], [-1.5, -3.0, -3.5]), 'choir': ([1500, 3000, 5000, 9000], [-1.0, -3.5, -3.0, -1.0])}),
         'mountain': dict(ir=(3.8, 0.03, 7000), send_mul={'pad': 1.8, 'strings': 1.8, 'pluck': 1.9, 'stab': 1.7, 'rhythm': 1.5},
                          echo=(('stab', 'strings_pad'), 0.33, 0.38, 0.22))}
PIECE_TILT = {'coastline': {31: 2.0, 63: 3.5, 125: 2.5, 250: 1.0, 2000: -1.0, 4000: -2.5, 8000: -3.0, 16000: -3.0},
              'desert': {63: 1.5, 125: 1.0}, 'mountain': {63: 1.5, 125: 1.0}}
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
        if role(g) in ('break', 'kick', 'perc', 'kit'):
            # parallel compression: a crushed copy under the dry drums for weight and punch
            z = compress(y, -32, 8.0, att=0.001, rel=0.08); z *= np.sqrt(kpow(y) / max(kpow(z), 1e-20)); y = y + 0.5 * z
        tl = SPACE.get(piece, {}).get('tilt', {}).get(role(g))
        if tl: y = shelf_eq(y, np.array([tl[0][0] / 2] + tl[0], float), np.array([0.0] + tl[1]))
        if role(g) in STEREO: y = stereo(y, *STEREO[role(g)])
        proc[g] = y
    if piece in DUCK and 'kit' in proc:
        names, depth, rel = DUCK[piece]
        from scipy.signal import butter, sosfiltfilt
        lo_ = sosfiltfilt(butter(2, 120 / (SR / 2), 'low', output='sos'), proc['kit'].mean(1))
        e_ = np.sqrt(np.convolve(lo_ ** 2, np.ones(240) / 240, 'same')); e_ /= max(np.percentile(e_, 99.5), 1e-9)
        # kick envelope: instant rise, exponential release, normalised to 1 at a full kick
        B_ = 48; pk = np.minimum(e_[:len(e_) // B_ * B_].reshape(-1, B_).max(1), 1.0); r_ = np.exp(-B_ / (rel * SR)); eb = np.empty_like(pk); v_ = 0.0
        for k in range(len(pk)): v_ = pk[k] if pk[k] > v_ else v_ * r_; eb[k] = v_
        env = np.interp(np.arange(len(e_)), np.arange(len(eb)) * B_ + B_ / 2, eb)
        dg = 1 - depth * env
        for g in names:
            if g in proc: proc[g] = proc[g] * dg[:len(proc[g]), None]
    sp = SPACE.get(piece, {})
    if sp.get('echo'):
        names, dt_, fb, wet = sp['echo']; d_ = int(dt_ * SR)
        for g in names:
            if g not in proc: continue
            y = proc[g]; m = y.mean(1); out = np.zeros_like(y); tap = m.copy(); side = 0
            for k in range(6):
                tap = np.concatenate([np.zeros(d_), tap[:-d_]]) * (fb if k else 1.0)
                tap = hp(lp(tap[:, None], 6000, 1), 300, 1)[:, 0]
                out[:, side] += tap; side ^= 1
            proc[g] = y + out * wet
    # gated parts (the original chops them rhythmically) keep their gaps: less reverb in proportion
    SEND = {}
    for g in proc:
        c = CHAIN[role(g)]; SEND[g] = c['send']
        if g in op and c['send'] > 0.1:
            o = sum(OS[str(i)].astype(float) for i in groups[g] if str(i) in OS)[:L]; H = 960
            e = 20 * np.log10(np.sqrt(np.mean(o[:len(o) // H * H].reshape(-1, H) ** 2, 1)) + 1e-7)
            on = e > e.max() - 45; p90 = np.percentile(e[on], 90) if on.any() else 0
            act = np.convolve(on, np.ones(25), 'same') > 12; gate = float(np.mean(e[act] < p90 - 18)) if act.any() else 0
            SEND[g] = c['send'] * max(0.25, 1 - 2 * gate)
        SEND[g] = SEND[g] * sp.get('send_mul', {}).get(role(g), 1.0) + sp.get('send_add', {}).get(role(g), 0.0)
    print('  reverb sends:', {g: round(v, 2) for g, v in SEND.items()}, flush=True)
    # Balance is judged in the finished mix: the master tonal correction is linear, so each stem gets
    # the same curve before its share is compared with the original's, and the gains are re-solved.
    pb = PIECE_BIAS.get(piece, {})
    wb = {g: op[g] * 10 ** (pb.get(g, pb.get(role(g), 0)) / 10) for g in proc if g in op}
    # parts the remaster adds (no original stem) take a fixed share of the mix
    for g, sh_ in PIECE_EXTRA.get(piece, {}).items():
        if g in proc: wb[g] = sh_ / (1 - sh_) * sum(wb.values())
    tot_o = sum(wb.values()); want = {g: wb[g] / tot_o for g in wb}
    TGT = {c: TARGET[c] + PIECE_TILT.get(piece, {}).get(c, 0) for c in TARGET}
    gain = {g: np.sqrt(want[g] / max(kpow(proc[g]), 1e-20)) * 10 ** (BIAS.get(role(g), 0) / 20) for g in want}
    def master_curve(stems):
        m = sum(stems[g] * gain[g] for g in gain); m = m + fftconvolve(sum(stems[g] * gain[g] * SEND[g] for g in gain), IR, axes=0)[:L] * 0.9
        sh = oct_share(m); return np.array([np.clip(TGT[c] - (sh[c] - sh[1000]), -6, 6) for c in TARGET])
    IR = reverb_ir(*sp['ir']) if sp.get('ir') else reverb_ir(); F = np.array(list(TARGET), float); curve = np.zeros(len(F))
    for it in range(4):
        eqd = {g: shelf_eq(proc[g], F, curve) for g in gain}
        curve = np.clip(curve + master_curve(eqd) * 0.8, [-15] + [-6] * (len(F) - 1), [5] + [6] * (len(F) - 1))
        eqd = {g: shelf_eq(proc[g], F, curve) for g in gain}
        pw = {g: kpow(eqd[g]) * gain[g] ** 2 for g in gain}; tp = sum(pw.values())
        for g in gain: gain[g] *= np.sqrt(want[g] / max(pw[g] / tp, 1e-20)) ** 0.8 * 10 ** (BIAS.get(role(g), 0) / 20 * 0.2)
    eqd = {g: shelf_eq(proc[g], F, curve) for g in gain}
    pw = {g: kpow(eqd[g]) * gain[g] ** 2 for g in gain}; tp = sum(pw.values())
    print('  share orig/mix %:', {g: '%.1f/%.1f' % (100 * want[g], 100 * pw[g] / tp) for g in gain}, 'master curve', dict(zip(TARGET, np.round(curve, 1))), flush=True)
    mix = np.zeros((L, 2)); send = np.zeros((L, 2))
    for g in gain:
        mix += eqd[g] * gain[g]; send += eqd[g] * gain[g] * SEND[g]
    mix += fftconvolve(send, IR, axes=0)[:L] * 0.9
    mix = compress(mix, thr_db=-12, ratio=1.6, att=0.02, rel=0.25)
    sh = oct_share(mix); corr = {c: np.clip(TGT[c] - (sh[c] - sh[1000]), -2, 2) for c in TARGET}
    mix = shelf_eq(mix, np.array(list(corr), float), np.array([corr[c] for c in corr]))
    # the arrangement's dynamics: each passage sits as far above or below the song's typical level as it
    # does in the original (intros and breakdowns stay down, so the full sections land)
    ow = os.environ.get('ORIGWAV', 'pieces/%s.xmlog.wav') % piece
    if os.path.exists(ow):
        o = np.fromfile(ow, '<i2', offset=44).reshape(-1, 2) / 32768.0; n = min(len(o), len(mix)); W, Hh = int(3 * SR), int(0.5 * SR)
        def st(x):
            y = kweight(x); p = np.sum(y ** 2, 1); c = np.concatenate([[0], np.cumsum(p)])
            idx = np.arange(0, n, Hh); a = np.clip(idx - W // 2, 0, n); b = np.clip(idx + W // 2, 0, n)
            return idx, 10 * np.log10((c[b] - c[a]) / np.maximum(b - a, 1) + 1e-12)
        idx, lo = st(o[:n]); _, lr = st(mix[:n]); act = lo > np.median(lo) - 25
        d = (lo - np.median(lo[act])) - (lr - np.median(lr[act])); d = np.where(act, np.clip(d, -12, 3), 0)
        d = np.convolve(d, np.ones(5) / 5, 'same')
        gdb = np.interp(np.arange(len(mix)), idx, d); mix *= 10 ** (gdb / 20)[:, None]
        print('  contour correction dB: min %.1f max %.1f' % (d.min(), d.max()), flush=True)
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
