#!/usr/bin/env python3
"""crank.py -- unattended, DETERMINISTIC lever sweep to byte-exact, in the
function's REAL translation unit, scored register-blind.  Zero tokens.

    .venv/bin/python tools/crank.py                    # every live Pool A row from t4lane.py
    .venv/bin/python tools/crank.py 0x10018A50 ...     # these VAs (must be report.csv rows)
    .venv/bin/python tools/crank.py --budget 200 --no-commit --no-ledger

WHY A THIRD MACHINE.  tools/ghidra_to_match.py closed the drafts a heuristic
rewrite could close (0 of 296 left on 2026-09-07), and tools/permute.py is a
random annealer that (a) scores by RAW diff bytes, which register rotation
makes meaningless, (b) compiles the function alone in a wrapped TU, when the
surrounding TU is a proven codegen input (docs/VC5-IDIOMS.md: file position,
commutative order), and (c) has none of the four levers that closed most of
the 2026-09-03..06 wins: declaration order, file position, guard shape and
commutative operand order -- its header still calls declaration order inert.
126 permuter runs on the tree found 0 byte-exact endpoints.

WHAT THIS DOES.  For one function already in the tree with status=diff:

  1. compile its OWN file unchanged; measure (exact?, register-blind gap,
     instruction delta, masked divergence regions, byte delta, raw diffs);
  2. enumerate candidates from a fixed, ordered lever list -- every candidate
     is the tree file with ONE change to that function:
       filepos   the function moved to the end / the front of the TU
       decl      declaration-run permutations (exhaustive <= 4, else each
                 declaration to the end and to the front of its run)
       comm      each `x = a OP b` with OP in + * swapped
       stmt      each adjacent pair of independent simple statements swapped
       mut       every SOUND mutation in tools/permute.py, run at seeds
                 0..SEEDS-1 and de-duplicated (guard shape, if-invert,
                 temps, widths, loop shape, ...)
  3. first-improvement hill climb on the lexicographic score
     (exact, reggap, |insn delta|, regions, |byte delta|, raw); a candidate
     that compiles worse or equal is dropped; the climb restarts from the
     first lever after every accept; stops at byte-exact, at --budget
     compiles, or when a full round moves nothing;
  4. BYTE-EXACT: the file is written, the one-file sweep re-run, EVERY row
     of that file must be match-or-unchanged (the cluster rule), then
     `git commit -- <file>` with the lever history in the message.  Any
     regression in a neighbour: tree restored, result parked in
     build/ghidra_work/<VA>.crank.c, nothing committed.
     NOT EXACT: nothing in the tree changes except (unless --no-ledger) one
     `@t4-pass` line above the function's tag when >= 10 compiles ran
     (CLAUDE.md rule 12: a counted pass), committed once at the end.

Soundness only matters for the ACCEPTED intermediate states, which steer the
climb; the endpoint is either byte-identical to the original (correct by
definition) or discarded.  So only permute.py's sound set is used.

Every compile is logged to build/match/crank.log with its score.  A file that
is dirty in `git status` is skipped: another session has it open.
"""
import os, re, sys, csv, time, random, hashlib, itertools, subprocess, shutil, datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'fnmatch'))
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
S = os.path.join(ROOT, 'build', 'match', 't3d'); os.makedirs(S, exist_ok=True)
LOG = os.path.join(ROOT, 'build', 'match', 'crank.log')

import permute as P                       # noqa: E402
from match_diff import parse_coff_obj     # noqa: E402
from triage import _bag, _strip_pad       # noqa: E402
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

SEEDS = 6
DECL_RE = re.compile(r'^(\s+)(static\s+)?(const\s+)?(unsigned\s+|signed\s+)?'
                     r'(int|float|double|char|short|long|u?int(8|16|32)_t|BOOL|DWORD|WORD|BYTE|'
                     r'[A-Z][A-Za-z0-9_]*)\b\s*\**\s*[A-Za-z_]\w*(\[[^\]]*\])*(\s*=\s*[^;]+)?;\s*(/\*.*\*/)?\s*$')


def log(s):
    with open(LOG, 'a') as f:
        f.write('%s %s\n' % (time.strftime('%H:%M:%S'), s))
    print(s, flush=True)


