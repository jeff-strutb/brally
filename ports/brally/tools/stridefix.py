#!/usr/bin/env python3
"""Turn strided table accesses through interior globals into indexed fields.

The decompiler names a table's fields one global per field and indexes the
table by bytes:

    *(int *)((char *)&DAT_1021ce84 + slot * 0x978)      (field +0x2C of row slot)
    (&DAT_10661844)[param_1 * 0x36]                      (int-scaled: 0x36 * 4)
    (BrCarState *)((char *)(DAT_1021ceb0) + slot * 0x978)

On a 64-bit build the rows are not 0x978 bytes and the globals are not
adjacent, so every such access becomes the row and field it always meant:

    g_aBrNetSlot[slot].f02C
    g_aBrTexSlot[param_1].bLive
    g_aBrNetSlot[slot].cars

The global's ADDRESS is taken from the name written in the source (an alias
name keeps the address it was declared at), so the field is found by offset
in the canonical table that holds it: the record's spec names it. Only a
stride equal to a row (or an outer dimension) of that table is rewritten;
anything else is reported and left alone.

Usage: stridefix.py [--dry] FILE...
"""
import collections
import csv
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402
import recspec  # noqa: E402
import rewrite  # noqa: E402

ROOT = rawscan.ROOT
DRY = '--dry' in sys.argv


def load_tables():
    """canonical arrays: [(va, name, elem type, dims)] and name -> va."""
    tabs = []
    for h in glob.glob(os.path.join(ROOT, 'ports/brally/include/*.h')):
        for m in re.finditer(r'^extern\s+(?:const\s+)?([\w\s\*]+?)\s+(\w+)((?:\[\w*\])+);\s*/\*\s*0x([0-9A-Fa-f]{8})',
                             open(h, encoding='latin-1').read(), re.M):
            dims = re.findall(r'\[(\w*)\]', m.group(3))
            if not all(d and re.match(r'(0x[0-9a-fA-F]+|\d+)$', d) for d in dims):
                continue
            tabs.append((int(m.group(4), 16), m.group(2), m.group(1).strip(), [int(d, 0) for d in dims]))
    va_of = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'ports/brally/types/globals.csv'))):
        if r['va']:
            va_of.setdefault(r['name'], int(r['va'], 16))
    return tabs, va_of


def size32(t):
    try:
        return recspec.size32(t, [])
    except SystemExit:
        return None


def strip(n):
    while n.get('kind') in ('ParenExpr',) or (n.get('kind') == 'ImplicitCastExpr' and n.get('castKind') in (
            'NoOp', 'LValueToRValue', 'IntegralCast')):
        n = n['inner'][0]
    return n


def tokrange(s, n):
    """(a, z) of n's text in the file, expansion-aware."""
    r = n.get('range', {})
    b, e = r.get('begin', {}), r.get('end', {})
    b = b.get('expansionLoc', b)
    e = e.get('expansionLoc', e)
    if 'offset' not in b or 'offset' not in e:
        return None
    return b['offset'], e['offset'] + e.get('tokLen', 0)


def int_lit(n):
    n = strip(n)
    if n.get('kind') == 'IntegerLiteral':
        return int(n['value'])
    if n.get('kind') == 'CStyleCastExpr':
        return int_lit(n['inner'][0])
    return None


