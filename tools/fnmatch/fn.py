#!/usr/bin/env python3
"""Per-function compile + scorecard for ANY function in report.csv.

Generalises the 0x100250D0 sandbox: looks the function up by VA or name,
compiles the .c file it lives in (or a variant copy of that file), and scores
the named symbol against the extracted original bytes.

    python3 tools/fnmatch/fn.py 0x10006510              # score as the tree has it
    python3 tools/fnmatch/fn.py 0x10006510 --detail regnorm 30
    python3 tools/fnmatch/fn.py 0x10006510 --make w1    # private variant copy
    python3 tools/fnmatch/fn.py 0x10006510 --var w1     # score that copy

Variants live in build/match/t3d/fn_<VA>_<tag>.c, so several probers can work
the same function at once without contending (see tools/fnmatch/README.md).
Read the REGNORM number, not RAW -- see the README for why.
"""
from __future__ import print_function
import argparse, csv, os, re, shutil, subprocess, sys
from collections import Counter
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
R32=r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b'; R16=r'\b(ax|bx|cx|dx|si|di|bp)\b'
R8=r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b'
VARDIR = os.path.join(ROOT, 'build', 'match', 't3d')


def row(key):
    k = key.lower()
    with open(os.path.join(ROOT, 'build', 'match', 'report.csv')) as f:
        for r in csv.DictReader(f):
            if r['va'].lower() == k or r['name'].lower() == k:
                return r
    sys.exit('no report.csv row for %r' % key)


def compile_file(src, tag):
    objdir = os.path.join(ROOT, 'build', 'match', 'obj_' + tag)
    os.makedirs(objdir, exist_ok=True)
    obj = os.path.join(objdir, os.path.splitext(os.path.basename(src))[0] + '.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    cmd = ['sh', os.path.join(ROOT, 'tools', 'wine.sh'),
           os.path.join(ROOT, 'tools', 'msvc5', 'bin', 'cl.exe'), '/nologo',
           '/O2', '/W3', '/I', 'include', '/I', 'tools/msvc5-compat',
           '/I', 'tools/msvc5/include', '/DBR_MATCHING_BUILD', '/c',
           os.path.relpath(src, ROOT),
           '/Fo' + os.path.relpath(obj, ROOT).replace('/', '\\')]
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180)
    if not os.path.exists(obj):
        errs = [l.strip() for l in (p.stdout + p.stderr).splitlines()
                if 'error' in l.lower()]
        return None, errs[:5] or ['no obj, no diagnostic']
    return obj, []


def norm(t, m):
    t = re.sub(r'esp [+-] 0x[0-9a-f]+', 'esp+S', t)
    t = re.sub(r'0x[0-9a-f]+', 'I', t); t = re.sub(r'\b\d+\b', 'I', t)
    if m != 'raw':
        t = re.sub(R32, 'R', t); t = re.sub(R16, 'W', t); t = re.sub(R8, 'B', t)
    return t


def bag(code, m):
    c = Counter()
    for i in md.disasm(code, 0):
        c[norm('%s %s' % (i.mnemonic, i.op_str), m)] += 1
    return c


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('key', help='function VA (0x...) or name')
    ap.add_argument('--make', metavar='TAG', help='create a private variant copy')
    ap.add_argument('--var', metavar='TAG', help='score a variant copy')
    ap.add_argument('--detail', nargs='*', metavar='ARG',
                    help='raw|regnorm [N] -- print the multiset difference')
    a = ap.parse_args()
    r = row(a.key)
    va, name, src = r['va'], r['name'], os.path.join(ROOT, r['file'])
    vpath = os.path.join(VARDIR, 'fn_%s_%s.c' % (va, a.make or a.var or ''))
    if a.make:
        os.makedirs(VARDIR, exist_ok=True); shutil.copy(src, vpath)
        print('made %s  (from %s, fn %s)' % (os.path.relpath(vpath, ROOT), r['file'], name))
        return 0
    use, tag = (vpath, 'fn_' + a.var) if a.var else (src, 'fnbase')
    obj, errs = compile_file(use, tag)
    if obj is None:
        print('COMPILE FAILED'); [print('  ' + e) for e in errs]; return 1
    t = parse_coff_obj(obj)
    if name not in t:
        print('symbol %s not in %s (have: %s)' % (name, obj, ', '.join(list(t)[:8])))
        return 1
    rc, relocs = t[name][0], t[name][1]
    while rc and rc[-1] == 0x90:
        rc = rc[:-1]
    ob = os.path.join(ROOT, 'build', 'match', 'orig', va + '.bin')
    orig = open(ob, 'rb').read()
    # Relocation slots are zero in an unlinked .obj and are patched at link
    # time, so they ALWAYS differ from the original and must be masked -- the
    # sweep's own scorer does this (match_sweep.score).  Without it a genuinely
    # byte-exact function reads as hundreds of differing bytes.
    fd = next((i for i in range(min(len(orig), len(rc)))
               if i not in relocs and orig[i] != rc[i]),
              min(len(orig), len(rc)))
    ndiff = sum(1 for i in range(min(len(orig), len(rc)))
                if i not in relocs and orig[i] != rc[i])
    oi = len(list(md.disasm(orig, 0))); ri = len(list(md.disasm(rc, 0)))
    res = {}
    for m in ('raw', 'regnorm'):
        O, R = bag(orig, m), bag(rc, m)
        res[m] = (sum((R - O).values()), sum((O - R).values()))
    ident = (len(rc) >= len(orig) and ndiff == 0)
    print('%s %s  [%s]' % (va, name, r['file']))
    print('  BYTES orig=%d recomp=%d (%+d)   INSNS orig=%d recomp=%d (%+d)'
          % (len(orig), len(rc), len(rc) - len(orig), oi, ri, ri - oi))
    # ‼ DIFFS is a POSITIONAL compare -- byte i against byte i, relocs masked,
    # no alignment.  Any size difference shifts everything after it, so the
    # number is dominated by that shift and is NOT comparable across builds of
    # different sizes.  (Measured on 0x1000EAF0: a two-byte change read as a
    # 47% DIFFS drop, because a 4,600-byte tail moved from delta -2 to 0.)
    # Say so, rather than let it be read as a distance-to-exact.
    skew = '' if len(rc) == len(orig) else '  <-- SIZE DIFFERS: positional, compare only same-size builds'
    print('  FIRSTDIV=+0x%x  DIFFS=%d (reloc-masked)%s  RAW %d+%d  REGNORM %d+%d%s'
          % (fd, ndiff, skew, res['raw'][0], res['raw'][1], res['regnorm'][0],
             res['regnorm'][1], '   *** BYTE-EXACT ***' if ident else ''))
    if a.detail is not None:
        m = a.detail[0] if a.detail else 'regnorm'
        n = int(a.detail[1]) if len(a.detail) > 1 else 25
        O, R = bag(orig, m), bag(rc, m)
        print('  --- recomp EXTRA ---')
        for k, v in (R - O).most_common(n): print('   +%3d  %s' % (v, k))
        print('  --- recomp MISSING ---')
        for k, v in (O - R).most_common(n): print('   -%3d  %s' % (v, k))
    return 0


if __name__ == '__main__':
    sys.exit(main())