# --------------------------------------------------------------------------- compile + measure
def compile_tu(text, tag):
    relc = 'build/match/t3d/crank_%s.c' % tag
    relo = 'build/match/t3d/crank_%s.obj' % tag
    obj = os.path.join(ROOT, relo)
    open(os.path.join(ROOT, relc), 'w').write(text)
    if os.path.exists(obj):
        os.unlink(obj)
    cmd = ['sh', 'tools/wine.sh', 'tools/msvc5/bin/cl.exe', '/nologo', '/O2', '/W3',
           '/I', 'include', '/I', 'tools/msvc5-compat', '/I', 'tools/msvc5/include',
           '/DBR_MATCHING_BUILD', '/c', relc, '/Fo' + relo.replace('/', '\\')]
    try:
        subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180)
    except subprocess.TimeoutExpired:
        return None
    return obj if os.path.exists(obj) else None


def measure(obj, sym, va, key):
    t = parse_coff_obj(obj)
    if sym not in t:
        return None
    rc, rel = t[sym][0], set(t[sym][1])
    rc = _strip_pad(rc)
    orig = open(os.path.join(ROOT, 'build', 'match', 'orig', va + '.bin'), 'rb').read()
    n = min(len(orig), len(rc))
    raw = sum(1 for i in range(n) if i not in rel and orig[i] != rc[i]) + abs(len(orig) - len(rc))
    exact = (len(rc) == len(orig) and raw == 0)
    O, R = _bag(orig, 'regnorm'), _bag(rc, 'regnorm', rel)
    reg = sum((R - O).values()) + sum((O - R).values())
    oi = len(list(md.disasm(orig, 0))); ri = len(list(md.disasm(rc, 0)))
    regions, lost = 0, 0
    if not exact:
        c = [PY, 'tools/divergence.py', obj, 'build/match/orig/%s.bin' % va, sym,
             '--key', str(key), '--mask-slots']
        out = subprocess.run(c, cwd=ROOT, capture_output=True, text=True).stdout
        m = re.search(r'total divergence regions from offset 0x0: (\d+)', out)
        regions = int(m.group(1)) if m else 999
        m = re.search(r'NEVER COMPARED[^\n]*?(\d+) B', out)
        lost = int(m.group(1)) if m else 0
    return dict(exact=exact, raw=raw, reg=reg, di=ri - oi, db=len(rc) - len(orig),
                regions=regions + (1 if lost > 32 else 0), oi=oi, ob=len(orig))


def score(m):
    return (0 if m['exact'] else 1, m['reg'], abs(m['di']), m['regions'], abs(m['db']), m['raw'])


def fmt(m):
    return ('EXACT' if m['exact'] else
            'reg %d insn %+d regions %d bytes %+d raw %d' % (m['reg'], m['di'], m['regions'], m['db'], m['raw']))


# --------------------------------------------------------------------------- levers
def _fn(text, sym):
    return P._split_fn(text, sym)


def lever_filepos(text, sym):
    loc = P._find_func_def(text, name=sym)
    if not loc:
        return
    start, ob, cb, _ = loc
    # carry the comment block directly above the definition (WHAT IT DOES / tags)
    head = text[:start]
    m = re.search(r'(/\*(?:(?!\*/).)*\*/\s*)+$', head, re.S)
    if m:
        start = m.start()
    fn = text[start:cb + 1]
    rest = text[:start] + text[cb + 1:]
    # end: before a trailing #endif if the file ends inside a guard, else append
    tail = re.search(r'\n#endif[^\n]*\s*$', rest)
    if tail:
        yield 'filepos:end', rest[:tail.start()] + '\n\n' + fn + '\n' + rest[tail.start():]
    else:
        yield 'filepos:end', rest.rstrip('\n') + '\n\n' + fn + '\n'
    first = P._find_func_def(rest, last=False)
    if first and first[0] > 0:
        s = first[0]
        h = rest[:s]
        m2 = re.search(r'(/\*(?:(?!\*/).)*\*/\s*)+$', h, re.S)
        if m2:
            s = m2.start()
        if s != start:
            yield 'filepos:front', rest[:s] + fn + '\n\n' + rest[s:]


def _decl_runs(body):
    """The function-top declaration run (C89: declarations precede statements).
    Blank and comment lines inside the run are kept in place."""
    lines = body.split('\n')
    run = []
    for i, l in enumerate(lines):
        s = l.strip()
        if not s or s.startswith(('/*', '*', '//')):
            continue
        if DECL_RE.match(l) and ',' not in l.split('=')[0] and '(' not in l.split('=')[0]:
            run.append(i)
        else:
            break
    return lines, ([run] if len(run) > 1 else [])


