#!/usr/bin/env python3
"""Emit a byte-exact C++ TU for a UI menu-page builder from its Ghidra draft.

The family: ~26 unmatched functions, 760 B to 4109 B, that build a menu page
by `new`-ing a 0x348 page and then a run of 0x1E214 BrCtl entries, each its
own /GX EH state. Five were hand-solved first (0x100425E0, 0x10048160,
0x100458D0, 0x100451F0, 0x10048F10) -- every one byte-exact on the first
compile once the shape was known, which is what makes this worth
generating rather than typing.

The generated source follows the three levers those five established:
  1. the null check is a CHAR bool computed AFTER the slot store, which is
     what emits `sete al / test al`; an int bool folds to a plain `jne`.
  2. simple float lvalues push raw; only computed y offsets (`f33C - k`)
     become fld/fsub/fstp -- so the emitter keeps the draft's expression.
  3. the entry tail is w2AB6-store, then w2AB4-inc, then w14, then w344.

    python3 tools/gen_menubuilder.py 0x10043050            # print
    python3 tools/gen_menubuilder.py 0x10043050 --write     # emit the TU

It is DELIBERATELY strict: any statement in the draft it does not
recognise makes it stop with the offending line rather than guess. A
builder with a hand-written block in it (a fill loop, a clamp, an
interpolation -- 0x10046E70 and 0x1004ABE0 have these) will bail here, and
that is correct: read the asm for those, do not trust the draft.
"""
from __future__ import print_function

import argparse
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRAFTS = os.path.join(ROOT, 'build', 'ghidra_work')
CPP_DIR = os.path.join(ROOT, 'src', 'core', 'cpp')
REFERENCE = os.path.join(CPP_DIR, '0x10048F10.cpp')      # widest class block


def f32(hexlit):
    """0x43430000 -> '195.0f'."""
    v = struct.unpack('<f', struct.pack('<I', int(hexlit, 16)))[0]
    if v == int(v):
        return '%d.0f' % int(v) if v != 0 else '0'
    return '%rf' % v


def arg(tok):
    """Render one s38 argument the way the family's matched TUs spell it."""
    tok = tok.strip()
    if tok in ('param_1', 'unaff_retaddr') or re.match(r'^uVar\d+$', tok):
        return 'parent'     # s38's first argument is always the parent
    m = re.match(r'^\*\(int \*\)\(iVar\d+ \+ (0x33[8c])\)$', tok)
    if m:
        return 'cont->f%s' % m.group(1)[2:].upper()
    m = re.match(r'^\*\(float \*\)\(iVar\d+ \+ (0x33[8c])\) - _(DAT_[0-9a-f]+)$', tok)
    if m:
        return 'cont->f%s - %s' % (m.group(1)[2:].upper(), m.group(2))
    if tok == '0':
        return '0'
    if tok == '0xffffffff':
        return '-1'
    if re.match(r'^0x[0-9a-f]+$', tok):
        v = int(tok, 16)
        # s38's x/y are floats; everything else is a small integer flag
        return f32(tok) if v >= 0x38000000 else tok.upper().replace('0X', '0x')
    if re.match(r'^\d+$', tok):
        return tok
    raise ValueError(tok)


def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    out.append(cur)
    return out


PFN = {1: 'pfn04', 2: 'pfn08', 3: 'pfn0C', 4: 'pfn10', 5: 'pfn14',
       6: 'pfn18'}


