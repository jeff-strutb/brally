#!/usr/bin/env python3
"""streamcmp.py -- run the original (n64/tools/n64box.py) and the port over
the same script and compare what each produced, in order: every graphics
task's display-list digest, every frame-buffer swap, every audio buffer.
The first difference is reported with its frame and its index in that
stream (for lockstep.py --task, the gfx index is the task).

    streamcmp.py [--script FILE] [--frames N]
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
PY = os.path.join(ROOT, '.venv/bin/python')
KINDS = ('gfx', 'swap', 'ai')


def streams(path):
    out = {k: [] for k in KINDS}
    for line in open(path):
        f = line.split()
        if len(f) >= 3 and f[1] in out:
            out[f[1]].append((int(f[0]), f[2]))
    return out


def run_box(a):
    """the original under the port's view of n64box (tools/tgrbox.py)"""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import tgrbox
    box = tgrbox.Box(script=a.script)
    r = box.run(a.frames)
    print('stopped: %s after %d frames, %d gfx tasks' % (r, box.frame, sum(1 for x in box.log if x[1] == 'gfx')))
    with open(a.log, 'w') as f:
        for row in box.log:
            f.write('%d %s %s\n' % row)


def script_frames(path):
    """a script's full length: its `frames N` line, else its last input + 300"""
    last, fixed = 0, None
    for line in open(path):
        w = line.split('#')[0].split()
        if not w:
            continue
        if w[0] == 'frames' and len(w) > 1:
            fixed = int(w[1])
        elif w[0] == 'p2' and len(w) > 1 and w[1].isdigit():
            last = max(last, int(w[1]))
        elif w[0].isdigit():
            last = max(last, int(w[0]))
    return fixed or last + 300


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--script')
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--bin', default=os.path.join(ROOT, 'build/tgrally/tgrally'), help='the port binary')
    ap.add_argument('--timeout', type=int, default=180, help='seconds before the port counts as hung')
    ap.add_argument('--box', action='store_true', help=argparse.SUPPRESS)
    ap.add_argument('--log', help=argparse.SUPPRESS)
    a = ap.parse_args()
    if a.box:
        run_box(a)
        return
    tag = os.path.splitext(os.path.basename(a.script))[0] if a.script else 'boot'
    d = os.path.join(ROOT, 'build/tgrally/stream')
    os.makedirs(d, exist_ok=True)
    plog = os.path.join(d, tag + '.port')
    sc = ['--script', a.script] if a.script else []
    # the original's streams depend only on the script, the length, the ROM
    # and the box: they are cached, so a rerun only runs the port
    import hashlib
    import inspect
    key = hashlib.sha1()
    full = script_frames(a.script) if a.script else a.frames
    for part in (open(a.script, 'rb').read() if a.script else b'', str(full).encode(),
                 open(os.path.join(ROOT, 'n64/tools/n64box.py'), 'rb').read(),
                 inspect.getsource(run_box).encode(),
                 open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tgrbox.py'), 'rb').read(), str(os.path.getsize(os.path.join(
                     ROOT, 'reference/tgrally/Top Gear Rally (USA).z64'))).encode()):
        key.update(part)
    cache = os.path.join(ROOT, 'build/tgrally/boxcache')
    os.makedirs(cache, exist_ok=True)
    blog = os.path.join(cache, '%s-%s.log' % (tag, key.hexdigest()[:16]))
    box = None
    if not os.path.exists(blog):
        box = subprocess.Popen([PY, __file__, '--box', '--frames', str(full), '--log', blog + '.%d.tmp' % os.getpid()] + sc,
                               cwd=ROOT, stdout=subprocess.PIPE, text=True)
    try:                        # a hung port must not hang the comparison
        port = subprocess.run([a.bin, '--headless', '--frames', str(a.frames), '--trace', plog] + sc,
                              cwd=ROOT, capture_output=True, text=True, timeout=a.timeout)
    except subprocess.TimeoutExpired:
        port = subprocess.CompletedProcess([], -999, '', 'timed out after %d s (hung)' % a.timeout)
    if box:
        print('original:', box.communicate()[0].strip())
        os.replace(blog + '.%d.tmp' % os.getpid(), blog)
    else:
        print('original: cached')
    if port.returncode:
        print('port exited %d: %s' % (port.returncode, port.stderr.strip()[-400:]))
    keep = a.frames
    b, p = streams(blog), streams(plog)
    b = {k: [x for x in v if x[0] < keep] for k, v in b.items()}     # the run's length of the original
    same = True
    for k in KINDS:
        n = min(len(b[k]), len(p[k]))
        for i in range(n):
            if b[k][i] != p[k][i]:
                print('%-4s %d differs: original frame %d %s, port frame %d %s' % (k, i, *b[k][i], *p[k][i]))
                same = False
                break
        else:
            if len(b[k]) != len(p[k]):
                print('%-4s the same for %d, then original has %d, port %d' % (k, n, len(b[k]), len(p[k])))
                same = False
            else:
                print('%-4s %d, the same' % (k, n))
    sys.exit(0 if same else 1)


if __name__ == '__main__':
    main()
