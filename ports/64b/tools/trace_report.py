#!/usr/bin/env python3
"""Turn the traced build's memory trace into what each source site touched.

Inputs (from BR_TRACE_BUILD=1 ports/macos/wasm/build_wasm.sh and scripted
runs of build/wasm_trace/brally):
  build/wasm_trace/trace_sites.csv   site -> object, code offset, kind, width
  build/wasm_trace/trace.bin         per-site counts, address samples, values
  build/wasm_trace/obj/*.o           line tables (DWARF) of each object
Address maps: ports/64b/types/globals.csv (original globals by address).

Output build/portable/trace/:
  sites.csv     one row per executed load/store: source file:line:col,
                kind, width, count, the objects and offsets it touched,
                how many of its 4-byte values were addresses (and to what)
  objects.csv   per object (a global, or a heap block by its allocating
                line): every offset read or written, its width, whether it
                ever held an address, and the source lines that touch it
                -- the record catalog, measured.

Usage: trace_report.py
"""
import bisect
import collections
import csv
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
LLVM = os.environ.get('BR_WASM_LLVM', '/opt/homebrew/opt/emscripten/libexec/llvm/bin')
NS, NV = 16, 4
REC = struct.Struct('<QIIII%dI%dI%dIBBBB4x' % (NS, NS, NV))   # C pads to 8
AREC = struct.Struct('<QII')


def line_table(obj):
    """[(addr, file, line, col)] sorted, from llvm-dwarfdump --debug-line."""
    p = subprocess.run([os.path.join(LLVM, 'llvm-dwarfdump'), '--debug-line', obj],
                       capture_output=True, text=True)
    rows, dirs, files = [], {}, {}
    cur = None
    for ln in p.stdout.split('\n'):
        m = re.match(r'include_directories\[\s*(\d+)\] = "(.*)"', ln)
        if m:
            dirs[int(m.group(1))] = m.group(2)
            continue
        m = re.match(r'file_names\[\s*(\d+)\]:', ln)
        if m:
            cur = int(m.group(1))
            files[cur] = ['', 0]
            continue
        m = re.match(r'\s+name: "(.*)"', ln)
        if m and cur is not None:
            files[cur][0] = m.group(1)
            continue
        m = re.match(r'\s+dir_index: (\d+)', ln)
        if m and cur is not None:
            files[cur][1] = int(m.group(1))
            continue
        m = re.match(r'0x([0-9a-f]+)\s+(\d+)\s+(\d+)\s+(\d+)', ln)
        if m:
            rows.append((int(m.group(1), 16), int(m.group(4)), int(m.group(2)), int(m.group(3))))
    names = {k: os.path.join(dirs.get(d, ''), n) for k, (n, d) in files.items()}
    rows.sort()
    return [(a, names.get(f, '?'), l, c) for a, f, l, c in rows]


