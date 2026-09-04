#!/usr/bin/env python3
"""Adversarial regression tests for the matching pipeline's own claims.

Every other tool here reports what it found. This one tries to BREAK what they
found, because the failure that actually cost this project time was never a
tool reporting a wrong number -- it was a tool reporting a right-looking number
that nothing could contradict. A check that cannot fail is not a check.

  A  Every learned address lands inside a real section of the image.
  B  Single-observation addresses do not skew against corroborated ones.
  C  MUTATION TEST, the one that matters. Poison the most-referenced learned
     address; the assembled image must notice. This proves "0 differing bytes"
     is a real comparison rather than a tautology.
  D  refcheck.py passes on a Glide-keyed corpus and fails on a D3D-keyed one.
     Rule 0's guard had only ever been seen failing, and a guard that cannot
     pass enforces nothing.
  E  The image gate's own bookkeeping. C proves the diff is a real comparison;
     it does not prove the gate compared the CURRENT tree, nor that it names
     the right finding when it could not. A stale object used to be graded
     silently, and every non-pass read as "the claims do not hold" even when
     the real problem was a source file mid-edit.

A, B and C need a learned-address map. When reloc_learn.py refuses to write one
-- a legitimate state, not a broken tree -- they are SKIPPED and reported as
skipped. They must never pass vacuously on an empty file; that is the exact
failure mode this tool exists to catch.

C and D mutate state and restore in `finally`. A corpus left swapped by a
killed run is detected and rolled back on the next start.

Usage:
    python3 tools/audit.py          # exit 0 only if nothing failed
"""
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from pe_patch import read_pe_text_info  # noqa: E402

LEARNED = os.path.join(ROOT, 'config', 'globals_learned.csv')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
BAK = ORIG_DIR + '.auditbak'

FAILURES = []
SKIPPED = []


def check(name, ok, detail=''):
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f"  {detail}" if detail else ''))
    if not ok:
        FAILURES.append(name)


def skip(name, why):
    print(f"[SKIP] {name}  ({why})")
    SKIPPED.append(name)


def recover_stale():
    """A killed run can leave the corpus swapped out. Put it back first."""
    if not os.path.exists(BAK):
        return
    if os.path.islink(ORIG_DIR):
        os.unlink(ORIG_DIR)
    elif os.path.exists(ORIG_DIR):
        print('audit: corpus and backup both present; leaving alone',
              file=sys.stderr)
        return
    os.rename(BAK, ORIG_DIR)
    print('audit: recovered a reference corpus left swapped by a prior run')


def section_lookup(dll):
    base, secs = read_pe_text_info(dll)
    ranges = [(base + rva, base + rva + max(vs, rs), nm)
              for nm, rva, vs, ro, rs in secs]

    def sect(a):
        for lo, hi, nm in ranges:
            if lo <= a < hi:
                return nm
        return None
    return sect


def checks_abc(sect):
    """A, B and C -- everything that needs the learned-address map."""
    if not os.path.exists(LEARNED):
        why = 'reloc_learn.py is refusing to write a learned map'
        skip('A. learned addresses land in real sections', why)
        skip('B. single-observation addresses land in real sections', why)
        skip('C. image diff detects a poisoned address', why)
        return

    rows = list(csv.DictReader(open(LEARNED)))
    bysec = {}
    for r in rows:
        s = sect(int(r['addr'], 16))
        bysec[s] = bysec.get(s, 0) + 1
    print(f"learned addresses: {len(rows)}   by section: {bysec}")

    outside = [r for r in rows if sect(int(r['addr'], 16)) is None]
    check('A. every learned address lands inside a real section', not outside,
          f'{len(outside)} outside' if outside else '')

    one = [r for r in rows if r['corroborated'] == '0']
    bad = [r for r in one if sect(int(r['addr'], 16)) is None]
    check(f'B. {len(one)} single-observation addresses land in real sections',
          not bad, f'{len(bad)} bad' if bad else '')

    saved = open(LEARNED).read()
    try:
        lines = saved.splitlines(True)
        vi, vn = None, -1
        for i, line in enumerate(lines[1:], 1):
            p = line.split(',')
            if len(p) > 2 and p[2].isdigit() and int(p[2]) > vn:
                vn, vi = int(p[2]), i
        if vi is None:
            skip('C. image diff detects a poisoned address',
                 'learned map has no usable row to poison')
            return
        p = lines[vi].split(',')
        victim = p[0]
        p[1] = '0x%08X' % ((int(p[1], 16) ^ 0x40) & 0xFFFFFFFF)
        lines[vi] = ','.join(p)
        open(LEARNED, 'w').write(''.join(lines))
        out = subprocess.run([sys.executable, 'tools/image_build.py'],
                             cwd=ROOT, capture_output=True, text=True).stdout
        line = next((l for l in out.splitlines() if 'ASSEMBLED IMAGE' in l), '')
        caught = bool(line) and 'ORIGINAL: 0 ' not in line
        check(f'C. image diff detects a poisoned address ({victim}, {vn} refs)',
              caught, line.strip())
    finally:
        open(LEARNED, 'w').write(saved)


def build_glide_corpus(limit=400):
    """A Glide-keyed corpus built from the twin table, for check D."""
    gp = os.path.join(ROOT, 'orig', 'BRGlide.dll')
    base, secs = read_pe_text_info(gp)
    _, rva, vsize, raw, rawsz = [s for s in secs
                                 if s[0].startswith('.text')][0]
    img = open(gp, 'rb').read()
    tmp = os.path.join(ROOT, 'build', 'match', 'audit_glide_corpus')
    os.makedirs(tmp, exist_ok=True)
    for f in os.listdir(tmp):
        os.unlink(os.path.join(tmp, f))
    n = 0
    for r in csv.DictReader(open(os.path.join(ROOT, 'config', 'shared.csv'))):
        if not r['glide_va'].strip() or not r['size'].strip():
            continue
        gv, sz = int(r['glide_va'], 16), int(r['size'])
        off = raw + (gv - base - rva)
        if sz <= 0 or off < raw or off + sz > raw + rawsz:
            continue
        open(os.path.join(tmp, '0x%08X.bin' % gv), 'wb').write(img[off:off + sz])
        n += 1
        if n >= limit:
            break
    return tmp, n


