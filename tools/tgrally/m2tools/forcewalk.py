"""forcewalk.py VA [--seed k,k] [--rounds N] : walk the function front to back,
finding at each first differing word the single CDX_FORCE (split or a pool
colour on a web whose colour is one of the two registers involved there) that
pushes the first diff furthest.  The force set it ends with is the allocation
the ROM implies; each force is then a source question."""
import re, sys
sys.path.insert(0, 'tools/tgrally')
import n64alloc as A, n64build as B  # noqa: E402
from concurrent.futures import ThreadPoolExecutor
from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_BIG_ENDIAN
import struct
md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)
args = sys.argv[1:]
va = int(args[0], 16)
fixed = [k for k in args[args.index('--seed') + 1].split(',') if k] if '--seed' in args else []
rounds = int(args[args.index('--rounds') + 1]) if '--rounds' in args else 20
g = A.Grader(va, None)
REGNUM = {v: k for k, v in A.REG.items()}

def same(a, b):
    # an unresolved local-data relocation: same opcode and registers, our immediate is a section offset
    if (a >> 16) == (b >> 16) and ((a >> 26) in (1, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or ((a >> 26) == 0x11 and ((a >> 21) & 31) == 8)):
        return True   # a branch: its offset moves with code length elsewhere
    return a == b or ((a >> 16) == (b >> 16) and (a >> 26) in (0x0f, 0x09, 0x23, 0x24, 0x25, 0x2b, 0x28, 0x29, 0x31, 0x39) and (a & 0xffff) < 0x1000)

def build(keys, log=False):
    env = {'CDX_PROC': str(g.proc)}
    if keys:
        env['CDX_FORCE'] = ','.join(keys)
    if log:
        env['CDX_LOG'] = '1'
    obj, lg = A.compile_traced(g.f, env)
    if obj is None:
        return None, None, lg
    have = {p[0]: p for p in B.carve(obj)}
    _, s, e = have[g.name]
    st, nd, notes, ours, theirs = B.grade(obj, g.rom, g.name, s, e, g.va, g.fmap[g.va], g.syms, g.fnvas)
    first = next((k for k, (a, b) in enumerate(zip(ours, theirs)) if not same(a, b)), min(len(ours), len(theirs)))
    return (first, -nd), (ours, theirs), lg

def regs(w):
    for i in md.disasm(struct.pack('>I', w), 0):
        return set(re.findall(r'\$(\w+)', i.op_str))
    return set()

score, (ours, theirs), log = build(fixed, log=True)
print('start', score, fixed, flush=True)
for rnd in range(rounds):
    d = score[0]
    if d >= len(theirs):
        print('EXACT'); break
    want = set()
    for k in range(max(d - 2, 0), min(d + 3, len(ours))):
        want |= regs(ours[k]) | regs(theirs[k])
    want = {REGNUM[r] for r in want if r in REGNUM}
    rows = A.decisions(log)
    cands = set()
    for row in rows:
        w = 'p1:w%s' % row['web']
        if any(f.startswith(w + '=') or f.startswith(w + '@') for f in fixed):
            continue
        c = int(row.get('bestcolor', '-1'))
        if c in want or row['decision'] != 'color':
            cands.add(w + '=s')
            if '@' not in w and row['decision'] == 'color':
                cands.add(w + '@1=s')
            for r in want:
                if r != c and r < 23:
                    cands.add('%s=c%d' % (w, r))
    cands = sorted(cands)
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(lambda k: (k, build(fixed + [k])[0]), cands))
    res = [(k, s) for k, s in res if s is not None]
    k, s = max(res, key=lambda r: r[1])
    if s <= score:
        print('round', rnd, 'stuck at', score, 'cands', len(cands), flush=True); break
    fixed.append(k)
    score, (ours, theirs), log = build(fixed, log=True)
    print('round', rnd, k, score, flush=True)
print('final', ','.join(fixed), score)
