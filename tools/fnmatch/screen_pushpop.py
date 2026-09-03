#!/usr/bin/env python3
"""Screen the diff residue for the LEADING-CONSTANT-GROUP class.

The signature, proven on 0x1006DC30 BrMat3Skew: the recompile emits a
`push R` / `pop R` pair the original does not have, because a constant that
the original stores in one leading group is spelled interleaved in our C and
so stays live across the body, costing a callee-saved register.  The fix is
source-level -- hoist every store of that constant into one leading group,
DESCENDING by field offset -- so these rows are worth hand-solving even when
their raw diff count is large.

Reads whatever objects the last sweep left in build/match/obj_*/, exactly as
triage.py does; it compiles nothing.  A row for a file you have just edited
is only as fresh as that file's last sweep.

    python3 tools/fnmatch/screen_pushpop.py

YIELD, measured 2026-09-03 on the first run: 90 of 244 diff rows carry the
pair, but nearly all of them are rows whose recompile is hundreds of bytes
larger -- a spilled callee-saved register is a symptom of ANY size blow-up,
so the pair alone is not the class.  Read the ranked head, not the count:
only rows whose EXTRA multiset is close to just `push R` + `pop R` are
candidates.  The one clean head row (0x1001EC30) turned out to be a
different cause entirely, so treat this as triage, not a family.
"""
from __future__ import print_function
import sys, os, csv, re
from collections import Counter
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
R32 = r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b'
R16 = r'\b(ax|bx|cx|dx|si|di|bp)\b'
R8 = r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b'


def norm(t):
    t = re.sub(r'esp [+-] 0x[0-9a-f]+', 'esp+S', t)
    t = re.sub(r'0x[0-9a-f]+', 'I', t); t = re.sub(r'\b\d+\b', 'I', t)
    t = re.sub(R32, 'R', t); t = re.sub(R16, 'W', t); t = re.sub(R8, 'B', t)
    return t


def bag(code):
    c = Counter()
    for i in md.disasm(code, 0):
        c[norm('%s %s' % (i.mnemonic, i.op_str))] += 1
    return c


def find_obj(src):
    stem = os.path.splitext(os.path.basename(src))[0] + '.obj'
    best = None
    for d in sorted(os.listdir(os.path.join(ROOT, 'build', 'match'))):
        if not d.startswith('obj'):
            continue
        p = os.path.join(ROOT, 'build', 'match', d, stem)
        if os.path.exists(p) and (best is None or os.path.getmtime(p) > os.path.getmtime(best)):
            best = p
    return best


def main():
    hits = []
    cache = {}
    with open(os.path.join(ROOT, 'build', 'match', 'report.csv')) as f:
        rows = [r for r in csv.DictReader(f) if r['status'] == 'diff']
    for r in rows:
        obj = find_obj(r['file'])
        if not obj:
            continue
        if obj not in cache:
            try:
                cache[obj] = parse_coff_obj(obj)
            except Exception:
                cache[obj] = {}
        t = cache[obj]
        if r['name'] not in t:
            continue
        rc = t[r['name']][0]
        while rc and rc[-1] == 0x90:
            rc = rc[:-1]
        ob = os.path.join(ROOT, 'build', 'match', 'orig', r['va'] + '.bin')
        if not os.path.exists(ob):
            continue
        orig = open(ob, 'rb').read()
        O, R = bag(orig), bag(rc)
        extra, missing = R - O, O - R
        if extra.get('push R') and extra.get('pop R') and len(rc) > len(orig):
            hits.append((sum(extra.values()) + sum(missing.values()),
                         len(rc) - len(orig), r, extra, missing))
    hits.sort()
    print('%-26s %-12s %6s %6s %5s %5s' %
          ('symbol', 'va', 'origB', 'delta', 'xtra', 'miss'))
    for tot, delta, r, extra, missing in hits:
        print('%-26s %-12s %6s %+6d %5d %5d   %s'
              % (r['name'][:26], r['va'], r['orig_size'], delta,
                 sum(extra.values()), sum(missing.values()), r['file']))
        if tot <= 12:
            for k, v in extra.most_common(6):
                print('      +%2d %s' % (v, k))
            for k, v in missing.most_common(6):
                print('      -%2d %s' % (v, k))
    print('\n%d of %d diff rows carry the spurious push/pop pair' % (len(hits), len(rows)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
