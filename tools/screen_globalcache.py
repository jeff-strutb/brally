#!/usr/bin/env python3
"""Screen the residue for globals cached in a local instead of updated in place.

A decompiled draft says "load the global, work on the copy, store it back".
The original says it in place, and the difference is not cosmetic: the copy
gives the loaded value a lifetime the original never gave it, which flips
which operand a commutative op accumulates into and turns an `inc` that
destroys the load into a `lea` that preserves it. Three functions in
src/core/slice2_16.c went byte-exact on that one edit; see the
"Read and update the GLOBAL" entry in docs/VC5-IDIOMS.md.

The tell is in the SOURCE, not the bytes: inside one matching-build body, a
local is assigned from a global and, later, that same global is assigned
from a local.

    python3 tools/screen_globalcache.py            # ranked candidates
    python3 tools/screen_globalcache.py --all      # matched rows too
"""
from __future__ import print_function

import argparse
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')

# A global here is any identifier that is not a local of the function: we
# approximate with the project's naming (DAT_*, g_*, s_*) plus anything
# ALL_CAPS-free that the body never declares.
GLOBALISH = re.compile(r'^(DAT_[0-9a-fA-F]+|g_\w+|s_\w+)$')
ASSIGN = re.compile(r'^\s*(\**\(?[\w.\->\[\]]+\)?)\s*=\s*([^=;][^;]*);')
IDENT = re.compile(r'\b([A-Za-z_]\w*)\b')


def body_of(text, name):
    m = re.search(r'\b%s\s*\([^;{]*\)\s*\{' % re.escape(name), text)
    if not m:
        return None
    depth = 0
    start = m.end() - 1
    for i in range(start, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    return None


def cached_globals(body):
    """{global: (local, load_line, store_line)} for load-then-store-back."""
    loads = {}          # local -> (global, line no)
    hits = {}
    for n, line in enumerate(body.splitlines()):
        m = ASSIGN.match(line)
        if not m:
            continue
        lhs, rhs = m.group(1).strip(), m.group(2).strip()
        rhs_ids = IDENT.findall(rhs)
        # local = GLOBAL;  (rhs is one identifier, optionally cast)
        if len(rhs_ids) == 1 and GLOBALISH.match(rhs_ids[0]) \
                and not GLOBALISH.match(lhs) and lhs.isidentifier():
            loads[lhs] = (rhs_ids[0], n)
        # GLOBAL = local;
        elif GLOBALISH.match(lhs):
            for local, (g, ln) in loads.items():
                if g == lhs and re.search(r'\b%s\b' % re.escape(local), rhs):
                    hits[lhs] = (local, ln, n)
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--all', action='store_true')
    args = ap.parse_args()

    cache = {}
    rows = []
    with open(REPORT) as f:
        for r in csv.DictReader(f):
            if not args.all and r['status'] != 'diff':
                continue
            path = os.path.join(ROOT, r['file'])
            if path not in cache:
                try:
                    cache[path] = open(path).read()
                except IOError:
                    cache[path] = ''
            body = body_of(cache[path], r['name'])
            if not body:
                continue
            hits = cached_globals(body)
            if hits:
                rows.append((int(r['diffs'] or 0), r, hits))

    rows.sort(key=lambda t: t[0])
    for nd, r, hits in rows:
        names = ', '.join('%s via %s' % (g, v[0]) for g, v in hits.items())
        print('%-12s diffs=%-6d %-32s %s' % (r['va'], nd, r['name'], names))
        print('%s%s' % (' ' * 14, r['file']))
    print('\n%d diff rows cache a global in a local and store it back' % len(rows))


if __name__ == '__main__':
    main()
