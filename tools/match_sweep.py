#!/usr/bin/env python3
"""Sweep every decomped source file through the original compiler and report
which @implements'd functions come out byte-identical.

Each file is compiled twice -- /O2 (what the release build used for nearly
everything) and /Od (stubs and unoptimised wrappers) -- and each function takes
whichever of the two matches.  Results land in build/match/report.csv so the
next pass can sort the frontier by how close each function is.

Usage:
    python3 tools/match_sweep.py                 # sweep everything
    python3 tools/match_sweep.py src/core/x.c    # sweep one file
    python3 tools/match_sweep.py --summary       # re-print last report
"""
import csv
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from match_diff import parse_implements, parse_coff_obj  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Go through the wrapper so this uses the repo-local Wine and prefix, exactly
# like build_match.sh does.
WINE = os.path.join(ROOT, 'tools', 'wine.sh')


def find_cl():
    for cand in (os.path.join(ROOT, 'tools', 'msvc5', 'bin', 'cl.exe'),
                 os.path.join(ROOT, 'tools', 'msvc5', 'cl.exe')):
        if os.path.exists(cand):
            return cand
    sys.exit('cl.exe not found under tools/msvc5/ -- run: sh setup.sh')


CL = find_cl()
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')

VARIANTS = [('O2', '/O2'), ('Od', '/Od')]


def compile_variant(src, tag, opt):
    objdir = os.path.join(ROOT, 'build', 'match', 'obj_' + tag)
    os.makedirs(objdir, exist_ok=True)
    base = os.path.splitext(os.path.basename(src))[0]
    obj = os.path.join(objdir, base + '.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    rel_obj = os.path.relpath(obj, ROOT).replace('/', '\\')
    # cl.exe reads a leading '/' as an option prefix, so a Unix absolute path
    # is parsed as an unknown flag and the compiler reports "missing source
    # filename".  Everything handed to it must be relative to the repo root.
    rel_src = os.path.relpath(src, ROOT)
    # msvc5-compat supplies stdint.h/stdbool.h, which VC5 predates.  It is
    # tracked in git, unlike msvc5/include, which setup.sh re-extracts.
    cmd = ['sh', WINE, CL, '/nologo', opt, '/W3', '/I', 'include',
           '/I', 'tools/msvc5-compat', '/I', 'tools/msvc5/include',
           '/DBR_MATCHING_BUILD', '/c', rel_src, '/Fo' + rel_obj]
    # Wine occasionally wedges on a prefix lock; a stuck cl.exe must not stall
    # the whole sweep, so cap it and report the file as a compile failure.
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           timeout=120)
        out = p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return None, ['cl.exe timed out']
    if not os.path.exists(obj):
        err = [l.strip() for l in out.splitlines() if 'error' in l.lower()]
        return None, err[:3] or ['no obj, no diagnostic']
    return obj, []


def score(orig_bytes, recomp_bytes, relocs):
    """Return (is_match, n_real_diffs, recomp_len)."""
    trimmed = recomp_bytes[:len(orig_bytes)]
    ndiff = sum(1 for i in range(min(len(trimmed), len(orig_bytes)))
                if i not in relocs and trimmed[i] != orig_bytes[i])
    size_ok = len(trimmed) >= len(orig_bytes)
    return (ndiff == 0 and size_ok), ndiff, len(recomp_bytes)


def sweep_file(src):
    """Return list of dicts, one per @implements'd function in src."""
    implements = parse_implements(src)
    if not implements:
        return []

    objs = {}
    errors = {}
    for tag, opt in VARIANTS:
        obj, err = compile_variant(src, tag, opt)
        if obj:
            try:
                objs[tag] = parse_coff_obj(obj)
            except Exception as e:              # malformed obj -- treat as fail
                errors[tag] = [str(e)]
        else:
            errors[tag] = err

    rows = []
    for va, name in implements:
        orig_path = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
        row = {'file': os.path.relpath(src, ROOT), 'va': '0x%08X' % va,
               'name': name, 'status': '', 'opt': '', 'orig_size': '',
               'recomp_size': '', 'diffs': ''}
        if not objs:
            row['status'] = 'compile_error'
            row['diffs'] = '; '.join(next(iter(errors.values()), [])) or 'unknown'
            rows.append(row)
            continue
        if not os.path.exists(orig_path):
            row['status'] = 'no_orig'
            rows.append(row)
            continue

        with open(orig_path, 'rb') as f:
            orig_bytes = f.read()
        row['orig_size'] = len(orig_bytes)

        best = None
        for tag, _ in VARIANTS:
            funcs = objs.get(tag)
            if not funcs or name not in funcs:
                continue
            recomp, relocs = funcs[name]
            ok, ndiff, rlen = score(orig_bytes, recomp, relocs)
            # rank: match first, then fewest diffs, then closest size
            key = (0 if ok else 1, ndiff, abs(rlen - len(orig_bytes)))
            if best is None or key < best[0]:
                best = (key, tag, ok, ndiff, rlen)

        if best is None:
            row['status'] = 'not_in_obj'
        else:
            _, tag, ok, ndiff, rlen = best
            row['status'] = 'match' if ok else 'diff'
            row['opt'] = tag
            row['recomp_size'] = rlen
            row['diffs'] = ndiff
        rows.append(row)
    return rows


def sources():
    out = []
    for dirpath, _, files in os.walk(os.path.join(ROOT, 'src')):
        for fn in sorted(files):
            if not fn.endswith('.c'):
                continue
            p = os.path.join(dirpath, fn)
            with open(p, errors='replace') as f:
                if '@implements' in f.read():
                    out.append(p)
    return sorted(out)


def summarise(rows):
    total = len(rows)
    match = [r for r in rows if r['status'] == 'match']
    diff = [r for r in rows if r['status'] == 'diff']
    other = [r for r in rows if r['status'] not in ('match', 'diff')]
    mbytes = sum(int(r['orig_size']) for r in match if r['orig_size'] != '')
    print()
    print('=' * 62)
    print('  MATCH  %4d / %d tagged functions  (%.1f%%, %d bytes)'
          % (len(match), total, 100.0 * len(match) / max(total, 1), mbytes))
    print('  DIFF   %4d' % len(diff))
    for st in sorted(set(r['status'] for r in other)):
        print('  %-6s %4d' % (st.upper()[:6], sum(1 for r in other
                                                  if r['status'] == st)))
    print('=' * 62)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if '--summary' in sys.argv:
        with open(REPORT) as f:
            summarise(list(csv.DictReader(f)))
        return

    srcs = args or sources()
    all_rows = []
    for i, src in enumerate(srcs, 1):
        rows = sweep_file(src)
        all_rows.extend(rows)
        m = sum(1 for r in rows if r['status'] == 'match')
        # A build that will not compile must never look like a build that
        # compiled and matched nothing.
        broke = [r for r in rows if r['status'] == 'compile_error']
        note = '  !! COMPILE ERROR: %s' % broke[0]['diffs'][:70] if broke else ''
        print('[%3d/%3d] %-40s %d/%d%s' % (i, len(srcs),
                                           os.path.relpath(src, ROOT),
                                           m, len(rows), note), flush=True)

    if not args:                                # full sweep -- rewrite report
        os.makedirs(os.path.dirname(REPORT), exist_ok=True)
        with open(REPORT, 'w', newline='') as f:
            w = csv.DictWriter(f, fieldnames=['file', 'va', 'name', 'status',
                                              'opt', 'orig_size',
                                              'recomp_size', 'diffs'])
            w.writeheader()
            w.writerows(all_rows)
    summarise(all_rows)


if __name__ == '__main__':
    main()
