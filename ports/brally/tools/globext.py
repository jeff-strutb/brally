#!/usr/bin/env python3
"""Give every global the 64-bit core defines at least its original extent.

The original's globals sit in one image: code that treats a global as the
start of a buffer, a table or a record reaches the bytes after it, up to the
next address anything names. In the 64-bit core each global is its own
object, so a global declared as an `int` but used as a 0x400-byte path
buffer overruns into whatever the linker put next.

For every global defined in src/brally/core/data/br_globals.c with its original
address, this measures:

    size32   sizeof at i686 (the original layout of the declared type)
    extent   from its address to the next address the core defines

and rewrites each definition whose extent is larger into storage that keeps
the declared type at offset 0 and pads to the extent (64-bit size of the
type plus extent - size32), under the same linker symbol:

    BR_GLOBAL_EXTENT(int, g_navArg, , 0x3FC);   /* 0x10B72F48 */

Globals whose size32 is LARGER than their extent overlap the next named
global: the original reads one object through two names. Those are listed
in build/brally/null-soft/overlaps.txt; each needs its second name folded into the
first by hand.

Usage: globext.py [--dry]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
GLOBALS_C = 'ports/brally/src/core/data/br_globals.c'
DEF = re.compile(r'^(?!\s)(?!#)(?!typedef)(?!extern)(?!static)(?P<decl>[^;{}()\n=]*?(?:\(\s*\*\s*(?P<fn>\w+)\s*(?:\[[^\]]*\])*\s*\)[^;{}=\n]*|\b(?P<name>\w+)\s*(?P<dims>(?:\[[^\]]*\])*)))\s*;\s*/\*\s*0x(?P<va>[0-9A-Fa-f]{8})\b[^\n]*$', re.M)
PADDED = re.compile(r'^BR_GLOBAL_EXTENT\((?P<type>.*?), (?P<name>\w+), (?P<dims>[^,]*), (?P<pad>0x[0-9A-F]+)\);\s*/\*\s*0x(?P<va>[0-9A-Fa-f]{8})', re.M)


def defs(text):
    out = []
    for m in DEF.finditer(text):
        name = m.group('fn') or m.group('name')
        if not name or name in ('const', 'volatile'):
            continue
        out.append((int(m.group('va'), 16), name, m))
    return out


def sizes32(names, open_arrays=(), exprs=None):
    """i686 sizeof of each name; for an array defined with an empty first
    dimension, the size of one element. `exprs` maps a name to the operand
    to measure instead (a padded global's declared type)."""
    exprs = exprs or {}
    src = '#include "%s"\n' % GLOBALS_C
    src += ''.join('const unsigned int __sz_%s = sizeof(%s);\n'
                   % (n, exprs.get(n) or n + ('[0]' if n in open_arrays else ''))
                   for n in names)
    p = subprocess.run(['clang', '-target', 'i386-apple-macos10.13', '-I.'] + vm.FLAGS +
                       ['-x', 'c', '-std=gnu89', '-S', '-o', '-', '-'],
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
    here = defs(text)
    # every address the core defines anywhere bounds the one before it
    vas = set(va for va, _, _ in here)
    vas |= set(int(m.group('va'), 16) for m in PADDED.finditer(text))
    # blocks of neighbouring globals (globfold.py) and the names in them
    vas |= set(int(m.group('first'), 16) for m in re.finditer(r'^BR_BLOCK\((?P<first>[0-9A-F]{8}),', text, re.M))
    vas |= set(int(m.group(1), 16) for m in re.finditer(r'^BR_BLOCK_AT\([^\n]*/\*\s*0x([0-9A-Fa-f]{8})', text, re.M))
    for dp, _, fs in os.walk('ports/brally/src/core'):
        for f in fs:
            if f.endswith(('.c', '.cpp')) and os.path.join(dp, f) != GLOBALS_C:
                vas |= set(va for va, _, _ in defs(open(os.path.join(dp, f), encoding='latin-1').read()))
    vas = sorted(v for v in vas if v >= 0x10000000)
    nxt = {vas[i]: vas[i + 1] for i in range(len(vas) - 1)}
    open_arrays = set(n for _, n, m in here if (m.group('dims') or '').startswith('[]'))
    sz = sizes32([n for _, n, _ in here], open_arrays)
    edits, over = [], []
    for va, name, m in here:
        if va not in nxt or name not in sz:
            continue
        ext = nxt[va] - va
        s = sz[name]
        if name in open_arrays:
            # `T a[][K];` holds one element; size it to the extent instead
            dims = '[%d]%s' % (max(1, ext // s), m.group('dims')[2:])
            edits.append((m.start('dims'), m.end('dims'), dims))
            continue
        if s > ext:
            over.append('0x%08X %-32s size32 0x%X > extent 0x%X (next 0x%08X)' % (va, name, s, ext, nxt[va]))
        elif s < ext:
            if m.group('fn'):
                continue        # a function pointer slot: nothing walks past it
            decl = m.group('decl')
            ty = decl[:m.start('name') - m.start('decl')].strip()
            edits.append((m.start(), m.end(), 'BR_GLOBAL_EXTENT(%s, %s, %s, 0x%X);  /* 0x%08X */'
                          % (ty, name, m.group('dims'), ext - s, va)))
    # already padded: the pad follows the extent when a neighbour went away
    padded = [m for m in PADDED.finditer(text) if not m.group('dims').startswith('[]')]
    psz = sizes32([m.group('name') for m in padded],
                  exprs={m.group('name'): '%s%s' % (m.group('type'), m.group('dims')) for m in padded})
    for m in padded:
        va, name = int(m.group('va'), 16), m.group('name')
        if va not in nxt or name not in psz:
            continue
        want = nxt[va] - va - psz[name]
        if want > 0 and want != int(m.group('pad'), 16):
            edits.append((m.start('pad'), m.end('pad'), '0x%X' % want))
    os.makedirs('build/brally/null-soft', exist_ok=True)
    open('build/brally/null-soft/overlaps.txt', 'w').write('\n'.join(over) + '\n')
    print('padded %d, overlapping %d (build/brally/null-soft/overlaps.txt)' % (len(edits), len(over)))
    if dry or not edits:
        return
    for b, e, rep in sorted(edits, reverse=True):
        text = text[:b] + rep + text[e:]
    open(GLOBALS_C, 'w').write(text)


if __name__ == '__main__':
    main()
