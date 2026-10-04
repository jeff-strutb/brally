#!/usr/bin/env python3
"""Rewrite a file's accesses through a local "view" struct onto the
canonical record it stands for.

The matching source often declares its own partial struct for a shared
record -- `char pad[0x2AE8]; Phase *pSub;` -- whose field offsets are the
original 32-bit ones.  On a 64-bit build those offsets are wrong wherever
the canonical record holds pointers before the field.  For every member
access through the view this tool substitutes the canonical field at the
same ORIGINAL offset, keeping the view's type so nothing downstream
changes:

    p->pSub      ->  (*(Phase * *)&((BrUiCtl_ *)(p))->pOwner)
    v.f68        ->  (*(int *)&((BrPhase_ *)&(v))->f68)

Offsets come from clang's i386 record layouts of the file itself, so the
view and the canonical are measured by the compiler, never by hand.  The
member accesses come from clang's AST, so only real accesses are touched.

It reports, and does not rewrite:
  KIND   the view's field and the canonical's disagree about holding a
         pointer (an int view of a pointer, or the reverse)
  NOFIT  no canonical field starts at or contains the offset
  MACRO  the access is spelled inside a macro expansion
  VALUE  an object, array element or sizeof of the view type: its size
         is the view's, not the canonical's

Usage: viewmerge.py FILE VIEW CANON [BASE] [--include HDR] [--dry]
  BASE: the view's offset inside the canonical (default 0)
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
FLAGS = ['-D_FORTIFY_SOURCE=0', '-w', '-fms-extensions', '-fdeclspec',
         '-Iports/brally/platform/include', '-Iports/brally/include']
for h in ('ports/brally/platform/include/win32.h', 'ports/brally/platform/include/glide.h',
          'ports/brally/platform/include/br_x87.h', 'ports/brally/include/br_crt.h',
          'ports/brally/include/br_addr32.h', 'ports/brally/platform/include/br_lp64.h',
          'ports/brally/include/br_globals.h', 'ports/brally/include/br_funcs.h'):
    FLAGS += ['-include', h]


def lang(f):
    if f.endswith('.cpp'):
        return ['-x', 'c++', '-std=c++98', '-fno-exceptions', '-fno-rtti']
    return ['-x', 'c', '-std=gnu89']


def run(args):
    return subprocess.run(['clang'] + args, capture_output=True, cwd=ROOT, text=True, errors='replace')


# ---------------------------------------------------------------- layouts
def layouts(f, extra):
    """{record name: [(offset, depth, type, name)]} for the i386 build."""
    p = run(['-target', 'i386-apple-macos10.13'] + FLAGS + extra + lang(f) +
            ['-fsyntax-only', '-Xclang', '-fdump-record-layouts-complete', f])
    recs, cur, sizes = {}, None, {}
    for line in p.stdout.split('\n'):
        m = re.match(r'^\s*(\d+)(?::\d+-\d+)? \| (\s*)(.*)$', line)
        if m:
            off, depth, rest = int(m.group(1)), len(m.group(2)), m.group(3)
            if depth == 0 and cur is None:
                rm = re.match(r'^(?:struct|class|union) (\S+)$', rest)
                cur = rm.group(1) if rm else rest
                recs.setdefault(cur, [])
                continue
            if cur is not None and depth > 0:
                tm = re.match(r'^(.*?)\s*(\w+)$', rest)
                if tm:
                    recs[cur].append((off, depth // 2, tm.group(1).strip(), tm.group(2)))
                else:
                    recs[cur].append((off, depth // 2, rest, ''))
            continue
        sm = re.match(r'^\s*\| \[sizeof=(\d+)', line)
        if sm and cur is not None:
            sizes[cur] = int(sm.group(1))
            cur = None
    return recs, sizes


def children(entries, i):
    """Direct children (index list) of entry i (or of the record if i is None)."""
    depth = 0 if i is None else entries[i][1]
    start = 0 if i is None else i + 1
    out = []
    for j in range(start, len(entries)):
        d = entries[j][1]
        if d <= depth:
            break
        if d == depth + 1:
            out.append(j)
    return out


def array_dims(t):
    m = re.match(r'^(.*?)((?:\[\d+\])+)$', t)
    if not m:
        return None, []
    return m.group(1).strip(), [int(x) for x in re.findall(r'\[(\d+)\]', m.group(2))]


def tsize(t, sizes):
    t = t.strip()
    if t.endswith('*'):
        return 4
    base, dims = array_dims(t)
    if dims:
        n = 1
        for d in dims:
            n *= d
        e = tsize(base, sizes)
        return None if e is None else e * n
    t2 = re.sub(r'^(const |volatile |struct |union |class |enum )+', '', t)
    if t2 in sizes:
        return sizes[t2]
    return {'char': 1, 'unsigned char': 1, 'signed char': 1, 'uint8_t': 1, 'int8_t': 1, '_Bool': 1, 'bool': 1,
            'short': 2, 'unsigned short': 2, 'int16_t': 2, 'uint16_t': 2,
            'int': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4, 'int32_t': 4, 'uint32_t': 4,
            'float': 4, 'double': 8, 'long long': 8, 'unsigned long long': 8, 'int64_t': 8,
            'uint64_t': 8}.get(t2)


PTR_TYPEDEFS = set()


def load_ptr_typedefs(f, extra):
    """Typedef names that are pointers (data or function), from the
    preprocessed file, so a layout's `BrHookFn_ pfn` counts as a pointer."""
    p = run(FLAGS + extra + lang(f) + ['-E', '-P', f])
    txt = re.sub(r'\s+', ' ', p.stdout)
    for m in re.finditer(r'typedef ([^;{}]*?);', txt):
        body = m.group(1)
        fm = re.search(r'\(\s*(?:__\w+\s+|__attribute__\(\(\w+\)\)\s*)*\*\s*(\w+)\s*\)\s*\(', body)
        if fm:
            PTR_TYPEDEFS.add(fm.group(1))
            continue
        pm = re.match(r'^(.*?)\s*(\w+)$', body.strip())
        if pm:
            t, n = pm.group(1).strip(), pm.group(2)
            if t.endswith('*') or t.split()[-1:] and t.split()[-1] in PTR_TYPEDEFS:
                PTR_TYPEDEFS.add(n)


