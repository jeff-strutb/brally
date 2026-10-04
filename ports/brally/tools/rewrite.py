#!/usr/bin/env python3
"""Turn raw-offset memory accesses into named fields.

For every `*(T *)(base + K)`, `((T *)(base + K))[i]` or `(T *)(base + K)`
in a core file, where base points at a struct with a spec
(ports/brally/types/<Rec>.spec):

  * the struct is known from base's own type (`Rec *`), or from
    ports/brally/types/decisions.csv (file, function, variable -> Rec, delta)
    for variables the decompiler left as int or a byte pointer; such a
    variable is retyped to `Rec *` in its declaration;
  * K (+ delta) is looked up in the spec, descending into nested structs
    that have specs of their own;
  * the access becomes `base->field`, or `*(T *)&base->field` when the code
    reads the field as a different type than it is declared (the original's
    exact semantics).

Offsets that fall in padding are reported (and with --absorb added to the
spec as fXXXX of the accessed type, a pointer type when the code uses the
value as an address). Accesses written inside macros are reported, not
rewritten.

Usage: rewrite.py [--absorb] [--dry] FILE...
"""
import argparse
import collections
import csv
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recspec  # noqa: E402
import rawscan  # noqa: E402

ROOT = recspec.ROOT
TYPES = recspec.TYPES
DECISIONS = os.path.join(TYPES, 'decisions.csv')

_specs = {}


def spec(rec):
    if rec not in _specs:
        p = recspec.spec_path(rec)
        _specs[rec] = recspec.load(rec) if os.path.exists(p) else None
    return _specs[rec]


def rec_of_type(qt):
    """The spec'd struct a pointer type points at, or None."""
    t = re.sub(r'\b(const|volatile)\b', '', qt).strip()
    m = re.match(r'(?:struct\s+)?(\w+)\s*\*$', t)
    if m and spec(m.group(1)):
        return m.group(1)
    return None


INT_CLASS = {}
for names, cls in ((('int', 'int32_t', 'LONG', 'BOOL', 'INT', 'long', 'signed int'), 'i32'),
                   (('unsigned int', 'uint32_t', 'DWORD', 'UINT', 'unsigned', 'ULONG', 'unsigned long'), 'u32'),
                   (('short', 'int16_t', 'SHORT', 'signed short'), 'i16'),
                   (('unsigned short', 'uint16_t', 'WORD', 'USHORT'), 'u16'),
                   (('char', 'signed char', 'int8_t', 'CHAR'), 'i8'),
                   (('unsigned char', 'uint8_t', 'BYTE', 'UCHAR'), 'u8'),
                   (('float', 'FLOAT'), 'f32'), (('double',), 'f64')):
    for n in names:
        INT_CLASS[n] = cls


def norm(t):
    t = re.sub(r'\b(const|volatile|struct)\b', '', t)
    t = re.sub(r'\s+', ' ', t).strip()
    t = re.sub(r'\s*\*', '*', t)
    stars = len(t) - len(t.rstrip('*'))
    b = t.rstrip('*').strip()
    return INT_CLASS.get(b, b) + '*' * stars


def same_type(a, b):
    na, nb = norm(a), norm(b)
    if na == nb:
        return True
    return na.endswith('*') and nb.endswith('*') and (na == 'void*' or nb == 'void*')


PADDING = {}     # (rec, k, want) -> innermost (rec, k) that has padding there


def resolve_in(ty, d, want, hdrs):
    """Path suffix (starting with '[' or '.') and type for offset d inside a
    value of type ty, or (None, why)."""
    am = re.match(r'(.*?)\s*\[(\w+)\]((?:\[\w+\])*)$', ty)
    if am:
        elem = am.group(1) + am.group(3)
        esz = recspec.size32(elem, hdrs)
        i, d2 = divmod(d, esz)
        if d2 == 0 and (same_type(elem, want) or not re.search(r'\[|struct|^Br', elem) or
                        re.sub(r'\s', '', elem) == re.sub(r'\s', '', want)):
            return '[%d]' % i, elem
        p, t = resolve_in(elem, d2, want, hdrs)
        if p is not None:
            return '[%d]%s' % (i, p), t
        return None, t
    sub = rec_of_type(re.sub(r'^struct\s+', '', ty) + ' *')
    if sub:
        p, t = resolve(sub, d, want)
        if p:
            return '.' + p, t
        return None, t
    if d == 0:
        return '', ty
    return None, 'inside %s at +%d' % (ty, d)


