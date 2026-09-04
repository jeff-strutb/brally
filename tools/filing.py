#!/usr/bin/env python3
"""config/filing.csv -- the durable answer to "which module does this function
belong to", decided ONCE and never re-derived.

Rule 6 says file each function into its named module as you match it.  That was
not happening: on 2026-09-03, 570 of 845 matched C functions still sat in
`sliceN_MM.c` address batches, because tools/autofile.py places a new MATCH by
ADDRESS -- it picks the slice whose VA range brackets the function.  Nothing
recorded what a function IS, so every filing attempt re-analysed it from
scratch and most attempts never happened at all.

This tool writes that record.  Each row is one hand-C target function:

    glide_va,name,module,basis,file

`module` is a folder under src/core (see src/core/README.md), or empty when
nothing yet justifies an assignment.  `basis` says WHY, so the assignment can
be audited instead of trusted:

    filed       the function is already in that module -- ground truth
    neighbour   both nearest already-filed functions BY ADDRESS agree.  MSVC 5
                emits a translation unit contiguously, so an unfiled function
                bracketed by two filed ones from the same module is almost
                certainly from that same original .c
    prefix      its Br<Word> name prefix maps to exactly one module across the
                filed ground truth, from >= 2 samples and with no counterexample
    (empty)     UNASSIGNED.  Not a gap in this tool -- a decision nobody has
                made yet.  It must be made by a person, once, and recorded here

Assignments are deliberately NOT invented for the rest.  A guessed module in a
config file is worse than an empty one: it reads as authoritative, it is not
checked by any gate, and finding it wrong later costs more than never having
written it.  Measured coverage of the automatic bases is about a third of the
backlog; the other two thirds are a decision queue, and `--todo` prints it.

    python3 tools/filing.py                 # rewrite config/filing.csv
    python3 tools/filing.py --todo          # the unassigned queue, biggest first
    python3 tools/filing.py --set 0xVA menus   # record one decision by hand
"""
import bisect
import collections
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILING = os.path.join(ROOT, 'config', 'filing.csv')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
CORE = os.path.join(ROOT, 'src', 'core')

# Folders that are a responsibility (src/core/README.md).  `generated` and
# `cpp` are process artifacts, not responsibilities, and never a filing target.
NOT_A_MODULE = {'generated', 'cpp'}


def modules():
    return sorted(d for d in os.listdir(CORE)
                  if os.path.isdir(os.path.join(CORE, d))
                  and d not in NOT_A_MODULE)


def module_of(relfile):
    """The responsibility folder a source file sits in, or None."""
    d = os.path.dirname(relfile)
    if not d.startswith('src/core/'):
        return None
    m = d[len('src/core/'):]
    return None if (m in NOT_A_MODULE or '/' in m) else m


def is_slice(relfile):
    return bool(re.match(r'slice\d+_\d+\.c$', os.path.basename(relfile)))


def load_report():
    if not os.path.exists(REPORT):
        return []
    with open(REPORT) as f:
        return [r for r in csv.DictReader(f)
                if r.get('status') == 'match' and r.get('va')]


def existing():
    """Rows already recorded, so a hand decision is never overwritten."""
    out = {}
    if os.path.exists(FILING):
        with open(FILING) as f:
            for r in csv.DictReader(f):
                out[r['glide_va'].lower()] = r
    return out


