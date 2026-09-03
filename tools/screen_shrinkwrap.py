#!/usr/bin/env python3
"""Screen the residue for the shrink-wrapped-register-save cause class.

VC5 sinks a callee-saved `push` past an early-out only when the source has
NO early return there -- a guarded block (`if (a && b) { ... }`) sinks the
saves into the block, while `if (!a) return id;` chains hoist them into the
prologue and rotate the whole allocation.  See docs/VC5-IDIOMS.md
("Semantically-redundant mid-returns block shrink-wrap", "A wrapping `if`
is not an early return", and the 0x1003BA30 entry).

The tell is visible in the ORIGINAL bytes alone and needs no compile: a
push of ebx/ebp/esi/edi that appears AFTER the first conditional jump.
This lists every `diff` row in report.csv whose original does that, so the
class can be worked as a family instead of found by accident.

    python3 tools/screen_shrinkwrap.py            # ranked candidates
    python3 tools/screen_shrinkwrap.py --all      # include match rows too
"""
from __future__ import print_function

import argparse
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402

REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
SAVES = ('ebx', 'ebp', 'esi', 'edi')

md = Cs(CS_ARCH_X86, CS_MODE_32)


def late_saves(code, va):
    """(n_late, first_late_offset, n_prologue) for one original."""
    seen_branch = False
    late = []
    prologue = 0
    for ins in md.disasm(code, va):
        m = ins.mnemonic
        if m == 'push' and ins.op_str in SAVES:
            if seen_branch:
                late.append(ins.address - va)
            else:
                prologue += 1
        elif m.startswith('j') and m != 'jmp':
            seen_branch = True
    return len(late), (late[0] if late else None), prologue


def early_returns(path, name):
    """Count `return` statements in the named function's matching body."""
    try:
        text = open(os.path.join(ROOT, path)).read()
    except IOError:
        return None
    m = re.search(r'\b%s\s*\(' % re.escape(name), text)
    if not m:
        return None
    depth = 0
    started = False
    n = 0
    for i in range(m.end(), len(text)):
        c = text[i]
        if c == '{':
            depth += 1
            started = True
        elif c == '}':
            depth -= 1
            if started and depth == 0:
                break
        elif started and c == 'r' and text[i:i + 6] == 'return':
            n += 1
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--all', action='store_true',
                    help='include rows already matching')
    args = ap.parse_args()

    rows = []
    with open(REPORT) as f:
        for r in csv.DictReader(f):
            if not args.all and r['status'] != 'diff':
                continue
            va = int(r['va'], 16)
            binp = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
            if not os.path.exists(binp):
                continue
            code = open(binp, 'rb').read()
            nlate, first, npro = late_saves(code, va)
            if not nlate:
                continue
            nret = early_returns(r['file'], r['name'])
            rows.append((nlate, r, npro, first, nret, int(r['diffs'] or 0)))

    rows.sort(key=lambda t: (-t[0], t[5]))
    print('%-12s %6s %5s %5s %5s %5s  %s'
          % ('va', 'diffs', 'late', 'pro', 'at', 'rets', 'name'))
    for nlate, r, npro, first, nret, nd in rows:
        print('%-12s %6d %5d %5d %5s %5s  %s  [%s]'
              % (r['va'], nd, nlate, npro,
                 '+0x%X' % first if first is not None else '-',
                 '-' if nret is None else nret, r['name'], r['file']))
    print('\n%d candidates (originals that sink a callee-save past a branch)'
          % len(rows))


if __name__ == '__main__':
    main()
