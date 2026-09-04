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


def tag_vas_at_head(rel):
    """The @implements VAs this file had in the last commit."""
    r = subprocess.run(['git', 'show', 'HEAD:' + rel],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        return set()
    return set(m.group(1).upper() for m in TAG.finditer(r.stdout))


def tag_vas_now(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return set(m.group(1).upper() for m in TAG.finditer(f.read()))


def main():
    bad, batches, filed_into_batch = [], [], []
    for rel in staged_files():
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        if re.match(r'src/core/slice\d+_\d+\.c$', rel):
            was = subprocess.run(['git', 'cat-file', '-e', 'HEAD:' + rel],
                                 cwd=ROOT, capture_output=True)
            if was.returncode != 0:
                batches.append(rel)
            else:
                # THE GAP THAT MADE THE BACKLOG. Refusing a NEW sliceN_MM.c
                # never stopped anyone dropping a new match into an EXISTING
                # one, which is exactly what tools/autofile.py did by address
                # -- 570 byte-exact functions stranded that way, and clearing
                # them cost a whole session. A match must be BORN in its
                # module. Compared by VA against HEAD so that re-spelling a
                # function already in the batch (the normal way a wall falls)
                # is untouched: only a VA the file did not have before is a
                # new match being filed into an address batch.
                for va in tag_vas_now(path) - tag_vas_at_head(rel):
                    filed_into_batch.append((rel, va))
        lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
        add = added_lines(rel)
        for i, l in enumerate(lines):
            if not TAG.search(l) or (i + 1) not in add:
                continue
            if not described(lines, i):
                bad.append((rel, i + 1, TAG.search(l).group(1)))

    if not bad and not batches and not filed_into_batch:
        return 0

    print('\nRULE 6 (CLAUDE.md): a decompiled function is not done until it')
    print('says what it does and lives in its module.\n')
    for rel, ln, va in bad:
        print('  NO DESCRIPTION  %s:%d  %s' % (rel, ln, va))
    for rel in batches:
        print('  NEW ADDRESS BATCH  %s -- never add a sliceN_MM.c' % rel)
    for rel, va in filed_into_batch:
        print('  NEW MATCH IN A BATCH  %s  %s -- file it in its module' % (rel, va))
    if bad:
        print('\nAdd a WHAT IT DOES: comment by the @implements tag. Plain')
        print('English, what the function is FOR -- not what the codegen does:')
        print('\n  /* WHAT IT DOES: read exactly n bytes, aborting if the file')
        print('   * was short. A truncated read means corrupt game data. */')
        print('  /* @implements 0x10008E60 glide BrFileReadChecked */\n')
        print('Write it now, while you still know. Nobody else can recover it')
        print('without re-tracing the whole function.')
    if batches or filed_into_batch:
        print('\nFile the function into a responsibility folder instead.')
        print('See src/core/README.md and tools/filing.py.')
    if filed_into_batch:
        print('\nA sliceN_MM.c is an address batch, not a module. Put the')
        print('function in the folder that owns what it DOES, with the')
        print('include set it needs, and sweep both files before committing.')
        print('Re-spelling a function the batch already had is fine -- this')
        print('fires only on a VA the file did not have in the last commit.')
    print('\n(--no-verify bypasses this; tools/fileaudit.py will still see it.)')
    return 1


if __name__ == '__main__':
    sys.exit(main())
