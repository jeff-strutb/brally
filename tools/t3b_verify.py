#!/usr/bin/env python3
"""t3b_verify.py -- differential equivalence oracle: does the rebuilt C behave
like the original, regardless of instruction shape?

This turns the T3b tier ("works, built differently") from a hand-label into a
measured one.  It executes BOTH the original function's bytes and the
recompiled bytes through the same x86+x87 interpreter (tools/x87emu.py) on
identical random inputs, and compares the return value.  Same output across
many random inputs => behaviourally equivalent.

SOUNDNESS over completeness.  It only returns a verdict of EQUIVALENT or DIFF
when the run is fully contained in a randomised stack window -- i.e. the
function is a pure-ish scalar/float function that touches no global, no
dereferenced pointer, and makes no external call.  Anything else (a relocation
in the object, an out-of-window memory access, an unmapped call, an opcode the
interpreter doesn't model) yields UNCLASSIFIED, never a false EQUIVALENT.  That
lights up the math/geometry/state subset of T2 first; pointer/global/COM
functions need input scaffolding this v1 deliberately does not fake.

    python3 tools/t3b_verify.py 0x10034360           # one function
    python3 tools/t3b_verify.py --t2                 # sweep the whole T2 pile
    python3 tools/t3b_verify.py --t2 --seeds 200      # more inputs per function

Return-type (int in eax vs float in st0) is read from the function's C
prototype in the tree.  Verdicts: EQUIVALENT (T3b or better), DIFF (a real
logic difference -- genuinely still T2), UNCLASSIFIED (out of this oracle's
reach).  A byte-exact (T4) function is trivially EQUIVALENT and is a built-in
sanity check.
"""
from __future__ import print_function
import argparse, csv, os, re, struct, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import x87emu

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = False

STACK_BASE = 0x00200000
WIN_LO = STACK_BASE - 0x2000
WIN_HI = STACK_BASE + 0x0800
HEAP_BASE = 0x00300000          # pointer-arg buffers live here
BUF_STRIDE = 0x400
BUF_SIZE = 0x100


class RecMem(dict):
    """A byte memory that records every address touched, so an access outside
    the regions we set up (a global, a wild pointer) can be detected and the
    run rejected rather than silently passing."""
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.touched = set()

    def get(self, a, d=0):
        self.touched.add(a)
        return dict.get(self, a, d)

    def __setitem__(self, a, v):
        self.touched.add(a)
        dict.__setitem__(self, a, v)


def _in_regions(addr, regions):
    for lo, hi in regions:
        if lo <= addr < hi:
            return True
    return False


def disasm(code, base):
    """capstone -> x87emu listing tuples (addr, mnemonic, op_str)."""
    return [(i.address, i.mnemonic, i.op_str) for i in md.disasm(code, base)]


def _lcg(seed):
    x = (seed * 2654435761 + 0x9E3779B9) & 0xFFFFFFFF
    def nxt():
        nonlocal x
        x = (x * 1103515245 + 12345) & 0xFFFFFFFF
        return x
    return nxt


def parse_signature(name):
    """Read the function's C prototype from the tree. Returns
    (ret, [param_kinds]) where ret in {'int','float'} and each kind in
    {'int','float','ptr'} -- or None if the signature is not a plain cdecl of
    scalar/pointer args (register conventions and by-value structs are left to
    a human, never guessed)."""
    import subprocess
    try:
        # grep the plain identifier (git grep -E is unreliable with \b/\s);
        # do the precise prototype match in Python below.
        g = subprocess.run(['git', 'grep', '-h', name, '--', 'src/'],
                           cwd=ROOT, capture_output=True, text=True)
    except Exception:
        return None
    best = None
    for ln in g.stdout.splitlines():
        ln = ln.strip()
        if ln.endswith(';') or ln.startswith(('/', '*', '#')):
            continue                                   # declaration or comment
        m = re.match(r'^([A-Za-z_][\w \t]*?)\b%s\s*\(([^)]*)\)' % re.escape(name), ln)
        if not m:
            continue
        rettype, params = m.group(1), m.group(2).strip()
        if re.search(r'__fastcall|__thiscall|__stdcall|BR_THISCALL|BR_FASTCALL', rettype):
            return None                                # register convention: skip
        if rettype.split() and rettype.split()[-1] == 'void':
            ret = 'void'                               # no return value: compare side effects only
        elif ('float' in rettype or 'double' in rettype) and '*' not in rettype:
            ret = 'float'
        else:
            ret = 'int'
        kinds = []
        if params and params != 'void':
            for p in params.split(','):
                p = p.strip()
                if '*' in p or p.endswith('[]'):
                    kinds.append('ptr')
                elif 'float' in p or 'double' in p:
                    kinds.append('float')
                elif re.search(r'\b(int|char|short|long|unsigned|size_t|BrFixed|'
                               r'int8_t|int16_t|int32_t|uint8_t|uint16_t|uint32_t)\b', p):
                    kinds.append('int')
                else:
                    return None                        # by-value struct / unknown
        best = (ret, kinds)
        break
    return best


