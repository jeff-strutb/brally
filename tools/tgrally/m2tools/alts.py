"""alts.py VA draft.c ALTS.py : score alternative spellings with bsc.py.

ALTS.py defines ALTS = [(name, old, new), ...]; each alternative replaces
every occurrence of old in the draft (asserting it is present).  Prints the
blind score of the base and of each alternative, best first."""
import os, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
va, draft, altf = sys.argv[1], sys.argv[2], sys.argv[3]
ns = {}
exec(open(altf).read(), ns)
base = open(draft).read()
work = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tw')


def score(item):
    name, old, new = item
    if old is None:
        s = base
    else:
        assert old in base, name
        s = base.replace(old, new)
    fd, p = tempfile.mkstemp(suffix='.c', dir=work); os.close(fd)
    open(p, 'w').write(s)
    out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'bsc.py'), va, p],
                         capture_output=True, text=True).stdout
    os.unlink(p)
    try:
        return name, int(out.split('blind=')[1].split()[0])
    except Exception:
        return name, None


items = [('BASE', None, None)] + ns['ALTS']
with ThreadPoolExecutor(8) as ex:
    res = list(ex.map(score, items))
for name, n in sorted(res, key=lambda r: (r[1] is None, r[1])):
    print(f'{n}\t{name}')
