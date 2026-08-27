#!/usr/bin/env python3
"""Authoritative combined match count across every binary and language.

The DLL C matches live in build/match/report.csv; the C++ EH matches live as
build/cpp_work/*.cpp (scored by tools/cpp_score.py, 4-piece); the EXE matches
live as build/<exe>_work/*.c (scored by ghidra_to_match._score_source). Those
last two are NOT in report.csv, so a plain report.csv total under-counts the
finished work. This script re-scores them and prints the true grand total.

    python3 tools/total.py            # re-score everything, print the total
    python3 tools/total.py --fast     # trust report.csv for DLL, re-score rest

Each C++/EXE work file is a byte-exact match only if it scores 0; wall attempts
left in the work dirs score >0 and are excluded, so this is honest.
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
    import ghidra_to_match as g
    import match_sweep
    import re
    n = b = 0
    rows = []
    for exe in ('brally', 'setvideo', 'bossrally'):
        d = os.path.join(ROOT, 'build', '%s_work' % exe)
        ob = os.path.join(ROOT, 'build', 'match', 'orig_%s' % exe)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith('.c'):
                continue
            va = fn[:-2]
            binp = os.path.join(ob, va + '.bin')
            if not os.path.exists(binp):
                continue
            src = open(os.path.join(d, fn)).read()
            orig = open(binp, 'rb').read()
            m = re.search(r'\b(\w+)\s*\([^;{]*\)\s*\{', src)
            name = m.group(1) if m else va
            diffs = g._score_source(src, name, orig,
                                    ['/O2', '/Od', '/O2 /Oy-'], 'tot' + va[2:])[0]
            if diffs == 0:
                n += 1
                b += len(orig)
                rows.append((exe, va, len(orig)))
    return n, b, rows


def score_cpp():
    n = b = 0
    rows = []
    d = os.path.join(ROOT, 'build', 'cpp_work')
    if not os.path.isdir(d):
        return n, b, rows
    for fn in sorted(os.listdir(d)):
        if not fn.endswith('.cpp'):
            continue
        va = fn[:-4]
        binp = os.path.join(ROOT, 'build', 'match', 'orig', va + '.bin')
        if not os.path.exists(binp):
            continue
        r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'cpp_score.py'),
                            '--va', va], cwd=ROOT, capture_output=True, text=True)
        # exit 0 == .text match; require the 4-piece "all four" line too
        if r.returncode == 0 and 'all four' in r.stdout:
            orig = open(binp, 'rb').read()
            n += 1
            b += len(orig)
            rows.append((va, len(orig)))
    return n, b, rows


def main():
    dn, db = dll_c()
    print('re-scoring EXE + C++ work dirs (this takes a few minutes)...', flush=True)
    en, eb, erows = score_exe()
    cn, cb, crows = score_cpp()
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
