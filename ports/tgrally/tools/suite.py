#!/usr/bin/env python3
"""suite.py -- every n64box script through streamcmp.py, in parallel.

The port binary is copied first, so the tree can be rebuilt while a suite
runs; the longest scripts start first (the run takes as long as the
longest one); the original's streams come from streamcmp's cache after the
first run.  Results go to build/tgrally/null-null/suite/<time>/: one .res per script
and summary.txt, which is also printed.

    suite.py [--jobs N] [NAME...]      (names: script basenames, default all)
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
PY = os.path.join(ROOT, '.venv/bin/python')
SCRIPTS = os.path.join(ROOT, 'tools/tgrally/n64box_scripts')


def frames_of(path):
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


def summary(res):
    text = open(res).read()
    lines = [l for l in text.splitlines() if re.search(r'differs|then original|exited', l)]
    if not lines:
        return 'IDENTICAL'
    return ' | '.join(re.sub(r'original frame (\d+) \w+, port frame \d+ \w+', r'frame \1', l.strip())
                      for l in lines[:3])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 8)
    ap.add_argument('names', nargs='*')
    a = ap.parse_args()
    out = os.path.join(ROOT, 'build/tgrally/null-null/suite', time.strftime('%Y%m%d-%H%M%S'))
    os.makedirs(out)
    binary = os.path.join(out, 'tgrally')
    shutil.copy2(os.path.join(ROOT, 'build/tgrally/null-null/tgrally'), binary)
    jobs = []
    for p in sorted(glob.glob(os.path.join(SCRIPTS, '*.txt'))):
        name = os.path.splitext(os.path.basename(p))[0]
        if a.names and name not in a.names:
            continue
        jobs.append((frames_of(p), name, p))
    jobs.sort(reverse=True)

    def run(job):
        frames, name, path = job
        res = os.path.join(out, name + '.res')
        with open(res, 'w') as f:
            subprocess.run([PY, os.path.join(ROOT, 'ports/tgrally/tools/streamcmp.py'), '--bin', binary,
                            '--script', os.path.relpath(path, ROOT), '--frames', str(frames)],
                           cwd=ROOT, stdout=f, stderr=subprocess.STDOUT)
        return name, summary(res)
    t0 = time.time()
    with ThreadPoolExecutor(a.jobs) as ex:
        results = dict(ex.map(run, jobs))
    rows = ['%-26s %s' % (n, results[n]) for n in sorted(results)]
    same = sum(1 for v in results.values() if v == 'IDENTICAL')
    rows.append('%d of %d identical (%.0f s)' % (same, len(results), time.time() - t0))
    open(os.path.join(out, 'summary.txt'), 'w').write('\n'.join(rows) + '\n')
    print('\n'.join(rows))
    sys.exit(0 if same == len(results) else 1)


if __name__ == '__main__':
    main()
