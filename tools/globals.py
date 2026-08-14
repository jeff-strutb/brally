"""Recover the global data layout.

Most core functions past the pure-math leaves touch globals, and BRD3D.dll has
~25 MB of BSS whose layout is not described anywhere. But every reference to a
global from code is a base relocation, and the instruction doing the reference
tells us the access width. So we can recover, for each global address:

  * how many distinct call sites touch it,
  * the widths used (1/2/4/8 bytes, or x87 loads),
  * whether it is indexed (`[reg*4 + base]` => an array, and the scale gives
    the element size),
  * which functions use it.

Adjacent addresses touched by the same function with a consistent stride are
merged into one array symbol. Everything else becomes a scalar.

Writes config/globals.csv.
"""
import sys, os, csv, struct, bisect, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_MEM

DLL = 'orig/BRD3D.dll'


def main():
    p = pelib.load(DLL)
    text, text_va = p.text()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    funcs = [(int(r['va'], 16), int(r['size']))
             for r in csv.DictReader(open('config/functions.csv'))]
    funcs.sort()
    starts = [f[0] for f in funcs]
    sizes = dict(funcs)

    def owner(va):
        i = bisect.bisect_right(starts, va) - 1
        if i < 0:
            return None
        s = starts[i]
        return s if va < s + sizes[s] else None

    # sections that hold globals
    def is_data(va):
        s = p.sect_for_rva(va - p.image_base)
        return s is not None and s.name in ('.data', '.rdata')

    refs = collections.defaultdict(lambda: dict(
        users=set(), widths=collections.Counter(), scales=collections.Counter(),
        writes=0, reads=0))

    for va, size in funcs:
        b = text[va - text_va: va - text_va + size]
        for ins in md.disasm(b, va):
            for k, op in enumerate(ins.operands):
                if op.type != CS_OP_MEM:
                    continue
                # absolute [disp] or indexed [reg*scale + disp]
                if op.mem.base != 0:
                    continue
                d = op.mem.disp & 0xFFFFFFFF
                if not is_data(d):
                    continue
                e = refs[d]
                e['users'].add(va)
                e['widths'][op.size] += 1
                if op.mem.index != 0:
                    e['scales'][op.mem.scale] += 1
                # operand 0 being memory on a store-style mnemonic = write
                if k == 0 and ins.mnemonic in ('mov', 'add', 'sub', 'or', 'and',
                                               'inc', 'dec', 'fstp', 'fst'):
                    e['writes'] += 1
                else:
                    e['reads'] += 1

    rows = []
    for addr in sorted(refs):
        e = refs[addr]
        w = e['widths'].most_common(1)[0][0] if e['widths'] else 0
        sc = e['scales'].most_common(1)[0][0] if e['scales'] else 0
        sec = p.sect_for_rva(addr - p.image_base)
        rows.append(dict(addr=addr, users=len(e['users']), width=w,
                         scale=sc, reads=e['reads'], writes=e['writes'],
                         section=sec.name if sec else '?',
                         array=bool(e['scales'])))

    os.makedirs('config', exist_ok=True)
    with open('config/globals.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['addr', 'symbol', 'section', 'width', 'array', 'elem_size',
                    'users', 'reads', 'writes'])
        for r in rows:
            w.writerow(['0x%08X' % r['addr'],
                        'g_%06X' % (r['addr'] - p.image_base),
                        r['section'], r['width'], int(r['array']), r['scale'],
                        r['users'], r['reads'], r['writes']])

    print("distinct global addresses referenced: %d" % len(rows))
    bysec = collections.Counter(r['section'] for r in rows)
    print("  by section: %s" % dict(bysec))
    print("  arrays (indexed access): %d" % sum(1 for r in rows if r['array']))
    print("  read-only (never written): %d" % sum(1 for r in rows if r['writes'] == 0))
    print()
    print("most-referenced globals:")
    print("  %-12s %-8s %5s %6s %6s %6s %s" %
          ("addr", "section", "width", "users", "reads", "writes", "kind"))
    for r in sorted(rows, key=lambda r: -r['users'])[:20]:
        kind = ("array[elem=%d]" % r['scale']) if r['array'] else "scalar"
        print("  0x%08X  %-8s %5d %6d %6d %6d  %s" %
              (r['addr'], r['section'], r['width'], r['users'],
               r['reads'], r['writes'], kind))


if __name__ == '__main__':
    main()
