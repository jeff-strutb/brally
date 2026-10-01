#!/usr/bin/env python3
"""Move the file-private types br_globals.h needs into br_coretypes.h.

Compiles br_globals.h on its own; for every unknown type (or undeclared
array-size macro) it finds the one core source file that defines it, cuts the
definition out of that file and appends it to br_coretypes.h. Repeats until
the header compiles.

Usage: movetypes.py
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
CORE = os.path.join(ROOT, 'ports/64b/include/br_coretypes.h')
FLAGS = ['-fsyntax-only', '-w', '-ferror-limit=0', '-fms-extensions', '-fdeclspec', '-D_FORTIFY_SOURCE=0',
         '-Iports/64b/platform/include', '-Iports/64b/include',
         '-include', 'ports/64b/platform/include/win32.h', '-include', 'ports/64b/platform/include/br_lp64.h',
         '-x', 'c', '-std=gnu89']


def errors():
    p = subprocess.run(['clang'] + FLAGS + ['-include', 'ports/64b/include/br_globals.h', '-'],
                       input=b'', capture_output=True, cwd=ROOT)
    return p.stderr.decode('utf-8', 'replace')


def sources():
    for dp, _, fns in os.walk(os.path.join(ROOT, 'ports/64b/src/core')):
        for fn in fns:
            if fn.endswith(('.c', '.cpp')):
                yield os.path.join(dp, fn)


def find_def(name):
    """(file, start, end, text) of the one definition of a type or macro."""
    hits = []
    pats = [re.compile(r'typedef\s+(?:struct|union|enum)\s*\w*\s*\{', re.S),
            re.compile(r'typedef\s+[^;{}]*?\b%s\b[^;{}]*;' % re.escape(name)),
            re.compile(r'(?:struct|union)\s+%s\s*\{' % re.escape(name)),
            re.compile(r'^[ \t]*#[ \t]*define[ \t]+%s\b.*$' % re.escape(name), re.M)]
    for f in sources():
        s = open(f, encoding='latin-1').read()
        if not re.search(r'\b%s\b' % re.escape(name), s):
            continue
        # typedef struct { ... } NAME;
        for m in re.finditer(r'\}\s*%s\s*;' % re.escape(name), s):
            start = s.rfind('typedef', 0, m.start())
            if start == -1:
                continue
            seg = s[start:m.end()]
            if seg.count('{') == seg.count('}'):
                hits.append((f, start, m.end(), seg))
                break
        else:
            m = pats[1].search(s) or pats[3].search(s)
            if m:
                hits.append((f, m.start(), m.end(), m.group(0)))
            else:
                m = pats[2].search(s)
                if m:
                    i, depth = m.end() - 1, 0
                    while i < len(s):
                        if s[i] == '{':
                            depth += 1
                        elif s[i] == '}':
                            depth -= 1
                            if depth == 0:
                                break
                        i += 1
                    e = s.find(';', i) + 1
                    hits.append((f, m.start(), e, s[m.start():e]))
    # identical definitions in several files: take them all out, keep one
    texts = {re.sub(r'\s+', ' ', re.sub(r'/\*.*?\*/|//[^\n]*', '', h[3], flags=re.S)).strip() for h in hits}
    if not hits or len(texts) > 1:
        return None, hits
    return hits[0], hits


def main():
    os.chdir(ROOT)
    core = open(CORE).read()
    if '<stdio.h>' not in core:
        core = core.replace('#include <stdint.h>\n', '#include <stdint.h>\n#include <stdio.h>\n'
                            '#include "br_vec.h"\n#include "br_mat.h"\n', 1)
        open(CORE, 'w').write(core)
    moved, stuck = [], set()
    for _ in range(40):
        e = errors()
        names = []
        for m in re.finditer(r"error: (?:unknown type name|use of undeclared identifier) '(\w+)'", e):
            if m.group(1) not in names and m.group(1) not in stuck:
                names.append(m.group(1))
        for m in re.finditer(r"error: (?:array has incomplete element type|variable has incomplete type) "
                             r"'(?:struct )?(\w+)'", e):
            if m.group(1) not in names and m.group(1) not in stuck:
                names.append(m.group(1))
        if not names:
            break
        progress = False
        for nm in names:
            one, hits = find_def(nm)
            if one is None:
                stuck.add(nm)
                print('  cannot place %s: %d differing definitions' % (nm, len(hits)))
                continue
            text = one[3]
            for f, a, b, _ in sorted(hits, key=lambda h: (h[0], -h[1])):
                s = open(f, encoding='latin-1').read()
                s = s[:a] + '/* %s: br_coretypes.h */' % nm + s[b:]
                open(f, 'w', encoding='latin-1').write(s)
            core = open(CORE).read()
            # dependencies are found after their users: put each new one
            # first, after the includes
            marker = '/* BR_CORETYPES_BODY */\n'
            if marker not in core:
                core = core.replace('typedef int (*funcptr)();', marker + 'typedef int (*funcptr)();', 1)
            core = core.replace(marker, marker + text + '\n\n', 1)
            open(CORE, 'w').write(core)
            moved.append(nm)
            progress = True
        if not progress:
            break
    left = re.findall(r'error: .*', errors())
    print('moved %d types/macros into br_coretypes.h; %d errors left' % (len(moved), len(left)))
    for x in left[:15]:
        print('  ' + x)


if __name__ == '__main__':
    main()