def resolve(rec, k, want, depth=0):
    """(path, field type) of the field of rec at offset k, or (None, why)."""
    sp = spec(rec)
    if sp is None:
        return None, 'no spec for %s' % rec
    meta, fields = sp
    hdrs = meta.get('headers', '').split()
    for f in fields:
        sz = recspec.size32(f.ty, hdrs)
        if not (f.off <= k < f.off + sz):
            continue
        d = k - f.off
        if d == 0 and (same_type(f.ty, want) or re.sub(r'\s', '', f.ty) == re.sub(r'\s', '', want)):
            return f.name, f.ty
        p, t = resolve_in(f.ty, d, want, hdrs)
        if p is not None:
            return f.name + p, t
        if isinstance(t, str) and t.startswith('padding@'):
            return None, t
        return None, t
    return None, 'padding@%s@%d' % (rec, k)


def load_decisions():
    out = {}
    if os.path.exists(DECISIONS):
        for r in csv.DictReader(open(DECISIONS)):
            out[(r['file'], r['func'], r['base'])] = (r['record'], int(r['delta'] or '0', 0))
    return out


def src_text(s, n):
    r = n.get('range', {})
    b, e = r.get('begin', {}), r.get('end', {})
    if 'offset' not in b or 'offset' not in e or 'spellingLoc' in b or 'expansionLoc' in b:
        return None, None, None
    a, z = b['offset'], e['offset'] + e.get('tokLen', 0)
    return a, z, s[a:z]


def pointee(qt):
    t = qt.strip()
    assert t.endswith('*'), qt
    return t[:-1].rstrip()


