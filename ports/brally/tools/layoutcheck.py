#!/usr/bin/env python3
"""Find local record views that disagree with the canonical 64-bit record.

    layoutcheck.py [TU ...]        (default: every core TU)

The decompiled bodies describe the original's objects with local views:
`char pad[0x2AE8]; GameSub *pSub; /* +0x2AE8 */`. On the 64-bit host such a
view is right only if each member sits where the canonical record (the
typed model in ports/brally/include) puts the same original field; a pad sized
from i386 offsets, or a 4-byte stand-in for a pointer, moves it.

Every record member written with an original-offset note (`/* +0xNN */`) is
collected with its real 64-bit offset, from clang's record-layout dump:
the headers' records are the canonical set, each TU's own records are the
views. A view is paired with the canonical record that shares the most
noted offsets (at least MIN_SHARED, and only where the shared notes agree
in size class), and every noted member whose real offset differs from the
canonical member at the same original offset is printed:

    file:line  View.member  orig=+0xNN  view=0x..  Canonical.member=0x..

With --raw, every noted member whose real offset differs from its note is
printed instead (no pairing), which for a TU whose objects have no
canonical model is the useful list.
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(__file__))
from ptrarith import FLAGS, ROOT  # noqa: E402

MIN_SHARED = 3
HEAD = re.compile(r'^\s*0 \| (?:class|struct|union) (\S+)')
ROW = re.compile(r'^\s*(\d+)(?::\d+-\d+)? \|( +)(.*\S)\s*$')
DECL = re.compile(r'^\s*(?:typedef\s+)?(class|struct|union)\s+(\w+)?\s*(?::[^{;]*)?\{')
MEMB = re.compile(r'(\w+)\s*(?:\[[^\]]*\]\s*)*;\s*/[*/][^+;]*?\+\s*(0x[0-9A-Fa-f]+)\b')
END = re.compile(r'^\s*\}\s*(\w+)?')


def layouts(path):
    cc = ['clang++', '-std=c++17'] if path.endswith('.cpp') else ['clang', '-std=gnu11']
    src, inp = [path], None
    if path.endswith('.h'):
        src, inp = ['-x', 'c', '-'], '#include "%s"\n' % os.path.basename(path)
    p = subprocess.run(cc + ['-fsyntax-only', '-Xclang', '-fdump-record-layouts-complete'] + FLAGS + src,
                       cwd=ROOT, capture_output=True, text=True, errors='replace', timeout=600, input=inp)
    recs, cur = {}, None
    for ln in p.stdout.split('\n'):
        m = HEAD.match(ln)
        if m:
            cur = recs.setdefault(m.group(1).split('::')[-1], {})
            continue
        m = ROW.match(ln)
        if m and cur is not None and len(m.group(2)) == 3:      # top-level members only
            cur.setdefault(m.group(3).split()[-1], int(m.group(1)))
    return recs


def noted(path):
    """{record: {orig_off: (member, real_off, line)}} for records defined in path"""
    recs = layouts(path)
    out, stack = {}, []
    src = open(os.path.join(ROOT, path), errors='replace').read().split('\n')
    pending = []
    for i, ln in enumerate(src, 1):
        m = DECL.match(ln)
        if m:
            stack.append([m.group(2) or '', []])
        if stack:
            for mm in MEMB.finditer(ln):
                stack[-1][1].append((mm.group(1), int(mm.group(2), 16), i))
        e = END.match(ln)
        if stack and e and not DECL.match(ln):
            name, mems = stack.pop()
            name = name if name in recs else (e.group(1) or name)
            if name in recs:
                d = out.setdefault(name, {})
                for mem, note, line in mems:
                    if mem in recs[name]:
                        d.setdefault(note, (mem, recs[name][mem], line))
    return out


def main():
    os.chdir(ROOT)
    raw = sys.argv[1:2] == ['--raw']
    if raw:
        del sys.argv[1]
    tus = sys.argv[1:] or sorted(
        os.path.join(dp, f) for dp, _, fs in os.walk('ports/brally/src/core') for f in fs if f.endswith(('.c', '.cpp')))
    hdrs = sorted('ports/brally/include/' + f for f in os.listdir('ports/brally/include') if f.endswith('.h'))
    jobs = int(os.environ.get('JOBS', '12'))
    canon = {}
    if not raw:
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            for h, res in zip(hdrs, ex.map(noted, hdrs)):
                for rec, d in res.items():
                    canon.setdefault(rec, (h, d))
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        for tu, res in zip(tus, ex.map(noted, tus)):
            for rec, d in sorted(res.items()):
                if raw:
                    for note, (mem, real, line) in sorted(d.items()):
                        if note != real:
                            print('%s:%d  %s.%s orig=+%#x real=%#x' % (tu, line, rec, mem, note, real))
                    continue
                if rec in canon:
                    continue        # the canonical record itself, used directly
                best, shared = None, 0
                for crec, (h, cd) in canon.items():
                    n = len(set(d) & set(cd))
                    if n > shared:
                        best, shared = crec, n
                if shared < min(MIN_SHARED, len(d)) or shared == 0:
                    continue
                cd = canon[best][1]
                for note, (mem, real, line) in sorted(d.items()):
                    if note in cd and cd[note][1] != real:
                        print('%s:%d  %s.%s orig=+%#x view=%#x  %s.%s=%#x' % (
                            tu, line, rec, mem, note, real, best, cd[note][0], cd[note][1]), flush=True)


if __name__ == '__main__':
    main()
