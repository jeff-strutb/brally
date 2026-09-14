#!/usr/bin/env python3
"""Build the CRT proven-idiom corpus: compile Microsoft's own CRT source
with our cl.exe and score it against the objects Microsoft shipped.

WHY.  The game corpus (tools/corpus.py) can only prove constructs the game
itself already uses; byte/string-handling shapes that appear nowhere in our
matched code are unprovable from it.  The VC5 CRT is the one body of code
where we hold BOTH sides: the exact source (tools/msvc5/crt/src, vendored
from VCPP-5.00.iso) and the compiler's own output for it (tools/msvc5/lib/
LIBC.LIB).  Every function that recompiles byte-exact is a proof "this C
produced these bytes" with the same standing as a matched game function.

FLAG SETS.  Two independent compiles per source file:
  crt: the flags LIBC.LIB was actually built with, read from the CRT
       makefiles (MAKEFILE:285-292 CC_OPTS_BASE + i386, MAKEFILE.SUB:52
       retail CFLAGS):
         -Zelp8 -W3 -WX -GFy -DWIN32 -GB -Gi- -DWIN32_LEAN_AND_MEAN
         -DNOSERVICE -D_MBCS -D_MB_MAP_DIRECT -D_CRTBLD -O2
  o2:  the game sweep's plain '/O2 /W3' (same preprocessor defines, none of
       the CRT codegen flags), so the corpus records which shapes survive
       under the flag set our matched code is proven at.

Members whose i386 object came from a .ASM under INTEL/ (memcpy, strlen,
the FP dispatch...) have no C to compile and are recorded no_source; a .c
of the same name scoring DIFF against an assembler-built object would be
meaningless, not a near-miss.

    .venv/bin/python tools/crtcorpus.py            # full run -> corpus_crt.csv
    .venv/bin/python tools/crtcorpus.py --only atox strtol

Output build/match/corpus_crt.csv: function, source, flags, status, bytes.
Shipped member objects land in build/match/crt/ship/, compiles in
build/match/crt/obj_<flags>/, so corpus.py can index the proven bytes.
"""
import argparse
import collections
import concurrent.futures as cf
import csv
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from crtlib import members  # noqa: E402
from match_diff import parse_coff_obj  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WINE = os.path.join(ROOT, 'tools', 'wine.sh')
MSVC_DIR = os.environ.get('BR_MSVC', os.path.join(ROOT, 'tools', 'msvc5'))
if not os.path.isabs(MSVC_DIR):
    MSVC_DIR = os.path.join(ROOT, MSVC_DIR)
CL = os.path.join(MSVC_DIR, 'bin', 'cl.exe')
SRC = os.path.join(MSVC_DIR, 'crt', 'src')
LIB = os.path.join(MSVC_DIR, 'lib', 'LIBC.LIB')
OUT = os.path.join(ROOT, 'build', 'match', 'corpus_crt.csv')
WORK = os.path.join(ROOT, 'build', 'match', 'crt')

# Preprocessor state is identical in both sets -- the point of 'o2' is to
# isolate the CODEGEN flags, not to compile different code.
DEFS = ('-DWIN32 -DWIN32_LEAN_AND_MEAN -DNOSERVICE '
        '-D_MBCS -D_MB_MAP_DIRECT -D_CRTBLD')
FLAGSETS = {
    'crt': '-Zelp8 -W3 -WX -GFy -GB -Gi- -O2 ' + DEFS,
    'o2': '/O2 /W3 ' + DEFS,
}


def source_map():
    """basename ('atox') -> repo-relative .c path, top level then INTEL."""
    m = {}
    for sub in ('', 'INTEL'):
        d = os.path.join(SRC, sub)
        for fn in sorted(os.listdir(d)):
            if fn.upper().endswith('.C'):
                key = fn[:-2].lower()
                m.setdefault(key, os.path.relpath(os.path.join(d, fn), ROOT))
    return m


