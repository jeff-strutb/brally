#!/usr/bin/env python3
"""ptrfields.py -- turn every pointer member of the core's structs into a
TgrAddr (a 4-byte original address), so each struct keeps the N64's layout
in the arena (tgr_addr.h).

For every struct or union defined in ports/tgrally (src and include), each
member whose type is a pointer, or an array of pointers, is rewritten
in place:

    unsigned char *data;   /* 0x00 pixels */   ->   TgrAddr data;   /* unsigned char * -- 0x00 pixels */
    void (*fn)(int);                           ->   TgrAddr fn;     /* void (*)(int) */
    MenuItem *items[4];                        ->   TgrAddr items[4];   /* MenuItem * */

Uses then fail to compile until they go through TGR_PTR / tgr_addr32 /
TGR_FN; that is the point.  Run once; it reports what it changed.
"""
import os
import re
import sys
from concurrent.futures import ProcessPoolExecutor

import clang.cindex as ci

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
K = ci.TypeKind


def tus():
    out = []
    for dp, dn, fn in os.walk(os.path.join(ROOT, 'ports/tgrally/src')):
        out += [os.path.join(dp, f) for f in fn if f.endswith('.c')]
    return sorted(out)


def ptrish(t):
    t = t.get_canonical()
    while t.kind == K.CONSTANTARRAY:
        t = t.element_type.get_canonical()
    return t.kind in (K.POINTER, K.BLOCKPOINTER)


def scan(tu):
    idx = ci.Index.create()
    tree = idx.parse(tu, args=['-target', 'mips-linux-gnu', '-std=gnu89', '-w',
                               '-I' + os.path.join(ROOT, 'ports/tgrally/include'),
                               '-I' + os.path.join(ROOT, 'ports/tgrally/platform/include'),
                               '-include', os.path.join(ROOT, 'ports/tgrally/platform/include/ultra64.h'),
                               '-include', os.path.join(ROOT, 'ports/tgrally/platform/include/tgr_core.h')])
    out = set()
    for c in tree.cursor.walk_preorder():
        if c.kind != ci.CursorKind.FIELD_DECL:
            continue
        f = c.location.file.name if c.location.file else ''
        if '/ports/tgrally/src/' not in f and '/ports/tgrally/include/' not in f:
            continue
        if ptrish(c.type):
            out.add((f, c.location.line, c.spelling, c.type.spelling))
    return out


def main():
    found = set()
    with ProcessPoolExecutor(14) as ex:
        for s in ex.map(scan, tus()):
            found |= s
    byfile = {}
    for f, line, name, ty in found:
        byfile.setdefault(f, []).append((line, name, ty))
    n = 0
    for f, rows in sorted(byfile.items()):
        lines = open(f).read().split('\n')
        for line, name, ty in sorted(rows, reverse=True):
            s = lines[line - 1]
            code, sep, cmt = s.partition('/*')
            m = re.match(r'^(\s*)(.*?)(;)(\s*)$', code)
            if not m:
                print('%s:%d: cannot rewrite %r' % (os.path.relpath(f, ROOT), line, s.strip()))
                continue
            ind, decl, semi, tail = m.groups()
            if ',' in decl and '(' not in decl:
                print('%s:%d: several members on one line: %r' % (os.path.relpath(f, ROOT), line, s.strip()))
                continue
            dims = ''
            fm = re.search(r'\(\s*\*\s*' + re.escape(name) + r'\s*((?:\[[^\]]*\])*)\s*\)', decl)
            if fm:
                dims = fm.group(1)
            else:
                am = re.search(r'\b' + re.escape(name) + r'\s*((?:\[[^\]]*\])*)\s*$', decl)
                if not am:
                    print('%s:%d: no member %s in %r' % (os.path.relpath(f, ROOT), line, name, s.strip()))
                    continue
                dims = am.group(1)
            base = re.sub(r'\[[^\]]*\]', '', ty).strip()
            note = base + (' -- ' + cmt.rstrip().rstrip('*/').strip() if sep else '')
            new = '%sTgrAddr %s%s;%s/* %s */' % (ind, name, dims, tail or '  ', note.strip())
            lines[line - 1] = new
            n += 1
        open(f, 'w').write('\n'.join(lines))
    print('ptrfields: %d members retyped' % n)


if __name__ == '__main__':
    main()
