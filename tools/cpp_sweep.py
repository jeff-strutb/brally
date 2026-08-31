#!/usr/bin/env python3
"""Sweep src/core/cpp/*.cpp through cl /GX and write build/match/report_cpp.csv.

ADDITIVE. Does not touch match_sweep.py, report.csv, or the C obj_* dirs.
Each @implements'd function is scored as four pieces by tools/cpp_score.py
(function .text, FuncInfo, unwind-action .text, handler thunk). status=match
only when all four are 0.

    python3 tools/cpp_sweep.py                      # every src/core/cpp/*.cpp
    python3 tools/cpp_sweep.py src/core/cpp/0x....cpp
    python3 tools/cpp_sweep.py --summary            # reprint last report
"""
from __future__ import print_function

import argparse
import csv
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import cpp_score  # noqa: E402
import match_diff  # noqa: E402
import match_sweep  # noqa: E402

CPP_DIR = os.path.join(ROOT, 'src', 'core', 'cpp')
REPORT = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
FIELDS = list(match_sweep.FIELDS) + ['pieces']

_OPT_TAG = {
    '/O2 /GX /MD': 'O2',
    '/Od /GX /MD': 'Od',
    '/O2 /Oy- /GX /MD': 'O2y',
}


def _opt_tag(opt):
    return _OPT_TAG.get(opt, opt.replace(' /GX /MD', '').replace('/', ''))


def _atomic_write(path, text):
    d = os.path.dirname(path)
    os.makedirs(d, exist_ok=True)
    tmp = path + '.tmp'
    with open(tmp, 'w', newline='') as f:
        f.write(text)
    os.replace(tmp, path)


def sources(paths=None):
    if paths:
        out = []
        for p in paths:
            p = os.path.abspath(p)
            if os.path.isdir(p):
                for fn in sorted(os.listdir(p)):
                    if fn.endswith('.cpp'):
                        out.append(os.path.join(p, fn))
            elif p.endswith('.cpp') and os.path.exists(p):
                out.append(p)
        return out
    if not os.path.isdir(CPP_DIR):
        return []
    out = []
    for fn in sorted(os.listdir(CPP_DIR)):
        if not fn.endswith('.cpp'):
            continue
        p = os.path.join(CPP_DIR, fn)
        with open(p, errors='replace') as f:
            if '@implements' in f.read():
                out.append(p)
    return out


def _pick_funcinfo(oi, fis):
    if not fis:
        return None
    if len(fis) == 1:
        return fis[0]
    if oi:
        for cand in fis:
            if cand.get('maxState') == oi.get('maxState'):
                return cand
    return fis[0]


def _score_unwinds(oi, ri):
    """True iff every orig unwind action .text matches the .obj action."""
    if not oi and not ri:
        return True
    if not oi or not ri:
        return False
    ou = oi.get('unwinds') or []
    ru = ri.get('unwinds') or []
    if len(ou) != len(ru):
        return False
    for a, b in zip(ou, ru):
        act_va = a['action']
        act_path = os.path.join(cpp_score.ORIG_DIR, '0x%08X.bin' % act_va)
        if not os.path.exists(act_path):
            return False
        act_orig = match_sweep.load_orig(act_path, act_va)
        act_rb = b.get('bytes') or b''
        act_relocs = b.get('relocs') or set()
        ok, _nd, _rlen = match_sweep.score(act_orig, act_rb, act_relocs)
        if not ok:
            return False
    return True


def _score_handler(handler, ri):
    if not handler:
        return not ri or not ri.get('handler_bytes')
    if not ri:
        return False
    hpath = os.path.join(cpp_score.ORIG_DIR, '0x%08X.bin' % handler)
    hb = ri.get('handler_bytes') or b''
    hr = ri.get('handler_relocs') or set()
    if not os.path.exists(hpath) or not hb:
        return False
    horig = match_sweep.load_orig(hpath, handler)
    ok, _nd, _rlen = match_sweep.score(horig, hb, hr)
    return ok


