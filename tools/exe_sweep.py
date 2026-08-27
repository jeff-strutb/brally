#!/usr/bin/env python3
"""Sweep src/exe/**/*.c against the three in-scope EXE orig bins.

ADDITIVE. Does not touch build/match/report.csv or the DLL sweep. Each EXE
has its own CRT linkage, so the cl flags are per-binary:

    BRally.exe    /MD   (MSVCRT.dll, CRT calls are FF 15)
    SetVideo.exe  /ML   (static libc, CRT calls are E8)
    BossRally.exe /MT   (static libcmt, CRT calls are E8)

@implements tags are `0xVA <exe>.exe Name` (the `.exe` is load-bearing:
match_sweep.sources() walks all of src/ looking for `@implements`, and
match_diff.parse_implements requires `0xVA word word` — `brally.exe` fails
that parse so a DLL full-sweep cannot score these against BRGlide orig).

Usage:
    python3 tools/exe_sweep.py                  # src/exe, write report_exe.csv
    python3 tools/exe_sweep.py src/exe/brally/0x00401000.c
    python3 tools/exe_sweep.py --ingest-work    # copy score-0 work TUs into src/exe
    python3 tools/exe_sweep.py --summary
"""
from __future__ import print_function

import csv
import io
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ghidra_to_match as g  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build', 'match', 'report_exe.csv')
SRC_EXE = os.path.join(ROOT, 'src', 'exe')

EXES = ('brally', 'setvideo', 'bossrally')
# Per-binary CRT. /O2 first: every verified EXE match was /O2.
CRT = {'brally': '/MD', 'setvideo': '/ML', 'bossrally': '/MT'}
OPT_SHAPES = ('/O2', '/Od', '/O2 /Oy-')

FIELDS = ['exe', 'file', 'va', 'name', 'status', 'opt',
          'orig_size', 'recomp_size', 'diffs']

# `0xVA brally.exe FreeObjList` — the `.exe` keeps parse_implements from
# matching (it wants two \\w+ tokens after the VA).
IMPL_RE = re.compile(
    r'@implements\s+0x([0-9A-Fa-f]+)\s+(\w+)\.exe\s+(\w+)')
FUNC_RE = re.compile(r'\b(\w+)\s*\([^;{]*\)\s*\{')
_NOT_FUNC = frozenset(('if', 'for', 'while', 'switch', 'do', 'catch'))


def orig_dir(exe):
    return os.path.join(ROOT, 'build', 'match', 'orig_%s' % exe)


def opts_for(exe):
    crt = CRT[exe]
    return ['%s %s' % (shape, crt) for shape in OPT_SHAPES]


def func_name(src):
    """First real C function. Strip comments first — a comment like
    `ID_APP_EXIT (0xE141) ...` plus the body's `{` is a false match."""
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
    src = re.sub(r'//[^\n]*', '', src)
    for m in FUNC_RE.finditer(src):
        n = m.group(1)
        if n not in _NOT_FUNC:
            return n
    return None


def parse_implements(src):
    """Return (va_hex, exe, name) from an EXE @implements tag, or None."""
    m = IMPL_RE.search(src)
    if not m:
        return None
    va = '0x%08X' % int(m.group(1), 16)
    return va, m.group(2).lower(), m.group(3)


def with_implements(src, va, exe, name):
    tag = '/* @implements %s %s.exe %s */' % (va, exe, name)
    if IMPL_RE.search(src):
        return src
    needle = '#ifdef BR_MATCHING_BUILD'
    i = src.find(needle)
    if i >= 0:
        j = i + len(needle)
        return src[:j] + '\n' + tag + src[j:]
    return tag + '\n' + src


def _atomic_write(path, text):
    d = os.path.dirname(path)
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.tmp-exe-')
    try:
        with os.fdopen(fd, 'w', newline='') as f:
            f.write(text)
        os.replace(tmp, path)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise


def score_text(src, name, orig, exe, va):
    """(diffs, opt, recomp_len). diffs is None on compile/extract failure."""
    tag = 'ex%s%s' % (exe[0], va[2:])
    nd, opt, rb, _ = g._score_source(src, name, orig, opts_for(exe), tag)
    rlen = len(rb) if rb is not None else 0
    return nd, opt, rlen


def row_for(exe, rel, va, name, orig, nd, opt, rlen):
    row = {'exe': exe, 'file': rel, 'va': va, 'name': name or '',
           'status': '', 'opt': opt or '', 'orig_size': len(orig),
           'recomp_size': rlen, 'diffs': ''}
    if nd is None:
        row['status'] = 'compile_error'
        row['orig_size'] = len(orig)
    elif nd == 0:
        row['status'] = 'match'
        row['diffs'] = 0
    else:
        row['status'] = 'diff'
        row['diffs'] = nd
    return row


