#!/usr/bin/env python3
"""Run viewmerge.py for every local view types/viewmap.csv names.

Reads build/portable/views.csv (viewscan.py) for which files declare which
local records, applies the curated map (a row with a file applies to that
file only and wins over the name-wide row; an empty canon means "not a
view"), and prints viewmerge's notes.
"""
import csv
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
HDR = {'BrUiCtl_': 'br_ui.h', 'BrUiPage_': 'br_ui.h', 'BrPhase_': 'br_phase.h',
       'BrTextList': 'slice3_39.h', 'BrTextBox': 'slice3_39.h', 'BrDriverCar': 'slice3_41.h',
       'BrCarBody': 'br_cartypes.h', 'BrAiPathNode': 'br_coretypes.h', 'BrPeerRec': 'br_coretypes.h',
       'BrNetSlot': 'slice1_02.h', 'BrCollPlane': 'slice1_08.h', 'BrRbBody': 'slice3_44.h'}


def main():
    os.chdir(ROOT)
    only = sys.argv[1:]
    rules, per_file = {}, {}
    for r in csv.DictReader(open('ports/brally/types/viewmap.csv')):
        if r['file']:
            per_file[(r['file'], r['view'])] = r['canon']
        else:
            rules[r['view']] = r['canon']
    jobs = []
    for r in csv.DictReader(open('build/portable/views.csv')):
        f, v = r['file'], r['view']
        if only and f not in only:
            continue
        canon = per_file.get((f, v), rules.get(v))
        if canon:
            jobs.append((f, v, canon))
    for f, v, canon in jobs:
        p = subprocess.run([sys.executable, 'ports/brally/tools/viewmerge.py', f, v, canon,
                            '--include', 'ports/brally/include/' + HDR[canon]], capture_output=True, text=True)
        sys.stdout.write(p.stdout + p.stderr)


if __name__ == '__main__':
    main()
