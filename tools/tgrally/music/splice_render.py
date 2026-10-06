"""Play a note list through Splice INSTRUMENT (Audio Unit) with a saved patch state, offline."""
import pedalboard, numpy as np, mido, time
AU = '/Library/Audio/Plug-Ins/Components/Splice INSTRUMENT.component'
_cache = {}
def plugin(state_path):
    if state_path not in _cache:
        p = pedalboard.load_plugin(AU); p.raw_state = open(state_path, 'rb').read()
        # samples stream in on the plugin's own threads: wait until a probe note sounds
        for _ in range(30):
            time.sleep(1.5)
            y = p([mido.Message('note_on', note=60, velocity=100, time=0.05), mido.Message('note_off', note=60, velocity=0, time=0.6)], duration=1.2, sample_rate=48000, num_channels=2, reset=False)
            if np.abs(y).max() > 1e-3: break
        p.reset(); _cache[state_path] = p
    return _cache[state_path]
def render(state_path, notes, total, sr=48000, cents=0.0):
    """notes: (start_frame, end_frame, midi, velocity 1-127). Returns (total, 2) float."""
    p = plugin(state_path); msgs = []
    p.parameters['global_tune'].raw_value = 0.5 + cents / 7000.0     # 0.001 of range = 7 cents
    for a, b, m, v in notes:
        msgs.append(mido.Message('note_on', note=int(m), velocity=int(np.clip(v, 1, 127)), time=a / sr))
        msgs.append(mido.Message('note_off', note=int(m), velocity=0, time=b / sr))
    msgs.sort(key=lambda m: (m.time, m.type == 'note_on'))
    y = p(msgs, duration=total / sr, sample_rate=sr, num_channels=2, reset=True)
    time.sleep(0.5)
    return y.T.astype(np.float64)

def ensemble(state_path, notes, total, sr=48000, passes=((-9.0, 0.0, -0.7), (0.0, 0.016, 0.0), (9.0, 0.029, 0.7))):
    """The same part sung three times, a few cents apart, a few ms apart and spread across the stereo field,
    so the patch reads as a large section rather than one group of voices."""
    out = np.zeros((total, 2))
    for c, d, pan in passes:
        y = render(state_path, notes, total, sr, c); k = int(d * sr)
        th = (pan + 1) * np.pi / 4; m = y.mean(1); side = (y[:, 0] - y[:, 1]) / 2
        l, r = (m + side) * np.cos(th) * np.sqrt(2), (m - side) * np.sin(th) * np.sqrt(2)
        out[k:, 0] += l[:total - k]; out[k:, 1] += r[:total - k]
    return out / np.sqrt(len(passes))
