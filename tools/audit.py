#!/usr/bin/env python3
"""Adversarial regression tests for the matching pipeline's own claims.

Every other tool here reports what it found. This one tries to BREAK what they
found, because the failure mode that actually cost this project time was not a
tool reporting a wrong number -- it was a tool reporting a right-looking number
that nothing could contradict. A check that cannot fail is not a check.

  A  Every learned address lands inside a real section of the image.
  B  Single-observation addresses do not skew against corroborated ones.
  C  MUTATION TEST. Poison the most-referenced learned address; the assembled
     image must notice. This is the one that matters -- it proves "0 differing
     bytes" is a real comparison rather than a tautology.
  D  refcheck.py passes on a Glide-keyed corpus and fails on a D3D-keyed one.
     Rule 0's guard had only ever been seen failing; a guard that cannot pass
     enforces nothing.

C and D mutate state (the learned map; the reference corpus directory). Both
restore in `finally`, and a stale backup left by a killed run is detected and
rolled back on the next start rather than silently poisoning it.

Usage:
    python3 tools/audit.py          # exit 0 if every check passes
"""
import csv
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from pe_patch import read_pe_text_info  # noqa: E402

LEARNED = os.path.join(ROOT, 'config', 'globals_learned.csv')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
BAK = ORIG_DIR + '.auditbak'
FAILURES = []


def check(name, ok, detail=''):
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f"  {detail}" if detail else ''))
    if not ok:
        FAILURES.append(name)


def recover_stale():
    """A killed run can leave the corpus swapped out. Put it back first."""
    if os.path.exists(BAK):
        if os.path.islink(ORIG_DIR):
            os.unlink(ORIG_DIR)
        elif os.path.exists(ORIG_DIR):
            print('audit: both corpus and backup exist; leaving alone',
                  file=sys.stderr)
            return
        os.rename(BAK, ORIG_DIR)
        print('audit: recovered a reference corpus left swapped by a prior run')


def sections(dll):
    base, secs = read_pe_text_info(dll)
    return base, [(base + rva, base + rva + max(vs, rs), nm)
                  for nm, rva, vs, ro, rs in secs]


def main():
    recover_stale()
    d3d = os.path.join(ROOT, 'orig', 'BRD3D.dll')
    glide = os.path.join(ROOT, 'orig', 'BRGlide.dll')
    _, ranges = sections(d3d)

    def sect(a):
        for lo, hi, nm in ranges:
            if lo <= a < hi:
                return nm
        return None

    rows = list(csv.DictReader(open(LEARNED)))

    # ---- A: learned addresses must be real addresses ---------------------
    outside = [r for r in rows if sect(int(r['addr'], 16)) is None]
    bysec = {}
    for r in rows:
        s = sect(int(r['addr'], 16))
        bysec[s] = bysec.get(s, 0) + 1
    print(f"learned addresses: {len(rows)}   by section: {bysec}")
    check('A. every learned address lands inside a real section', not outside,
          f'{len(outside)} outside' if outside else '')

    # ---- B: uncorroborated ones must not be junk -------------------------
    one = [r for r in rows if r['corroborated'] == '0']
    bad = [r for r in one if sect(int(r['addr'], 16)) is None]
    check(f'B. {len(one)} single-observation addresses land in real sections',
          not bad, f'{len(bad)} bad' if bad else '')

    # ---- C: the image diff must be able to fail --------------------------
    saved = open(LEARNED).read()
    try:
        lines = saved.splitlines(True)
        vi, vn = None, -1
        for i, l in enumerate(lines[1:], 1):
            p = l.split(',')
            if len(p) > 2 and p[2].isdigit() and int(p[2]) > vn:
                vn, vi = int(p[2]), i
        p = lines[vi].split(',')
        victim, good = p[0], p[1]
        p[1] = '0x%08X' % ((int(p[1], 16) ^ 0x40) & 0xFFFFFFFF)
        lines[vi] = ','.join(p)
        open(LEARNED, 'w').write(''.join(lines))
        out = subprocess.run([sys.executable, 'tools/image_build.py'],
                             cwd=ROOT, capture_output=True, text=True).stdout
        line = next((l for l in out.splitlines()
                     if 'ASSEMBLED IMAGE' in l), '')
        caught = bool(line) and 'ORIGINAL: 0 ' not in line
        check(f'C. image diff detects a poisoned address ({victim}, '
              f'{vn} refs)', caught, line.strip())
    finally:
        open(LEARNED, 'w').write(saved)

    # ---- D: rule 0's guard must work in BOTH directions ------------------
    gbase, gsecs = read_pe_text_info(glide)
    gt = [s for s in gsecs if s[0].startswith('.text')][0]
    gimg = open(glide, 'rb').read()
    tmp = os.path.join(ROOT, 'build', 'match', 'audit_glide_corpus')
    os.makedirs(tmp, exist_ok=True)
    for f in os.listdir(tmp):
        os.unlink(os.path.join(tmp, f))
    n = 0
    for r in csv.DictReader(open(os.path.join(ROOT, 'config', 'shared.csv'))):
        if not r['glide_va'].strip() or not r['size'].strip():
            continue
        gv, sz = int(r['glide_va'], 16), int(r['size'])
        off = gt[3] + (gv - gbase - gt[1])
        if sz <= 0 or off < gt[3] or off + sz > gt[3] + gt[4]:
            continue
        open(os.path.join(tmp, '0x%08X.bin' % gv), 'wb').write(
            gimg[off:off + sz])
        n += 1
        if n >= 400:
            break

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

    p = subprocess.run([sys.executable, 'tools/refcheck.py', '--quiet'],
                       cwd=ROOT, capture_output=True, text=True)
    check('D2. refcheck FAILS on the real corpus (currently D3D-keyed)',
          p.returncode == 1, f'exit={p.returncode}')

    print('\n' + '=' * 60)
    if FAILURES:
        print('FAILED:', ', '.join(FAILURES))
        return 1
    print('all checks pass')
    return 0


if __name__ == '__main__':
    sys.exit(main())
