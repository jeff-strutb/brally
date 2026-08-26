#!/usr/bin/env python3
"""Auto-file --refine MATCHes into the tree so unattended runs leave
committed, verified matches behind (CLAUDE.md rule 7).

For every ghidra_learnings.csv row with result MATCH whose VA is not yet
@implements-tagged in src/, this tool:
  1. extracts the function + only the support declarations it uses from the
     wrapped TU in build/ghidra_work/ (<va>.refined.c preferred),
  2. picks the slice file whose tagged VA range brackets the address,
  3. inserts the block before the trailing  #endif /* BR_MATCHING_BUILD */,
  4. verifies with the single-file sweep (the ONLY authority: the new row
     must be 'match' and no existing row in that file may regress),
  5. commits that one file — or reverts it and re-sweeps to restore
     report.csv, flagging the function for hand filing.

Anything the extractor cannot prove safe (BrDlCmd emit blocks, NAN bits,
stack0x/dollar-temp hacks, no #endif anchor, name collisions) is flagged to
build/autofile_log.csv, never guessed at.  Runs strictly serially and never
touches include/ (rule 10).

Usage:
    python3 tools/autofile.py                # file every new MATCH
    python3 tools/autofile.py --dry-run      # show what would happen
    python3 tools/autofile.py --no-commit    # file + verify, skip git commit
    python3 tools/autofile.py --va 0x...     # one function
"""
import csv
import os
import re
import subprocess
import sys
from datetime import datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK_DIR = os.path.join(ROOT, 'build', 'ghidra_work')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
LEARNINGS = os.path.join(ROOT, 'build', 'ghidra_learnings.csv')
LOG = os.path.join(ROOT, 'build', 'autofile_log.csv')

# Constructs the extractor refuses to carry into the tree unreviewed.
UNSAFE = ['stack0x', '_dollar_', '_S_T', '_ghidra_nan_bits', 'BrDlCmd',
          'CONCAT', '__m_cNumPods']

FOOTER_RE = re.compile(r'^#endif\s*/\*\s*BR_MATCHING_BUILD\s*\*/\s*$', re.M)


def log_row(rows, va, action, detail):
    rows.append({'va': va, 'action': action, 'detail': detail,
                 'timestamp': datetime.now().isoformat(timespec='seconds')})
    print(f'  {action:12s} {va}  {detail}')


# ---------------------------------------------------------------------------
# Extraction: wrapped TU -> (decl lines, function text) or a refusal reason
# ---------------------------------------------------------------------------

def find_function(tu, name):
    """Return (start, end) of the definition of `name` in tu, or None.
    start is at the beginning of the signature line; end is past the
    closing brace."""
    for m in re.finditer(r'\b%s\s*\(' % re.escape(name), tu):
        # a definition's parameter list is followed by '{' (possibly across
        # blank lines); a call or declaration is not
        depth = 0
        i = m.end() - 1
        while i < len(tu):
            if tu[i] == '(':
                depth += 1
            elif tu[i] == ')':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if i >= len(tu):
            continue
        j = i + 1
        while j < len(tu) and tu[j] in ' \t\r\n':
            j += 1
        if j >= len(tu) or tu[j] != '{':
            continue
        # walk back to the start of the signature: just past the previous
        # ';', '}', '*/' or '#...' line — whichever is nearest
        back = max(tu.rfind(';', 0, m.start()), tu.rfind('}', 0, m.start()),
                   tu.rfind('*/', 0, m.start()) + 1 if
                   tu.rfind('*/', 0, m.start()) >= 0 else -1)
        start = tu.find('\n', back) + 1 if back >= 0 else 0
        end = match_brace(tu, j)
        if end is None:
            continue
        return start, end
    return None


def match_brace(tu, open_idx):
    """Index just past the brace matching tu[open_idx], skipping strings,
    chars, and comments. None if unbalanced."""
    depth = 0
    i = open_idx
    n = len(tu)
    while i < n:
        c = tu[i]
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n and tu[i] != q:
                i += 2 if tu[i] == '\\' else 1
        elif c == '/' and i + 1 < n and tu[i + 1] == '*':
            i = tu.find('*/', i + 2)
            if i < 0:
                return None
            i += 1
        elif c == '/' and i + 1 < n and tu[i + 1] == '/':
            i = tu.find('\n', i)
            if i < 0:
                return None
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return None


