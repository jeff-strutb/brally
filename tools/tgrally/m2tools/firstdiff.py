"""firstdiff.py VA draft.c [N] : the first N differing lines (positional) with context."""
import os, subprocess, sys
va, f = sys.argv[1], sys.argv[2]
import re
nosp = '--nosp' in sys.argv or '--blind' in sys.argv
blind = '--blind' in sys.argv
argv = [a for a in sys.argv if a not in ('--nosp', '--blind')]
n = int(argv[3]) if len(argv) > 3 else 30


def spdiff(l):
    """True when the line differs beyond sp-relative offsets (frame-size shift)."""
    if '|' not in l:
        return False
    a, b = l.split('|', 1)
    m = lambda x: re.sub(r'-?0x[0-9a-f]+\(\$sp\)', 'S($sp)', re.sub(r'(addiu\s+\$\w+, \$sp, )-?0x[0-9a-f]+', r'\1K',x[12:] if x is a else x)).split()
    if blind:
        A_ = [re.sub(r'\$[a-z]\d', 'R', x) for x in m(a)]
        B_ = [re.sub(r'\$[a-z]\d', 'R', x) for x in m(b)]
        # an unresolved local-data relocation shows as lui 0 / addiu off on our side
        if A_[:1] == B_[:1] and A_[:1] in (['lui'], ['addiu']) and len(A_) == len(B_):
            A_, B_ = A_[:-1], B_[:-1]
        return A_ != B_
    return m(a) != m(b)
out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'dv.py'), va, f],
                     capture_output=True, text=True).stdout.splitlines()
k = next((i for i, l in enumerate(out) if '!!' in l and (not nosp or spdiff(l)) and 'beq' not in l and 'bne' not in l and ' b ' not in l
          and 'bnez' not in l and 'beqz' not in l and 'bgez' not in l and 'blez' not in l and 'bc1' not in l
          and 'bltz' not in l and 'bgtz' not in l), None)
print(out[0], 'first real diff line', k)
if k is not None:
    for l in out[max(k - 12, 1):k + n]:
        print(l)
