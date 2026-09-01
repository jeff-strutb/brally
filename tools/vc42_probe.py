#!/usr/bin/env python3
"""Compile source files with Visual C++ 4.2 (tools/msvc42) and score every
@implements'd function against the original bytes -- a parallel to match_sweep
that answers "what does the 1996 compiler reproduce?".

  python3 tools/vc42_probe.py --files src/core/slice2_12.c ...   # specific files
  python3 tools/vc42_probe.py --status match --limit 40          # cross-check
  python3 tools/vc42_probe.py --status diff                      # hunt new wins

Reuses match_sweep's parse_implements / load_orig / score / parse_coff_obj so
the reloc-masking and preamble handling are IDENTICAL to the VC5 sweep; only
the compiler, include path and opt variants change.
"""
from __future__ import print_function
import argparse, csv, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor, as_completed

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import match_sweep as ms
from match_diff import parse_coff_obj

WINE = os.path.join(ROOT, 'tools', 'wine.sh')
CL42 = os.path.join(ROOT, 'tools', 'msvc42', 'bin', 'CL.EXE')
VARIANTS = [('v42_O2', '/O2'), ('v42_Ox', '/Ox'), ('v42_O1', '/O1'),
            ('v42_O2y', '/O2 /Oy-')]


def compile42(src, tag, opt):
    objdir = os.path.join(ROOT, 'build', 'match', 'obj_' + tag)
    os.makedirs(objdir, exist_ok=True)
    base = os.path.splitext(os.path.basename(src))[0]
    obj = os.path.join(objdir, base + '.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    rel_obj = os.path.relpath(obj, ROOT).replace('/', '\\')
    rel_src = os.path.relpath(src, ROOT)
    cmd = ['sh', WINE, CL42, '/nologo'] + opt.split() + ['/W3', '/I', 'include',
           '/I', 'tools/msvc5-compat', '/I', 'tools/msvc42/include',
           '/DBR_MATCHING_BUILD', '/c', rel_src, '/Fo' + rel_obj]
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           timeout=180)
    except subprocess.TimeoutExpired:
        return None, ['timeout']
    if not os.path.exists(obj):
        out = p.stdout + p.stderr
        err = [l.strip() for l in out.splitlines() if 'error' in l.lower()]
        return None, err[:2] or ['no obj']
    return obj, []


def probe_file(src):
    impl = ms.parse_implements(src)
    if not impl:
        return []
    objs = {}
    firsterr = None
    for tag, opt in VARIANTS:
        obj, err = compile42(src, tag, opt)
        if obj:
            try:
                objs[tag] = parse_coff_obj(obj)
            except Exception as e:
                firsterr = firsterr or [str(e)]
        else:
            firsterr = firsterr or err
    out = []
    for va, name in impl:
        orig_path = os.path.join(ms.ORIG_DIR, '0x%08X.bin' % va)
        rec = {'va': '0x%08X' % va, 'name': name, 'file': os.path.relpath(src, ROOT)}
        if not objs:
            rec.update(status='compile_error', diffs=';'.join(firsterr or ['?']))
            out.append(rec); continue
        if not os.path.exists(orig_path):
            rec.update(status='no_orig', diffs=''); out.append(rec); continue
        orig = ms.load_orig(orig_path, va)
        rec['orig_size'] = len(orig)
        best = None
        for tag, _ in VARIANTS:
            f = objs.get(tag)
            if not f or name not in f:
                continue
            recomp, relocs = f[name]
            ok, ndiff, rlen = ms.score(orig, recomp, relocs)
            key = (0 if ok else 1, ndiff, abs(rlen - len(orig)))
            if best is None or key < best[0]:
                best = (key, tag, ok, ndiff, rlen)
        if best is None:
            rec.update(status='not_in_obj', diffs='')
        else:
            _, tag, ok, ndiff, rlen = best
            rec.update(status='match' if ok else 'diff', opt=tag,
                       diffs=ndiff, recomp_size=rlen)
        out.append(rec)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--files', nargs='*')
    ap.add_argument('--status', help='filter report.csv rows by this status')
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--jobs', type=int, default=6)
    a = ap.parse_args()

    # current VC5 status per (va)
    cur = {}
    with open(ms.REPORT) as fh:
        for r in csv.DictReader(fh):
            cur[r['va']] = r

    files = []
    if a.files:
        files = a.files
    else:
        seen = set()
        for r in cur.values():
            if a.status and r['status'] != a.status:
                continue
            if r['file'] not in seen:
                seen.add(r['file']); files.append(os.path.join(ROOT, r['file']))
    if a.limit:
        files = files[:a.limit]

    print('VC4.2 probe: %d files, %d jobs' % (len(files), a.jobs))
    results = []
    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = {ex.submit(probe_file, f): f for f in files}
        for i, fut in enumerate(as_completed(futs)):
            try:
                results.extend(fut.result())
            except Exception as e:
                print('  ERR', futs[fut], e)
            print('  [%d/%d] %s' % (i + 1, len(files),
                  os.path.relpath(futs[fut], ROOT)), end='\r')
    print()

    new_match, regress, improved, ce = [], [], [], []
    for rec in results:
        c = cur.get(rec['va'])
        v5 = c['status'] if c else '?'
        v42 = rec['status']
        if v42 == 'compile_error':
            ce.append(rec); continue
        if v42 == 'match' and v5 != 'match':
            new_match.append(rec)
        elif v5 == 'match' and v42 != 'match':
            regress.append(rec)
        elif v42 == 'diff' and v5 == 'diff':
            try:
                if int(rec.get('diffs', 1e9)) < int(c['diffs']):
                    improved.append((rec, c))
            except (ValueError, KeyError):
                pass

    print('\n=== NEW MATCHES under VC4.2 (were not match under VC5):', len(new_match))
    for r in sorted(new_match, key=lambda x: x['va']):
        print('  %s %s [%s]  %s' % (r['va'], r['name'], r.get('opt'), r['file']))
    print('\n=== REGRESSIONS (VC5 match, VC4.2 does NOT):', len(regress))
    for r in sorted(regress, key=lambda x: str(x.get('diffs')))[:40]:
        print('  %s %s  v42_diffs=%s (%s)' % (r['va'], r['name'], r.get('diffs'), r['status']))
    print('\n=== IMPROVED diffs (still diff, fewer under VC4.2):', len(improved))
    for r, c in sorted(improved, key=lambda x: int(x[0]['diffs']))[:40]:
        print('  %s %s  %s -> %s' % (r['va'], r['name'], c['diffs'], r['diffs']))
    print('\n=== compile_error files:', len(set(r['file'] for r in ce)))
    for f in sorted(set(r['file'] for r in ce))[:20]:
        e = next(r for r in ce if r['file'] == f)
        print('  %s : %s' % (f, str(e.get('diffs'))[:80]))


if __name__ == '__main__':
    main()
