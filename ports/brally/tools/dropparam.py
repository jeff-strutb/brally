#!/usr/bin/env python3
"""Drop trailing parameters a definition declares but never uses (the
original takes fewer), from the definition, its br_funcs.h prototype and
every call that passes them.

Usage: dropparam.py NAME=KEEP...   (KEEP = how many leading params stay)
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dropedx import files, split_args  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))


def main():
    want = dict((a.split('=')[0], int(a.split('=')[1])) for a in sys.argv[1:])
    for f in files():
        s = open(f, encoding='latin-1').read()
        t = s
        for name, keep in want.items():
            out, pos = [], 0
            for m in re.finditer(r'\b%s\s*\(' % re.escape(name), t):
                if m.start() < pos:
                    continue
                try:
                    spans, close = split_args(t, m.end() - 1)
                except ValueError:
                    continue
                if len(spans) <= keep:
                    continue
                if keep == 0:
                    after = t[close + 1:close + 40]
                    is_decl = re.match(r'\s*[;{]', after) and re.search(r'\w\s*$', t[max(0, m.start() - 40):m.start()])
                    repl = 'void' if is_decl else ''
                    out.append(t[pos:m.end()] + repl)
                else:
                    out.append(t[pos:spans[keep - 1][1]])
                pos = close
            out.append(t[pos:])
            t = ''.join(out)
        if t != s:
            open(f, 'w', encoding='latin-1').write(t)
            print(os.path.relpath(f, ROOT))


if __name__ == '__main__':
    main()
