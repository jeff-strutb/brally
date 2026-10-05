#!/usr/bin/env python3
"""Lay the original's neighbouring globals out as the original does.

The original's globals sit in one image, and code reaches from one into the
next: a table cleared as int[0x53] takes the session words after it with it
(DAT_10ac5a48), the AI corridor scan reads nine entries past one array and
writes the next one's first. In the 64-bit core every name had its own
object, so a write through one name never reached the neighbour the
original wrote. globext.py gave each object a private pad up to the next
address instead, which keeps the code in bounds but not in step.

This rewrites src/core/data/br_globals.c so that every run of neighbouring
globals whose layout is the same at i686 and in the core (no pointers) is one
block of storage holding the original's bytes, and each name in it is a
linker symbol at its original offset:

    BR_BLOCK(10AC5A48, 0x8, 0x170);  /* 0x10AC5A48 */
    BR_BLOCK_AT(10AC5A48, DAT_10ac5a48, 0x8);  /* 0x10AC5A48 */

The storage starts at the 64-byte boundary below the first global, so each
name keeps the original's alignment mod 64; br_blockdata_<first> names the
block's own bytes, from its first global on, for the data lift.

A run ends at a global whose layout differs (it holds pointers), at one
defined in another file, and before a gap of more than MAXGAP bytes no one
names. The data lift copies each block from the image.

Usage: globfold.py [--dry]      (writes build/portable/folds.txt)
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import globext  # noqa: E402
import viewmerge as vm  # noqa: E402

ROOT = globext.ROOT
GLOBALS_C = globext.GLOBALS_C
MAXGAP = 0x1000
# buffers the original uses well past their declared type: the frame's two
# display lists, 96000 bytes each from +0x200 (BrFrameBeginDl, BrFrameEnd)
EXTENT = {0x106B80A8: 0x200 + 2 * 96000}
# tables the core extends past the original's (the key table's port-only
# Toggle Remaster entry): their own storage, so the extra entry does not
# land on the original's next globals
OWN = {'g_aKeyEnt0AAAD0', 'g_brBindAAAD4'}
BLOCK = re.compile(r'^BR_BLOCK\((?P<first>[0-9A-F]{8}), (?P<head>0x[0-9A-F]+), (?P<len>0x[0-9A-F]+)\);', re.M)
BLOCK_AT = re.compile(r'^BR_BLOCK_AT\((?P<base>[0-9A-F]{8}), (?P<name>\w+), (?P<off>0x[0-9A-F]+)\);\s*/\*\s*0x(?P<va>[0-9A-Fa-f]{8})', re.M)


def sizes(exprs, host):
    """sizeof each expression, at i686 or in the core (host)"""
    src = '#include "%s"\n' % GLOBALS_C
    src += ''.join('const unsigned int __sz_%s = sizeof(%s);\n' % (n, e) for n, e in exprs.items())
    target = [] if host else ['-target', 'i386-apple-macos10.13']
    p = subprocess.run(['clang'] + target + ['-I.'] + vm.FLAGS + ['-x', 'c', '-std=gnu89', '-S', '-o', '-', '-'],
                       input=src, capture_output=True, text=True, cwd=ROOT)
    if p.returncode:
        sys.exit(p.stderr[:4000])
    got, cur = {}, None
    for line in p.stdout.splitlines():
        m = re.match(r'^_*__sz_(\w+):', line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r'^\s*\.long\s+(\d+)', line)
        if m and cur:
            got[cur] = int(m.group(1))
            cur = None
    return got


def main():
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    text = open(GLOBALS_C).read()
    if BLOCK_AT.search(text) or BLOCK.search(text):
        sys.exit('globfold: br_globals.c already has blocks')
    g = {}           # name -> (va, match, declared-type expression)
    for va, name, m in globext.defs(text):
        if m.group('fn') or (m.group('dims') or '').startswith('[]') or not name:
            continue
        g[name] = (va, m, name)
    for m in globext.PADDED.finditer(text):
        ty, dims = m.group('type'), m.group('dims')
        g[m.group('name')] = (int(m.group('va'), 16), m, ty + dims if dims else ty)
    exprs = {n: e for n, (_, _, e) in g.items()}
    s32, s64 = sizes(exprs, False), sizes(exprs, True)
    stops = {}       # addresses a run may not cross: name -> why
    for n in OWN & set(g):
        stops[g[n][0]] = '%s extended past the original here' % n
    for n in g:
        if n not in s32 or n not in s64:
            stops[g[n][0]] = '%s not measured' % n
        elif s32[n] != s64[n]:
            stops[g[n][0]] = '%s 0x%X at i686, 0x%X here' % (n, s32[n], s64[n])
    for va, name, m in globext.defs(text):
        if name not in g:
            stops.setdefault(va, '%s (function pointer or open array)' % name)
    for dp, _, fs in os.walk('ports/brally/src/core'):
        for f in fs:
            p = os.path.join(dp, f)
            if f.endswith(('.c', '.cpp')) and p != GLOBALS_C:
                for va, n, _ in globext.defs(open(p, encoding='latin-1').read()):
                    if va >= 0x10000000:
                        stops.setdefault(va, '%s defined in %s' % (n, p))
    # the data lift copies the image from its first data address: a block
    # must not start below it and run into it
    for va in (0x10077000,):
        stops.setdefault(va, 'start of the lifted data')
    allv = sorted(set([g[n][0] for n in g] + list(stops)))
    nxt = {allv[i]: allv[i + 1] for i in range(len(allv) - 1)}
    runs, cur, reach = [], [], 0
    for va in allv:
        names = [n for n in g if g[n][0] == va]
        ok = va not in stops and len(names) == 1
        if va in stops and cur and va < reach:
            continue        # inside a buffer the run already covers: stays its own object
        if ok and cur:
            last = cur[-1]
            near = max(g[last][0] + s32[last], nxt.get(g[last][0], va))
            if va - (g[last][0] + s32[last]) > MAXGAP and va > near and va >= reach:
                ok = False
        if ok:
            cur.append(names[0])
            reach = max(reach, va + max(s32[names[0]], EXTENT.get(va, 0)))
            continue
        if len(cur) > 1:
            runs.append(cur)
        cur = [names[0]] if va not in stops and len(names) == 1 else []
        reach = va + max(s32[names[0]], EXTENT.get(va, 0)) if cur else 0
    if len(cur) > 1:
        runs.append(cur)
    rep, edits = [], []
    for run in runs:
        first, last = g[run[0]][0], g[run[-1]][0]
        base = first & ~63
        end = max(max(g[n][0] + max(s32[n], EXTENT.get(g[n][0], 0)) for n in run),
                  nxt.get(last, last + s32[run[-1]]))
        rep.append('block 0x%08X..0x%08X %d names' % (base, end, len(run)))
        first_at = min(g[n][1].start() for n in run)
        edits.append((first_at, first_at, 'BR_BLOCK(%08X, 0x%X, 0x%X);  /* 0x%08X */\n'
                      % (first, first - base, end - first, first)))
        for n in run:
            m = g[n][1]
            semi = text.index(';', m.start())
            edits.append((m.start(), semi + 1, 'BR_BLOCK_AT(%08X, %s, 0x%X);' % (first, n, g[n][0] - base)))
    rep.insert(0, '%d blocks hold %d names; %d addresses stop a run' % (len(runs), sum(map(len, runs)), len(stops)))
    rep += ['stop 0x%08X %s' % (va, why) for va, why in sorted(stops.items())]
    os.makedirs('build/portable', exist_ok=True)
    open('build/portable/folds.txt', 'w').write('\n'.join(rep) + '\n')
    print(rep[0], '(build/portable/folds.txt)')
    if dry:
        return
    for s, e, r in sorted(edits, reverse=True):
        text = text[:s] + r + text[e:]
    open(GLOBALS_C, 'w').write(text)


if __name__ == '__main__':
    main()