def parse(va):
    path = os.path.join(DRAFTS, '%s.c' % va.upper())
    if not os.path.exists(path):
        path = os.path.join(DRAFTS, '%s.c' % va)
    if not os.path.exists(path):
        sys.exit('no draft for %s' % va)
    text = open(path).read()

    m = re.search(r'^(\w+) (\w+)\((?:int param_1|void)\)\s*$', text, re.M)
    if not m:
        sys.exit('draft has no recognisable entry point')
    name = m.group(2)
    body = text[m.end():]
    body = body[:body.index('\n}\n')]

    externs, stmts = set(), []
    lines = [l.rstrip() for l in body.splitlines()]
    joined, i = [], 0
    while i < len(lines):          # rejoin the draft's wrapped statements
        l = lines[i].strip()
        while (l and not l.endswith((';', '{', '}')) and i + 1 < len(lines)):
            i += 1
            l += lines[i].strip()
        joined.append(l)
        i += 1

    # Collapse the known dropdown FILL LOOP into one marker. It is the same
    # six lines in every member that has it (verified byte-exact first in
    # 0x10048F10): walk the save table's 0x104-byte records from +4 and add
    # each one's name through the selector's +0x10 slot. The record address
    # is null-tested even though it cannot be null -- that is the original's.
    fill = re.compile(
        r'^iVar(\d+) = 0;$\n'
        r'^do \{$\n'
        r'^iVar(\d+) = \*\(int \*\)\(DAT_10ac5c60 \+ (0xc[04])\) \+ 4 \+ iVar\1;$\n'
        r'^if \(iVar\2 != 0\) \{$\n'
        r'^\(\*\*\(funcptr \*\)\(piVar\d+\[0xe0e\] \+ 0x10\)\)'
        r'\(iVar\2,0,1,&(DAT_\w+),([01])\);$\n'
        r'^\}$\n'
        r'^iVar\1 = iVar\1 \+ 0x104;$\n'
        r'^\} while \(iVar\1 < (\d+)\);$', re.M)
    # The SUBLINK trio. The draft reads w14 into a temp, bumps w2AB4, then
    # stores the temp + 1 -- but the matched siblings show the source is the
    # inline expression stored FIRST and the w2AB4 bump second (a named
    # short temp allocates to ax where the original uses dx).
    sub = re.compile(
        r'^sVar\d+ = \*\(short \*\)\(iVar\d+ \+ 0x14\);$\n'
        r'^\*\(short \*\)\(piVar\d+ \+ 0xaad\) = \(short\)piVar\d+\[0xaad\] \+ 1;$\n'
        r'^\*\(short \*\)\(\(int\)piVar\d+ \+ 0x2ab6\) = sVar\d+ \+ 1;$', re.M)
    # The inline STRCPY into the item label. Ghidra unrolls MSVC's
    # `repne scasb` + `rep movs` expansion into two loops; it is one
    # `strcpy(p->m2B5C.szName, src)` in the source. The item's vptr is read
    # in the middle of it and used straight after for the relayout vcall.
    cpy = re.compile(
        r'^uVar(\d+) = 0xffffffff;$\n'
        r'(?:^pcVar\d+ = (?:\(char \*\))?&(DAT_\w+);$\n)?'
        r'^do \{$\n(?:^.*$\n)*?^\} while \(cVar\d+ != .\\0.\);$\n'
        r'^uVar\1 = ~uVar\1;$\n'
        r'^iVar(\d+) = piVar\d+\[0xad7\];$\n'
        r'^pcVar\d+ = pcVar\d+ \+ -uVar\1;$\n'
        r'^pcVar\d+ = \(char \*\)\(\(int\)piVar\d+ \+ 0x2b65\);$\n'
        r'^for \(uVar\d+ = uVar\1 >> 2;.*$\n(?:^.*$\n)*?^\}$\n'
        r'^for \(uVar\1 = uVar\1 & 3;.*$\n(?:^.*$\n)*?^\}$\n', re.M)

    def _cpy(m):
        src = m.group(2)
        return ('@@LABELCPY %s\n' % (src if src else '?'))

    # The item RECT, written to the control's own +0x50 copy and to the
    # item's a424, then the measured width.
    rect = re.compile(
        r'^piVar(\d+)\[0x14\] = (0x[0-9a-f]+);$\n'
        r'^piVar\1\[0xbe0\] = \2;$\n'
        r'^piVar\1\[0x16\] = (0x[0-9a-f]+);$\n'
        r'^piVar\1\[0xbe2\] = \3;$\n'
        r'^piVar\1\[0x15\] = (0x[0-9a-f]+);$\n'
        r'^piVar\1\[0xbe1\] = \4;$\n'
        r'^piVar\1\[0x17\] = (0x[0-9a-f]+);$\n'
        r'^piVar\1\[0xbe3\] = \5;$\n'
        r'^\*\(short \*\)\(piVar\1 \+ 0xbde\) = '
        r'\(\(short\)piVar\1\[0xbe2\] - \(short\)piVar\1\[0xbe0\]\) \+ -0x10;$',
        re.M)

    blob = '\n'.join(joined)
    blob = sub.sub('@@SUBLINK', blob)
    blob = cpy.sub(_cpy, blob)
    blob = rect.sub(lambda m: '@@RECT %s %s %s %s'
                    % (m.group(2), m.group(4), m.group(3), m.group(5)), blob)
    m = fill.search(blob)
    while m:
        blob = blob[:m.start()] + ('@@FILL %s %s %s %s'
                                   % (m.group(3), m.group(4), m.group(6),
                                      m.group(5))) \
               + blob[m.end():]
        m = fill.search(blob)
    joined = blob.split('\n')

    for l in joined:
        if l.startswith('@@LABELCPY'):
            src = l.split()[1]
            if src == '?':
                if not PARTIAL[0]:
                    sys.exit('label strcpy with a computed source -- fill by hand')
                stmts.append(('raw', '@@UNHANDLED@@ strcpy source is computed'))
            else:
                externs.add(src)
                stmts.append(('raw', 'strcpy(p->m2B5C.szName, &%s);' % src))
            continue
        if l.startswith('@@RECT '):
            _, x0, y0, x1, y1 = l.split()
            stmts.append(('raw',
                'p->f050 = %s;\n    p->m2B5C.a424[0] = %s;\n'
                '    p->f058 = %s;\n    p->m2B5C.a424[2] = %s;\n'
                '    p->f054 = %s;\n    p->m2B5C.a424[1] = %s;\n'
                '    p->f05C = %s;\n    p->m2B5C.a424[3] = %s;\n'
                '    p->m2B5C.w41C = (short)(p->m2B5C.a424[2] - '
                'p->m2B5C.a424[0]) - 0x10;'
                % (x0, x0, x1, x1, y0, y0, y1, y1)))
            continue
        if l == '@@SUBLINK':
            stmts.append(('raw', 'p->w2AB6[0] = (short)(cont->w14 + 1);'))
            stmts.append(('raw', 'p->w2AB4 += 1;'))
            continue
        if l.startswith('@@FILL '):
            _, fld, dat, end, last = l.split()
            externs.add(dat); externs.add('@Root')
            fld = 'pTable' if fld == '0xc0' else 'pTableC4'
            stmts.append(('raw',
                '{\n        int off = 0;\n\n        do {\n'
                '            char *psz = &g_brRoot5C60->%s->aRecs[off];\n\n'
                '            if (psz != 0)\n'
                '                p->m3838.s4(psz, 0, 1, &%s, %s);\n'
                '            off += 0x104;\n'
                '        } while (off < %s);\n    }' % (fld, dat, last, end)))
            continue
        if not l or l in ('{', '}', 'else {') or l.startswith('//'):
            continue
        if re.match(r'^(void|int|short|char|funcptr|undefined|unsigned|'
                    r'\w+ \*?[a-z]\w*;)', l) and l.endswith(';') and '=' not in l:
            continue
        if re.match(r'^(\w*Stack_\w+|local_\d+|\*unaff_FS_OFFSET|'
                    r'pvVar\d+ = operator_new|if \(pvVar\d+|piVar\d+ = \(int \*\)'
                    r'(0x0|FUN_10040b10\(\))|iVar\d+ = (0|BrUiPageCtor_10048470\(\)|'
                    r'\*piVar\d+)|return 1)', l):
            continue
        if re.match(r'^if \((piVar\d+|iVar\d+) == (\(int \*\))?(0x0|0)\)', l) or \
           l == 'FUN_100378c0(4);':
            continue
        # Ghidra materialises the /GX state number as a local before the
        # epilogue restores fs:[0]; it is not a statement in the source.
        if re.match(r'^uVar\d+ = \d+;$', l):
            continue

        if re.match(r'^iVar\d+ = DAT_10ac5c60;$', l):
            continue          # the root-object pointer, re-read at the vcall
        m = re.match(r'^\(\*\*\(funcptr \*\)\(\*\*\(int \*\*\)\(iVar\d+ \+ '
                     r'(0xc[04])\) \+ 4\)\)\((\w+)\);$', l)
        if m:
            fld = 'pTable' if m.group(1) == '0xc0' else 'pTableC4'
            externs.add('$' + m.group(2))
            stmts.append(('raw', 'g_brRoot5C60->%s->s1(&%s);' % (fld, m.group(2))))
            externs.add('@Root')
            continue
        m = re.match(r'^\*\(short \*\)\((?:param_1|unaff_retaddr) \+ 0x12\) = 0;$', l)
        if m:
            stmts.append(('raw', 'parent->w12 = 0;')); continue
        m = re.match(r'^\*\(int \*\)\((?:param_1|unaff_retaddr) \+ 0x6c \+ .*\) = ([01]);$', l)
        if m:
            stmts.append(('raw', 'parent->a6C[parent->w10] = %s;' % m.group(1)))
            continue
        m = re.match(r'^\*\(int \*\)\((?:param_1|unaff_retaddr) \+ 0x14 \+ .*\) = iVar\d+;$', l)
        if m:
            stmts.append(('newpage', None)); continue
        m = re.match(r'^\*\(short \*\)\((?:param_1|unaff_retaddr) \+ 0x10\) = .*\+ 1;$', l)
        if m:
            stmts.append(('raw', 'parent->w10 += 1;')); continue
        m = re.match(r'^\*\(int \*\)\(iVar\d+ \+ 0x340\) = (?:param_1|unaff_retaddr);$', l)
        if m:
            stmts.append(('raw', 'cont->f340 = parent;')); continue
        m = re.match(r'^\*\(int \*\)\(iVar\d+ \+ 0x10\) = 0;$', l)
        if m:
            stmts.append(('raw', 'cont->f10 = 0;')); continue
        m = re.match(r'^\*\(int \*\)\(iVar\d+ \+ (0x33[8c])\) = (0x[0-9a-f]+);$', l)
        if m:
            stmts.append(('raw', 'cont->f%s = %s;'
                          % (m.group(1)[2:].upper(), f32(m.group(2)))))
            continue
        m = re.match(r'^\*\(int \*\*\)\(iVar\d+ \+ 0x18 \+ .*\) = piVar\d+;$', l)
        if m:
            stmts.append(('newctl', None)); continue
        m = re.match(r'^\(\*\*\(funcptr \*\)\((?:\*piVar\d+|iVar\d+) \+ 0x38\)\)'
                     r'\((.*)\);$', l)
        if m:
            try:
                a = [arg(t) for t in split_args(m.group(1))]
            except ValueError:
                if not PARTIAL[0]:
                    raise
                stmts.append(('raw', '@@UNHANDLED@@ %s' % l)); continue
            stmts.append(('raw', 'p->s38(%s);' % ', '.join(a))); continue
        m = re.match(r'^piVar\d+\[(\d)\] = \(int\)(\w+);$', l)
        if m:
            slot = int(m.group(1))
            if slot not in PFN:
                if PARTIAL[0]:
                    stmts.append(('raw', '@@UNHANDLED@@ %s' % l)); continue
                sys.exit('unknown pfn slot %d in: %s' % (slot, l))
            externs.add(m.group(2))
            stmts.append(('raw', 'p->%s = (CtlFn)%s;' % (PFN[slot], m.group(2))))
            continue
        m = re.match(r'^\*\(short \*\)\(piVar\d+ \+ 0x7883\) = (0x[0-9a-f]+|\d+);$', l)
        if m:
            stmts.append(('raw', 'p->w1E20C = %s;' % m.group(1))); continue
        m = re.match(r'^uVar\d+ = BrStrGet\((0x[0-9a-f]+|\d+),1,(\d),&(DAT_\w+)\);$', l)
        if m:
            externs.add(m.group(3))
            stmts.append(('pending_s34',
                          'p->s34(BrStrGet(%s), 1, %s, &%s);'
                          % (m.group(1), m.group(2), m.group(3))))
            continue
        m = re.match(r'^\(\*\*\(funcptr \*\)\(iVar\d+ \+ 0x34\)\)\(uVar\d+\);$', l)
        if m:
            stmts.append(('flush_s34', None)); continue
        m = re.match(r'^\(\*\*\(funcptr \*\)\(iVar\d+ \+ 0x34\)\)'
                     r'\(&(\w+),(\d),(\d),&(\w+)\);$', l)
        if m:
            externs.add(m.group(1)); externs.add(m.group(4))
            stmts.append(('raw', 'p->s34(&%s, %s, %s, &%s);'
                          % (m.group(1), m.group(2), m.group(3), m.group(4))))
            continue
        m = re.match(r'^\*\(short \*\)\(iVar\d+ \+ 0x14\) = .*\+ 1;$', l)
        if m:
            stmts.append(('raw', 'cont->w14 += 1;')); continue
        m = re.match(r'^\*\(short \*\)\(iVar\d+ \+ 0x344\) = .*\+ 1;$', l)
        if m:
            stmts.append(('raw', 'cont->w344 += 1;')); continue
        m = re.match(r'^\(\*\*\(funcptr \*\)\(piVar\d+\[0xe0e\] \+ 0x14\)\)'
                     r'\((.*)\);$', l)
        if m:
            a = split_args(m.group(1))
            for t in a:
                for d in re.findall(r'&(DAT_\w+)', t):
                    externs.add(d)
            a = [t.strip().replace('&', '&').replace('0xffffffff', '-1')
                 for t in a]
            stmts.append(('raw', 'p->m3838.s5(%s);' % ', '.join(a))); continue
        m = re.match(r'^(DAT_10a\w+) = (\d+|0x[0-9a-f]+);$', l)
        if m:
            externs.add('#' + m.group(1))
            stmts.append(('raw', '%s = %s;' % (m.group(1), m.group(2)))); continue
        if re.match(r'^\(\*\*\(funcptr \*\)\((?:iVar\d+|piVar\d+\[0xad7\]) \+ 4\)\)'
                    r'\(\);$', l):
            stmts.append(('raw', 'p->m2B5C.s1();')); continue
        m = re.match(r'^piVar\d+\[0xe0f\] = \(int\)(\w+);$', l)
        if m:
            externs.add(m.group(1))
            stmts.append(('raw', 'p->m3838.pfn04 = (int (*)(void))%s;'
                          % m.group(1))); continue
        m = re.match(r'^piVar\d+\[0xe13\] = \(int\)(\w+);$', l)
        if m:
            externs.add(m.group(1))
            stmts.append(('raw', 'p->m3838.f14 = (int)%s;' % m.group(1)))
            continue
        m = re.match(r'^piVar\d+\[0xbdf\] = (\d+);$', l)
        if m:
            stmts.append(('raw', 'p->m2B5C.f420 = %s;' % m.group(1))); continue
        m = re.match(r'^piVar\d+\[0x787d\] = 1;$', l)
        if m:
            stmts.append(('raw', 'p->f1E1F4 = 1;')); continue
        m = re.match(r'^((?:BrSub|FUN_|Br)\w+)\(\);$', l)
        if m:
            externs.add(m.group(1))
            stmts.append(('raw', '%s();' % m.group(1))); continue
        m = re.match(r'^((?:g_|DAT_)\w+) = piVar\d+;$', l)
        if m:
            externs.add('*' + m.group(1))
            stmts.append(('raw', '%s = p;' % m.group(1))); continue
        m = re.match(r'^(g_\w+) = (\d+|0x[0-9a-f]+);$', l)
        if m:
            externs.add('#' + m.group(1))
            stmts.append(('raw', '%s = %s;' % (m.group(1), m.group(2)))); continue
        m = re.match(r'^(g_\w+) = (g_\w+|DAT_\w+);$', l)
        if m:
            externs.add('#' + m.group(1)); externs.add('#' + m.group(2))
            stmts.append(('raw', '%s = %s;' % (m.group(1), m.group(2)))); continue
        m = re.match(r'^\*\(funcptr \*\)\(iVar\d+ \+ (4|8|0xc)\) = (\w+);$', l)
        if m:
            externs.add(m.group(2))
            fld = {'4': 'pfnA', '8': 'pfnB', '0xc': 'pfnC'}[m.group(1)]
            stmts.append(('raw', 'cont->%s = (int (*)(void))%s;'
                          % (fld, m.group(2)))); continue

        if PARTIAL[0]:
            # Emit a marker that CANNOT compile, so a half-generated file can
            # never be mistaken for a finished one. Fill each in by hand from
            # the asm, then score.
            stmts.append(('raw', '@@UNHANDLED@@ %s' % l))
            continue
        sys.exit('gen_menubuilder: unrecognised draft line -- read the asm for\n'
                 'this one instead of trusting the draft:\n    %s' % l)

    return name, stmts, externs


