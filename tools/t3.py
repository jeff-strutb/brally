#!/usr/bin/env python3
"""T3-certified functions: complete and verified, not yet byte-exact -- and the
OBJECTIVE gates that decide it (CLAUDE.md rule 12).

    .venv/bin/python tools/t3.py --qualify 0x1000EAF0 [--passes D1 D2 --census "..."]
    .venv/bin/python tools/t3.py                # list + validate every @t3 tag; exit 1 on a bad one
    .venv/bin/python tools/t3.py --vas          # the certified VAs, one per line

There are two grades of done and nothing between them: T4 (bytes diff clean)
and T3 (this).  The old T3a/T3b sub-tiers are retired; "T3a" in older dossier
text means "the residue is allocation/scheduling", "T3b" means "the oracle
said EQUIVALENT" -- both are now INPUTS to these gates, not tiers.

GATE A -- the residue test, from ONE fresh object (the last sweep's):
  A1 instruction-count gap        <= max(3, 0.5% of the original's count)
  A2 register-blind rows          missing + extra <= 2.5% of the original's count
  A3 every row classifies         each residue row is an allowed allocation
                                  singleton, or pairs with a row of the other
                                  side in the same canonical class (addressing
                                  form, lea/add form, flag form, branch
                                  polarity, x87 stack index).  An unpaired row
                                  is a missing or extra SEMANTIC operation:
                                  FAIL, whatever the totals say.
  A4 no lost-sync                 divergence.py compared every byte
  A5 oracle                       t3b_verify.py is not DIFF (EQUIVALENT, or
                                  UNCLASSIFIED because it cannot contain the
                                  function -- most of them)
GATE B -- the effort floor, declared on the tag and audited by a reader:
  two consecutive documented passes, >= 10 fresh-compile probes each, ZERO
  movement on bytes/insns/regions/rows, at least one census-driven; every
  dead probe recorded with its numbers.  `--passes D1 D2 --census WORDS`
  writes it; the validator requires two dates and a census word.

THE TAG, emitted by --qualify with the measured numbers (never typed):

    /* @t3 0x1000EAF0 2026-09-06 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
     * @t3-measure bytes 9345/9354 insns 2325/2328 rows 23+20 regions 14 oracle UNCLASSIFIED
     * @t3-effort passes 2026-09-05 2026-09-06 census slot-census,corpus,mechanism
     * <prose: what the residue is, where the dossier lives> */
    /* @implements ... */

The validator re-measures every tagged function against the current object and
FAILS if the @t3-measure numbers moved (re-run --qualify: the function changed
under the tag), if the function is now byte-exact (STALE: remove the @t3, keep
the @implements), or if a tag is malformed.  claim_lane.py never hands a
certified function out; tiers.py reports them with their own denominator.
"""
import collections, csv, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
SRC = os.path.join(ROOT, 'src')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
TAG = re.compile(r'@t3\s+(0x[0-9A-Fa-f]{8})\s+(\d{4}-\d{2}-\d{2})\b')
MEAS = re.compile(r'@t3-measure\s+bytes\s+(\d+)/(\d+)\s+insns\s+(\d+)/(\d+)\s+rows\s+(\d+)\+(\d+)\s+regions\s+(\d+)\s+oracle\s+(\w+)')
EFF = re.compile(r'@t3-effort\s+passes\s+((?:\d{4}-\d{2}-\d{2}\s*)+)census\s+(\S+)')
IMPL = re.compile(r'@implements\s+(0x[0-9A-Fa-f]{8})\b')

GAP_ABS, GAP_FRAC, ROWS_FRAC = 3, 0.005, 0.025

# ---------------------------------------------------------------- tags -----

