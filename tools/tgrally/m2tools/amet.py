"""amet.py VA draft.c : aligned metrics -- lines differing with registers, and with registers masked."""
import difflib, os, re, subprocess, sys
out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'dv.py'), sys.argv[1], sys.argv[2]],
                     capture_output=True, text=True).stdout.splitlines()
head = out[0] if out else 'ERR'
ours, rom = [], []
for l in out[1:]:
    if '|' not in l:
        continue
    a, b = l.split('|', 1)
    o, r = a[12:].strip(), b.strip()
    if o: ours.append(re.sub(r'\s+', ' ', o))
    if r: rom.append(re.sub(r'\s+', ' ', r))
def cnt(A, B):
    sm = difflib.SequenceMatcher(None, A, B, autojunk=False)
    return sum(max(i2 - i1, j2 - j1) for t, i1, i2, j1, j2 in sm.get_opcodes() if t != 'equal')
mask = lambda L: [re.sub(r'\$(t\d|s\d|v\d|a\d|f\d+|at|ra)', 'R', x) for x in L]
print(head, 'aligned', cnt(ours, rom), 'blind', cnt(mask(ours), mask(rom)), 'len', len(ours), len(rom))
