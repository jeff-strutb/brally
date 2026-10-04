#!/usr/bin/env python3
"""Remove the x86 thiscall-as-fastcall dummy EDX parameter.

The matching source spells a thiscall with stack arguments as
`__fastcall f(T *this, int _edx, args...)`.  On a 64-bit ABI every argument
is positional, so the dummy shifts the real ones; a vtable call through a
C++ view (this, args...) would hand args to the wrong parameters.  This
removes the dummy from each such definition and its prototype, and the
second argument from every direct call.

Usage: dropedx.py NAME...
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))


def files():
    for top in ('ports/brally/src/core', 'ports/brally/include'):
        for dp, _, fs in os.walk(os.path.join(ROOT, top)):
            for f in fs:
                if f.endswith(('.c', '.cpp', '.h')):
                    yield os.path.join(dp, f)


def split_args(s, i):
    """s[i] == '(' -> (list of (start, end) arg spans, index of ')')."""
    depth, spans, start = 0, [], i + 1
    j = i
    while j < len(s):
        c = s[j]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                spans.append((start, j))
                return spans, j
        elif c == ',' and depth == 1:
            spans.append((start, j))
            start = j + 1
        j += 1
    raise ValueError('unbalanced')


def drop_second(s, name, nparams):
    """Drop the second argument of every call or declaration that has all
    nparams (calls that already omit the dummy are left alone)."""
    out, pos, n = [], 0, 0
    for m in re.finditer(r'\b%s\s*\(' % re.escape(name), s):
        if m.start() < pos:
            continue
        spans, close = split_args(s, m.end() - 1)
        if len(spans) != nparams:
            continue
        a, b = spans[1]
        arg = s[a:b].strip()
        # a declaration's dummy, or a call's second argument
        out.append(s[pos:spans[0][1]])
        pos = b
        n += 1
    out.append(s[pos:])
    return ''.join(out), n


def nparams_of(name):
    for f in files():
        s = open(f, encoding='latin-1').read()
        m = re.search(r'__fastcall\s*\**\s*%s\s*\(' % re.escape(name), s)
        if m:
            spans, close = split_args(s, m.end() - 1)
            if '{' in s[close:close + 40]:
                return len(spans)
    raise SystemExit('no definition: ' + name)


def main():
    names = {n: nparams_of(n) for n in sys.argv[1:]}
    for f in files():
        s = open(f, encoding='latin-1').read()
        t = s
        for name, k in names.items():
            t, n = drop_second(t, name, k)
        t = re.sub(r'\n\s*\(void\)\s*_edx\w*;', '', t)
        if t != s:
            open(f, 'w', encoding='latin-1').write(t)
            print(os.path.relpath(f, ROOT))


if __name__ == '__main__':
    main()
