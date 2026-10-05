#!/usr/bin/env python3
"""fnedit.py -- replace function definitions in a core file.

    fnedit.py FILE < defs.c

defs.c holds complete function definitions; each replaces the definition
of the same name in FILE (from its return type's line to the closing brace
in column 0), leaving the comments above it alone.
"""
import re
import sys


def defs_of(text):
    out = []
    for m in re.finditer(r'^([A-Za-z_][^\n;{}()]*?\b(\w+)\s*\([^;{]*?\)\s*)\{', text, re.M | re.S):
        start = m.start()
        depth = 0
        i = m.end() - 1
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        out.append((m.group(2), start, i + 1))
    return out


def main():
    path = sys.argv[1]
    src = open(path).read()
    new = sys.stdin.read()
    for name, s, e in defs_of(new):
        body = new[s:e]
        found = [d for d in defs_of(src) if d[0] == name]
        if len(found) != 1:
            sys.exit('%s: %d definitions of %s' % (path, len(found), name))
        _, ss, ee = found[0]
        src = src[:ss] + body + src[ee:]
    open(path, 'w').write(src)


if __name__ == '__main__':
    main()
