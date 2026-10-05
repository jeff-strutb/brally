#!/usr/bin/env python3
"""ptruses.py -- rewrite the uses of struct members ptrfields.py turned into
TgrAddr, so the core compiles and means what it meant.

    ptruses.py BEFORE_TREE

BEFORE_TREE is a copy of ports/tgrally taken before ptrfields.py ran (its
members still pointers).  Each TU there is read through clang's AST; every
expression naming a retyped member is rewritten in the current tree by what
its context does with it:

    s->p->x, s->p[i], *s->p      TGR_PTR(T *, s->p)->x ...     (a dereference)
    s->p = q                     s->p = tgr_addr32(q)          (TGR_FA for code)
    s->p += n, s->p++            s->p += (n) * sizeof(T)       (element steps)
    f(s->p), (T *)s->p, s->p + n TGR_PTR(T *, s->p) ...        (used as a pointer)
    s->fn(a)                     TGR_FN(R (*)(A), s->fn)(a)
    s->p == 0, if (s->p), (int)s->p    unchanged               (an address is a value)

Lines and columns are the same in both trees (ptrfields.py rewrote only the
member declarations).  It prints what it could not decide.
"""
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
CUR = os.path.join(ROOT, 'ports/tgrally')


def flags(base):
    return ['clang', '-fsyntax-only', '-std=gnu89', '-w', '-ferror-limit=0',
            '-I' + os.path.join(base, 'include'), '-I' + os.path.join(base, 'platform/include'),
            '-include', os.path.join(base, 'platform/include/ultra64.h'),
            '-include', os.path.join(base, 'platform/include/tgr_core.h'),
            '-Xclang', '-ast-dump=json']


def loc_of(r, main, text=None, expand=True):
    """(begin offset, end offset) of a range in the main file, or None.
    Spelling locations first (a macro argument is spelled where it is
    written); when those do not make a range outside a macro body, the
    expansion (the whole macro call)."""
    def pick(kind):
        b, e = r.get('begin', {}), r.get('end', {})
        b = b.get(kind, b) if kind else b
        e = e.get(kind, e) if kind else e
        if 'offset' not in b or 'offset' not in e:
            return None
        for x in (b, e):
            if 'file' in x and os.path.abspath(x['file']) != main:
                return None
            if 'includedFrom' in x:
                return None
        bo, eo = b['offset'], e['offset'] + e.get('tokLen', 0)
        if kind == 'expansionLoc' and text is not None:
            # every token of a macro call expands to the macro's name: take
            # the call's argument list too
            j = eo
            while j < len(text) and text[j] in ' \t':
                j += 1
            if j < len(text) and text[j] == '(':
                depth = 0
                while j < len(text):
                    if text[j] == '(':
                        depth += 1
                    elif text[j] == ')':
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                eo = j + 1
        if eo <= bo:
            return None
        if text is not None and (in_define(text, bo) or in_define(text, max(bo, eo - 1))):
            return None
        return bo, eo
    got = pick('spellingLoc') or (pick('expansionLoc') if expand else None)
    if got or expand:
        return got
    # a member access whose base is a macro call (PAD(i)->ghost): begin at
    # the call, end at the member
    b, e = r.get('begin', {}), r.get('end', {})
    bx, es = b.get('expansionLoc'), e.get('spellingLoc', e)
    if not bx or 'offset' not in bx or 'offset' not in es:
        return None
    for x in (bx, es):
        if 'file' in x and os.path.abspath(x['file']) != main or 'includedFrom' in x:
            return None
    bo, eo = bx['offset'], es['offset'] + es.get('tokLen', 0)
    if eo <= bo or text is None or in_define(text, bo) or in_define(text, eo - 1):
        return None
    return bo, eo


def in_define(text, off):
    """an offset inside a #define (with its continuation lines)"""
    ls = text.rfind('\n', 0, off) + 1
    while True:
        le = text.find('\n', ls)
        line = text[ls:le if le >= 0 else len(text)]
        if line.lstrip().startswith('#'):
            return True
        if ls == 0:
            return False
        prev = text.rfind('\n', 0, ls - 1) + 1
        if not text[prev:ls - 1].rstrip().endswith('\\'):
            return False
        ls = prev


