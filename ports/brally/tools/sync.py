#!/usr/bin/env python3
"""sync.py -- what the decomp has done since the 64-bit core forked from it.

ports/brally/src and ports/brally/include are src/ and include/ as of the
commit in ports/brally/src/FORKED-FROM, retyped for 64 bits on top.  Most
decomp work after the fork is byte shape (M2: functions respelled to match
byte for byte, bodies moved into the C++ lane), which never has to flow into
the core.  A blind three-way merge of that work is wrong: the decomp's
respelled body lands beside the core's retyped one and the result reads
neither's locals.  So this reports, and the carrying is by hand:

    sync.py [--to COMMIT]          every decomp commit since the fork, each
                                   with the files it touched, then each
                                   function whose signature changed (return
                                   type or parameters), and the files the
                                   decomp added, removed or renamed
    sync.py --merge FILE [--to C]  three-way merge one file into the core
                                   (git merge-file conflict markers)
    sync.py --stamp [COMMIT]       the core now has everything up to COMMIT
                                   (default HEAD): move FORKED-FROM

A commit that changes behaviour, a signature, a global's identity or a file
name is carried into the core and checked in lockstep; a respelling alone is
not.
"""
import argparse
import os
import re
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
PORT = os.path.join(ROOT, 'ports/brally')
STAMP = os.path.join(PORT, 'src/FORKED-FROM')
TREES = ('src', 'include')
EXTS = ('.c', '.cpp', '.h')
# a top-level function definition or prototype: `type name(params)`
SIG = re.compile(r'^(?!static\b|typedef\b|#|\s|/|\*)([A-Za-z_][\w \t\*]*?[\s\*])(\w+)\s*\(([^;{)]*)\)\s*;?\s*$', re.M)


def git(*args):
    return subprocess.run(['git'] + list(args), cwd=ROOT, capture_output=True, text=True, check=True).stdout


def show(commit, path):
    r = subprocess.run(['git', 'show', '%s:%s' % (commit, path)], cwd=ROOT, capture_output=True)
    return r.stdout.decode('latin-1') if r.returncode == 0 else None


def sigs(text):
    out = {}
    for ret, name, params in SIG.findall(text or ''):
        out.setdefault(name, ' '.join(('%s%s(%s)' % (ret, name, params)).split()))
    return out


def report(base, to):
    print('decomp commits since the fork (%s..%s):' % (base[:8], to[:8]))
    for line in git('log', '--format=%h %s', '%s..%s' % (base, to), '--', *TREES).splitlines():
        h = line.split()[0]
        files = [f for f in git('show', '--format=', '--name-only', h, '--', *TREES).split() if f.endswith(EXTS)]
        print('  %s' % line[:150])
        for f in files:
            print('      %s' % f)
    print('\nsignatures changed:')
    for line in git('diff', '--name-status', '-M', base, to, '--', *TREES).splitlines():
        row = line.split('\t')
        st, old, new = row[0], row[1], row[-1]
        if not new.endswith(EXTS):
            continue
        if st.startswith('A'):
            print('  NEW     %s' % new)
            continue
        if st.startswith('D'):
            print('  REMOVED %s' % old)
            continue
        if st.startswith('R'):
            print('  RENAMED %s -> %s' % (old, new))
        a, b = sigs(show(base, old)), sigs(show(to, new))
        for name in sorted(set(a) & set(b)):
            if a[name] != b[name]:
                print('  %s\n      was %s\n      now %s' % (new, a[name], b[name]))


def merge(base, to, path):
    ours = os.path.join(PORT, path)
    old, new = show(base, path), show(to, path)
    if old is None or new is None or not os.path.exists(ours):
        raise SystemExit('sync: %s is not in the fork base, the target and the core' % path)
    with tempfile.TemporaryDirectory() as d:
        fb, ft = os.path.join(d, 'base'), os.path.join(d, 'theirs')
        open(fb, 'w', encoding='latin-1').write(old)
        open(ft, 'w', encoding='latin-1').write(new)
        r = subprocess.run(['git', 'merge-file', '-L', 'port', '-L', 'fork base', '-L', 'decomp', ours, fb, ft])
    print('sync: %s merged, %d conflicts' % (path, max(r.returncode, 0)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--to', default='HEAD')
    ap.add_argument('--merge')
    ap.add_argument('--stamp', nargs='?', const='HEAD')
    a = ap.parse_args()
    base = open(STAMP).read().split()[0]
    if a.stamp:
        to = git('rev-parse', a.stamp).strip()
        open(STAMP, 'w').write(to + '\n')
        print('sync: FORKED-FROM is now %s' % to[:8])
        return
    to = git('rev-parse', a.to).strip()
    if a.merge:
        merge(base, to, a.merge)
    elif to == base:
        print('sync: the core is at %s already' % to[:8])
    else:
        report(base, to)


if __name__ == '__main__':
    main()
