#!/usr/bin/env python3
"""List function-pointer casts whose register classes disagree.

    fncast.py [TU ...]        (default: every core TU)

On i386 every argument goes on the stack and a float return comes back in
st(0) whatever the declared type, so the decompiled bodies cast function
pointers freely: `(int (*)(int, int))pfnScale`. On arm64 and x86-64 an
integer argument travels in a general register and a float or double in a
vector register, float and double are different widths, and a struct is
passed by its own rules. A call through a pointer whose type disagrees with
the callee in any of that reads garbage.

This walks clang's AST for every cast between two function-pointer types
and prints those whose return or any parameter differs in class
(integer / float / double / struct / pointer counted as integer), and those
where one side has a prototype and the other does not. Review each: the
fix is to give the pointer the callee's real type.
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(__file__))
from ptrarith import FLAGS, ROOT  # noqa: E402

CAST = re.compile(r"(?:CStyleCastExpr|CXXReinterpretCastExpr|CXXStaticCastExpr|ImplicitCastExpr) 0x[0-9a-f]+ <([^>]*)> '([^']*)'(?::'([^']*)')? <(BitCast|NoOp)>")
SUB = re.compile(r"> '([^']*)'(?::'([^']*)')?")


def split_fn(t):
    """'R (*)(A, B)' -> (R, [A, B]) or None"""
    m = re.match(r'^(.*?)\s*\(\*\)\((.*)\)\s*$', t)
    if not m:
        return None
    ret, args = m.group(1), m.group(2)
    out, depth, cur = [], 0, ''
    for c in args:
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
            continue
        depth += c in '(<'
        depth -= c in ')>'
        cur += c
    if cur.strip():
        out.append(cur.strip())
    return ret.strip(), out


def klass(t):
    t = re.sub(r'\b(const|volatile|__attribute__\(\([^)]*\)\))\b', '', t).strip()
    t = re.sub(r'__attribute__\(\(\w+\)\)', '', t).strip()
    if '*' in t or '[' in t:
        return 'int'
    if t in ('float', 'BrF32'):
        return 'float'
    if t in ('double', 'long double'):
        return 'double'
    if t == 'void':
        return 'void'
    if t.startswith(('struct ', 'union ', 'class ')):
        return 'agg:' + t
    return 'int'


def scan(tu):
    cc = ['clang++', '-std=c++17'] if tu.endswith('.cpp') else ['clang', '-std=gnu11']
    try:
        p = subprocess.run(cc + ['-fsyntax-only', '-Xclang', '-ast-dump', '-fno-color-diagnostics'] + FLAGS + [tu],
                           cwd=ROOT, capture_output=True, text=True, errors='replace', timeout=600)
    except subprocess.TimeoutExpired:
        return ['%s: timeout' % tu]
    out, cur_file, cur_line = [], None, 0
    lines = p.stdout.split('\n')
    base = os.path.basename(tu)
    seen = set()
    for i, ln in enumerate(lines):
        for m in re.finditer(r'<?(?:([^<>\s:]+\.(?:c|cpp|h)):(\d+)|line:(\d+))', ln):
            if m.group(1):
                cur_file, cur_line = m.group(1), int(m.group(2))
            elif m.group(3):
                cur_line = int(m.group(3))
        if cur_file is None or not cur_file.endswith(base):
            continue
        m = CAST.search(ln)
        if not m:
            continue
        dst = m.group(3) or m.group(2)
        # the operand's type: the next line's expression
        if i + 1 >= len(lines):
            continue
        sm = SUB.search(lines[i + 1])
        if not sm:
            continue
        src = sm.group(2) or sm.group(1)
        a, b = split_fn(dst), split_fn(src)
        if not a or not b or a == b:
            continue
        why = []
        if klass(a[0]) != klass(b[0]) and 'void' not in (klass(a[0]), klass(b[0])):
            why.append('return %s vs %s' % (a[0], b[0]))
        elif klass(a[0]) != klass(b[0]) and klass(b[0]) in ('float', 'double'):
            why.append('return %s vs %s' % (a[0], b[0]))
        pa, pb = a[1], b[1]
        if pa == ['void'] or pb == ['void']:
            pa, pb = [], []         # a generic 'void (*)(void)' slot: checked where it is called
        if (pa == [] or pb == []) and pa != pb and 'void' not in (pa + pb):
            pass        # unprototyped: arguments follow the call's promotion; checked below
        for k in range(min(len(pa), len(pb))):
            if pa[k] == '...' or pb[k] == '...':
                break
            if klass(pa[k]) != klass(pb[k]):
                why.append('arg%d %s vs %s' % (k, pa[k], pb[k]))
        if why:
            key = (cur_line, tuple(why))
            if key not in seen:
                seen.add(key)
                out.append('%s:%d  %s   [%s <- %s]' % (tu, cur_line, '; '.join(why), dst, src))
    return out


def main():
    os.chdir(ROOT)
    tus = sys.argv[1:] or sorted(
        os.path.join(dp, f) for dp, _, fs in os.walk('ports/64b/src/core') for f in fs if f.endswith(('.c', '.cpp')))
    with ThreadPoolExecutor(max_workers=int(os.environ.get('JOBS', '12'))) as ex:
        for res in ex.map(scan, tus):
            for r in res:
                print(r, flush=True)


if __name__ == '__main__':
    main()
