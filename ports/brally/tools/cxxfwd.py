#!/usr/bin/env python3
"""Resolve the C++ lane's calls into functions other files define.

The C++-lane files declare small local classes whose methods are functions
at fixed addresses in the original; MSVC called them by address, so one
`?SetPos@Car5C6D0@@...` and another file's `?SetPos@BrCar@@...` were the
same function at 0x1006F680. Natively each local class's method is its own
undefined symbol. For every such symbol a TU references and no TU defines:

  non-virtual method, constructor, destructor
      an out-of-line definition in that TU that calls the C entry of the
      function at the method's original address (build/brally/wasm32/sites.csv gives
      the address, the @implements line the entry, br_funcs.h its
      prototype):
          void Car5C6D0::SetPos(float a1, float a2, float a3)
          { BrCarSetPos_1006F680((void *)this, a1, a2, a3); }

  virtual method called on an object of known type (clang calls it
  directly; MSVC went through the vtable)
      the call is rewritten to go through the object's vtable slot:
          (BR_VFN(&pObj->m2B5C, 2, void (*)(void *)))(&pObj->m2B5C)

Usage: cxxfwd.py [--dry] [FILE...]    (default: every TU with such symbols)
"""
import csv
import glob
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

ROOT = vm.ROOT
OBJ = 'build/brally/null-soft/obj'


def missing_by_tu():
    defined, refs = set(), {}
    for o in glob.glob(OBJ + '/*.o'):
        out = subprocess.run(['nm', o], capture_output=True, text=True).stdout
        for line in out.splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in 'TDBSCtdbsc' and p[1].isupper():
                defined.add(p[2])
            elif len(p) == 2 and p[0] == 'U':
                refs.setdefault(o, set()).add(p[1])
    tus = {}
    for o, syms in refs.items():
        miss = set(s for s in syms if s.startswith('__Z') and s not in defined)
        if miss:
            src = 'ports/brally/src/core/' + os.path.basename(o)[:-2].replace('__', '/')
            tus[src] = miss
    return tus


def sites():
    """(src path under src/brally/core, qualified name) -> VA"""
    out = {}
    for r in csv.DictReader(open('build/brally/wasm32/sites.csv')):
        out.setdefault((r['src'], r['name']), int(r['va'], 16))
    return out


def implementers():
    """VA -> the canonical function placed there (build/brally/wasm32/placement.csv)"""
    out = {}
    for line in open('build/brally/wasm32/placement.csv'):
        p = line.strip().split(',')
        if len(p) >= 2 and p[0].startswith('0x'):
            out.setdefault(int(p[0], 16), p[1])
    return out


def va_from_name(cls, name, kind):
    """Addresses the decompiled names carry: m_1006CDD0, and Class_CTOR_DTOR."""
    m = re.match(r'^m_([0-9A-Fa-f]{8})$', name)
    if m:
        return int(m.group(1), 16)
    m = re.match(r'^\w+?_([0-9A-Fa-f]{8})_([0-9A-Fa-f]{8})$', cls)
    if m and kind == 'CXXConstructorDecl':
        return int(m.group(1), 16)
    if m and kind == 'CXXDestructorDecl':
        return int(m.group(2), 16)
    return None


def prototypes():
    out = {}
    for m in re.finditer(r'^(?!#)([^\n;()]*?)\b(\w+)\(([^;]*)\);$', open('ports/brally/include/br_funcs.h').read(), re.M):
        params = [p.strip() for p in split_params(m.group(3))]
        if params == ['void']:
            params = []
        out[m.group(2)] = (m.group(1).strip(), params)
    return out


def split_params(s):
    out, d, cur = [], 0, ''
    for ch in s:
        if ch in '([':
            d += 1
        elif ch in ')]':
            d -= 1
        if ch == ',' and d == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def fn_type(qt):
    """'float (int, float)' -> ('float', ['int', 'float'])"""
    m = re.match(r'^(.*?)\s*\((.*)\)\s*(const)?$', qt)
    ret, params = m.group(1).strip(), [p.strip() for p in split_params(m.group(2))]
    if params == ['void']:
        params = []
    return ret, params