class Walk:
    def __init__(self, main, text):
        self.main, self.text = main, text
        self.base = BASE
        self.fields = {}            # FieldDecl id -> (pointer type, pointee type, is function, is array)
        self.edits = []             # (begin, end, kind, data)
        self.notes = []
        self.file = None

    def pointer_field(self, d):
        q = d['type'].get('desugaredQualType', d['type']['qualType'])
        qq = d['type']['qualType']
        arr = re.search(r'\[\d*\]\s*$', q) is not None and '(*' not in q
        if arr:
            q = re.sub(r'\s*\[\d*\]\s*$', '', q)
            qq = re.sub(r'\s*\[\d*\]\s*$', '', qq)
        if not (q.endswith('*') or '(*' in q):
            return None
        fn = '(*)' in q and q.endswith(')') and not q.startswith('(')
        if fn:
            pointee = None
        elif q.endswith('*'):
            pointee = qq[:-1].strip() if qq.endswith('*') else q[:-1].strip()
        else:
            pointee = None
        return (qq, pointee, fn, arr)

    def track(self, l):
        """clang's JSON names a file only when it changes: follow it in
        print order (spelling before expansion)."""
        for x in (l.get('spellingLoc'), l.get('expansionLoc'), l):
            if x and 'file' in x:
                self.file = x['file']

    def collect(self, node, infile):
        k = node.get('kind')
        loc = node.get('loc', {})
        self.track(loc)
        decl_file = self.file
        rng = node.get('range', {})
        self.track(rng.get('begin', {}))
        self.track(rng.get('end', {}))
        if k == 'FieldDecl' and decl_file and os.path.abspath(decl_file).startswith(self.base) \
                and 'zlib' not in decl_file and 'zinfl' not in decl_file:
            pf = self.pointer_field(node)
            if pf:
                self.fields[node['id']] = pf
        for c in node.get('inner', []):
            self.collect(c, infile)

    @staticmethod
    def strip(n):
        while n.get('kind') in ('ImplicitCastExpr', 'ParenExpr') and n.get('inner'):
            n = n['inner'][0]
        return n

    def is_null(self, n):
        n = self.strip(n)
        if n.get('kind') == 'CStyleCastExpr' and n.get('inner'):
            n = self.strip(n['inner'][0])
        return n.get('kind') == 'IntegerLiteral' and n.get('value') == '0'

    def field_use(self, n):
        """The retyped member an expression names, if it is one (looking
        through implicit casts and parentheses)."""
        n = self.strip(n)
        if n.get('kind') == 'MemberExpr' and n.get('referencedMemberDecl') in self.fields:
            f = self.fields[n['referencedMemberDecl']]
            if not f[3]:
                return n, f
        if n.get('kind') == 'ArraySubscriptExpr' and n.get('inner'):
            b = self.strip(n['inner'][0])
            if b.get('kind') == 'MemberExpr' and b.get('referencedMemberDecl') in self.fields:
                f = self.fields[b['referencedMemberDecl']]
                if f[3]:
                    return n, f
        return None, None

    def in_define(self, off):
        """an offset inside a #define in the main file: a macro body, which
        each expansion would rewrite again"""
        ls = self.text.rfind('\n', 0, off) + 1
        while True:
            line = self.text[ls:self.text.find('\n', ls)]
            if line.lstrip().startswith('#define'):
                return True
            prev = self.text.rfind('\n', 0, ls - 1) + 1 if ls > 0 else 0
            if ls == 0 or not self.text[prev:ls - 1].rstrip().endswith('\\'):
                return False
            ls = prev

    def wrap(self, n, f, how='ptr'):
        r = loc_of(n.get('range', {}), self.main, self.text, expand=False)
        if r and self.in_define(r[0]):
            self.notes.append('in a macro body at offset %d' % r[0])
            return
        if not r:
            b = n.get('range', {}).get('begin', {})
            b = b.get('expansionLoc', b)
            self.notes.append('in a macro at line %s' % b.get('line', '?'))
            return
        self.edits.append((r[0], r[1], how, f))

    def visit(self, node, parents):
        k = node.get('kind')
        inner = node.get('inner', [])
        for c in inner:
            self.visit(c, parents + [node])
        u, f = self.field_use(node)
        if u is not node:
            return
        # climb to the context, through implicit casts and parentheses
        chain = [node]
        i = len(parents) - 1
        while i >= 0 and parents[i].get('kind') in ('ImplicitCastExpr', 'ParenExpr'):
            chain.append(parents[i])
            i -= 1
        if i < 0:
            return
        ctx = parents[i]
        top = chain[-1]
        ck = ctx.get('kind')
        ptype, pointee, fn, arr = f
        first = inner and False
        kids = ctx.get('inner', [])
        idx = next((j for j, c in enumerate(kids) if c is top), -1)
        if ck == 'MemberExpr' and ctx.get('isArrow'):
            self.wrap(node, f)
        elif ck == 'ArraySubscriptExpr' and idx == 0:
            self.wrap(node, f)
        elif ck == 'UnaryOperator':
            op = ctx.get('opcode')
            if op == '*':
                self.wrap(node, f)
            elif op in ('++', '--'):
                r = loc_of(ctx.get('range', {}), self.main, self.text)
                ru = loc_of(node.get('range', {}), self.main, self.text)
                if r and ru and not self.in_define(r[0]):
                    self.edits.append((r[0], r[1], 'step', (f, op, ru)))
            elif op in ('!', '&'):
                pass
            else:
                self.notes.append('unary %s on a member' % op)
        elif ck == 'BinaryOperator':
            op = ctx.get('opcode')
            other = kids[1 - idx] if len(kids) == 2 else None
            if op == '=':
                if idx == 0:
                    if other is not None and not self.is_null(other):
                        ou, of = self.field_use(other)
                        if ou is None:
                            self.wrap_rhs(other, f)
                else:
                    lu, lf = self.field_use(kids[0])
                    if lu is None:
                        self.wrap(node, f)
            elif op in ('==', '!=', '<', '>', '<=', '>='):
                ou, of = self.field_use(other) if other is not None else (None, None)
                if other is not None and not self.is_null(other) and ou is None:
                    oq = self.strip(other).get('type', {}).get('qualType', '')
                    if '*' in oq or '[' in oq:
                        self.wrap(node, f)
            elif op in ('+', '-'):
                ou, of = self.field_use(other) if other is not None else (None, None)
                if op == '-' and ou is not None:
                    self.notes.append('a difference of two members')
                self.wrap(node, f)
            elif op in ('&&', '||', ','):
                pass
            else:
                self.notes.append('binary %s on a member' % op)
        elif ck == 'CompoundAssignOperator':
            op = ctx.get('opcode')
            if idx == 0 and op in ('+=', '-='):
                r = loc_of(kids[1].get('range', {}), self.main, self.text)
                if r and not self.in_define(r[0]):
                    self.edits.append((r[0], r[1], 'scale', f))
            elif idx == 1:
                self.wrap(node, f)
        elif ck == 'CallExpr':
            if idx == 0:
                self.wrap(node, f, 'fn')
            else:
                self.wrap(node, f)
        elif ck == 'CStyleCastExpr':
            q = ctx.get('type', {}).get('qualType', '')
            if '*' in q:
                self.wrap(node, f)
        elif ck in ('ReturnStmt', 'VarDecl', 'ConditionalOperator', 'InitListExpr'):
            q = None
            if ck == 'VarDecl':
                q = ctx.get('type', {}).get('qualType', '')
                if '*' in q:
                    self.wrap(node, f)
            elif ck == 'ConditionalOperator':
                if idx != 0:
                    self.wrap(node, f)
            elif ck == 'ReturnStmt':
                self.wrap(node, f)
        elif ck in ('IfStmt', 'WhileStmt', 'ForStmt', 'DoStmt', 'SwitchStmt'):
            pass
        else:
            self.notes.append('context %s' % ck)

    def wrap_rhs(self, other, f):
        r = loc_of(other.get('range', {}), self.main, self.text)
        if r and self.in_define(r[0]):
            self.notes.append('assignment in a macro body')
            return
        if not r:
            self.notes.append('assignment in a macro')
            return
        self.edits.append((r[0], r[1], 'addr', f))


