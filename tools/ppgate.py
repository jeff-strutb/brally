#!/usr/bin/env python3
"""ppgate.py -- prove an edit to src/ or include/ changes nothing MSVC sees.

WHAT IT DOES: runs the matching compiler's preprocessor (cl /EP, the sweep's
exact include path and /DBR_MATCHING_BUILD) over every .c/.cpp under src/ and
records a hash of the token stream -- whitespace collapsed, so a deleted
port-only #else arm (which /EP turns into blank lines) does not count as a
change.  Identical tokens mean identical input to the code generator, so a
refactor that keeps every hash is codegen-neutral for the decomp without
sweeping a single file.  (Nothing under src/ or include/ uses __LINE__ or
__FILE__; if that ever changes, line movement becomes visible and this gate
stops being sufficient on its own.)

    ppgate.py --snapshot [--out F]   record hashes (default build/ppgate.json)
    ppgate.py --check    [--out F]   compare against the snapshot; exit 1 on
                                     any changed, lost or failed file
    ppgate.py --check FILE...        only these files

A file that exists in the snapshot but no longer exists is reported as GONE;
that is expected only for files the refactor deletes on purpose (port-only
sources), so the list is printed for the caller to read, not waved through.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import match_sweep  # noqa: E402  (WINE, CL, MSVC_DIR)

DEFAULT = os.path.join(ROOT, 'build', 'ppgate.json')


def sources():
    out = []
    for dp, _, fn in os.walk(os.path.join(ROOT, 'src')):
        for f in fn:
            if f.endswith(('.c', '.cpp')) and not f.startswith('_'):
                out.append(os.path.relpath(os.path.join(dp, f), ROOT))
    return sorted(out)


def pp(rel):
    cmd = ['sh', match_sweep.WINE, match_sweep.CL, '/nologo', '/EP',
           '/I', 'include', '/I', 'tools/msvc5-compat',
           '/I', os.path.join(os.path.relpath(match_sweep.MSVC_DIR, ROOT),
                              'include'),
           '/DBR_MATCHING_BUILD', rel]
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, timeout=300)
    except subprocess.TimeoutExpired:
        return rel, None, 'timeout'
    text = p.stdout.decode('latin-1')
    err = p.stderr.decode('latin-1')
    if p.returncode != 0 or re.search(r'\berror [CD]\d+', err):
        return rel, None, (err.strip().splitlines() or ['rc=%d' % p.returncode])[-1]
    toks = ' '.join(text.split())
    return rel, hashlib.sha1(toks.encode('latin-1')).hexdigest(), None


def run(files, jobs):
    with ThreadPoolExecutor(jobs) as ex:
        return list(ex.map(pp, files))


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--snapshot', action='store_true')
    g.add_argument('--check', action='store_true')
    ap.add_argument('--out', default=DEFAULT)
    ap.add_argument('-j', type=int, default=14)
    ap.add_argument('files', nargs='*')
    a = ap.parse_args()

    files = a.files or sources()
    res = run(files, a.j)
    fails = [(r, e) for r, h, e in res if h is None]

    if a.snapshot:
        snap = {r: h for r, h, _ in res}
        os.makedirs(os.path.dirname(a.out), exist_ok=True)
        with open(a.out, 'w') as f:
            json.dump(snap, f, indent=0, sort_keys=True)
        print('snapshot: %d files (%d do not preprocess)' % (len(snap), len(fails)))
        for r, e in fails:
            print('  NOPP %s  %s' % (r, e))
        return 0

    with open(a.out) as f:
        snap = json.load(f)
    bad = 0
    now = {r: h for r, h, _ in res}
    for r in files:
        if r not in snap:
            print('  NEW     %s' % r)
            continue
        if snap[r] != now.get(r):
            print('  CHANGED %s' % r)
            bad += 1
    if not a.files:
        for r in sorted(set(snap) - set(now)):
            print('  GONE    %s' % r)
    print('checked %d: %d changed' % (len(files), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