def main():
    os.chdir(ROOT)
    T = 'build/wasm_trace'
    sites = list(csv.DictReader(open(os.path.join(T, 'trace_sites.csv'))))
    data = open(os.path.join(T, 'trace.bin'), 'rb').read()
    n = struct.unpack_from('<I', data, 0)[0]
    assert n == len(sites), 'trace.bin is from another build (%d sites vs %d)' % (n, len(sites))
    recs = [REC.unpack_from(data, 4 + i * REC.size) for i in range(n)]
    off = 4 + n * REC.size
    arecs = [AREC.unpack_from(data, off + i * AREC.size) for i in range(n)]

    # a TU that went through msvc_lax.py was compiled from a preprocessed
    # copy without line markers: its lines map back to the source by text
    lax_cache = {}

    def remap(lc):
        m = re.match(r'build/wasm_trace/lax/(.*)\.i:(\d+):(\d+)$', lc)
        if not m:
            return lc
        src = m.group(1).replace('__', '/')
        if src not in lax_cache:
            ii = open('build/wasm_trace/lax/%s.i' % m.group(1), encoding='latin-1').read().split('\n')
            orig = open(src, encoding='latin-1').read().split('\n') if os.path.exists(src) else []
            idx = collections.defaultdict(list)
            for k2, l2 in enumerate(orig):
                key = re.sub(r'\s+', '', l2)
                if len(key) > 3:
                    idx[key].append(k2 + 1)
            lax_cache[src] = (ii, idx, len(orig))
        ii, idx, norig = lax_cache[src]
        ln = int(m.group(2))
        key = re.sub(r'\s+', '', ii[ln - 1]) if 0 < ln <= len(ii) else ''
        cands = idx.get(key, [])
        if not cands:
            return '%s:?(i%d)' % (src, ln)
        # the candidate nearest the same relative position in the file
        want = ln * norig / max(len(ii), 1)
        best = min(cands, key=lambda c: abs(c - want))
        return '%s:%d:%s' % (src, best, m.group(3))

    # source location of every site
    tables = {}
    loc = {}
    for i, s in enumerate(sites):
        if recs[i][0] == 0 and arecs[i][0] == 0:
            continue
        ob = s['obj']
        if ob not in tables:
            tables[ob] = line_table(ob)
        t = tables[ob]
        k = bisect.bisect_right(t, (int(s['addr']), '￿', 1 << 30, 1 << 30)) - 1
        loc[i] = remap('%s:%d:%d' % (t[k][1], t[k][2], t[k][3])) if k >= 0 else '?'

    # the canonical objects of the 64-bit core (its generated declarations),
    # with their original extent; an address belongs to the SMALLEST object
    # that holds it, and inside a table of records to (index, offset)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import recspec
    import rewrite
    size_of = {}
    for r in csv.DictReader(open('ports/64b/types/globals.csv')):
        if r['va'] and r['size32']:
            size_of[r['name']] = max(size_of.get(r['name'], 0), int(r['size32']))
    canon = []          # (va, end, name, elem record or None, elem size)
    import glob as _g
    for h in _g.glob('ports/64b/include/*.h'):
        for m in re.finditer(r'^extern\s+(?:const\s+)?([\w\s\*]+?)\s*\b(\w+)((?:\[\w*\])*);\s*/\*\s*0x([0-9A-Fa-f]{8})',
                             open(h, encoding='latin-1').read(), re.M):
            ty, nm, dims, va = m.group(1).strip(), m.group(2), m.group(3), int(m.group(4), 16)
            ds = [int(d, 0) for d in re.findall(r'\[(\w+)\]', dims) if re.match(r'(0x[0-9a-fA-F]+|\d+)$', d)]
            rec = re.sub(r'^struct\s+', '', ty)
            esz = None
            if not ty.endswith('*') and rewrite.spec(rec):
                esz = int(rewrite.spec(rec)[0]['size'], 16)
            else:
                rec = None
                try:
                    esz = recspec.size32(ty, [])
                except SystemExit:
                    esz = None
            if esz and ds and len(ds) == len(re.findall(r'\[', dims)):
                total = esz
                for d in ds:
                    total *= d
            elif esz and not dims:
                total = esz
            else:
                total = size_of.get(nm, esz or 4)
            canon.append((va, va + max(total, 1), nm, rec, esz, bool(re.match(r'^(unsigned\s+|signed\s+)?char$|^u?int8_t$', ty))))
    canon.sort()
    # an object's extent never runs past the next address any original
    # code references on its own (a byte array declared huge in one file
    # would otherwise swallow its neighbours)
    refd = sorted({int(r['va'], 16) for r in csv.DictReader(open('build/wasm/sites.csv'))
                   if r['addend'] == '0' and 0x10077000 <= int(r['va'], 16) < 0x118F2000})
    capped = []
    for va, end, nm, rec, esz, isbytes in canon:
        k = bisect.bisect_right(refd, va)
        nxt = refd[k] if k < len(refd) else end
        if isbytes and end > nxt and va < nxt:
            end = nxt
        capped.append((va, end, nm, rec, esz))
    canon = capped
    cva = [c[0] for c in canon]

    def obj_of(ea):
        if ea < 0x00100000:
            return 'portdata', ea
        if 0x02000000 <= ea < 0x03000000:
            return 'stack', 0
        k = bisect.bisect_right(cva, ea) - 1
        best = None
        j = k
        while j >= 0 and ea - canon[j][0] < 0x400000:
            va, end, nm, rec, esz = canon[j]
            if va <= ea < end and (best is None or end - va < best[1] - best[0]):
                best = canon[j]
            j -= 1
        if best is None:
            # past the end of the object before it: a table the 64-bit core
            # declares smaller than the code walks
            if k >= 0:
                va, end, nm, rec, esz = canon[k]
                return 'OVERRUN:%s' % nm, ea - va
            return '0x%08X' % ea, 0
        va, end, nm, rec, esz = best
        if rec and esz:
            return '%s<%s>' % (nm, rec), (ea - va) % esz
        return nm, ea - va

    out = os.path.join('build/portable/trace')
    os.makedirs(out, exist_ok=True)
    objects = collections.defaultdict(lambda: collections.defaultdict(
        lambda: {'w': set(), 'ptr': 0, 'n': 0, 'sites': set()}))
    with open(os.path.join(out, 'sites.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['site', 'loc', 'kind', 'width', 'count', 'lo', 'hi', 'nptr', 'nnz', 'touches', 'values'])
        for i, s in enumerate(sites):
            r = recs[i]
            cnt, lo, hi, nptr, nnz = r[0], r[1], r[2], r[3], r[4]
            if not cnt:
                continue
            eas = r[5:5 + NS]
            ass = r[5 + NS:5 + 2 * NS]
            vals = r[5 + 2 * NS:5 + 2 * NS + NV]
            nea, nval, width = r[5 + 2 * NS + NV], r[6 + 2 * NS + NV], r[7 + 2 * NS + NV]
            touch = []
            for k in range(nea):
                if ass[k]:
                    ob = 'heap@' + loc.get(ass[k] - 1, 'site%d' % (ass[k] - 1))
                    o2 = eas[k]
                else:
                    ob, o2 = obj_of(eas[k])
                touch.append('%s+0x%X' % (ob, o2))
                ent = objects[ob][(o2, width)]
                ent['n'] += 1
                ent['sites'].add(loc.get(i, '?'))
                if width == 4 and nptr:
                    ent['ptr'] = max(ent['ptr'], 1)
                if width == 4 and nnz and nptr * 2 >= nnz:
                    ent['ptr'] = 2
            vs = ['%s+0x%X' % obj_of(v) if 0x10000000 <= v < 0x12000000 else '0x%08X' % v for v in vals[:nval]]
            w.writerow([i, loc.get(i, '?'), s['kind'], width, cnt, '0x%08X' % lo, '0x%08X' % hi,
                        nptr, nnz, ' '.join(touch), ' '.join(vs)])
    with open(os.path.join(out, 'objects.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['object', 'offset', 'width', 'holds_address', 'samples', 'sites'])
        for ob in sorted(objects):
            for (o2, wd), e in sorted(objects[ob].items()):
                w.writerow([ob, '0x%X' % o2, wd, {0: '', 1: 'sometimes', 2: 'yes'}[e['ptr']], e['n'],
                            ' '.join(sorted(e['sites'])[:6])])
    with open(os.path.join(out, 'allocs.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['site', 'loc', 'count', 'min_size', 'max_size'])
        for i, a in enumerate(arecs):
            if a[0]:
                w.writerow([i, loc.get(i, '?'), a[0], a[1], a[2]])
    print('trace: %d sites executed, %d objects, %d allocation sites' % (
        sum(1 for r in recs if r[0]), len(objects), sum(1 for a in arecs if a[0])))


if __name__ == '__main__':
    main()