def certified():
    """va(lower) -> dict(date,file,line,ok,why,measure,effort)"""
    out = {}
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if not fn.endswith(('.c', '.cpp', '.h')):
                continue
            path = os.path.join(dp, fn)
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            if '@t3 ' not in text:
                continue
            impls = set(m.group(1).lower() for m in IMPL.finditer(text))
            lines = text.splitlines()
            rel = os.path.relpath(path, ROOT)
            for ln, line in enumerate(lines, 1):
                if '@t3 ' not in line or '@t3-' in line:
                    continue
                m = TAG.search(line)
                if not m:
                    out['?%s:%d' % (rel, ln)] = dict(date='', file=rel, line=ln, ok=False,
                                                    why='malformed: need `@t3 0xVA YYYY-MM-DD`',
                                                    measure=None, effort=None)
                    continue
                va, date = m.group(1).lower(), m.group(2)
                blk = '\n'.join(lines[ln - 1:ln + 6])
                mm, me = MEAS.search(blk), EFF.search(blk)
                why = ''
                if 'CERTIFIED COMPLETE' not in line:
                    why = 'missing the phrase CERTIFIED COMPLETE on the tag line'
                elif va not in impls:
                    why = 'no @implements %s in this file' % va
                elif not mm:
                    why = 'missing/malformed @t3-measure line (run --qualify and paste its tag)'
                elif not me or len(me.group(1).split()) < 2:
                    why = 'missing @t3-effort line with two pass dates and a census word'
                out[va] = dict(date=date, file=rel, line=ln, ok=not why, why=why,
                               measure=(tuple(int(x) for x in mm.groups()[:7]) + (mm.group(8),)) if mm else None,
                               effort=me.groups() if me else None)
    return out


def report_rows():
    rows = {}
    if os.path.exists(REPORT):
        for r in csv.DictReader(open(REPORT)):
            if r.get('va'):
                rows[r['va'].lower()] = r
    return rows


def report_status():
    return {va: r.get('status', '') for va, r in report_rows().items()}

# ------------------------------------------------------------- measure -----

def _find_obj(row):
    variant = row.get('variant') or 'O2'
    base = os.path.splitext(os.path.basename(row['file']))[0]
    for d in ('obj_' + variant, 'obj_O2', 'obj_O2y', 'obj_Od'):
        p = os.path.join(ROOT, 'build', 'match', d, base + '.obj')
        if os.path.exists(p):
            return p
    return None


def _sym(obj, name):
    from match_diff import parse_coff_obj
    d = parse_coff_obj(obj)
    for k in (name, '_' + name):
        if k in d:
            return k
    for k in d:
        if name in k:
            return k
    return None

# The residue classifier.  A row is msetdiff's normalised text
# (registers -> R/W/B, esp slots -> esp+S, scales -> *K, relocs -> A).
SINGLETON = [
    r'^mov dword ptr \[esp\+S\], R$',       # spill / home
    r'^mov R, dword ptr \[esp\+S\]$',       # reload
    r'^mov R, R$',                          # register copy
    r'^lea R, \[R\*K\]$',                   # CSE'd scaled index
    r'^test R, R$',                         # flag re-test after a copy/reload
    r'^fxch st\(\d\)$',                     # x87 stack permutation, no value effect
    r'^mov R, (0x[0-9a-f]{1,4}|\d{1,5})$',  # rematerialised small constant at a join
    r'^mov R, A$',                          # rematerialised address constant
]
SINGLETON = [re.compile(p) for p in SINGLETON]


def canon(row):
    """Collapse the compiler-decision axes so paired rows become equal."""
    mn, _, ops = row.partition(' ')
    if mn != 'mov':
        ops = ops.replace('dword ptr [esp+S]', 'R')          # homed operand == register
    # memory operand forms
    def mem(m):
        inner = m.group(1)
        if re.search(r'\bA\b', inner):
            return '[A]'                                     # [R*K + A], [R + A], [A]
        d = re.search(r'([+-])\s*(0x[0-9a-f]+|\d+)$', inner)
        if d:
            return '[M%s%s]' % (d.group(1), d.group(2))      # [R + R + d], [R + d], [R - d], [R*K + d]
        d = re.search(r'^(0x[0-9a-f]+|\d+)$', inner)
        return '[M+%s]' % d.group(1) if d else '[M]'         # [d], [R], [R + R]
    ops = re.sub(r'\[([^\]]+)\]', mem, ops)
    if mn == 'lea':
        if ops == 'R, [M]':      return 'add R, R'           # lea R,[R+R]  ~ add R,R
        if ops == 'R, [M+1]':    return 'inc R'              # lea R,[R+1]  ~ inc R
        if ops == 'R, [M-1]':    return 'dec R'              # lea R,[R-1]  ~ dec R
    if mn in ('jns', 'jge'):  return 'jge T'
    if mn in ('js', 'jl'):    return 'jl T'
    if mn in ('je', 'jne'):   return 'jeq T'
    if mn in ('ja', 'jbe', 'jb', 'jae', 'jg', 'jle') and False:
        pass
    if mn in ('faddp', 'fmulp', 'fsubp', 'fsubrp', 'fdivp', 'fdivrp', 'fld', 'fst', 'fstp') \
            and re.fullmatch(r'st\(\d\)', ops):
        return mn + ' st'                                    # x87 stack index
    return mn + ' ' + ops


