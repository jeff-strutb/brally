#!/usr/bin/env python3
"""Sweep the DECLARATION ORDER of a function's locals -- a real VC5 codegen axis.

    .venv/bin/python tools/declsweep.py <base.c> <VA> <Symbol> <tagprefix> \
        [--mode end|front|all] [--start-marker TEXT]

WHY THIS EXISTS.  "Declaration order is inert" was measured on /O2 SLOT PACKING
(where it is true) and then quoted as a general fact for months.  It is FALSE
for tie-breaks, and two different mechanisms were proven byte-for-byte on
2026-09-04/05:

  * the symbol's absolute INDEX decides an x87 completion-order tie-break
    between comparable float products read through pointer locals -- on
    0x1000EAF0 the four row pointers declared in field order took masked
    divergence regions 21 -> 18 (24 permutations: exactly two outcomes);
  * for a product of two NAMED locals, the LATER-declared symbol becomes the
    `imul` DESTINATION -- on 0x100250D0 that decided whether a channel product
    survived the divide-by-255 `imul` or needed a spill slot, 31 -> 28 regions.

So when a schedule or an allocation is one notch off and every expression form
in the dossier is dead, sweep this axis before writing T3a.  It is cheap: each
probe is one compile, ~6 s.

`--mode end` moves each declaration to the end of its run, `front` to the
front, `all` to every position in the run (that is the exhaustive form; on a
40-local function it is a few hundred compiles).  Anything whose (masked, raw,
bytes, insns) differs from the base is flagged DIFFERS with its region map, and
the log lands next to the script.

MEASURED INERT, so do not expect these to move: the declaration order of extern
globals, unused extra locals (a local with no uses never gets an index),
renames, and slot packing itself.  MEASURED DEAD on a whole function:
0x1000A110, ~300 compiles -- the lever needs comparable float products through
pointer locals, or two named factors of an integer product, and a byte-lane
spill residue has neither.

Scores each variant with tools/probe.py, so read that file's header for what
the numbers mean and which of them lie on their own.
"""
import re, subprocess, os, sys, argparse
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = os.path.join(ROOT, 'build', 'match', 't3d')
os.makedirs(S, exist_ok=True)
ap = argparse.ArgumentParser(); ap.add_argument('base'); ap.add_argument('va'); ap.add_argument('sym'); ap.add_argument('prefix')
ap.add_argument('--mode', default='end'); ap.add_argument('--start-marker', default=None)
a = ap.parse_args()
base = open(a.base).read()
marker = a.start_marker or (a.sym + '(')
body_at = base.index(marker)
lines = base.splitlines(keepends=True)
decl_re = re.compile(r'^(\s+)(int|float|double|unsigned|uint32_t|uint16_t|uint8_t|int32_t|int16_t|int8_t|char|short|long|BrWheelRec|BrMat4|BrVec3|const)\b[^;=(]*?\b[A-Za-z_]\w*(\[[^\]]*\])?(\s*=\s*[^;]+)?;\s*(/\*.*\*/)?\s*$')
start_line = base[:body_at].count('\n')
cands = [i for i in range(start_line, len(lines)) if decl_re.match(lines[i]) and ',' not in lines[i].split('=')[0]]
def measure(tag, txt):
    p = S + '/' + tag + '.c'; open(p, 'w').write(txt)
    r = subprocess.run([ROOT + '/.venv/bin/python', os.path.join(ROOT, 'tools', 'probe.py'), p, tag, '--no-mset', '--va', a.va, '--sym', a.sym],
                       capture_output=True, text=True).stdout
    b = re.search(r'BYTES orig=\d+ recomp=(\d+).*?INSNS orig=\d+ recomp=(\d+)', r)
    m = re.search(r'MASKED.*?total divergence regions from offset 0x0: (\d+)', r)
    raw = re.search(r'RAW.*?total divergence regions from offset 0x0: (\d+)', r)
    regs = [l.split()[3] for l in r.splitlines() if l.strip().startswith('region')]
    os.unlink(p)
    if not (b and m and raw): return ('FAIL', r[-800:], '', '', '')
    return (m.group(1), raw.group(1), b.group(1), b.group(2), ' '.join(regs))
out = open(S + '/' + a.prefix + '.log', 'w')
ref = measure(a.prefix + '_ref', base); print('ref', ref[:4]); out.write('ref %s\n' % (ref[:4],)); out.flush()
def run_of(i):
    k = i
    while k - 1 > start_line and decl_re.match(lines[k - 1]): k -= 1
    j = i
    while j + 1 < len(lines) and decl_re.match(lines[j + 1]): j += 1
    return k, j
n = 0
for i in cands:
    k, j = run_of(i)
    if k == j: continue
    positions = []
    if a.mode == 'all':
        positions = [q for q in range(k, j + 1) if q != i]
    else:
        q = j if a.mode == 'end' else k
        if q == i: q = k if a.mode == 'end' else j
        positions = [q]
    for q in positions:
        run = lines[k:j + 1]; item = lines[i]
        rest = [l for idx, l in enumerate(run) if idx != i - k]
        rest.insert(q - k, item)
        new = lines[:k] + rest + lines[j + 1:]
        tag = '%s_%d_%d' % (a.prefix, i, q)
        res = measure(tag, ''.join(new)); n += 1
        diff = '' if res[:4] == ref[:4] else '  <-- DIFFERS'
        line = 'line %4d -> pos %4d  %-55s masked=%s raw=%s bytes=%s insns=%s%s' % (i + 1, q + 1, lines[i].strip()[:55], res[0], res[1], res[2], res[3], diff)
        if diff: line += '\n      regions: ' + res[4]
        print(line); out.write(line + '\n'); out.flush()
print('done', n)
