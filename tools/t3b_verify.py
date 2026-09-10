#!/usr/bin/env python3
"""t3b_verify.py -- differential equivalence oracle: does the rebuilt C behave
like the original, regardless of instruction shape?

This is the ONLY gate that tests what T3 actually claims.  A1..A4 compare our
compiled bytes with the original's and argue the differences look like
compiler choices; that is an inference about instruction shape.  This executes
BOTH functions on identical inputs and compares what they produce.

It runs them inside the REAL reference image (tools/t3b_env.py): our
relocations are resolved to the original's own addresses, globals live where
they really live and hold what they really hold, and a call executes the
ORIGINAL's callee on both sides -- so the function under test is the only
thing that differs, and calling the wrong helper, or the right one with the
wrong arguments, shows up instead of being stubbed away.

WHAT IS COMPARED is everything the caller could observe: the return value, the
buffers it passed in, and every byte of memory either side wrote to a global
or to the caller's frame.  What is NOT compared is the callee's own stack
frame and its incoming argument slots -- the callee owns those, two
compilations lay them out differently by design, and comparing them reports a
difference on nearly every function that has locals.

SOUNDNESS OVER COMPLETENESS, still.  Anything the harness cannot set up
identically for both sides is refused, never guessed: a relocation naming a
symbol with no known address, a by-value struct parameter, an access outside
the mapped image, an opcode the interpreter does not model, substituted bytes
long enough to bury the next function's entry point.  All of those are
UNCLASSIFIED.

    python3 tools/t3b_verify.py 0x10034360            # one function
    python3 tools/t3b_verify.py --t2                  # sweep the T2 pile
    python3 tools/t3b_verify.py --certified           # sweep the @t3 tags
    python3 tools/t3b_verify.py --t2 --seeds 200      # more inputs each
    python3 tools/t3b_verify.py 0x10034360 --isolated # the old, image-free run

VERDICTS.  EQUIVALENT (agreed on every input).  DIFF (a real logic difference
-- genuinely still T2, and worth reading as a bug report).  EQUIV-MODULO-FP
(agreed except in the last place or two of a float: the interpreter keeps x87
registers as 64-bit doubles where the hardware keeps 80-bit extended, so a
small disagreement is evidence about the interpreter, not the transcription --
it is reported separately rather than being called a bug or waved through).
UNCLASSIFIED (out of reach).  A byte-exact function is trivially EQUIVALENT
and is a built-in sanity check.
"""
from __future__ import print_function
import argparse, csv, os, re, struct, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import x87emu
import t3b_env as ENV

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = False

