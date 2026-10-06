#!/usr/bin/env python3
"""Fleet runner for the deterministic C permuter (tools/brally/permute.py).

Cranks the permuter over the near-miss frontier in PARALLEL (one process per
worker slot, each grinding a different function), and BANKS every byte-exact
result into the tree (permute.py only writes a result file; this files it into
the real .c, re-verifies, and commits). See tools/brally/perm.sh.

The permuter's sweet spot is small-diff register/coloring near-misses -- exactly
the functions the closer loops skip as walls -- so this targets those.
"""
import argparse, csv, os, queue, re, subprocess, sys, threading, time
from concurrent.futures import ThreadPoolExecutor, as_completed
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
REPORT = os.path.join(ROOT, 'build', 'brally', 'win32', 'match', 'report.csv')
ORIG = os.path.join(ROOT, 'build', 'brally', 'win32', 'match', 'orig')
FN = os.path.join(ROOT, 'tools', 'brally', 'fnmatch', 'fn.py')
GRAVE = {'0x1000EAF0', '0x10019A70', '0x100250D0'}     # known graveyards
PERMUTE = os.path.join(ROOT, 'tools', 'brally', 'permute.py')
LEDGER = os.path.join(ROOT, 'build', 'brally', 'win32', 'match', 'perm_attempted.csv')
BANK_LOCK = threading.Lock()   # serialize tree edits + git


def sh(*a):
    return subprocess.run(a, cwd=ROOT, capture_output=True, text=True)


def rows():
    with open(REPORT) as f:
        return list(csv.DictReader(f))


def is_eh(va):
    try:
        d = open(os.path.join(ORIG, va + '.bin'), 'rb').read()
    except OSError:
        return True
    txt = ' '.join('%s %s' % (i.mnemonic, i.op_str) for i in list(md.disasm(d, 0))[:6])
    return 'push -1' in txt and 'fs:[0]' in txt



def regnorm_gap(out):
    """(extra, missing) register-blind multiset gap from a --detail run, or None.

    This is the number the whole project ranks by: how many instruction SHAPES
    differ after normalizing away register choice. gap 0 with diffs > 0 = a pure
    register-allocation wall (allocation-only residue) -- no C spelling flips it, so don't grind it.
    """
    m = re.search(r'REGNORM (\d+)\+(\d+)', out)
    return (int(m.group(1)), int(m.group(2))) if m else None


def fn_score(va, detail=False):
    # always compute --detail: the register-blind gap is how we rank progress
    # and detect allocation-only walls; the raw DIFFS count alone is register noise.
    args = [PY, FN, va, '--detail', 'regnorm', '40']
    out = subprocess.run(args, cwd=ROOT, capture_output=True, text=True).stdout
    if 'COMPILE FAILED' in out:
        return {'ok': False, 'out': out}
    m = re.search(r'DIFFS=(\d+)', out)
    if not m:
        return {'ok': False, 'out': out}
    gap = regnorm_gap(out)
    return {'ok': True, 'byte_exact': 'BYTE-EXACT' in out,
            'diffs': int(m.group(1)), 'out': out,
            'gap': gap, 'gapsum': (gap[0] + gap[1]) if gap else None}


def brace_match(lines, start):
    depth = 0; instr = None; incom = False
    for idx in range(start, len(lines)):
        line = lines[idx]; k = 0
        while k < len(line):
            c = line[k]; nxt = line[k + 1] if k + 1 < len(line) else ''
            if incom:
                if c == '*' and nxt == '/':
                    incom = False; k += 2; continue
                k += 1; continue
            if instr:
                if c == '\\':
                    k += 2; continue
                if c == instr:
                    instr = None
                k += 1; continue
            if c == '/' and nxt == '*':
                incom = True; k += 2; continue
            if c == '/' and nxt == '/':
                break
            if c in '"\'':
                instr = c; k += 1; continue
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return idx
            k += 1
    return None


def locate_fn(path, name):
    """Return (start_line, end_line) inclusive of the function DEFINITION, or None."""
    lines = open(path).read().split('\n')
    pat = re.compile(r'^[A-Za-z_].*\b' + re.escape(name) + r'\s*\(')
    for i, l in enumerate(lines):
        if not pat.match(l) or l.rstrip().endswith(';'):
            continue
        j = i
        while j < len(lines) and '{' not in lines[j]:
            if ';' in lines[j]:            # a prototype, not a definition
                j = None; break
            j += 1
        if j is None or j >= len(lines):
            continue
        end = brace_match(lines, j)
        if end is not None:
            return i, end
    return None


def load_attempted():
    s = set()
    if os.path.exists(LEDGER):
        for line in open(LEDGER):
            va = line.split(',')[0].strip()
            if va:
                s.add(va)
    return s


def mark(va, outcome):
    os.makedirs(os.path.dirname(LEDGER), exist_ok=True)
    with open(LEDGER, 'a') as f:
        f.write('%s,%s\n' % (va, outcome))


def save_learning(va, name, before, after, mutation_seq):
    """Save the winning before/after (and the mutation sequence that cracked it)
    so the idiom-merge pass can generalize it into docs/brally/VC5-IDIOMS.md -- turning a
    permuter win into a reusable idiom."""
    try:
        d = os.path.join(ROOT, 'config', 'brally', 'worklists', 'idioms_new')
        os.makedirs(d, exist_ok=True)
        seq = ('\nMUTATION SEQUENCE that cracked it: ' + mutation_seq + '\n') if mutation_seq else ''
        with open(os.path.join(d, 'perm-' + va + '.md'), 'w') as f:
            f.write(f'# {name} ({va}) -- byte-exact via deterministic permuter\n{seq}\n'
                    f'BEFORE:\n```c\n{before}\n```\n\nAFTER (byte-exact):\n```c\n{after}\n```\n')
    except OSError:
        pass