DECL_FN = re.compile(r'^(?:extern\s+)?[\w][\w\s\*]*?\b(\w+)\s*\([^;{]*\)\s*;\s*$')
DECL_DATA = re.compile(r'^extern\s+[\w][\w\s]*?[\*\s]\s*\**(\w+)\s*(?:\[[^\]]*\])?\s*;\s*$')


def harvest_decls(wrapper, body):
    """Declaration lines from the wrapper region that the body references."""
    kept = []
    for raw in wrapper.split('\n'):
        line = raw.strip()
        if not line or line.startswith(('#', '/*', '*', '//', 'typedef')):
            continue
        if not line.endswith(';'):
            continue
        m = DECL_FN.match(line) or DECL_DATA.match(line)
        if not m:
            continue
        name = m.group(1)
        if re.search(r'\b%s\b' % re.escape(name), body):
            kept.append(line)
    return kept


def extract(tu, name):
    """Return (decls, func_text, needs_funcptr) or (None, reason, None)."""
    span = find_function(tu, name)
    if span is None:
        return None, 'definition not found in TU', None
    start, end = span
    body = tu[start:end]
    decls = harvest_decls(tu[:start], body)
    blob = '\n'.join(decls) + '\n' + body
    for bad in UNSAFE:
        if bad in blob:
            return None, 'unsafe construct: ' + bad, None
    other_defs = set(re.findall(r'\b(\w+)\s*\([^;{)]*\)\s*\n?\s*\{', tu))
    other_defs -= {name, 'switch', 'if', 'while', 'for', 'return', 'sizeof'}
    for od in other_defs:
        if re.search(r'\b%s\b' % re.escape(od), body):
            return None, 'references sibling definition ' + od, None
    return decls, body, bool(re.search(r'\bfuncptr\b', blob))


# ---------------------------------------------------------------------------
# Placement and verification
# ---------------------------------------------------------------------------

def slice_map(report_rows):
    """slice file -> (min_va, max_va) over its tagged functions."""
    spans = {}
    for r in report_rows:
        f = r['file']
        if not re.match(r'src/core/slice\d+_\d+\.c$', f):
            continue
        va = int(r['va'], 16)
        lo, hi = spans.get(f, (va, va))
        spans[f] = (min(lo, va), max(hi, va))
    return spans


def pick_slice(spans, va):
    inside = [f for f, (lo, hi) in spans.items() if lo <= va <= hi]
    if inside:
        return min(inside, key=lambda f: spans[f][1] - spans[f][0])
    below = [f for f, (lo, hi) in spans.items() if hi < va]
    if below:
        return max(below, key=lambda f: spans[f][1])
    return min(spans, key=lambda f: spans[f][0])


def run_sweep(relfile):
    p = subprocess.run([sys.executable, os.path.join('tools', 'match_sweep.py'),
                        relfile], cwd=ROOT, capture_output=True, text=True,
                       timeout=600)
    return p.returncode == 0, p.stdout + p.stderr


def report_rows_for(relfile):
    if not os.path.exists(REPORT):
        return {}
    with open(REPORT) as f:
        return {r['va'].lower(): r['status'] for r in csv.DictReader(f)
                if r['file'] == relfile}


