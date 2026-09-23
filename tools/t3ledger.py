#!/usr/bin/env python3
"""t3ledger.py -- the live oracle's verdict on every T3 function.

config/t3_live.csv is written by tools/t3live.py and read by tools/t3.py
(Gate A5) and tools/tiers.py.  One row per @t3 VA:

    va, name, verdict, calls, captured, contexts, detail, date

VERDICTS
    EQUIVALENT   every captured real call agreed: return registers, every
                 byte either side wrote outside its own frame, and the import
                 calls it made (name + arguments), in order.
    DIVERGENT    at least one real call disagreed; `detail` names the first
                 difference and the capture that shows it.
    UNCOVERED    no driven script reached the function.  Never a pass.
    UNRUNNABLE   the function was reached but a side could not be run to
                 completion in isolation (a thread switch or blocking wait
                 inside it).  Never a pass.
    UNVERIFIED   no live-oracle run yet.  Every certification the retired
                 synthetic-seed oracle granted starts here: that oracle fed
                 random bytes and passed BrRaceStep while it called
                 BrCarSlotSetup with its arguments swapped.

A function is T3 only when this ledger says EQUIVALENT (CLAUDE.md rule 12).

    .venv/bin/python tools/t3ledger.py                  # counts by verdict
    .venv/bin/python tools/t3ledger.py --list DIVERGENT
    .venv/bin/python tools/t3ledger.py --seed-unverified  # add missing @t3 VAs
"""
from __future__ import print_function
import csv
import datetime
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
LEDGER = os.path.join(ROOT, 'config', 't3_live.csv')
FIELDS = ['va', 'name', 'verdict', 'calls', 'captured', 'contexts', 'detail', 'date']
VERDICTS = ('EQUIVALENT', 'DIVERGENT', 'UNRUNNABLE', 'UNCOVERED', 'UNVERIFIED')


def load():
    out = {}
    if os.path.exists(LEDGER):
        for r in csv.DictReader(open(LEDGER)):
            out[r['va'].lower()] = r
    return out


def save(rows):
    with open(LEDGER, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, lineterminator='\n')
        w.writeheader()
        for va in sorted(rows):
            w.writerow({k: rows[va].get(k, '') for k in FIELDS})


def verdict(va):
    r = load().get(va.lower())
    return r['verdict'] if r else 'UNVERIFIED'


def passes(va):
    return verdict(va) == 'EQUIVALENT'


def seed_unverified():
    from t3 import certified, report_rows
    rows = load()
    names = {k: v.get('name', '') for k, v in report_rows().items()}
    added = 0
    for va, info in certified().items():
        if va.startswith('?') or va in rows:
            continue
        rows[va] = {'va': va, 'name': names.get(va, ''), 'verdict': 'UNVERIFIED',
                    'calls': '', 'captured': '', 'contexts': '',
                    'detail': 'certified by the retired synthetic-seed oracle; not re-checked',
                    'date': datetime.date.today().isoformat()}
        added += 1
    save(rows)
    return added


def main(argv):
    if '--seed-unverified' in argv:
        print('added %d row(s)' % seed_unverified())
        return 0
    rows = load()
    if '--list' in argv:
        want = argv[argv.index('--list') + 1].upper()
        for va, r in sorted(rows.items()):
            if r['verdict'] == want:
                print('%s %-32s %s' % (va, r['name'], r['detail']))
        return 0
    from collections import Counter
    c = Counter(r['verdict'] for r in rows.values())
    print('live-oracle ledger: %d T3 functions' % len(rows))
    for v in VERDICTS:
        print('  %-11s %4d' % (v, c.get(v, 0)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