def score_src_file(path):
    """Score one src/exe/<exe>/<VA>.c. Returns a report row or None."""
    rel = os.path.relpath(path, ROOT).replace('\\', '/')
    parts = rel.split('/')
    # src/exe/<exe>/<VA>.c
    if len(parts) < 4 or parts[0] != 'src' or parts[1] != 'exe':
        print('skip (not src/exe/...): %s' % rel, flush=True)
        return None
    exe = parts[2]
    if exe not in CRT:
        print('skip (unknown exe %s): %s' % (exe, rel), flush=True)
        return None
    va = os.path.splitext(parts[3])[0]
    src = open(path).read()
    impl = parse_implements(src)
    name = None
    if impl:
        va, tag_exe, name = impl
        if tag_exe != exe:
            print('!! tag exe %s != dir %s in %s' % (tag_exe, exe, rel),
                  flush=True)
    if not name:
        name = func_name(src)
    binp = os.path.join(orig_dir(exe), va + '.bin')
    row = {'exe': exe, 'file': rel, 'va': va, 'name': name or '',
           'status': '', 'opt': '', 'orig_size': '', 'recomp_size': '',
           'diffs': ''}
    if not os.path.exists(binp):
        row['status'] = 'no_orig'
        return row
    orig = open(binp, 'rb').read()
    if not name:
        row['status'] = 'not_in_obj'
        row['orig_size'] = len(orig)
        return row
    nd, opt, rlen = score_text(src, name, orig, exe, va)
    return row_for(exe, rel, va, name, orig, nd, opt, rlen)


def sources(extra=None):
    out = []
    if extra:
        for a in extra:
            p = a if os.path.isabs(a) else os.path.join(ROOT, a)
            if os.path.isfile(p) and p.endswith('.c'):
                out.append(p)
        return out
    if not os.path.isdir(SRC_EXE):
        return []
    for dirpath, _, files in os.walk(SRC_EXE):
        for fn in sorted(files):
            if fn.endswith('.c'):
                out.append(os.path.join(dirpath, fn))
    return sorted(out)


def merge_report(new_rows, swept_files):
    keep = []
    if os.path.exists(REPORT):
        with open(REPORT) as f:
            keep = [r for r in csv.DictReader(f)
                    if r.get('file') not in swept_files
                    and os.path.exists(os.path.join(ROOT, r.get('file') or ''))]
    out = keep + new_rows
    out.sort(key=lambda r: (r.get('exe') or '', r.get('va') or ''))
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=FIELDS, extrasaction='ignore')
    w.writeheader()
    w.writerows(out)
    _atomic_write(REPORT, buf.getvalue())
    return out


def summarise(rows):
    total = len(rows)
    match = [r for r in rows if r['status'] == 'match']
    diff = [r for r in rows if r['status'] == 'diff']
    other = [r for r in rows if r['status'] not in ('match', 'diff')]
    mbytes = sum(int(r['orig_size']) for r in match if r['orig_size'] != '')
    print()
    print('=' * 62)
    by = {}
    for r in match:
        by.setdefault(r['exe'], [0, 0])
        by[r['exe']][0] += 1
        by[r['exe']][1] += int(r['orig_size'])
    for exe in EXES:
        n, b = by.get(exe, [0, 0])
        print('  %-10s %4d fns  %8d B' % (exe, n, b))
    print('  ' + '-' * 42)
    print('  MATCH  %4d / %d tagged functions  (%d bytes)'
          % (len(match), total, mbytes))
    print('  DIFF   %4d' % len(diff))
    for st in sorted(set(r['status'] for r in other)):
        print('  %-6s %4d' % (st.upper()[:6], sum(1 for r in other
                                                  if r['status'] == st)))
    print('=' * 62)
    print('  report: %s' % os.path.relpath(REPORT, ROOT))


