#!/usr/bin/env python3
"""Lay the original's data out as the 64-bit core will hold it.

Two ranges of BRGlide.dll's memory become one struct each, members in
original address order:
  data  0x10077000 .. 0x100BCE00   .rdata + initialised .data
  bss   0x100BCE00 .. 0x118EF184   the zero-filled rest of .data
Every canonical object (build/brally/null-soft/canon.csv, from unify.py) is a member
at its original address; the bytes between are filler members. A filler
dword is a pointer member exactly where the original holds an address: the
DLL's base relocations say so for initialised data; for the bss the trace
(build/brally/null-soft/trace/objects.csv) does.

This pass checks before anything is generated:
  OVERLAP   two canonical objects claim the same bytes
  RELOC     an initialised object holds an address (relocation) at an
            offset where its type has no pointer
  TYPELESS  a canonical object whose size cannot be determined

Usage: imagemap.py [--range data|bss]
Output: build/brally/null-soft/image/<range>_layout.csv, <range>_check.txt
"""
import bisect
import collections
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe32  # noqa: E402
import recspec  # noqa: E402
import rewrite  # noqa: E402

ROOT = recspec.ROOT
RANGES = {'data': (0x10077000, 0x100BCE00), 'bss': (0x100BCE00, 0x118EF184)}
_size_cache = {}


def size_of(ty):
    """Original size of a canonical's type, or None."""
    if ty in _size_cache:
        return _size_cache[ty]
    t = re.sub(r'\b(const|volatile)\b', '', ty).strip()
    m = re.match(r'^(.*?)\s*((?:\[\w*\])+)$', t)
    dims = []
    elem = t
    if m:
        elem = m.group(1).strip()
        dims = re.findall(r'\[(\w*)\]', m.group(2))
    base = re.sub(r'^struct\s+', '', elem)
    esz = None
    if elem.endswith('*') or '(*' in elem:
        esz = 4
    elif rewrite.spec(base):
        esz = int(rewrite.spec(base)[0]['size'], 16)
    else:
        try:
            esz = recspec.size32(elem, ['br_globals.h'])
        except SystemExit:
            esz = None
    if esz is None:
        _size_cache[ty] = None
        return None
    n = 1
    for d in dims:
        if not re.match(r'(0x[0-9a-fA-F]+|\d+)$', d):
            _size_cache[ty] = None
            return None
        n *= int(d, 0)
    _size_cache[ty] = esz * n
    return esz * n


def pointer_offsets(ty, size):
    """Offsets (in the original layout) where the type holds a pointer, or
    None when it cannot be told (a struct without a spec)."""
    t = re.sub(r'\b(const|volatile)\b', '', ty).strip()
    m = re.match(r'^(.*?)\s*((?:\[\w*\])+)$', t)
    elem, dims = (m.group(1).strip(), re.findall(r'\[(\w*)\]', m.group(2))) if m else (t, [])
    base = re.sub(r'^struct\s+', '', elem)
    esz0 = size_of(elem) or 0
    if any(not re.match(r'(0x[0-9a-fA-F]+|\d+)$', d) for d in dims):
        n = size // esz0 if esz0 else 0          # unsized: as many as the object holds
    else:
        n = 1
        for d in dims:
            n *= int(d, 0)
    if elem.endswith('*') or '(*' in elem:
        return {4 * i for i in range(n)}
    if re.match(r'^(unsigned |signed )?(char|short|int|long|float|double)$|^u?int(8|16|32|64)_t$|^(BYTE|WORD|DWORD|BOOL|LONG)$', elem):
        return set()
    sp = rewrite.spec(base)
    if not sp:
        return None
    esz = int(sp[0]['size'], 16)
    one = set()

    def walk(rec, at):
        s2 = rewrite.spec(rec)
        if not s2:
            return False
        hdrs = s2[0].get('headers', '').split()
        for f in s2[1]:
            ft = f.ty.strip()
            fm = re.match(r'^(.*?)\s*((?:\[\w*\])+)$', ft)
            fe, fd = (fm.group(1).strip(), re.findall(r'\[(\w*)\]', fm.group(2))) if fm else (ft, [])
            cnt = 1
            for d in fd:
                cnt *= int(d, 0)
            if fe.endswith('*') or ft.startswith('fnptr:'):
                for i in range(cnt):
                    one.add(at + f.off + 4 * i)
                continue
            sub = re.sub(r'^struct\s+', '', fe)
            if rewrite.spec(sub):
                ssz = int(rewrite.spec(sub)[0]['size'], 16)
                for i in range(cnt):
                    walk(sub, at + f.off + i * ssz)
        return True
    walk(base, 0)
    return {k * esz + o for k in range(n) for o in one}


