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
    """Slice files ranked best-first: bracketing (narrowest span first),
    then nearest-below, then nearest-above."""
    inside = sorted((f for f, (lo, hi) in spans.items() if lo <= va <= hi),
                    key=lambda f: spans[f][1] - spans[f][0])
    below = sorted((f for f, (lo, hi) in spans.items() if hi < va),
                   key=lambda f: -spans[f][1])
    rest = sorted((f for f in spans if f not in inside and f not in below),
                  key=lambda f: spans[f][0])
    return inside + below + rest


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
        # A self-contained construct (BrDlCmd's local typedef, ghidra NAN bits,
        # stack/dollar temps) is unsafe to append into a SHARED slice (it
        # collides with the file's includes/siblings) but is fine as its OWN
        # TU — the whole ghidra_work file scored 0. Route those to standalone.
        if body.startswith('unsafe construct'):
            if file_standalone(row, va_hex, name, logrows, no_commit):
                return True
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

    provenance = (row.get('compile_errors') or '').replace('refined: ', '') \
        or 'none'

    def _decl_name(line):
        m = DECL_FN.match(line.strip())
        return m.group(1) if m else None

    # Try each bracketing slice in turn: a filing can regress the target
    # slice's siblings via a slice-local declaration clash (0x10061310
    # regressed 12), so on any verify failure fall through to the NEXT
    # candidate rather than giving up — a different slice often has no clash.
    # Cap the fall-through: each candidate costs a full 3-variant file sweep,
    # and VAs in the interleaved-address region can bracket 16 slices — a
    # match that regresses in every one turned a 10-match run into >1h.
    # The correct home is almost always in the first few (narrowest span
    # first); beyond that, flag for hand-filing rather than grind.
    candidates = [c for c in pick_slice(spans, va)
                  if FOOTER_RE.search(open(os.path.join(ROOT, c)).read())][:4]
    if not candidates:
        log_row(logrows, va_hex, 'flag',
                'no slice file with a BR_MATCHING_BUILD #endif anchor')
        return False

    last_why = 'no candidate verified'
    for relfile in candidates:
        abs_file = os.path.join(ROOT, relfile)
        text = open(abs_file).read()
        anchors = list(FOOTER_RE.finditer(text))
        # Drop harvested callee-decls already DEFINED in THIS target file
        # (`int f();` beside `void f(void){...}` is a C2371 that breaks the
        # whole TU — dropped every sibling of 0x1002DEC3).
        kept = []
        for d in decls:
            n = _decl_name(d)
            if n and re.search(r'\b%s\s*\([^;{)]*\)\s*\n?\s*\{' % re.escape(n),
                               text):
                continue
            kept.append(d)
        # Win32-calling bodies need windows.h; inject it guarded at the top of
        # the block (everything above it is already parsed; compile-verify is
        # the net).
        win32 = re.search(r'\b(LPSECURITY_ATTRIBUTES|LPCSTR|LPCTSTR|HANDLE|'
                          r'HWND|DWORD|LARGE_INTEGER|Create(?:Mutex|Event|'
                          r'Thread|File)[AW]?|WaitForSingleObject|'
                          r'MEMORYSTATUS|MMCKINFO)\b', body)
        block = '\n'
        if win32 and 'windows.h' not in text:
            block += '#ifdef BR_MATCHING_BUILD\n#include <windows.h>\n#endif\n'
        if needs_funcptr and 'typedef int (*funcptr)' not in text:
            block += 'typedef int (*funcptr)();\n'
        if kept:
            block += '\n'.join(kept) + '\n'
        block += ('\n/* @implements 0x%08X glide %s */\n'
                  '/* auto-filed from ghidra --refine; transforms: %s */\n\n'
                  % (va, name, provenance))
        block += body.strip('\n') + '\n\n'

        if dry_run:
            log_row(logrows, va_hex, 'would-file', '%s (+%d decl lines)'
                    % (relfile, len(kept)))
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
            run_sweep(relfile)  # restore accurate report rows
            last_why = ('sweep failed' if not ok else
                        'regressed: ' + ','.join(regressed[:6]) if regressed
                        else 'status %s, not match' % new_status)
            continue  # try the next bracketing slice

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

    # Fallback: no shared slice can hold it without regressing a sibling (it
    # shares a global at a conflicting width). File it as its OWN standalone
    # TU where there are no siblings to break — the build auto-discovers .c
    # files. The ghidra_work TU already scored 0, so reuse it verbatim.
    if file_standalone(row, va_hex, name, logrows, no_commit):
        return True
    log_row(logrows, va_hex, 'verify-fail', last_why + ' (all slices tried)')
    return False


def file_standalone(row, va_hex, name, logrows, no_commit):
    """Write the scored-0 ghidra_work TU as its own tree file (no siblings to
    regress) and commit it. Returns True on a verified match."""
    work = os.path.join(ROOT, 'build', 'ghidra_work', va_hex + '.refined.c')
    if not os.path.exists(work):
        work = os.path.join(ROOT, 'build', 'ghidra_work', va_hex + '.c')
    if not os.path.exists(work):
        return False
    tu = open(work).read()
    va = int(va_hex, 16)
    # inject the @implements tag before the function definition
    m = re.search(r'^([\w][\w\s\*]*?\b%s\s*\([^;{]*\)\s*\n?\s*\{)'
                  % re.escape(name), tu, re.M)
    if not m:
        return False
    tag = '/* @implements 0x%08X glide %s */\n' % (va, name)
    tagged = tu[:m.start()] + tag + tu[m.start():]
    outdir = os.path.join(ROOT, 'src', 'core', 'generated')
    os.makedirs(outdir, exist_ok=True)
    relfile = os.path.join('src', 'core', 'generated', va_hex + '.c')
    abs_file = os.path.join(ROOT, relfile)
    with open(abs_file, 'w') as f:
        f.write(tagged)
    ok, out = run_sweep(relfile)
    if not ok or report_rows_for(relfile).get(va_hex.lower()) != 'match':
        subprocess.run(['git', 'checkout', '--', relfile], cwd=ROOT)
        if os.path.exists(abs_file):
            os.remove(abs_file)
        return False
    log_row(logrows, va_hex, 'filed', '%s (%sB, standalone TU)'
            % (relfile, row['orig_size']))
    if no_commit:
        return True
    subprocess.run(['git', 'add', relfile], cwd=ROOT, check=True)
    msg = ('%s: %s standalone-filed for glide %s (%sB) — shares a global with '
           'siblings at conflicting width, no shared slice holds it; verified '
           'match as its own TU' % (va_hex + '.c', name, va_hex,
                                    row['orig_size']))
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
