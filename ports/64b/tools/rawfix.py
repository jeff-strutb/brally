#!/usr/bin/env python3
"""Anchor 32-bit byte offsets into shared records on a named field.

    (unsigned char *)pCar + 0x106C + 4u * i
 -> (unsigned char *)&pCar->f106C[0] + 4u * i

The decompiled bodies address records by the original's byte offsets.
For a record that holds pointers those offsets are wrong at 64 bits from
the first pointer on.  From clang's AST this finds every `(char-ish *)E
+ OFFSET` where E points to such a record, takes the constant part K of
OFFSET, finds the field containing K in the record's i386 layout, and
re-bases the arithmetic on that field's address; whatever followed K
(a scaled index) is kept, which is right when it stays inside a field
that holds no pointers -- the case for every array indexed this way.

Usage: rawfix.py [--dry] FILE...
"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

ROOT = vm.ROOT
CHARP = re.compile(r'^(const )?(unsigned char|char|uint8_t|signed char|BYTE) \*$')


def strip(n):
    while n.get('kind') in ('ParenExpr', 'ImplicitCastExpr') and n.get('inner'):
        n = n['inner'][0]
    return n


def const_split(n):
    """(K, rest_node_or_None) for an integer expression K [+ rest]."""
    n2 = strip(n)
    if n2.get('kind') == 'IntegerLiteral':
        return int(n2['value']), None
    if n2.get('kind') == 'BinaryOperator' and n2.get('opcode') == '+':
        a, b = n2['inner']
        a2, b2 = strip(a), strip(b)
        if a2.get('kind') == 'IntegerLiteral':
            return int(a2['value']), b
        if b2.get('kind') == 'IntegerLiteral':
            return int(b2['value']), a
    return None, None


def main():
    import concurrent.futures
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not files:
        for dp, _, fs in os.walk('ports/64b/src/core'):
            files += [os.path.join(dp, x) for x in fs if x.endswith(('.c', '.cpp'))]
    total, notes = 0, []
    with concurrent.futures.ProcessPoolExecutor(14) as ex:
        for k, nt in ex.map(one, [(f, dry) for f in sorted(files)]):
            total += k
            notes += nt
    print('anchored: %d' % total)
    for nt in notes:
        print('  ' + nt)


def one(args):
    f, dry = args
    total, notes = 0, []
    if True:
        vm.load_ptr_typedefs(f, [])
        recs, sizes = vm.layouts(f, [])
        tree = vm.ast(f, [])
        text = open(f, encoding='latin-1').read()
        fpath = os.path.abspath(f)
        edits = []

        def has_ptr(r, seen=()):
            for o, d, t, nme in recs.get(r, []):
                if vm.is_ptr(t):
                    return True
            return False

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
            if vm.CUR['file'] and os.path.abspath(vm.CUR['file']) != fpath:
                return
            if n.get('kind') != 'BinaryOperator' or n.get('opcode') != '+':
                return
            lhs, rhs = n['inner']
            c = strip(lhs)
            if c.get('kind') != 'CStyleCastExpr':
                return
            if not CHARP.match((c.get('type') or {}).get('qualType', '')):
                return
            e = c['inner'][0]
            et = ((e.get('type') or {}).get('desugaredQualType') or (e.get('type') or {}).get('qualType', ''))
            m = re.match(r'^(?:const )?(?:struct |class )?(\w+) \*$', et)
            if not m or m.group(1) not in recs or not has_ptr(m.group(1)):
                return
            rec = m.group(1)
            k, rest = const_split(rhs)
            r_all, r_e = rng(n), rng(e)
            if k is None or r_all is None or r_e is None:
                if r_all:
                    notes.append('%s:%d non-constant offset into %s' % (f, text.count('\n', 0, r_all[0]) + 1, rec))
                return
            res = vm.resolve(recs, sizes, rec, k, None)
            if res is None:
                notes.append('%s:%d +0x%X not in %s' % (f, text.count('\n', 0, r_all[0]) + 1, k, rec))
                return
            path, leaf, rem = res
            # anchor on the field the offset lands in: the whole field when
            # the offset is its start, else its first element / its start
            if vm.is_ptr(leaf) and rem:
                notes.append('%s:%d +0x%X inside a pointer field of %s' % (f, text.count('\n', 0, r_all[0]) + 1, k, rec))
                return
            ct = c['type']['qualType']
            anchor = '(%s)&((%s *)(%s))->%s' % (ct, rec, text[r_e[0]:r_e[1]], path)
            if rem:
                anchor = '(%s + %d)' % (anchor, rem)
            if rest is not None:
                r_r = rng(rest)
                if r_r is None:
                    return
                rep = '(%s + %s)' % (anchor, text[r_r[0]:r_r[1]])
            else:
                rep = '(%s)' % anchor
            edits.append((r_all[0], r_all[1], rep))
        vm.CUR['file'] = None
        vm.walk(tree, visit)
        edits.sort()
        keep, last = [], -1
        for ed in edits:
            if ed[0] >= last:
                keep.append(ed)
                last = ed[1]
        out = text
        for b0, e0, rep in sorted(keep, reverse=True):
            out = out[:b0] + rep + out[e0:]
        total += len(keep)
        if keep and not dry:
            open(f, 'w', encoding='latin-1').write(out)
    return total, notes


if __name__ == '__main__':
    main()
