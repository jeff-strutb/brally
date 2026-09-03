#!/usr/bin/env python3
"""Local-LLM propose / CPU-verify loop for byte-exact matching. No API tokens.

The LLM proposes a revised C function; fn.py compiles it under MSVC 5.0 and
byte-diffs it against the original (exact, free). ONLY byte-exact results are
committed -- byte-exact means the bytes are identical to the original, which is
the strongest possible correctness guarantee. Non-exact edits (even ones that
reduce the diff) are reverted at the end of each function, so the tree only ever
gains verified matches. Driven by ai.sh (which bootstraps Ollama + the model).
"""
import argparse, csv, functools, json, os, re, subprocess, sys, time, urllib.request
print = functools.partial(print, flush=True)   # updates appear immediately, piped or not
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
ORIG = os.path.join(ROOT, 'build', 'match', 'orig')
FN = os.path.join(ROOT, 'tools', 'fnmatch', 'fn.py')
PY = os.path.join(ROOT, '.venv', 'bin', 'python')
GRAVE = {'0x1000EAF0', '0x10019A70', '0x100250D0'}     # known graveyards
OLLAMA = 'http://localhost:11434/api/chat'

SYS_PROMPT = """You are reconstructing C source that compiles under MSVC 5.0 (/O2) to BYTE-IDENTICAL x86 machine code as a fixed target. You are given the current C function, the target disassembly, and how the current compiled bytes DIFFER from the target. Propose a revised C function whose compiled bytes move CLOSER to the target (ideally identical).
RULES:
- Keep the same function name.
- Do NOT add NULL checks, bounds checks, or any guard the target lacks -- the original binary has none. Removing such a guard often IS the fix.
- Infer types, signedness, widths, argument order, and evaluation order from the TARGET bytes (e.g. a byte load `mov al,[x]` means an unsigned char; a 16-bit op means a short; `test/jl` means signed).
- Do not invent new logic; match what the target bytes actually do.
Return ONLY the revised function (return type + signature + body) inside ONE ```c code block. No prose."""


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


def disasm(va, cap=400):
    d = open(os.path.join(ORIG, va + '.bin'), 'rb').read()
    out = []
    for i in md.disasm(d, 0):
        out.append('%4x  %s %s' % (i.address, i.mnemonic, i.op_str))
        if len(out) >= cap:
            out.append('... (truncated)')
            break
    return '\n'.join(out)


def regnorm_gap(out):
    """(extra, missing) register-blind multiset gap from a --detail run, or None.

    This is the number the whole project ranks by: how many instruction SHAPES
    differ after normalizing away register choice. gap 0 with diffs > 0 = a pure
    register-allocation wall (T3a) -- no C spelling flips it, so don't grind it.
    """
    m = re.search(r'REGNORM (\d+)\+(\d+)', out)
    return (int(m.group(1)), int(m.group(2))) if m else None


def fn_score(va, detail=False):
    # always compute --detail: the register-blind gap is how we rank progress
    # and detect T3a walls; the raw DIFFS count alone is register noise.
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


IDIOMS = ''     # phrasebook digest, loaded from docs/VC5-IDIOMS.md in main()
PLAYBOOK = ''   # matching-method digest, loaded from docs/STRUCTURAL-PLAYBOOK.md


def propose(model, name, cur, disa, detail, temperature, history=None):
    sys_msg = SYS_PROMPT
    if PLAYBOOK:
        sys_msg += ('\n\nMATCHING METHOD (the project playbook -- follow this):\n'
                    + PLAYBOOK)
    if IDIOMS:
        sys_msg += ('\n\nPROVEN MSVC 5.0 SOURCE->BYTES IDIOMS (a phrasebook built by '
                    'this project -- use these construct/byte mappings when they apply):\n'
                    + IDIOMS)
    # Feed back the last failed attempt + its divergence so the model REFINES
    # instead of re-guessing blind. This is the single biggest lever the old
    # loop lacked: each iteration was independent, so temperature was its only
    # source of variation.
    hist = ''
    if history:
        prev_src, prev_detail = history
        hist = (f"\n\nYOUR PREVIOUS ATTEMPT compiled but was NOT byte-exact. It was:\n"
                f"```c\n{prev_src}\n```\n"
                f"and it left this register-blind divergence:\n{prev_detail}\n"
                f"Analyze WHY that divergence remained and change your approach; "
                f"do not repeat the same source.\n")
    user = (f"FUNCTION: {name}\n\nCURRENT C SOURCE:\n```c\n{cur}\n```\n\n"
            f"TARGET x86 DISASSEMBLY (MSVC 5.0 /O2, addresses at base 0):\n```\n{disa}\n```\n\n"
            f"HOW THE CURRENT BYTES DIFFER (register-blind shape multiset; "
            f"'+ N ...' = recomp emits N shapes the target does not; "
            f"'- N ...' = target has N the recomp lacks):\n{detail}\n"
            f"{hist}\n"
            f"Return the revised C function in one ```c block.")
    body = json.dumps({'model': model, 'stream': False,
                       'messages': [{'role': 'system', 'content': sys_msg},
                                    {'role': 'user', 'content': user}],
                       # num_ctx must be set explicitly or Ollama defaults to a tiny
                       # window that silently truncates the disasm + idioms.
                       'options': {'temperature': temperature, 'num_ctx': 32768}}).encode()
    req = urllib.request.Request(OLLAMA, body, {'Content-Type': 'application/json'})
    resp = json.loads(urllib.request.urlopen(req, timeout=900).read())
    txt = resp['message']['content']
    m = re.search(r'```(?:c|cpp)?\s*\n(.*?)```', txt, re.S)
    return m.group(1).strip() if m else None