def process(f, decisions, absorb, dry, report):
    a = rawscan.ast(f)
    if a is None:
        report['noast'].append(f)
        return 0
    path = os.path.join(ROOT, f)
    s = open(path, encoding='latin-1').read()
    edits = []          # (start, end, text)
    retype = {}         # decl id -> (start, end, text)
    fabs = os.path.abspath(path)

    def infile(n):
        loc = n.get('loc') or n.get('range', {}).get('begin', {})
        fl = loc.get('file')
        return fl is None or os.path.abspath(os.path.join(ROOT, fl)) == fabs

    decls = {}
    consumed = set()
    rscale, rdelta = {}, {}
    arith = []          # (node, parents) arithmetic on a DeclRef

    assigns = []

    def walk(n, fn, parents):
        if n.get('kind') == 'BinaryOperator' and n.get('opcode') == '=':
            assigns.append((n, fn))
        if n.get('kind') in ('BinaryOperator', 'CompoundAssignOperator') and n.get('opcode') in (
                '+', '-', '+=', '-='):
            arith.append((n, fn))
        if n.get('kind') == 'UnaryOperator' and n.get('opcode') in ('++', '--'):
            arith.append((n, fn))
        k = n.get('kind')
        if k in ('ParmVarDecl', 'VarDecl'):
            decls[n.get('id')] = n
        if k in ('FunctionDecl', 'CXXMethodDecl', 'CXXConstructorDecl', 'CXXDestructorDecl') and 'inner' in n:
            fn = n.get('name', '?')
        if k in ('CStyleCastExpr', 'ImplicitCastExpr') and n.get('castKind') in ('IntegralToPointer', 'BitCast', 'NoOp') \
                and rawscan.qt(n).rstrip().endswith('*') and (k == 'CStyleCastExpr' or n.get('castKind') != 'NoOp'):
            handle(n, fn, parents)
        if k == 'ArraySubscriptExpr' or (k == 'UnaryOperator' and n.get('opcode') == '*'):
            handle_plain(n, fn, parents)
        for c in n.get('inner', []) or []:
            if isinstance(c, dict) and (parents or infile(c) or c.get('kind') not in (
                    'FunctionDecl', 'VarDecl', 'RecordDecl', 'TypedefDecl', 'CXXRecordDecl')):
                walk(c, fn, parents + [n])

    def handle(n, fn, parents):
        op = n['inner'][0]
        def eat(x):
            x = rawscan.strip_parens(x)
            if x.get('kind') == 'BinaryOperator' and x.get('opcode') in ('+', '-', '*'):
                consumed.add(x.get('id'))
                for c in x.get('inner', []):
                    eat(c)
        eat(op)
        idx = []
        base, off = rawscan.base_of(op, idx)
        bt = rawscan.qt(base)
        sb = rawscan.strip_parens(base)
        if sb.get('kind') == 'BinaryOperator' and sb.get('opcode') in ('+', '-') and rec_of_type(bt):
            report['unresolved'][('?', off, 'byte term on a typed base')].append((f, fn))
            return
        rec = rec_of_type(bt)
        delta = 0
        bname = ''
        bk, bn, bid = rawscan.describe(base)
        scale = 1
        if rec is None:
            nb = norm(bt)
            dec = decisions.get((f, fn or '', bn)) or decisions.get((f, '', bn))
            if n.get('castKind') != 'IntegralToPointer' and nb not in ('i8*', 'u8*', 'void*'):
                if not (dec and nb in ('i32*', 'u32*', 'i16*', 'u16*', 'f32*')):
                    return
                scale = {'i32*': 4, 'u32*': 4, 'f32*': 4, 'i16*': 2, 'u16*': 2}[nb]
            if not dec:
                return
            rec, delta = dec
            if bk in ('ParmVarDecl', 'VarDecl'):
                bname = bn
                retype[bid] = rec
                rscale[bid] = scale
                rdelta[bid] = delta
            else:
                return
        if off == 0 and n.get('castKind') in ('BitCast', 'NoOp') and rec_of_type(bt):
            return      # a plain pointer cast of a typed base: not raw
        want = pointee(rawscan.qt(n))
        k = off * scale + delta
        sub = ''
        inner_idx = None
        if idx:
            rsize = int(spec(rec)[0]['size'], 16)
            if len(idx) != 1:
                report['unresolved'][(rec, k, 'index*%s' % ','.join(hex(c) for _, c in idx))].append((f, fn))
                return
            ia, iz, itxt = src_text(s, idx[0][0])
            if itxt is None:
                report['macro'].append((f, fn, rec, hex(k)))
                return
            if idx[0][1] == rsize:
                sub = '[%s]' % itxt
            else:
                inner_idx = (itxt, idx[0][1])
        pth, fty = resolve(rec, k, want)
        a0, z0, _ = src_text(s, n)
        if a0 is None:
            report['macro'].append((f, fn, rec, hex(k)))
            return
        if pth is None:
            if isinstance(fty, str) and fty.startswith('padding@'):
                _, prec, pk = fty.split('@')
                report['unresolved'][(prec, int(pk), want)].append((f, fn))
            else:
                report['unresolved'][(rec, k, want)].append((f, fn))
            return
        if inner_idx:
            # an array inside the record, indexed: the element at k must be
            # one of an array whose element size is the stride
            m = re.match(r'^(.*)\[(\d+)\]$', pth)
            esz = None
            if m:
                try:
                    hdrs = spec(rec)[0].get('headers', '').split()
                    esz = recspec.size32(fty, hdrs)
                except SystemExit:
                    esz = None
            if not m or esz != inner_idx[1]:
                report['unresolved'][(rec, k, 'index*0x%X' % inner_idx[1])].append((f, fn))
                return
            n0 = int(m.group(2))
            pth = '%s[%s%s]' % (m.group(1), ('%d + ' % n0) if n0 else '', inner_idx[0])
        if bname:
            btxt = bname
        else:
            ba, bz, btxt = src_text(s, rawscan.strip_parens(base))
            if btxt is None:
                report['macro'].append((f, fn, rec, hex(k)))
                return
            if not re.match(r'^[\w.>\-\[\]]+$', btxt):
                btxt = '(%s)' % btxt
        field = ('%s%s.%s' % (btxt, sub, pth)) if sub else ('%s->%s' % (btxt, pth))
        par = parents[-1] if parents else {}
        exact = same_type(fty, want)
        fptr = fty.rstrip().endswith('*') or fty.startswith('fnptr')
        wptr = want.rstrip().endswith('*')
        if par.get('kind') == 'UnaryOperator' and par.get('opcode') == '*':
            ua, uz, _ = src_text(s, par)
            if ua is None:
                report['macro'].append((f, fn, rec, hex(k)))
                return
            if exact or (fptr and wptr):
                text = field if exact else '*(%s *)&%s' % (want, field)
            elif fptr and norm(want) in ('i32', 'u32'):
                # a pointer field read as a 32-bit int: what happens to the value?
                j = len(parents) - 2
                child = par
                while j >= 0 and parents[j].get('kind') == 'ParenExpr':
                    child = parents[j]
                    j -= 1
                up = parents[j] if j >= 0 else {}
                lhs = up.get('kind') == 'BinaryOperator' and up.get('opcode') == '=' and \
                    up['inner'][0] is child
                if lhs:
                    rhs = up['inner'][1]
                    if rawscan.const_of(rhs) == 0:
                        text = field
                        ea, ez, _ = src_text(s, rhs)
                        if ea is not None:
                            edits.append((ea, ez, '0'))
                    else:
                        text = '*(%s *)&%s /* BR_LP64_INT_STORED_IN_PTR */' % (want, field)
                        report['lp64'] += 1
                elif feeds_address(parents[:-1]):
                    text = '((char *)%s)' % field
                elif zero_test(parents[:-1]):
                    text = field
                else:
                    text = 'BR_LP64_PTR_AS_INT(%s)' % field
                    report['lp64'] += 1
            elif wptr and norm(fty) in ('i32', 'u32'):
                text = '*(%s *)&%s /* BR_LP64_INT_FIELD_HOLDS_PTR */' % (want, field)
                report['int_holds_ptr'][(rec, pth)].append((f, fn))
                report['lp64'] += 1
            else:
                text = '*(%s *)&%s' % (want, field)
            edits.append((ua, uz, text))
        else:
            text = ('&%s' % field) if exact or norm(want) == 'void' else '(%s *)&%s' % (want, field)
            edits.append((a0, z0, text))

    def handle_plain(n, fn, parents):
        """`p[K]` or `*(p + K)` with p a byte pointer to a known record, or a
        record pointer with a byte-sized constant (K >= 16)."""
        if n.get('kind') == 'ArraySubscriptExpr':
            b0, i0 = n['inner'][0], n['inner'][1]
            k = rawscan.const_of(i0)
            base = rawscan.strip_parens(b0)
            while base.get('kind') == 'ImplicitCastExpr':
                base = rawscan.strip_parens(base['inner'][0])
        else:
            op = rawscan.strip_parens(n['inner'][0])
            while op.get('kind') == 'ImplicitCastExpr' and op.get('castKind') in ('NoOp', 'LValueToRValue'):
                op = rawscan.strip_parens(op['inner'][0])
            if op.get('kind') == 'BinaryOperator' and op.get('opcode') in ('+', '-'):
                base, k = rawscan.base_of(op)
                base = rawscan.strip_parens(base)
                consumed.add(op.get('id'))
            elif op.get('kind') == 'DeclRefExpr':
                base, k = op, 0
            else:
                return
        if k is None:
            return
        bt = rawscan.qt(base)
        bk, bn, bid = rawscan.describe(base)
        rec = rec_of_type(bt)
        if rec and k < 16:
            return
        if rec is None:
            dec = decisions.get((f, fn or '', bn)) or decisions.get((f, '', bn))
            nb = norm(bt)
            scl = {'i8*': 1, 'u8*': 1, 'i32*': 4, 'u32*': 4, 'f32*': 4, 'i16*': 2, 'u16*': 2}.get(nb)
            if not dec or not scl:
                return
            if k == 0 and scl == 1 and n.get('kind') != 'ArraySubscriptExpr':
                pass
            rec = dec[0]
            k = k * scl + dec[1]
            if bk in ('ParmVarDecl', 'VarDecl'):
                retype[bid] = rec
                rscale[bid] = scl
                rdelta[bid] = dec[1]
            if bk in ('ParmVarDecl', 'VarDecl'):
                retype[bid] = rec
        want = rawscan.qt(n)
        if rec_of_type(bt):
            want = 'uint8_t'      # the code indexed BYTES before the retype
        pth, fty = resolve(rec, k, want)
        na, nz, _ = src_text(s, n)
        if pth is None or na is None:
            report['unresolved'][(rec, k, want)].append((f, fn))
            return
        btxt = bn if bn else None
        if btxt is None:
            ba, bz, btxt = src_text(s, base)
            if btxt is None:
                return
            if not re.match(r'^[\w.>\-\[\]]+$', btxt):
                btxt = '(%s)' % btxt
        field = '%s->%s' % (btxt, pth)
        edits.append((na, nz, field if same_type(fty, want) else '*(%s *)&%s' % (want, field)))

    def zero_test(chain):
        """Is the loaded value only compared with 0 / used as a truth value?"""
        i = len(chain) - 1
        while i >= 0 and (chain[i].get('kind') == 'ParenExpr' or (
                chain[i].get('kind') == 'ImplicitCastExpr' and chain[i].get('castKind') in (
                    'LValueToRValue', 'IntegralCast', 'NoOp'))):
            i -= 1
        if i < 0:
            return False
        n = chain[i]
        if n.get('kind') == 'BinaryOperator' and n.get('opcode') in ('==', '!='):
            return any(rawscan.const_of(c) == 0 for c in n['inner'])
        if n.get('kind') == 'UnaryOperator' and n.get('opcode') == '!':
            return True
        if n.get('kind') == 'BinaryOperator' and n.get('opcode') in ('&&', '||'):
            return True
        return n.get('kind') in ('IfStmt', 'WhileStmt', 'DoStmt', 'ConditionalOperator') or (
            n.get('kind') == 'ImplicitCastExpr' and n.get('castKind') == 'IntegralToBoolean')

    def feeds_address(chain):
        """Does the value at the end of chain (a load) flow into address
        arithmetic / an int-to-pointer conversion?"""
        i = len(chain) - 1
        while i >= 0:
            n = chain[i]
            k = n.get('kind')
            if k in ('ParenExpr',) or (k == 'ImplicitCastExpr' and n.get('castKind') in (
                    'LValueToRValue', 'IntegralCast', 'NoOp')):
                i -= 1
                continue
            if k == 'BinaryOperator' and n.get('opcode') in ('+', '-'):
                i -= 1
                continue
            return k in ('CStyleCastExpr', 'ImplicitCastExpr') and n.get('castKind') == 'IntegralToPointer'
        return False

    for top in a.get('inner', []):
        if infile(top):
            walk(top, None, [])

    # arithmetic on a record pointer: `p + 0x2B68` meant one record and
    # `p + 0x370` a field's address, both in BYTES in the original
    for n, fn in arith:
        if n.get('id') in consumed:
            continue
        inner = n.get('inner', [])
        ref = rawscan.strip_parens(inner[0]) if inner else {}
        while ref.get('kind') == 'ImplicitCastExpr':
            ref = rawscan.strip_parens(ref['inner'][0])
        if ref.get('kind') != 'DeclRefExpr':
            continue
        did = (ref.get('referencedDecl') or {}).get('id')
        rec = retype.get(did) or rec_of_type(rawscan.qt(ref))
        if not rec:
            continue
        vname = ref.get('referencedDecl', {}).get('name')
        size = int(spec(rec)[0]['size'], 16)
        if n.get('kind') == 'UnaryOperator':
            if did in retype:
                report['stride'].append((f, fn, vname, '++/--'))
            continue
        k = rawscan.const_of(inner[1])
        if k is not None:
            k *= rscale.get(did, 1)
        ka, kz, ktxt = src_text(s, rawscan.strip_parens(inner[1]))
        if k is None:
            t = rawscan.strip_parens(inner[1])
            while t.get('kind') == 'ImplicitCastExpr':
                t = rawscan.strip_parens(t['inner'][0])
            if t.get('kind') == 'BinaryOperator' and t.get('opcode') == '*':
                c, i2 = rawscan.const_of(t['inner'][1]), t['inner'][0]
                if c is None:
                    c, i2 = rawscan.const_of(t['inner'][0]), t['inner'][1]
                ia, iz, itxt = src_text(s, i2)
                if c == size and itxt is not None and ka is not None:
                    edits.append((ka, kz, itxt))
                    continue
            if did in retype:
                report['stride'].append((f, fn, vname, ktxt))
            continue
        if did not in retype and k < 16:
            continue        # typed code stepping by elements
        if k % size == 0:
            if ka is not None:
                edits.append((ka, kz, str(k // size)))
            continue
        pth, fty = resolve(rec, k, 'void')
        na, nz, _ = src_text(s, n)
        if pth and na is not None and n.get('kind') == 'BinaryOperator' and n.get('opcode') == '+':
            edits.append((na, nz, '((void *)&%s->%s)' % (vname, pth)))
        else:
            report['stride'].append((f, fn, vname, ktxt))

    # a retyped variable that pointed INTO its record (delta != 0): every value
    # assigned to it becomes the record's address
    for n, fn in assigns:
        lhs = rawscan.strip_parens(n['inner'][0])
        did = (lhs.get('referencedDecl') or {}).get('id') if lhs.get('kind') == 'DeclRefExpr' else None
        if did is None or not rdelta.get(did):
            continue
        rec = retype[did]
        pth, _ = resolve(rec, rdelta[did], 'void')
        ra, rz, rtxt = src_text(s, n['inner'][1])
        if ra is None:
            la, lz, _ = src_text(s, lhs)
            if la is not None:
                eq = s.find('=', lz)
                sc = s.find(';', eq)
                if eq != -1 and sc != -1 and '\n' not in s[lz:eq]:
                    ra, rz = eq + 1, sc
                    while ra < rz and s[ra] == ' ':
                        ra += 1
                    rtxt = s[ra:rz]
        if pth and ra is not None:
            edits.append((ra, rz, '((%s *)((char *)(%s) - offsetof(%s, %s)))' % (rec, rtxt, rec, pth)))
        else:
            report['stride'].append((f, fn, lhs.get('referencedDecl', {}).get('name'), 'assignment into +delta'))

    # retype decision variables: replace the declared type before the name.
    # A declaration shared with other variables (`uint8_t *a, *b;`) is split
    # so only the decided one changes type.
    by_begin = collections.defaultdict(list)
    for d in decls.values():
        b = d.get('range', {}).get('begin', {})
        if d.get('kind') == 'VarDecl' and 'offset' in b:
            by_begin[b['offset']].append(d)
    done_decl = set()
    for did, rec in list(retype.items()):
        d = decls.get(did)
        if not d or did in done_decl:
            continue
        r = d.get('range', {})
        b = r.get('begin', {})
        loc = d.get('loc', {})
        if 'offset' not in b or 'offset' not in loc or 'spellingLoc' in b:
            report['macro'].append((f, d.get('name'), rec, 'decl'))
            continue
        sibs = by_begin.get(b['offset'], [])
        if d.get('kind') == 'VarDecl' and len(sibs) > 1:
            sibs = sorted(sibs, key=lambda x: x['loc']['offset'])
            semi = s.find(';', sibs[-1]['range']['end']['offset'])
            first_name = sibs[0]['loc']['offset']
            tstart = b['offset']
            # the type text ends where the first declarator's stars begin
            tend = first_name
            while tend > tstart and s[tend - 1] in ' *\t':
                tend -= 1
            base_ty = s[tstart:tend]
            body = s[tend:semi]
            parts, depth, cur = [], 0, ''
            for ch in body:
                if ch in '([{':
                    depth += 1
                elif ch in ')]}':
                    depth -= 1
                if ch == ',' and depth == 0:
                    parts.append(cur)
                    cur = ''
                else:
                    cur += ch
            parts.append(cur)
            out = []
            for part, sd in zip(parts, sibs):
                part = part.strip()
                if sd['id'] in retype:
                    out.append('%s *%s;' % (retype[sd['id']], re.sub(r'^\*+\s*', '', part)))
                else:
                    out.append('%s %s;' % (base_ty, part))
            if len(parts) == len(sibs):
                edits.append((tstart, semi + 1, ' '.join(out)))
                done_decl.update(sd['id'] for sd in sibs)
            else:
                report['macro'].append((f, d.get('name'), rec, 'shared decl'))
            continue
        edits.append((b['offset'], loc['offset'], '%s *' % rec))

    # a retyped variable needs its record's declaration
    for rec in set(retype.values()):
        hdr = os.path.basename(spec(rec)[0].get('header', ''))
        if hdr.endswith('.h') and not re.search(r'#\s*include\s*"%s"' % re.escape(hdr), s):
            m = re.search(r'^#\s*include[^\n]*\n', s, re.M)
            at = m.end() if m else 0
            edits.append((at, at, '#include "%s"\n' % hdr))

    # innermost first, drop overlaps (a rerun takes the next layer)
    edits.sort(key=lambda e: (e[1] - e[0]))
    taken = []
    for e in edits:
        if any(not (e[1] <= t[0] or e[0] >= t[1]) for t in taken):
            report['deferred'] += 1
            continue
        taken.append(e)
    if taken and not dry:
        for a0, z0, text in sorted(taken, reverse=True):
            s = s[:a0] + text + s[z0:]
        open(path, 'w', encoding='latin-1').write(s)
    return len(taken)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--absorb', action='store_true')
    ap.add_argument('--dry', action='store_true')
    ap.add_argument('files', nargs='+')
    a = ap.parse_args()
    os.chdir(ROOT)
    decisions = load_decisions()
    report = {'stride': [], 'lp64': 0, 'macro': [], 'unresolved': collections.defaultdict(list), 'deferred': 0, 'noast': [],
              'int_holds_ptr': collections.defaultdict(list), 'ptr_read_as_int': collections.defaultdict(list)}
    total = 0
    for f in a.files:
        total += process(f, decisions, a.absorb, a.dry, report)
    print('rewritten %d accesses; deferred (nested) %d; in macros %d; unresolved %d offsets; LP64 markers %d' % (
        total, report['deferred'], len(report['macro']), len(report['unresolved']), report['lp64']))
    if report['unresolved']:
        by = collections.defaultdict(list)
        for (rec, k, want), where in report['unresolved'].items():
            by[rec].append((k, want, len(where)))
        for rec, lst in by.items():
            print('  %s: %s' % (rec, ', '.join('0x%X:%s(%d)' % x for x in sorted(lst)[:40])))
    for x in report['stride']:
        print('  BR_LP64 stride on a retyped variable: %s %s %s %s' % x)
    for k in ('int_holds_ptr', 'ptr_read_as_int'):
        if report[k]:
            print('  %s: %s' % (k, ', '.join('%s.%s' % x for x in report[k])))
    if a.absorb and report['unresolved']:
        added = collections.defaultdict(int)
        SCALAR = ('i32', 'u32', 'i16', 'u16', 'i8', 'u8', 'f32', 'f64')
        manual = collections.defaultdict(list)
        for (rec, k, want), where in sorted(report['unresolved'].items()):
            meta, fields = spec(rec)
            if meta.get('noemit'):
                continue
            nw = norm(want)
            if nw not in SCALAR and nw not in ('void*', 'i8*', 'u8*'):
                manual[rec].append((k, want, where))
                continue
            hdrs = meta.get('headers', '').split()
            if any(f.off <= k < f.off + recspec.size32(f.ty, hdrs) for f in fields):
                continue    # inside a declared field (a misread width): leave for a person
            fields.append(recspec.Field(k, want, 'f%04X' % k))
            added[rec] += 1
        for rec in added:
            meta, fields = spec(rec)
            recspec.save(rec, meta, fields)
            recspec.cmd_emit(rec)
        print('absorbed: %s' % dict(added))
        for rec, lst in manual.items():
            print('  %s needs a person (struct-typed accesses):' % rec)
            for k, want, where in lst:
                print('    0x%04X %-24s %s' % (k, want, ', '.join(sorted({'%s:%s' % (os.path.basename(a), b) for a, b in where}))[:150]))


if __name__ == '__main__':
    main()
