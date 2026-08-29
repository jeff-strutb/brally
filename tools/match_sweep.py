#!/usr/bin/env python3
"""Sweep every decomped source file through the original compiler and report
which @implements'd functions come out byte-identical.

Each file is compiled twice -- /O2 (what the release build used for nearly
everything) and /Od (stubs and unoptimised wrappers) -- and each function takes
whichever of the two matches.  Results land in build/match/report.csv so the
next pass can sort the frontier by how close each function is.

THE FULL SWEEP IS BOOKKEEPING, NOT WORK.  To make one function bit-exact you
only need its own file compiled and objdiff'd -- about 11 seconds.  Sweeping all
104 files to fix one function is pure waste; sweep the FILE you are working on.
The whole-tree run exists to produce an accurate total, nothing more.

Results are remembered.  EVERY run merges its rows into report.csv, including a
single-file run -- that is what stops the report drifting out of date and
removes any need to re-baseline.  A full sweep additionally skips files whose
source and headers are unchanged since their cached result.

Usage:
    python3 tools/match_sweep.py                 # sweep everything (cached)
    python3 tools/match_sweep.py src/core/x.c    # sweep one file  <- normal case
    python3 tools/match_sweep.py --summary       # re-print last report
    python3 tools/match_sweep.py --force         # ignore the cache
"""
import csv
import hashlib
import io
import json
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from match_diff import parse_implements, parse_coff_obj  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Go through the wrapper so this uses the repo-local Wine and prefix, exactly
# like build_match.sh does.
WINE = os.path.join(ROOT, 'tools', 'wine.sh')


def find_cl():
    for cand in (os.path.join(ROOT, 'tools', 'msvc5', 'bin', 'cl.exe'),
                 os.path.join(ROOT, 'tools', 'msvc5', 'cl.exe')):
        if os.path.exists(cand):
            return cand
    sys.exit('cl.exe not found under tools/msvc5/ -- run: sh setup.sh')


CL = find_cl()
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
CACHE = os.path.join(ROOT, 'build', 'match', 'sweep_cache.json')

# Ten originals carry a 16-byte link-stage preamble (jmp +0x0b over 11 nops)
# fused into their map entry; the compiler's output starts at +0x10.  The
# preamble bytes are link output — same category as relocs and jmp thunks —
# so they are recorded in config/preambles.csv, verified VERBATIM here, and
# the body is matched in full.  A tagged match still accounts for every
# original byte: preamble verified + body compiler-matched.
def _load_preambles():
    p = os.path.join(ROOT, 'config', 'preambles.csv')
    out = {}
    if os.path.exists(p):
        with open(p) as f:
            for r in csv.DictReader(f):
                out[r['va'].lower()] = bytes.fromhex(r['preamble_hex'])
    return out

PREAMBLES = _load_preambles()

def load_orig(orig_path, va):
    """Original bytes for `va` with any recorded link preamble verified and
    stripped.  Dies loudly if the recorded preamble no longer matches."""
    with open(orig_path, 'rb') as f:
        b = f.read()
    pre = PREAMBLES.get('0x%08x' % va if isinstance(va, int) else str(va).lower())
    if pre is not None:
        if not b.startswith(pre):
            sys.exit('preambles.csv mismatch at %s: recorded %s, bin starts %s'
                     % (va, pre.hex(), b[:len(pre)].hex()))
        return b[len(pre):]
    return b

# O2y = /O2 with frame-pointer omission disabled: a minority of original TUs
# keep push ebp/mov ebp,esp under otherwise full optimisation (first proven
# on BrGameStepIs 0x1002E302; the 0x10031xxx region is the same class).
# O2p = /O2 with precise floating point.  Without /Op, MSVC 5.0 keeps an
# int->float conversion in the x87 register; with it, every such conversion is
# followed by the round-to-float idiom `fstp dword [tmp]; fld dword [tmp]`, and
# `/ 2.0f` stops being strength-reduced to a multiply.  A float-heavy TU that
# the original compiled this way can NEVER match under /O2 alone, so it was
# invisible to every sweep before 2026-08-28.  Found on 0x100215C0, which goes
# from a large multiset gap under /O2 to a single surplus fxch under /O2 /Op.
VARIANTS = [('O2', '/O2'), ('Od', '/Od'), ('O2y', '/O2 /Oy-'),
            ('O2p', '/O2 /Op')]

