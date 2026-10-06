"""spell.py VA draft.c 'old text' 'new1' 'new2' ... : first-diff line index for each spelling.

Replaces the exact old text in the draft with each candidate and reports the
first real differing listing line (firstdiff.py) -- the longer, the better
for top-down giants."""
import os, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
va, f, old = sys.argv[1], sys.argv[2], sys.argv[3]
b = open(f).read()
assert old in b, 'old text not found'
def run(new):
    fd, p = tempfile.mkstemp(suffix='.c'); os.close(fd)
    open(p, 'w').write(b.replace(old, new, 1))
    out = (subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'firstdiff.py'), va, p, '0', os.environ.get('FD_MODE', '--nosp')],
                          capture_output=True, text=True).stdout.splitlines() or ['compile failed'])[0]
    os.unlink(p)
    return new, out
with ThreadPoolExecutor(6) as ex:
    for k, (new, out) in enumerate(ex.map(run, [old] + sys.argv[4:])):
        print('%2d %-50s %s' % (k, ' '.join(new.split())[:50], out))
