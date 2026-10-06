"""ascore.py VA file.c [...] : length-robust score for long functions.

Aligns each draft's listing with the ROM (difflib on full instruction text,
branch/jal targets masked) and prints the number of changed instructions
(sum of the larger side of each non-equal hunk) and the structural count
(same with registers masked)."""
import difflib, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
va = sys.argv[1]
def score(f):
    out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'dv.py'), va, f],
                         capture_output=True, text=True).stdout.splitlines()[1:]
    o = [l.split('|', 1)[0][12:].strip() for l in out if '|' in l]
    r = [l.split('|', 1)[1].strip() for l in out if '|' in l]
    o = [x for x in o if x]; r = [x for x in r if x]
    res = []
    for regs in (True, False):
        def k(s):
            s = re.sub(r'\s+', ' ', s); s = re.sub(r'0x[0-9a-f]{6,8}', 'A', s)
            return s if regs else re.sub(r'\$\w+', '$', s)
        sm = difflib.SequenceMatcher(None, [k(x) for x in o], [k(x) for x in r], autojunk=False)
        res.append(sum(max(i2 - i1, j2 - j1) for t, i1, i2, j1, j2 in sm.get_opcodes() if t != 'equal'))
    return f, res[0], res[1], len(o), len(r)
with ThreadPoolExecutor(6) as ex:
    for f, a, s, no, nr in ex.map(score, sys.argv[2:]):
        print('%s changed=%d structural=%d len %d/%d' % (os.path.basename(f), a, s, no, nr))
