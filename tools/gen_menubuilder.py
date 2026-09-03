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
    if tok in ('param_1', 'unaff_retaddr'):
        return 'parent'
    m = re.match(r'^\*\(int \*\)\(iVar\d+ \+ (0x33[8c])\)$', tok)
    if m:
        return 'cont->f%s' % m.group(1)[2:].upper()
    m = re.match(r'^\*\(float \*\)\(iVar\d+ \+ 0x33c\) - _(DAT_[0-9a-f]+)$', tok)
    if m:
        return 'cont->f33C - %s' % m.group(1)
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


PFN = {1: 'pfn04', 2: 'pfn08', 3: 'pfn0C', 4: 'pfn10', 5: 'pfn14'}


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

    for l in joined:
        if not l or l in ('{', '}', 'else {') or l.startswith('//'):
            continue
        if re.match(r'^(void|int|short|char|funcptr|undefined|unsigned|'
                    r'\w+ \*?[a-z]\w*;)', l) and l.endswith(';') and '=' not in l:
            continue
        if re.match(r'^(uStack_c|iStack_c|local_4|puStack_8|\*unaff_FS_OFFSET|'
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

        m = re.match(r'^\*\(short \*\)\(param_1 \+ 0x12\) = 0;$', l)
        if m:
            stmts.append(('raw', 'parent->w12 = 0;')); continue
        m = re.match(r'^\*\(int \*\)\(param_1 \+ 0x6c \+ .*\) = ([01]);$', l)
        if m:
            stmts.append(('raw', 'parent->a6C[parent->w10] = %s;' % m.group(1)))
            continue
        m = re.match(r'^\*\(int \*\)\(param_1 \+ 0x14 \+ .*\) = iVar\d+;$', l)
        if m:
            stmts.append(('newpage', None)); continue
        m = re.match(r'^\*\(short \*\)\(param_1 \+ 0x10\) = .*\+ 1;$', l)
        if m:
            stmts.append(('raw', 'parent->w10 += 1;')); continue
        m = re.match(r'^\*\(int \*\)\(iVar\d+ \+ 0x340\) = param_1;$', l)
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
            a = [arg(t) for t in split_args(m.group(1))]
            stmts.append(('raw', 'p->s38(%s);' % ', '.join(a))); continue
        m = re.match(r'^piVar\d+\[(\d)\] = \(int\)(\w+);$', l)
        if m:
            slot = int(m.group(1))
            if slot not in PFN:
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
                     r'\(&(DAT_\w+),1,(\d),&(DAT_\w+)\);$', l)
        if m:
            externs.add(m.group(1)); externs.add(m.group(3))
            stmts.append(('raw', 'p->s34(&%s, 1, %s, &%s);'
                          % (m.group(1), m.group(2), m.group(3))))
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
        m = re.match(r'^piVar\d+\[0x787d\] = 1;$', l)
        if m:
            stmts.append(('raw', 'p->f1E1F4 = 1;')); continue
        m = re.match(r'^((?:BrSub|FUN_|Br)\w+)\(\);$', l)
        if m:
            externs.add(m.group(1))
            stmts.append(('raw', '%s();' % m.group(1))); continue
        m = re.match(r'^(g_\w+) = piVar\d+;$', l)
        if m:
            externs.add('*' + m.group(1))
            stmts.append(('raw', '%s = p;' % m.group(1))); continue

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


def emit(va, name, stmts, externs, size):
    sfx = va[4:].upper()
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
                sys.exit('s34 call with no preceding BrStrGet')
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
        elif e.startswith('DAT_'):
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
           ' */\n' % (va, name, name, size))

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
    args = ap.parse_args()

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