def main():
    os.chdir(ROOT)
    which = sys.argv[sys.argv.index('--range') + 1] if '--range' in sys.argv else 'data'
    lo, hi = RANGES[which]
    canon = []
    for r in csv.DictReader(open('build/brally/null-soft/canon.csv')):
        va = int(r['va'], 16)
        if lo <= va < hi:
            sz = size_of(r['type'])
            if sz is None and r['size32']:
                sz = int(r['size32'])
            canon.append((va, r['name'], r['type'], sz, r['forced'] == '1'))
    canon.sort()
    pe = pe32.PE('reference/brally/orig/BRGlide.dll')
    relocs0 = sorted(r for r in pe.relocs() if lo <= r < hi) if which == 'data' else []
    # resolve extents: a weak declaration (bytes, chars, plain ints, an
    # unsized array) ends where the next object begins; a typed table that
    # holds other objects AND contradicts the relocations is trimmed to whole
    # elements before the first of them
    WEAK = re.compile(r'^(const\s+)?(unsigned\s+|signed\s+)?(char|u?int(8|16|32)_t|int|short|unsigned|float)\s*(\[|$)')
    fixed = []
    for i, (va, nm, ty, sz, forced) in enumerate(canon):
        nxt = canon[i + 1][0] if i + 1 < len(canon) else hi
        if sz and va + sz > nxt and not forced:
            weak = bool(WEAK.match(ty)) or '[]' in ty
            bad = False
            if not weak and which == 'data':
                po = pointer_offsets(ty, sz)
                inside = relocs0[bisect.bisect_left(relocs0, va):bisect.bisect_left(relocs0, va + sz)]
                bad = po is not None and any((r - va) not in po for r in inside)
            if weak or bad:
                el = re.match(r'^(.*?)\s*\[', ty)
                esz = size_of(el.group(1)) if el else sz
                room = nxt - va
                sz = max(esz or 1, room - room % (esz or 1)) if esz else room
        fixed.append((va, nm, ty, sz, forced))
    canon = fixed
    relocs = sorted(r for r in pe.relocs() if lo <= r < hi) if which == 'data' else []
    if which == 'bss':
        tp = collections.defaultdict(set)
        # the trace's address-holding offsets, by canonical name (+ record offset is lost here:
        # use raw addresses from sites.csv samples instead)
    out, check, aliases = [], [], []
    pos = lo
    cur = None
    k = 0
    while k < len(canon):
        va, nm, ty, sz, forced = canon[k]
        if sz is None:
            check.append('TYPELESS  0x%08X %s %s' % (va, nm, ty))
            sz = 4
        end = va + sz
        if va < pos:
            aliases.append((va, nm, ty, sz, cur))
            if va + (sz or 0) > pos:
                check.append('OVERLAP   0x%08X %s (%s, 0x%X) starts inside %s and runs past its end (0x%08X)'
                             % (va, nm, ty, sz, cur, pos))
            k += 1
            continue
        if va > pos:
            out.append((pos, va - pos, 'gap', '', ''))
        # the next object may start inside this one
        po = pointer_offsets(ty, sz) if sz else None
        if which == 'data':
            inside = relocs[bisect.bisect_left(relocs, va):bisect.bisect_left(relocs, end)]
            for r in inside:
                if po is None:
                    check.append('RELOC?    0x%08X %s (%s): address at +0x%X, type unchecked' % (va, nm, ty, r - va))
                    break
                if (r - va) not in po:
                    check.append('RELOC     0x%08X %s (%s): address at +0x%X, no pointer there'
                                 % (va, nm, ty, r - va))
        out.append((va, sz, 'object', nm, ty))
        cur = nm
        pos = max(pos, end)
        k += 1
    if pos < hi:
        out.append((pos, hi - pos, 'gap', '', ''))
    os.makedirs('build/brally/null-soft/image', exist_ok=True)
    with open('build/brally/null-soft/image/%s_layout.csv' % which, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['va', 'size', 'kind', 'name', 'type'])
        for va, sz, kd, nm, ty in out:
            w.writerow(['0x%08X' % va, sz, kd, nm, ty])
    open('build/brally/null-soft/image/%s_check.txt' % which, 'w').write('\n'.join(check) + '\n')
    with open('build/brally/null-soft/image/%s_aliases.csv' % which, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['va', 'name', 'type', 'size', 'inside'])
        for va, nm, ty, sz, inn in aliases:
            w.writerow(['0x%08X' % va, nm, ty, sz, inn])
    c = collections.Counter(l.split()[0] for l in check)
    gaps = [o for o in out if o[2] == 'gap']
    print('%s: %d objects, %d gaps (%d bytes); %s' % (which, sum(1 for o in out if o[2] == 'object'),
                                                     len(gaps), sum(g[1] for g in gaps), dict(c)))


if __name__ == '__main__':
    main()
