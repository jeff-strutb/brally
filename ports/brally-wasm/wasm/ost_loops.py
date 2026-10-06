#!/usr/bin/env python3
"""Barry Leitch's own recordings of the Top Gear Rally music, looped the way
the N64 loops the modules: where each one repeats, and how loud the set is.

    ost_loops.py <recordings dir> <n64 music dir> <xmloop> <cd music dir>

The recordings (the 96 kHz / 24-bit FLACs the composer sold) are the same
six pieces as the ROM's modules, played through once and a bit, then faded
out. The N64 plays a module forever: from the start into its loop, then
round the loop. This finds that loop in each recording so the port can do
the same with a hard, sample-exact jump and never reach the fade.

The passes of a recording are not identical sample for sample (it was
recorded in real time: the second pass drifts by tens to hundreds of
samples against the first, and its mix differs slightly), so the loop is
the pair of points where the two passes agree best:

  1. the loop length the module has (xmloop: restart order to end of
     pass), which the recordings run up to 0.4% slower than, fixes where
     to look: the best lags of a dozen 2 s windows within 1.2% of it are
     the candidates, and the one that also holds best over 30 s windows
     is the recording's loop length (a bar that repeats inside the loop
     matches over 2 s, not over 30);
  2. every 50 ms from the module's restart point to 25 s before the end
     (clear of the fade), the 250 ms either side of a point is compared
     with the same span one loop later, at 4 kHz;
  3. the 100 best are compared again at the full rate, over 100 ms, and
     the jump itself is measured sample by sample (the step it would put
     in the waveform, over 2 ms either side, against the local level);
     the smallest step among points whose passes agree to 0.995 or better
     wins.

The recordings are matched to the modules by name: "Title" for the title
module, and the N64 game's own track names (modules.json race_names, read
from the ROM) for the race modules. All six must be present.

Writes the recordings into the n64 music dir as rec_<name>.flac (clones
where the filesystem allows) and adds to its modules.json:

    "recordings": {"title": {file, rate, loop_start, loop_end},
                   "race": [...5, in race order], "gain_db": g}

loop_start/loop_end are sample frames: play [0, loop_end), then
[loop_start, loop_end) forever. gain_db brings the set's mean integrated
loudness (over each recording up to its loop end) to the CD tracks' mean.
Needs numpy and ffmpeg.
"""
import json
import os
import re
import shutil
import subprocess
import sys

import numpy as np

R4 = 4000
FADE_GUARD = 25.0


def decode(path, rate=None, mono=False):
    a = ['ffmpeg', '-v', 'error', '-i', path]
    if mono:
        a += ['-ac', '1']
    if rate:
        a += ['-ar', str(rate)]
    x = np.frombuffer(subprocess.run(a + ['-f', 'f32le', '-'], capture_output=True, check=True).stdout,
                      np.float32)
    return x if mono else x.reshape(-1, 2)


def rate_of(path):
    return int(subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'stream=sample_rate',
                               '-of', 'csv=p=0', path], capture_output=True, text=True, check=True).stdout)


def lufs(path, seconds=None):
    a = ['ffmpeg', '-nostats', '-i', path] + (['-t', '%.3f' % seconds] if seconds else []) + \
        ['-af', 'ebur128', '-f', 'null', '-']
    out = subprocess.run(a, capture_output=True, text=True).stderr
    return float(re.findall(r'I:\s+(-?[\d.]+) LUFS', out)[-1])


def ncorr(big, seg):
    """normalised correlation of seg at every offset in big"""
    w = len(seg)
    c = np.correlate(big, seg, 'valid')
    e = np.sqrt(np.convolve(big * big, np.ones(w), 'valid')) * np.sqrt(np.dot(seg, seg)) + 1e-12
    return c / e


