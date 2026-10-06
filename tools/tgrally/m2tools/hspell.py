"""hspell.py VA draft.c 'old' 'new1' ... : structural (loose, register-blind) hunk
count and the ROM addresses of the hunks for each spelling -- for uopt-coloured
functions, where the first positional diff stops at a colouring residue."""
import os, re, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
va, f, old = sys.argv[1], sys.argv[2], sys.argv[3]
b = open(f).read()
assert old in b, 'old text not found'
H = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hunks.py')
def run(new):
    fd, p = tempfile.mkstemp(suffix='.c'); os.close(fd)
    open(p, 'w').write(b.replace(old, new, 1))
    out = subprocess.run([sys.executable, H, va, p, '--loose'], capture_output=True, text=True).stdout
    os.unlink(p)
    hs = re.findall(r'=== hunk \d+ \w+ ours\[\d+:\d+\] rom\[(\d+):(\d+)\] @(\w+)', out)
    tail = out.strip().splitlines()[-1] if out.strip() else 'compile failed'
    size = sum(int(b_) - int(a_) for a_, b_, _ in hs)
    return new, tail, [h[2] for h in hs]
with ThreadPoolExecutor(8) as ex:
    res = list(ex.map(run, [old] + sys.argv[4:]))
base = set(res[0][2])
for k, (new, tail, addrs) in enumerate(res):
    gone = sorted(base - set(addrs)); new_ = sorted(set(addrs) - base)
    print('%2d %-45s %s  -%s +%s' % (k, ' '.join(new.split())[:45], tail, ','.join(gone[:6]), ','.join(new_[:6])))
