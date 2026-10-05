#!/usr/bin/env python3
"""bestruct.py -- make a TU's view of a cartridge-data struct big-endian.

    bestruct.py FILE STRUCT [STRUCT ...]

Inside each named typedef'd struct in FILE: TgrAddr and 4-byte integers
become be32_t, floats bef_t, shorts be16_t; bytes and arrays of bytes stay.
Uses then fail to compile until they go through the accessors (errfix.py
does the routine ones).
"""
import re
import sys

MAP = [(r'\bTgrAddr\b', 'be32_t'), (r'\bunsigned int\b', 'be32_t'), (r'\bsigned int\b', 'be32_t'),
       (r'\bint\b', 'be32_t'), (r'\bunsigned short\b', 'be16_t'), (r'\bsigned short\b', 'be16_t'),
       (r'\bshort\b', 'be16_t'), (r'\bfloat\b', 'bef_t'), (r'\bBrVec3\b', 'BrVec3be')]


def main():
    path = sys.argv[1]
    s = open(path).read()
    for name in sys.argv[2:]:
        i = s.index('typedef struct ' + name)
        j = s.index('} ' + name + ';', i)
        body = s[i:j]
        lines = body.split('\n')
        out = [lines[0]]
        for l in lines[1:]:
            code, sep, cmt = l.partition('/*')
            for a, b in MAP:
                code = re.sub(a, b, code)
            out.append(code + sep + cmt)
        s = s[:i] + '\n'.join(out) + s[j:]
    open(path, 'w').write(s)


if __name__ == '__main__':
    main()