FIELDS = ['file', 'va', 'name', 'status', 'opt', 'orig_size', 'recomp_size',
          'diffs']


def headers_digest():
    """Hash every header the compile could pull in.

    Deliberately blunt: ANY header edit invalidates every cached file. Headers
    here reach dozens of translation units, so per-file dependency tracking
    would be the only alternative and it is not worth the complexity. During a
    normal matching round the edits are in .c files and this costs nothing.
    """
    h = hashlib.sha256()
    for base in ('include', os.path.join('tools', 'msvc5-compat')):
        d = os.path.join(ROOT, base)
        for dirpath, _, files in os.walk(d):
            for fn in sorted(files):
                p = os.path.join(dirpath, fn)
                h.update(os.path.relpath(p, ROOT).encode())
                try:
                    with open(p, 'rb') as f:
                        h.update(f.read())
                except OSError:
                    pass
    h.update(repr(VARIANTS).encode())
    return h.hexdigest()


def file_key(src, hdigest):
    """Cache key for one source file: its own bytes plus the header digest."""
    h = hashlib.sha256()
    h.update(hdigest.encode())
    with open(src, 'rb') as f:
        h.update(f.read())
    return h.hexdigest()


def load_cache():
    try:
        with open(CACHE) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_cache(cache):
    os.makedirs(os.path.dirname(CACHE), exist_ok=True)
    _atomic_write(CACHE, json.dumps(cache))


def _atomic_write(path, text):
    """Write via a temp file + rename so a killed run cannot truncate the file.

    A previous session lost a whole round of work to files that were written
    non-atomically and then rolled back; the report is cheap to protect.
    """
    d = os.path.dirname(path)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.tmp-')
    try:
        with os.fdopen(fd, 'w', newline='') as f:
            f.write(text)
        os.replace(tmp, path)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise


def merge_report(new_rows, swept_files):
    """Fold this run's rows into report.csv instead of discarding them.

    Single-file runs used to write nothing at all, so every incremental match
    made the report staler and the only cure was a 20-minute full sweep. Now
    each run REPLACES the row set of the files it swept and leaves every other
    file's rows untouched, so the report is always current.

    Replacing per FILE (rather than updating per function) is what makes a
    function that was deleted, renamed or moved out of a file disappear from
    the report instead of lingering as a stale row.

    That covers a file being EDITED, but not one being deleted: a file that no
    longer exists is never swept again, so nothing ever replaces its rows and
    they sit in the report forever.  When br_smallfn.c was split into five
    named modules its 36 rows stayed behind, double-claiming 38 addresses that
    the new modules also claim and inflating the match total by 36.  Rows whose
    source file is gone are therefore dropped on every run.
    """
    keep = []
    if os.path.exists(REPORT):
        with open(REPORT) as f:
            keep = [r for r in csv.DictReader(f)
                    if r.get('file') not in swept_files
                    and os.path.exists(os.path.join(ROOT, r.get('file') or ''))]

    out = keep + new_rows
    # Stable order so the file diffs cleanly between runs.
    out.sort(key=lambda r: (r.get('file') or '', r.get('va') or ''))

    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=FIELDS, extrasaction='ignore')
    w.writeheader()
    w.writerows(out)
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    _atomic_write(REPORT, buf.getvalue())
    return out


