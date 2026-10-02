#!/usr/bin/env python3
"""COM-style vtable calls written with 32-bit byte offsets.

    (*(Fn *)(*(int *)(OBJ) + 0x30))(OBJ, ...)
 -> (*(Fn *)&((void **)*(void ***)(OBJ))[12])(OBJ, ...)

The original reads the object's vtable pointer as an int and adds the
slot's byte offset; at 64 bits the vtable pointer is eight bytes and so is
every slot, so the slot index (offset / 4) is what is kept.

Usage: vtcallfix.py [--dry] [FILE...]
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
HEAD = re.compile(r'\(\*\(([A-Za-z_][\w ]*?)\s*\*\)\s*\(\*\((?:int|unsigned int|uint32_t|DWORD) \*\)\s*')


def balanced(s, i):
    """s[i] starts an operand: a parenthesised expression or an identifier."""
    if s[i] == '(':
        d, j = 0, i
        while j < len(s):
            if s[j] == '(':
                d += 1
            elif s[j] == ')':
                d -= 1
                if d == 0:
                    return j + 1
            j += 1
        return None
    m = re.match(r'[A-Za-z_]\w*(?:\[[^\]]*\])?', s[i:])
    return i + m.end() if m else None


def fix(s):
    out, pos, n = [], 0, 0
    for m in HEAD.finditer(s):
        if m.start() < pos:
            continue
        if not re.match(r'^(CC_\w+|COM\w*|\w*Fn\w*|\w*Proc\w*|\w*Func\w*)$', m.group(1).strip()):
            continue                  # a data read, not a vtable slot call
        e = balanced(s, m.end())
        if e is None:
            continue
        tail = re.match(r'\s*\)?\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\s*\)\)', s[e:])
        if not tail:
            continue
        k = int(tail.group(1), 0)
        if k % 4:
            continue
        obj = s[m.end():e]
        out.append(s[pos:m.start()])
        out.append('(*(%s *)&((void **)*(void ***)(%s))[%d])' % (m.group(1), obj, k // 4))
        pos = e + tail.end()
        n += 1
    out.append(s[pos:])
    return ''.join(out), n


def main():
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not files:
        for dp, _, fs in os.walk('ports/64b/src/core'):
            files += [os.path.join(dp, f) for f in fs if f.endswith(('.c', '.cpp'))]
    total = 0
    for f in files:
        s = open(f, encoding='latin-1').read()
        t, n = fix(s)
        if n:
            total += n
            print('%s: %d' % (f, n))
            if not dry:
                open(f, 'w', encoding='latin-1').write(t)
    print('vtable calls rewritten: %d' % total)


if __name__ == '__main__':
    main()
