#!/usr/bin/env python3
"""Undo Ghidra's counter-fold across a two-store budget-checked loop body.

Ghidra merges two consecutive `count += N` updates into one `count += 2N` and
rewrites the first budget test against the *pre* value, so a loop that the
original source wrote as

    count = count + N;
    *p = A;
    p = p + 1;
    if (count >= bound) { EXIT }
    count = count + N;
    *p = B;
    p = p + 1;
    if (count >= bound) { EXIT }

comes back out of the decompiler as

    *p = A;
    if (count + N >= bound) { EXIT }
    count = count + 2N;
    p[1] = B;
    p = p + 2;
    if (count >= bound) { EXIT }

The two forms are semantically identical (the counter value on the first exit
path is dead, because that path returns), but they compile very differently.
The folded form lets VC5 coalesce the pair of stores into a batch
(`mov [r]; mov [r+2]; add r,4`), which frees the output pointer's register and
rotates the allocation across the whole function.  The split form makes VC5
emit the original's `mov [esi], r; add esi, 2` with the budget check on its
own control edge between the halves.

Proven on 0x100250D0 BrTex3dExpand (8480 B): 15 of 16 folded sites, taking the
function from +1152 to +512 bytes over the original and +234 to +81
instructions, with two IDX4 arms becoming instruction-for-instruction
identical to the original.  See docs/idioms-A.md.

This is a GENERIC Ghidra artifact, not a quirk of one function: any decompiled
loop that writes two elements per iteration under a running byte budget comes
back folded the same way.

    python3 tools/gen_countfold.py --validate
    python3 tools/gen_countfold.py --file <path.c>
"""
from __future__ import print_function

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# `if (CTR + N >= BOUND) {`  -- the folded first guard.
_G1 = re.compile(r'^(\s*)if \((\w+) \+ (\d+) >= (\w+)\) \{$')
# `if (CTR >= BOUND) {`      -- the unfolded second guard.
_G2 = re.compile(r'^(\s*)if \((\w+) >= (\w+)\) \{$')
_ADD = re.compile(r'^\s*(\w+) = \1 \+ (\d+);$')
_PADV = re.compile(r'^(\s*)(\w+) = \2 \+ 2;$')
_STORE0 = re.compile(r'^(\s*)\*(\w+) = ')
_STORE1 = re.compile(r'^(\s*)(\w+)\[1\] = ')


def _block_end(lines, i):
    """Index of the line closing the brace opened on line i, or None."""
    depth = 0
    for k in range(i, len(lines)):
        depth += lines[k].count('{') - lines[k].count('}')
        if depth == 0 and k > i:
            return k
    return None


def _stmt_end(lines, i):
    """Index of the line ending the (possibly multi-line) statement at i."""
    for k in range(i, min(i + 12, len(lines))):
        if lines[k].rstrip().endswith(';'):
            return k
    return None


def _find_sites(lines):
    """Yield dicts describing each foldable site, latest first."""
    out = []
    for i, ln in enumerate(lines):
        m1 = _G1.match(ln)
        if not m1:
            continue
        ind, ctr, n, bound = m1.group(1), m1.group(2), int(m1.group(3)), m1.group(4)
        e1 = _block_end(lines, i)
        if e1 is None:
            continue
        exit_body = lines[i + 1:e1]
        # Second guard: same counter and bound, same exit body, in the same
        # block.  Anything further away is a different pair.
        g2 = None
        for k in range(e1 + 1, min(e1 + 40, len(lines))):
            m2 = _G2.match(lines[k])
            if not m2:
                continue
            if m2.group(2) != ctr or m2.group(3) != bound:
                break
            e2 = _block_end(lines, k)
            if e2 is None or [s.strip() for s in lines[k + 1:e2]] != \
                             [s.strip() for s in exit_body]:
                break
            g2 = k
            break
        if g2 is None:
            continue
        span = range(e1 + 1, g2)
        # Exactly one `CTR = CTR + 2N;` in the span.
        adds = [k for k in span if _ADD.match(lines[k])
                and _ADD.match(lines[k]).group(1) == ctr
                and int(_ADD.match(lines[k]).group(2)) == 2 * n]
        if len(adds) != 1:
            continue
        # Exactly one `PTR = PTR + 2;` in the span, and one `PTR[1] = ` store
        # naming the same pointer.
        st1 = [k for k in span if _STORE1.match(lines[k])]
        if len(st1) != 1:
            continue
        ptr = _STORE1.match(lines[st1[0]]).group(2)
        advs = [k for k in span if _PADV.match(lines[k])
                and _PADV.match(lines[k]).group(2) == ptr]
        if len(advs) != 1:
            continue
        # The A-store is the nearest preceding `*PTR = ` on the same pointer.
        a0 = None
        for k in range(i - 1, max(i - 14, -1), -1):
            m = _STORE0.match(lines[k])
            if m and m.group(2) == ptr:
                a0 = k
                break
            if _G2.match(lines[k]) or _G1.match(lines[k]):
                break
        if a0 is None:
            continue
        a1 = _stmt_end(lines, a0)
        if a1 is None or a1 >= i:
            continue
        out.append(dict(ind=ind, ctr=ctr, n=n, bound=bound, ptr=ptr,
                        a0=a0, a1=a1, g1=i, e1=e1, add=adds[0],
                        st1=st1[0], adv=advs[0], g2=g2))
    return out


