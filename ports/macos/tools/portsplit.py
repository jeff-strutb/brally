#!/usr/bin/env python3
"""portsplit.py -- move a decomp file's port arms out into a port spec.

WHAT IT DOES: takes a file that still carries BR_MATCHING_BUILD / _MSC_VER
conditionals and splits it in two.  The file itself is rewritten to its
matching view -- exactly the lines MSVC compiled, no conditionals -- and the
port's differences from that view become ports/macos/patch/<path>.port (see
portgen.py for the format).  Applying the spec to the rewritten file gives
back the port view the conditionals used to select, which this script checks
before it writes anything.

    portsplit.py FILE...        split; refuse any file whose round trip fails
    portsplit.py --dry FILE...  check only, write nothing

A file whose port view is its matching view needs no spec and gets none.
"""
import difflib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portgen  # noqa: E402

ROOT = portgen.ROOT
M_DEFS = ['-DBR_MATCHING_BUILD', '-D_MSC_VER=1100']
P_DEFS = ['-UBR_MATCHING_BUILD', '-U_MSC_VER']


def view(path, defs):
    p = subprocess.run(['unifdef'] + defs + [path], capture_output=True)
    if p.returncode > 1:
        raise SystemExit('unifdef failed on %s: %s' % (path, p.stderr.decode()))
    return p.stdout.decode('latin-1')


def norm(s):
    return ' '.join(s.split())


def code(s):
    """An item's tokens as the compiler sees them: comments removed."""
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'//[^\n]*', ' ', s)
    return norm(s)


def make_spec(m, p, rel):
    mi = portgen.keyed_items(m)
    pi = portgen.split_items(p)
    mt = [code(m[s:e]) for _, s, e in mi]
    pt = [code(p[s:e]) for s, e, _ in pi]
    ops = []
    sm = difflib.SequenceMatcher(None, mt, pt, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        for k in range(i1, i2):
            ops.append('@drop %s\n' % mi[k][0])
        if j2 > j1:
            anchor = mi[i2 - 1][0] if i2 > i1 else (mi[i1 - 1][0] if i1 else '^')
            blk = ''.join(p[s:e] for s, e, _ in pi[j1:j2])
            if not blk.endswith('\n'):
                blk += '\n'
            ops.append('@after %s\n%s@end\n' % (anchor, blk))
    # The /MD preamble (`#define _CRTIMP __declspec(dllimport)`) is inert
    # for the port when nothing in the file spells _CRTIMP: a #define no
    # token uses changes nothing clang compiles.  No spec for that alone.
    if ops == ['@drop define:_CRTIMP\n'] and m.count('_CRTIMP') == 1:
        return None
    if not ops:
        return None
    head = ('# Port deviations for %s -- see ports/macos/tools/portgen.py.\n'
            '# The decomp file is what MSVC compiles; these are the items the\n'
            '# Mac port drops, and the port bodies it compiles in their place.\n'
            % rel)
    return head + ''.join(ops)


def split(rel, dry):
    path = os.path.join(ROOT, rel)
    m = view(path, M_DEFS)
    p = view(path, P_DEFS)
    spec = make_spec(m, p, rel)
    if spec is not None:
        got = portgen.apply_spec(m, spec, rel)
        if code(got) != code(p):
            a, b = got.splitlines(), p.splitlines()
            d = list(difflib.unified_diff(b, a, 'port-view', 'spec-applied',
                                          lineterm='', n=1))[:20]
            return 'ROUNDTRIP-FAIL', '\n'.join(d)
    if not dry:
        with open(path, 'w', newline='') as f:
            f.write(m)
        sp = os.path.join(portgen.PATCH, rel + '.port')
        if spec is not None:
            os.makedirs(os.path.dirname(sp), exist_ok=True)
            with open(sp, 'w', newline='') as f:
                f.write(spec)
        elif os.path.exists(sp):
            os.unlink(sp)
    return ('spec' if spec else 'nospec'), ''


def main():
    args = sys.argv[1:]
    dry = '--dry' in args
    files = [a for a in args if a != '--dry']
    bad = 0
    counts = {}
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor() as ex:
        res = list(ex.map(split, files, [dry] * len(files)))
    for rel, (st, detail) in zip(files, res):
        counts[st] = counts.get(st, 0) + 1
        if st == 'ROUNDTRIP-FAIL':
            bad += 1
            print('FAIL %s\n%s' % (rel, detail))
    print(' '.join('%s=%d' % kv for kv in sorted(counts.items())))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
