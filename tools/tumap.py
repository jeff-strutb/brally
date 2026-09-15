#!/usr/bin/env python3
"""Original translation-unit oracle for BRGlide.dll.

MSVC 5.0 emits each floating-point literal ONCE PER TRANSLATION UNIT and does
not fold those copies across units at link time: `.rdata` of orig/BRGlide.dll
holds 28 aligned copies of 1.0f and 18 of 0.5f.  So two functions that load
the SAME `.rdata` float address were compiled in the SAME original TU.  A
union-find over every function's x87 constant references recovers the original
TU partition -- the missing half of the x87 operand-role wall (the roles are
decided by the TU's PRECEDING function definitions; this says who they were).

    .venv/bin/python tools/tumap.py            # build config/tu_map.csv + gate 1
    .venv/bin/python tools/tumap.py --groups   # ranked multi-member TU table
    .venv/bin/python tools/tumap.py --validate  # gate 2: precision on --corpus ext/ext2

Output config/tu_map.csv: tu_id, va, order, flag, tier, size, file, evidence.
One row per function that references an .rdata float (the oracle is silent for
integer-only TUs -- that is a second, un-built signal, plan Phase 5 backlog).

The lever is only for the straight-line x87 class the 09-13 micro-TU lab
measured; role-insensitive functions match wherever they are filed, so an
all-T4 group moving files proves nothing.  See docs and
[[tu-constant-pool-oracle-2026-09-15]].
"""
import argparse, csv, json, os, sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import pe  # noqa: E402
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, x86_const  # noqa: E402

_md = Cs(CS_ARCH_X86, CS_MODE_32)
_md.detail = True
_md.skipdata = True


def _csv(path):
    p = os.path.join(ROOT, path)
    return list(csv.DictReader(open(p))) if os.path.exists(p) else []


def rdata_range(pe_path):
    p = pe.load(pe_path)
    s = p.section_by_name('.rdata')
    lo = p.image_base + s.vaddr
    return lo, lo + s.vsize


# --- union-find --------------------------------------------------------------
class UF:
    def __init__(self):
        self.p = {}

    def find(self, x):
        self.p.setdefault(x, x)
        r = x
        while self.p[r] != r:
            r = self.p[r]
        while self.p[x] != r:
            self.p[x], x = r, self.p[x]
        return r

    def union(self, a, b):
        self.p[self.find(a)] = self.find(b)


def fp_consts_of(code, va, lo, hi):
    """The set of .rdata float addresses this function's x87 ops reference."""
    out = set()
    for ins in _md.disasm(code, va):
        if not ins.mnemonic.startswith('f'):
            continue
        for op in ins.operands:
            if op.type == x86_const.X86_OP_MEM:
                m = op.mem
                if m.base == 0 and m.index == 0:
                    d = m.disp & 0xffffffff
                    if lo <= d < hi:
                        out.add(d)
    return out


# --- tier classification (mirrors tools/tiers.py, CLAUDE.md rule 12) ---------
def tier_map():
    """va(int) -> ('T1'|'T2'|'T3'|'T4'|'cpp'|'fenced', size, file)."""
    glide = {int(r['va'], 16): int(r['size']) if r['size'] else 0
             for r in _csv('config/functions_glide.csv')}
    fenced = set(int(r['va'], 16) for r in _csv('config/fenced.csv'))
    rep = {int(r['va'], 16): r for r in _csv('build/match/report.csv')
           if r.get('orig_size')}
    cpp = {int(r['va'], 16): r for r in _csv('build/match/report_cpp.csv')
           if r.get('va')}
    try:
        from t3 import certified
        cert = set(int(va, 16) for va, i in certified().items()
                   if not va.startswith('?') and i['ok'])
    except Exception:
        cert = set()

    out = {}
    for va, sz in glide.items():
        f = rep.get(va, {}).get('file') or cpp.get(va, {}).get('file') or ''
        if va in fenced:
            out[va] = ('fenced', sz, f)
        elif va in cert:
            out[va] = ('T3', sz, f)
        elif va in rep:
            out[va] = ('T4' if rep[va]['status'] == 'match' else 'T2', sz, f)
        elif va in cpp:
            out[va] = ('cpp', sz, f)
        else:
            out[va] = ('T1', sz, f)
    return out, rep


def build(pe_path, coalesce=True):
    lo, hi = rdata_range(pe_path)
    tiers, rep = tier_map()
    uf = UF()
    fn_consts = {}
    addr_fns = defaultdict(list)
    for va in sorted(tiers):
        p = os.path.join(ROOT, 'build/match/orig', '0x%08X.bin' % va)
        if not os.path.exists(p):
            continue
        cs = fp_consts_of(open(p, 'rb').read(), va, lo, hi)
        if not cs:
            continue
        fn_consts[va] = cs
        uf.find(va)
        for c in cs:
            addr_fns[c].append(va)
    for c, vs in addr_fns.items():
        for v in vs[1:]:
            uf.union(vs[0], v)

    raw = defaultdict(list)
    for va in fn_consts:
        raw[uf.find(va)].append(va)
    groups = coalesce_spans(raw) if coalesce else raw
    return groups, fn_consts, tiers, rep, (lo, hi), raw


