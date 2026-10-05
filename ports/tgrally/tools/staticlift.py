#!/usr/bin/env python3
"""staticlift.py -- a function's static variables that the original kept at
a known address become that address's symbol, in game memory.

The decomp writes a function-scope static with its address in a comment:

    static unsigned char entered = 0;     /* 0x80270844 */

or whose name is its address (`static int D_80316220;`).

In the arena memory model that variable lives at 0x80270844 like every
other piece of game state (and its initial value comes from the ROM), so
the declaration becomes `extern unsigned char D_80270844;` and the name is
replaced within the function.

    staticlift.py FILE...
"""
import re
import sys

DECL = re.compile(r'^(\s+)static\s+([^=;(]*?)\s*\b(\w+)\s*((?:\[[^\]]*\])*)\s*(?:=\s*([^;]*))?;(.*?/\*.*?\b0x(80[0-9A-Fa-f]{6})\b.*)$')
# or the address is the name: `static int D_80316220;`, `static int x8031B1D0;`
NAMED = re.compile(r'^(\s+)static\s+([^=;(]*?)\s*\b((?:D_|x)(80[0-9A-Fa-f]{6}))\s*((?:\[[^\]]*\])*)\s*(?:=\s*([^;]*))?;(.*)$')


def func_end(src, start):
    """the closing brace of the body opening at start (strings, character
    constants and comments skipped)"""
    depth = 0
    i = start
    n = len(src)
    while i < n:
        c = src[i]
        if c == '/' and src.startswith('/*', i):
            i = src.find('*/', i + 2) + 2
            continue
        if c == '/' and src.startswith('//', i):
            i = src.find('\n', i)
            continue
        if c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == '\\' else 1
            i = j + 1
            continue
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return n


def lift_one(src):
    """the first eligible static, lifted; None when there is none"""
    lines = src.split('\n')
    offs = [0]
    for l in lines:
        offs.append(offs[-1] + len(l) + 1)
    for i, l in enumerate(lines):
        m = DECL.match(l)
        if m:
            ind, ty, name, dims, init, rest, addr = m.groups()
        else:
            m = NAMED.match(l)
            if not m:
                continue
            ind, ty, name, addr, dims, init, rest = m.groups()
        a = int(addr, 16)
        if not (0x8026FAB0 <= a < 0x80382BB0):
            continue
        sym = 'D_%08X' % a
        k = i
        while k > 0 and not lines[k].startswith('{'):
            k -= 1
        body_end = func_end(src, offs[k])
        line_start, line_end = offs[i], offs[i] + len(l)
        body = re.sub(r'(?<![.>\w])%s\b' % re.escape(name), sym, src[line_end:body_end])
        return src[:line_start] + '%sextern %s %s%s;%s' % (ind, ty, sym, dims, rest) + body + src[body_end:]
    return None


def main():
    for path in sys.argv[1:]:
        src = open(path).read()
        n = 0
        while True:
            nxt = lift_one(src)
            if nxt is None:
                break
            src = nxt
            n += 1
        open(path, 'w').write(src)
        if n:
            print('%s: %d statics' % (path, n))


if __name__ == '__main__':
    main()
