#!/usr/bin/env python3
"""The filing gate: a matched function must live in the module it belongs to,
and must say in plain English what it does.

TWO rules, both long-standing, neither previously enforced:

  * rule 6 -- file each function into its named module as you match it
  * src/core/README.md -- "every function carries its original address and a
    description of behaviour", written as a `WHAT IT DOES:` comment directly
    above its @implements tag

A maintainer who has to re-trace a function to learn what it is for pays the
analysis cost twice, and the second time without the context the first had.
That is the whole reason the comment is mandatory: the byte-exactness is proved
by the sweep, but the PURPOSE only ever exists in the head of whoever matched
it, until they write it down.


Rule 6 has always said so; nothing enforced it, so by 2026-09-03 two thirds of
the matched C code (570 of 845 functions) was sitting in `sliceN_MM.c` address
batches, and the backlog grew with every unattended batch because
tools/autofile.py placed new matches by address.  A rule with no gate is a
preference.  This is the gate.

It fails (exit 1) on:

  * a matched function in an address batch that config/filing.csv has ALREADY
    assigned to a module -- the decision exists, the code just has not moved
  * a matched function with no filing.csv row at all -- it was matched without
    anyone recording what it is
  * a new `sliceN_MM.c` file (src/core/README.md: never add one)
  * a function whose file disagrees with its recorded module

It does NOT fail on a function that is honestly undecided (a filing.csv row
with an empty module).  That is a queue, not a defect, and it is reported as a
count so it can only shrink.  Run `python3 tools/filing.py --todo` to work it.

    python3 tools/fileaudit.py            # gate; exit 1 on a violation
    python3 tools/fileaudit.py --list     # every violation, not just a count
"""
import collections
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from filing import (FILING, is_slice, load_report, module_of,  # noqa: E402
                    NOT_A_MODULE)

# The batches that existed when the gate was introduced. The gate refuses any
# slice file not on this list, so the count can fall and never rise.
BASELINE = 62
# Functions without a WHAT IT DOES: comment when the check was introduced.
# The gate refuses any increase, so this can only fall.
DESC_BASELINE = 196


def shared_map():
    """glide va -> d3d va. A tag may name either; both are the same function."""
    out = {}
    p = os.path.join(ROOT, 'config', 'shared.csv')
    if os.path.exists(p):
        with open(p) as f:
            for r in csv.DictReader(f):
                g = (r.get('glide_va') or '').lower()
                if g:
                    out[g] = (r.get('d3d_va') or '').lower()
    return out


def describes(path, va, dva, _cache={}):
    """True iff the @implements tag for `va` carries a WHAT IT DOES: comment."""
    if path not in _cache:
        try:
            _cache[path] = open(os.path.join(ROOT, path)).read().split('\n')
        except OSError:
            _cache[path] = []
    lines = _cache[path]
    idx = None
    for i, l in enumerate(lines):
        if '@implements' in l and (va in l.lower() or (dva and dva in l.lower())):
            idx = i
            break
    if idx is None:
        return None                      # no tag at all: a different defect
    # Walk up through the attached comment block. Preprocessor lines are
    # stepped over, not treated as the end of it: a function whose matching
    # body is guarded writes the description above the `#ifdef`, and the tag
    # sits inside -- counting that as undescribed reports a comment that is
    # plainly there. The same guard means one description can cover both the
    # matching and the port body, which is correct: they are one function.
    k = idx - 1
    seen_code = False
    while k >= 0:
        s = lines[k].strip()
        if not s or s.startswith('#'):
            k -= 1
            continue
        if s.startswith('*') or s.startswith('/*') or s.endswith('*/'):
            if 'WHAT IT DOES' in s:
                return True
            k -= 1
            continue
        if not seen_code and s.startswith('@'):
            k -= 1
            continue
        break
    return False


def main():
    show = '--list' in sys.argv

    if not os.path.exists(FILING):
        sys.exit('config/filing.csv missing -- run python3 tools/filing.py')
    with open(FILING) as f:
        rec = {r['glide_va'].lower(): r for r in csv.DictReader(f)}

    rows = load_report()
    g2d = shared_map()
    unrecorded, stranded, wrong, undocumented = [], [], [], []
    undecided = 0
    for r in rows:
        gva = '0x%08x' % int(r['va'], 16)
        if describes(r['file'], gva, g2d.get(gva)) is False:
            undocumented.append(r)
        key = '0x%08x' % int(r['va'], 16)
        e = rec.get(key)
        if e is None:
            unrecorded.append(r)
            continue
        want = e.get('module') or ''
        here = module_of(r['file'])
        if not want:
            undecided += 1
        elif is_slice(r['file']):
            stranded.append((r, want))
        elif here and here != want:
            wrong.append((r, want, here))

    core = os.path.join(ROOT, 'src', 'core')
    slices = sorted(f for f in os.listdir(core)
                    if f.startswith('slice') and f.endswith('.c'))
    grew = len(slices) - BASELINE

    print('matched C functions        : %d' % len(rows))
    print('  undecided (a queue)      : %d' % undecided)
    print('  assigned but not moved   : %d' % len(stranded))
    print('  matched, never recorded  : %d' % len(unrecorded))
    print('  in the wrong module      : %d' % len(wrong))
    print('  no WHAT IT DOES: comment : %d  (baseline %d)'
          % (len(undocumented), DESC_BASELINE))
    print('address batches remaining  : %d  (baseline %d)' % (len(slices), BASELINE))

    if show:
        for r in undocumented[:200]:
            print('   UNDOCUMENTED %s %-30s %s'
                  % (r['va'], r['name'], r['file']))
        for r, want in stranded[:80]:
            print('   STRANDED %s %-30s -> %s' % (r['va'], r['name'], want))
        for r in unrecorded[:80]:
            print('   UNRECORDED %s %-28s %s' % (r['va'], r['name'], r['file']))
        for r, want, here in wrong[:80]:
            print('   WRONG %s %-26s in %s, recorded %s'
                  % (r['va'], r['name'], here, want))

    drift = max(0, len(undocumented) - DESC_BASELINE)
    bad = len(stranded) + len(unrecorded) + len(wrong) + max(0, grew) + drift
    if bad:
        print('\nFAIL: %d violation(s).' % bad)
        if grew > 0:
            print('  %d new address batch(es) -- never add a sliceN_MM.c.' % grew)
        if drift:
            print('  %d function(s) matched WITHOUT a WHAT IT DOES: comment.' % drift)
            print('  Write what it is for now, while you still know. Nobody')
            print('  else can recover it without re-tracing the whole function.')
        if stranded:
            print('  Move the assigned ones with tools/refile.py.')
        if unrecorded:
            print('  Record the rest with tools/filing.py.')
        return 1
    if undocumented:
        print('\nOK against the baselines, but %d function(s) still have no'
              ' WHAT IT DOES: comment.' % len(undocumented))
        print('   Lower DESC_BASELINE in this file as they are written, so the'
              ' number can only fall.')
        return 0
    print('\nOK: nothing matched is stranded, misplaced, unrecorded or'
          ' undocumented.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