def _sfloat_bits(rnd):
    """A finite, modest random float in [-100, 100] as its 32-bit pattern.
    Tame on purpose -- equivalence needs identical inputs, not extreme ones,
    and arbitrary bit patterns make inf/NaN that overflow int conversions."""
    x = ((rnd() % 2000001) - 1000000) / 10000.0
    return struct.unpack('<I', struct.pack('<f', x))[0]


def _fill_dwords(mem, lo, hi, rnd):
    """Fill [lo, hi) with per-dword tame-float bit patterns (also fine read as
    ints), so nothing read as a float is inf/NaN."""
    a = lo
    while a + 4 <= hi:
        bits = _sfloat_bits(rnd)
        for k in range(4):
            dict.__setitem__(mem, a + k, (bits >> (8 * k)) & 0xFF)
        a += 4


def _setup(seed, sig):
    """Build a fresh (mem, regs, regions, buffers) for one seed and one
    signature. regions = address ranges the run is allowed to touch."""
    rnd = _lcg(seed)
    mem = RecMem()
    regions = [(WIN_LO, WIN_HI)]
    _fill_dwords(mem, WIN_LO, WIN_HI, rnd)
    regs = {r: (rnd() % 4000) - 2000 & 0xFFFFFFFF
            for r in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')}
    regs['esp'] = STACK_BASE
    buffers = []
    ret, kinds = sig
    bufidx = 0
    for i, kind in enumerate(kinds):
        slot = STACK_BASE + 4 + 4 * i                  # cdecl: args above return addr
        if kind == 'ptr':
            buf = HEAP_BASE + bufidx * BUF_STRIDE
            bufidx += 1
            _fill_dwords(mem, buf, buf + BUF_SIZE, rnd)
            regions.append((buf, buf + BUF_SIZE))
            buffers.append((buf, buf + BUF_SIZE))
            val = buf
        elif kind == 'float':
            val = _sfloat_bits(rnd)
        else:
            val = (rnd() % 4000) - 2000 & 0xFFFFFFFF
        for k in range(4):
            dict.__setitem__(mem, slot + k, (val >> (8 * k)) & 0xFF)
    return mem, regs, regions, buffers


def _snapshot(mem, buffers):
    return bytes(dict.get(mem, a, 0)
                 for (lo, hi) in buffers for a in range(lo, hi))


def verify(va, name, orig_bytes, recomp_bytes, seeds, sig):
    lo = disasm(orig_bytes, va)
    lr = disasm(recomp_bytes, va)
    ret = sig[0]
    for s in range(1, seeds + 1):
        try:
            mo, ro, rego, bo = _setup(s, sig)
            Mo = x87emu.Machine(mo, ro, lo); Mo.run(va)
            mr, rr, regr, br = _setup(s, sig)
            Mr = x87emu.Machine(mr, rr, lr); Mr.run(va)
        except Exception as e:
            return 'UNCLASSIFIED', 'run escaped oracle (%s)' % type(e).__name__
        for mm, rg in ((mo, rego), (mr, regr)):
            if any(not _in_regions(a, rg) for a in mm.touched):
                return 'UNCLASSIFIED', 'touches memory outside set-up regions'
        if ret == 'float':
            a = Mo.st[0] if Mo.st else 0.0
            b = Mr.st[0] if Mr.st else 0.0
            same = (a == b) or (a != a and b != b)
        elif ret == 'void':
            same = True                 # no return value; eax is scratch
        else:
            same = (Mo.R['eax'] == Mr.R['eax'])
        same = same and (_snapshot(mo, bo) == _snapshot(mr, br))   # side effects
        if not same:
            return 'DIFF', 'seed %d diverges (return or written memory)' % s
    return 'EQUIVALENT', '%d inputs agree (return + side effects)' % seeds


def _recomp_for(va_hex, name):
    """Find the recompiled bytes for a function, and refuse if the object has
    relocations (=> references a global; out of this oracle's reach)."""
    for d in ('obj_O2', 'obj_O2y', 'obj_O2p', 'obj_Od'):
        p = os.path.join(ROOT, 'build', 'match', d)
        if not os.path.isdir(p):
            continue
        for f in os.listdir(p):
            if not f.endswith('.obj'):
                continue
            try:
                parsed = parse_coff_obj(os.path.join(p, f))
            except Exception:
                continue
            if name in parsed:
                rec = parsed[name]
                code = rec[0] if isinstance(rec, tuple) else rec
                relocs = rec[1] if isinstance(rec, tuple) and len(rec) > 1 else None
                if relocs:
                    return None, 'has relocations (references a global)'
                return code, None
    return None, 'no recompiled object found'


def load_orig(va_hex):
    p = os.path.join(ROOT, 'build', 'match', 'orig', va_hex + '.bin')
    if not os.path.exists(p):
        p = os.path.join(ROOT, 'build', 'match', 'orig', va_hex.lower() + '.bin')
    if not os.path.exists(p):
        return None
    return open(p, 'rb').read()


def one(va, name, seeds):
    va_hex = va if va.lower().startswith('0x') else '0x' + va
    va_int = int(va_hex, 16)
    orig = load_orig(va_hex)
    if orig is None:
        return 'UNCLASSIFIED', 'no original bytes'
    recomp, why = _recomp_for(va_hex, name)
    if recomp is None:
        return 'UNCLASSIFIED', why
    sig = parse_signature(name)
    if sig is None:
        return 'UNCLASSIFIED', 'signature not a plain cdecl of scalar/ptr args'
    return verify(va_int, name, orig, recomp, seeds, sig)


def t2_rows():
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    for r in csv.DictReader(open(rep)):
        if r.get('orig_size') and r.get('status') == 'diff':
            yield r['va'], r['name']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va', nargs='?')
    ap.add_argument('--t2', action='store_true', help='sweep all T2 functions')
    ap.add_argument('--seeds', type=int, default=64)
    ap.add_argument('--name', default=None)
    a = ap.parse_args()

    if a.t2:
        tally = {'EQUIVALENT': 0, 'DIFF': 0, 'UNCLASSIFIED': 0}
        eqv, diff = [], []
        for va, name in t2_rows():
            verdict, detail = one(va, name, a.seeds)
            tally[verdict] += 1
            if verdict == 'EQUIVALENT':
                eqv.append((va, name))
            elif verdict == 'DIFF':
                diff.append((va, name, detail))
        print('T3b (EQUIVALENT, works-but-different):', tally['EQUIVALENT'])
        print('still-T2 (DIFF, real logic gap):      ', tally['DIFF'])
        print('UNCLASSIFIED (out of oracle reach):   ', tally['UNCLASSIFIED'])
        if eqv:
            print('\n-- newly-provable T3b --')
            for va, name in eqv:
                print('  %s %s' % (va, name))
        if diff:
            print('\n-- DIFF (a real bug caught) --')
            for va, name, d in diff[:40]:
                print('  %s %s  %s' % (va, name, d))
        return 0

    if not a.va:
        ap.error('give a VA or --t2')
    name = a.name
    if not name:
        rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
        for r in csv.DictReader(open(rep)):
            if r['va'].lower() == a.va.lower():
                name = r['name']; break
    if not name:
        ap.error('no report.csv row for %s; pass --name' % a.va)
    verdict, detail = one(a.va, name, a.seeds)
    print('%s %s: %s (%s)' % (a.va, name, verdict, detail))
    return 0


if __name__ == '__main__':
    sys.exit(main())
