"""Sweep a function's position within its own TU and report the best slot.

Where a function sits in its translation unit decides its register allocation,
and that is a lever no amount of re-spelling reaches.  0x1001DD00 BrVec3dCross
goes from REGNORM 8+8 to 0+0 at the head of br_vecd.c and nowhere else; on
0x1006D530 BrRbQuatDerivative a neighbour edit had silently moved it and cost
142 differing bytes, which nothing but a re-measure would have found.

It pays on a minority of rows -- about three in twenty-five when this was
written -- so it is a sweep, not a hypothesis: lift the function (its leading
comment block through the closing brace at column 0), reinsert it ahead of
every other top-level definition in turn, and score each arrangement with
`fn.py --var`.  Slots where the function would land above a typedef it needs
do not compile and are reported as skipped, not scored.

This VERIFIES NOTHING.  Moving a function perturbs its neighbours too, so a
move is only worth keeping after `match_sweep.py` on the whole file shows no
row went backwards.

    .venv/bin/python tools/possweep.py 0x1001DD00 [0xVA ...]
"""
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY = os.path.join(ROOT, '.venv/bin/python')

DEF = re.compile(r'^[A-Za-z_][A-Za-z0-9_ \t\*]*\**[A-Za-z_][A-Za-z0-9_]*\s*\([^;]*$', re.M)


def rows():
    out = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'build/match/report.csv'))):
        out[r['va'].lower()] = r
    return out


def lift(src, va, name):
    """Return (block, rest) or None."""
    m = re.search(r'^/\* @implements %s\b.*$' % va, src, re.M | re.I)
    if not m:
        m = re.search(r'^\w[^\n]*\b%s\s*\(' % re.escape(name), src, re.M)
        if not m:
            return None
    # walk back over contiguous comment / attribute lines to the blank line
    start = m.start()
    while start > 0:
        prev = src.rfind('\n', 0, start - 1) + 1
        t = src[prev:start - 1].lstrip()
        if t.startswith('/*') or t.startswith('*') or t.endswith('*/'):
            start = prev
            continue
        break
    # forward to the closing brace at column 0 after the definition
    body = src.find('\n{\n', m.start())
    if body < 0:
        return None
    end = src.find('\n}\n', body)
    if end < 0:
        return None
    end += 3
    return src[start:end], src[:start] + src[end:]


def slots(rest):
    """Insertion offsets: ahead of each top-level definition's comment block."""
    out = []
    for m in DEF.finditer(rest):
        j = rest.rfind('\n\n', 0, m.start())
        out.append((m.group(0).strip()[:44], j + 2 if j > 0 else m.start()))
    seen, uniq = set(), []
    for label, off in out:
        if off not in seen:
            seen.add(off)
            uniq.append((label, off))
    return uniq


def measure(va, tag):
    r = subprocess.run([PY, 'tools/fnmatch/fn.py', va, '--var', tag],
                       cwd=ROOT, capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if 'DIFFS' in line:
            return line.strip()
    return None


def run(va):
    r = rows().get(va.lower())
    if not r:
        print('%s: no report row' % va)
        return
    path = os.path.join(ROOT, r['file'])
    src = open(path).read()
    got = lift(src, va, r['name'])
    if not got:
        print('%s %s: cannot lift from %s' % (va, r['name'], r['file']))
        return
    blk, rest = got
    tag = 'pos'
    subprocess.run([PY, 'tools/fnmatch/fn.py', va, '--make', tag],
                   cwd=ROOT, capture_output=True, text=True)
    vf = os.path.join(ROOT, 'build/match/t3d/fn_%s_%s.c' % (va, tag))
    print('=== %s %s  (%s, %s B)' % (va, r['name'], r['file'], r['orig_size']))
    open(vf, 'w').write(src)
    m = measure(va, tag)
    print('  %-46s %s' % ('[in place]', m or '(compile error)'))
    skipped = 0
    for label, off in slots(rest):
        open(vf, 'w').write(rest[:off] + blk + '\n' + rest[off:])
        m = measure(va, tag)
        if m is None:
            skipped += 1
            continue
        print('  before %-40s %s' % (label, m))
    open(vf, 'w').write(rest.rstrip() + '\n\n' + blk)
    m = measure(va, tag)
    print('  %-46s %s' % ('[end of TU]', m or '(compile error)'))
    if skipped:
        print('  (%d slot(s) would not compile there)' % skipped)
    open(vf, 'w').write(src)


if __name__ == '__main__':
    for a in sys.argv[1:]:
        run(a)