def main():
    os.chdir(ROOT)
    tabs, va_of = load_tables()
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    total = 0
    skipped = collections.Counter()
    for f in files:
        a = rawscan.ast(f)
        if a is None:
            continue
        s = open(f, encoding='latin-1').read()
        fabs = os.path.abspath(f)
        edits = []

        def base_token(n):
            """global name written as the base (`&NAME`, `NAME`, `(NAME)`)."""
            rg = tokrange(s, n)
            if not rg:
                return None
            t = re.sub(r'\s', '', s[rg[0]:rg[1]])
            t = re.sub(r'^\((char\*|unsignedchar\*|uint8_t\*|int\*|void\*)\)', '', t)
            m = re.match(r'^\(*&?\(*([A-Za-z_]\w*)\)*$', t)
            return m.group(1) if m and m.group(1) in va_of else None

        def table_for(va):
            for tva, nm, elem, dims in tabs:
                esz = size32(elem)
                if not esz:
                    continue
                total_sz = esz
                for d in dims:
                    total_sz *= d
                if tva <= va < tva + total_sz:
                    return tva, nm, elem, dims, esz
            return None

        def decompose(plus):
            """('+' chain) -> (base node, pointee width, [(idx text, scale)], const) or None."""
            terms = []

            def flat(n):
                n = strip(n)
                if n.get('kind') == 'BinaryOperator' and n.get('opcode') == '+':
                    flat(n['inner'][0])
                    flat(n['inner'][1])
                else:
                    terms.append(n)
            flat(plus)
            base = [t for t in terms if t.get('type', {}).get('qualType', '').rstrip().endswith('*') or
                    '[' in t.get('type', {}).get('qualType', '')]
            if len(base) != 1:
                return None
            pt = plus.get('type', {}).get('qualType', '')
            if not pt.rstrip().endswith('*'):
                return None
            w = size32(pt.rstrip()[:-1].strip()) if not re.match(r'^(const\s+)?void\s*\*$', pt.strip()) else 1
            if not w:
                return None
            idx, const = [], 0
            for t in terms:
                if t is base[0]:
                    continue
                v = int_lit(t)
                if v is not None:
                    const += v
                    continue
                t2 = strip(t)
                if t2.get('kind') == 'BinaryOperator' and t2.get('opcode') in ('*', '<<'):
                    l, r = t2['inner']
                    lv, rv = int_lit(l), int_lit(r)
                    if t2['opcode'] == '<<' and rv is not None:
                        rg = tokrange(s, l)
                        idx.append((s[rg[0]:rg[1]], 1 << rv))
                        continue
                    if t2['opcode'] == '*' and (lv is not None or rv is not None):
                        e = r if lv is not None else l
                        rg = tokrange(s, e)
                        idx.append((s[rg[0]:rg[1]], lv if lv is not None else rv))
                        continue
                return None
            return base[0], w, idx, const

        def build(tok, w, idx, const, want, as_addr):
            va = va_of[tok] + const * w
            tb = table_for(va_of[tok])
            if not tb:
                skipped['no table holds %s' % tok] += 1
                return None
            tva, cn, elem, dims, esz = tb
            strides = []
            st = esz
            for d in reversed(dims):
                strides.insert(0, st)
                st *= d
            # index term per dimension
            per = [[] for _ in dims]
            for txt, sc in idx:
                b = sc * w
                if b not in strides:
                    skipped['stride 0x%X is not a row of %s' % (b, cn)] += 1
                    return None
                per[strides.index(b)].append(txt)
            off = va - tva
            subs = []
            for k, stv in enumerate(strides):
                i0, off = divmod(off, stv)
                parts = (['%d' % i0] if i0 else []) + ['(%s)' % t if re.search(r'\W', t.strip()) else t.strip()
                                                         for t in per[k]]
                subs.append('[%s]' % (' + '.join(parts) if parts else '0'))
            row = cn + ''.join(subs)
            if rewrite.spec(re.sub(r'^struct\s+', '', elem)):
                path, fty = rewrite.resolve(re.sub(r'^struct\s+', '', elem), off, want or 'char')
            elif off == 0:
                path, fty = '', elem
            else:
                skipped['offset into a scalar table %s' % cn] += 1
                return None
            if path is None:
                skipped['no field at +0x%X of %s' % (off, elem)] += 1
                return None
            lv = row + ('.' + path if path else '')
            return lv, fty

        def visit(n, parents):
            k = n.get('kind')
            done = False
            if k == 'BinaryOperator' and n.get('opcode') == '+' and not (
                    parents and strip(parents[-1]) is not parents[-1] and False):
                # only the top of a '+' chain
                par = parents[-1] if parents else {}
                if not (par.get('kind') == 'BinaryOperator' and par.get('opcode') == '+') and \
                        not (par.get('kind') == 'ParenExpr' and len(parents) > 1 and
                             parents[-2].get('kind') == 'BinaryOperator' and parents[-2].get('opcode') == '+'):
                    d = decompose(n)
                    if d and d[2]:
                        base, w, idx, const = d
                        tok = base_token(base)
                        if tok:
                            done = handle(n, parents, tok, w, idx, const)
            if k == 'ArraySubscriptExpr':
                b, i = n['inner']
                tok = base_token(strip(b) if strip(b).get('kind') != 'ImplicitCastExpr' else strip(b)['inner'][0])
                if tok:
                    t = n.get('type', {}).get('qualType', '')
                    w = size32(t)
                    i2 = strip(i)
                    idx, const = [], 0
                    terms = []

                    def fl(x):
                        x = strip(x)
                        if x.get('kind') == 'BinaryOperator' and x.get('opcode') == '+':
                            fl(x['inner'][0])
                            fl(x['inner'][1])
                        else:
                            terms.append(x)
                    fl(i2)
                    ok = bool(w)
                    for x in terms:
                        v = int_lit(x)
                        if v is not None:
                            const += v
                        elif x.get('kind') == 'BinaryOperator' and x.get('opcode') == '*' and \
                                (int_lit(x['inner'][0]) is not None or int_lit(x['inner'][1]) is not None):
                            l, r = x['inner']
                            sc = int_lit(l) if int_lit(l) is not None else int_lit(r)
                            e = r if int_lit(l) is not None else l
                            rg = tokrange(s, e)
                            idx.append((s[rg[0]:rg[1]], sc))
                        else:
                            ok = False
                    if ok and idx:
                        res = build(tok, w, idx, const, t, False)
                        rg = tokrange(s, n)
                        if res and rg:
                            lv, fty = res
                            text = lv if rewrite.same_type(fty, t) else '(*(%s *)&%s)' % (t, lv)
                            edits.append((rg[0], rg[1], text))
                            done = True
            if not done:
                for c in n.get('inner', []) or []:
                    visit(c, parents + [n])

        def handle(plus, parents, tok, w, idx, const):
            # climb: casts to T*, then an optional '*'
            node, i = plus, len(parents) - 1
            cast_t = None
            while i >= 0 and parents[i].get('kind') in ('ParenExpr', 'CStyleCastExpr', 'ImplicitCastExpr'):
                if parents[i].get('kind') == 'CStyleCastExpr':
                    cast_t = parents[i].get('type', {}).get('qualType')
                node = parents[i]
                i -= 1
            deref = i >= 0 and parents[i].get('kind') == 'UnaryOperator' and parents[i].get('opcode') == '*'
            pt = (cast_t or plus.get('type', {}).get('qualType', '')).strip()
            if not pt.endswith('*'):
                return False
            want = pt[:-1].strip()
            res = build(tok, w, idx, const, want, not deref)
            if not res:
                return False
            lv, fty = res
            if deref:
                outer = parents[i]
                text = lv if rewrite.same_type(fty, want) else '(*(%s *)&%s)' % (want, lv)
            else:
                outer = node
                if rewrite.same_type(fty, want):
                    text = '(&%s)' % lv
                elif re.match(r'.*\[\w+\]$', fty or '') and rewrite.same_type(re.sub(r'\s*\[\w+\]$', '', fty), want):
                    text = '(%s)' % lv          # an array field decays to its element pointer
                else:
                    text = '((%s)&%s)' % (pt, lv)
            rg = tokrange(s, outer)
            if not rg:
                return False
            edits.append((rg[0], rg[1], text))
            return True

        for n in a.get('inner', []):
            loc = n.get('loc', {})
            fl = loc.get('file') or loc.get('spellingLoc', {}).get('file') or loc.get('expansionLoc', {}).get('file')
            if fl and os.path.abspath(os.path.join(ROOT, fl)) != fabs:
                continue
            visit(n, [])
        # apply bottom-up, dropping overlaps (outer edits win)
        edits = sorted(set(edits), key=lambda e: (e[0], -e[1]))
        keep, last = [], -1
        for e in edits:
            if e[0] >= last:
                keep.append(e)
                last = e[1]
        for a0, z, t in reversed(keep):
            s = s[:a0] + t + s[z:]
        if keep and not DRY:
            open(f, 'w', encoding='latin-1').write(s)
        total += len(keep)
        if keep:
            print('%s: %d' % (f, len(keep)))
    print('strided accesses rewritten: %d' % total)
    for why, c in skipped.most_common(30):
        print('  left: %s (%d)' % (why, c))


if __name__ == '__main__':
    main()
