#!/usr/bin/env python3
"""Progressive auto-decompiler: transform existing source until it matches.

For each non-matching function, applies known source transforms (remove NULL
checks, change calling conventions, swap types, etc.), compiles via Wine+MSVC5,
and checks the bytes against the original binary.  Matches get written back as
BR_MATCHING_BUILD blocks.  Failures get a learnings record with exactly what
was tried and what the remaining diff looks like.

Usage:
    python3 tools/auto_decomp.py                  # attempt all diff functions
    python3 tools/auto_decomp.py src/core/foo.c   # attempt one file's diffs
    python3 tools/auto_decomp.py --dry-run         # show what would be tried
    python3 tools/auto_decomp.py --report          # print learnings summary
"""
import csv
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from datetime import datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
LEARNINGS = os.path.join(ROOT, 'build', 'match', 'learnings.csv')

import match_diff
import match_sweep

# ---------------------------------------------------------------------------
# Source extraction: find a function's body in the .c file
# ---------------------------------------------------------------------------

def find_function_span(src_text, func_name, va_hex):
    """Find the start/end offsets of a function body in the source.

    Returns (tag_start, body_start, body_end) character offsets, or None.
    tag_start is the @implements comment line.
    body_start is the opening '{'.
    body_end is the matching '}'.
    """
    # Find the @implements tag
    pattern = re.compile(
        r'/\*\s*@implements\s+0x[0-9A-Fa-f]+\s+\w+\s+' + re.escape(func_name) + r'\s*\*/')
    m = pattern.search(src_text)
    if not m:
        return None

    tag_start = m.start()
    # Find the opening brace after the tag
    rest = src_text[m.end():]

    # Skip BR_MATCHING_BUILD blocks if they exist
    # Look for the opening brace of the function definition
    brace_pos = None
    depth = 0
    in_ifdef = False
    i = 0
    while i < len(rest):
        c = rest[i]
        if rest[i:].startswith('#ifdef BR_MATCHING_BUILD'):
            in_ifdef = True
        if rest[i:].startswith('#else') and in_ifdef:
            in_ifdef = False
        if rest[i:].startswith('#endif') and in_ifdef:
            in_ifdef = False

        if not in_ifdef:
            if c == '{' and depth == 0:
                brace_pos = m.end() + i
                break
        i += 1

    if brace_pos is None:
        return None

    # Now find the matching closing brace
    depth = 1
    j = brace_pos + 1
    while j < len(src_text) and depth > 0:
        if src_text[j] == '{':
            depth += 1
        elif src_text[j] == '}':
            depth -= 1
        j += 1

    return (tag_start, brace_pos, j)


def extract_function_body(src_text, func_name, va_hex):
    """Extract just the function body (between { and })."""
    span = find_function_span(src_text, func_name, va_hex)
    if span is None:
        return None
    _, body_start, body_end = span
    return src_text[body_start:body_end]


def extract_function_region(src_text, func_name, va_hex):
    """Extract from @implements tag through closing brace."""
    span = find_function_span(src_text, func_name, va_hex)
    if span is None:
        return None
    tag_start, _, body_end = span
    return src_text[tag_start:body_end]


# ---------------------------------------------------------------------------
# Source transforms
#
# Each transform takes the full source text and function metadata, and returns
# a modified source text (or None if it doesn't apply).
# ---------------------------------------------------------------------------