ENTRY = ('    p = new BrCtl;\n'
         '    cont->a18[cont->w14] = p;\n'
         '    bad = (p == 0);\n'
         '    if (bad)\n'
         '        FUN_100378c0(4);\n')

PAGE = ('    cont = new Page%s;\n'
        '    parent->a14[parent->w10] = cont;\n'
        '    bad = (cont == 0);\n'
        '    if (bad)\n'
        '        FUN_100378c0(4);\n')


PARTIAL = [False]
SFX = ['']


def emit(va, name, stmts, externs, size):
    sfx = va[4:].upper()
    SFX[0] = sfx
    ref = open(REFERENCE).read()
    classes = ref[ref.index('class GameUi;'):ref.index('typedef int (*CtlFn)')]
    classes = classes.replace('Page48F10', 'Page' + sfx).replace('48F10', sfx)

    out, pending = [], None
    for kind, payload in stmts:
        if kind == 'newpage':
            out.append(PAGE % sfx)
        elif kind == 'newctl':
            out.append(ENTRY)
        elif kind == 'pending_s34':
            pending = payload
        elif kind == 'flush_s34':
            if pending is None:
                if not PARTIAL[0]:
                    sys.exit('s34 call with no preceding BrStrGet')
                out.append('    @@UNHANDLED@@ s34 with no BrStrGet\n')
            else:
                out.append('    %s\n' % pending)
            pending = None
        else:
            out.append('    %s\n' % payload)

    ext = []
    for e in sorted(externs):
        if e.startswith('*'):
            ext.append('BrCtl *%s;' % e[1:])
        elif e.startswith('#'):
            ext.append('int %s;' % e[1:])
        elif e.startswith('$'):
            ext.append('extern char %s;' % e[1:])
        elif e == '@Root':
            ext.append('Root%s *g_brRoot5C60;   /* 0x10AC5C60 */' % SFX[0])
        elif e.startswith('DAT_') or e.startswith('g_a') or e.startswith('s_'):
            ext.append('extern char  %s;' % e)
        elif e.startswith('_DAT_'):
            ext.append('extern float %s;' % e[1:])
        else:
            ext.append('int %s();' % e)
    # the y-offset constants live in .rdata at 0x1007xxxx and are floats;
    # every other DAT_ the emitter references is a format/label string.
    seen = ' '.join(p or '' for _, p in stmts)
    for d in sorted(set(re.findall(r'\b(DAT_1007[0-9a-f]{4})\b', seen))):
        line = 'extern float %s;' % d
        if line not in ext:
            ext.append(line)

    body = ''.join(out).replace('_DAT_', 'DAT_')

    hdr = ('/* @implements %s glide %s\n'
           ' * @cpp_kind free\n'
           ' * @cpp_symbol ?%s@@YAHPAVGameUi@@@Z\n'
           ' *\n'
           ' * %d B cdecl EH-frame menu-page builder. Emitted by\n'
           ' * tools/gen_menubuilder.py from the Ghidra draft; the class\n'
           ' * layouts and the three family levers come from the hand-solved\n'
           ' * 0x100425E0 / 0x10048160 (char bool after the slot store, raw\n'
           ' * float pushes for simple lvalues, w14-then-w344 tails).\n'
           ' */\n'
           '#ifdef BR_MATCHING_BUILD\n'
           '#define _CRTIMP __declspec(dllimport)\n'
           '#include <string.h>\n'
           '#endif\n\n' % (va, name, name, size))

    return (hdr + classes + 'typedef int (*CtlFn)(BrCtl *);\n\nextern "C" {\n'
            + '\n'.join(ext)
            + '\nchar *BrStrGet(int);\nvoid FUN_100378c0(int);\n}\n\n'
            + 'int %s(GameUi *parent)\n{\n'
              '    Page%s *cont;\n    BrCtl     *p;\n    char       bad;\n\n'
              % (name, sfx)
            + body + '\n    return 1;\n}\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va')
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--partial', action='store_true',
                    help='scaffold: mark unrecognised lines with a\n                          deliberately uncompilable @@UNHANDLED@@ so\n                          they must be filled in by hand')
    args = ap.parse_args()
    PARTIAL[0] = args.partial

    va = args.va.lower()
    if not va.startswith('0x'):
        va = '0x' + va
    binp = os.path.join(ROOT, 'build', 'match', 'orig', '%s.bin' % va.upper())
    size = os.path.getsize(binp) if os.path.exists(binp) else 0

    name, stmts, externs = parse(va.upper())
    src = emit(va, name, stmts, externs, size)

    if not args.write:
        sys.stdout.write(src)
        return
    out = os.path.join(CPP_DIR, '0x%s.cpp' % va[2:].upper())
    open(out, 'w').write(src)
    print('wrote %s -- now score it with tools/cpp_score.py'
          % os.path.relpath(out, ROOT))


if __name__ == '__main__':
    main()
