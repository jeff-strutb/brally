#!/usr/bin/env python3
"""Drop arguments a call passes beyond what the function's definition takes.

Several decompiled call sites pass arguments the callee never declares (the
original's stack convention let the callee ignore them). With the true
prototype visible, clang reports `too many arguments to function call,
expected M, have N`; the surplus arguments are removed -- the callee never
read them. Too FEW arguments is reported, not touched.

Usage: fixargs.py [FILE...]   (default: the failing files of the last build)
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402

ROOT = rawscan.ROOT


def split_args(s, i):
    """s[i] == '(' -> list of (start, end) of each argument, index after ')'."""
    depth, start, out, j = 0, i + 1, [], i
    while j < len(s):
        c = s[j]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                if s[start:j].strip():
                    out.append((start, j))
                return out, j
        elif c == ',' and depth == 1:
            out.append((start, j))
            start = j + 1
        elif c == '"' or c == "'":
            q = c
            j += 1
            while j < len(s) and s[j] != q:
                j += 2 if s[j] == '\\' else 1
        j += 1
    return None, i


def main():
    os.chdir(ROOT)
    files = sys.argv[1:] or [l.split(None, 1)[1].strip() for l in open('build/portable/compile.txt')
                             if l.startswith('FAIL ')]
    total, few = 0, 0
    for f in files:
        lang = ['-x', 'c++', '-std=c++98'] if f.endswith('.cpp') else ['-x', 'c', '-std=gnu89']
        p = subprocess.run(['clang'] + rawscan.flags_for(f) + ['-ferror-limit=0'] + lang + [f],
                           capture_output=True, text=True, errors='replace', cwd=ROOT)
        text = open(f, encoding='latin-1').read()
        lines = text.split('\n')
        starts = [0]
        for l in lines:
            starts.append(starts[-1] + len(l) + 1)
        edits = []
        for m in re.finditer(r'^%s:(\d+):(\d+): error: too many arguments to function call, expected '
                             r'(?:single argument \'\w+\'|(\d+)), have (\d+)' % re.escape(f), p.stderr, re.M):
            ln, col = int(m.group(1)), int(m.group(2))
            want = int(m.group(3)) if m.group(3) is not None else 1
            pos = starts[ln - 1] + col - 1          # clang points at the first surplus argument
            # find the call's '(' : walk back to the unmatched '('
            d, k = 0, pos
            while k > 0:
                k -= 1
                if text[k] == ')':
                    d += 1
                elif text[k] == '(':
                    if d == 0:
                        break
                    d -= 1
            args, end = split_args(text, k)
            if args is None or len(args) <= want:
                continue
            cut_from = args[want - 1][1] if want > 0 else k + 1
            edits.append((cut_from, end))
        few += len(re.findall(r'^%s:\d+:\d+: error: too few arguments' % re.escape(f), p.stderr, re.M))
        for a, z in sorted(set(edits), reverse=True):
            text = text[:a] + text[z:]
        if edits:
            open(f, 'w', encoding='latin-1').write(text)
            total += len(set(edits))
    print('fixargs: %d calls trimmed; %d calls pass too few (left for a person)' % (total, few))


if __name__ == '__main__':
    main()
