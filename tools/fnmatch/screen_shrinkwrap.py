#!/usr/bin/env python3
"""Screen the diff residue for the SHRINK-WRAPPED PROLOGUE class.

The signature, proven byte-exact on 0x1006BD70 BrSndBankMute (see
docs/VC5-IDIOMS.md, "An `&&` guard chain SHRINK-WRAPS the callee-saved
saves"): the original saves its callee-saved registers in the PROLOGUE, our
recompile saves them inside the guarded block, because the guard is spelled
as one `&&` chain wrapping the body instead of a run of early returns.  The
instruction MULTISET is identical -- both sides push and pop the same
registers the same number of times -- so triage scores the row 0 reggap and
files it as a colouring wall.  It is not one; it is a guard-shape defect and
the fix is a source rewrite.

The screen: same push/pop bag on both sides, but the FIRST callee-saved push
sits at a different instruction index, and the original's is inside the first
handful of instructions (a real prologue).

    python3 tools/fnmatch/screen_shrinkwrap.py

Rows it prints are candidates, not matches: confirm by looking for a
multi-condition guard at the top of the original before rewriting.

YIELD, first run 2026-09-03 over all 208 diff rows: TWO candidates, and
NEITHER is this class -- 0x10029D70 BrMat4Mul saves LATER in the original
than in our build (the opposite direction, and it has no guard at all), and
0x10015B10 BrTextEmitString is a 3,050-byte function whose recompile differs
by 2,294 bytes, so its save placement is a symptom of the size gap.  The
shrink-wrap defect is a SINGLETON in the tagged pool.  Re-run this after a
T1 intake batch, not against the worked-out tagged pool.
"""
from __future__ import print_function
import sys, os, csv, re
from collections import Counter
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
SAVED = ('esi', 'edi', 'ebx', 'ebp')


def insns(code):
    return [(i.mnemonic, i.op_str) for i in md.disasm(code, 0)]


def pushpop_bag(ins):
    c = Counter()
    for m, o in ins:
        if m in ('push', 'pop') and o in SAVED:
            c['%s %s' % (m, o)] += 1
    return c


def first_save(ins):
    for n, (m, o) in enumerate(ins):
        if m == 'push' and o in SAVED:
            return n
    return None


def find_obj(src):
    stem = os.path.splitext(os.path.basename(src))[0] + '.obj'
    best = None
    d0 = os.path.join(ROOT, 'build', 'match')
    for d in sorted(os.listdir(d0)):
        if not d.startswith('obj'):
            continue
        p = os.path.join(d0, d, stem)
        if os.path.exists(p) and (best is None or os.path.getmtime(p) > os.path.getmtime(best)):
            best = p
    return best


def main():
    cache = {}
    hits = []
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
        rec = cache[obj].get(r['name'])
        if rec is None:
            continue
        binp = os.path.join(ROOT, 'build', 'match', 'orig', r['va'].upper().replace('0X', '0x') + '.bin')
        if not os.path.exists(binp):
            continue
        o = insns(open(binp, 'rb').read())
        n = insns(rec if isinstance(rec, bytes) else rec[0])
        if not o or not n:
            continue
        bo, bn = pushpop_bag(o), pushpop_bag(n)
        if not bo or bo != bn:
            continue                      # different saves == a different defect
        fo, fn_ = first_save(o), first_save(n)
        if fo is None or fn_ is None or fo == fn_:
            continue
        if fo > 6:
            continue                      # the original's is not a prologue save
        hits.append((fn_ - fo, r['va'], r['name'], r['file'], fo, fn_,
                     int(r['orig_size']), int(r['diffs'])))
    hits.sort(key=lambda h: (-h[0], h[7]))
    print('%-12s %-30s %5s %5s %6s %6s  %s' %
          ('VA', 'name', 'origI', 'recI', 'origB', 'diffs', 'file'))
    for d, va, name, f, fo, fn_, ob, df in hits:
        print('%-12s %-30s %5d %5d %6d %6d  %s' % (va, name[:30], fo, fn_, ob, df, f))
    print('\n%d candidate row(s): same saves, different block.' % len(hits))


main()
