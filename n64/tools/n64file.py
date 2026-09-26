"""File a graded candidate into its module: name it, describe it, re-verify.

    .venv/bin/python n64/tools/n64file.py 0x8020AD50 n64/src/menus/frontend.c \\
        BrFrontSetCursor "remember which menu row is highlighted"

Takes build/n64/cand/<VA>.c (written by n64gen.py), renames the function,
puts `WHAT IT DOES:` and `@implements` above it, merges its declarations into
the destination file's declaration block, and appends it.  Then the whole
destination file is rebuilt with n64build.py; if ANY function in it is no
longer EXACT the file is restored and the move refused -- surroundings decide
codegen, and a move that costs a neighbour is not a move (project rule 6).

The name goes into n64/config/symbols_tgr.csv so every other file resolves it.

A new destination file is created with a header line saying what the module
is (--module-doc).  Declarations live between the markers
`/* -- declarations -- */` and `/* -- end declarations -- */`.
"""
import argparse
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
sys.path.insert(0, os.path.join(N64, 'tools'))
import n64build as B  # noqa: E402

DB, DE = '/* -- declarations -- */', '/* -- end declarations -- */'


def wrap(text, width=76, lead=' * '):
    words, lines, cur = text.split(), [], ''
    for w in words:
        if len(cur) + len(w) + 1 > width - len(lead):
            lines.append(cur)
            cur = w
        else:
            cur = (cur + ' ' + w).strip()
    if cur:
        lines.append(cur)
    return lines


def comment(what):
    lines = wrap('WHAT IT DOES: ' + what)
    if len(lines) == 1:
        return '/* %s */' % lines[0]
    return '/* ' + lines[0] + '\n' + '\n'.join(' * ' + l for l in lines[1:]) + ' */'


def split_candidate(src, va):
    head, _, body = src.partition('/* @implements')
    body = body.split('*/', 1)[1].lstrip('\n')
    decls = [l for l in head.split('\n')
             if l.strip() and not l.startswith('#include')]
    return decls, body


def add_symbol(name, va):
    rows = []
    if os.path.exists(B.SYMS):
        rows = list(csv.DictReader(open(B.SYMS)))
    rows = [r for r in rows if r['name'] != name and int(r['va'], 16) != va]
    rows.append(dict(va='%08X' % va, name=name, kind='func'))
    rows.sort(key=lambda r: r['va'])
    with open(B.SYMS, 'w', newline='') as f:
        w = csv.DictWriter(f, ['va', 'name', 'kind'], lineterminator='\n')
        w.writeheader()
        w.writerows(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va')
    ap.add_argument('dest')
    ap.add_argument('name')
    ap.add_argument('what')
    ap.add_argument('--module-doc', default='')
    ap.add_argument('--src', help='candidate file (default build/n64/cand/<VA>.c)')
    ap.add_argument('--allow-diff', action='store_true',
                    help='file a body that is not byte-exact (a T2, on its way to T3); '
                         'every other function in the file must stay as it was')
    a = ap.parse_args()
    va = int(a.va, 16)
    cand = a.src or os.path.join(B.OUT, 'cand', '0x%08X.c' % va)
    src = open(cand).read()
    decls, body = split_candidate(src, va)
    body = re.sub(r'\bfunc_%08X\b' % va, a.name, body)
    body = re.sub(r'\n\s*return;\n}', '\n}', body)        # Ghidra's trailing return
    body = re.sub(r'/\* WARNING:[^*]*\*/\n', '', body)
    body = re.sub(r"\)\n\n{", ")\n{", body)
    body = re.sub(r"{\n(\s*\n)+", "{\n", body)
    dest = os.path.abspath(a.dest)
    existed = os.path.exists(dest)
    old = open(dest).read() if existed else None
    base = old
    if base is None:
        mod = os.path.basename(dest)
        doc = a.module_doc or 'Top Gear Rally (N64).'
        base = '/* %s -- %s\n */\n#include "tgr/common.h"\n\n%s\n%s\n' % (mod, doc, DB, DE)
    pre, _, rest = base.partition(DB)
    have, _, post = rest.partition(DE)
    lines = [l for l in have.split('\n') if l.strip()]
    ext = re.compile(r'^extern\s+(.+?)\s*\b(D_[0-9A-F]{8}|\w+)\s*;$')
    known = {}
    for l in lines:
        m = ext.match(l.strip())
        if m:
            known[m.group(2)] = m.group(1).strip()
    for d in decls:
        m = ext.match(d.strip())
        if m and m.group(2) in known and known[m.group(2)] != m.group(1).strip():
            # same variable, declared with another type in this file: keep the
            # file's declaration and cast this body's uses (a cast between
            # 4-byte integer and pointer types costs no code)
            sym, want, old = m.group(2), m.group(1).strip(), known[m.group(2)]
            four = lambda t: t.endswith('*') or t in ('int', 'unsigned int', 'u32', 's32')
            if not (four(want) and four(old)):
                print('REFUSED: %s is declared %s here and %s in this body' % (sym, old, want))
                return 1
            body = re.sub(r'(?<![\w&])\b%s\b(?!\s*=[^=])' % sym, '((%s)%s)' % (want, sym), body)
            continue
        if d not in lines:
            lines.append(d)
    new = (pre + DB + '\n' + '\n'.join(lines) + '\n' + DE + post.rstrip('\n') + '\n\n'
           + comment(a.what) + '\n' + '/* @implements 0x%08X tgr %s */\n' % (va, a.name)
           + body.strip('\n') + '\n')
    exact_before = set()
    vpath = os.path.join(B.OUT, 'verify.csv')
    if os.path.exists(vpath):
        exact_before = {r['va'] for r in csv.DictReader(open(vpath)) if r['status'] == 'EXACT'}
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    open(dest, 'w').write(new)
    syms_before = open(B.SYMS).read() if os.path.exists(B.SYMS) else None
    add_symbol(a.name, va)
    p = subprocess.run([sys.executable, os.path.join(N64, 'tools/n64build.py'), dest],
                       cwd=ROOT, capture_output=True, text=True)
    rows = [l.split() for l in p.stdout.split('\n') if re.match(r'^[0-9A-F]{8} ', l) or l.startswith(' ')]
    bad = [r for r in rows if len(r) > 1 and r[1] != 'EXACT'
           and not (a.allow_diff and r[0] == '%08X' % va and r[1] == 'DIFF')]
    if a.allow_diff:
        # the new body may differ; a neighbour that was exact may not drop
        bad = [r for r in rows if len(r) > 1 and r[0] != '%08X' % va and r[1] != 'EXACT'
               and r[0] in exact_before]
        if not any(r[0] == '%08X' % va and r[1] in ('EXACT', 'DIFF') for r in rows):
            bad.append(['%08X' % va, 'MISSING'])
    if p.returncode or bad or not rows:
        if existed:
            open(dest, 'w').write(old)
            subprocess.run([sys.executable, os.path.join(N64, 'tools/n64build.py'), '-q', dest],
                           cwd=ROOT, capture_output=True)
        else:
            os.unlink(dest)
        if syms_before is not None:
            open(B.SYMS, 'w').write(syms_before)
        print('REFUSED %08X -> %s' % (va, os.path.relpath(dest, ROOT)))
        print(p.stdout[-2000:])
        return 1
    print('FILED %08X %s -> %s' % (va, a.name, os.path.relpath(dest, ROOT)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