def main():
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    want = [a for a in sys.argv[1:] if not a.startswith('--')]
    tus = missing_by_tu()
    st, impl, protos = sites(), implementers(), prototypes()
    report = []
    for src, miss in sorted(tus.items()):
        if want and src not in want:
            continue
        tree = vm.ast(src, [])
        text = open(src, encoding='latin-1').read()
        fpath = os.path.abspath(src)
        decls, records = {}, {}

        def collect(n, rec=None):
            k = n.get('kind')
            if k in ('CXXRecordDecl',) and n.get('completeDefinition'):
                rec = n
                records[n['id']] = n
            if k in ('CXXMethodDecl', 'CXXConstructorDecl', 'CXXDestructorDecl') and n.get('mangledName') in miss \
                    and rec is not None and not n.get('isImplicit'):
                decls[n['id']] = (n, rec)
            for c in n.get('inner', []) or []:
                collect(c, rec)
        collect(tree)

        edits, defs, done = [], [], set()

        def rng(n):
            r = n.get('range') or {}
            b, e = r.get('begin', {}), r.get('end', {})
            if 'offset' not in b or 'offset' not in e or 'spellingLoc' in b or 'expansionLoc' in b:
                return None
            return b['offset'], e['offset'] + e.get('tokLen', 0)

        def visit(n, parent):
            if n.get('kind') != 'CXXMemberCallExpr':
                return
            me = n['inner'][0]
            if me.get('kind') != 'MemberExpr':
                return
            d = decls.get(me.get('referencedMemberDecl'))
            if d is None or not d[0].get('virtual'):
                return
            decl, rec = d
            if rec.get('bases'):
                report.append('%s: %s has bases; slot not derived' % (src, decl['mangledName']))
                return
            virt = [c for c in rec.get('inner', []) if c.get('kind') in ('CXXMethodDecl', 'CXXDestructorDecl')
                    and c.get('virtual') and not c.get('isImplicit')]
            slot = [c['id'] for c in virt].index(decl['id'])
            obj = me['inner'][0]
            ro, rn = rng(obj), rng(n)
            if ro is None or rn is None:
                report.append('%s: %s call in a macro' % (src, decl['mangledName']))
                return
            o = text[ro[0]:ro[1]]
            ptr = o if me.get('isArrow') else '&(%s)' % o
            if o == '' or (me.get('isArrow') and obj.get('kind') == 'CXXThisExpr' and obj.get('isImplicit')):
                ptr = 'this'
            ret, params = fn_type(decl['type']['qualType'])
            args = [text[a[0]:a[1]] for a in (rng(x) for x in n['inner'][1:]) if a]
            ft = '%s (*)(%s)' % (ret, ', '.join(['void *'] + params))
            edits.append((rn[0], rn[1], '(BR_VFN(%s, %d, %s))(%s)' % (ptr, slot, ft, ', '.join([ptr] + args))))
            done.add(decl['mangledName'])

        vm.CUR['file'] = None
        vm.walk(tree, visit)

        for did, (decl, rec) in decls.items():
            mn = decl['mangledName']
            if decl.get('virtual') or mn in done:
                continue
            cls, name = rec['name'], decl['name']
            q = '%s::%s' % (cls, name)
            va = st.get((src.replace('ports/brally/', ''), q)) or va_from_name(cls, name, decl['kind'])
            if va is None:
                report.append('%s: %s (%s) has no address in sites.csv' % (src, q, mn))
                continue
            entry = impl.get(va)
            if entry is None or entry not in protos:
                report.append('%s: %s at 0x%08X has no C entry%s' % (src, q, va, '' if entry is None else ' prototype (' + entry + ')'))
                continue
            pret, pparams = protos[entry]
            ret, params = fn_type(decl['type']['qualType'])
            k = decl['kind']
            formals = ', '.join('%s a%d' % (t, i + 1) for i, t in enumerate(params))
            # the original passes `this` and the arguments; an entry that
            # takes fewer (an empty stub) never read the rest
            actual = (['this'] + ['a%d' % (i + 1) for i in range(len(params))])[:len(pparams)]
            actual = ['(%s)%s' % (pparams[i], a) for i, a in enumerate(actual)]
            call = '%s(%s)' % (entry, ', '.join(actual))
            if k == 'CXXConstructorDecl':
                head = '%s::%s(%s)' % (cls, cls, formals)
                body = '%s;' % call
            elif k == 'CXXDestructorDecl':
                head = '%s::~%s()' % (cls, cls)
                body = '%s;' % call
            else:
                head = '%s %s::%s(%s)' % (ret, cls, name, formals)
                body = ('%s;' % call) if ret == 'void' else ('return (%s)%s;' % (ret, call))
            defs.append('/* 0x%08X: the original calls %s by address */\n%s\n{\n    %s\n}\n' % (va, entry, head, body))
        if not edits and not defs:
            continue
        out = text
        keep, last = [], -1
        for e in sorted(edits):
            if e[0] >= last:
                keep.append(e)
                last = e[1]
        for b0, e0, rep in sorted(keep, reverse=True):
            out = out[:b0] + rep + out[e0:]
        if defs:
            out = out.rstrip('\n') + '\n\n/* Methods of the local classes above that other files define: each is\n * the function at its original address, reached through its C entry. */\n' + '\n'.join(defs)
        print('%s: %d vtable calls, %d forwards' % (src, len(keep), len(defs)))
        if not dry:
            open(src, 'w', encoding='latin-1').write(out)
    for r in report:
        print('  ' + r)


if __name__ == '__main__':
    main()