def ingest_work():
    """Copy each work .c that scores 0 into src/exe/<exe>/<VA>.c.

    Wall attempts (score != 0, compile failure, no function) stay in the
    work dirs and are not tree-resident.
    """
    rows = []
    copied = skipped = 0
    for exe in EXES:
        work = os.path.join(ROOT, 'build', '%s_work' % exe)
        dest = os.path.join(SRC_EXE, exe)
        os.makedirs(dest, exist_ok=True)
        if not os.path.isdir(work):
            print('no work dir %s' % work, flush=True)
            continue
        files = sorted(fn for fn in os.listdir(work) if fn.endswith('.c'))
        for i, fn in enumerate(files, 1):
            va = fn[:-2]
            src_path = os.path.join(work, fn)
            src = open(src_path).read()
            binp = os.path.join(orig_dir(exe), va + '.bin')
            name = func_name(src)
            rel_dest = 'src/exe/%s/%s' % (exe, fn)
            if not os.path.exists(binp):
                print('[%s %d/%d] %s  no_orig — skip' % (exe, i, len(files), va),
                      flush=True)
                skipped += 1
                continue
            orig = open(binp, 'rb').read()
            if not name:
                print('[%s %d/%d] %s  no function — skip' % (
                    exe, i, len(files), va), flush=True)
                skipped += 1
                continue
            nd, opt, rlen = score_text(src, name, orig, exe, va)
            if nd != 0:
                print('[%s %d/%d] %s %s  diffs=%s — wall, not copied' % (
                    exe, i, len(files), va, name, nd), flush=True)
                skipped += 1
                continue
            tagged = with_implements(src, va, exe, name)
            outp = os.path.join(dest, fn)
            with open(outp, 'w') as f:
                f.write(tagged)
            rows.append(row_for(exe, rel_dest, va, name, orig, nd, opt, rlen))
            copied += 1
            print('[%s %d/%d] %s %s  MATCH %d B %s -> %s' % (
                exe, i, len(files), va, name, len(orig), opt, rel_dest),
                flush=True)
    keep = {r['file'] for r in rows}
    for exe in EXES:
        dest = os.path.join(SRC_EXE, exe)
        if not os.path.isdir(dest):
            continue
        for fn in os.listdir(dest):
            if not fn.endswith('.c'):
                continue
            rel = 'src/exe/%s/%s' % (exe, fn)
            if rel not in keep:
                os.unlink(os.path.join(dest, fn))
    rows.sort(key=lambda r: (r['exe'], r['va']))
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=FIELDS, extrasaction='ignore')
    w.writeheader()
    w.writerows(rows)
    _atomic_write(REPORT, buf.getvalue())
    print('ingest: copied %d score-0, skipped %d walls/empty' % (
        copied, skipped), flush=True)
    summarise(rows)
    return rows


def sweep(paths):
    srcs = sources(paths)
    if not srcs:
        print('no src/exe/**/*.c (run: python3 tools/exe_sweep.py --ingest-work)')
        return []
    all_rows = []
    swept = set()
    for i, src in enumerate(srcs, 1):
        rel = os.path.relpath(src, ROOT)
        swept.add(rel.replace('\\', '/'))
        row = score_src_file(src)
        if row is None:
            continue
        all_rows.append(row)
        st = row['status']
        extra = ''
        if st == 'match':
            extra = '  %s B' % row['orig_size']
        elif st == 'diff':
            extra = '  diffs=%s' % row['diffs']
        print('[%3d/%3d] %-42s %s%s' % (i, len(srcs), rel, st, extra),
              flush=True)
    merged = merge_report(all_rows, swept)
    if paths:
        summarise(all_rows)
        print('  tree total, from report_exe.csv:')
        summarise(merged)
    else:
        summarise(merged)
    return merged


def count_matches():
    """(n, bytes, [(exe, va, bytes), ...]) from report_exe.csv."""
    n = b = 0
    rows = []
    if not os.path.exists(REPORT):
        return n, b, rows
    with open(REPORT) as f:
        for r in csv.DictReader(f):
            if r.get('status') != 'match' or not r.get('orig_size'):
                continue
            n += 1
            nb = int(r['orig_size'])
            b += nb
            rows.append((r.get('exe') or '', r['va'], nb))
    return n, b, rows


def main():
    args, ingest, summary = [], False, False
    for a in sys.argv[1:]:
        if not a.startswith('--'):
            args.append(a)
        elif a == '--ingest-work':
            ingest = True
        elif a == '--summary':
            summary = True
        elif a in ('--help', '-h'):
            print(__doc__)
            return
        else:
            sys.exit('unknown option %s' % a)
    if summary:
        if not os.path.exists(REPORT):
            sys.exit('no %s — run tools/exe_sweep.py first' % REPORT)
        with open(REPORT) as f:
            summarise(list(csv.DictReader(f)))
        return
    if ingest:
        ingest_work()
        return
    sweep(args)


if __name__ == '__main__':
    main()