def coalesce_spans(raw):
    """Merge FP-const components whose VA spans interleave into one TU.

    The linker keeps a translation unit's COMDATs contiguous, so two const
    components that interleave in VA (A .. B .. A .. B) were emitted together
    and are the same original TU; the sparse literal graph split them only
    because they share no single copied constant.  Gate 2 proved the seed
    merges never cross a source file, so widening a component to its own VA
    span cannot pull in a foreign TU that is not already interleaved with it.
    """
    comps = sorted(([min(m), max(m), list(m)] for m in raw.values()),
                   key=lambda x: x[0])
    out = []
    for lo, hi, members in comps:
        if out and lo <= out[-1][1]:            # span intersects the open TU
            out[-1][1] = max(out[-1][1], hi)
            out[-1][2].extend(members)
        else:
            out.append([lo, hi, members])
    return {lo: members for lo, hi, members in out}


OPEN = ('T1', 'T2')


def group_stats(members, tiers):
    by = defaultdict(int)
    openb = 0
    files = set()
    for va in members:
        t, sz, f = tiers[va]
        by[t] += 1
        if f:
            files.add(f)
        if t in OPEN:
            openb += sz
    return by, openb, files


def group_flag(members, rep):
    """Majority opt of the group's T4 (match) rows; else of any row present."""
    def majority(rows):
        c = defaultdict(int)
        for r in rows:
            c[r.get('opt') or ''] += 1
        return max(c, key=c.get) if c else ''
    present = [rep[va] for va in members if va in rep]
    t4 = [r for r in present if r['status'] == 'match']
    return majority(t4) or majority(present)


def write_map(groups, fn_consts, tiers, rep, out_path):
    ordered = sorted(groups.values(), key=lambda g: min(g))
    rows = []
    for gi, members in enumerate(ordered):
        members = sorted(members)
        tu_id = 'tu_%03d' % gi
        flag = group_flag(members, rep)
        for order, va in enumerate(members):
            t, sz, f = tiers[va]
            ev = ' '.join('0x%08X' % c for c in sorted(fn_consts[va]))
            rows.append({'tu_id': tu_id, 'va': '0x%08X' % va, 'order': order,
                         'flag': flag, 'tier': t, 'size': sz, 'file': f,
                         'evidence': ev})
    with open(out_path, 'w', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=['tu_id', 'va', 'order', 'flag',
                                           'tier', 'size', 'file', 'evidence'],
                           lineterminator='\n')
        w.writeheader()
        w.writerows(rows)
    return len(rows)


def gate1_disjoint(groups):
    """After coalescing, no two TU spans may overlap.  Returns overlap count."""
    spans = sorted((min(m), max(m)) for m in groups.values())
    bad = sum(1 for a, b in zip(spans, spans[1:]) if b[0] <= a[1])
    return bad


def cmd_default(args):
    groups, fn_consts, tiers, rep, _, raw = build(args.pe)
    out = os.path.join(ROOT, 'config/tu_map.csv')
    n = write_map(groups, fn_consts, tiers, rep, out)
    multi = [g for g in groups.values() if len(g) >= 2]
    print('functions referencing an .rdata FP const: %d' % len(fn_consts))
    print('raw FP-const components: %d   coalesced TUs: %d   multi-member: %d'
          % (len(raw), len(groups), len(multi)))
    print('wrote %s (%d rows)' % (os.path.relpath(out, ROOT), n))
    print('\n-- gate 1 (coalesced TU spans are disjoint) --')
    bad = gate1_disjoint(groups)
    merged = len(raw) - len(groups)
    print('  %d raw components coalesced by VA-span interleave into %d TUs'
          % (merged, len(groups)))
    print('  %s: %d overlapping TU spans remain'
          % ('PASS' if bad == 0 else 'FAIL', bad))
    return 0


def cmd_groups(args):
    groups, fn_consts, tiers, rep, _, raw = build(args.pe)
    multi = [sorted(g) for g in groups.values() if len(g) >= 2]
    multi.sort(key=lambda g: min(g))
    print('%-12s %4s %3s %3s %3s %3s %8s %5s %-8s' %
          ('first VA', 'fns', 'T4', 'T3', 'T2', 'T1', 'open B', 'files', 'flag'))
    for members in multi:
        by, openb, files = group_stats(members, tiers)
        print('0x%08X %4d %3d %3d %3d %3d %8d %5d %-8s' %
              (min(members), len(members), by['T4'], by['T3'], by['T2'],
               by['T1'], openb, len(files), group_flag(members, rep)))
    tot_open = sum(group_stats(m, tiers)[1] for m in multi)
    print('\n%d multi-member groups, %d B open (T1+T2) inside them' %
          (len(multi), tot_open))
    return 0


