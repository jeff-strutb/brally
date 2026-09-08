#!/usr/bin/env python3
"""Pick the next byte-exact targets from the tree.  Nothing here is typed by
hand; run it at session start.  The lane spec that consumes this output is
handed to the session directly (specs in the repo go stale).

    .venv/bin/python tools/t4lane.py                # both pools, 20 primaries + alternates
    .venv/bin/python tools/t4lane.py --pool A       # register-only T2 rows, by translation unit
    .venv/bin/python tools/t4lane.py --pool B       # smallest untouched drafts, screened
    .venv/bin/python tools/t4lane.py --n 30 --max-bytes 400
    .venv/bin/python tools/t4lane.py --claim         # lock Pool B primaries (TOKEN)

‼ LOCK THROUGH THIS TOOL, never `claim_lane.py claim N` (hard error).
`--claim` with both pools locks Pool B only; Pool A is printed as T3
candidates. `--pool A --claim` still locks Pool A for a qualify batch.

POOL A -- rows in build/match/report.csv with status=diff whose register-blind
instruction multiset equals the original's (reggap 0, tools/fnmatch/triage.py):
the same instructions, only allocation/order/one fact differs.  Grouped by
translation unit (the cluster rule), files ordered by their smallest masked
diff, rows inside a file by diff.  Excluded: T3-certified rows (@t3),
C++-lane rows, rows claimed by a live token.

A PARK IS A HOLD ONLY WHILE IT IS NEWER THAN THE SCREENS.  The declaration-
order, guard-shape, sibling-asymmetry and file-position screens landed
2026-09-03..06 (SCREENS_DATE); a row parked before that date was parked
without them, so it is LIVE and prints `[park <date> predates screens]`.
Parked on or after that date: HELD, needing a lever the park predates.  A
`@t4-pass` ledger line is NOT a hold: Gate B needs two of them at the current
numbers. Colouring walls (`reggap 0`) are T3 candidates, not a T4 grind.
A file dead list is never a hold: it is the input to the next pass.

POOL B -- functions with no @implements anywhere (the T1 set from
tools/tiers.py), smallest first, run through the mechanical screen from
docs/MATCHING.md: a `6A FF` prologue (C++ EH frame) is out; an odd
address (split map row) is out; a VA present in report_cpp.csv is the C++
lane's; any `fxch` (x87 juggling) is out; more than two 16-bit register ops
(byte/word lanes) is out.  Survivors print with their draft path; rejects
print with the reason so the screen is auditable.

Both pools honour the same exclusions and are independent of each other.
"""
import csv, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'fnmatch'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

W16 = re.compile(r'\b(ax|bx|cx|dx|si|di|bp|ah|bh|ch|dh)\b')
NOTE = re.compile(r'\bPARKED\b|\bDEAD\b|[Dd]o not re-run|dead list|do NOT re-run')
# Newest of the mechanical screens (file-position lever, 2026-09-06).  A park
# older than this was made without them and is not a hold.
SCREENS_DATE = '2026-09-06'


def _csv(path):
    p = os.path.join(ROOT, path)
    return list(csv.DictReader(open(p))) if os.path.exists(p) else []


def _orig(va):
    p = os.path.join(ROOT, 'build', 'match', 'orig', '0x%08X.bin' % int(va, 16))
    return open(p, 'rb').read() if os.path.exists(p) else b''


def exclusions():
    cert = set()
    try:
        from t3 import certified
        cert = set(va for va, i in certified().items() if not va.startswith('?'))
    except Exception:
        pass
    cpp = set((r.get('va') or '').lower() for r in _csv('build/match/report_cpp.csv'))
    claimed, parked = set(), {}
    for r in _csv('build/match/lane_claims.csv'):
        if r.get('status') == 'parked':
            try:
                import datetime
                parked[r['va'].lower()] = datetime.date.fromtimestamp(float(r.get('ts') or 0)).isoformat()
            except Exception:
                parked[r['va'].lower()] = '?'
        else:
            claimed.add(r['va'].lower())
    return cert, cpp, claimed, parked


def has_note(path, va):
    """An explicit park marker or dead list in `path` within 400 chars of va.
    A plain 'residue' mention is NOT a hold: read it, then work the row."""
    try:
        t = open(os.path.join(ROOT, path), encoding='utf-8', errors='replace').read()
    except OSError:
        return False
    for m in re.finditer(re.escape(va[2:]), t, re.I):
        seg = t[max(0, m.start() - 400):m.end() + 200]
        if NOTE.search(seg):
            return True
    return False


def has_mention(path, va):
    """A softer 'residue'/'note' mention near va: print as a flag, never a hold."""
    try:
        t = open(os.path.join(ROOT, path), encoding='utf-8', errors='replace').read()
    except OSError:
        return False
    for m in re.finditer(re.escape(va[2:]), t, re.I):
        seg = t[max(0, m.start() - 400):m.end() + 200]
        if re.search(r'(?i)\bresidue\b|\bwall\b', seg):
            return True
    return False


def ledger(path, va):
    """(count, last date) of @t4-pass lines for va in path."""
    try:
        t = open(os.path.join(ROOT, path), encoding='utf-8', errors='replace').read()
    except OSError:
        return 0, ''
    ds = re.findall(r'@t4-pass\s+' + re.escape(va) + r'\s+\d+\s+(\d{4}-\d{2}-\d{2})', t, re.I)
    return len(ds), max(ds) if ds else ''


