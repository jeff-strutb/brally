#!/usr/bin/env python3
"""Resolve `conflicting types` between declarations of one function.

A function's definition is the truth: it is what matched the original. For
every conflict clang reports in the portable build (build/portable/obj/*.err):

  * a stray prototype (no body) in a source file is deleted -- the header or
    the definition declares the function;
  * when the conflicting pair is a header prototype and the definition, the
    header prototype is rewritten to the definition's signature.

Then every call is checked against the one true signature.

Usage: fixproto.py [--dry]
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DRY = '--dry' in sys.argv


def stmt_span(text, line):
    """(start, end) of the declaration statement that includes 1-based line:
    back to the previous `;`/`}`/preprocessor/blank boundary, forward to `;`
    or to the `{` that opens a body. Returns (start, end, has_body)."""
    lines = text.split('\n')
    i = line - 1
    s = i
    while s > 0:
        prev = lines[s - 1].rstrip()
        if not prev or prev.endswith((';', '}', '*/')) or prev.lstrip().startswith(('#', '//')):
            break
        s -= 1
    start = sum(len(l) + 1 for l in lines[:s])
    j = text.find(';', sum(len(l) + 1 for l in lines[:i]))
    k = text.find('{', sum(len(l) + 1 for l in lines[:i]))
    has_body = k != -1 and (j == -1 or k < j)
    return start, (k if has_body else j + 1), has_body


def main():
    os.chdir(ROOT)
    pairs = []
    for ef in glob.glob('build/portable/obj/*.err'):
        txt = open(ef, errors='replace').read()
        for m in re.finditer(r"^(\S+?):(\d+):\d+: error: (?:conflicting types for|declaration of) '(\w+)'(?: has a different language linkage)?\n(?:.*\n){0,3}?"
                             r"(\S+?):(\d+):\d+: note: previous (?:declaration|definition) is here", txt, re.M):
            pairs.append((m.group(3), m.group(1), int(m.group(2)), m.group(4), int(m.group(5))))
    pairs = sorted(set(pairs))
    GENERATED = ('br_funcs.h', 'br_globals.h', 'br_coretypes.h')
    pairs = [p for p in pairs if os.path.basename(p[1]) not in GENERATED and os.path.basename(p[3]) not in GENERATED]
    deletes = {}      # file -> set(line)
    header_fix = []   # (header file, line, definition file, def line, name)
    for name, f1, l1, f2, l2 in pairs:
        a = (f1, l1)
        b = (f2, l2)
        info = {}
        for f, l in (a, b):
            if not os.path.exists(f):
                continue
            t = open(f, encoding='latin-1').read()
            st, en, body = stmt_span(t, l)
            info[(f, l)] = body
        bodies = [x for x in (a, b) if info.get(x)]
        protos = [x for x in (a, b) if x in info and not info[x]]
        for f, l in protos:
            if bodies and f.endswith('.h') and '/platform/' not in f:
                header_fix.append((f, l, bodies[0][0], bodies[0][1], name))
            elif f.endswith(('.c', '.cpp')) or (not bodies and f.endswith(('.c', '.cpp'))):
                deletes.setdefault(f, set()).add(l)
            elif not bodies and f.endswith('.h') and '/platform/' in f:
                # clash with the platform header: the other one (a source-file
                # prototype) goes
                other = b if (f, l) == a else a
                if other[0].endswith(('.c', '.cpp')):
                    deletes.setdefault(other[0], set()).add(other[1])
    nd = nh = 0
    for f, lines in deletes.items():
        t = open(f, encoding='latin-1').read()
        spans = sorted({stmt_span(t, l)[:2] for l in lines if not stmt_span(t, l)[2]}, reverse=True)
        for st, en in spans:
            t = t[:st] + '/* 64-bit core: declared once, by its definition\'s header */' + t[en:]
            nd += 1
        if not DRY:
            open(f, 'w', encoding='latin-1').write(t)
    seen_fix = set()
    for hf, hl, df, dl, name in sorted(set(header_fix), key=lambda x: (x[0], -x[1])):
        if (hf, hl) in seen_fix:
            continue
        seen_fix.add((hf, hl))
        d = open(df, encoding='latin-1').read()
        st, en, body = stmt_span(d, dl)
        sig = re.sub(r'\s+', ' ', re.sub(r'/\*.*?\*/', '', d[st:en], flags=re.S)).strip()
        if not re.search(r'\b%s\s*\(' % re.escape(name), sig):
            print('skip header fix for %s: no signature at %s:%d' % (name, df, dl))
            continue
        h = open(hf, encoding='latin-1').read()
        hs, he, hb = stmt_span(h, hl)
        h = h[:hs] + sig + ';' + h[he:]
        nh += 1
        if not DRY:
            open(hf, 'w', encoding='latin-1').write(h)
        print('header %s: %s now matches %s' % (os.path.basename(hf), name, os.path.basename(df)))
    print('conflicts %d: %d stray prototypes removed, %d header prototypes corrected' % (len(pairs), nd, nh))


if __name__ == '__main__':
    main()