def lever_decl(text, sym):
    parts = _fn(text, sym)
    if not parts:
        return
    lines, runs = _decl_runs(parts['body'])
    for run in runs:
        orig = [lines[i] for i in run]
        cands = []
        if len(run) <= 4:
            for perm in itertools.permutations(range(len(run))):
                if list(perm) != list(range(len(run))):
                    cands.append(('decl:perm' + ''.join(map(str, perm)), [orig[p] for p in perm]))
        else:
            for k in range(len(run)):
                rest = orig[:k] + orig[k + 1:]
                cands.append(('decl:%d->end' % k, rest + [orig[k]]))
                cands.append(('decl:%d->front' % k, [orig[k]] + rest))
        for label, new in cands:
            nl = list(lines)
            for i, l in zip(run, new):
                nl[i] = l
            yield label, P._rebuild(parts, '\n'.join(nl))


def _balanced(s):
    return s.count('(') == s.count(')') and s.count('[') == s.count(']')


def lever_comm(text, sym):
    parts = _fn(text, sym)
    if not parts:
        return
    body = parts['body']
    pat = re.compile(r'(?m)^([ \t]*)([^;{}=\n]+?)\s=\s([^;\n]+?)\s([+*])\s([^;\n]+?);[ \t]*$')
    for m in pat.finditer(body):
        a, op, b = m.group(3), m.group(4), m.group(5)
        if not (_balanced(a) and _balanced(b)) or '?' in a + b or '&&' in a + b or '||' in a + b:
            continue
        if re.search(r'\s[+*]\s', a) and re.search(r'\s[+*]\s', b):
            pass
        new = body[:m.start()] + '%s%s = %s %s %s;' % (m.group(1), m.group(2), b, op, a) + body[m.end():]
        yield 'comm:%s@%d' % (op, body[:m.start()].count('\n')), P._rebuild(parts, new)


def lever_stmt(text, sym):
    parts = _fn(text, sym)
    if not parts:
        return
    body = parts['body']
    stmts = P._top_statements(body)
    ctl = ('if', 'for', 'while', 'switch', 'do', 'return', 'case', 'default', 'break', 'continue', '#', '}', '{', 'else')
    for i in range(len(stmts) - 1):
        a, b = stmts[i], stmts[i + 1]
        if P._is_decl(a) or P._is_decl(b):
            continue
        if a.strip().startswith(ctl) or b.strip().startswith(ctl):
            continue
        if '(' in a and not re.search(r'\)\s*[-+*/]|\(\s*\w+\s*\*?\s*\)', a):
            pass
        la, lb = P._lhs_names(a), P._lhs_names(b)
        ia, ib = P._idents(a), P._idents(b)
        if la & ib or lb & ia or not la or not lb:
            continue
        s2 = list(stmts); s2[i], s2[i + 1] = b, a
        yield 'stmt:swap@%d' % i, P._rebuild(parts, ''.join(s2))


def lever_mut(text, sym):
    parts = _fn(text, sym)
    if not parts:
        return
    seen = set()
    for w, f in P._mutation_table(False):
        for seed in range(SEEDS):
            try:
                nb, lab = f(parts['body'], random.Random(seed))
            except Exception:
                continue
            h = hashlib.md5(nb.encode()).hexdigest()
            if h in seen or nb == parts['body']:
                continue
            seen.add(h)
            yield 'mut:%s' % lab, P._rebuild(parts, nb)


SAMEBASE = re.compile(r'\(char \*\)&(DAT_([0-9a-fA-F]{8})) \+ ([A-Za-z_]\w*)\b(?! \+ 0x)')


