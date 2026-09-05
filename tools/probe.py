#!/usr/bin/env python3
"""ONE variant, ONE compile, EVERY measure that matters -- the probe loop for a
big function, ~6 s a turn.

    .venv/bin/python tools/probe.py <variant.c> <TAG> \
        [--va 0x1000EAF0] [--sym BrSceneDlBuild] [--key 6] [--full] [--no-mset]

WHY THIS EXISTS.  Grinding a giant means one edit, one compile, one judgement,
hundreds of times, and every measure this project trusts lives in a different
tool.  Reading them one at a time is how sessions have gone wrong: a byte count
that improved while a lost-sync gap opened, a FIRSTDIV "regression" that was a
slot renumbering, a region count that fell because the streams misaligned.  So
this prints them TOGETHER, from one fresh compile, and refuses to let you read
any of them alone:

  * BYTES / INSNS against the original, alignment padding stripped
  * the PROLOGUE of both streams side by side -- a frame change is a hard reject
    and no other number can tell you it happened
  * divergence.py MASKED (--mask-slots) and RAW maps with --deltas, including
    every lost-sync / NEVER COMPARED line: a change that opens a gap is a
    regression whatever else improved
  * msetdiff.py register-blind multiset rows (MISSING = the original has it,
    EXTRA = we emit it), which is the honest structural gap
  * the /FAcs equate table -- every local's frame slot as [esp+0xNN], read off
    the compiler's own listing instead of inferred from displacements

Never point it at a tree file to "test" one: copy the file first.  The variant
is compiled with the EXACT sweep flags into build/match/t3d/probe_<TAG>.obj
(plus probe_<TAG>.cod, whose `^  00<offset>` lines map a divergence offset to a
source line), so each tag owns its own object and N probers never contend.

‼ NEVER run tools/match_sweep.py to score a probe: it is a 20-minute
bookkeeping pass, and the one-file sweep still compiles the whole TU.  This is
the measuring instrument; the sweep is the scoreboard, run once at the end.

Companion tools: tools/declsweep.py (sweep a declaration through every position
in its run), tools/dumpasm.py and the --full listing here (read the bytes).
Built 2026-09-04/05 grinding 0x1000EAF0, 0x100250D0 and 0x1000A110; it is what
found the declaration-order tie-breaks recorded in docs/VC5-IDIOMS.md.
"""
import os, re, subprocess, sys, argparse
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
PY = os.path.join(ROOT, '.venv', 'bin', 'python')

ap = argparse.ArgumentParser()
ap.add_argument('src'); ap.add_argument('tag')
ap.add_argument('--va', default='0x1000EAF0'); ap.add_argument('--sym', default='BrSceneDlBuild')
ap.add_argument('--key', default='6'); ap.add_argument('--rows', default='40')
ap.add_argument('--no-mset', action='store_true'); ap.add_argument('--no-div', action='store_true')
ap.add_argument('--full', action='store_true', help='also print the full region listing (context 3)')
a = ap.parse_args()

from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

# cl.exe under wine wants paths RELATIVE to ROOT; the variant is copied into
# build/match/t3d/ (the same directory fn.py uses) under a per-tag name.
import shutil
vdir = os.path.join(ROOT, 'build', 'match', 't3d'); os.makedirs(vdir, exist_ok=True)
relc = 'build/match/t3d/probe_%s.c' % a.tag
relo = 'build/match/t3d/probe_%s.obj' % a.tag
relcod = 'build/match/t3d/probe_%s.cod' % a.tag
obj = os.path.join(ROOT, relo); cod = os.path.join(ROOT, relcod)
for p in (obj, cod):
    if os.path.exists(p): os.unlink(p)
if os.path.abspath(a.src) != os.path.join(ROOT, relc):
    shutil.copy(a.src, os.path.join(ROOT, relc))
cmd = ['sh', 'tools/wine.sh', 'tools/msvc5/bin/cl.exe', '/nologo', '/O2', '/W3',
       '/I', 'include', '/I', 'tools/msvc5-compat', '/I', 'tools/msvc5/include',
       '/DBR_MATCHING_BUILD', '/c', '/FAcs', '/Fa' + relcod.replace('/', '\\'),
       relc, '/Fo' + relo.replace('/', '\\')]
p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=300)
if not os.path.exists(obj):
    print('COMPILE FAILED'); print(p.stdout[-3000:]); print(p.stderr[-2000:]); sys.exit(1)