LEDGER = os.path.join(ROOT, 'build', 'match', 'ai_attempted.csv')


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


TEMPS = [0.2, 0.5, 0.8, 1.1]   # cycled across retries for diverse proposals


def save_idiom(va, name, before, after):
    """Save a winning before/after so the idiom merge (SPEC B) can generalize it."""
    try:
        d = os.path.join(ROOT, 'build', 'match', 'idioms_new')
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, 'ai-' + va + '.md'), 'w') as f:
            f.write(f'# {name} ({va}) -- byte-exact via local-LLM loop\n\n'
                    f'BEFORE:\n```c\n{before}\n```\n\nAFTER (byte-exact):\n```c\n{after}\n```\n')
    except OSError:
        pass


def work_function(a, va, name, rel, path, base, size):
    """Run up to --iters LLM attempts on one function. Returns True if it landed.

    Progress is tracked by the register-blind gap (gapsum), not raw DIFFS: raw
    diffs move with register rotation that no source change controls, so ranking
    by them chases noise. A proposal is kept as the working baseline only when it
    shrinks the gap; byte-exact (diffs 0) always wins.
    """
    best_gap = base.get('gapsum')
    print(f'=== {va} {name} [{rel}]  {size} B  baseline DIFFS={base["diffs"]} '
          f'gap={best_gap} ===')
    # T3a screen: gap already 0 but not byte-exact => pure register-allocation
    # wall. The project has proven (permuter 0/95, refine 0/258) no C spelling
    # flips these. Don't spend a single iteration; park it.
    if best_gap == 0:
        print(f'  gap=0 (T3a register-allocation wall) -- no source change wins; skip\n')
        return False
    got = False
    history = None
    for it in range(a.iters):
        lines = open(path).read().split('\n')
        loc = locate_fn(path, name)
        if not loc:
            break
        s, e = loc
        cur = '\n'.join(lines[s:e + 1])
        detail = fn_score(va)['out']
        temp = TEMPS[it % len(TEMPS)]
        print(f'  iter{it}: querying model (temp {temp}) + compiling (~30-90s)...')
        try:
            prop = propose(a.model, name, cur, disasm(va), detail, temp, history)
        except Exception as ex:
            print(f'  iter{it}: LLM error: {ex}')
            break
        if not prop:
            print(f'  iter{it}: no code block in reply')
            continue
        open(path, 'w').write('\n'.join(lines[:s] + prop.split('\n') + lines[e + 1:]))
        sc = fn_score(va)
        if not sc['ok']:
            print(f'  iter{it}: compile failed, revert')
            sh('git', 'checkout', '--', rel); continue
        if sc['byte_exact']:
            sh('git', 'add', '--', rel)
            sh('git', 'commit', '-q', '-m', f'{name}: byte-exact via local-LLM loop ({va})')
            save_idiom(va, name, cur, prop)
            print(f'  iter{it}: *** BYTE-EXACT *** committed')
            got = True; break
        gap = sc.get('gapsum')
        # remember this attempt + its divergence for the next iteration's prompt
        history = (prop, sc['out'])
        if gap is not None and best_gap is not None and gap < best_gap:
            print(f'  iter{it}: gap {best_gap} -> {gap} (DIFFS={sc["diffs"]}; kept as baseline)')
            best_gap = gap
            if gap == 0:
                # structure now matches; only register choice remains. Nothing
                # more the model can do -- stop and revert to clean.
                print(f'  iter{it}: gap hit 0 (structure matched; residual is T3a). Parking.')
                break
        else:
            print(f'  iter{it}: gap {gap} >= {best_gap} (DIFFS={sc["diffs"]}), revert')
            sh('git', 'checkout', '--', rel)
    if not got:
        sh('git', 'checkout', '--', rel)   # discard non-exact edits; tree stays clean
        print(f'  no byte-exact ({name}); reverted to clean\n')
    else:
        print()
    return got