def transform_remove_null_checks(src_text, func_name, va_hex):
    """Remove if(ptr == NULL) / if(!ptr) / if(ptr != NULL) guard blocks."""
    body = extract_function_body(src_text, func_name, va_hex)
    if body is None:
        return None

    modified = body
    changed = False

    # Pattern: if (X == NULL) return Y;  or  if (X == NULL) { return Y; }
    # Remove the entire guard
    null_patterns = [
        # if (X == NULL) return 0;
        r'if\s*\(\s*(\w+)\s*==\s*NULL\s*\)\s*\{?\s*return\s+[^;]+;\s*\}?',
        # if (X == NULL) { ... return; }
        r'if\s*\(\s*(\w+)\s*==\s*NULL\s*\)\s*\{[^}]*return[^}]*\}',
        # if (!X) return 0;
        r'if\s*\(\s*!\s*(\w+)\s*\)\s*\{?\s*return\s+[^;]+;\s*\}?',
        # if (!X) { ... return; }
        r'if\s*\(\s*!\s*(\w+)\s*\)\s*\{[^}]*return[^}]*\}',
    ]
    for pat in null_patterns:
        new = re.sub(pat, '', modified)
        if new != modified:
            modified = new
            changed = True

    # Pattern: if (X != NULL) { BODY } — unwrap the guard, keep the body
    unwrap_patterns = [
        r'if\s*\(\s*(\w+)\s*!=\s*NULL\s*\)\s*\{',
        r'if\s*\(\s*(\w+)\s*\)\s*\{',  # if (X) {
    ]
    for pat in unwrap_patterns:
        m = re.search(pat, modified)
        if m:
            # Find the matching closing brace and remove the if + braces
            start = m.start()
            brace_start = modified.index('{', start)
            depth = 1
            k = brace_start + 1
            while k < len(modified) and depth > 0:
                if modified[k] == '{': depth += 1
                elif modified[k] == '}': depth -= 1
                k += 1
            # Replace if (...) { BODY } with just BODY
            inner = modified[brace_start + 1:k - 1]
            modified = modified[:start] + inner + modified[k:]
            changed = True

    if not changed:
        return None

    return src_text.replace(body, modified)


def transform_remove_deviation_blocks(src_text, func_name, va_hex):
    """Remove blocks marked with DEVIATION comments."""
    body = extract_function_body(src_text, func_name, va_hex)
    if body is None:
        return None

    # Look for /* DEVIATION */ comments near NULL checks
    if 'DEVIATION' not in body:
        return None

    modified = body
    # Remove lines containing DEVIATION comments and the associated if-block
    lines = modified.split('\n')
    new_lines = []
    skip_depth = 0
    for line in lines:
        stripped = line.strip()
        if 'DEVIATION' in line:
            # Skip this comment line
            continue
        if skip_depth > 0:
            skip_depth += stripped.count('{') - stripped.count('}')
            if skip_depth <= 0:
                skip_depth = 0
            continue
        # Check for NULL check immediately after DEVIATION removal
        if re.match(r'\s*if\s*\(\s*\w+\s*(==\s*NULL|!=\s*NULL)\s*\)', stripped):
            if '{' in stripped:
                skip_depth = 1
            continue
        new_lines.append(line)

    new_body = '\n'.join(new_lines)
    if new_body == modified:
        return None
    return src_text.replace(body, new_body)


# ---------------------------------------------------------------------------
# Compile-and-verify engine
# ---------------------------------------------------------------------------

def compile_and_check(src_path, func_name, va_hex, opt='/O2'):
    """Compile a source file and check one function against the original.

    Returns (is_match, n_diffs, recomp_size) or (None, None, None) on error.
    """
    orig_path = os.path.join(ORIG_DIR, f'{va_hex}.bin')
    if not os.path.exists(orig_path):
        return None, None, None
    with open(orig_path, 'rb') as f:
        orig_bytes = f.read()

    obj, err = match_sweep.compile_variant(src_path, 'auto', opt)
    if err or obj is None:
        return None, None, None

    funcs = match_diff.parse_coff_obj(obj)
    if func_name not in funcs:
        return None, None, None

    recomp_bytes, relocs = funcs[func_name]
    is_match, ndiff, recomp_sz = match_sweep.score(orig_bytes, recomp_bytes, relocs)
    return is_match, ndiff, recomp_sz


def try_transforms(src_path, func_name, va_hex, transforms, dry_run=False):
    """Try each transform on a function, compile, and check.

    Returns (winning_transform_name, modified_source) or (None, None).
    """
    with open(src_path) as f:
        original_src = f.read()

    # First check current state
    if not dry_run:
        is_match, ndiff, _ = compile_and_check(src_path, func_name, va_hex)
        if is_match:
            return ('already_matches', original_src)
        baseline_diffs = ndiff

    for tname, tfunc in transforms:
        modified = tfunc(original_src, func_name, va_hex)
        if modified is None:
            continue  # transform didn't apply

        if dry_run:
            return (tname, modified)  # just report what would be tried

        # Write to a temp file in the same directory (so relative includes work)
        src_dir = os.path.dirname(src_path)
        base = os.path.basename(src_path)
        tmp_path = os.path.join(src_dir, '_auto_' + base)
        try:
            with open(tmp_path, 'w') as f:
                f.write(modified)
            is_match, ndiff, _ = compile_and_check(tmp_path, func_name, va_hex)
            if is_match:
                return (tname, modified)
            # Even if not a match, check if it improved
            if ndiff is not None and baseline_diffs is not None:
                if ndiff < baseline_diffs:
                    # Improved but not matched — try combining with other transforms
                    pass
        finally:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)

    return (None, None)