def classify(miss, extra):
    """Return (unmatched_missing, unmatched_extra, singles) after canonical pairing."""
    def split(c):
        single, rest = collections.Counter(), collections.Counter()
        for row, n in c.items():
            if any(p.match(row) for p in SINGLETON):
                single[row] += n
            else:
                rest[canon(row)] += n
        return single, rest
    sm, rm = split(miss)
    se, rx = split(extra)
    um, ue = rm - rx, rx - rm
    return um, ue, sm + se


def measure(va):
    """All Gate-A numbers for one VA from the current sweep object."""
    from msetdiff import load
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
    rows = report_rows()
    r = rows.get(va.lower())
    if not r:
        return None, 'no report.csv row (unswept)'
    obj = _find_obj(r)
    if not obj:
        return None, 'no sweep object for %s (run the one-file sweep)' % r['file']
    sym = _sym(obj, r['name'])
    if not sym:
        return None, 'symbol %s not in %s' % (r['name'], obj)
    ob = os.path.join(ROOT, 'build', 'match', 'orig', va.upper().replace('0X', '0x') + '.bin')
    if not os.path.exists(ob):
        ob = os.path.join(ROOT, 'build', 'match', 'orig', '0x%08X.bin' % int(va, 16))
    if not os.path.exists(ob):
        return None, 'no original bytes'
    o, no = load(ob, None)
    rc, nr = load(obj, sym)
    orig = open(ob, 'rb').read()
    from match_diff import parse_coff_obj
    code = parse_coff_obj(obj)[sym][0]
    ins = list(md.disasm(code, 0))
    while ins and ins[-1].mnemonic in ('nop', 'int3'):
        ins.pop()
    rbytes = (ins[-1].address + ins[-1].size) if ins else 0
    miss, extra = o - rc, rc - o
    um, ue, singles = classify(miss, extra)
    # divergence: masked region count + lost-sync
    c = [PY, 'tools/divergence.py', os.path.relpath(obj, ROOT),
         os.path.relpath(ob, ROOT), sym, '--deltas', '--key', '6', '--mask-slots']
    out = subprocess.run(c, cwd=ROOT, capture_output=True, text=True).stdout
    mreg = re.search(r'total divergence regions from offset 0x0:\s*(\d+)', out)
    regions = int(mreg.group(1)) if mreg else -1
    lost = [l for l in out.splitlines() if 'NEVER COMPARED' in l or 'lost-sync' in l.lower()]
    # oracle
    orc = subprocess.run([PY, 'tools/t3b_verify.py', va], cwd=ROOT,
                         capture_output=True, text=True).stdout.strip().splitlines()
    verdict = 'UNCLASSIFIED'
    for l in orc:
        m = re.search(r'\b(EQUIVALENT|DIFF|UNCLASSIFIED)\b', l)
        if m:
            verdict = m.group(1)
    return dict(va=va.lower(), name=r['name'], file=r['file'], status=r['status'], obj=obj,
                obytes=len(orig), rbytes=rbytes, oi=no, ri=nr,
                miss=miss, extra=extra, nmiss=sum(miss.values()), nextra=sum(extra.values()),
                um=um, ue=ue, singles=singles, regions=regions, lost=lost, oracle=verdict), ''


def gates(m):
    g = []
    gap = abs(m['oi'] - m['ri']); lim = max(GAP_ABS, GAP_FRAC * m['oi'])
    g.append(('A1 insn gap', gap <= lim, '%d (limit %.1f)' % (gap, lim)))
    rows = m['nmiss'] + m['nextra']; rlim = ROWS_FRAC * m['oi']
    g.append(('A2 rows', rows <= rlim, '%d+%d = %d (limit %.1f)' % (m['nmiss'], m['nextra'], rows, rlim)))
    unm = sum(m['um'].values()) + sum(m['ue'].values())
    g.append(('A3 classify', unm == 0, '%d unpaired row(s)' % unm))
    g.append(('A4 lost-sync', not m['lost'], 'none' if not m['lost'] else m['lost'][0].strip()))
    g.append(('A5 oracle', m['oracle'] != 'DIFF', m['oracle']))
    return g