def compile_one(src_rel, flags, tag):
    objdir = os.path.join(WORK, 'obj_' + tag)
    os.makedirs(objdir, exist_ok=True)
    base = os.path.splitext(os.path.basename(src_rel))[0].lower()
    obj = os.path.join(objdir, base + '.obj')
    if os.path.exists(obj):
        os.unlink(obj)
    rel_obj = os.path.relpath(obj, ROOT).replace('/', '\\')
    cmd = ['sh', WINE, CL, '-c', '-nologo'] + flags.split() + [
        '-I', os.path.relpath(SRC, ROOT),
        '-I', os.path.join(os.path.relpath(MSVC_DIR, ROOT), 'include'),
        '-Fo' + rel_obj, src_rel]
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           timeout=180)
        out = p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return None, 'timeout'
    if not os.path.exists(obj):
        err = [l.strip() for l in out.splitlines() if 'error' in l.lower()]
        return None, '; '.join(err[:2]) or 'no obj, no diagnostic'
    return obj, ''


def strip_pad(b):
    b = bytearray(b)
    while b and b[-1] in (0xCC, 0x90):
        b.pop()
    return bytes(b)


def score(ship_fn, our_fn):
    ob, orel = ship_fn
    rb, rrel = our_fn
    ob, rb = strip_pad(ob), strip_pad(rb)
    if len(ob) != len(rb):
        return 'len_mismatch', abs(len(ob) - len(rb))
    mask = orel | rrel
    nd = sum(1 for i in range(len(ob)) if i not in mask and ob[i] != rb[i])
    return ('match', 0) if nd == 0 else ('diff', nd)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', nargs='*', help='member basenames to run')
    ap.add_argument('--jobs', type=int, default=6)
    a = ap.parse_args()

    srcs = source_map()
    shipdir = os.path.join(WORK, 'ship')
    os.makedirs(shipdir, exist_ok=True)

    ship = {}          # base -> {fn: (bytes, relocs)}
    no_source = []
    for name, body in members(LIB):
        base = os.path.splitext(name)[0].lower()
        if a.only and base not in a.only:
            continue
        p = os.path.join(shipdir, base + '.obj')
        with open(p, 'wb') as f:
            f.write(body)
        try:
            fns = parse_coff_obj(p)
        except Exception:
            continue
        fns = {k: v for k, v in fns.items() if strip_pad(v[0])}
        if not fns:
            continue
        if base not in srcs:
            no_source.append((base, sorted(fns)))
            continue
        ship[base] = fns

    jobs = [(base, srcs[base], tag, flags)
            for base in sorted(ship) for tag, flags in FLAGSETS.items()]
    results = {}
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = {ex.submit(compile_one, s, fl, tg): (b, tg)
                for b, s, tg, fl in jobs}
        done = 0
        for fu in cf.as_completed(futs):
            b, tg = futs[fu]
            results[(b, tg)] = fu.result()
            done += 1
            if done % 50 == 0:
                print('  %d/%d compiles' % (done, len(futs)), file=sys.stderr)

    rows = []
    for base in sorted(ship):
        for tag in FLAGSETS:
            obj, err = results[(base, tag)]
            ours = {}
            if obj:
                try:
                    ours = parse_coff_obj(obj)
                except Exception:
                    err = err or 'unparsable obj'
            for fn in sorted(ship[base]):
                nbytes = len(strip_pad(ship[base][fn][0]))
                if not obj:
                    st, extra = 'compile_fail', err
                elif fn not in ours:
                    st, extra = 'not_in_obj', ''
                else:
                    st, nd = score(ship[base][fn], ours[fn])
                    extra = str(nd) if st != 'match' else ''
                rows.append({'function': fn, 'source': srcs[base],
                             'flags': tag, 'status': st,
                             'bytes': nbytes, 'detail': extra})
    for base, fns in sorted(no_source):
        for fn in fns:
            rows.append({'function': fn, 'source': 'INTEL asm/prebuilt',
                         'flags': '', 'status': 'no_source',
                         'bytes': '', 'detail': base})

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=['function', 'source', 'flags',
                                          'status', 'bytes', 'detail'])
        w.writeheader()
        w.writerows(rows)

    per = collections.Counter((r['flags'], r['status']) for r in rows
                              if r['flags'])
    total = {tag: sum(v for (t, s), v in per.items() if t == tag)
             for tag in FLAGSETS}
    for tag in FLAGSETS:
        print('%-4s %4d/%d match  (%s)' % (
            tag, per.get((tag, 'match'), 0), total[tag],
            ', '.join('%s %d' % (s, v) for (t, s), v in sorted(per.items())
                      if t == tag and s != 'match')))
    print('no_source: %d functions (asm/prebuilt members)'
          % sum(1 for r in rows if r['status'] == 'no_source'))
    print('written: %s' % os.path.relpath(OUT, ROOT))


if __name__ == '__main__':
    main()
