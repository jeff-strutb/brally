#!/usr/bin/env python3
"""sync.py -- bring the decomp's progress into the port's fork of it.

ports/tgrally/src is src/tgrally as of the commit in src/FORKED-FROM, with the
port's changes on top (addresses, byte order, the arena).  The decomp keeps
moving (M2: functions respelled to match byte for byte, new ones
transcribed); each src/tgrally file changed since the fork commit is merged
three ways into the port's copy: base = src/tgrally at FORKED-FROM, theirs =
src/tgrally at HEAD (committed work only), ours = the port's file.  A file new
in the decomp is copied; one the decomp deleted is reported.

    sync.py [--to COMMIT] [--dry-run]

Clean merges are written and FORKED-FROM moves to COMMIT.  With conflicts,
the files are written with conflict markers, FORKED-FROM stays, and the
conflicting files are listed: resolve them, then run again.
"""
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SRC = os.path.join(ROOT, 'ports/tgrally/src')
STAMP = os.path.join(SRC, 'FORKED-FROM')


def git(*args, check=True):
    return subprocess.run(['git'] + list(args), cwd=ROOT, capture_output=True, text=True, check=check).stdout


def show(commit, path):
    r = subprocess.run(['git', 'show', '%s:%s' % (commit, path)], cwd=ROOT, capture_output=True)
    return r.stdout if r.returncode == 0 else None


# the decomp tree, now and before 2026-10-05 (src/tgrally)
TREES = ('src/tgrally', 'src/tgrally')


def rel_of(path):
    """The path under the decomp's source tree; its headers (src/tgrally/include,
    formerly src/tgrally/include) are not port source and stay out."""
    if path.startswith('src/tgrally/include/'):
        return None
    for t in TREES:
        if path.startswith(t + '/'):
            return path[len(t) + 1:]
    return None


def show_rel(commit, rel):
    for t in TREES:
        b = show(commit, t + '/' + rel)
        if b is not None:
            return b
    return None


def changed_rels(base, to):
    """(status, rel) for each decomp file that differs between BASE and TO,
    whichever layout either commit has; a pure move is not a change."""
    out = {}
    for l in git('diff', '--name-status', '-M', base, to, '--', *TREES).splitlines():
        row = l.split('\t')
        st, old, new = row[0], row[1], row[-1]
        rn, ro = rel_of(new), rel_of(old)
        if rn is None:
            continue
        if st.startswith('R') and rn == ro and st[1:] == '100':
            continue
        out[rn] = 'D' if st.startswith('D') else ('A' if show_rel(base, rn) is None else 'M')
    return [(st, rel) for rel, st in sorted(out.items())]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--to', default='HEAD')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--resolved', action='store_true', help='conflicts resolved: finish the pending sync')
    a = ap.parse_args()
    base = open(STAMP).read().split()[0]
    if a.resolved:
        to = open(STAMP + '.pending').read().split()[0]
        files = [rel for st, rel in changed_rels(base, to)]
        finish(to, [f for f in files if f.endswith(('.c', '.h', '.s'))])
        print('sync: FORKED-FROM is now %s' % to[:8])
        return
    to = git('rev-parse', a.to).strip()
    if to == base:
        print('sync: the fork is at %s already' % to[:8])
        return
    changed = changed_rels(base, to)
    clean, conflicts, added, removed = [], [], [], []
    for st, rel in changed:
        if not rel.endswith(('.c', '.h', '.s')):
            continue                # the decomp's progress notes are not port source
        ours = os.path.join(SRC, rel)
        if st.startswith('D'):
            removed.append(rel)
            continue
        new = show_rel(to, rel)
        old = show_rel(base, rel)
        if old is None or not os.path.exists(ours):
            added.append(rel)
            if not a.dry_run:
                os.makedirs(os.path.dirname(ours), exist_ok=True)
                open(ours, 'wb').write(new)
            continue
        with tempfile.TemporaryDirectory() as d:
            fb, ft = os.path.join(d, 'base'), os.path.join(d, 'theirs')
            open(fb, 'wb').write(old)
            open(ft, 'wb').write(new)
            fo = os.path.join(d, 'ours')
            open(fo, 'wb').write(open(ours, 'rb').read())
            r = subprocess.run(['git', 'merge-file', '-L', 'port', '-L', 'fork base', '-L', 'decomp',
                                fo, fb, ft], capture_output=True)
            (conflicts if r.returncode else clean).append(rel)
            if not a.dry_run:
                open(ours, 'wb').write(open(fo, 'rb').read())
    for rel in clean:
        print('merged    %s' % rel)
    for rel in added:
        print('added     %s' % rel)
    for rel in removed:
        print('REMOVED   %s (deleted in the decomp: delete or keep by hand)' % rel)
    for rel in conflicts:
        print('CONFLICT  %s' % rel)
    if a.dry_run:
        return
    if conflicts:
        open(STAMP + '.pending', 'w').write(to + '\n')
        print('sync: %d conflicts; FORKED-FROM stays at %s until they are resolved '
              '(then: sync.py --resolved)' % (len(conflicts), base[:8]))
        sys.exit(1)
    finish(to, clean + added)
    print('sync: %d merged, %d added; FORKED-FROM is now %s' % (len(clean), len(added), to[:8]))


def finish(to, files):
    """the decomp's spellings the arena model must convert (raw display-list
    words, pointer casts) are rewritten where the compiler flags them; then
    the stamp moves"""
    paths = [os.path.join(SRC, f) for f in files if f.endswith('.c')]
    paths = [p for p in paths if os.path.exists(p)]   # a file the decomp deleted
    if paths:
        subprocess.run([sys.executable, os.path.join(ROOT, 'ports/tgrally/tools/errfix.py')] + paths, cwd=ROOT)
    subprocess.run([sys.executable, os.path.join(ROOT, 'ports/tgrally/tools/abicheck.py')], cwd=ROOT)
    open(STAMP, 'w').write(to + '\n')
    if os.path.exists(STAMP + '.pending'):
        os.remove(STAMP + '.pending')


if __name__ == '__main__':
    main()
