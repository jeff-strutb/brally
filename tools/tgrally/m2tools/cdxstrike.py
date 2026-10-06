"""cdxstrike.py VA draft.c PROC SYM[,SYM...] [--p2] : diagnostic only.

Splits every phase-1 web of the given uopt symbols (cascading through the
split pieces) with the instrumented uopt, then prints the force string and
the blind score.  Answers "if these values were not in callee-saved
registers, would the rest line up with the ROM?"."""
import os, re, subprocess, sys, tempfile
sys.path.insert(0, 'tools/tgrally')
import n64t3 as T
va, draft, proc, syms = sys.argv[1], sys.argv[2], sys.argv[3], set(sys.argv[4].split(','))
CC = 'build/tgrally/ext/instr/out/cc'
p, n, _ = T.source_of(int(va, 16))
tree = open(p).read(); body = T.function_text(tree, n)
fd, full = tempfile.mkstemp(suffix='.c', dir='build/tgrally/n64/m2tools/tw'); os.close(fd)
open(full, 'w').write(tree.replace(body, open(draft).read().strip('\n')))
flags = ['-c', '-mips2', '-non_shared', '-G', '0', '-w', '-Xfullwarn', '-Wab,-r4300_mul', '-Isrc/tgrally/include', '-O2']
force = []
for it in range(60):
    log = full + '.log'
    env = dict(os.environ, CDX_LOG='1', CDX_PROC=proc, CDX_OUT=log, CDX_FORCE=','.join(force))
    subprocess.run([CC] + flags + ['-o', full + '.o', full], env=env, capture_output=True)
    new = []
    for l in open(log):
        if not l.startswith('[CDX] p1color'):
            continue
        m = dict(kv.split('=', 1) for kv in l.split()[2:] if '=' in kv)
        if m['sym'] in syms and (m['reg'].startswith('s') or int(m['color']) >= 30):
            key = 'p1:w%s=s' % m['web']
            if key not in force:
                new.append(key)
    if not new:
        break
    force += new
os.unlink(full); os.unlink(full + '.o'); os.unlink(full + '.log')
print('CDX_FORCE=' + ','.join(force))
env = dict(os.environ, TGR_CC=CC, CDX_PROC=proc, CDX_FORCE=','.join(force))
print(subprocess.run([sys.executable, 'tools/tgrally/m2tools/bsc.py', va, draft], env=env,
                     capture_output=True, text=True).stdout)