def lever_samebase(text, sym):
    """Ghidra names every record field as its own DAT_ global; the original
    addressed them from ONE symbol (a struct, or an array of them).  VC5 will
    not hoist a load above a store to the SAME base symbol, but freely
    reorders across DISTINCT symbols -- so the Ghidra spelling changes the
    schedule (0x100283C0: store,store,load became load,store,store; the park
    note blamed scheduling and probed three orderings dead).  Group every
    `(char *)&DAT_X + idx` by idx; where two or more symbols share an index,
    re-spell them all from the lowest symbol plus a constant displacement.
    Both `idx + 0xNN` and `0xNN + idx` orders are offered."""
    parts = _fn(text, sym)
    if not parts:
        return
    body = parts['body']
    groups = {}
    for m in SAMEBASE.finditer(body):
        groups.setdefault(m.group(3), {})[m.group(1)] = int(m.group(2), 16)
    for idx, syms in groups.items():
        if len(syms) < 2:
            continue
        base_sym, base = min(syms.items(), key=lambda kv: kv[1])
        for order in ('idx-first', 'disp-first'):
            def rep(m):
                if m.group(3) != idx:
                    return m.group(0)
                d = syms[m.group(1)] - base
                if d == 0:
                    return '(char *)&%s + %s' % (base_sym, idx)
                return ('(char *)&%s + %s + 0x%x' if order == 'idx-first' else '(char *)&%s + 0x%x + %s') % (
                    (base_sym, idx, d) if order == 'idx-first' else (base_sym, d, idx))
            nb = SAMEBASE.sub(rep, body)
            if nb != body:
                yield 'samebase:%s:%s:%s' % (base_sym, idx, order), P._rebuild(parts, nb)


LEVERS = [('samebase', lever_samebase), ('filepos', lever_filepos), ('decl', lever_decl),
          ('comm', lever_comm), ('stmt', lever_stmt), ('mut', lever_mut)]


# --------------------------------------------------------------------------- tree bookkeeping
def report_row(va):
    with open(os.path.join(ROOT, 'build', 'match', 'report.csv')) as f:
        for r in csv.DictReader(f):
            if r['va'].lower() == va.lower():
                return r
    return None


def file_rows(path):
    with open(os.path.join(ROOT, 'build', 'match', 'report.csv')) as f:
        return {r['va']: r['status'] for r in csv.DictReader(f) if r['file'] == path}


def dirty(path):
    out = subprocess.run(['git', 'status', '--short', '--', path], cwd=ROOT, capture_output=True, text=True).stdout
    return bool(out.strip())


def sweep(path):
    subprocess.run([PY, 'tools/match_sweep.py', path], cwd=ROOT, capture_output=True, text=True, timeout=900)


def ledger_line(text, va, sym, n, m):
    k = len(re.findall(r'@t4-pass\s+' + re.escape(va), text, re.I)) + 1
    line = ('/* @t4-pass %s %d %s probes %d bytes %d insns %d regions %d rows %d census no  (tools/crank.py) */\n'
            % (va, k, datetime.date.today().isoformat(), n, m['ob'] + m['db'], m['oi'] + m['di'], m['regions'], m['reg']))
    tag = re.search(r'(?m)^[ \t]*/\*\s*@implements\s+' + re.escape(va), text, re.I)
    if not tag:
        return text
    return text[:tag.start()] + line + text[tag.start():]


