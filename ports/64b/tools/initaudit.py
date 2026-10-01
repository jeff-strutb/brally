#!/usr/bin/env python3
"""Initialised definitions the port lost.

Every file-scope definition WITH an initialiser in the original decompiled
files (`T name = ...;`, `static const float k = 0.65f;`) is the source's
statement of that object's value. unify.py moved definitions into
br_globals.c without their initialisers. For each such definition this
reports whether the fork still carries it, and whether its address is
verified (a Glide relocation names it: the data lift will supply the
original's bytes there) or not (the initialiser is the only truth).

Usage: initaudit.py
Output: build/portable/initaudit.csv
"""
import csv
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubaudit  # noqa: E402

ROOT = stubaudit.ROOT


def defs(src):
    c = stubaudit.scrub(src)
    out = []
    depth = 0
    i = 0
    starts = [0]
    for m in re.finditer(r'[{};]', c):
        ch = m.group(0)
        if ch == '{':
            pre = c[max(0, m.start() - 20):m.start()]
            depth += 0 if re.search(r'extern\s+"C"\s*$', pre) else 1
            if not re.search(r'extern\s+"C"\s*$', pre):
                pass
        elif ch == '}':
            depth = max(0, depth - 1)
    # simple pass: statements at file scope that contain '=' and end in ';'
    level = 0
    stmt_start = 0
    for k, ch in enumerate(c):
        if ch == '{':
            pre = c[max(0, k - 20):k]
            if re.search(r'extern\s+"C"\s*$', pre):
                stmt_start = k + 1
                continue
            if level == 0 and '=' in c[stmt_start:k]:
                level += 1          # an aggregate initialiser: stays in this statement
                continue
            level += 1
        elif ch == '}':
            level = max(0, level - 1)
            if level == 0:
                stmt_start = k + 1
        elif ch == ';' and level == 0:
            st = src[stmt_start:k]
            cs = c[stmt_start:k]
            stmt_start = k + 1
            if '=' not in cs or cs.strip().startswith(('#', 'typedef', 'extern')) or '(' in cs.split('=')[0]:
                continue
            m = re.match(r'\s*((?:static\s+|const\s+|volatile\s+)*)([\w\s\*]+?)\s*\b(\w+)\s*((?:\[[^\]]*\])*)\s*=', cs)
            if m:
                out.append((m.group(3), ' '.join(st.split())[:120]))
    return out


def main():
    os.chdir(ROOT)
    verified = {r['name'] for r in csv.DictReader(open('build/wasm/sites.csv'))}
    canon = {r['name'] for r in csv.DictReader(open('build/portable/canon.csv'))}
    rows = []
    for f in sorted(glob.glob('src/core/**/*.c*', recursive=True)):
        fork = 'ports/64b/' + f
        if not os.path.exists(fork):
            continue
        fs = open(fork, encoding='latin-1').read()
        for name, text in defs(open(f, encoding='latin-1').read()):
            kept = re.search(r'\b%s\s*(\[[^\]]*\])*\s*=' % re.escape(name), stubaudit.scrub(fs)) is not None
            if kept:
                continue
            rows.append((f, name, name in verified, name in canon, text))
    with open('build/portable/initaudit.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['file', 'name', 'address_verified', 'canonical', 'original'])
        w.writerows(rows)
    print('initialised definitions the fork lost: %d (%d at verified addresses, %d not)' % (
        len(rows), sum(1 for r in rows if r[2]), sum(1 for r in rows if not r[2])))


if __name__ == '__main__':
    main()
