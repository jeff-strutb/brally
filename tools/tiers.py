#!/usr/bin/env python3
"""Four-tier coverage for BRGlide.dll -- where every hand-C function stands.

    T1  still asm       no hand-written C in the tree yet (only raw decompiler
                        output). The real transcription has not started.
    T2  decomp'd (C)    transcribed C exists but still has real STRUCTURAL
                        differences from the original -- missing/extra/changed
                        instructions. Logic not yet proven equivalent.
    T3  codegen-only    transcribed C that compiled to the SAME instructions as
        difference      the original -- identical register-blind multiset, same
                        count -- differing only in register allocation and
                        scheduling. The strongest STATIC evidence of behavioural
                        equivalence, but NOT execution-verified.
    T4  byte-exact      diffs clean against the original bytes.

T3 is NOT a proof of "same inputs -> same outputs": that needs an execution
oracle (a differential harness running both versions), which does not exist for
arbitrary functions here. T3 is the static proxy -- an identical instruction
multiset modulo register naming -- and the threshold is printed so it is
auditable. Read it as "done but for register colouring", not "behaviourally
certified".

    python3 tools/tiers.py            # the four counts + bytes
    python3 tools/tiers.py --list T1  # dump the VAs in a tier

Needs fresh comparison objects for the T2/T3 split (run tools/match_sweep.py
first). Diffing functions with no measurable object fall to T2.
"""
import csv, os, re, sys, subprocess as sp

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'fnmatch'))
from triage import measure, _objs   # reuse the register-blind metric

# --- T3 threshold (auditable) ---------------------------------------------
# "codegen-only": the register-blind instruction multiset is IDENTICAL to the
# original (reg gap 0) and the instruction count matches. What is left can only
# be register allocation + scheduling. This is deliberately strict -- a single
# extra/absent instruction shape means real code differs, so it is T2.
REG_GAP_MAX = 0        # register-blind extra+missing instruction shapes
REG_FRAC_MAX = 0.0
COMPLETE_LO, COMPLETE_HI = 100, 100  # instruction count must equal the original


def _glide_map():
    m = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'config/functions_glide.csv'))):
        m[r['va'].upper()] = int(r['size']) if r['size'] else 0
    return m


def main():
    args = sys.argv[1:]
    mapped = _glide_map()
    fenced = set(r['va'].upper() for r in
                 csv.DictReader(open(os.path.join(ROOT, 'config/fenced.csv'))))
    target = {va: s for va, s in mapped.items() if va not in fenced}

    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    match, diff = [], []
    for r in csv.DictReader(open(rep)):
        if not r.get('orig_size'):
            continue
        (match if r['status'] == 'match' else diff).append(r)

    objs = _objs()
    t3, t2 = [], []
    for r in diff:
        m = measure(r['va'].lower(), r['name'], objs)
        complete = m and m['oi'] and COMPLETE_LO <= 100.0 * m['ri'] / m['oi'] <= COMPLETE_HI
        near = m and (m['reg'] <= REG_GAP_MAX or (m['oi'] and m['reg'] <= REG_FRAC_MAX * m['oi']))
        (t3 if (complete and near) else t2).append((r, m))

    # manifest of the T3 (codegen-only) VAs so the treemap can recolour them
    man = os.path.join(ROOT, 'build', 'match', 'tier3.csv')
    with open(man, 'w', newline='') as f:
        w = csv.writer(f); w.writerow(['va', 'bytes'])
        for r, _ in t3:
            w.writerow([r['va'], r['orig_size']])

    n_target = len(target)
    n_tr = len(match) + len(diff)          # transcribed = has @implements
    n_t1 = n_target - n_tr
    def by(rows): return sum(int(x['orig_size']) for x in rows)
    b_t4, b_diff = by(match), by(diff)
    b_t3 = sum(int(r['orig_size']) for r, _ in t3)
    b_t2 = b_diff - b_t3
    b_t1 = sum(target[va] for va in target) - (b_t4 + b_diff)

    if args and args[0] == '--list':
        pick = args[1].upper() if len(args) > 1 else 'T1'
        if pick == 'T3':
            for r, m in sorted(t3, key=lambda x: int(x[0]['orig_size'])):
                print(r['va'], r['orig_size'], r['name'], 'reggap', m['reg'])
        elif pick == 'T2':
            for r, m in sorted(t2, key=lambda x: -int(x[0]['orig_size'])):
                print(r['va'], r['orig_size'], r['name'],
                      ('reggap %d' % m['reg']) if m else 'unmeasured')
        return 0

    print("=" * 60)
    print(f"  BRGlide.dll hand-C target: {n_target} functions"
          f"   ({sum(target.values())} B of .text)")
    print("  " + "-" * 56)
    print(f"  T1  still asm (no hand-C yet)   {n_t1:5d} fns   {b_t1:8d} B")
    print(f"  T2  decomp'd, real diffs        {len(t2):5d} fns   {b_t2:8d} B")
    print(f"  T3  codegen-only diff (est.)    {len(t3):5d} fns   {b_t3:8d} B")
    print(f"  T4  byte-exact                  {len(match):5d} fns   {b_t4:8d} B")
    print("  " + "-" * 56)
    print(f"      transcribed (T2+T3+T4)      {n_tr:5d} fns")
    print(f"      done or done-bar-codegen    {len(match)+len(t3):5d} fns"
          f"   (T3+T4)")
    print("=" * 60)
    print(f"  T3 rule: size {COMPLETE_LO}-{COMPLETE_HI}% complete AND register-"
          f"blind gap <= {REG_GAP_MAX} or {REG_FRAC_MAX*100:.0f}% of insns.")
    print("  fenced (linker/EH-reproduced) sits outside this table; see")
    print("  config/fenced.csv / tools/coverage.py.")


if __name__ == '__main__':
    sys.exit(main())
