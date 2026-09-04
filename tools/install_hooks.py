#!/usr/bin/env python3
"""Point this clone's git hooks at tools/hooks/ so rule 6 is enforced.

Git hooks live in .git/, which is NOT tracked, so a hook committed to the tree
does nothing until someone wires it up -- and "someone remembers to wire it up"
is exactly the failure mode rule 6 already had once. setup.sh runs this, and it
is safe to re-run.

    python3 tools/install_hooks.py            # install
    python3 tools/install_hooks.py --check    # report, change nothing
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'tools', 'hooks')


def main():
    check = '--check' in sys.argv
    cur = subprocess.run(['git', 'config', '--get', 'core.hooksPath'],
                         cwd=ROOT, capture_output=True, text=True).stdout.strip()
    want = 'tools/hooks'
    if cur == want:
        print('hooks: already installed (core.hooksPath=%s)' % cur)
        return 0
    if check:
        print('hooks: NOT installed (core.hooksPath=%r) -- run '
              'python3 tools/install_hooks.py' % (cur or None))
        return 1
    if cur:
        # Someone points hooks elsewhere on purpose; refuse rather than
        # silently taking their configuration over.
        print('hooks: core.hooksPath is already %r -- leaving it alone.' % cur)
        print('       Copy tools/hooks/pre-commit into it by hand.')
        return 1
    for f in os.listdir(SRC):
        os.chmod(os.path.join(SRC, f), 0o755)
    subprocess.run(['git', 'config', 'core.hooksPath', want], cwd=ROOT)
    print('hooks: installed (core.hooksPath=%s)' % want)
    print('       pre-commit refuses a commit that adds an @implements with no')
    print('       WHAT IT DOES: comment, or that adds a new sliceN_MM.c.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