def load_walls():
    """VAs Grok/closers already parked as walls (build/match/walls_log.csv)."""
    p = os.path.join(ROOT, 'build', 'match', 'walls_log.csv')
    s = set()
    if os.path.exists(p):
        for line in open(p):
            va = line.split(',')[0].strip()
            if va.startswith('0x'):
                s.add(va)
    return s


def one_pass(a):
    attempted = set() if a.retry_failed else load_attempted()
    walls = load_walls()
    cands = [r for r in rows() if r.get('status') == 'diff'
             and r['va'] not in GRAVE and r['va'] not in attempted
             and a.min_size <= int(r['orig_size']) <= a.max_size]
    # Fresh (non-wall) functions first, smallest first -- best odds for a local
    # model; known coloring/x87 walls sink to the end but aren't excluded (the
    # loop has already cracked one function that was declared an unfixable wall).
    cands.sort(key=lambda r: (r['va'] in walls, int(r['orig_size'])))
    print(f'{len(cands)} untried candidate(s) in [{a.min_size},{a.max_size}] B '
          f'({sum(1 for r in cands if r["va"] in walls)} of them parked walls, tried last); '
          f'working up to {a.max_fns} with {a.model}\n')
    worked = landed = 0
    for r in cands:
        if worked >= a.max_fns:
            break
        va, name, rel = r['va'], r['name'], r['file']
        path = os.path.join(ROOT, rel)
        if is_eh(va):
            mark(va, 'skip-eh'); continue
        if sh('git', 'diff', '--quiet', '--', rel).returncode != 0:
            print(f'skip (file dirty, another process?): {rel}'); continue   # retry later
        if not locate_fn(path, name):
            print(f'skip (cannot locate source): {name}'); mark(va, 'skip-locate'); continue
        base = fn_score(va)
        if not base['ok']:
            mark(va, 'skip-compile'); continue
        if base.get('byte_exact'):
            mark(va, 'already-match'); continue
        got = work_function(a, va, name, rel, path, base, int(r['orig_size']))
        worked += 1; landed += 1 if got else 0
        mark(va, 'landed' if got else 'failed')
    return worked, landed, len(cands)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--model', default='qwen2.5-coder:32b')
    ap.add_argument('--iters', type=int, default=4, help='LLM attempts per function')
    ap.add_argument('--max-fns', type=int, default=25, help='functions per pass')
    ap.add_argument('--min-size', type=int, default=40)
    ap.add_argument('--max-size', type=int, default=1200)
    ap.add_argument('--forever', action='store_true',
                    help='keep passing over new candidates until you Ctrl-C')
    ap.add_argument('--retry-failed', action='store_true',
                    help='also re-attempt functions the ledger already marked failed')
    a = ap.parse_args()

    global IDIOMS, PLAYBOOK
    try:
        # the method/tier-ladder spec: teaches the model to reshape toward the
        # target's instructions rather than just guess spellings.
        PLAYBOOK = open(os.path.join(ROOT, 'docs', 'STRUCTURAL-PLAYBOOK.md')).read()
        print(f'loaded {len(PLAYBOOK)} chars of the structural playbook into the prompt')
    except OSError:
        print('note: docs/STRUCTURAL-PLAYBOOK.md not found; running without the method spec')
    try:
        doc = open(os.path.join(ROOT, 'docs', 'VC5-IDIOMS.md')).read()
        # Idioms are appended over time, so keep the foundational ones (head) AND
        # the most recently proven ones (tail); drop the middle if it's too big.
        IDIOMS = doc if len(doc) <= 30000 else (doc[:15000] +
                 '\n\n[...older middle idioms omitted...]\n\n' + doc[-15000:])
        print(f'loaded {len(IDIOMS)} chars of the VC5-IDIOMS phrasebook into the prompt')
    except OSError:
        print('note: docs/VC5-IDIOMS.md not found; running without the phrasebook')

    while True:
        worked, landed, total = one_pass(a)
        print(f'pass done: worked {worked}, landed {landed} byte-exact '
              f'(of {total} untried candidate(s)).')
        if not a.forever:
            break
        if worked == 0:
            print('no untried candidates left; sleeping 300s then rescanning '
                  '(Ctrl-C to stop)...')
            time.sleep(300)


if __name__ == '__main__':
    main()