def text_for(kind, f, s):
    ptype, pointee, fn, arr = f
    if kind == 'ptr':
        return 'TGR_PTR(%s, %s)' % (ptype, s)
    if kind == 'fn':
        return 'TGR_FN(%s, %s)' % (ptype, s)
    if kind == 'addr':
        if fn:
            return 'TGR_FA(%s)' % s
        return 'tgr_addr32(%s)' % s
    if kind == 'scale':
        return '(%s) * sizeof(%s)' % (s, pointee or 'char')
    raise ValueError(kind)


BASE = ''


def process(args):
    global BASE
    before, tu = args
    BASE = os.path.abspath(before)
    main = os.path.join(before, tu)
    p = subprocess.run(flags(before) + [main], capture_output=True, text=True)
    try:
        ast = json.loads(p.stdout)
    except ValueError:
        return tu, [], ['no AST']
    text = open(main, 'rb').read().decode('latin-1')
    w = Walk(os.path.abspath(main), text)
    w.collect(ast, False)
    for d in ast.get('inner', []):
        if d.get('kind') == 'FunctionDecl':
            w.visit(d, [])
    return tu, w.edits, w.notes


def line_col(text, off):
    line = text.count('\n', 0, off) + 1
    col = off - (text.rfind('\n', 0, off) + 1)
    return line, col


