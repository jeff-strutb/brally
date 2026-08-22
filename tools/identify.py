#!/usr/bin/env python3
"""Identify each shipped binary by what it imports and what it says."""
import glob
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SUBSYS = {1: 'native', 2: 'GUI', 3: 'console'}


def pe(path):
    d = open(path, 'rb').read()
    if d[:2] != b'MZ':
        return None
    off = struct.unpack_from('<I', d, 0x3C)[0]
    if off + 4 > len(d) or d[off:off + 4] != b'PE\0\0':
        return {'fmt': 'NE / 16-bit (not PE32)'}
    nsec = struct.unpack_from('<H', d, off + 6)[0]
    optsz = struct.unpack_from('<H', d, off + 20)[0]
    opt = off + 24
    magic = struct.unpack_from('<H', d, opt)[0]
    sub = struct.unpack_from('<H', d, opt + 68)[0]
    base = struct.unpack_from('<I', d, opt + 28)[0]
    secs = []
    for i in range(nsec):
        s = off + 24 + optsz + i * 40
        nm = d[s:s + 8].rstrip(b'\0').decode('latin1', 'replace')
        vs, rva, rs, ro = struct.unpack_from('<IIII', d, s + 8)
        secs.append((nm, rva, vs, ro, rs))

    def r2o(r):
        for nm, srva, vs, ro, rs in secs:
            if srva <= r < srva + max(vs, rs):
                return ro + (r - srva)
        return None

    # imports
    idir, isz = struct.unpack_from('<II', d, opt + 96 + 1 * 8)
    imps = {}
    o = r2o(idir) if idir else None
    if o:
        while o + 20 <= len(d):
            oft, ts, fc, nrva, fta = struct.unpack_from('<IIIII', d, o)
            if not nrva:
                break
            no = r2o(nrva)
            if no is None:
                break
            dll = d[no:d.index(b'\0', no)].decode('latin1', 'replace')
            fns = []
            t = r2o(oft or fta)
            if t:
                while True:
                    v = struct.unpack_from('<I', d, t)[0]
                    if not v:
                        break
                    if not (v & 0x80000000):
                        a = r2o(v)
                        if a:
                            fns.append(d[a + 2:d.index(b'\0', a + 2)]
                                       .decode('latin1', 'replace'))
                    t += 4
            imps[dll] = fns
            o += 20
    txt = [s for s in secs if s[0].startswith('.text')]
    return {'fmt': 'PE32' if magic == 0x10b else hex(magic),
            'subsystem': SUBSYS.get(sub, sub), 'base': base,
            'text': txt[0][2] if txt else 0, 'imports': imps,
            'sections': [s[0] for s in secs], 'data': d}


def strings(d, lo=6):
    out = re.findall(rb'[\x20-\x7e]{%d,}' % lo, d)
    return [s.decode('latin1') for s in out]


def main():
    files = sorted(glob.glob(os.path.join(ROOT, 'orig', '*')))
    for f in files:
        if os.path.isdir(f):
            continue
        name = os.path.basename(f)
        info = pe(f)
        print('=' * 72)
        print(f"{name}   ({os.path.getsize(f):,} bytes)")
        if not info:
            print('  not a PE/MZ file')
            continue
        if 'imports' not in info:
            print(f"  {info['fmt']}")
            continue
        print(f"  {info['fmt']} {info['subsystem']}  base={info['base']:#x}  "
              f".text={info['text']:,}  sections={','.join(info['sections'])}")
        print(f"  imports {len(info['imports'])} DLLs:")
        for dll, fns in sorted(info['imports'].items()):
            print(f"    {dll:20s} {len(fns):4d} fns  "
                  f"{', '.join(fns[:6])}{'...' if len(fns) > 6 else ''}")
        ss = strings(info['data'])
        # signal-bearing strings only
        keys = [s for s in ss if re.search(
            r'\.(dll|exe|ini|cfg|txt|dat|bin|wav|xm|cd)|glide|3dfx|direct|d3d|'
            r'ddraw|voodoo|resolution|screen|registry|SOFTWARE\\|error|failed|'
            r'usage|命|rally|boss', s, re.I)][:22]
        print('  telling strings:')
        for s in keys:
            print(f'    {s[:88]}')


if __name__ == '__main__':
    main()
