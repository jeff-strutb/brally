#!/usr/bin/env python3
"""Find every local "view" struct in the core and the canonical record it
is a view of.

A view is a struct or class a core file declares for itself whose fields
sit at the original 32-bit offsets of some shared record (often with
`pad` arrays between them).  For each local record this compares its
fields, by i386 offset, size and pointer-ness, with every record the
shared headers define that holds a pointer, and reports the best match.
viewmerge.py then rewrites the accesses.

Output: build/portable/views.csv
  file, view, canon, score (matching fields), fields, conflicts
Usage: viewscan.py [FILE...]
"""
import concurrent.futures
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

ROOT = vm.ROOT
PAD = re.compile(r'^_?(pad|rest|gap|unk|_?[a-z]?[0-9A-Fa-f]{2,5}_?pad)', re.I)


def header_records():
    """Names of records defined in the shared headers."""
    names = set()
    for dp, _, fs in os.walk(os.path.join(ROOT, 'ports/brally/include')):
        for f in fs:
            s = open(os.path.join(dp, f), encoding='latin-1').read()
            names.update(re.findall(r'\b(?:struct|class|union)\s+(\w+)\s*(?::[^{;]*)?\{', s))
            names.update(re.findall(r'\}\s*(\w+)\s*;', s))
    return names


HDR = header_records()


def local_records(text):
    return set(re.findall(r'^\s*(?:typedef\s+)?(?:struct|class|union)\s+(\w+)\s*(?::[^{;]*)?\{', text, re.M))


def has_ptr(recs, name, seen=None):
    seen = seen or set()
    if name in seen:
        return False
    seen.add(name)
    for o, d, t, n in recs.get(name, []):
        if vm.is_ptr(t):
            return True
    return False


def first_ptr(recs, sizes, name, base=0, depth=0):
    """Smallest offset of a pointer anywhere inside record `name`."""
    best = None
    for o, d, t, n in recs.get(name, []):
        if vm.is_ptr(t):
            best = o if best is None else min(best, o)
        b, dims = vm.array_dims(t)
        bn = re_strip(b) if dims else re_strip(t)
        if depth < 4 and bn in recs and bn != name:
            sub = first_ptr(recs, sizes, bn, 0, depth + 1)
            if sub is not None:
                best = o + sub if best is None else min(best, o + sub)
    return best


def re_strip(t):
    return re.sub(r'^(const |struct |union |class )+', '', (t or '').strip())


def score(recs, sizes, view, canon):
    good, bad, total = 0, [], 0
    fp = first_ptr(recs, sizes, canon)
    beyond = 0
    for j in vm.children(recs[view], None):
        o, d, t, n = recs[view][j]
        if not n or PAD.match(n):
            continue
        total += 1
        r = vm.resolve(recs, sizes, canon, o, vm.want_of(t, recs, sizes))
        if r is None:
            bad.append('NOFIT:' + n)
            continue
        path, leaf, rem = r
        if rem:
            continue
        vs, ls = vm.tsize(t, sizes), vm.tsize(leaf, sizes)
        if vm.is_ptr(t) != vm.is_ptr(leaf):
            if vs == 4 or ls == 4:
                bad.append('KIND:%s/%s' % (n, path))
            continue
        if vs == ls or vs is None or ls is None:
            good += 1
            if fp is not None and o > fp:
                beyond += 1
    if beyond == 0:
        return 0, total, bad      # nothing past the canonical's first pointer: layout-neutral
    return good, total, bad


def scan(f):
    rel = os.path.relpath(f, ROOT)
    text = open(f, encoding='latin-1').read()
    loc = local_records(text)
    if not loc:
        return []
    extra = ['-include', 'ports/brally/tools/canon_all.h']
    try:
        vm.load_ptr_typedefs(rel, extra)
        recs, sizes = vm.layouts(rel, extra)
    except Exception as e:  # noqa: BLE001
        return [(rel, '?', 'ERROR ' + str(e)[:60], 0, 0, '')]
    cands = [c for c in recs if c in HDR and c not in loc and has_ptr(recs, c) and sizes.get(c, 0) >= 0x20]
    out = []
    for v in sorted(loc):
        if v not in recs:
            continue
        best = None
        for c in cands:
            g, t, bad = score(recs, sizes, v, c)
            if t == 0:
                continue
            key = (g - 2 * len(bad), g)
            if best is None or key > best[0]:
                best = (key, c, g, t, bad)
        if best and best[2] >= 3 and best[2] >= 0.75 * best[3]:
            out.append((rel, v, best[1], best[2], best[3], ' '.join(best[4])))
        else:
            out.append((rel, v, '', best[2] if best else 0, best[3] if best else 0, ''))
    return out


def main():
    os.chdir(ROOT)
    files = sys.argv[1:]
    if not files:
        for dp, _, fs in os.walk('ports/brally/src/core'):
            files += [os.path.join(dp, x) for x in fs if x.endswith(('.c', '.cpp'))]
    rows = []
    with concurrent.futures.ProcessPoolExecutor(14) as ex:
        for r in ex.map(scan, sorted(files)):
            rows += r
    os.makedirs('build/portable', exist_ok=True)
    with open('build/portable/views.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['file', 'view', 'canon', 'score', 'fields', 'conflicts'])
        w.writerows(rows)
    hit = [r for r in rows if r[2] and not r[2].startswith('ERROR')]
    print('local records %d; matched to a canonical %d' % (len(rows), len(hit)))


if __name__ == '__main__':
    main()
