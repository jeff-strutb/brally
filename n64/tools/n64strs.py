"""List the strings a Top Gear Rally function references -- a quick way to see
what a function is for.   .venv/bin/python n64/tools/n64strs.py 0x8020082C"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n64rom as n


def strings_of(va):
    s = n.fstart(va)
    i = n.F.index(s)
    e = n.F[i + 1] if i + 1 < len(n.F) else n.r2v(n.TEXT_E)
    out = []
    for t, pcs in n.xref.items():
        if any(s <= p < e for p in pcs) and 0x8026FAB0 <= t < 0x802AC400:
            r = n.v2r(t)
            j = r
            while j < len(n.d) and n.d[j] != 0 and j - r < 120:
                j += 1
            b = n.d[r:j]
            if len(b) >= 3 and all(32 <= c < 127 or c in (9, 10) for c in b):
                out.append((t, b.decode()))
    return sorted(out)


if __name__ == '__main__':
    for a in sys.argv[1:]:
        print('== %s' % a)
        for t, st in strings_of(int(a, 16)):
            print('  %08X  %r' % (t, st))
