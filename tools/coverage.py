#!/usr/bin/env python3
"""Honest hand-C coverage for BRGlide.dll.

The raw "N of M mapped functions" number overstates the remaining work,
because a large share of the map is not hand-written C at all: linker import
thunks, incremental-link jump stubs, and C++ exception-handling funclets that
fall out of compiling a .cpp's try/catch.  Those are enumerated in
config/fenced.csv (built by signature, conservatively -- see that file).

This tool subtracts the fence from the glide map to report the real hand-C
target and how much of it is byte-exact.

    python3 tools/coverage.py

Uses config/functions_glide.csv (the GAME binary's map -- NOT functions.csv,
which is D3D-keyed) and the DLL-C match count from tools/total.py.
"""
import csv, os, subprocess, sys
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load(path, cols=('va', 'size')):
    with open(os.path.join(ROOT, path)) as f:
        return list(csv.DictReader(f))


def main():
    mapped = load('config/functions_glide.csv')
    n_map = len(mapped)
    b_map = sum(int(r['size']) for r in mapped if r['size'])

    fenced = load('config/fenced.csv')
    by_class = Counter(r['class'] for r in fenced)
    n_fence = len(fenced)
    b_fence = sum(int(r['size']) for r in fenced)

    # DLL-C byte-exact count from total.py's report
    n_exact = b_exact = 0
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    if os.path.exists(rep):
        for r in csv.DictReader(open(rep)):
            if r.get('status') == 'match' and r.get('orig_size'):
                n_exact += 1
                b_exact += int(r['orig_size'])

    target = n_map - n_fence
    tb = b_map - b_fence
    print("=" * 58)
    print(f"  glide map (game binary)      {n_map:5d} fns   {b_map:8d} B")
    print(f"  fenced (not hand-C)         -{n_fence:5d} fns  -{b_fence:8d} B")
    for c, k in sorted(by_class.items(), key=lambda x: -x[1]):
        cb = sum(int(r['size']) for r in fenced if r['class'] == c)
        print(f"      {c:16s}         {k:5d} fns   {cb:8d} B")
    print("  " + "-" * 54)
    print(f"  HAND-C TARGET                {target:5d} fns   {tb:8d} B")
    print(f"  byte-exact (DLL C)           {n_exact:5d} fns   {b_exact:8d} B"
          f"   ({100.0*n_exact/target:.1f}% of target)")
    print(f"  remaining hand-C             {target-n_exact:5d} fns")
    print("=" * 58)
    print("  fenced = linker thunks/stubs (reproduced at link) + C++ EH")
    print("  funclets (reproduced by their parent TU). Not outstanding")
    print("  hand-C work. See config/fenced.csv.")


if __name__ == '__main__':
    main()
