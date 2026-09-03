#!/usr/bin/env python3
"""Generate byte-exact C++ TUs for the "catalogue string into the item label"
family.

The shape (100 bytes, cdecl, one argument):

    strcpy(pObj->m2B5C.szName, BrStrByIndex(<table>[<selector>]));
    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);
    return 1;

Members differ ONLY in the selector global's address and the index table's
address, so the family is found by byte template rather than by guessing:
the template and its wildcard mask are DERIVED from the three members that
were hand-solved (0x10039270 / 0x10039350 / 0x10039510) by diffing their
original bytes against each other. Any byte position where those three
disagree is a wildcard; everything else must match exactly. That keeps the
screen honest -- it cannot drift onto a function that merely looks similar.

    python3 tools/gen_uilabel.py            # list candidates, write nothing
    python3 tools/gen_uilabel.py --write    # emit src/core/cpp/<VA>.cpp

Already-matched VAs are skipped. Score what it writes with cpp_score /
cpp_sweep like any other TU -- this generator does not claim a match, it
only writes the source.
"""
from __future__ import print_function

import argparse
import csv
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = os.path.join(ROOT, 'build', 'match', 'orig')
CPP_DIR = os.path.join(ROOT, 'src', 'core', 'cpp')

SEEDS = ['0x10039270', '0x10039350', '0x10039510']

TEMPLATE = '''/* @implements %(va)s glide %(name)s
 * @cpp_kind free
 * @cpp_symbol ?%(name)s@@YAHPAVObj%(sfx)s@@@Z
 *
 * cdecl, one arg, `ret`, %(size)d B. Put the catalogue string the selector
 * at %(sel_c)s points at into the owner's +0x2B5C item label, relayout
 * through the +0x04 vcall and apply. Returns 1.
 *
 * Same 0x438 item record as 0x10041300. One of the identical siblings this
 * family is made of -- they differ only in the selector global and the
 * index table -- so this TU was emitted by tools/gen_uilabel.py from a
 * byte template derived from the hand-solved members.
 *
 * Where a port body exists for the same function it reaches its globals
 * through a BrUiGlobals* second parameter; the original is cdecl with ONE
 * argument and direct global addresses. Leave the port body port-only and
 * let this file carry the @implements, the way the 0x10038650 sibling does.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Item%(sfx)s {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 relayout */
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();
    virtual void  s11();

    int   f004;
    char  b008;
    char  szName[0x401];        /* +0x009 */
};

typedef char chk_name%(sfx)s[(unsigned)&((Item%(sfx)s *)0)->szName == 9 ? 1 : -1];

class Obj%(sfx)s {
public:
    char        pad000[0x2B5C];
    Item%(sfx)s m2B5C;          /* +0x2B5C */
};

typedef char chk_item%(sfx)s[(unsigned)&((Obj%(sfx)s *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int  %(sel)s;                   /* %(sel_c)s */
int  %(tbl)s[];                 /* %(tbl_c)s */

char *BrStrByIndex(int idx);                    /* 0x1006D280 */
void  BrItemApply_10038380(void *pObj, int a);  /* 0x10038380 */
}

int %(name)s(Obj%(sfx)s *pObj)
{
    strcpy(pObj->m2B5C.szName, BrStrByIndex(%(tbl)s[%(sel)s]));

    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);

    return 1;
}
'''


def load(va):
    p = os.path.join(ORIG, '%s.bin' % va)
    return open(p, 'rb').read() if os.path.exists(p) else None


def derive_mask(seeds):
    """Bytes that differ between any two seeds are wildcards."""
    bufs = [load(v) for v in seeds]
    if any(b is None for b in bufs):
        sys.exit('missing reference bytes for a seed; run the extractor first')
    n = len(bufs[0])
    if any(len(b) != n for b in bufs):
        sys.exit('seeds differ in length -- the family is not what this expects')
    fixed = [all(b[i] == bufs[0][i] for b in bufs) for i in range(n)]
    return bufs[0], fixed, n