def file_one(row, report_rows, spans, tagged_vas, dry_run, no_commit, logrows):
    va_hex = row['va']
    va = int(va_hex, 16)
    name = row['name']
    if va_hex.lower() in tagged_vas:
        log_row(logrows, va_hex, 'skip', 'already tagged in tree')
        return False
    src_path = os.path.join(WORK_DIR, va_hex + '.refined.c')
    if not os.path.exists(src_path):
        src_path = os.path.join(WORK_DIR, va_hex + '.c')
    if not os.path.exists(src_path):
        log_row(logrows, va_hex, 'flag', 'no ghidra_work source')
        return False
    tu = open(src_path).read()

    decls, body, needs_funcptr = extract(tu, name)
    if decls is None:
        log_row(logrows, va_hex, 'flag', body)
        return False

    # symbol collision: only a DEFINITION elsewhere collides — an extern
    # callee reference is satisfied, not shadowed, by the new definition
    g = subprocess.run(['git', 'grep', '-l', r'\b%s\b' % name, '--', 'src/'],
                       cwd=ROOT, capture_output=True, text=True)
    defre = re.compile(r'\b%s\s*\([^;{)]*\)\s*\n?\s*\{' % re.escape(name))
    for hit in g.stdout.split():
        if defre.search(open(os.path.join(ROOT, hit)).read()):
            log_row(logrows, va_hex, 'flag',
                    'symbol %s already defined in %s' % (name, hit))
            return False

    relfile = pick_slice(spans, va)
    abs_file = os.path.join(ROOT, relfile)
    text = open(abs_file).read()
    anchors = list(FOOTER_RE.finditer(text))
    if not anchors:
        log_row(logrows, va_hex, 'flag',
                relfile + ' has no BR_MATCHING_BUILD #endif anchor')
        return False
    provenance = (row.get('compile_errors') or '').replace('refined: ', '') \
        or 'none'
    block = '\n'
    if needs_funcptr and 'typedef int (*funcptr)' not in text:
        block += 'typedef int (*funcptr)();\n'
    if decls:
        block += '\n'.join(decls) + '\n'
    block += ('\n/* @implements 0x%08X glide %s */\n'
              '/* auto-filed from ghidra --refine; transforms: %s */\n\n'
              % (va, name, provenance))
    block += body.strip('\n') + '\n\n'

    if dry_run:
        log_row(logrows, va_hex, 'would-file', '%s (+%d decl lines)'
                % (relfile, len(decls)))
        return False

    before = report_rows_for(relfile)
    ins = anchors[-1].start()
    with open(abs_file, 'w') as f:
        f.write(text[:ins] + block + text[ins:])

    ok, out = run_sweep(relfile)
    after = report_rows_for(relfile)
    regressed = [v for v, st in before.items()
                 if st == 'match' and after.get(v) != 'match']
    new_status = after.get(va_hex.lower())
    if not ok or new_status != 'match' or regressed:
        subprocess.run(['git', 'checkout', '--', relfile], cwd=ROOT,
                       check=True)
        run_sweep(relfile)  # restore accurate report rows for the file
        why = ('sweep failed' if not ok else
               'regressed: ' + ','.join(regressed) if regressed else
               'status %s, not match' % new_status)
        log_row(logrows, va_hex, 'verify-fail', why)
        return False

    log_row(logrows, va_hex, 'filed', '%s (%sB, verified match)'
            % (relfile, row['orig_size']))
    if no_commit:
        return True
    msg = ('%s: %s auto-filed for glide %s (%sB) — matched by --refine '
           '(transforms: %s), verified by single-file sweep'
           % (os.path.basename(relfile), name, va_hex, row['orig_size'],
              provenance))
    subprocess.run(['git', 'commit', '-m', msg, '--', relfile], cwd=ROOT,
                   check=True, capture_output=True)
    log_row(logrows, va_hex, 'committed', msg[:70])
    return True


def main():
    dry_run = '--dry-run' in sys.argv
    no_commit = '--no-commit' in sys.argv
    target = None
    for i, a in enumerate(sys.argv):
        if a == '--va' and i + 1 < len(sys.argv):
            target = sys.argv[i + 1].lower()

    with open(LEARNINGS) as f:
        matches = [r for r in csv.DictReader(f) if r['result'] == 'MATCH']
    if target:
        matches = [r for r in matches if r['va'].lower() == target]
    with open(REPORT) as f:
        report_rows = list(csv.DictReader(f))
    tagged_vas = {r['va'].lower() for r in report_rows}
    spans = slice_map(report_rows)

    todo = [r for r in matches if r['va'].lower() not in tagged_vas]
    print('%d MATCH rows, %d not yet in tree' % (len(matches), len(todo)))
    logrows = []
    filed = 0
    for row in todo:                       # strictly serial (rule 10)
        try:
            if file_one(row, report_rows, spans, tagged_vas, dry_run,
                        no_commit, logrows):
                filed += 1
        except Exception as e:
            subprocess.run(['git', 'checkout', '--', 'src/'], cwd=ROOT)
            log_row(logrows, row['va'], 'error', str(e))

    if logrows:
        exists = os.path.exists(LOG)
        with open(LOG, 'a', newline='') as f:
            w = csv.DictWriter(f, fieldnames=['va', 'action', 'detail',
                                              'timestamp'])
            if not exists:
                w.writeheader()
            w.writerows(logrows)
    print('filed %d/%d; log: build/autofile_log.csv' % (filed, len(todo)))


if __name__ == '__main__':
    main()
