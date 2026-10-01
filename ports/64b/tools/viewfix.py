#!/usr/bin/env python3
"""Member accesses through a file's own view of a table become the table's
fields.

A decompiled file often declares its own struct for memory another table
owns, at the address it saw:

    extern BrRaceCarSel g_aBrRaceCarSel[];     (0x10AF2094: car 0 + 0xE8C)
    ... g_aBrRaceCarSel[i].f11C ...

On a 64-bit build the view's layout and the table's no longer agree, so each
member access is rewritten to the canonical field at the same original
address:

    g_aBrRaceCar[i].lap

The member's offset is the view's ORIGINAL layout (an i686 compile of the
file with offsetof probes); the address is the view's declared one plus
that offset; the field is the canonical table's (stridefix.load_tables).
An indexed view must have the table's row size. Chains of `.member` and
constant subscripts are followed to the outermost access.

Usage: viewfix.py [--dry] FILE...
"""
import collections
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402
import recspec  # noqa: E402
import rewrite  # noqa: E402
import stridefix  # noqa: E402

ROOT = rawscan.ROOT
DRY = '--dry' in sys.argv


def probe(f, exprs):
    """Evaluate integer constant expressions in file f's ORIGINAL (i686)
    layout: {expr: value}."""
    if not exprs:
        return {}
    s = open(os.path.join(ROOT, f), encoding='latin-1').read()
    src = s + '\n' + ''.join('char __br_probe_%d[(%s) + 1];\n' % (i, e) for i, e in enumerate(exprs))
    key = f[len('ports/64b/src/core/'):].replace('/', '__')
    al = os.path.join(ROOT, 'build/portable/alias', key + '.h')
    lang = ['-x', 'c++', '-std=c++98'] if f.endswith('.cpp') else ['-x', 'c', '-std=gnu89']
    cmd = ['clang'] + recspec.I686 + recspec.FLAGS + ['-include', 'ports/64b/include/br_globals.h',
                                                      '-include', 'ports/64b/include/br_funcs.h']
    if os.path.exists(al):
        cmd += ['-include', al]
    cmd += ['-ferror-limit=0'] + lang + ['-Xclang', '-ast-dump=json', '-Xclang', '-ast-dump-filter=__br_probe_', '-']
    p = subprocess.run(cmd, input=src.encode('latin-1'), capture_output=True, cwd=ROOT)
    out = {}
    dec = json.JSONDecoder()
    t = p.stdout.decode('latin-1')
    i = 0
    while True:
        i = t.find('{', i)
        if i < 0:
            break
        try:
            obj, j = dec.raw_decode(t, i)
        except ValueError:
            i += 1
            continue
        i = j
        nm = obj.get('name', '')
        m = re.match(r'__br_probe_(\d+)$', nm)
        if m:
            mm = re.search(r'\[(\d+)\]', obj.get('type', {}).get('qualType', ''))
            if mm:
                out[exprs[int(m.group(1))]] = int(mm.group(1)) - 1
    return out