def pool_a(max_bytes):
    from triage import measure, _objs
    cert, cpp, claimed, parked = exclusions()
    objs = _objs()
    rows = [r for r in _csv('build/match/report.csv')
            if r.get('status') == 'diff' and r.get('orig_size') and int(r['orig_size']) <= max_bytes]
    live, held = {}, []
    for r in rows:
        va = r['va'].lower()
        if va in cert or va in cpp or va in claimed:
            continue
        m = measure(va, r['name'], objs)
        if not m or m['reg'] != 0 or m['oi'] != m['ri']:
            continue
        diff = int(r.get('diffs') or r.get('diff_bytes') or list(r.values())[-1] or 0)
        npass, last = ledger(r['file'], r['va'])
        rec = dict(va=r['va'], name=r['name'], size=int(r['orig_size']), diff=diff,
                   file=r['file'], note=has_note(r['file'], r['va']),
                   mention=has_mention(r['file'], r['va']), passes=npass, flag='')
        pdate = parked.get(va, '')
        if pdate >= SCREENS_DATE:
            rec['why'] = 'lane-parked %s' % pdate
            held.append(rec)
        else:
            if pdate:
                rec['flag'] = '[park %s predates screens]' % pdate
            elif rec['note']:
                rec['flag'] = '[dead list in header: read it first]'
            elif rec['mention']:
                rec['flag'] = '[residue note: read it first]'
            live.setdefault(r['file'], []).append(rec)
    for f in live:
        live[f].sort(key=lambda x: (x['diff'], x['size']))
    files = sorted(live, key=lambda f: (live[f][0]['diff'], len(live[f])))
    return files, live, sorted(held, key=lambda x: (x['diff'], x['size']))


def pool_b(max_bytes):
    cert, cpp, claimed, parked = exclusions()
    fenced = set(r['va'].upper() for r in _csv('config/fenced.csv'))
    target = {}
    for r in _csv('config/functions_glide.csv'):
        if r['va'].upper() not in fenced and r.get('size'):
            target[r['va'].upper()] = int(r['size'])
    tagged = set((r.get('va') or '').upper() for r in _csv('build/match/report.csv') if r.get('orig_size'))
    tagged |= set(va.upper() for va in cpp)
    keep, out = [], []
    for va, sz in sorted(target.items(), key=lambda kv: kv[1]):
        if va in tagged or sz > max_bytes or sz == 0:
            continue
        b = _orig(va)
        why = []
        if b[:2] == b'\x6a\xff': why.append('C++ EH frame')
        if int(va, 16) % 2: why.append('odd address (split map row)')
        if va.lower() in claimed: why.append('claimed')
        if va.lower() in parked: why.append('parked')
        ins = list(md.disasm(b, 0))
        fx = sum(1 for i in ins if i.mnemonic.startswith('fxch'))
        w16 = sum(1 for i in ins if W16.search(i.op_str))
        if fx: why.append('%d fxch' % fx)
        if w16 > 2: why.append('%d 16-bit ops' % w16)
        draft = None
        for cand in ('build/ghidra_work/%s.refined.c' % va.lower(), 'build/ghidra_decomp/%s.c' % va.lower()):
            if os.path.exists(os.path.join(ROOT, cand)):
                draft = cand; break
        if not draft: why.append('no draft')
        rec = dict(va=va, size=sz, draft=draft or '-', why=', '.join(why))
        (out if why else keep).append(rec)
    return keep, out


def main(argv):
    n = int(argv[argv.index('--n') + 1]) if '--n' in argv else 20
    mb = int(argv[argv.index('--max-bytes') + 1]) if '--max-bytes' in argv else 400
    pool = argv[argv.index('--pool') + 1].upper() if '--pool' in argv else 'BOTH'
    primaries = []
    claim_a = pool == 'A'   # --claim on BOTH locks Pool B only; Pool A is T3
    if pool in ('A', 'BOTH'):
        files, live, held = pool_a(mb)
        total = sum(len(v) for v in live.values())
        print('=== POOL A: register-only T2 rows (reggap 0, <= %d B) -- T3 candidates, not a T4 grind -- %d rows in %d files'
              % (mb, total, len(files)))
        k = 0
        for f in files:
            print('  %s' % f)
            for r in live[f]:
                k += 1
                mark = ''
                if claim_a and k <= n:
                    primaries.append(r['va'])
                    mark = '   <-- primary'
                print('     %s %-30s %4d B  diff %3d  ledger %d  %s%s'
                      % (r['va'], r['name'], r['size'], r['diff'], r['passes'], r['flag'], mark))
        if held:
            print('  --- HELD (parked or passed on/after %s: only with a lever the park PREDATES): %d'
                  % (SCREENS_DATE, len(held)))
            for r in held:
                print('     %s %-30s %4d B  diff %3d  ledger %d  %-22s %s'
                      % (r['va'], r['name'], r['size'], r['diff'], r['passes'], r['why'], r['file']))
        if pool == 'BOTH':
            print('  (not claimed; qualify with tools/t3.py --qualify <VA>)')
    if pool in ('B', 'BOTH'):
        keep, out = pool_b(mb)
        print('=== POOL B: smallest untouched drafts (<= %d B), mechanically screened -- %d clean, %d rejected'
              % (mb, len(keep), len(out)))
        for i, r in enumerate(keep):
            if i < n:
                primaries.append(r['va'])
            print('     %s %4d B  %s%s' % (r['va'], r['size'], r['draft'], '   <-- primary' if i < n else ''))
        print('  --- rejected by the screen (do not start in this lane):')
        for r in out:
            print('     %s %4d B  %s' % (r['va'], r['size'], r['why']))
    if '--claim' in argv:
        if not primaries:
            print('nothing to claim'); return 1
        from claim_lane import claim
        print('=== locking %d primaries in build/match/lane_claims.csv' % len(primaries))
        claim(len(primaries), vas=primaries)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