def transform_countfold(src, orig=None):
    """Split every folded two-store budget site.  Returns (new_src, n_sites)."""
    lines = src.split('\n')
    sites = _find_sites(lines)
    if not sites:
        return src, 0
    for s in reversed(sites):                      # latest first: indices hold
        ind, ctr, n, ptr = s['ind'], s['ctr'], s['n'], s['ptr']
        bump = '%s%s = %s + %d;' % (ind, ctr, ctr, n)
        # second half, working backwards so earlier edits keep their indices
        lines[s['adv']] = '%s%s = %s + 1;' % (
            _PADV.match(lines[s['adv']]).group(1), ptr, ptr)
        m = _STORE1.match(lines[s['st1']])
        lines[s['st1']] = '%s*%s = %s' % (m.group(1), ptr,
                                          lines[s['st1']][m.end():])
        del lines[s['add']]                        # the folded `+= 2N`
        lines.insert(s['e1'] + 1, bump)            # after the first guard's `}`
        # first half
        lines[s['g1']] = '%sif (%s >= %s) {' % (ind, ctr, s['bound'])
        astore_ind = _STORE0.match(lines[s['a0']]).group(1)
        lines.insert(s['a1'] + 1, '%s%s = %s + 1;' % (astore_ind, ptr, ptr))
        lines.insert(s['a0'], bump)
    return '\n'.join(lines), len(sites)


def gen_countfold(src, orig=None):
    """Yield (label, mutated_source) in the _refine_candidates style."""
    new, n = transform_countfold(src, orig)
    if n and new != src:
        yield ('countfold', new)


# ---------------------------------------------------------------------------
# validation: replay against the function the idiom was derived from
# ---------------------------------------------------------------------------

def _validate():
    """Replay on the pre-transform BrTex3dExpand and report site coverage."""
    import subprocess
    ref = subprocess.run(
        ['git', 'show', 'HEAD~1:src/core/drawing/br_tex3d_expand.c'],
        cwd=ROOT, capture_output=True, text=True)
    if ref.returncode:
        print('validate: cannot read the pre-transform revision', file=sys.stderr)
        return 1
    before = ref.stdout
    folded = before.count('+ 2 >= cbMax')
    new, n = transform_countfold(before)
    print('pre-transform folded sites in br_tex3d_expand.c: %d' % folded)
    print('sites rewritten by transform_countfold:          %d' % n)
    left = len(_find_sites(new.split('\n')))
    print('sites still folded after the transform:          %d' % left)
    if n == 0:
        print('FAIL: transform fired on nothing')
        return 1
    if left:
        print('FAIL: transform is not idempotent')
        return 1
    out = os.path.join(ROOT, 'build', 'match', 't3d', 'v_countfold.c')
    if os.path.isdir(os.path.dirname(out)):
        with open(out, 'w') as f:
            f.write(new)
        print('wrote %s -- score it with:' % out)
        print('    sh build/match/t3d/vdiff.sh countfold')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--validate', action='store_true',
                    help='replay on the function the idiom came from')
    ap.add_argument('--file', help='apply to a .c file and print the result')
    ap.add_argument('--in-place', action='store_true')
    a = ap.parse_args()
    if a.validate:
        return _validate()
    if a.file:
        src = open(a.file).read()
        new, n = transform_countfold(src)
        print('sites: %d' % n, file=sys.stderr)
        if a.in_place:
            open(a.file, 'w').write(new)
        else:
            sys.stdout.write(new)
        return 0
    ap.print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())