def bank(permuted_path, va, name, rel, mutation_seq=''):
    """Extract the function from the permuter's result and file it into the tree."""
    if not os.path.exists(permuted_path):
        return False
    ploc = locate_fn(permuted_path, name)
    if not ploc:
        return False
    plines = open(permuted_path).read().split('\n')
    newfn = plines[ploc[0]:ploc[1] + 1]
    path = os.path.join(ROOT, rel)
    with BANK_LOCK:
        if sh('git', 'diff', '--quiet', '--', rel).returncode != 0:
            return False                      # tree file dirty; don't clobber
        tloc = locate_fn(path, name)
        if not tloc:
            return False
        tlines = open(path).read().split('\n')
        before = '\n'.join(tlines[tloc[0]:tloc[1] + 1])
        open(path, 'w').write('\n'.join(tlines[:tloc[0]] + newfn + tlines[tloc[1] + 1:]))
        sc = fn_score(va)
        if sc.get('ok') and sc.get('byte_exact'):
            sh('git', 'add', '--', rel)
            sh('git', 'commit', '-q', '-m', f'{name}: byte-exact via permuter ({va})')
            save_learning(va, name, before, '\n'.join(newfn), mutation_seq)
            return True
        sh('git', 'checkout', '--', rel)   # verify failed in tree context; revert
        return False


def work(va, name, rel, slots, secs, iters):
    slot = slots.get()
    try:
        print(f'[{va} {name}] permuting (slot {slot}, up to {secs}s)...')
        out = subprocess.run([PY, PERMUTE, '--va', va, '--worker', str(slot),
                              '--max-seconds', str(secs), '--iters', str(iters)],
                             cwd=ROOT, capture_output=True, text=True)
        txt = out.stdout + out.stderr
        m = re.search(r'byte-exact -> (\S+)', txt)
        if m:
            pf = m.group(1)
            pf = pf if os.path.isabs(pf) else os.path.join(ROOT, pf)
            # permute.py prints "  <va>: mut -> mut -> ..." under "cracked mutation sequences"
            sm = re.search(re.escape(va) + r':\s*([^\n]+)', txt)
            mutation_seq = sm.group(1).strip() if sm else ''
            if bank(pf, va, name, rel, mutation_seq):
                print(f'[{va} {name}] *** BYTE-EXACT -- banked & committed ***')
                mark(va, 'landed'); return True
            print(f'[{va} {name}] permuter matched but bank failed (see {pf})')
            mark(va, 'bankfail'); return False
        best = re.search(r'\b(\d+) diffs\b', txt)
        print(f'[{va} {name}] no match ({best.group(1) if best else "?"} diffs best)')
        mark(va, 'failed'); return False
    finally:
        slots.put(slot)


def candidates(a, attempted):
    out = []
    for r in rows():
        if r.get('status') != 'diff' or r['va'] in GRAVE or r['va'] in attempted:
            continue
        d = int(r['diffs'])
        if not (a.min_diffs <= d <= a.max_diffs):
            continue
        if a.min_size <= int(r['orig_size']) <= a.max_size and not is_eh(r['va']):
            out.append(r)
    out.sort(key=lambda r: int(r['diffs']))     # closest register near-misses first
    return out


def one_pass(a):
    attempted = set() if a.retry else load_attempted()
    cands = candidates(a, attempted)
    print(f'{len(cands)} untried near-miss candidate(s) '
          f'({a.min_diffs}-{a.max_diffs} diffs, {a.min_size}-{a.max_size} B); '
          f'{a.workers} parallel permuters, {a.secs}s each\n')
    if not cands:
        return 0, 0
    cands = cands[:a.max_fns]
    slots = queue.Queue()
    for i in range(a.workers):
        slots.put(i)
    landed = 0
    with ThreadPoolExecutor(max_workers=a.workers) as ex:
        futs = [ex.submit(work, r['va'], r['name'], r['file'], slots, a.secs, a.iters)
                for r in cands]
        for f in as_completed(futs):
            try:
                landed += 1 if f.result() else 0
            except Exception as ex2:
                print('  worker error:', ex2)
    return len(cands), landed


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--workers', type=int, default=5, help='parallel permuter processes')
    ap.add_argument('--secs', type=int, default=420, help='timebox per function (seconds)')
    ap.add_argument('--iters', type=int, default=100000, help='permuter iteration cap per function')
    ap.add_argument('--max-fns', type=int, default=40, help='functions per pass')
    ap.add_argument('--min-diffs', type=int, default=1)
    ap.add_argument('--max-diffs', type=int, default=40, help='permuter sweet spot: small register walls')
    ap.add_argument('--min-size', type=int, default=40)
    ap.add_argument('--max-size', type=int, default=1500)
    ap.add_argument('--forever', action='store_true', help='keep passing until Ctrl-C')
    ap.add_argument('--retry', action='store_true', help='re-attempt ledger-failed functions')
    a = ap.parse_args()
    while True:
        worked, landed = one_pass(a)
        print(f'\npass done: {worked} functions permuted, {landed} byte-exact.')
        if not a.forever:
            break
        if worked == 0:
            print('no untried candidates; sleeping 300s then rescanning (Ctrl-C to stop)...')
            time.sleep(300)


if __name__ == '__main__':
    main()