def is_ptr(t):
    t = re.sub(r'^(const |volatile )+', '', t.strip())
    b, dims = array_dims(t)
    if dims:
        t = b
    return t.endswith('*') or '(*)' in t or t in PTR_TYPEDEFS


def resolve(recs, sizes, canon, off, want=None):
    """Path expression and leaf type of the canonical field at byte offset
    `off` (relative to the record start).  Returns (path, leaf_type, rem)
    where rem is a byte offset left inside a scalar, or None."""
    entries = recs.get(canon)
    if entries is None:
        return None
    path = ''
    idx = None
    while True:
        kids = children(entries, idx)
        chosen = None
        for j in kids:
            o, d, t, n = entries[j]
            sz = tsize(t, sizes)
            if sz is None:
                nxt = [entries[k][0] for k in kids if entries[k][0] > o]
                sz = (min(nxt) if nxt else 1 << 30) - o
            if o <= off < o + max(sz, 1):
                chosen = j
                break
        if chosen is None:
            return None
        o, d, t, n = entries[chosen]
        path += ('.' if path else '') + n
        base, dims = array_dims(t)
        rel = off - o
        if want is not None and rel == 0 and tsize(t, sizes) == want:
            return path, t, 0         # the whole field the view names
        if want == 'record' and rel == 0 and not dims and children(entries, chosen):
            return path, t, 0         # an embedded object the view names
        if dims:
            esz = tsize(base, sizes)
            if esz is None or esz == 0:
                return path, t, rel
            for k in range(len(dims)):
                inner = 1
                for d2 in dims[k + 1:]:
                    inner *= d2
                step = esz * inner
                i = rel // step
                path += '[%d]' % i
                rel -= i * step
            bname = re.sub(r'^(const |struct |union |class )+', '', base)
            if rel == 0 and bname not in recs:
                return path, base, 0
            if want is not None and rel == 0 and esz == want:
                return path, base, 0
            if want == 'record' and rel == 0 and bname in recs:
                return path, base, 0
            if bname in recs:
                sub = resolve(recs, sizes, bname, rel, want)
                if sub is None:
                    return None
                return path + '.' + sub[0], sub[1], sub[2]
            return path, base, rel
        if children(entries, chosen):
            idx = chosen          # nested entries carry offsets from the top
            continue
        return path, t, rel


def want_of(t, recs, sizes):
    """What a view field asks to be matched against: a whole embedded
    object ('record') or a field of its own size."""
    b, dims = array_dims(t)
    if not dims and not is_ptr(t) and re.sub(r'^(const |struct |union |class )+', '', t.strip()) in recs:
        return 'record'
    return tsize(t, sizes)


# -------------------------------------------------------------------- AST
def ast(f, extra):
    p = run(FLAGS + extra + lang(f) + ['-fsyntax-only', '-Xclang', '-ast-dump=json', f])
    return json.loads(p.stdout)


CUR = {'file': None}
SUBSCRIPT = {}


