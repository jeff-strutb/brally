#!/usr/bin/env python3
"""Rewrite C++ `new View` onto the original's allocation and constructor.

The C++ lane wrote `p = new Page46E70;`: MSVC's operator new of the class's
32-bit size and a call to its constructor.  At 64 bits the view's size is
not the object's, and the constructor is an original C function (the
wasm lane's symbol sites map `??0View@@QAE@XZ` to its address).  Each
new-expression becomes

    ((View *)br_new_obj(sizeof(Canon), Ctor))

with Canon the view's canonical record (types/viewmap.csv) and Ctor the C
function at the constructor's address; a class without a constructor is
just allocated.

Usage: newfix.py [--dry] [FILE...]
"""
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

ROOT = vm.ROOT


def ctor_map():
    pl = {int(r['va'], 16): r['name'] for r in csv.DictReader(open('build/brally/wasm32/placement.csv'))}
    out = {}
    for r in csv.reader(open('build/brally/wasm32/sites.csv')):
        if len(r) >= 5 and r[1].startswith('??0'):
            cls = r[2].split('::')[0]
            out[(r[0].replace('src/brally/core/', 'ports/brally/src/core/'), cls)] = pl.get(int(r[4], 16))
    return out


def canon_map():
    rules, per = {}, {}
    for r in csv.DictReader(open('ports/brally/types/viewmap.csv')):
        (per if r['file'] else rules)[(r['file'], r['view']) if r['file'] else r['view']] = r['canon']
    views = {(r['file'], r['view']) for r in csv.DictReader(open('build/brally/null-soft/views.csv'))}
    return rules, per, views


def main():
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not files:
        for dp, _, fs in os.walk('ports/brally/src/core'):
            files += [os.path.join(dp, f) for f in fs if f.endswith('.cpp')]
    ctors = ctor_map()
    rules, per, views = canon_map()
    total = 0
    for f in sorted(files):
        text = open(f, encoding='latin-1').read()
        if not re.search(r'\bnew\s+\w', text):
            continue
        tree = vm.ast(f, [])
        fpath = os.path.abspath(f)
        edits = []

        def visit(n, parent):
            if vm.CUR['file'] and os.path.abspath(vm.CUR['file']) != fpath:
                return
            if n.get('kind') != 'CXXNewExpr' or n.get('isArray'):
                return
            t = (n.get('type') or {}).get('qualType', '')
            m = re.match(r'^(?:class |struct )?(\w+) \*$', t)
            r = n.get('range') or {}
            b, e = r.get('begin', {}), r.get('end', {})
            if not m or 'offset' not in b or 'offset' not in e:
                return
            cls = m.group(1)
            canon = per.get((f, cls), rules.get(cls)) if (f, cls) in views else None
            size = 'sizeof(%s)' % (canon or cls)
            ctor = ctors.get((f, cls))
            if ctor:
                rep = '((%s *)br_new_obj(%s, (void *(*)(void *))%s))' % (cls, size, ctor)
            else:
                rep = '((%s *)BrOperatorNew(%s))' % (cls, size)
            edits.append((b['offset'], e['offset'] + e.get('tokLen', 0), rep))
        vm.CUR['file'] = None
        vm.walk(tree, visit)
        if not edits:
            continue
        out = text
        for b0, e0, rep in sorted(edits, reverse=True):
            out = out[:b0] + rep + out[e0:]
        total += len(edits)
        print('%s: %d' % (f, len(edits)))
        if not dry:
            open(f, 'w', encoding='latin-1').write(out)
    print('new-expressions rewritten: %d' % total)


if __name__ == '__main__':
    main()