def main():
    os.chdir(ROOT)
    tabs, va_of = stridefix.load_tables()
    singles = []        # canonical non-array objects of a spec'd record
    import glob
    for h in glob.glob('ports/64b/include/*.h'):
        for m in re.finditer(r'^extern\s+(?:const\s+)?(?:struct\s+)?(\w+)\s+(\w+);\s*/\*\s*0x([0-9A-Fa-f]{8})',
                             open(h, encoding='latin-1').read(), re.M):
            if rewrite.spec(m.group(1)):
                singles.append((int(m.group(3), 16), m.group(2), m.group(1)))
    table_sizes = []
    for tva, nm, elem, dims in tabs:
        e = re.sub(r'^struct\s+', '', elem)
        if not rewrite.spec(e):
            continue
        es = int(rewrite.spec(e)[0]['size'], 16)
        tot = es
        for d in dims:
            tot *= d
        table_sizes.append((tva, tva + tot, nm, e, dims, es))
    for tva, nm, e in singles:
        es = int(rewrite.spec(e)[0]['size'], 16)
        table_sizes.append((tva, tva + es, nm, e, None, es))

    def owner(va):
        for a, z, nm, e, dims, es in table_sizes:
            if a <= va < z:
                return a, nm, e, dims, es
        return None

    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    total = 0
    left = collections.Counter()
    for f in files:
        a = rawscan.ast(f)
        if a is None:
            continue
        s = open(f, encoding='latin-1').read()
        fabs = os.path.abspath(f)
        cands = []

        def text(n):
            rg = stridefix.tokrange(s, n)
            return (rg, s[rg[0]:rg[1]]) if rg else (None, None)

        def chain(n, parents):
            """outermost '.'/[const] chain ending at n's top; returns
            (top node, root view name, index text or None, designator, member type)."""
            des = []
            cur = n
            k = len(parents) - 1
            # climb while the parent continues the chain
            while k >= 0:
                p = parents[k]
                if p.get('kind') == 'MemberExpr' and not p.get('isArrow') and strip_p(p['inner'][0]) is cur:
                    cur = p
                    k -= 1
                    continue
                if p.get('kind') == 'ArraySubscriptExpr' and strip_p(p['inner'][0]) is cur and \
                        stridefix.int_lit(p['inner'][1]) is not None and cur is not n:
                    cur = p
                    k -= 1
                    continue
                if p.get('kind') in ('ParenExpr', 'ImplicitCastExpr') and p.get('castKind', 'NoOp') in (
                        'NoOp', 'LValueToRValue', 'ArrayToPointerDecay') and k > 0 and \
                        parents[k - 1].get('kind') == 'ArraySubscriptExpr' and cur is not n:
                    k -= 1
                    continue
                break
            return cur

        def strip_p(x):
            while x.get('kind') in ('ParenExpr',) or (x.get('kind') == 'ImplicitCastExpr' and x.get('castKind') in (
                    'NoOp', 'LValueToRValue', 'ArrayToPointerDecay')):
                x = x['inner'][0]
            return x

        def visit(n, parents):
            if n.get('kind') == 'MemberExpr' and not n.get('isArrow'):
                b = strip_p(n['inner'][0])
                rg, bt = text(b)
                if bt is not None:
                    bt2 = re.sub(r'\s', '', bt)
                    m = re.match(r'^\(?([A-Za-z_]\w*)\)?(?:\[(.+)\])?$', bt2)
                    if m and m.group(1) in va_of:
                        top = chain(n, parents)
                        rgt, tt = text(top)
                        if rgt and rgt[0] == rg[0]:
                            vt = b.get('type', {}).get('qualType', '')
                            if m.group(2) is not None:
                                bb = strip_p(b['inner'][0]) if b.get('kind') == 'ArraySubscriptExpr' else None
                                vt = b.get('type', {}).get('qualType', '')
                            cands.append((rgt, m.group(1), bt[bt.index('[') + 1:bt.rindex(']')] if m.group(2) else None,
                                          tt[len(bt):] if tt.startswith(bt) else None, vt,
                                          top.get('type', {}).get('qualType', '')))
                            return
            for c in n.get('inner', []) or []:
                visit(c, parents + [n])

        for n in a.get('inner', []):
            loc = n.get('loc', {})
            fl = loc.get('file') or loc.get('spellingLoc', {}).get('file') or loc.get('expansionLoc', {}).get('file')
            if fl and os.path.abspath(os.path.join(ROOT, fl)) != fabs:
                continue
            visit(n, [])
        cands = [c for c in cands if c[3]]
        if not cands:
            continue
        exprs = []
        for rg, nm, idx, des, vt, mt in cands:
            vt0 = re.sub(r'\b(const|volatile)\b', '', vt).strip()
            exprs.append('__builtin_offsetof(%s, %s)' % (vt0, re.sub(r'^\.', '', re.sub(r'\s', '', des))))
            exprs.append('sizeof(%s)' % vt0)
        vals = probe(f, sorted(set(exprs)))
        edits = []
        for rg, nm, idx, des, vt, mt in cands:
            vt0 = re.sub(r'\b(const|volatile)\b', '', vt).strip()
            off = vals.get('__builtin_offsetof(%s, %s)' % (vt0, re.sub(r'^\.', '', re.sub(r'\s', '', des))))
            vsz = vals.get('sizeof(%s)' % vt0)
            if off is None or vsz is None:
                left['no original layout for %s' % vt0] += 1
                continue
            va = va_of[nm]
            o = owner(va)
            if not o:
                left['%s is in no spec\'d table' % nm] += 1
                continue
            tva, cn, rec, dims, es = o
            if nm == cn:
                continue            # the table itself
            if idx is not None and dims is None:
                left['%s indexed but %s is one object' % (nm, cn)] += 1
                continue
            if idx is not None and vsz != es:
                left['%s rows 0x%X != %s rows 0x%X' % (nm, vsz, cn, es) ] += 1
                continue
            rel = va - tva + off
            if dims is None:
                row = cn
                inrow = rel
            else:
                strides = []
                st = es
                for d in reversed(dims):
                    strides.insert(0, st)
                    st *= d
                subs = []
                r2 = rel
                for k2, stv in enumerate(strides):
                    i0, r2 = divmod(r2, stv)
                    parts = ['%d' % i0] if i0 else []
                    if idx is not None and k2 == len(strides) - 1:
                        parts.append('(%s)' % idx.strip() if re.search(r'\W', idx.strip()) else idx.strip())
                    subs.append('[%s]' % (' + '.join(parts) if parts else '0'))
                row = cn + ''.join(subs)
                inrow = r2
            path, fty = rewrite.resolve(rec, inrow, mt)
            if path is None:
                left['no %s field at +0x%X (%s)' % (rec, inrow, fty)] += 1
                continue
            lv = '%s.%s' % (row, path)
            if rewrite.same_type(fty, mt):
                txt = lv
            else:
                am = re.match(r'^(.*?)\s*\[(\w+)\]$', mt)
                if am:
                    txt = '(*(%s (*)[%s])&%s)' % (am.group(1), am.group(2), lv)
                else:
                    txt = '(*(%s *)&%s)' % (mt, lv)
            edits.append((rg[0], rg[1], txt))
        for a0, z, t in sorted(set(edits), reverse=True):
            s = s[:a0] + t + s[z:]
        if edits and not DRY:
            open(f, 'w', encoding='latin-1').write(s)
        if edits:
            print('%s: %d' % (f, len(edits)))
        total += len(edits)
    print('view member accesses rewritten: %d' % total)
    for why, c in left.most_common(40):
        print('  left: %s (%d)' % (why, c))


if __name__ == '__main__':
    main()