STACK_BASE = 0x00200000
WIN_LO = STACK_BASE - 0x2000
WIN_HI = STACK_BASE + 0x0800
HEAP_BASE = 0x00300000          # pointer-arg buffers live here
BUF_STRIDE = 0x400
BUF_SIZE = 0x100
IMG_STEPS = 2000000        # budget for a run that now enters real callees


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
    (ret, [param_kinds], conv) where ret is in {'void','int','float'}, each
    kind is in {'int','float','ptr'} and conv is 'cdecl' or 'fastcall' -- or
    None if the prototype uses a by-value struct, which cannot be placed
    without knowing its layout and is never guessed.

    `BR_THISCALL1` is the tree's spelling of __fastcall (include/br_match.h),
    used to reach the original's __thiscall through C: `this` arrives in ecx.
    Refusing those cost 62 of the 233 T2 functions -- the second largest block
    after unresolvable relocations -- for a convention the harness can simply
    honour when it places the arguments."""
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
        conv = 'cdecl'
        if re.search(r'__fastcall|BR_THISCALL1|BR_FASTCALL', rettype):
            conv = 'fastcall'
        elif re.search(r'__thiscall', rettype):
            return None            # not expressible in C here; the C++ lane owns it
        rettype = re.sub(r'__fastcall|__stdcall|BR_THISCALL1?|BR_FASTCALL', ' ', rettype)
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
        best = (ret, kinds, conv)
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
    ret, kinds = sig[0], sig[1]
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


_OBJ_INDEX = None


def _obj_index():
    """name -> (obj path, size of the compiled function). Built once."""
    global _OBJ_INDEX
    if _OBJ_INDEX is None:
        _OBJ_INDEX = {}
        for d in ('obj_O2', 'obj_O2y', 'obj_O2p', 'obj_Od'):
            p = os.path.join(ROOT, 'build', 'match', d)
            if not os.path.isdir(p):
                continue
            for f in sorted(os.listdir(p)):
                if not f.endswith('.obj'):
                    continue
                try:
                    parsed = parse_coff_obj(os.path.join(p, f))
                except Exception:
                    continue
                for n, rec in parsed.items():
                    code = rec[0] if isinstance(rec, tuple) else rec
                    _OBJ_INDEX.setdefault(n, (os.path.join(p, f), len(code)))
    return _OBJ_INDEX


def _setup_img(seed, sig):
    """Memory, registers and allowed regions for one seed, over the real image.

    Identical by construction for the two runs of a seed: the stack window and
    the pointer-argument buffers come from a seeded generator, and everything
    else is the image itself (or, in the BSS tail, a function of the address).
    """
    rnd = _lcg(seed)
    mem = ENV.ImgMem(ENV.image(), seed)
    regions = [(WIN_LO, WIN_HI)]
    a = WIN_LO
    while a + 4 <= WIN_HI:
        mem.put_dword(a, _sfloat_bits(rnd))
        a += 4
    regs = {r: (rnd() % 4000) - 2000 & 0xFFFFFFFF
            for r in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')}
    regs['esp'] = STACK_BASE
    buffers = []
    _ret, kinds, conv = sig[0], sig[1], sig[2]
    bufidx = 0
    # __fastcall gives ecx then edx to the first two parameters that are
    # register-eligible; a float or an aggregate is skipped and passes the
    # register on to the next one.  Everything left goes on the stack in
    # source order above the return address, exactly as cdecl does.
    freeregs = ['ecx', 'edx'] if conv == 'fastcall' else []
    stackpos = 0
    for kind in kinds:
        if kind == 'ptr':
            buf = HEAP_BASE + bufidx * BUF_STRIDE
            bufidx += 1
            b = buf
            while b + 4 <= buf + BUF_SIZE:
                mem.put_dword(b, _sfloat_bits(rnd))
                b += 4
            regions.append((buf, buf + BUF_SIZE))
            buffers.append((buf, buf + BUF_SIZE))
            val = buf
        elif kind == 'float':
            val = _sfloat_bits(rnd)
        else:
            val = (rnd() % 4000) - 2000 & 0xFFFFFFFF
        if freeregs and kind in ('int', 'ptr'):
            regs[freeregs.pop(0)] = val
        else:
            mem.put_dword(STACK_BASE + 4 + 4 * stackpos, val)
            stackpos += 1
    return mem, regs, regions, buffers, stackpos


FP_REL = 1e-6


def _is_rounding(x, y):
    """Are two 32-bit patterns the same number to within x87 rounding noise?

    The interpreter keeps x87 registers as 64-bit doubles where the hardware
    keeps 80-bit extended, so whenever the two compilations round an
    intermediate at different points -- one spills it to a float slot, the
    other keeps it in st -- the results can part company in the last place or
    two.  On hardware that difference is real but lands at a different bit, so
    a small disagreement here is evidence about the interpreter, not about the
    transcription.  Report it as its own verdict; never call it a bug and
    never wave it through as equality.
    """
    fx = struct.unpack('<f', struct.pack('<I', x))[0]
    fy = struct.unpack('<f', struct.pack('<I', y))[0]
    if fx != fx or fy != fy or fx in (float('inf'), float('-inf')) \
       or fy in (float('inf'), float('-inf')):
        return False
    return abs(fx - fy) <= FP_REL * max(1.0, abs(fx), abs(fy))


def _is_rounding_f(x, y):
    if x != x or y != y:
        return x != x and y != y
    try:
        return abs(x - y) <= FP_REL * max(1.0, abs(x), abs(y))
    except OverflowError:
        return False


def _classify_diff(mo, mr, addrs):
    """'rounding' if every differing dword is the same float to within x87
    noise, else 'real'."""
    if not addrs:
        return 'rounding'
    for base in sorted({a & ~3 for a in addrs}):
        x = sum(mo.peek(base + k) << (8 * k) for k in range(4))
        y = sum(mr.peek(base + k) << (8 * k) for k in range(4))
        if x != y and not _is_rounding(x, y):
            return 'real'
    return 'rounding'


def _observable(a, arg_end):
    """Is a written address something the CALLER could see?

    Everything below the entry esp is the function's own frame -- spill slots,
    saved registers, the scratch it builds results in.  Two compilations of
    the same source lay that out differently by design, so comparing it
    reports a difference on almost every function that has locals at all, and
    the caller cannot observe any of it.  The incoming argument slots and the
    return-address slot are the same story: in cdecl the callee owns its
    parameter copies, and reusing one as scratch is an ordinary idiom, so a
    write there is invisible to the caller too.

    What the caller CAN observe is memory above its argument area, the buffers
    it passed in, and globals -- and those are what get compared.
    """
    return not (WIN_LO <= a < arg_end)


def _img_snapshot(mem, buffers, written, arg_end):
    """What the run produced: the argument buffers, plus every observable byte
    either side wrote (globals and the caller's frame above the arguments),
    read back through the overlay."""
    buf = bytes(mem.peek(a) for (lo, hi) in buffers for a in range(lo, hi))
    glob = tuple(sorted((a, mem.peek(a)) for a in written if _observable(a, arg_end)))
    return buf, glob


def verify_img(va, name, orig_bytes, recomp_bytes, seeds, sig):
    """Run both sides inside the mapped image and compare everything
    observable: the return value, the argument buffers, and every byte of
    memory either side wrote."""
    rounding = 0
    prog_o, idx_o = ENV.program_for(va, orig_bytes)
    prog_r, idx_r = ENV.program_for(va, recomp_bytes)
    ret = sig[0]
    for s in range(1, seeds + 1):
        try:
            mo, ro, rego, bo, nstack = _setup_img(s, sig)
            Mo = x87emu.Machine(mo, ro, prog_o, idx_o); Mo.run(va, maxsteps=IMG_STEPS)
            mr, rr, _regr, br, _ns = _setup_img(s, sig)
            Mr = x87emu.Machine(mr, rr, prog_r, idx_r); Mr.run(va, maxsteps=IMG_STEPS)
        except Exception as e:
            return 'UNCLASSIFIED', 'run escaped oracle (%s: %s)' % (
                type(e).__name__, str(e)[:60])
        for mm in (mo, mr):
            if mm.unmapped:
                return 'UNCLASSIFIED', 'touched %d address(es) outside the image' % len(mm.unmapped)
        written = mo.written | mr.written
        if ret == 'float':
            a = Mo.st[0] if Mo.st else 0.0
            b = Mr.st[0] if Mr.st else 0.0
            same = (a == b) or (a != a and b != b)
        elif ret == 'void':
            same = True
        else:
            same = (Mo.R['eax'] == Mr.R['eax'])
        if not same:
            if ret == 'float' and _is_rounding_f(a, b):
                rounding = rounding or s
            else:
                return 'DIFF', 'seed %d: return value differs (%r vs %r)' % (
                    s, a if ret == 'float' else Mo.R['eax'],
                    b if ret == 'float' else Mr.R['eax'])
        arg_end = STACK_BASE + 4 + 4 * nstack
        addrs = [x for (lo, hi) in bo for x in range(lo, hi)
                 if mo.peek(x) != mr.peek(x)]
        addrs += [x for x in written
                  if _observable(x, arg_end) and mo.peek(x) != mr.peek(x)]
        if addrs:
            if _classify_diff(mo, mr, addrs) == 'real':
                where = ('an output buffer' if addrs[0] < STACK_BASE
                         else 'memory at 0x%08X' % addrs[0])
                return 'DIFF', 'seed %d: %s differs' % (s, where)
            rounding = rounding or s
    if rounding:
        return 'EQUIV-MODULO-FP', (
            '%d inputs agree except x87 intermediate rounding (first at seed %d); '
            'the interpreter has 64-bit registers where the hardware has 80-bit, '
            'so a last-place disagreement here is not evidence either way' %
            (seeds, rounding))
    return 'EQUIVALENT', '%d inputs agree (return + globals + side effects)' % seeds


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


def one(va, name, seeds, isolated=False):
    """Verdict for one function.

    By default the run happens inside the mapped reference image, so globals,
    constants and calls all work.  `isolated=True` selects the original
    behaviour -- no image, and any relocation is a refusal -- which is kept
    because it is a strictly stronger claim when it succeeds: the function is
    then proven to depend on nothing but its arguments.
    """
    va_hex = va if va.lower().startswith('0x') else '0x' + va
    va_int = int(va_hex, 16)
    orig = load_orig(va_hex)
    if orig is None:
        return 'UNCLASSIFIED', 'no original bytes'
    sig = parse_signature(name)
    if sig is None:
        return 'UNCLASSIFIED', 'signature not a plain cdecl of scalar/ptr args'
    if isolated:
        recomp, why = _recomp_for(va_hex, name)
        if recomp is None:
            return 'UNCLASSIFIED', why
        return verify(va_int, name, orig, recomp, seeds, sig)
    ent = _obj_index().get(name)
    if ent is None:
        return 'UNCLASSIFIED', 'no recompiled object found'
    obj, size = ent
    buried = ENV.shadows_a_neighbour(va_int, size)
    if buried:
        return 'UNCLASSIFIED', 'substituted bytes bury %s' % ', '.join(buried[:2])
    code, why = ENV.resolve_bytes(obj, name, va_int, size)
    if code is None:
        blocked = ENV.unresolved_symbols(obj, name, size)
        return 'UNCLASSIFIED', '%s: %s' % (why, ', '.join(blocked[:3]) or '?')
    return verify_img(va_int, name, orig, code, seeds, sig)


def t2_rows():
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    for r in csv.DictReader(open(rep)):
        if r.get('orig_size') and r.get('status') == 'diff':
            yield r['va'], r['name']


def certified_rows():
    """The rows already carrying an @t3 tag -- the set where a behavioural
    verdict is the claim being made, so this is the sweep that matters."""
    import subprocess
    vas = set(subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 't3.py'),
                              '--vas'], cwd=ROOT, capture_output=True,
                             text=True).stdout.split())
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    for r in csv.DictReader(open(rep)):
        if r['va'].lower() in vas:
            yield r['va'], r['name']


ORDER = ('EQUIVALENT', 'EQUIV-MODULO-FP', 'DIFF', 'UNCLASSIFIED')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va', nargs='?')
    ap.add_argument('--t2', action='store_true', help='sweep all T2 functions')
    ap.add_argument('--certified', action='store_true',
                    help='sweep the functions already carrying an @t3 tag')
    ap.add_argument('--isolated', action='store_true',
                    help='the image-free run: refuses any relocation, but when '
                         'it succeeds it proves the function depends on nothing '
                         'but its arguments')
    ap.add_argument('--reasons', action='store_true',
                    help='with a sweep, tally why functions were unclassified')
    ap.add_argument('--seeds', type=int, default=64)
    ap.add_argument('--name', default=None)
    a = ap.parse_args()

    if a.t2 or a.certified:
        import collections
        tally = collections.Counter()
        reasons = collections.Counter()
        out = collections.defaultdict(list)
        rows = certified_rows() if a.certified else t2_rows()
        for va, name in rows:
            verdict, detail = one(va, name, a.seeds, isolated=a.isolated)
            tally[verdict] += 1
            out[verdict].append((va, name, detail))
            if verdict == 'UNCLASSIFIED':
                reasons[detail.split(':')[0][:58]] += 1
        total = sum(tally.values())
        print('%s: %d functions' % ('certified @t3' if a.certified else 'T2 pile', total))
        for k in ORDER:
            print('  %-16s %3d' % (k, tally[k]))
        for k in ('EQUIVALENT', 'EQUIV-MODULO-FP'):
            if out[k]:
                print('\n-- %s --' % k)
                for va, name, _ in out[k]:
                    print('  %s %s' % (va, name))
        if out['DIFF']:
            print('\n-- DIFF (a real behavioural gap, read as a bug report) --')
            for va, name, d in out['DIFF'][:40]:
                print('  %s %s  %s' % (va, name, d))
        if a.reasons and reasons:
            print('\n-- why unclassified --')
            for k, n in reasons.most_common(12):
                print('  %3d  %s' % (n, k))
        return 0

    if not a.va:
        ap.error('give a VA, --t2 or --certified')
    name = a.name
    if not name:
        rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
        for r in csv.DictReader(open(rep)):
            if r['va'].lower() == a.va.lower():
                name = r['name']; break
    if not name:
        ap.error('no report.csv row for %s; pass --name' % a.va)
    verdict, detail = one(a.va, name, a.seeds, isolated=a.isolated)
    print('%s %s: %s (%s)' % (a.va, name, verdict, detail))
    return 0


if __name__ == '__main__':
    sys.exit(main())
