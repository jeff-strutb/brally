#!/usr/bin/env python3
"""Re-triage the diff residue by REGISTER-BLIND structural gap, not raw diffs.

A raw diff count is inflated by register-allocation naming: two objects can
share every instruction shape and still differ in most bytes.  The honest
question for a stuck function is how much of its gap SURVIVES normalising
every GP register to a single name.

    structural% = regnorm_gap / raw_gap

  ~0%    a true coloring wall: identical instruction multiset, different
         allocation.  Source permutation will not reach it.
  30-60% mostly structural.  There is real source work available, and the
         raw count that got it shelved was naming inflation.
  >80%   almost entirely structural.  If this was ever retired as "coloring",
         that was a misdiagnosis -- attack it.

Established 2026-08-28: 0x100250D0 read 1097+863 raw (a wall) but 432+198
register-blind, and fixing the structural defect took it from +1152 to +512
bytes.  See docs/idioms-A.md and docs/VC5-IDIOMS.md.

Ranks COMPLETE transcriptions first: a recomp far smaller than the original
is the known missing-code class (a factored helper the original inlined), a
different workstream, so those sort last.

CAVEAT: this reads whatever is in build/match/obj_*/, which is only as fresh
as the last sweep that touched each file.  A row whose numbers look wrong for
a function you just edited is a stale .obj -- re-sweep that one file.

    python3 tools/fnmatch/triage.py                  # whole diff residue, ranked
    python3 tools/fnmatch/triage.py 0x10030770       # one function
"""
from __future__ import print_function
import sys, re, os, csv
from collections import Counter
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
R32 = r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b'
R16 = r'\b(ax|bx|cx|dx|si|di|bp)\b'
R8  = r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b'


# ‼ RELOC'D OPERANDS MUST BE MASKED.  Until 2026-09-03 this file (and
# tools/fnmatch/fn.py) did not, while tools/msetdiff.py did, so the reggap
# this tool RANKS EVERY LANE BY was inflated on any function that touches a
# global absolutely.  An unlinked .obj keeps the ADDEND in the reloc'd field
# and the symbol in the relocation -- capstone prints `[edx*4]` or `push 0` --
# while the LINKED original prints the whole absolute.  Unmasked, the two
# never pair and every such instruction counts twice, once MISSING and once
# EXTRA.  Measured the day it was fixed: 0x1000A110's reggap fell 48 -> 18
# and 0x1000EAF0's 86 -> 38, while 0x100250D0 (which indexes no global array)
# did not move at all -- so the error is NOT a uniform scale factor and every
# reggap COMPARISON BETWEEN functions made before that date is unsafe,
# including the parked/SHAPE verdicts below.  Keep this in step with
# msetdiff.norm and fn.norm.
BRANCH = re.compile(r'^(j[a-z]+|call|loop[a-z]*)$')


def _norm(t, m, relocd=False):
    if BRANCH.match(t.split(' ', 1)[0]) and re.fullmatch(
            r'0x[0-9a-f]+', t.split(' ', 1)[1] if ' ' in t else ''):
        return t.split(' ', 1)[0] + ' T'
    t = re.sub(r'esp [+-] 0x[0-9a-f]+', 'esp+S', t)
    t = re.sub(r'\*(1|2|4|8)\b', '*K', t)
    if m != 'raw':
        t = re.sub(R32, 'R', t); t = re.sub(R16, 'W', t); t = re.sub(R8, 'B', t)
    if relocd or re.search(r'0x1[0-9a-f]{7}', t):
        t = re.sub(r'0x[0-9a-f]+', 'A', t)
        t = re.sub(r'\[(\d+)\]', '[A]', t)
        t = re.sub(r'\[([A-Za-z]+\*K) \+ \d+\]', r'[\1 + A]', t)
        if 'A' not in t:
            if re.search(r',\s*0$', t):
                t = re.sub(r',\s*0$', ', A', t)
            elif '[' not in t and re.search(r'\s0$', t):
                t = re.sub(r'\s0$', ' A', t)
            elif '[' in t:
                def _abs(mo):
                    inner = re.sub(r'\s*\+\s*(0x[0-9a-f]+|\d+)$', '', mo.group(1))
                    return '[' + inner + ' + A]'
                t = re.sub(r'\[([^]]*)\]', _abs, t, count=1)
    t = re.sub(r'0x[0-9a-f]+', 'I', t); t = re.sub(r'\b\d+\b', 'I', t)
    return t


def _bag(code, m, relocs=frozenset()):
    c = Counter()
    for i in md.disasm(code, 0):
        rd = any(o in relocs for o in range(i.address, i.address + i.size))
        c[_norm('%s %s' % (i.mnemonic, i.op_str), m, rd)] += 1
    return c


def _strip_pad(b):
    while b and b[-1] == 0x90:
        b = b[:-1]
    return b


def _objs():
    """symbol -> code bytes, across every compiled variant dir."""
    out = {}
    for d in ('obj_O2', 'obj_O2y', 'obj_Od'):
        p = os.path.join(ROOT, 'build', 'match', d)
        if not os.path.isdir(p):
            continue
        for f in os.listdir(p):
            if not f.endswith('.obj'):
                continue
            try:
                t = parse_coff_obj(os.path.join(p, f))
            except Exception:
                continue
            for sym, v in t.items():
                out.setdefault(sym, (_strip_pad(v[0]), set(v[1])))
    return out


