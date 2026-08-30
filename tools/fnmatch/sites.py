#!/usr/bin/env python3
"""Named-site worklist: align orig vs recomp instruction streams and emit each
divergent region as a numbered site with orig-VA anchors and a class guess.

The anti-shotgun tool: every edit should target a site this list names.

    python3 tools/fnmatch/sites.py <orig.bin> <obj> <symbol> [orig_base_va]
"""
from __future__ import print_function
import sys, re, difflib
import os
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

R32 = r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b'
R16 = r'\b(ax|bx|cx|dx|si|di|bp)\b'
R8  = r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b'


def regblind(t):
    t = re.sub(r'esp [+-] 0x[0-9a-f]+', 'esp+S', t)
    t = re.sub(r'\b0x[0-9a-f]+\b', 'I', t); t = re.sub(r'\b\d+\b', 'I', t)
    t = re.sub(R32, 'R', t); t = re.sub(R16, 'W', t); t = re.sub(R8, 'B', t)
    return t


def classify(o_txt, r_txt):
    o, r = ' | '.join(o_txt), ' | '.join(r_txt)
    if re.search(r'(add R, R|lea R, \[R \+ R\])', o) and 'esp+S' in r:
        return 'DOUBLING/SPILL'
    if re.search(r'\bB\b', o) and not re.search(r'\bB\b', r):
        return 'BYTE-WIDTH'
    if 'imul' in o or 'imul' in r:
        return 'IMUL-PARK'
    if re.search(r'j(le|g|ge|l|e|ne) ', o) or re.search(r'j(le|g|ge|l|e|ne) ', r):
        return 'BRANCH/ORDER'
    if 'esp+S' in r and 'esp+S' not in o:
        return 'SPILL'
    if o_txt == [] or r_txt == []:
        return 'INS/DEL'
    return 'OTHER'


def main():
    ob, objp, sym = sys.argv[1], sys.argv[2], sys.argv[3]
    base = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0
    orig = open(ob, 'rb').read()
    rc = parse_coff_obj(objp)[sym][0]
    while rc and rc[-1] == 0x90:
        rc = rc[:-1]
    O = [(i.address + base, '%s %s' % (i.mnemonic, i.op_str)) for i in md.disasm(orig, 0)]
    R = [(i.address, '%s %s' % (i.mnemonic, i.op_str)) for i in md.disasm(rc, 0)]
    so = [regblind(t) for _, t in O]; sr = [regblind(t) for _, t in R]
    sm = difflib.SequenceMatcher(None, so, sr, autojunk=False)
    sites = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        sites.append((i1, i2, j1, j2))
    tot_o = sum(i2 - i1 for i1, i2, _, _ in sites)
    tot_r = sum(j2 - j1 for _, _, j1, j2 in sites)
    print('%d sites; %d orig insns / %d recomp insns inside them '
          '(orig total %d, recomp %d)\n' % (len(sites), tot_o, tot_r, len(O), len(R)))
    for n, (i1, i2, j1, j2) in enumerate(sites):
        o_txt = [O[k][1] for k in range(i1, i2)]
        r_txt = [R[k][1] for k in range(j1, j2)]
        cls = classify([regblind(t) for t in o_txt], [regblind(t) for t in r_txt])
        oa = '%08x' % O[i1][0] if i2 > i1 else ('~%08x' % O[min(i1, len(O)-1)][0])
        ra = '%04x' % R[j1][0] if j2 > j1 else ('~%04x' % R[min(j1, len(R)-1)][0])
        print('SITE %-3d %-14s orig@%s (%d)  recomp@%s (%d)'
              % (n, cls, oa, i2 - i1, ra, j2 - j1))
        for k in range(i1, min(i2, i1 + 6)):
            print('    O %08x  %s' % (O[k][0], O[k][1]))
        if i2 - i1 > 6:
            print('    O ... +%d more' % (i2 - i1 - 6))
        for k in range(j1, min(j2, j1 + 6)):
            print('    R %04x      %s' % (R[k][0], R[k][1]))
        if j2 - j1 > 6:
            print('    R ... +%d more' % (j2 - j1 - 6))
    return 0


if __name__ == '__main__':
    sys.exit(main())
