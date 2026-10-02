#!/usr/bin/env python3
"""Name the field at an original (i386) byte offset of a canonical record.

    offpath.py BrDriverCar 0x29A8 0xF08 ...
    offpath.py --dump BrDriverCar

Lays the records out with clang in i386 mode (the force-included headers
plus tools/canon_all.h) and resolves each offset to a member path, so a
decompiled `*(int *)(p + 0x29A8)` can be rewritten as `p->field`.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402


def main():
    args = sys.argv[1:]
    dump = '--dump' in args
    args = [a for a in args if a != '--dump']
    rec, offs = args[0], [int(a, 0) for a in args[1:]]
    os.chdir(vm.ROOT)
    tu = os.path.join(vm.ROOT, 'ports/64b/tools/canon_all.h')
    recs, sizes = vm.layouts(tu, [])
    if rec not in recs:
        sys.exit('no record %s' % rec)
    if dump:
        for o, d, t, n in recs[rec]:
            print('0x%04X %s%s  %s' % (o, '  ' * d, n, t))
        return
    for off in offs:
        r = vm.resolve(recs, sizes, rec, off, None)
        print('0x%X %s' % (off, '-' if r is None else '%s  [%s]%s' % (r[0], r[1], ' +%d' % r[2] if r[2] else '')))


if __name__ == '__main__':
    main()