def cmd_lane(args):
    """Phase-2 picker: multi-member TUs that still have open bytes, cheapest
    first (fewest open bytes, most T4 pool-mates to restore the TU state), with
    the per-TU move list.  This is the intake that replaces the dead Pool B."""
    groups, fn_consts, tiers, rep, _, raw = build(args.pe)
    cand = []
    for members in groups.values():
        members = sorted(members)
        by, openb, files = group_stats(members, tiers)
        # The C++ lane is Phase 5; its templated COMDATs interleave many real
        # TUs in VA, so span-coalesce over-merges them (the 0x100393C0 blob,
        # 39 files).  A real x87 TU is a handful of our .c files.
        if by['cpp'] or len(files) > 12:
            continue
        if openb > 0 and len(members) >= 2 and by['T4'] >= 1:
            cand.append((openb, -by['T4'], members, by, files))
    cand.sort()
    for openb, negt4, members, by, files in cand:
        print('TU 0x%08X  open %d B  (T4 %d / T3 %d / T2 %d / T1 %d)  %d files  flag %s'
              % (min(members), openb, by['T4'], by['T3'], by['T2'], by['T1'],
                 len(files), group_flag(members, rep)))
        for va in members:
            t, sz, f = tiers[va]
            mark = '  <- OPEN' if t in OPEN else ''
            print('    0x%08X %-4s %5d B  %s%s' % (va, t, sz, f or '(no file)', mark))
        print()
    return 0


def cmd_validate(args):
    """Gate 2: replay the union-find on a corpus that HAS source, and score
    precision against source-file truth (one .c file == one TU)."""
    corp = args.corpus
    idx = os.path.join(ROOT, 'build/match', 'corpus_%s_index.json' % corp)
    recipe = os.path.join(ROOT, 'build/match', corp, 'recipe.json')
    exe = None
    if os.path.exists(recipe):
        exe = json.load(open(recipe)).get('exe')
    if not os.path.exists(idx) or not exe or not os.path.exists(os.path.join(ROOT, exe)):
        print('gate 2 NOT RUN: corpus %s index or image missing' % corp)
        return 2
    lo, hi = rdata_range(os.path.join(ROOT, exe))
    import re
    memref = re.compile(r'^f\w*\s.*\[0x([0-9a-fA-F]+)\]')
    fns = json.load(open(idx))['fns']
    truth = {}     # va -> source file
    fn_consts = {}
    uf = UF()
    addr_fns = defaultdict(list)
    for fn in fns:
        va = int(fn['va'], 16)
        truth[va] = fn.get('file') or '?'
        cs = set()
        for tok in fn.get('toks', []):
            m = memref.match(tok)
            if m:
                d = int(m.group(1), 16)
                if lo <= d < hi:
                    cs.add(d)
        if not cs:
            continue
        fn_consts[va] = cs
        uf.find(va)
        for c in cs:
            addr_fns[c].append(va)
    for c, vs in addr_fns.items():
        for v in vs[1:]:
            uf.union(vs[0], v)
    groups = defaultdict(list)
    for va in fn_consts:
        groups[uf.find(va)].append(va)
    multi = [g for g in groups.values() if len(g) >= 2]
    pairs = same = 0
    pure = 0
    for g in multi:
        ok = True
        for i in range(len(g)):
            for j in range(i + 1, len(g)):
                pairs += 1
                if truth[g[i]] == truth[g[j]]:
                    same += 1
                else:
                    ok = False
        if ok:
            pure += 1
    print('corpus %s: %d fns ref an .rdata float, %d groups, %d multi-member'
          % (corp, len(fn_consts), len(groups), len(multi)))
    if not pairs:
        print('  INCONCLUSIVE: no multi-member groups to score in this corpus')
        return 2
    prec = 100.0 * same / pairs
    print('  pure (all-one-source) multi-member groups: %d / %d' % (pure, len(multi)))
    print('  pair precision on multi-member groups: %.1f%% (%d/%d)'
          % (prec, same, pairs))
    print('  GATE 2 %s (threshold 95%%)' % ('PASS' if prec >= 95.0 else 'FAIL'))
    return 0 if prec >= 95.0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pe', default=os.path.join(ROOT, 'orig/BRGlide.dll'))
    ap.add_argument('--groups', action='store_true', help='ranked multi-member table')
    ap.add_argument('--lane', action='store_true', help='Phase-2 picker: open TUs, cheapest first, with move lists')
    ap.add_argument('--validate', action='store_true', help='gate 2 on a source corpus')
    ap.add_argument('--corpus', default='ext', help='corpus for --validate (ext/ext2)')
    args = ap.parse_args()
    if args.validate:
        return cmd_validate(args)
    if args.lane:
        return cmd_lane(args)
    if args.groups:
        return cmd_groups(args)
    return cmd_default(args)


if __name__ == '__main__':
    sys.exit(main())