warn = [l for l in (p.stdout + p.stderr).splitlines() if 'warning' in l or 'error' in l]
for w in warn[:8]: print('  cl:', w.strip())

t = parse_coff_obj(obj)
if a.sym not in t:
    print('symbol %s not in obj (have %s)' % (a.sym, list(t)[:8])); sys.exit(1)
rc, relocs = t[a.sym][0], t[a.sym][1]
while rc and rc[-1] == 0x90: rc = rc[:-1]
orig = open(os.path.join(ROOT, 'build', 'match', 'orig', a.va + '.bin'), 'rb').read()
oi = list(md.disasm(orig, 0)); ri = list(md.disasm(rc, 0))
fd = next((i for i in range(min(len(orig), len(rc))) if i not in relocs and orig[i] != rc[i]), min(len(orig), len(rc)))
ident = (len(rc) == len(orig) and fd == len(orig))
print('=== %s %s  tag=%s' % (a.va, a.sym, a.tag))
print('BYTES orig=%d recomp=%d (%+d)   INSNS orig=%d recomp=%d (%+d)   FIRSTDIV(aligned-blind)=+0x%x%s'
      % (len(orig), len(rc), len(rc) - len(orig), len(oi), len(ri), len(ri) - len(oi), fd,
         '   *** BYTE-EXACT ***' if ident else ''))
print('--- prologue (orig | recomp), first 10 insns')
for x, y in zip(oi[:10], ri[:10]):
    print('  %-34s | %s' % (x.mnemonic + ' ' + x.op_str, y.mnemonic + ' ' + y.op_str))

if not a.no_div:
    for flag, label in (('--mask-slots', 'MASKED'), ('', 'RAW')):
        c = [PY, 'tools/divergence.py', obj, 'build/match/orig/%s.bin' % a.va, a.sym, '--deltas', '--key', a.key]
        if flag: c.append(flag)
        out = subprocess.run(c, cwd=ROOT, capture_output=True, text=True).stdout
        lines = out.splitlines()
        tot = [l for l in lines if l.startswith('total divergence') or 'lost-sync' in l or 'NEVER COMPARED' in l or 'RE-ANCHORED' in l]
        print('--- divergence %s (key %s): ' % (label, a.key) + ' || '.join(tot))
        if label == 'MASKED':
            for l in lines:
                if l.startswith('region'): print('   ' + l)
    if a.full:
        c = [PY, 'tools/divergence.py', obj, 'build/match/orig/%s.bin' % a.va, a.sym, '3', '--key', a.key]
        print(subprocess.run(c, cwd=ROOT, capture_output=True, text=True).stdout)

if not a.no_mset:
    c = [PY, 'tools/msetdiff.py', 'build/match/orig/%s.bin' % a.va, obj, a.sym, a.rows]
    out = subprocess.run(c, cwd=ROOT, capture_output=True, text=True).stdout
    print('--- msetdiff (register-blind multiset)')
    print('\n'.join('   ' + l for l in out.splitlines()[-60:]))

# equate table for THIS function only
eq = []
if os.path.exists(cod):
    txt = open(cod, errors='replace').read()
    m = re.search(r'\n(.*?)_%s\s+PROC NEAR' % re.escape(a.sym), txt, re.S)
    head = txt[:m.end()] if m else txt
    # take the equates immediately above the PROC line (after the previous ENDP)
    tail = head.rsplit('ENDP', 1)[-1]
    eq = re.findall(r'^(_[A-Za-z_][A-Za-z0-9_]*\$[0-9]*) = (-?\d+)', tail, re.M)
    sub = re.search(r'sub\s+esp,\s*(0x[0-9a-f]+|\d+)', ' '.join(x.mnemonic + ' ' + x.op_str for x in ri[:8]))
    fs = int(sub.group(1), 0) if sub else None
    # esp-relative slot = frame + 12 (ebx/esi/edi pushes) + off  -- valid for the push ebp/mov ebp,esp/push x3/sub esp,N prologue
    print('--- /FAcs equates (frame sub esp,%s): name  ebp-rel  esp-slot' % (hex(fs) if fs else '?'))
    for n, off in eq:
        off = int(off)
        if off > 0: continue
        slot = (fs + 12 + off) if fs is not None else None
        print('   %-18s %5d   %s' % (n, off, ('[esp+0x%x]' % slot) if slot is not None else '?'))
