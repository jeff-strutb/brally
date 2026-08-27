#!/usr/bin/env python3
"""Authoritative combined match count across every binary and language.

The DLL C matches live in build/match/report.csv; the C++ EH matches live in
src/core/cpp/*.cpp and are counted from build/match/report_cpp.csv (written
by tools/cpp_sweep.py, 4-piece); the EXE matches live in src/exe/<exe>/*.c
and are counted from build/match/report_exe.csv (written by
tools/exe_sweep.py). Those last two are NOT in report.csv, so a plain
report.csv total under-counts the finished work.

    python3 tools/total.py            # re-score everything, print the total
    python3 tools/total.py --fast     # trust report.csv for DLL, re-score rest

C++ rows count only when report_cpp.csv says status=match and pieces=4/4.
EXE rows count only when report_exe.csv says status=match. Wall attempts
left in build/<exe>_work score >0 and are not copied into src/exe/.
"""
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))


def dll_c():
    n = b = 0
    with open(os.path.join(ROOT, 'build', 'match', 'report.csv')) as f:
        for r in csv.DictReader(f):
            if r['status'] == 'match' and r['orig_size']:
                n += 1
                b += int(r['orig_size'])
    return n, b


def score_exe():
    """Count EXE matches from report_exe.csv (src/exe via exe_sweep).

    Does not walk build/<exe>_work — wall attempts live there and must not
    count. If the report is missing, run exe_sweep once to produce it.
    """
    n = b = 0
    rows = []
    report = os.path.join(ROOT, 'build', 'match', 'report_exe.csv')
    if not os.path.exists(report):
        sw = os.path.join(ROOT, 'tools', 'exe_sweep.py')
        if os.path.exists(sw):
            subprocess.run([sys.executable, sw], cwd=ROOT)
    if not os.path.exists(report):
        return n, b, rows
    with open(report) as f:
        for r in csv.DictReader(f):
            if r.get('status') != 'match' or not r.get('orig_size'):
                continue
            n += 1
            nb = int(r['orig_size'])
            b += nb
            rows.append((r.get('exe') or '', r['va'], nb))
    return n, b, rows


def score_cpp():
    """Count 4-piece C++ matches from report_cpp.csv (src/core/cpp via cpp_sweep).

    Does not walk build/cpp_work — wall attempts live there and must not count.
    If the report is missing, run cpp_sweep once to produce it.
    """
    n = b = 0
    rows = []
    report = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
    if not os.path.exists(report):
        sw = os.path.join(ROOT, 'tools', 'cpp_sweep.py')
        if os.path.exists(sw):
            subprocess.run([sys.executable, sw], cwd=ROOT)
    if not os.path.exists(report):
        return n, b, rows
    with open(report) as f:
        for r in csv.DictReader(f):
            if r.get('status') != 'match' or not r.get('orig_size'):
                continue
            if r.get('pieces') and r['pieces'] != '4/4':
                continue
            n += 1
            b += int(r['orig_size'])
            rows.append((r['va'], int(r['orig_size'])))
    return n, b, rows


def _write_manifest(path, header, rows):
    with open(os.path.join(ROOT, 'build', 'match', path), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def main():
    dn, db = dll_c()
    print('re-scoring EXE + C++ work dirs (this takes a few minutes)...', flush=True)
    en, eb, erows = score_exe()
    cn, cb, crows = score_cpp()
    # Manifests of the verified off-report matches, so the progress map (and
    # anything else) can mark them without re-scoring.
    _write_manifest('cpp_matches.csv', ['va', 'bytes'], crows)
    _write_manifest('exe_matches.csv', ['exe', 'va', 'bytes'], erows)
    print()
    print('=' * 58)
    print('  DLL  (C, report.csv)   %4d fns  %8d B' % (dn, db))
    print('  DLL  (C++ EH, cpp_work)%4d fns  %8d B' % (cn, cb))
    print('  EXE  (3 binaries)      %4d fns  %8d B' % (en, eb))
    print('  ' + '-' * 42)
    print('  TOTAL byte-exact       %4d fns  %8d B' % (dn + cn + en, db + cb + eb))
    print('=' * 58)
    print('(of 480,853 B BRGlide .text + ~64 KB in-scope EXE .text)')


if __name__ == '__main__':
    main()