def build():
    rows = load_report()
    prev = existing()

    filed = {}          # va -> module, ground truth
    unfiled = []        # rows still in an address batch
    for r in rows:
        va = int(r['va'], 16)
        m = module_of(r['file'])
        if m:
            filed[va] = m
        elif is_slice(r['file']):
            unfiled.append(r)

    # --- basis 2: address neighbours ---------------------------------------
    anchors = sorted(filed)
    def neighbour(va):
        i = bisect.bisect_left(anchors, va)
        lo = filed.get(anchors[i - 1]) if i > 0 else None
        hi = filed.get(anchors[i]) if i < len(anchors) else None
        return lo if (lo and lo == hi) else None

    # --- basis 3: name prefix, learned from ground truth only --------------
    def prefix(n):
        m = re.match(r'(Br[A-Za-z][a-z0-9]*)', n or '')
        return m.group(1) if m else None

    votes = collections.defaultdict(collections.Counter)
    for r in rows:
        m = module_of(r['file'])
        p = prefix(r.get('name'))
        if m and p:
            votes[p][m] += 1
    pure = {p: c.most_common(1)[0][0] for p, c in votes.items()
            if len(c) == 1 and sum(c.values()) >= 2}

    out = []
    for r in rows:
        va = int(r['va'], 16)
        key = '0x%08x' % va
        name = r.get('name') or ''
        old = prev.get(key)
        # ANY recorded decision wins. This used to preserve only basis=manual
        # and recompute everything else, which silently threw away the
        # description-derived and neighbour-derived assignments the moment the
        # tool was re-run -- the decision is supposed to be made ONCE.
        if old and old.get('module'):
            out.append({'glide_va': '0x%08X' % va, 'name': name,
                        'module': old['module'],
                        'basis': old.get('basis') or 'recorded',
                        'file': r['file']})
            continue
        m = module_of(r['file'])
        if m:
            basis = 'filed'
        else:
            m = neighbour(va)
            basis = 'neighbour' if m else ''
            if not m:
                m = pure.get(prefix(name))
                basis = 'prefix' if m else ''
        out.append({'glide_va': '0x%08X' % va, 'name': name,
                    'module': m or '', 'basis': basis, 'file': r['file']})

    out.sort(key=lambda r: r['glide_va'])
    with open(FILING, 'w', newline='') as f:
        w = csv.DictWriter(f, ['glide_va', 'name', 'module', 'basis', 'file'])
        w.writeheader()
        w.writerows(out)
    return out, unfiled


def main():
    if '--set' in sys.argv:
        i = sys.argv.index('--set')
        va, mod = sys.argv[i + 1], sys.argv[i + 2]
        if mod not in modules():
            sys.exit('not a module folder: %s (have: %s)'
                     % (mod, ', '.join(modules())))
        rows = list(csv.DictReader(open(FILING))) if os.path.exists(FILING) else []
        key = va.lower().replace('0x', '')
        hit = False
        for r in rows:
            if r['glide_va'].lower().replace('0x', '') == key:
                r['module'], r['basis'] = mod, 'manual'
                hit = True
        if not hit:
            sys.exit('no filing row for %s (run tools/filing.py first)' % va)
        with open(FILING, 'w', newline='') as f:
            w = csv.DictWriter(f, ['glide_va', 'name', 'module', 'basis', 'file'])
            w.writeheader()
            w.writerows(rows)
        print('recorded %s -> %s (manual)' % (va, mod))
        return

    out, unfiled = build()
    by = collections.Counter(r['basis'] or '(unassigned)' for r in out)
    misplaced = [r for r in out if r['module'] and is_slice(r['file'])]
    print('config/filing.csv: %d matched C functions' % len(out))
    for k in ('filed', 'manual', 'neighbour', 'prefix', '(unassigned)'):
        if by.get(k):
            print('   %-14s %4d' % (k, by[k]))
    print()
    print('still in an address batch : %d' % len(unfiled))
    print('   of those, assigned     : %d  (ready to move)' % len(misplaced))
    print('   of those, undecided    : %d  (run --todo)'
          % (len(unfiled) - len(misplaced)))

    if '--todo' in sys.argv:
        rows = load_report()
        size = {int(r['va'], 16): int(r['orig_size'] or 0) for r in rows}
        todo = [r for r in out if not r['module'] and is_slice(r['file'])]
        todo.sort(key=lambda r: -size.get(int(r['glide_va'], 16), 0))
        print('\nundecided, largest first:')
        for r in todo[:60]:
            print('   %s %6d B  %-34s %s'
                  % (r['glide_va'], size.get(int(r['glide_va'], 16), 0),
                     r['name'], r['file']))


if __name__ == '__main__':
    main()