def compile_variant(src, tag, opt):
    objdir = os.path.join(ROOT, 'build', 'match', 'obj_' + tag)
    os.makedirs(objdir, exist_ok=True)
    base = os.path.splitext(os.path.basename(src))[0]
    obj = os.path.join(objdir, base + '.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    rel_obj = os.path.relpath(obj, ROOT).replace('/', '\\')
    # cl.exe reads a leading '/' as an option prefix, so a Unix absolute path
    # is parsed as an unknown flag and the compiler reports "missing source
    # filename".  Everything handed to it must be relative to the repo root.
    rel_src = os.path.relpath(src, ROOT)
    # msvc5-compat supplies stdint.h/stdbool.h, which VC5 predates.  It is
    # tracked in git, unlike msvc5/include, which setup.sh re-extracts.
    cmd = ['sh', WINE, CL, '/nologo'] + opt.split() + ['/W3', '/I', 'include',
           '/I', 'tools/msvc5-compat', '/I', 'tools/msvc5/include',
           '/DBR_MATCHING_BUILD', '/c', rel_src, '/Fo' + rel_obj]
    # Wine occasionally wedges on a prefix lock; a stuck cl.exe must not stall
    # the whole sweep, so cap it and report the file as a compile failure.
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           timeout=120)
        out = p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return None, ['cl.exe timed out']
    if not os.path.exists(obj):
        err = [l.strip() for l in out.splitlines() if 'error' in l.lower()]
        return None, err[:3] or ['no obj, no diagnostic']
    return obj, []


def score(orig_bytes, recomp_bytes, relocs):
    """Return (is_match, n_real_diffs, recomp_len)."""
    trimmed = recomp_bytes[:len(orig_bytes)]
    ndiff = sum(1 for i in range(min(len(trimmed), len(orig_bytes)))
                if i not in relocs and trimmed[i] != orig_bytes[i])
    size_ok = len(trimmed) >= len(orig_bytes)
    return (ndiff == 0 and size_ok), ndiff, len(recomp_bytes)


def sweep_file(src, cache=None, fkey=None):
    """Return list of dicts, one per @implements'd function in src."""
    implements = parse_implements(src)
    if not implements:
        return []

    if cache is not None and fkey is not None:
        hit = cache.get(os.path.relpath(src, ROOT))
        base = os.path.splitext(os.path.basename(src))[0]
        have_objs = all(os.path.exists(os.path.join(
            ROOT, 'build', 'match', 'obj_' + tag, base + '.obj'))
            for tag, _ in VARIANTS)
        if hit and hit.get('key') == fkey and have_objs:
            return [dict(r) for r in hit['rows']]

    objs = {}
    errors = {}
    for tag, opt in VARIANTS:
        obj, err = compile_variant(src, tag, opt)
        if obj:
            try:
                objs[tag] = parse_coff_obj(obj)
            except Exception as e:              # malformed obj -- treat as fail
                errors[tag] = [str(e)]
        else:
            errors[tag] = err

    rows = []
    for va, name in implements:
        orig_path = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
        row = {'file': os.path.relpath(src, ROOT), 'va': '0x%08X' % va,
               'name': name, 'status': '', 'opt': '', 'orig_size': '',
               'recomp_size': '', 'diffs': ''}
        if not objs:
            row['status'] = 'compile_error'
            row['diffs'] = '; '.join(next(iter(errors.values()), [])) or 'unknown'
            rows.append(row)
            continue
        if not os.path.exists(orig_path):
            row['status'] = 'no_orig'
            rows.append(row)
            continue

        orig_bytes = load_orig(orig_path, va)
        row['orig_size'] = len(orig_bytes)

        best = None
        for tag, _ in VARIANTS:
            funcs = objs.get(tag)
            if not funcs or name not in funcs:
                continue
            recomp, relocs = funcs[name]
            ok, ndiff, rlen = score(orig_bytes, recomp, relocs)
            # rank: match first, then fewest diffs, then closest size
            key = (0 if ok else 1, ndiff, abs(rlen - len(orig_bytes)))
            if best is None or key < best[0]:
                best = (key, tag, ok, ndiff, rlen)

        if best is None:
            row['status'] = 'not_in_obj'
        else:
            _, tag, ok, ndiff, rlen = best
            row['status'] = 'match' if ok else 'diff'
            row['opt'] = tag
            row['recomp_size'] = rlen
            row['diffs'] = ndiff
        rows.append(row)
    return rows


def sources():
    out = []
    for dirpath, _, files in os.walk(os.path.join(ROOT, 'src')):
        for fn in sorted(files):
            if not fn.endswith('.c'):
                continue
            p = os.path.join(dirpath, fn)
            with open(p, errors='replace') as f:
                if '@implements' in f.read():
                    out.append(p)
    return sorted(out)


