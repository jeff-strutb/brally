#!/usr/bin/env python3
"""portpp.py -- prove a change leaves every port translation unit unchanged.

WHAT IT DOES: preprocesses every TU build.sh compiles for the Mac port, with
the flags build.sh gives it, and records a hash of the token stream
(`clang -E -P`, whitespace collapsed).  Identical tokens mean clang is handed
the same program, so a refactor of where port code lives -- src/ versus
ports/ -- that keeps every hash cannot have changed the port.

    portpp.py --snapshot [--out F]   record (default build/portpp.json)
    portpp.py --check    [--out F]   exit 1 if any TU's tokens changed

Run it after `ports/macos/tools/portgen.py` (build.sh does that first): a
module with a port spec is preprocessed from its generated copy under
build/port/, which is what build.sh compiles.  A TU is keyed by the path of
its SOURCE of record (src/core/..., tests/..., ports/...), so the key does
not move when a module gains or loses a spec.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portgen  # noqa: E402

DEFAULT = os.path.join(ROOT, 'build', 'portpp.json')


def cflags():
    """build.sh's CFLAGS, read from build.sh so the two cannot drift."""
    for ln in open(os.path.join(ROOT, 'build.sh')):
        if ln.startswith('CFLAGS='):
            return ln.split('=', 1)[1].strip().strip('"').split()
    sys.exit('portpp: no CFLAGS= line in build.sh')


def tus():
    """(key, argv) for every TU build.sh compiles, minus the link steps."""
    cf = cflags()
    out = []
    for key, path, extra in portgen.core_units():
        out.append((key, cf + extra + [path]))
    for s in ('slice3_32', 'slice6_71', 'slice6_73'):
        p = 'ports/macos/legacy/%s.c' % s
        out.append(('host:' + p, cf + ['-DBR_HOST_LINK', p]))
    for f in sorted(os.listdir(os.path.join(ROOT, 'ports', 'macos'))):
        if f.endswith('.c') and f != 'drive_sandbox.c':
            out.append(('ports/macos/' + f, cf + ['ports/macos/' + f]))
    for f in sorted(os.listdir(os.path.join(ROOT, 'tests'))):
        if f.startswith('test_') and f.endswith('.c'):
            out.append(('tests/' + f, cf + ['tests/' + f]))
    out.append(('tools/brview.c', cf + ['tools/brview.c']))
    return out


def pp(item):
    key, argv = item
    p = subprocess.run(['clang', '-E', '-P', '-w'] + argv, cwd=ROOT,
                       capture_output=True)
    if p.returncode != 0:
        return key, 'NOPP'
    toks = b' '.join(p.stdout.split())
    return key, hashlib.sha1(toks).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--snapshot', action='store_true')
    g.add_argument('--check', action='store_true')
    ap.add_argument('--out', default=DEFAULT)
    a = ap.parse_args()

    with ThreadPoolExecutor(os.cpu_count() or 8) as ex:
        now = dict(ex.map(pp, tus()))
    if a.snapshot:
        os.makedirs(os.path.dirname(a.out), exist_ok=True)
        json.dump(now, open(a.out, 'w'), indent=0, sort_keys=True)
        print('snapshot: %d port TUs (%d do not preprocess)'
              % (len(now), sum(v == 'NOPP' for v in now.values())))
        return 0
    old = json.load(open(a.out))
    bad = 0
    for k in sorted(set(old) | set(now)):
        if k not in now:
            print('  GONE    %s' % k)
        elif k not in old:
            print('  NEW     %s' % k)
        elif old[k] != now[k]:
            print('  CHANGED %s' % k)
            bad += 1
    print('checked %d port TUs: %d changed' % (len(now), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