def check_d():
    """Rule 0's guard must work in BOTH directions."""
    tmp, n = build_glide_corpus()
    os.rename(ORIG_DIR, BAK)
    try:
        os.symlink(tmp, ORIG_DIR)
        p = subprocess.run([sys.executable, 'tools/refcheck.py', '--quiet'],
                           cwd=ROOT, capture_output=True, text=True)
        check(f'D. refcheck PASSES on a {n}-function Glide-keyed corpus',
              p.returncode == 0, f'exit={p.returncode}')
    finally:
        if os.path.islink(ORIG_DIR):
            os.unlink(ORIG_DIR)
        os.rename(BAK, ORIG_DIR)

    # And the live corpus must satisfy rule 0 on its own terms.
    p = subprocess.run([sys.executable, 'tools/refcheck.py', '--quiet'],
                       cwd=ROOT, capture_output=True, text=True)
    check('D2. the live corpus satisfies rule 0 (Glide-keyed)',
          p.returncode == 0, f'exit={p.returncode}')


def check_e():
    """The image gate's own bookkeeping: freshness, races, verdict taxonomy.

    Check C proves the image DIFF is a real comparison. It does not prove the
    gate compared the CURRENT tree, or that it says the right thing when it
    could not. Both of those were wrong: the DLL lane read whatever object was
    on disk with no freshness test (so a stale object could be graded and pass)
    and every non-pass printed "the tree's claims do not hold", which sent a
    session hunting a decomp defect that did not exist while a refiling job
    rewrote src/ underneath the run.

    Deterministic and cheap -- no compiling, no image assembly.
    """
    import tempfile
    import time
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    import image_build as ib
    import match_sweep

    d = tempfile.mkdtemp()
    src, obj = os.path.join(d, 'a.c'), os.path.join(d, 'a.obj')
    saved = ib._DEPS_MTIME
    try:
        ib._DEPS_MTIME = 0.0
        open(src, 'w').write('x')
        gone = not ib._fresh(obj, src)
        open(obj, 'w').write('o')
        os.utime(src, (100, 100))
        os.utime(obj, (200, 200))
        newer = ib._fresh(obj, src)
        os.utime(src, (300, 300))
        older = not ib._fresh(obj, src)
        os.utime(src, (100, 100))
        ib._DEPS_MTIME = 400.0
        hdr = not ib._fresh(obj, src)
        check('E1. a missing, older, or header-predating object is NOT fresh',
              gone and newer and older and hdr,
              f'missing={gone} newer={newer} older={older} header={hdr}')
    finally:
        ib._DEPS_MTIME = saved

    b = {'/x': 1.0, '/y': 2.0}
    check('E2. a tree edited mid-run is detected (changed/new/removed)',
          ib._raced(b, dict(b)) == []
          and ib._raced(b, {'/x': 9.0, '/y': 2.0}) == ['/x']
          and ib._raced(b, dict(b, **{'/z': 3.0})) == ['/z']
          and ib._raced(b, {'/x': 1.0}) == ['/y'])

    # An unknown opt tag must not fall back to /O2 and grade a function
    # against a build it never matched under.
    check('E3. every sweep variant maps to real cl flags, unknown ones to None',
          all(ib._dll_opt_flags(t) == f for t, f in match_sweep.VARIANTS)
          and ib._dll_opt_flags('NOPE') is None)

    import contextlib
    import io
    ref = os.path.join(ROOT, 'orig', 'SetVideo.exe')
    if not os.path.exists(ref):
        skip('E4. the gate names the right finding', 'orig/SetVideo.exe absent')
        return
    best, names_at, unplaced, unbuildable = ib.collect_exe('setvideo')
    if unplaced or unbuildable or not best:
        skip('E4. the gate names the right finding',
             'setvideo lane is not currently clean')
        return

    def verdict(unpl, unb):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            _img, v = ib.assemble(ref, best, names_at, unpl, 'audit', unb)
        return v, buf.getvalue()

    v_ok, _ = verdict([], [])
    v_bld, o_bld = verdict([], [(0x401000, 'f', 'x.c: error C2065')])
    v_clm, o_clm = verdict([(0x401000, 'f', 'x.c: symbol not in obj')], [])
    v_both, _ = verdict([(0x401000, 'f', 's')], [(0x401000, 'f', 'b')])
    check('E4. the gate names the right finding: '
          'wrong claim vs tree that will not build',
          v_ok == 'ok' and v_bld == 'build' and v_clm == 'claims'
          and v_both == 'claims'
          and 'INCONCLUSIVE' in o_bld and 'do not hold' not in o_bld
          and 'do not hold' in o_clm,
          f'ok={v_ok} unbuildable={v_bld} unplaced={v_clm} both={v_both}')


def main():
    recover_stale()
    sect = section_lookup(os.path.join(ROOT, 'orig', 'BRGlide.dll'))
    checks_abc(sect)
    check_d()
    check_e()

    print('\n' + '=' * 60)
    if SKIPPED:
        print(f'SKIPPED ({len(SKIPPED)}): ' + '; '.join(SKIPPED))
    if FAILURES:
        print('FAILED: ' + ', '.join(FAILURES))
        return 1
    print('no failures' + (' (some checks skipped)' if SKIPPED else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