def measure(va, sym, objs):
    ob = os.path.join(ROOT, 'build', 'match', 'orig', va + '.bin')
    if not os.path.exists(ob) or sym not in objs:
        return None
    orig = open(ob, 'rb').read(); rc, rel = objs[sym]
    oi = len(list(md.disasm(orig, 0))); ri = len(list(md.disasm(rc, 0)))
    res = {}
    for m in ('raw', 'regnorm'):
        O, R = _bag(orig, m), _bag(rc, m, rel)
        res[m] = (sum((R - O).values()), sum((O - R).values()))
    raw, reg = sum(res['raw']), sum(res['regnorm'])
    return dict(va=va, sym=sym, ob=len(orig), rb=len(rc), oi=oi, ri=ri,
                raw=raw, reg=reg, pct=(100.0 * reg / raw) if raw else 0.0,
                rx=res['regnorm'][0], rm=res['regnorm'][1])


_EH_CACHE = {}


def _has_eh_frame(va):
    """True when the ORIGINAL bytes open with an SEH/C++ exception frame.

    `push -1` (6A FF) is the first instruction of MSVC's __try prologue, and
    it is unreachable from C source -- see the EH screen in
    docs/STRUCTURAL-PLAYBOOK.md and the cxx-eh-frame-wall note.
    """
    if va not in _EH_CACHE:
        p = os.path.join(ROOT, 'build', 'match', 'orig', '%s.bin' % va)
        try:
            with open(p, 'rb') as f:
                _EH_CACHE[va] = f.read(2) == b'\x6a\xff'
        except IOError:
            _EH_CACHE[va] = False
    return _EH_CACHE[va]


def main():
    want = set(a.lower() for a in sys.argv[1:])
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    # A function already byte-exact in the C++ or EXE workstream still sits as
    # status=diff here -- 95 of them as of 2026-09-03 -- so ranking report.csv
    # alone both inflates the denominator and hands closers work that is
    # already done. claim_lane.py has always filtered these; the ranking it
    # reads did not. (These C rows are superseded twins and want untagging.)
    done = set()
    for alt in ('report_cpp.csv', 'report_exe.csv'):
        p = os.path.join(ROOT, 'build', 'match', alt)
        if not os.path.exists(p):
            continue
        with open(p) as f:
            done.update((r.get('va') or '').lower()
                        for r in csv.DictReader(f) if r.get('status') == 'match')
    rows = []
    with open(rep) as f:
        for r in csv.DictReader(f):
            if r.get('status') != 'diff':
                continue
            if r['va'].lower() in done:
                continue
            if want and r['va'].lower() not in want and r['name'].lower() not in want:
                continue
            rows.append(r)
    objs = _objs()
    out = [m for m in (measure(r['va'], r['name'], objs) for r in rows) if m]
    # rank by structural gap among functions that are actually COMPLETE --
    # missing-code functions sort last, they are a different workstream.
    out.sort(key=lambda m: (m['ri'] < 0.8 * m['oi'], -m['reg']))
    print('%-24s %-11s %6s %5s %6s %7s %7s %6s  %s' %
          ('symbol', 'va', 'origB', 'cmpl', 'insnD', 'rawgap', 'reggap',
           'struct', 'verdict'))
    ranks = []
    for m in out:
        # completeness first: a recomp far SMALLER than the original is the
        # known missing-code class (a factored helper the original inlined),
        # not a shape problem -- see the inlined-helper-match-class note.
        c = 100.0 * m['ri'] / m['oi'] if m['oi'] else 0.0
        if _has_eh_frame(m['va']):
            # The original opens `push -1 / push <handler> / fs:[0]`: a C++ or
            # SEH exception frame, which C cannot emit at all.  These belong to
            # the separate C++ EH workstream, and without this test they read
            # as the most attractive rows on the board -- 41 of them, 37,677
            # bytes as of 2026-09-03, most showing up as
            # "MISSING CODE (2% complete)" because the port stands them in
            # with a forwarder.  Park-tier, below even the coloring walls.
            v = 'C++ EH FRAME - not reachable from C'
            score = 2000000 + m['reg']
        elif c < 80:
            v = 'MISSING CODE (%.0f%% complete)' % c
            score = 100000 + m['reg']
        elif m['pct'] < 15:
            v = 'coloring wall - real'
            score = 1000000 + m['reg']          # park-tier: claimed last
        elif m['pct'] < 70:
            v = 'mixed'
            score = 10000 + m['reg']
        else:
            v = 'SHAPE - best targets'
            score = m['reg']                    # small real work first
        ranks.append((m['va'], score, v))
        print('%-24s %-11s %6d %4.0f%% %+6d %7d %7d %5.0f%%  %s' %
              (m['sym'][:24], m['va'], m['ob'], c, m['ri'] - m['oi'],
               m['raw'], m['reg'], m['pct'], v))
    print('\n%d functions measured' % len(out))
    if not want:
        # publish the lane-priority ranking for tools/claim_lane.py: lower
        # score = claim first (SHAPE < mixed < missing-code < coloring wall).
        rank_csv = os.path.join(ROOT, 'build', 'match', 'triage_rank.csv')
        with open(rank_csv, 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(['va', 'score', 'verdict'])
            for va, score, v in ranks:
                w.writerow([va, score, v])
        print('lane ranking -> %s' % os.path.relpath(rank_csv, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