def operand_slots(tmpl, fixed):
    """Locate the four variable fields by DECODING the template, then check
    that every byte the seeds disagree on falls inside one of them. The
    fields are: the selector's imm32 (`mov eax,[imm32]`), the table's imm32
    (`mov ecx,[eax*4+imm32]`), and the two `call rel32` displacements."""
    if tmpl[0] != 0xA1:
        sys.exit('template does not start with `mov eax,[imm32]`')
    sel_off = 1

    tbl_key = b'\x8b\x0c\x85'
    i = tmpl.find(tbl_key)
    if i < 0:
        sys.exit('template has no `mov ecx,[eax*4+imm32]`')
    tbl_off = i + len(tbl_key)

    calls = []
    j = 0
    while True:
        j = tmpl.find(b'\xe8', j)
        if j < 0:
            break
        calls.append(j + 1)
        j += 5
    if len(calls) != 2:
        sys.exit('template has %d call sites, expected 2' % len(calls))

    fields = [(sel_off, 4), (tbl_off, 4)] + [(c, 4) for c in calls]
    allowed = set()
    for off, ln in fields:
        allowed.update(range(off, off + ln))

    stray = [i for i in range(len(fixed)) if not fixed[i] and i not in allowed]
    if stray:
        sys.exit('seeds disagree outside the known fields at %s -- the family '
                 'is not as uniform as this generator assumes'
                 % ', '.join('+0x%x' % o for o in stray))

    return sel_off, tbl_off, allowed


def matched_vas():
    out = set()
    for rep in ('report.csv', 'report_cpp.csv'):
        p = os.path.join(ROOT, 'build', 'match', rep)
        if not os.path.exists(p):
            continue
        for r in csv.DictReader(open(p)):
            if r.get('status') == 'match':
                out.add((r.get('va') or '').lower())
    return out


def names_by_va():
    out = {}
    for rep in ('report.csv', 'report_cpp.csv'):
        p = os.path.join(ROOT, 'build', 'match', rep)
        if not os.path.exists(p):
            continue
        for r in csv.DictReader(open(p)):
            va = (r.get('va') or '').lower()
            sym = r.get('symbol') or r.get('name')
            if va and sym:
                out.setdefault(va, sym)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--write', action='store_true', help='emit the TUs')
    args = ap.parse_args()

    tmpl, fixed, n = derive_mask(SEEDS)
    sel_off, tbl_off, allowed = operand_slots(tmpl, fixed)
    print('template %d bytes; selector imm at +0x%x, table imm at +0x%x, '
          '%d variable bytes' % (n, sel_off, tbl_off, len(allowed)))

    done = matched_vas()
    names = names_by_va()
    hits = []
    for fn in sorted(os.listdir(ORIG)):
        if not fn.endswith('.bin'):
            continue
        va = fn[:-4].lower()
        b = load(fn[:-4])
        if b is None or len(b) != n:
            continue
        if not all(i in allowed or b[i] == tmpl[i] for i in range(n)):
            continue
        sel = struct.unpack_from('<I', b, sel_off)[0]
        tbl = struct.unpack_from('<I', b, tbl_off)[0]
        hits.append((va, sel, tbl, va in done))

    for va, sel, tbl, is_done in hits:
        mark = 'matched' if is_done else 'CANDIDATE'
        print('  %-12s sel=0x%08X tbl=0x%08X  %s' % (va, sel, tbl, mark))

    todo = [h for h in hits if not h[3]]
    if not args.write:
        print('%d family members, %d not yet matched (use --write)'
              % (len(hits), len(todo)))
        return

    for va, sel, tbl, _ in todo:
        sfx = va[4:].upper()
        name = names.get(va) or ('BrUiTextLabel_%s' % va[2:].upper())
        src = TEMPLATE % dict(
            va='0x' + va[2:].upper(), name=name, sfx=sfx, size=n,
            sel='g_brSel%s' % ('%08X' % sel)[-4:],
            tbl='g_brTbl%s' % ('%08X' % tbl)[-5:],
            sel_c='0x%08X' % sel, tbl_c='0x%08X' % tbl)
        out = os.path.join(CPP_DIR, '0x%s.cpp' % va[2:].upper())
        open(out, 'w').write(src)
        print('wrote %s' % os.path.relpath(out, ROOT))


if __name__ == '__main__':
    main()
