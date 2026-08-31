#!/usr/bin/env python3
"""Fleet runner for the deterministic C permuter (tools/permute.py). No LLM.

Cranks the permuter over the near-miss frontier in PARALLEL (one process per
worker slot, each grinding a different function), and BANKS every byte-exact
result into the tree (permute.py only writes a result file; this files it into
the real .c, re-verifies, and commits). Pure CPU, zero tokens. See perm.sh.

The permuter's sweet spot is small-diff register/coloring near-misses -- exactly
the functions the closer loops skip as walls -- so this targets those.
"""
import argparse, os, queue, re, subprocess, sys, threading, time
from concurrent.futures import ThreadPoolExecutor, as_completed
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ai_loop   # reuse locate_fn, brace_match, fn_score, is_eh, sh, rows, GRAVE

ROOT = ai_loop.ROOT
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
PERMUTE = os.path.join(ROOT, 'tools', 'permute.py')
LEDGER = os.path.join(ROOT, 'build', 'match', 'perm_attempted.csv')
BANK_LOCK = threading.Lock()   # serialize tree edits + git


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
    so the idiom-merge pass can generalize it into docs/VC5-IDIOMS.md -- turning a
    permuter win into a reusable idiom for Grok and the local-LLM loop too."""
    try:
        d = os.path.join(ROOT, 'build', 'match', 'idioms_new')
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
    ploc = ai_loop.locate_fn(permuted_path, name)
    if not ploc:
        return False
    plines = open(permuted_path).read().split('\n')
    newfn = plines[ploc[0]:ploc[1] + 1]
    path = os.path.join(ROOT, rel)
    with BANK_LOCK:
        if ai_loop.sh('git', 'diff', '--quiet', '--', rel).returncode != 0:
            return False                      # tree file dirty; don't clobber
        tloc = ai_loop.locate_fn(path, name)
        if not tloc:
            return False
        tlines = open(path).read().split('\n')
        before = '\n'.join(tlines[tloc[0]:tloc[1] + 1])
        open(path, 'w').write('\n'.join(tlines[:tloc[0]] + newfn + tlines[tloc[1] + 1:]))
        sc = ai_loop.fn_score(va)
        if sc.get('ok') and sc.get('byte_exact'):
            ai_loop.sh('git', 'add', '--', rel)
            ai_loop.sh('git', 'commit', '-q', '-m', f'{name}: byte-exact via permuter ({va})')
            save_learning(va, name, before, '\n'.join(newfn), mutation_seq)
            return True
        ai_loop.sh('git', 'checkout', '--', rel)   # verify failed in tree context; revert
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
    for r in ai_loop.rows():
        if r.get('status') != 'diff' or r['va'] in ai_loop.GRAVE or r['va'] in attempted:
            continue
        d = int(r['diffs'])
        if not (a.min_diffs <= d <= a.max_diffs):
            continue
        if a.min_size <= int(r['orig_size']) <= a.max_size and not ai_loop.is_eh(r['va']):
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