def summarise(rows):
    total = len(rows)
    match = [r for r in rows if r['status'] == 'match']
    diff = [r for r in rows if r['status'] == 'diff']
    other = [r for r in rows if r['status'] not in ('match', 'diff')]
    mbytes = sum(int(r['orig_size']) for r in match if r['orig_size'] != '')
    print()
    print('=' * 62)
    print('  MATCH  %4d / %d tagged functions  (%.1f%%, %d bytes)'
          % (len(match), total, 100.0 * len(match) / max(total, 1), mbytes))
    print('  DIFF   %4d' % len(diff))
    for st in sorted(set(r['status'] for r in other)):
        print('  %-6s %4d' % (st.upper()[:6], sum(1 for r in other
                                                  if r['status'] == st)))
    print('=' * 62)


def main():
    # Parse flags EXPLICITLY and reject unknown ones. This used to filter out
    # anything starting with '--' and treat whatever was left as the file list,
    # so a typo -- or `--help` -- left zero arguments and silently launched the
    # 20-minute whole-tree run. Three subagents tripped that in one session.
    args, force = [], False
    for a in sys.argv[1:]:
        if not a.startswith('--'):
            args.append(a)
        elif a == '--summary':
            with open(REPORT) as f:
                summarise(list(csv.DictReader(f)))
            return
        elif a in ('--force', '--no-cache'):
            force = True
        elif a in ('--help', '-h'):
            print(__doc__)
            return
        else:
            sys.exit('unknown option %s (did you mean --summary or --force?)'
                     % a)

    srcs = args or sources()
    hdigest = headers_digest()
    cache = {} if force else load_cache()

    all_rows = []
    for i, src in enumerate(srcs, 1):
        fkey = file_key(src, hdigest)
        rel = os.path.relpath(src, ROOT)
        cached = (not force and cache.get(rel, {}).get('key') == fkey)
        rows = sweep_file(src, cache=None if force else cache, fkey=fkey)
        cache[rel] = {'key': fkey, 'rows': rows}
        all_rows.extend(rows)
        m = sum(1 for r in rows if r['status'] == 'match')
        # A build that will not compile must never look like a build that
        # compiled and matched nothing.
        broke = [r for r in rows if r['status'] == 'compile_error']
        note = '  !! COMPILE ERROR: %s' % broke[0]['diffs'][:70] if broke else ''
        if not note and cached:
            note = '  (cached)'
        print('[%3d/%3d] %-40s %d/%d%s' % (i, len(srcs), rel,
                                           m, len(rows), note), flush=True)

    # EVERY run writes back, single-file runs included. Discarding the rows of
    # a one-file run is what used to make report.csv drift until only a full
    # sweep could fix it.
    merged = merge_report(all_rows, {os.path.relpath(s, ROOT) for s in srcs})
    save_cache(cache)

    # Relearn the global addresses the image can tell us, on EVERY run for the
    # same reason report.csv is rewritten on every run: a map that only some
    # runs refresh is a map nobody can trust the age of. This reads objs, it
    # does not compile, so a one-file sweep pays a second or two for it.
    # It must come after merge_report -- the learner reads report.csv to know
    # which name sits at which address.
    try:
        from reloc_learn import learn_and_write
        res, dropped, wrote = learn_and_write(tuple(t for t, _ in VARIANTS))
        if wrote is None:
            print('  !! learned-address map NOT written: image contradicts '
                  '%d surveyed address(es)' % len(res['wrong']))
        else:
            print('  learned addresses: %d written, %d/%d reproduce a known '
                  'address%s' % (wrote, res['ok'], res['checked'],
                                 ', %d stale obj skipped' % len(dropped)
                                 if dropped else ''))
    except Exception as e:
        print('  !! learned-address map skipped: %s' % e)

    if args:
        # A one-file run should still show where the tree as a whole stands,
        # otherwise the only way to see the real total is the slow run.
        summarise(all_rows)
        print('  tree total, from report.csv:')
    summarise(merged if args else all_rows)


if __name__ == '__main__':
    main()
