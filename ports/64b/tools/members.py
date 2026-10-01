#!/usr/bin/env python3
"""Move every member access of a struct (or of a view of it) onto its
canonical spec.

The decomp often declares one object several times under different names
(views), each naming the fields one module touched. The 64-bit core keeps
one definition per object: ports/64b/types/<Rec>.spec. This rewrites
`x->name` / `x.name` for REC and for each VIEW to the canonical field at the
same ORIGINAL offset -- by name when the type agrees, as an explicit
reinterpretation of the canonical field when it does not -- and then turns
each view's definition into an alias of the canonical struct.

Run it BEFORE emitting a changed spec: the old layouts come from the
headers as they stand.

Usage: members.py REC [--view VIEW=FILE ...] [--dry] [FILE...]
       (FILE default: every core source file)
"""
import argparse
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recspec  # noqa: E402
import rawscan  # noqa: E402
import rewrite  # noqa: E402

ROOT = recspec.ROOT


def old_layout(rec, path, hdrs):
    """{field name: (offset, type)} of rec as currently declared."""
    sz, fl = recspec.layout32(rec, path, hdrs)
    return {name: (off, ty) for off, ty, name in fl}


def ptr_to(ty):
    """The C text of `pointer to ty`."""
    m = re.match(r'(.*?)\s*((?:\[\w+\])+)$', ty)
    if m:
        return '%s (*)%s' % (m.group(1), m.group(2))
    return ty + ' *'


def core_files():
    out = []
    for d in ('ports/64b/src/core',):
        for dp, _, fns in os.walk(os.path.join(ROOT, d)):
            for fn in fns:
                if fn.endswith(('.c', '.cpp')):
                    out.append(os.path.relpath(os.path.join(dp, fn), ROOT))
    return sorted(out)


