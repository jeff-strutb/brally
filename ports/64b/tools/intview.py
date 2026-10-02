#!/usr/bin/env python3
"""Fix 32-bit integer views of pointer-typed storage.

`*(int *)&p` (or uint32_t / DWORD / unsigned) reads or writes four bytes of
an object that is a pointer.  At 32 bits that is the whole pointer; at 64
bits it is half of one.  From clang's AST of each file:

  write   *(int *)&p = v;     ->  p = (T)(intptr_t)(v);
  read    ... *(int *)&p ...  ->  ... ((intptr_t)(p)) ...

The read keeps an integer type (callers compare it, add to it, pass it
on) but the whole address.  Compound assignments and increments through
such a view are reported, not rewritten.

Usage: intview.py [--dry] [FILE...]
"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

ROOT = vm.ROOT
INTS = {'int', 'unsigned int', 'int32_t', 'uint32_t', 'DWORD', 'unsigned', 'long', 'unsigned long',
        'LONG', 'UINT', 'BOOL', 'signed int'}


def qt(n):
    t = n.get('type') or {}
    return t.get('desugaredQualType') or t.get('qualType', '')


def strip(n):
    while n.get('kind') in ('ParenExpr',) or (n.get('kind') == 'ImplicitCastExpr'):
        if not n.get('inner'):
            break
        n = n['inner'][0]
    return n


def int_view(n):
    """n is `*(INT *)&E` with E a pointer: return E's node, else None."""
    if n.get('kind') != 'UnaryOperator' or n.get('opcode') != '*':
        return None
    c = strip(n['inner'][0])
    if c.get('kind') != 'CStyleCastExpr':
        return None
    ct = re.sub(r'\s*\*$', '', (c.get('type') or {}).get('qualType', '')).strip()
    ct = re.sub(r'^(volatile |const )+', '', ct)
    if ct not in INTS:
        return None
    a = strip(c['inner'][0])
    if a.get('kind') != 'UnaryOperator' or a.get('opcode') != '&':
        return None
    e = a['inner'][0]
    et = qt(e)
    if not (et.rstrip().endswith('*') or '(*)' in et):
        return None
    return e


def main():
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not files:
        for dp, _, fs in os.walk('ports/64b/src/core'):
            files += [os.path.join(dp, f) for f in fs if f.endswith(('.c', '.cpp'))]
    import concurrent.futures
    total, notes = 0, []
    with concurrent.futures.ProcessPoolExecutor(14) as ex:
        for n, nt in ex.map(one, [(f, dry) for f in sorted(files)]):
            total += n
            notes += nt
    print('int views of pointers fixed: %d' % total)
    for n in notes:
        print('  ' + n)


def one(args):
    f, dry = args
    total, notes = 0, []
    if True:
        try:
            tree = vm.ast(f, [])
        except Exception:  # noqa: BLE001
            notes.append('%s: no AST' % f)
            return 0, notes
        text = open(f, encoding='latin-1').read()
        fpath = os.path.abspath(f)
        edits = []

        def rng(n):
            r = n.get('range') or {}
            b, e = r.get('begin', {}), r.get('end', {})
            for loc in (b, e):
                if 'spellingLoc' in loc or 'expansionLoc' in loc:
                    return None
                if 'file' in loc and os.path.abspath(loc['file']) != fpath:
                    return None
            if 'offset' not in b or 'offset' not in e:
                return None
            return b['offset'], e['offset'] + e.get('tokLen', 0)

        def visit(n, parent):
            if n.get('kind') == 'UnaryOperator' and n.get('opcode') == '&':
                c = n['inner'][0]
                while c.get('kind') == 'ParenExpr' and c.get('inner'):
                    c = c['inner'][0]
                c['_addr'] = True       # the view's own address: leave it
            if vm.CUR['file'] and os.path.abspath(vm.CUR['file']) != fpath:
                return
            k = n.get('kind')
            if k == 'BinaryOperator' and n.get('opcode') == '=':
                lhs = strip(n['inner'][0]) if n['inner'][0].get('kind') == 'ParenExpr' else n['inner'][0]
                e = int_view(lhs)
                if e is not None:
                    r, re_, rr = rng(n), rng(e), rng(n['inner'][1])
                    if r and re_ and rr:
                        et = (e.get('type') or {}).get('qualType', '')
                        edits.append((r[0], r[1], '%s = (%s)(intptr_t)(%s)' % (
                            text[re_[0]:re_[1]], et, text[rr[0]:rr[1]]), 'W'))
                    else:
                        notes.append('%s:%d write in a macro' % (f, text.count('\n', 0, (rng(n) or (0,))[0]) + 1))
                    n['_done'] = True
                    lhs['_done'] = True
                    return
            if k in ('CompoundAssignOperator',) or (k == 'UnaryOperator' and n.get('opcode') in ('++', '--')):
                tgt = strip(n['inner'][0])
                if int_view(tgt) is not None:
                    r = rng(n)
                    notes.append('%s:%d compound update through an int view' % (
                        f, text.count('\n', 0, r[0]) + 1 if r else 0))
                    tgt['_done'] = True
                return
            if k == 'UnaryOperator' and not n.get('_done') and not n.get('_addr'):
                e = int_view(n)
                if e is not None:
                    r, re_ = rng(n), rng(e)
                    if r and re_:
                        edits.append((r[0], r[1], '((intptr_t)(%s))' % text[re_[0]:re_[1]], 'R'))
        vm.CUR['file'] = None
        vm.walk(tree, visit)
        if not edits:
            return 0, notes
        # drop edits nested inside another edit
        edits.sort()
        keep, last = [], -1
        for e in edits:
            if e[0] >= last:
                keep.append(e)
                last = e[1]
            else:
                notes.append('%s: nested int view skipped' % f)
        out = text
        for b0, e0, rep, kind in sorted(keep, reverse=True):
            out = out[:b0] + rep + out[e0:]
        total += len(keep)
        if not dry:
            open(f, 'w', encoding='latin-1').write(out)
    return total, notes


if __name__ == '__main__':
    main()