def score_one(src_path, va, impl_name):
    """Compile + 4-piece score. Returns a report row dict."""
    rel = os.path.relpath(src_path, ROOT)
    row = {
        'file': rel, 'va': '0x%08X' % va, 'name': impl_name or '',
        'status': '', 'opt': '', 'orig_size': '', 'recomp_size': '',
        'diffs': '', 'pieces': '0/4',
    }
    orig_path = os.path.join(cpp_score.ORIG_DIR, '0x%08X.bin' % va)
    if not os.path.exists(orig_path):
        row['status'] = 'no_orig'
        return row

    orig = cpp_score.load_orig_bytes(va)
    row['orig_size'] = len(orig)
    _impl, cpp_symbol, impl_kind = cpp_score.parse_implements_name(src_path, va)
    name = cpp_symbol or impl_name
    prefer = impl_kind or 'dtor'

    best = None
    last_err = []
    for i, opt in enumerate(cpp_score.DEFAULT_OPTS):
        tag = 'sweep_%08X_%d' % (va, i)
        obj, errs, _out = cpp_score.compile_cpp(src_path, tag, opt)
        if obj is None:
            last_err = errs
            continue
        try:
            funcs = cpp_score.parse_coff_code(obj)
        except Exception as e:
            last_err = [str(e)]
            continue
        found = cpp_score.find_symbol(funcs, name, prefer=prefer)
        if found is None:
            last_err = ['not_in_obj']
            continue
        rb, relocs = funcs[found]
        ok, nd, rlen = match_sweep.score(orig, rb, relocs)
        rec = (0 if ok else 1, nd, abs(rlen - len(orig)), opt, obj, rb)
        if best is None or rec[:3] < best[:3]:
            best = rec
        if ok:
            break

    if best is None:
        row['status'] = 'compile_error' if last_err != ['not_in_obj'] else 'not_in_obj'
        row['diffs'] = '; '.join(last_err) if last_err else 'unknown'
        return row

    _okrank, nd, _ds, opt, obj, rb = best
    row['opt'] = _opt_tag(opt)
    row['recomp_size'] = len(rb)
    row['diffs'] = nd

    pe = cpp_score.load_pe()
    handler, _fi_va, oi = cpp_score.orig_funcinfo(pe, va, orig)
    ri = _pick_funcinfo(oi, cpp_score.obj_funcinfos(obj))
    equal, _why = cpp_score.funcinfo_struct_equal(oi, ri)

    text_ok = nd == 0
    fi_ok = bool(equal)
    uw_ok = _score_unwinds(oi, ri)
    h_ok = _score_handler(handler, ri)
    n_ok = int(text_ok) + int(fi_ok) + int(uw_ok) + int(h_ok)
    row['pieces'] = '%d/4' % n_ok
    row['status'] = 'match' if n_ok == 4 else 'diff'
    return row


def sweep_file(src_path):
    rows = []
    implements = match_diff.parse_implements(src_path)
    if not implements:
        # parse_implements requires \w+ names; fall back to cpp_score's parser.
        with open(src_path) as f:
            text = f.read()
        for m in re.finditer(r'@implements\s+0x([0-9A-Fa-f]+)\s+\w+\s+(\S+)', text):
            implements.append((int(m.group(1), 16), m.group(2)))
    for va, name in implements:
        print('  %s  %s' % ('0x%08X' % va, name), flush=True)
        row = score_one(src_path, va, name)
        print('    %s  opt=%s orig=%s recomp=%s diffs=%s pieces=%s'
              % (row['status'], row['opt'] or '-', row['orig_size'] or '-',
                 row['recomp_size'] or '-', row['diffs'], row['pieces']),
              flush=True)
        rows.append(row)
    return rows


def merge_report(new_rows, swept_files):
    """Replace rows for files this run swept; drop rows whose source is gone."""
    keep = []
    if os.path.exists(REPORT):
        with open(REPORT) as f:
            keep = [r for r in csv.DictReader(f)
                    if r.get('file') not in swept_files
                    and os.path.exists(os.path.join(ROOT, r.get('file') or ''))]
    out = keep + new_rows
    out.sort(key=lambda r: (r.get('file') or '', r.get('va') or ''))
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=FIELDS, extrasaction='ignore')
    w.writeheader()
    w.writerows(out)
    _atomic_write(REPORT, buf.getvalue())
    return out


def summarise(rows):
    match = [r for r in rows if r['status'] == 'match']
    diff = [r for r in rows if r['status'] == 'diff']
    other = [r for r in rows if r['status'] not in ('match', 'diff')]
    mbytes = sum(int(r['orig_size']) for r in match if r['orig_size'] != '')
    print()
    print('=' * 62)
    print('  C++ sweep  %d tagged  %d match  %d diff  %d other'
          % (len(rows), len(match), len(diff), len(other)))
    print('  match bytes  %d  (of 480,853 BRGlide .text; 97,204 C++ EH class)'
          % mbytes)
    print('=' * 62)
    if diff:
        print('  diffs:')
        for r in diff:
            print('    %s  %s  diffs=%s  pieces=%s'
                  % (r['va'], r['name'], r['diffs'], r['pieces']))
    if other:
        for r in other:
            print('    %s  %s  %s' % (r['va'], r['status'], r['name']))


def load_report():
    if not os.path.exists(REPORT):
        print('no %s — run tools/cpp_sweep.py first' % os.path.relpath(REPORT, ROOT))
        return []
    with open(REPORT) as f:
        return list(csv.DictReader(f))


def main():
    ap = argparse.ArgumentParser(
        description='Score src/core/cpp/*.cpp (4-piece) into report_cpp.csv.')
    ap.add_argument('paths', nargs='*', help='.cpp files or a directory')
    ap.add_argument('--summary', action='store_true',
                    help='reprint last report_cpp.csv without compiling')
    args = ap.parse_args()
    if args.summary:
        summarise(load_report())
        return 0

    srcs = sources(args.paths or None)
    if not srcs:
        print('no @implements .cpp under src/core/cpp/')
        merge_report([], set())
        summarise([])
        return 0

    print('cpp_sweep  %d file%s' % (len(srcs), '' if len(srcs) == 1 else 's'),
          flush=True)
    new_rows = []
    swept = set()
    for src in srcs:
        rel = os.path.relpath(src, ROOT)
        print(rel, flush=True)
        swept.add(rel)
        new_rows.extend(sweep_file(src))
    rows = merge_report(new_rows, swept)
    summarise(rows)
    n_fail = sum(1 for r in new_rows if r['status'] != 'match')
    return 1 if n_fail else 0


if __name__ == '__main__':
    sys.exit(main())
