#!/usr/bin/env python3
"""crank.py -- unattended, DETERMINISTIC lever sweep to byte-exact, in the
function's REAL translation unit, scored register-blind.  Zero tokens.

    .venv/bin/python tools/crank.py                    # every live Pool A row from t4lane.py
    .venv/bin/python tools/crank.py 0x10018A50 ...     # these VAs (must be report.csv rows)
    .venv/bin/python tools/crank.py --budget 200 --no-commit --no-ledger
    .venv/bin/python tools/crank.py --all --max-bytes 1000 --workers 8
    .venv/bin/python tools/crank.py --all --max-bytes 1000 --workers 10 --loop
        # --loop: run forever in rounds -- re-pool, widen the mutation seeds,
        #   raise the budget 1.5x, re-run; only NEW candidates are compiled.
        # --all: EVERY C diff row (not only register-only), register-only first;
        # --workers N: N processes, disjoint functions, learning files shared under
        #   flock, the write/sweep/commit section serialised.  One Wine compile
        #   per process, so throughput scales with cores (14 on this machine).

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

SEEDS = int(os.environ.get('CRANK_SEEDS', '6'))
SEED0 = int(os.environ.get('CRANK_SEED0', '0'))
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
        for seed in range(SEED0, SEED0 + SEEDS):
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
RECORDS = os.path.join(ROOT, 'build', 'match', 'crank_records.csv')   # learned record bases
STATS = os.path.join(ROOT, 'build', 'match', 'crank_stats.json')      # per-lever tries/accepts/exacts
STATE = os.path.join(ROOT, 'build', 'match', 'crank_state.json')      # per-VA tried candidates
REFILE = os.path.join(ROOT, 'build', 'match', 'crank_refile.txt')     # exact in a slice: hand refile


def _json(path, default):
    try:
        import json
        return json.load(open(path))
    except Exception:
        return default


def _save_json(path, obj):
    import json
    json.dump(obj, open(path, 'w'), indent=1, sort_keys=True)


import fcntl, contextlib


@contextlib.contextmanager
def locked(name):
    """Cross-worker lock (flock) on build/match/crank.<name>.lock: the JSON
    learning files are read-modify-write, and the TREE section (write file,
    sweep, commit, filing) must never interleave between workers -- the sweep
    merges report.csv and git shares one index."""
    fd = open(os.path.join(ROOT, 'build', 'match', 'crank.%s.lock' % name), 'w')
    fcntl.flock(fd, fcntl.LOCK_EX)
    try:
        yield
    finally:
        fcntl.flock(fd, fcntl.LOCK_UN); fd.close()


def learned_records():
    """member DAT_ symbol -> (base symbol, displacement), from every samebase accept so far."""
    out = {}
    if os.path.exists(RECORDS):
        for r in csv.DictReader(open(RECORDS)):
            out[r['member']] = (r['base'], int(r['disp'], 16))
    return out


def learn_record(base_sym, base, syms, va):
  with locked('learn'):
    known = learned_records()
    new = [(m, a) for m, a in syms.items() if m not in known]
    if not new:
        return
    fresh = not os.path.exists(RECORDS)
    with open(RECORDS, 'a', newline='') as f:
        w = csv.writer(f)
        if fresh:
            w.writerow(['base', 'member', 'disp', 'learned_from'])
        for m, a in sorted(new, key=lambda x: x[1]):
            w.writerow([base_sym, m, '0x%x' % (a - base), va])
    return


def lever_samebase(text, sym):
    """Ghidra names every record field as its own DAT_ global; the original
    addressed them from ONE symbol (a struct, or an array of them).  VC5 will
    not hoist a load above a store to the SAME base symbol, but freely
    reorders across DISTINCT symbols -- so the Ghidra spelling changes the
    schedule (0x100283C0: store,store,load became load,store,store; the park
    note blamed scheduling and probed three orderings dead).  Group every
    `(char *)&DAT_X + idx` by idx; where two or more symbols share an index,
    re-spell them all from the lowest symbol plus a constant displacement.
    LEARNED: a member symbol seen in an earlier accept is re-spelled from its
    recorded base even when it is the only field this function touches.
    Both `idx + 0xNN` and `0xNN + idx` orders are offered."""
    parts = _fn(text, sym)
    if not parts:
        return
    body = parts['body']
    known = learned_records()
    groups = {}
    for m in SAMEBASE.finditer(body):
        groups.setdefault(m.group(3), {})[m.group(1)] = int(m.group(2), 16)
    for idx, syms in groups.items():
        if len(syms) >= 2:
            base_sym, base = min(syms.items(), key=lambda kv: kv[1])
            disp = {s: a - base for s, a in syms.items()}
            src = 'group'
        else:
            (s, a), = syms.items()
            if s not in known:
                continue
            base_sym, d = known[s]
            base, disp, src = a - d, {s: d}, 'learned'
        for order in ('idx-first', 'disp-first'):
            def rep(m, idx=idx, disp=disp, base_sym=base_sym, order=order):
                if m.group(3) != idx or m.group(1) not in disp:
                    return m.group(0)
                d = disp[m.group(1)]
                if d == 0:
                    return '(char *)&%s + %s' % (base_sym, idx)
                if order == 'idx-first':
                    return '(char *)&%s + %s + 0x%x' % (base_sym, idx, d)
                return '(char *)&%s + 0x%x + %s' % (base_sym, d, idx)
            nb = SAMEBASE.sub(rep, body)
            if nb != body:
                yield 'samebase:%s:%s:%s:%s' % (src, base_sym, idx, order), P._rebuild(parts, nb)


LEVERS = {'samebase': lever_samebase, 'filepos': lever_filepos, 'decl': lever_decl,
          'comm': lever_comm, 'stmt': lever_stmt, 'mut': lever_mut}
PRIOR = ['samebase', 'filepos', 'decl', 'comm', 'stmt', 'mut']


def lever_order():
    """LEARNING (1): levers that have produced exacts or accepts go first.
    Order = exacts desc, accept-rate desc, then the prior.  A lever with no
    history keeps its prior slot relative to the others without history."""
    st = _json(STATS, {})
    def key(name):
        s = st.get(name, {})
        tries = max(1, s.get('tries', 0))
        return (-s.get('exacts', 0), -(s.get('accepts', 0) / tries), PRIOR.index(name))
    return sorted(PRIOR, key=key)


def bump(label, accepted, exact):
  with locked('learn'):
    st = _json(STATS, {})
    for k in (label.split(':')[0], ':'.join(label.split(':')[:2]) if label.startswith('mut:') else None):
        if not k:
            continue
        s = st.setdefault(k, {'tries': 0, 'accepts': 0, 'exacts': 0})
        s['tries'] += 1; s['accepts'] += int(accepted); s['exacts'] += int(exact)
    _save_json(STATS, st)


def mut_rank():
    """LEARNING (1b): inside `mut`, mutations with accepts come first."""
    st = _json(STATS, {})
    def key(item):
        s = st.get('mut:' + item[0], {})
        return (-s.get('exacts', 0), -(s.get('accepts', 0) / max(1, s.get('tries', 0))))
    return key


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


def git(*args):
    return subprocess.run(['git'] + list(args), cwd=ROOT, capture_output=True, text=True)


def is_slice(path):
    return re.search(r'/slice\d+_\d+\.c$', path) is not None


def file_match(va, sym, path):
    """Rule 6 half b: record the module.  filing.py rebuilds config/filing.csv
    from report.csv and is known to DROP rows whose VA another session has
    in flight (memory: filing-py-drops-rows) -- so the rebuild is diffed
    against the previous file and every lost row is put back before commit."""
    fp = os.path.join(ROOT, 'config', 'filing.csv')
    if dirty('config/filing.csv'):
        log('  filing.csv is dirty (another session) -- filing row NOT written; run tools/filing.py by hand')
        return False
    before = list(csv.DictReader(open(fp)))
    subprocess.run([PY, 'tools/filing.py'], cwd=ROOT, capture_output=True, text=True)
    after = list(csv.DictReader(open(fp)))
    have = {r['glide_va'].lower() for r in after}
    lost = [r for r in before if r['glide_va'].lower() not in have]
    rows = sorted(after + lost, key=lambda r: int(r['glide_va'], 16))
    with open(fp, 'w', newline='') as f:
        w = csv.DictWriter(f, ['glide_va', 'name', 'module', 'basis', 'file']); w.writeheader(); w.writerows(rows)
    if lost:
        log('  filing: restored %d rows filing.py dropped (%s)' % (len(lost), ' '.join(r['glide_va'] for r in lost)))
    r = git('commit', '-q', '-m', 'filing: record %s %s (tools/crank.py)' % (va, sym), '--', 'config/filing.csv')
    return r.returncode == 0


def corpus_census(va, obj, sym, key):
    """Gate B's `census yes` is earned, not asserted: query the solved corpus at
    the first masked divergence of the best variant and log the answer."""
    c = [PY, 'tools/divergence.py', obj, 'build/match/orig/%s.bin' % va, sym, '--key', str(key), '--mask-slots']
    out = subprocess.run(c, cwd=ROOT, capture_output=True, text=True).stdout
    m = re.search(r'divergence #1 at orig\+0x([0-9a-f]+)', out)
    if not m:
        return False
    off = m.group(1)
    q = subprocess.run([PY, 'tools/corpus.py', 'find', '--from', va, '--at', '0x' + off, '--len', '8'],
                       cwd=ROOT, capture_output=True, text=True).stdout
    hits = [l for l in q.splitlines() if re.match(r'\s*0x1[0-9A-Fa-f]{7}\b', l)]
    log('  census: corpus at +0x%s (8 insns): %s' % (off, ('%d solved functions emit this run: %s' % (len(hits), '; '.join(h.strip()[:60] for h in hits[:3]))) if hits else 'MISS -- construct not proven anywhere in the tree'))
    return True


def ledger_line(text, va, n, m, census):
    k = len(re.findall(r'@t4-pass\s+' + re.escape(va), text, re.I)) + 1
    line = ('/* @t4-pass %s %d %s probes %d bytes %d insns %d regions %d rows %d census %s  (tools/crank.py) */\n'
            % (va, k, datetime.date.today().isoformat(), n, m['ob'] + m['db'], m['oi'] + m['di'],
               m['regions'], m['reg'], 'yes' if census else 'no'))
    tag = re.search(r'(?m)^[ \t]*/\*\s*@implements\s+' + re.escape(va), text, re.I)
    return text[:tag.start()] + line + text[tag.start():] if tag else text


def certify(va, path, hist, n):
    """Rule 12: the tool decides T3.  Run --qualify; if it emits a tag, fill its
    prose line with what this run measured and paste it above @implements."""
    out = subprocess.run([PY, 'tools/t3.py', '--qualify', va], cwd=ROOT, capture_output=True, text=True).stdout
    gate = [l for l in out.splitlines() if l.startswith('GATE')]
    m = re.search(r'(/\* @t3 .*?\*/)', out, re.S)
    if not m:
        log('  t3: ' + ' | '.join(gate)); return False
    tag = re.sub(r' \* <what the residue is.*?> \*/',
                 ' * residue after tools/crank.py: %d compiles this pass, levers accepted: %s;\n'
                 ' * every candidate and score is in build/match/crank.log.\n'
                 ' * Do not reopen before the end-grind (CLAUDE.md rule 12). */'
                 % (n, ' > '.join(hist) or 'none'), m.group(1), flags=re.S)
    fp = os.path.join(ROOT, path)
    t = open(fp, encoding='utf-8', errors='surrogateescape').read()
    at = re.search(r'(?m)^[ \t]*/\*\s*@implements\s+' + re.escape(va), t, re.I)
    if not at:
        return False
    open(fp, 'w', encoding='utf-8', errors='surrogateescape').write(t[:at.start()] + tag + '\n' + t[at.start():])
    chk = subprocess.run([PY, 'tools/t3.py'], cwd=ROOT, capture_output=True, text=True)
    if chk.returncode != 0:
        log('  t3 validator rejected the tag -- reverted:\n' + chk.stdout[-600:])
        open(fp, 'w', encoding='utf-8', errors='surrogateescape').write(t)
        return False
    log('*** %s T3-CERTIFIED (tools/t3.py --qualify emitted the tag)' % va)
    return True


# --------------------------------------------------------------------------- one function
def crank(va, budget, commit, ledger):
    row = report_row(va)
    if not row or row['status'] != 'diff':
        log('%s: not a diff row in report.csv' % va); return None
    path, sym = row['file'], row['name']
    if dirty(path):
        log('%s %s: %s is dirty in git -- another session has it; skipped' % (va, sym, path)); return None
    fp = os.path.join(ROOT, path)
    text0 = open(fp, encoding='utf-8', errors='surrogateescape').read()
    tag = va[2:].lower()
    key = max(3, min(6, int(row['orig_size']) // 40))
    obj = compile_tu(text0, tag)
    m0 = obj and measure(obj, sym, va, key)
    if not m0:
        log('%s %s: base does not compile / symbol absent -- skipped' % (va, sym)); return None
    if m0['exact']:
        log('%s %s: already exact in this TU (stale report row?) -- skipped' % (va, sym)); return None
    # LEARNING (2): candidates already compiled for this exact file text are not re-compiled
    state = _json(STATE, {})
    fh = hashlib.md5(text0.encode()).hexdigest()
    st = state.get(va, {})
    seen = set(st.get('tried', [])) if st.get('file_hash') == fh else set()
    st = {'file_hash': fh, 'tried': sorted(seen), 'runs': st.get('runs', 0) + 1 if st.get('file_hash') == fh else 1}
    log('=== %s %s  %s  %d B  base: %s   (run %d, %d candidates already tried, lever order %s)'
        % (va, sym, path, m0['ob'], fmt(m0), st['runs'], len(seen), ' '.join(lever_order())))
    best_s, best_t, hist, n = score(m0), text0, [], 1
    seen.add(fh)
    while n < budget and best_s[0]:
        improved = False
        for lname in lever_order():
            gen = LEVERS[lname](best_t, sym)
            if lname == 'mut':
                gen = sorted(gen, key=lambda lc: mut_rank()((lc[0].split(':')[1], None)))
            for label, cand in gen:
                h = hashlib.md5(cand.encode()).hexdigest()
                if h in seen:
                    continue
                seen.add(h)
                if n >= budget:
                    break
                obj = compile_tu(cand, tag); n += 1
                m = obj and measure(obj, sym, va, key)
                if not m:
                    log('  %3d %-40s compile/symbol fail' % (n, label[:40])); bump(label, False, False); continue
                s = score(m)
                acc = s < best_s
                bump(label, acc, m['exact'])
                log('  %3d %-40s %s%s' % (n, label[:40], fmt(m), ' <-- accept' if acc else ''))
                if acc:
                    if label.startswith('samebase:group:'):
                        # LEARNING (4): the per-field symbols this group folded, read from
                        # the PRE-change text, become a known record for every later function
                        _, _, bs, idx, _ = label.split(':')
                        pre = {mm.group(1): int(mm.group(2), 16) for mm in SAMEBASE.finditer(_fn(best_t, sym)['body'])
                               if mm.group(3) == idx}
                        base = int(bs[4:], 16)
                        learn_record(bs, base, {s_: a for s_, a in pre.items() if 0 <= a - base < 0x10000}, va)
                    best_s, best_t, improved = s, cand, True
                    hist.append(label)
                    break
            if improved:
                break
        if not improved:
            break
    st['tried'] = sorted(seen)
    with locked('learn'):
        state = _json(STATE, {}); state[va] = st; _save_json(STATE, state)
    obj = compile_tu(best_t, tag); m_best = measure(obj, sym, va, key) if obj else m0
    with locked('tree'):
        cur = open(fp, encoding='utf-8', errors='surrogateescape').read()
        if hashlib.md5(cur.encode()).hexdigest() != fh:
            # another worker or session changed this TU while we were compiling:
            # our candidates were built on stale text.  Never write over it.
            out = os.path.join(ROOT, 'build', 'ghidra_work', va + '.crank.c')
            open(out, 'w').write(best_t)
            log('--- %s %s: %s changed under this run (%s); result kept in %s; re-run' % (va, sym, path, fmt(m_best), os.path.relpath(out, ROOT)))
            return 'stale'
        if best_s[0] == 0:
            log('*** %s %s BYTE-EXACT after %d compiles: %s' % (va, sym, n, ' > '.join(hist)))
            out = os.path.join(ROOT, 'build', 'ghidra_work', va + '.crank.c')
            if is_slice(path):
                open(out, 'w').write(best_t)
                with open(REFILE, 'a') as f:
                    f.write('%s %s %s -> exact spelling in %s; move it into its module BY HAND (rule 6), sweep both files, commit\n'
                            % (va, sym, path, os.path.relpath(out, ROOT)))
                log('  %s is an address batch: NOT committed there (rule 6 forbids a machine refile). Spelling saved to %s; listed in build/match/crank_refile.txt'
                    % (path, os.path.relpath(out, ROOT)))
                return 'refile'
            if dirty(path):
                open(out, 'w').write(best_t); log('  %s went dirty during the run; result in %s' % (path, out)); return 'parked'
            before = file_rows(path)
            open(fp, 'w', encoding='utf-8', errors='surrogateescape').write(best_t)
            sweep(path)
            after = file_rows(path)
            regress = [v for v, s_ in before.items() if s_ == 'match' and after.get(v) != 'match']
            if after.get(va) != 'match' or regress:
                log('  sweep: row=%s regressions=%s -- RESTORED, result in %s' % (after.get(va), regress, out))
                open(fp, 'w', encoding='utf-8', errors='surrogateescape').write(text0); sweep(path)
                open(out, 'w').write(best_t)
                return 'parked'
            log('  sweep: match, no regressions in %s' % path)
            if commit:
                msg = '%s %s: byte-exact (tools/crank.py: %s; %d compiles)' % (va, sym, ' > '.join(hist), n)
                r = git('commit', '-q', '-m', msg, '--', path)
                log('  commit: %s' % ('ok' if r.returncode == 0 else (r.stdout + r.stderr)[-500:]))
                if r.returncode == 0:
                    file_match(va, sym, path)
            return 'match'
        log('--- %s %s: no byte-exact in %d compiles; best %s (%s)' % (va, sym, n, fmt(m_best), ' > '.join(hist) or 'base'))
        if hist:
            open(os.path.join(ROOT, 'build', 'ghidra_work', va + '.crank.c'), 'w').write(best_t)
        if ledger and n >= 10 and not dirty(path):
            census = corpus_census(va, obj, sym, key) if obj else False
            t2 = ledger_line(text0, va, n, m_best, census)
            if t2 != text0:
                open(fp, 'w', encoding='utf-8', errors='surrogateescape').write(t2)
                cert = certify(va, path, hist, n)
                if commit:
                    r = git('commit', '-q', '-m', '%s %s: @t4-pass ledger line%s (tools/crank.py, %d compiles, best %s)'
                            % (va, sym, ' + @t3 certification' if cert else '', n, fmt(m_best)), '--', path)
                    if r.returncode != 0:
                        log('  commit failed: ' + (r.stdout + r.stderr)[-500:])
                return 't3' if cert else 'diff'
        return 'diff'


def pool(argv):
    mb = int(argv[argv.index('--max-bytes') + 1]) if '--max-bytes' in argv else 400
    if '--all' in argv:
        # every C diff row <= mb, register-only rows first, then by register-blind gap and size
        from triage import measure, _objs
        objs = _objs()
        cpp = {(r.get('va') or '').lower() for r in csv.DictReader(open(os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')))}
        out = []
        for r in csv.DictReader(open(os.path.join(ROOT, 'build', 'match', 'report.csv'))):
            if r['status'] != 'diff' or r['va'].lower() in cpp or int(r['orig_size'] or 0) > mb:
                continue
            m = measure(r['va'].lower(), r['name'], objs)
            out.append((m['reg'] if m else 9999, int(r['orig_size']), r['va']))
        out.sort()
        log('crank: --all: %d C diff rows <= %d B (%d register-only)' % (len(out), mb, sum(1 for o in out if o[0] == 0)))
        return [o[2] for o in out]
    out = subprocess.run([PY, 'tools/t4lane.py', '--pool', 'A', '--n', '9999', '--max-bytes', str(mb)], cwd=ROOT,
                         capture_output=True, text=True).stdout
    vas = [l.split()[0] for l in out.splitlines() if l.strip().startswith('0x') and 'primary' in l]
    log('crank: %d live Pool A rows from t4lane.py' % len(vas))
    return vas


def main(argv):
    budget = int(argv[argv.index('--budget') + 1]) if '--budget' in argv else 150
    commit = '--no-commit' not in argv
    ledger = '--no-ledger' not in argv
    workers = int(argv[argv.index('--workers') + 1]) if '--workers' in argv else 1
    vas = [a for a in argv if a.lower().startswith('0x')]
    if not vas:
        vas = pool(argv)
    if '--loop' in argv and '--worker' not in argv:
        # FOREVER: round r re-pools (matches drop out, new rows appear), widens
        # the mutation seeds, raises the budget, and re-runs; tried candidates
        # are remembered per file text so each round costs only NEW compiles.
        r = 0
        base = [a for a in argv if a != '--loop' and not a.lower().startswith('0x')]
        while True:
            env = dict(os.environ, CRANK_SEEDS=str(6 * (r + 1)), CRANK_SEED0=str(6 * r))
            flags, skip = [], False
            for a in base:
                if skip: skip = False; continue
                if a == '--budget': skip = True; continue
                flags.append(a)
            flags += ['--budget', str(int(budget * (1.5 ** r)))]
            log('crank loop: round %d (seeds %d..%d, budget %d)' % (r, 6 * r, 6 * (r + 1) - 1, int(budget * (1.5 ** r))))
            subprocess.run([PY, 'tools/crank.py'] + flags, cwd=ROOT, env=env)
            r += 1
            if r >= int(argv[argv.index('--rounds') + 1]) if '--rounds' in argv else 1000000:
                return 0
    if workers > 1 and '--worker' not in argv:
        # PARALLEL: N child processes, disjoint VA lists, shared learning files
        # under flock, the tree section serialised by the 'tree' lock.  Each
        # compile is one Wine process, so N workers ~ N x throughput on N cores.
        flags = [a for a in argv if a in ('--no-commit', '--no-ledger')] + ['--budget', str(budget)]
        # all functions of one TU go to ONE worker (their file is shared state)
        byfile, order = {}, []
        for va in vas:
            row = report_row(va); f = row['file'] if row else va
            if f not in byfile:
                byfile[f] = []; order.append(f)
            byfile[f].append(va)
        lanes = [[] for _ in range(workers)]
        for i, f in enumerate(order):
            lanes[min(range(workers), key=lambda k: len(lanes[k]))].extend(byfile[f])
        procs = []
        for k in range(workers):
            mine = lanes[k]
            if not mine:
                continue
            procs.append(subprocess.Popen([PY, 'tools/crank.py', '--worker', str(k)] + flags + mine, cwd=ROOT,
                                          stdout=open(os.path.join(ROOT, 'build', 'match', 'crank_w%d.log' % k), 'w'),
                                          stderr=subprocess.STDOUT))
        log('crank: %d workers over %d functions (per-worker logs build/match/crank_w<k>.log)' % (len(procs), len(vas)))
        for p in procs:
            p.wait()
        if commit:
            subprocess.run([PY, 'tools/corpus.py', 'build'], cwd=ROOT, capture_output=True, text=True)
        tail = open(LOG).read().splitlines()
        from collections import Counter
        c = Counter()
        for l in tail:
            if 'BYTE-EXACT after' in l: c['exact'] += 1
            elif 'no byte-exact in' in l: c['miss'] += 1
        log('crank parallel done: %d functions -- see build/match/crank.log (this run: %d exact lines, %d miss lines in the whole log)'
            % (len(vas), c['exact'], c['miss']))
        return 0
    res = {}
    for va in vas:
        res[va] = crank(va, budget, commit, ledger)
    from collections import Counter
    c = Counter(res.values())
    if c['match'] and commit and '--worker' not in argv:
        subprocess.run([PY, 'tools/corpus.py', 'build'], cwd=ROOT, capture_output=True, text=True)   # LEARNING (3)
    log('crank done: %d functions -- T4 match %d, exact-needs-hand-refile %d, T3 certified %d, parked %d, diff %d, skipped %d'
        % (len(res), c['match'], c['refile'], c['t3'], c['parked'], c['diff'], c[None]))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