# All transforms, in order of application
ALL_TRANSFORMS = [
    ('remove_deviation', transform_remove_deviation_blocks),
    ('remove_null_check', transform_remove_null_checks),
]


# ---------------------------------------------------------------------------
# Batch runner
# ---------------------------------------------------------------------------

def load_report():
    with open(REPORT) as f:
        return list(csv.DictReader(f))


LEARNINGS_FIELDS = [
    'name', 'va', 'file', 'opt',
    'orig_size', 'recomp_size', 'size_ratio',
    'result', 'transform', 'description',
    'timestamp',
]


def run(target_file=None, dry_run=False):
    report = load_report()
    diffs = [r for r in report if r['status'] == 'diff']

    if target_file:
        rel = os.path.relpath(os.path.abspath(target_file), ROOT)
        diffs = [r for r in diffs if r['file'] == rel]

    if not diffs:
        print('No diff functions to process.')
        return

    print(f'Processing {len(diffs)} diff functions...')
    if dry_run:
        print('(dry run — showing applicable transforms, not compiling)')

    results = Counter()
    learnings = []

    for idx, row in enumerate(diffs):
        name = row['name']
        va = row['va']
        src_file = row['file']
        src_path = os.path.join(ROOT, src_file)

        if not os.path.exists(src_path):
            continue

        tname, modified = try_transforms(
            src_path, name, va, ALL_TRANSFORMS, dry_run=dry_run)

        if tname == 'already_matches':
            results['already_match'] += 1
            continue

        if tname is not None:
            results[f'fixed:{tname}'] += 1
            if not dry_run:
                # TODO: write the fix back as BR_MATCHING_BUILD block
                print(f'  MATCH  {name:40s} via {tname}')
            else:
                print(f'  WOULD_TRY  {name:40s} via {tname}')
        else:
            results['no_fix'] += 1

        ratio = int(row['recomp_size']) / int(row['orig_size']) if int(row['orig_size']) else 0
        learnings.append({
            'name': name,
            'va': va,
            'file': src_file,
            'opt': row.get('opt', 'O2'),
            'orig_size': row['orig_size'],
            'recomp_size': row['recomp_size'],
            'size_ratio': f'{ratio:.2f}',
            'result': tname or 'no_fix',
            'transform': tname or '',
            'description': '',
            'timestamp': datetime.now().isoformat(timespec='seconds'),
        })

        if (idx + 1) % 20 == 0:
            print(f'  [{idx+1}/{len(diffs)}]')

    # Write learnings
    os.makedirs(os.path.dirname(LEARNINGS), exist_ok=True)
    with open(LEARNINGS, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=LEARNINGS_FIELDS)
        w.writeheader()
        w.writerows(learnings)

    print(f'\n{"=" * 60}')
    print(f'  Results:')
    for k, v in results.most_common():
        print(f'    {k:30s} {v:4d}')
    total = sum(results.values())
    fixed = sum(v for k, v in results.items() if k.startswith('fixed:'))
    print(f'  Fixed: {fixed}/{total}')
    print(f'{"=" * 60}')
    print(f'Learnings: {LEARNINGS}')


def print_report():
    if not os.path.exists(LEARNINGS):
        print('No learnings yet. Run: .venv/bin/python3 tools/auto_decomp.py')
        return
    with open(LEARNINGS) as f:
        rows = list(csv.DictReader(f))
    results = Counter(r['result'] for r in rows)
    print(f'\n{"=" * 60}')
    for k, v in results.most_common():
        print(f'  {k:30s} {v:4d}')
    print(f'{"=" * 60}')


def main():
    if '--report' in sys.argv:
        print_report()
        return

    dry_run = '--dry-run' in sys.argv
    target_file = None
    for arg in sys.argv[1:]:
        if not arg.startswith('-'):
            target_file = arg
            break

    run(target_file=target_file, dry_run=dry_run)


if __name__ == '__main__':
    main()