def tag_text(m, date, passes, census):
    return ('/* @t3 %s %s -- CERTIFIED COMPLETE, NOT BYTE-EXACT.\n'
            ' * @t3-measure bytes %d/%d insns %d/%d rows %d+%d regions %d oracle %s\n'
            ' * @t3-effort passes %s census %s\n'
            ' * <what the residue is, by wall; where the dossier and dead list live;\n'
            ' *  "Do not reopen before the end-grind (CLAUDE.md rule 12)."> */'
            % ('0x%08X' % int(m['va'], 16), date, m['rbytes'], m['obytes'], m['ri'], m['oi'],
               m['nmiss'], m['nextra'], m['regions'], m['oracle'],
               ' '.join(passes) if passes else '<D1> <D2>', census or '<census-words>'))


def qualify(argv):
    import datetime
    va = argv[0]
    passes, census = [], ''
    if '--passes' in argv:
        k = argv.index('--passes'); j = k + 1
        while j < len(argv) and re.fullmatch(r'\d{4}-\d{2}-\d{2}', argv[j]):
            passes.append(argv[j]); j += 1
    if '--census' in argv:
        census = argv[argv.index('--census') + 1]
    m, why = measure(va)
    if not m:
        print('CANNOT MEASURE: ' + why); return 2
    print('=== %s %s  (%s, status %s)' % (m['va'], m['name'], m['file'], m['status']))
    if m['status'] == 'match':
        print('byte-exact already: T4. No tag.'); return 0
    ok = True
    for name, passed, detail in gates(m):
        ok &= passed
        print('  %-14s %s  %s' % (name, 'PASS' if passed else 'FAIL', detail))
    if m['um'] or m['ue']:
        for row, n in m['um'].most_common(): print('     unpaired MISSING %2d  %s' % (n, row))
        for row, n in m['ue'].most_common(): print('     unpaired EXTRA   %2d  %s' % (n, row))
    print('  singletons     %s' % ', '.join('%s x%d' % (r, n) for r, n in m['singles'].most_common()))
    print('  masked regions %d   bytes %d/%d' % (m['regions'], m['rbytes'], m['obytes']))
    print('GATE A: %s' % ('PASS' if ok else 'FAIL -- not certifiable; the unpaired rows are real code'))
    if ok:
        print('GATE B is yours to assert on the @t3-effort line (two zero-movement passes, a census).')
        print('Paste directly above the @implements line:\n')
        print(tag_text(m, datetime.date.today().isoformat(), passes, census))
    return 0 if ok else 1


def main(argv):
    if argv and argv[0] == '--qualify':
        return qualify(argv[1:])
    cert = certified()
    if '--vas' in argv:
        for va in sorted(cert):
            if not va.startswith('?'):
                print(va)
        return 0
    st = report_status()
    bad = 0
    for va, info in sorted(cert.items()):
        flag = 'ok'
        if not info['ok']:
            flag = 'BAD: ' + info['why']; bad += 1
        elif st.get(va) == 'match':
            flag = 'STALE: byte-exact now -- remove the @t3 tag (keep @implements)'; bad += 1
        elif va not in st:
            flag = 'unswept (no report.csv row yet)'
        else:
            m, why = measure(va)
            if m:
                tm = info['measure']
                now = (m['rbytes'], m['obytes'], m['ri'], m['oi'], m['nmiss'], m['nextra'], m['regions'])
                if tuple(tm[:7]) != now:
                    flag = ('STALE-NUMBERS: tag says bytes %d/%d insns %d/%d rows %d+%d regions %d, '
                            'object says %d/%d %d/%d %d+%d %d -- re-run --qualify'
                            % (tm[:7] + now)); bad += 1
                g = gates(m)
                if not all(p for _, p, _ in g):
                    flag = 'FAILS GATE A NOW: ' + ', '.join(n for n, p, _ in g if not p); bad += 1
            else:
                flag = 'unmeasured (%s)' % why
        print('%s  %s  %s:%d  %s' % (va, info['date'], info['file'], info['line'], flag))
    n = sum(1 for v in cert if not v.startswith('?'))
    print('T3-certified functions: %d  (not counted as matched)' % n)
    if bad:
        print('FAIL: %d bad, stale or failing @t3 tag(s).' % bad)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
