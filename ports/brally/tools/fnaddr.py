#!/usr/bin/env python3
"""Replace original code addresses written as integers with the function.

Decompiled code sometimes stores a function as its original address
(`g_pfn = 0x10023b70;`). For every literal in the original code range that
clang reports meeting a pointer type, the literal becomes the function placed
at that address (build/wasm/placement.csv), cast to the type the code expects.

Usage: fnaddr.py [FILE...]   (default: the failing files of the last build)
"""
import csv
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402

ROOT = rawscan.ROOT
TEXT_LO, TEXT_HI = 0x10001000, 0x10077000


def main():
    os.chdir(ROOT)
    place = {}
    for r in csv.DictReader(open('build/wasm/placement.csv')):
        place[int(r['va'], 16)] = r['name']
    files = sys.argv[1:] or [l.split(None, 1)[1].strip() for l in open('build/portable/compile.txt')
                             if l.startswith('FAIL ')]
    total, missing = 0, set()
    for f in files:
        lang = ['-x', 'c++', '-std=c++98'] if f.endswith('.cpp') else ['-x', 'c', '-std=gnu89']
        p = subprocess.run(['clang'] + rawscan.flags_for(f) + ['-ferror-limit=0', '-fdiagnostics-print-source-range-info']
                           + lang + [f], capture_output=True, text=True, errors='replace', cwd=ROOT)
        text = open(f, encoding='latin-1').read()
        lines = text.split('\n')
        starts = [0]
        for l in lines:
            starts.append(starts[-1] + len(l) + 1)
        edits = []
        for m in re.finditer(r"^%s:(\d+):(\d+):(\{[^\n]*?\})?: error: incompatible integer to pointer conversion "
                             r"(?:assigning to|passing|initializing|returning) '([^']+)'(?: \(aka '([^']+)'\))?"
                             % re.escape(f), p.stderr, re.M):
            target = m.group(4)
            rng = re.findall(r'\{(\d+):(\d+)-(\d+):(\d+)\}', m.group(3) or '')
            if not rng:
                continue
            for l1, c1, l2, c2 in rng:
                a = starts[int(l1) - 1] + int(c1) - 1
                z = starts[int(l2) - 1] + int(c2) - 1
                seg = text[a:z]
                lm = re.fullmatch(r'\s*\(?\s*(?:\(\s*\w+\s*\)\s*)?(0x[0-9a-fA-F]+)[uUlL]*\s*\)?\s*', seg)
                if not lm:
                    continue
                va = int(lm.group(1), 16)
                if not (TEXT_LO <= va < TEXT_HI):
                    continue
                fn = place.get(va)
                if not fn:
                    missing.add(hex(va))
                    continue
                # assigning: the literal is the right-hand range; passing: argument
                edits.append((a, z, '((%s)%s)' % (target, fn)))
        for a, z, t in sorted(set(edits), reverse=True):
            text = text[:a] + t + text[z:]
        if edits:
            open(f, 'w', encoding='latin-1').write(text)
            total += len(set(edits))
            print('%s: %d' % (f, len(set(edits))))
    print('fnaddr: %d literals -> functions; no function placed at: %s' % (total, sorted(missing)))


if __name__ == '__main__':
    main()