def find_loop(path, restart, loop):
    m = decode(path, R4, mono=True)
    n = len(m)
    guess, span = int(loop * 1.004 * R4), int(0.012 * loop * R4)
    lags = []
    for t in np.linspace(restart + 20, n / R4 - loop * 1.02 - FADE_GUARD, 12):
        a = int(t * R4)
        cc = ncorr(m[a + guess - span:a + guess + span + 2 * R4], m[a:a + 2 * R4])
        k = int(cc.argmax())
        lags.append((cc[k], guess - span + k))
    # A piece whose bars repeat inside the loop matches at several lengths a
    # bar apart; only the true loop also holds over long spans, so each
    # length found is scored over 30 s windows and the best kept.
    cands = sorted({l for c, l in lags if c > 0.3} or {l for c, l in lags})
    groups = []
    for l in cands:
        if groups and l - groups[-1][-1] <= R4 // 20:
            groups[-1].append(l)
        else:
            groups.append([l])
    W30, sl = 30 * R4, R4 // 10
    scored = []
    for g in groups:
        L = int(np.median(g))
        sc = []
        for t in np.linspace(restart + 10, n / R4 - loop * 1.02 - FADE_GUARD - 30, 6):
            a = int(t * R4)
            if a < 0 or a + L + sl + W30 > n:
                continue
            sc.append(float(ncorr(m[a + L - sl:a + L + sl + W30], m[a:a + W30]).max()))
        scored.append((np.mean(sc) if sc else 0, L))
    L4 = max(scored)[1]

    W = R4 // 4
    coarse = []
    for a in range(int((restart + 5) * R4), int(n - FADE_GUARD * R4 - L4 - W - 200), R4 // 20):
        cc = ncorr(m[a - W // 2 + L4 - 120:a - W // 2 + L4 + 120 + W], m[a - W // 2:a + W // 2])
        k = int(cc.argmax())
        coarse.append((float(cc[k]), a, L4 - 120 + k))
    coarse.sort(reverse=True)

    fr = rate_of(path)
    x = decode(path)
    xm = x.mean(1)
    w, j, sr = int(0.05 * fr), int(0.002 * fr), int(0.03 * fr)
    found = []
    for c4, a4, l4 in coarse[:300]:
        S = int(round(a4 / R4 * fr))
        E0 = int(round((a4 + l4) / R4 * fr))
        cc = ncorr(xm[E0 - sr - w:E0 + sr + w], xm[S - w:S + w])
        k = int(cc.argmax())
        E = E0 - sr + k
        # The jump plays frame E-1 then S where the recording would have
        # played E. What it adds is the difference between S and E in value
        # and in slope at that one frame; measured against the local
        # frame-to-frame change, over the samples either side (a fraction
        # of a millisecond, so the musical alignment is unchanged).
        d = np.sqrt((np.diff(x[S - j:S + j], axis=0) ** 2).mean()) + 1e-9
        junction, E = min((float(np.sqrt((((x[S] - x[e]) ** 2) +
                                           ((x[S + 1] - x[S]) - (x[e + 1] - x[e])) ** 2).mean())) / d, e)
                          for e in range(E - 16, E + 17))
        found.append((float(cc[k]), float(junction), S, int(E)))
    # the passes must agree around the jump (0.995 over 100 ms, or within
    # 0.01 of the best this recording gets); among those, the cleanest
    # junction
    need = min(0.995, max(f[0] for f in found) - 0.01)
    c100, junction, S, E = min((f for f in found if f[0] >= need), key=lambda f: f[1])
    if E > len(x) - FADE_GUARD * fr:
        sys.exit('ost_loops: %s: loop end runs into the fade' % path)
    return {'rate': fr, 'loop_start': S, 'loop_end': E, 'match': round(c100, 5), 'junction': round(junction, 4)}


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    rec_dir, n64, xmloop, cd = sys.argv[1:]
    mj = os.path.join(n64, 'modules.json')
    cues = json.load(open(mj))
    loops = {}
    for line in subprocess.run([xmloop] + [os.path.join(n64, f) for f in [cues['title']] + cues['race']],
                               capture_output=True, text=True, check=True).stdout.splitlines():
        f, rs, end = line.rsplit(' ', 2)
        loops[os.path.basename(f)] = (float(rs), float(end) - float(rs))
    flacs = [f for f in os.listdir(rec_dir) if f.lower().endswith('.flac')]

    def pick(name):
        key = name.lower().replace(' ', '')
        hits = [f for f in flacs if key in f.lower().replace(' ', '')]
        if len(hits) != 1:
            sys.exit('ost_loops: %d recordings in %s match "%s"' % (len(hits), rec_dir, name))
        return hits[0]

    want = [('title', 'Title', cues['title'])] + \
           [(n.lower().replace(' ', ''), n, m) for n, m in zip(cues['race_names'], cues['race'])]
    out = {}
    for key, name, mod in want:
        src = os.path.join(rec_dir, pick(name))
        rs, loop = loops[mod]
        info = find_loop(src, rs, loop)
        dst = 'rec_%s.flac' % key
        d = os.path.join(n64, dst)
        if os.path.exists(d):
            os.remove(d)
        if subprocess.run(['cp', '-c', src, d], capture_output=True).returncode:
            shutil.copyfile(src, d)
        info['file'] = dst
        info['lufs'] = lufs(src, info['loop_end'] / info['rate'])
        out[key] = info
        print('recordings: %-10s loop %9.3f s .. %9.3f s (%.3f s; module %.3f s), passes agree %.4f, junction %.3f, %.1f LUFS'
              % (name, info['loop_start'] / info['rate'], info['loop_end'] / info['rate'],
                 (info['loop_end'] - info['loop_start']) / info['rate'], loop, info['match'],
                 info['junction'], info['lufs']))
    cd_l = [lufs(os.path.join(cd, f)) for f in sorted(os.listdir(cd)) if f.endswith('.flac')]
    gain = float(np.mean(cd_l)) - float(np.mean([v['lufs'] for v in out.values()]))
    cues['recordings'] = {'title': out['title'],
                          'race': [out[n.lower().replace(' ', '')] for n in cues['race_names']],
                          'gain_db': round(gain, 2)}
    with open(mj, 'w') as f:
        json.dump(cues, f, indent=1)
        f.write('\n')
    print('recordings: CD mean %.1f LUFS, recordings mean %.1f LUFS, gain %+.2f dB'
          % (np.mean(cd_l), np.mean([v['lufs'] for v in out.values()]), gain))


if __name__ == '__main__':
    main()