def main():
    before = os.path.abspath(sys.argv[1])
    tus = []
    for dp, dn, fn in os.walk(os.path.join(before, 'src')):
        tus += [os.path.relpath(os.path.join(dp, f), before) for f in fn
                if f.endswith('.c') and 'zlib' not in dp]
    total = 0
    with ProcessPoolExecutor(14) as ex:
        results = list(ex.map(process, [(before, t) for t in sorted(tus)]))
    for tu, edits, notes in results:
        if not edits and not notes:
            continue
        old = open(os.path.join(before, tu), 'rb').read().decode('latin-1')
        cur = open(os.path.join(CUR, tu), 'rb').read().decode('latin-1')
        oldl, curl = old.split('\n'), cur.split('\n')
        if len(oldl) != len(curl):
            print('%s: line counts differ, skipped' % tu)
            continue
        # map an offset in the old text to the current one: same line, same column
        starts = [0]
        for l in curl[:-1]:
            starts.append(starts[-1] + len(l) + 1)

        def cur_off(o):
            ln, col = line_col(old, o)
            return starts[ln - 1] + col
        # innermost first: sort by start descending, then by length ascending
        seen = set()
        spans = []
        for b, e, kind, f in edits:
            key = (b, e, kind)
            if key in seen:
                continue
            seen.add(key)
            spans.append((cur_off(b), cur_off(e), kind, f))
        # apply nested edits from the innermost out: process by end desc,
        # and within, by start desc (inner spans start later or end earlier)
        spans.sort(key=lambda x: (x[0], -(x[1] - x[0])), reverse=True)
        txt = cur
        applied = []
        for b, e, kind, f in spans:
            # shift for edits already applied inside this span
            delta_b = sum(d for (ab, ae, d) in applied if ae <= b)
            if kind == 'step':
                ff, op, ru = f
                rb, re_ = cur_off(ru[0]), cur_off(ru[1])
                inner_d = sum(d for (ab, ae, d) in applied if ab >= rb and ae <= re_)
                lv = txt[rb:re_ + inner_d]
                new = '(%s %s= sizeof(%s))' % (lv, '+' if op == '++' else '-', ff[1] or 'char')
                inner_all = sum(d for (ab, ae, d) in applied if ab >= b and ae <= e)
                seg_end = e + inner_all
                txt = txt[:b] + new + txt[seg_end:]
                applied.append((b, e, len(new) - (seg_end - b)))
                total += 1
                continue
            inner_all = sum(d for (ab, ae, d) in applied if ab >= b and ae <= e)
            seg = txt[b:e + inner_all]
            new = text_for(kind, f, seg)
            txt = txt[:b] + new + txt[e + inner_all:]
            applied.append((b, e, len(new) - len(seg)))
            total += 1
        txt = re.sub(r'tgr_addr32\(\(\w[\w ]*\*\s*\)\s*(0x[0-9A-Fa-f]{8})\)', r'\1', txt)
        txt = re.sub(r'tgr_addr32\(\(\w[\w ]*\*\s*\)\s*(\(0x[0-9A-Fa-f]{8}[^()]*\))\)', r'\1', txt)
        open(os.path.join(CUR, tu), 'wb').write(txt.encode('latin-1'))
        for n in sorted(set(notes)):
            print('%s: %s' % (tu, n))
    print('ptruses: %d rewrites' % total)


if __name__ == '__main__':
    main()
