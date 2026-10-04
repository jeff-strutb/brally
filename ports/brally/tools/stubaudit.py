#!/usr/bin/env python3
"""Find and restore code that funcs.py's stub removal wrongly deleted.

funcs.py replaced `NAME(...) { ... }` with `/* NAME: the original function is
CANON */` wherever a line matched, which also hit `if (NAME(...)) {` blocks
INSIDE functions. A marker whose enclosing brace is a function body (not an
extern "C" block) is such a deletion: the k-th marker for NAME in a file is
the k-th matching block of the original decompiled file (funcs.py removed the
first remaining match on each run), so the original block is put back, with
NAME spelled as in the original (the file's alias header maps it).

Usage: stubaudit.py [--fix]
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def scrub(t):
    t = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), t, flags=re.S)
    t = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), t)
    t = re.sub(r'"(\\.|[^"\\\n])*"', lambda m: '"' + ' ' * (len(m.group(0)) - 2) + '"', t)
    t = re.sub(r"'(\\.|[^'\\\n])*'", lambda m: "'" + ' ' * (len(m.group(0)) - 2) + "'", t)
    return t


def enclosing(s, pos):
    """text before the innermost unmatched '{' before pos (scrubbed)."""
    c = scrub(s[:pos])
    d = 0
    for i in range(len(c) - 1, -1, -1):
        if c[i] == '}':
            d += 1
        elif c[i] == '{':
            if d == 0:
                return s[max(0, i - 200):i]        # the real text: `extern "C"` survives
            d -= 1
    return None


def blocks(src, name):
    """[(start, end)] of every `line ... NAME(...) {` block in src, in order."""
    out = []
    for m in re.finditer(r'^[^\n;{}#]*\b%s\s*\([^;{}]*\)\s*\{' % re.escape(name), src, re.M):
        i, d = m.end() - 1, 0
        c = scrub(src)
        while i < len(src):
            if c[i] == '{':
                d += 1
            elif c[i] == '}':
                d -= 1
                if d == 0:
                    break
            i += 1
        out.append((m.start(), i + 1))
    return out


def main():
    os.chdir(ROOT)
    fix = '--fix' in sys.argv
    n = 0
    for f in sorted(glob.glob('ports/brally/src/core/**/*.c*', recursive=True)):
        s = open(f, encoding='latin-1').read()
        marks = list(re.finditer(r'/\* (\w+): the original function is (\w+) \*/', s))
        if not marks:
            continue
        orig_p = f[len('ports/brally/'):]
        orig = open(orig_p, encoding='latin-1').read() if os.path.exists(orig_p) else ''
        seen = {}
        edits = []
        for m in marks:
            name = m.group(1)
            k = seen.get(name, 0)
            seen[name] = k + 1
            enc = enclosing(s, m.start())
            if enc is None or re.search(r'extern\s+"C"\s*$', enc.rstrip()):
                continue                    # file scope: a real stub removal
            ob = blocks(orig, name)
            if k >= len(ob):
                print('NO ORIGINAL BLOCK %s:%d %s (#%d)' % (f, s.count('\n', 0, m.start()) + 1, name, k))
                continue
            a, z = ob[k]
            print('restore %s:%d %s (#%d, %d bytes)' % (f, s.count('\n', 0, m.start()) + 1, name, k, z - a))
            edits.append((m.start(), m.end(), orig[a:z]))
            n += 1
        if fix and edits:
            for a, z, t in sorted(edits, reverse=True):
                s = s[:a] + t + s[z:]
            open(f, 'w', encoding='latin-1').write(s)
    print('deleted blocks inside functions: %d' % n)


if __name__ == '__main__':
    main()
