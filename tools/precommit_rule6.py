#!/usr/bin/env python3
"""Refuse a commit that adds a decompiled function without a description, or
that adds a new address batch. Rule 6, enforced.

Run from .git/hooks/pre-commit (installed by tools/install_hooks.py).

WHY A HOOK AND NOT JUST THE RULE. Rule 6 existed for months and was ignored:
by 2026-09-03, 570 of 845 matched C functions sat in address batches and 196
had no description. Nothing ran, so nothing stopped it. tools/fileaudit.py
audits the whole tree, which is the right tool for "where do we stand" but the
wrong one for "don't let this in" -- it fails on the pre-existing backlog, so
wiring it to every commit would block unrelated work and get switched off.

So this judges only the diff being committed:

  * every `@implements` line ADDED must have a `WHAT IT DOES:` comment in the
    same commit, on either side of the tag inside its comment block
  * no NEW `src/core/sliceN_MM.c` file (src/core/README.md: never add one)

It says nothing about the backlog. Draining that is fileaudit.py's job.
"""
import os
import re
import subprocess
import sys

ROOT = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True).stdout.strip()
TAG = re.compile(r'@implements\s+(0x[0-9A-Fa-f]+)')


def staged_files():
    out = subprocess.run(['git', 'diff', '--cached', '--name-only',
                          '--diff-filter=ACM'],
                         cwd=ROOT, capture_output=True, text=True).stdout
    return [f for f in out.splitlines() if f.endswith(('.c', '.cpp', '.h'))]


def added_lines(path):
    """Line numbers (in the NEW file) that this commit adds."""
    out = subprocess.run(['git', 'diff', '--cached', '-U0', '--', path],
                         cwd=ROOT, capture_output=True, text=True).stdout
    added, ln = set(), 0
    for l in out.splitlines():
        m = re.match(r'^@@ -\S+ \+(\d+)(?:,(\d+))?', l)
        if m:
            ln = int(m.group(1))
            continue
        if l.startswith('+') and not l.startswith('+++'):
            added.add(ln)
            ln += 1
        elif not l.startswith('-'):
            ln += 1
    return added


def described(lines, idx):
    """A WHAT IT DOES: in the comment block attached to the tag at idx."""
    for step in (-1, +1):
        k = idx + step
        while 0 <= k < len(lines):
            s = lines[k].strip()
            if not s or s.startswith('#'):
                k += step
                continue
            if s.startswith(('*', '/*')) or s.endswith('*/'):
                if 'WHAT IT DOES' in s:
                    return True
                k += step
                continue
            break
    return False


def main():
    bad, batches = [], []
    for rel in staged_files():
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        if re.match(r'src/core/slice\d+_\d+\.c$', rel):
            was = subprocess.run(['git', 'cat-file', '-e', 'HEAD:' + rel],
                                 cwd=ROOT, capture_output=True)
            if was.returncode != 0:
                batches.append(rel)
        lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
        add = added_lines(rel)
        for i, l in enumerate(lines):
            if not TAG.search(l) or (i + 1) not in add:
                continue
            if not described(lines, i):
                bad.append((rel, i + 1, TAG.search(l).group(1)))

    if not bad and not batches:
        return 0

    print('\nRULE 6 (CLAUDE.md): a decompiled function is not done until it')
    print('says what it does and lives in its module.\n')
    for rel, ln, va in bad:
        print('  NO DESCRIPTION  %s:%d  %s' % (rel, ln, va))
    for rel in batches:
        print('  NEW ADDRESS BATCH  %s -- never add a sliceN_MM.c' % rel)
    if bad:
        print('\nAdd a WHAT IT DOES: comment by the @implements tag. Plain')
        print('English, what the function is FOR -- not what the codegen does:')
        print('\n  /* WHAT IT DOES: read exactly n bytes, aborting if the file')
        print('   * was short. A truncated read means corrupt game data. */')
        print('  /* @implements 0x10008E60 glide BrFileReadChecked */\n')
        print('Write it now, while you still know. Nobody else can recover it')
        print('without re-tracing the whole function.')
    if batches:
        print('\nFile the function into a responsibility folder instead.')
        print('See src/core/README.md and tools/filing.py.')
    print('\n(--no-verify bypasses this; tools/fileaudit.py will still see it.)')
    return 1


if __name__ == '__main__':
    sys.exit(main())
