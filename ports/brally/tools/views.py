#!/usr/bin/env python3
"""Every declaration of each record in the core, compared at 32 bits.

The decompiled core often declares one object several times: a function
declares the fields it touches and pads the rest with byte arrays. At 32
bits all those views agree; at 64 bits pointer fields grow and padding does
not, so views of one object stop agreeing. This lists, per record name, the
distinct views (top-level field sets as offset:type:name) the i686 compile
sees, so each object can get one complete declaration.

Usage: views.py <dir of *.i686-pc-windows-msvc.rl dumps>  [--show NAME]
"""
import collections
import glob
import os
import re
import sys


def parse(txt):
    out = []
    for blk in txt.split('*** Dumping AST Record Layout')[1:]:
        lines = blk.strip('\n').split('\n')
        m = re.match(r'\s*\d+ \| (.*)$', lines[0])
        sz = re.search(r'\[sizeof=(\d+)', blk)
        if not (m and sz):
            continue
        fields = []
        for ln in lines[1:]:
            fm = re.match(r'\s*(\d+)(?::(\d+)-(\d+))? \|   (\S.*)$', ln)
            if fm:
                fields.append((int(fm.group(1)), fm.group(4).strip()))
        out.append((m.group(1).strip(), int(sz.group(1)), tuple(fields)))
    return out


def main():
    d = sys.argv[1]
    show = sys.argv[sys.argv.index('--show') + 1] if '--show' in sys.argv else None
    tags = set()
    for root in ('include', 'src'):
        for p in glob.glob(os.path.join(root, '**', '*'), recursive=True):
            if p.endswith(('.h', '.c', '.cpp')):
                for m in re.finditer(r'\b(?:struct|union|class)\s+([A-Za-z_]\w*)\s*(?::[^{;]*)?\{',
                                     open(p, errors='replace').read()):
                    tags.add(m.group(1))
    views = collections.defaultdict(lambda: collections.defaultdict(set))
    grows = set()
    for p in glob.glob(os.path.join(d, '*.i686-pc-windows-msvc.rl')):
        tu = os.path.basename(p).split('.i686')[0].replace('__', '/')
        v64 = {n: sz for n, sz, _ in parse(open(p.replace('i686-pc-windows-msvc', 'x86_64-pc-windows-msvc'), errors='replace').read())}
        for name, sz, fields in parse(open(p, errors='replace').read()):
            if name.split(' ', 1)[-1] not in tags:
                continue
            views[name][(sz, fields)].add(tu)
            if v64.get(name, sz) != sz:
                grows.add(name)
    tree = dict(views)
    multi = {n: v for n, v in tree.items() if len(v) > 1}
    PAD = re.compile(r'^(?:unsigned )?(?:char|BYTE|uint8_t|u8|UCHAR|int|DWORD)\[\d+\] (?:_|pad|rest|gap|unk|res)', re.I)
    padded = {n for n, v in tree.items() if any(PAD.match(f[1]) for (sz, fl) in v for f in fl)}
    ptr = grows
    print('records named in the core: %d' % len(tree))
    print('  with more than one distinct view: %d' % len(multi))
    print('  with byte-array padding: %d (with a pointer field too: %d)' % (len(padded), len(padded & ptr)))
    print('  multi-view AND pointer-bearing: %d' % len(set(multi) & ptr))
    sizes = collections.Counter()
    for n, v in multi.items():
        for (sz, fl) in v:
            sizes[n] += 0
    worst = sorted(multi.items(), key=lambda kv: -len(kv[1]))[:25]
    for n, v in worst:
        szs = sorted({sz for (sz, fl) in v})
        print('  %-40s %3d views  sizes %s' % (n, len(v), ','.join('%#x' % s for s in szs[:4])))
    if show:
        for (sz, fl), tus in tree[show].items():
            print('--- %#x  (%d TUs: %s)' % (sz, len(tus), ', '.join(sorted(tus)[:3])))
            for off, f in fl:
                print('   %6x  %s' % (off, f))


if __name__ == '__main__':
    main()