# --------------------------------------------------------------------------- one function
def crank(va, budget, commit, ledger):
    row = report_row(va)
    if not row or row['status'] != 'diff':
        log('%s: not a diff row in report.csv' % va); return None
    path, sym = row['file'], row['name']
    if dirty(path):
        log('%s %s: %s is dirty in git -- another session has it; skipped' % (va, sym, path)); return None
    text0 = open(os.path.join(ROOT, path), encoding='utf-8', errors='surrogateescape').read()
    tag = '%s' % va[2:].lower()
    key = max(3, min(6, int(row['orig_size']) // 40))
    obj = compile_tu(text0, tag)
    m0 = obj and measure(obj, sym, va, key)
    if not m0:
        log('%s %s: base does not compile / symbol absent -- skipped' % (va, sym)); return None
    if m0['exact']:
        log('%s %s: already exact in this TU (stale report row?) -- skipped' % (va, sym)); return None
    log('=== %s %s  %s  %d B  base: %s' % (va, sym, path, m0['ob'], fmt(m0)))
    best_s, best_t, hist, n, seen = score(m0), text0, [], 1, {hashlib.md5(text0.encode()).hexdigest()}
    while n < budget and best_s[0]:
        improved = False
        for lname, lever in LEVERS:
            for label, cand in lever(best_t, sym):
                h = hashlib.md5(cand.encode()).hexdigest()
                if h in seen:
                    continue
                seen.add(h)
                if n >= budget:
                    break
                obj = compile_tu(cand, tag); n += 1
                m = obj and measure(obj, sym, va, key)
                if not m:
                    log('  %3d %-28s compile/symbol fail' % (n, label)); continue
                s = score(m)
                mark = ' <-- accept' if s < best_s else ''
                log('  %3d %-28s %s%s' % (n, label, fmt(m), mark))
                if s < best_s:
                    best_s, best_t, improved = s, cand, True
                    hist.append(label)
                    break
            if improved:
                break
        if not improved:
            break
    m_best = None
    obj = compile_tu(best_t, tag); m_best = measure(obj, sym, va, key) if obj else m0
    if best_s[0] == 0:
        log('*** %s %s BYTE-EXACT after %d compiles: %s' % (va, sym, n, ' > '.join(hist)))
        if dirty(path):
            log('  %s went dirty during the run; result in build/ghidra_work/%s.crank.c' % (path, va))
            open(os.path.join(ROOT, 'build', 'ghidra_work', va + '.crank.c'), 'w').write(best_t); return 'parked'
        before = file_rows(path)
        open(os.path.join(ROOT, path), 'w', encoding='utf-8', errors='surrogateescape').write(best_t)
        sweep(path)
        after = file_rows(path)
        regress = [v for v, st in before.items() if st == 'match' and after.get(v) != 'match']
        if after.get(va) != 'match' or regress:
            log('  sweep: row=%s regressions=%s -- RESTORED, result in build/ghidra_work/%s.crank.c'
                % (after.get(va), regress, va))
            open(os.path.join(ROOT, path), 'w', encoding='utf-8', errors='surrogateescape').write(text0)
            sweep(path)
            open(os.path.join(ROOT, 'build', 'ghidra_work', va + '.crank.c'), 'w').write(best_t)
            return 'parked'
        log('  sweep: match, no regressions in %s' % path)
        if commit:
            msg = '%s %s: byte-exact (tools/crank.py: %s; %d compiles)' % (va, sym, ' > '.join(hist), n)
            r = subprocess.run(['git', 'commit', '-q', '-m', msg, '--', path],
                               cwd=ROOT, capture_output=True, text=True)
            log('  commit: %s' % ('ok' if r.returncode == 0 else r.stdout[-400:] + r.stderr[-400:]))
        return 'match'
    log('--- %s %s: no byte-exact in %d compiles; best %s (%s)' % (va, sym, n, fmt(m_best), ' > '.join(hist) or 'base'))
    if hist:
        open(os.path.join(ROOT, 'build', 'ghidra_work', va + '.crank.c'), 'w').write(best_t)
    if ledger and n >= 10 and not dirty(path):
        t2 = ledger_line(text0, va, sym, n, m_best)
        if t2 != text0:
            open(os.path.join(ROOT, path), 'w', encoding='utf-8', errors='surrogateescape').write(t2)
            return ('ledger', path)
    return 'diff'


def main(argv):
    budget = int(argv[argv.index('--budget') + 1]) if '--budget' in argv else 150
    commit = '--no-commit' not in argv
    ledger = '--no-ledger' not in argv
    vas = [a for a in argv if a.lower().startswith('0x')]
    if not vas:
        out = subprocess.run([PY, 'tools/t4lane.py', '--pool', 'A', '--n', '9999'], cwd=ROOT,
                             capture_output=True, text=True).stdout
        vas = [l.split()[0] for l in out.splitlines() if l.strip().startswith('0x') and 'primary' in l]
        log('crank: %d live Pool A rows from t4lane.py' % len(vas))
    res, ledgers = {}, []
    for va in vas:
        r = crank(va, budget, commit, ledger)
        if isinstance(r, tuple):
            ledgers.append(r[1]); r = 'diff'
        res[va] = r
    if ledgers and commit:
        subprocess.run(['git', 'commit', '-q', '-m', 'crank: @t4-pass ledger lines for %d functions (tools/crank.py)' % len(ledgers),
                        '--'] + sorted(set(ledgers)), cwd=ROOT, capture_output=True, text=True)
    from collections import Counter
    c = Counter(res.values())
    log('crank done: %d functions -- match %d, parked (exact but regressed/dirty) %d, diff %d, skipped %d'
        % (len(res), c['match'], c['parked'], c['diff'], c[None]))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