def process(f, layouts, canon, report, dry):
    """layouts: {record name: {field: (off, ty)}} for REC and its views."""
    a = rawscan.ast(f)
    if a is None:
        report['noast'].append(f)
        return 0
    path = os.path.join(ROOT, f)
    s = open(path, encoding='latin-1').read()
    fabs = os.path.abspath(path)
    owner = {}          # FieldDecl id -> record name

    def collect(n, rec):
        k = n.get('kind')
        if k in ('RecordDecl', 'CXXRecordDecl'):
            rec = n.get('name') or rec
        if k == 'TypedefDecl':
            pass
        if k == 'FieldDecl' and rec:
            owner[n['id']] = rec
        for c in n.get('inner', []) or []:
            if isinstance(c, dict):
                collect(c, rec)
    # typedef names -> tag, so an anonymous `typedef struct {...} X` is X
    tagname = {}

    def typedefs(n):
        if n.get('kind') == 'TypedefDecl':
            inner = n.get('inner', [])
            for c in inner:
                d = c.get('decl') or (c.get('inner', [{}])[0].get('decl') if c.get('inner') else None)
                if d and d.get('id'):
                    tagname[d['id']] = n.get('name')
        for c in n.get('inner', []) or []:
            if isinstance(c, dict):
                typedefs(c)
    typedefs(a)

    def collect2(n, rec):
        k = n.get('kind')
        if k in ('RecordDecl', 'CXXRecordDecl'):
            rec = tagname.get(n.get('id')) or n.get('name') or rec
        if k == 'FieldDecl' and rec:
            owner[n['id']] = rec
        for c in n.get('inner', []) or []:
            if isinstance(c, dict):
                collect2(c, rec)
    collect2(a, None)

    edits = []

    def infile(n):
        loc = n.get('loc') or n.get('range', {}).get('begin', {})
        fl = loc.get('file')
        return fl is None or os.path.abspath(os.path.join(ROOT, fl)) == fabs

    def walk(n):
        if n.get('kind') == 'MemberExpr':
            d = n.get('referencedMemberDecl')
            rec = owner.get(d)
            if rec in layouts:
                handle(n, rec)
        for c in n.get('inner', []) or []:
            if isinstance(c, dict) and (infile(c) or c.get('kind') not in (
                    'FunctionDecl', 'VarDecl', 'RecordDecl', 'TypedefDecl', 'CXXRecordDecl')):
                walk(c)

    def handle(n, rec):
        name = n.get('name')
        if not name or n.get('isImplicit'):
            return
        if name not in layouts[rec]:
            report['unknown'].append((f, rec, name))
            return
        off, oty = layouts[rec][name]
        for v in layouts:
            oty = re.sub(r'\b%s\b' % re.escape(v), canon, oty)
        pth, fty = rewrite.resolve(canon, off, oty)
        if pth is None:
            report['unresolved'][(canon, off, oty)].append((f, rec, name))
            return
        if pth == name and rec == canon:
            return
        r = n.get('range', {})
        b, e = r.get('begin', {}), r.get('end', {})
        if any(k in x for x in (b, e) for k in ('spellingLoc', 'expansionLoc')) or 'offset' not in e:
            # a member named in a macro ARGUMENT is spelled in this file
            sp = e.get('spellingLoc', {})
            same = rewrite.same_type(fty, oty) or re.sub(r'\s', '', fty) == re.sub(r'\s', '', oty)
            if same and 'offset' in sp and s[sp['offset']:sp['offset'] + len(name)] == name \
                    and 'includedFrom' not in sp and (sp.get('file') is None or
                                                      os.path.abspath(os.path.join(ROOT, sp['file'])) == fabs):
                edits.append((sp['offset'], sp['offset'] + len(name), pth))
                return
            report['macro'].append((f, rec, name))
            return
        if rewrite.same_type(fty, oty) or re.sub(r'\s', '', fty) == re.sub(r'\s', '', oty):
            # the member token is the last token of the MemberExpr
            z = e['offset'] + e.get('tokLen', 0)
            a0 = e['offset']
            if s[a0:z] != name:
                report['macro'].append((f, rec, name))
                return
            edits.append((a0, z, pth))
        else:
            base = n['inner'][0]
            ba, bz, btxt = rewrite.src_text(s, base)
            if btxt is None:
                report['macro'].append((f, rec, name))
                return
            op = '->' if n.get('isArrow') else '.'
            a0, z0 = b['offset'], e['offset'] + e.get('tokLen', 0)
            mark = ''
            if (fty.rstrip().endswith('*') or fty.startswith('fnptr')) and not oty.rstrip().endswith('*'):
                mark = ' /* BR_LP64_SCALAR_IN_PTR */'
                report['lp64'] += 1
                report.setdefault('lp64sites', []).append('%s:%d %s.%s' % (
                    f, s.count('\n', 0, a0) + 1, rec, name))
            edits.append((a0, z0, '(*(%s)&%s%s%s)%s' % (ptr_to(oty), btxt, op, pth, mark)))
            report['punned'] += 1

    for top in a.get('inner', []):
        if infile(top):
            walk(top)
    edits = sorted(set(edits))
    taken, last = [], -1
    for e in edits:
        if e[0] < last:
            report['overlap'] += 1
            continue
        taken.append(e)
        last = e[1]
    if taken and not dry:
        for a0, z0, text in reversed(taken):
            s = s[:a0] + text + s[z0:]
        open(path, 'w', encoding='latin-1').write(s)
    return len(taken)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rec')
    ap.add_argument('--view', action='append', default=[], help='VIEW=FILE declaring it')
    ap.add_argument('--old', help='file declaring REC as it stands (default: the spec header)')
    ap.add_argument('--dry', action='store_true')
    ap.add_argument('files', nargs='*')
    a = ap.parse_args()
    os.chdir(ROOT)
    meta, _ = recspec.load(a.rec)
    hdrs = meta.get('headers', '').split()
    layouts = {a.rec: old_layout(a.rec, a.old or meta['header'], hdrs)}
    for v in a.view:
        name, fpath = v.split('=', 1)
        layouts[name] = old_layout(name, fpath, hdrs)
    report = {'unknown': [], 'unresolved': collections.defaultdict(list), 'macro': [], 'noast': [],
              'punned': 0, 'overlap': 0, 'lp64': 0}
    n = 0
    for f in a.files or core_files():
        n += process(f, layouts, a.rec, report, a.dry)
    print('members: %d rewritten (%d reinterpreted, %d scalar-in-pointer), %d in macros, %d overlapping, %d unresolved' % (
        n, report['punned'], report['lp64'], len(report['macro']), report['overlap'], len(report['unresolved'])))
    for (rec, off, ty), where in sorted(report['unresolved'].items()):
        print('  unresolved %s+0x%X %s  <- %s' % (rec, off, ty, sorted({'%s.%s' % (r, nm) for _, r, nm in where})))
    for x in report.get('lp64sites', []):
        print('  scalar-in-pointer', x)
    for x in report['macro']:
        print('  macro', x)
    if report['noast']:
        print('  no AST:', report['noast'][:10])


if __name__ == '__main__':
    main()
