"""ugs.py VA draft.c FROM TO : ugen's raw -S output (pre-as1) for source lines FROM..TO of the spliced draft (read-only)."""
import os, re, subprocess, sys, tempfile
sys.path.insert(0, 'tools/tgrally')
import n64t3 as T, n64build as B
va = int(sys.argv[1], 16); path, name, _ = T.source_of(va)
tree = open(path).read(); body = T.function_text(tree, name)
nb = open(sys.argv[2]).read().strip('\n'); src = tree.replace(body, nb)
off = src[:src.index(nb)].count('\n')
lo, hi = int(sys.argv[3]) + off, int(sys.argv[4]) + off
d = tempfile.mkdtemp(); p = os.path.join(d, 'x.c'); open(p, 'w').write(src)
flags = [f for f in B.BASE_FLAGS if f != '-c'] + B.cflags_for(src)
subprocess.run([os.path.abspath('tools/toolchains/ido53/cc'), '-S'] + flags + ['x.c'], cwd=d, capture_output=True)
on = False
for l in open(os.path.join(d, 'x.s')):
    m = re.match(r'\s*\.loc\s+\d+\s+(\d+)', l)
    if m: on = lo <= int(m.group(1)) <= hi
    if on and not re.match(r'\s*\.(no)?alias', l): print(l.rstrip())
