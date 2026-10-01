#!/usr/bin/env python3
"""COM and C++ vtable calls written as byte offsets become slot indices.

The decompiler calls an interface method as

    (*(FnT *)(*(int *)pObj + 0x48))(pObj, ...)

-- the vtable pointer read as an int, plus the slot's BYTE offset. On a
64-bit build vtable slots are 8 bytes, so the same call is

    ((FnT)(*(void ***)(pObj))[0x48 / 4])(pObj, ...)

which is what this writes (slot = offset / 4, the original's 32-bit slot
size). The object expression is kept as written; when it is itself a raw
field read, rewrite.py's field naming should run first.

Usage: vtblfix.py [--dry] FILE...
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402
import stridefix  # noqa: E402

ROOT = rawscan.ROOT
DRY = '--dry' in sys.argv


def strip(n):
    while n.get('kind') in ('ParenExpr', 'ImplicitCastExpr'):
        n = n['inner'][0]
    return n


def strip_casts(n):
    while n.get('kind') in ('ParenExpr', 'ImplicitCastExpr', 'CStyleCastExpr'):
        n = n['inner'][0]
    return n


def main():
    os.chdir(ROOT)
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    total = 0
    for f in files:
        a = rawscan.ast(f)
        if a is None:
            continue
        s = open(f, encoding='latin-1').read()
        fabs = os.path.abspath(f)
        edits = []

        def text(n):
            rg = stridefix.tokrange(s, n)
            return (rg, s[rg[0]:rg[1]]) if rg else (None, None)

        def visit(n):
            if n.get('kind') == 'CallExpr' and n.get('inner'):
                callee = strip(n['inner'][0])
                if callee.get('kind') == 'UnaryOperator' and callee.get('opcode') == '*':
                    cast = strip(callee['inner'][0])
                    if cast.get('kind') == 'CStyleCastExpr':
                        ct = cast.get('type', {}).get('qualType', '')
                        plus = strip(cast['inner'][0])
                        if plus.get('kind') == 'BinaryOperator' and plus.get('opcode') == '+':
                            vt, k = plus['inner']
                            kv = stridefix.int_lit(k)
                            vt0 = strip_casts(vt)
                            if kv is not None and kv % 4 == 0 and vt0.get('kind') == 'UnaryOperator' and \
                                    vt0.get('opcode') == '*':
                                obj = vt0['inner'][0]
                                obj0 = strip_casts(obj)
                                rg, ot = text(obj0)
                                crg, _ = text(callee)
                                fnt = ct.strip()
                                if rg and crg and fnt.endswith('*'):
                                    fnt = fnt[:-1].strip()
                                    edits.append((crg[0], crg[1], '((%s)(*(void ***)(%s))[0x%X / 4])' % (
                                        fnt, ot, kv)))
            for c in n.get('inner', []) or []:
                visit(c)

        for n in a.get('inner', []):
            loc = n.get('loc', {})
            fl = loc.get('file') or loc.get('spellingLoc', {}).get('file') or loc.get('expansionLoc', {}).get('file')
            if fl and os.path.abspath(os.path.join(ROOT, fl)) != fabs:
                continue
            visit(n)
        keep, last = [], -1
        for e in sorted(set(edits), key=lambda e: (e[0], -e[1])):
            if e[0] >= last:
                keep.append(e)
                last = e[1]
        for a0, z, t in reversed(keep):
            s = s[:a0] + t + s[z:]
        if keep and not DRY:
            open(f, 'w', encoding='latin-1').write(s)
        if keep:
            print('%s: %d' % (f, len(keep)))
        total += len(keep)
    print('vtable calls rewritten to slots: %d' % total)


if __name__ == '__main__':
    main()
