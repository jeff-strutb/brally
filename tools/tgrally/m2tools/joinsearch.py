"""joinsearch.py VA draft.c A-B : join each line k in [A,B) with line k+1
(line-number ties change ugen's scheduling and free order) and report the
first real differing listing line per join, best first."""
import os, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
va, f, rng = sys.argv[1], sys.argv[2], sys.argv[3]
a, b = (int(x) for x in rng.split('-'))
lines = open(f).read().split('\n')
def run(k):
    nl = lines[:k - 1] + [lines[k - 1].rstrip() + ' ' + lines[k].strip()] + lines[k + 1:]
    fd, p = tempfile.mkstemp(suffix='.c'); os.close(fd)
    open(p, 'w').write('\n'.join(nl))
    out = (subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'firstdiff.py'), va, p, '0', os.environ.get('FD_MODE', '--nosp')],
                          capture_output=True, text=True).stdout.splitlines() or ['compile failed'])[0]
    os.unlink(p)
    return k, out
with ThreadPoolExecutor(8) as ex:
    res = list(ex.map(run, range(a, b)))
def key(r):
    try:
        return -int(r[1].split()[-1])
    except ValueError:
        return 0
for k, out in sorted(res, key=key):
    print('%4d %-60s %s' % (k, lines[k - 1].strip()[:60], out))