def note_file(n):
    """clang prints a location's file only when it differs from the last
    location it printed; follow that in traversal order."""
    for loc in (n.get('loc'), (n.get('range') or {}).get('begin'), (n.get('range') or {}).get('end')):
        if not loc:
            continue
        for k in ('spellingLoc', 'expansionLoc'):
            if k in loc and 'file' in loc[k]:
                CUR['file'] = loc[k]['file']
        if 'file' in loc:
            CUR['file'] = loc['file']


def walk(n, fn, parent=None):
    note_file(n)
    fn(n, parent)
    for c in n.get('inner', []) or []:
        walk(c, fn, n)


def ptr_to(t):
    """The type `pointer to t`, spelled for a cast."""
    t = t.strip()
    base, dims = array_dims(t)
    if dims:
        return '%s (*)%s' % (base, ''.join('[%d]' % d for d in dims))
    m = re.match(r'^(.*?)\(\*\)(\(.*\))$', t)
    if m:
        return '%s(**)%s' % (m.group(1), m.group(2))
    return t + ' *'


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    dry = '--dry' in sys.argv
    extra = []
    if '--include' in sys.argv:
        extra = ['-include', sys.argv[sys.argv.index('--include') + 1]]
        args = [a for a in args if a != sys.argv[sys.argv.index('--include') + 1]]
    f, view, canon = args[0], args[1], args[2]
    base = int(args[3], 0) if len(args) > 3 else 0
    os.chdir(ROOT)
    load_ptr_typedefs(f, extra)
    recs, sizes = layouts(f, extra)
    if view not in recs or canon not in recs:
        sys.exit('layout missing: view %s %s, canon %s %s' % (view, view in recs, canon, canon in recs))
    vfields = {}
    for j in children(recs[view], None):
        o, d, t, n = recs[view][j]
        if n:
            vfields[n] = (o, t)
    tree = ast(f, extra)
    text = open(f, encoding='latin-1').read()
    fpath = os.path.abspath(f)

    # field decl ids of the view record
    ids = {}

    def find_rec(n, parent):
        if n.get('kind') in ('RecordDecl', 'CXXRecordDecl') and n.get('name') == view and n.get('completeDefinition'):
            for c in n.get('inner', []) or []:
                if c.get('kind') == 'FieldDecl':
                    ids[c['id']] = c.get('name')
    walk(tree, find_rec)

    edits, notes = [], []

    def flat(loc):
        """A location as written in this file: a macro argument's spelling,
        else the macro use."""
        if 'spellingLoc' in loc and loc.get('isMacroArgExpansion'):
            return loc['spellingLoc']
        if 'expansionLoc' in loc:
            return loc['expansionLoc']
        return loc

    def norm_range(r):
        if not r:
            return r
        return {'begin': flat(r.get('begin', {})), 'end': flat(r.get('end', {}))}

    def loc_ok(r):
        b, e = r.get('begin', {}), r.get('end', {})
        for loc in (b, e):
            if 'file' in loc and os.path.abspath(loc['file']) != fpath:
                return False
        return 'offset' in b and 'offset' in e

    def find_subs(n, parent):
        if n.get('kind') == 'ArraySubscriptExpr':
            base, idxn = n['inner'][0], n['inner'][1]
            b = base
            while b.get('kind') in ('ImplicitCastExpr', 'ParenExpr') and b.get('inner'):
                b = b['inner'][0]
            r, ir = norm_range(n.get('range', {})), norm_range(idxn.get('range', {}))
            if b.get('kind') == 'MemberExpr' and loc_ok(r) and loc_ok(ir):
                SUBSCRIPT[id(b)] = (r['begin']['offset'], r['end']['offset'] + r['end'].get('tokLen', 0),
                                    ir['begin']['offset'], ir['end']['offset'] + ir['end'].get('tokLen', 0))
    walk(tree, find_subs)
    CUR['file'] = None

    def visit(n, parent):
        k = n.get('kind')
        if CUR['file'] and os.path.abspath(CUR['file']) != fpath and CUR['file'] != f:
            return
        if k == 'MemberExpr' and n.get('referencedMemberDecl') in ids:
            name = ids[n['referencedMemberDecl']]
            r = norm_range(n.get('range', {}))
            if not loc_ok(r):
                notes.append('MACRO %s line %s' % (name, r.get('begin', {}).get('line')))
                return
            b0 = r['begin']['offset']
            e0 = r['end']['offset'] + r['end'].get('tokLen', 0)
            inner = n['inner'][0]
            implicit_this = inner.get('kind') == 'CXXThisExpr' and inner.get('implicit')
            if implicit_this:
                ib = ie = None
            else:
                ir = norm_range(inner.get('range', {}))
                if not loc_ok(ir):
                    notes.append('MACRO base of %s' % name)
                    return
                ib = ir['begin']['offset']
                ie = ir['end']['offset'] + ir['end'].get('tokLen', 0)
            if text[e0 - len(name):e0] != name or (ib is not None and not (b0 <= ib < ie <= e0)):
                if text[e0 - len(name):e0] != name and 'expansionLoc' in json.dumps(n.get('range', {})):
                    notes.append('MACRO %s line %d' % (name, text.count('\n', 0, b0) + 1))
                return                      # compiler-generated (implicit copy etc.)
            vo, vt = vfields[name]
            res = resolve(recs, sizes, canon, base + vo, want_of(vt, recs, sizes))
            line = text.count('\n', 0, b0) + 1
            if res is None:
                notes.append('NOFIT %s @0x%X line %d' % (name, base + vo, line))
                return
            path, leaf, rem = res
            if is_ptr(vt) != is_ptr(leaf) and (is_ptr(vt) or is_ptr(leaf)):
                notes.append('KIND %s (%s) -> %s (%s) line %d' % (name, vt, path, leaf, line))
            arrow = n.get('isArrow')
            obj = '((%s *)(%s))' % (canon, '%s') if arrow else '((%s *)&(%s))' % (canon, '%s')
            if implicit_this:
                obj = '((%s *)(this))' % canon
            if rem:
                expr = '(*(%s)((char *)&%s->%s + %d))' % (ptr_to(vt), obj, path, rem)
            else:
                expr = '(*(%s)&%s->%s)' % (ptr_to(vt), obj, path)
            sub = SUBSCRIPT.get(id(n))
            vb, vdims = array_dims(vt)
            vbn = re.sub(r'^(const |struct |union |class )+', '', vb or '')
            lb, ldims = array_dims(leaf)
            if sub is not None and vdims and len(vdims) == 1 and vbn in recs and \
                    (path.endswith('[0]') or ldims):
                # an element of an array of records: index the canonical array
                sb, se, xb, xe = sub
                arr = path if ldims else path[:-3]
                if arrow:
                    eexpr = '(*(%s *)&((%s *)(%s))->%s[%s])' % (vb, canon, '%s', arr, '%s')
                else:
                    eexpr = '(*(%s *)&((%s *)&(%s))->%s[%s])' % (vb, canon, '%s', arr, '%s')
                if implicit_this:
                    eexpr = '(*(%s *)&((%s *)(this))->%s[%s])' % (vb, canon, arr, '%s')
                edits.append((sb, se, ib, ie, eexpr, name, xb, xe))
                return
            edits.append((b0, e0, ib, ie, expr, name))
        elif k in ('VarDecl', 'FieldDecl') and re.search(r'\b%s\b(?!\s*\*)' % re.escape(view),
                                                          (n.get('type') or {}).get('qualType', '')):
            qt = n['type']['qualType']
            if not qt.strip().endswith('*') and 'loc' in n and 'line' in n['loc']:
                notes.append('VALUE %s %s line %s' % (qt, n.get('name'), n['loc'].get('line')))
        elif k == 'UnaryExprOrTypeTraitExpr' and view in json.dumps(n.get('argType', '')):
            notes.append('VALUE sizeof(%s) line %s' % (view, n.get('range', {}).get('begin', {}).get('line')))
    walk(tree, visit)

    def render(start, end):
        out, pos = [], start
        for ed in sorted(edits):
            b0, e0, ib, ie, expr, name = ed[:6]
            if b0 < pos or e0 > end or (b0, e0) == (start, end):
                continue
            # outermost only: skip if contained in another edit inside range
            if any(b1 <= b0 and e0 <= e1 and (b1, e1) != (b0, e0) and b1 >= start and e1 <= end
                   and (b1, e1) != (start, end) for (b1, e1, *_r) in edits):
                continue
            out.append(text[pos:b0])
            if len(ed) == 8:
                xb, xe = ed[6], ed[7]
                if ib is None:
                    out.append(expr % render(xb, xe))
                else:
                    out.append(expr % (render(ib, ie), render(xb, xe)))
            else:
                out.append(expr % render(ib, ie) if ib is not None else expr)
            pos = e0
        out.append(text[pos:end])
        return ''.join(out)

    new = render(0, len(text))
    if extra and edits:
        hdr = os.path.basename(extra[1])
        if '#include "%s"' % hdr not in new:
            m = re.search(r'^#include[^\n]*\n', new, re.M)
            at = m.end() if m else 0
            new = new[:at] + '#include "%s"   /* %s, the canonical record */\n' % (hdr, canon) + new[at:]
    for n in notes:
        print(f + ': ' + n)
    print('%s: %d accesses through %s rewritten onto %s' % (f, len(edits), view, canon))
    if not dry and new != text:
        open(f, 'w', encoding='latin-1').write(new)


if __name__ == '__main__':
    main()
