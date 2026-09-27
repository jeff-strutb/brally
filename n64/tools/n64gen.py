"""Turn Ghidra's T1 drafts into compilable candidates and grade them.

    .venv/bin/python n64/tools/n64gen.py 0x80200050          # one function
    .venv/bin/python n64/tools/n64gen.py --all --max-size 400  # a batch
    .venv/bin/python n64/tools/n64gen.py --all --report build/n64/gen.csv

Nothing here is project code.  Each candidate is written to
build/n64/cand/<VA>.c with every declaration it needs, compiled with the real
N64 flags and graded by n64build.grade() -- the same resolved-relocation check
as the T4 gate.  A candidate that grades EXACT is a match waiting to be filed:
someone still has to name it, say WHAT IT DOES, and move it into its module
(project rule 6).  The rest are drafts with a measured distance.

What the transform does to Ghidra's C:
  * Ghidra types -> C types (undefined4 -> int, ...)
  * FUN_/DAT_ -> func_/D_ (the names n64build resolves by address)
  * one extern per data symbol, typed by the widest load/store the ROM makes
    to that address (lb/lbu/lh/lhu/lw/lwc1 decide signedness and width)
  * a real prototype for every callee, taken from the callee's own draft --
    an unprototyped call would promote float arguments to double
"""
import argparse
import csv
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
sys.path.insert(0, os.path.join(N64, 'tools'))
import n64build as B  # noqa: E402

DRAFTS = os.path.join(B.OUT, 'ghidra')
CAND = os.path.join(B.OUT, 'cand')

TYPEMAP = [
    (r'\bundefined8\b', 'long long'), (r'\bundefined4\b', 'int'),
    (r'\bundefined2\b', 'short'), (r'\bundefined1\b', 'char'),
    (r'\bundefined\b', 'char'), (r'\buint\b', 'unsigned int'),
    (r'\bushort\b', 'unsigned short'), (r'\bbyte\b', 'unsigned char'),
    (r'\buchar\b', 'unsigned char'), (r'\bulonglong\b', 'unsigned long long'),
    (r'\blonglong\b', 'long long'), (r'\bbool\b', 'int'), (r'\bdword\b', 'unsigned int'),
    (r'\bword\b', 'unsigned short'), (r'\bcode\b', 'void'),
    (r'\btrue\b', '1'), (r'\bfalse\b', '0'),
]

# op -> (width, C type).  Stores do not tell signedness; loads do.
OPTYPE = {0x20: (1, 'signed char'), 0x24: (1, 'unsigned char'),
          0x21: (2, 'short'), 0x25: (2, 'unsigned short'),
          0x23: (4, 'int'), 0x31: (4, 'float'), 0x35: (8, 'double'),
          0x28: (1, 'char'), 0x29: (2, 'short'), 0x2B: (4, 'int'),
          0x39: (4, 'float'), 0x3D: (8, 'double')}
LOADS = {0x20, 0x24, 0x21, 0x25, 0x23, 0x31, 0x35}


def data_types(rom):
    """VA -> C type, from how the ROM's code touches it (lui/lo pairs)."""
    seen = {}
    lui = {}
    starts = set(B.function_map())
    for va in range(B.BASE, B.BASE + (B.TEXT_E - B.ROMOFF), 4):
        if va in starts:
            lui = {}
        w = rom.word(va)
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        imm = B.sext16(w & 0xffff)
        if op == 0x0F:
            lui[rt] = (w & 0xffff) << 16
            continue
        if op in OPTYPE and rs in lui:
            t = (lui[rs] + imm) & 0xffffffff
            seen.setdefault(t, []).append(op)
        if op in (0x09,) and rs in lui:
            t = (lui[rs] + imm) & 0xffffffff
            seen.setdefault(t, []).append('addr')
        if op in (0x20, 0x21, 0x23, 0x24, 0x25, 0x09, 0x0F) and rt in lui and op != 0x0F:
            lui.pop(rt, None)
    out = {}
    for t, ops in seen.items():
        real = [o for o in ops if o != 'addr']
        if not real:
            continue
        loads = [o for o in real if o in LOADS]
        pick = max(loads or real, key=lambda o: OPTYPE[o][0])
        out[t] = OPTYPE[pick][1]
    return out


def draft(va):
    p = os.path.join(DRAFTS, '0x%08X.c' % va)
    if not os.path.exists(p):
        return None
    return open(p).read()


SIG = re.compile(r'^([\w \*]+?)\s*\b(FUN_[0-9a-fA-F]{8}|\w+)\s*\(([^)]*)\)\s*$', re.M)


def signature(text):
    for line in text.split('\n'):
        line = line.strip()
        if not line or line.startswith('/*') or line.startswith('//'):
            continue
        m = SIG.match(line)
        if m:
            return m.group(1).strip(), m.group(3).strip()
        return None
    return None


def retype(s):
    for a, b in TYPEMAP:
        s = re.sub(a, b, s)
    s = re.sub(r'\bFUN_([0-9a-fA-F]{8})\b', lambda m: 'func_' + m.group(1).upper(), s)
    s = re.sub(r'\bDAT_([0-9a-fA-F]{8})\b', lambda m: 'D_' + m.group(1).upper(), s)
    s = re.sub(r'\b_DAT_([0-9a-fA-F]{8})\b', lambda m: 'D_' + m.group(1).upper(), s)
    s = re.sub(r'\b[a-z]*Ram([0-9a-fA-F]{8})\b', lambda m: 'D_' + m.group(1).upper(), s)
    s = re.sub(r'\bPTR_\w*?_([0-9a-fA-F]{8})\b', lambda m: 'P_' + m.group(1).upper(), s)
    return s


_protos = {}


def prototype(va):
    if va in _protos:
        return _protos[va]
    t = draft(va)
    sig = signature(t) if t else None
    if sig is None:
        _protos[va] = 'int func_%08X();' % va
    else:
        ret, params = sig
        ret = retype(ret)
        params = retype(params)
        if params in ('', 'void'):
            params = 'void'
        _protos[va] = '%s func_%08X(%s);' % (ret, va, params)
    return _protos[va]


_rom = None


def c_string(va):
    """The NUL-terminated string at va in the ROM, as a C literal."""
    out = []
    while True:
        c = _rom.bytes(va, 1)[0]
        if c == 0 or len(out) > 400:
            break
        out.append(c)
        va += 1
    esc = {9: '\\t', 10: '\\n', 13: '\\r', 34: '\\"', 92: '\\\\'}
    return '"' + ''.join(esc.get(c, chr(c) if 32 <= c < 127 else '\\%03o' % c)
                         for c in out) + '"'


SYMLO, SYMHI = B.BASE, B.BSS_E          # addresses that are link-time symbols


def toggle(body, word):
    """Flip the signedness of every `word` (short/char) in body."""
    body = re.sub(r'\bunsigned %s\b' % word, '@@U@@', body)
    body = re.sub(r'(?<!signed )\b%s\b' % word, 'unsigned ' + word, body)
    return body.replace('@@U@@', word)


FLOAT_LIT = re.compile(r'(?<![\w.])(\d+\.\d*(?:[eE][-+]?\d+)?|\d+[eE][-+]?\d+)(?![\w.])')
STRING = re.compile(r'"(?:\\.|[^"\\])*"')


def fsuffix(body):
    """Ghidra prints every float constant as a double literal (`0.0`); the
    ROM decides which it was -- a single compare needs `0.0f`."""
    out, pos = [], 0
    for m in STRING.finditer(body):
        out.append(FLOAT_LIT.sub(r'\1f', body[pos:m.start()]))
        out.append(m.group(0))
        pos = m.end()
    out.append(FLOAT_LIT.sub(r'\1f', body[pos:]))
    return ''.join(out)


_DL_A = re.compile(r'(\w+) = (D_[0-9A-F]{8});\s*(\w+) = \2 \+ 2;\s*\*\2 = ([^;]+);\s*\2 = \3;\s*\1\[1\] = ([^;]+);', re.S)
_DL_B = re.compile(r'(\w+) = (D_[0-9A-F]{8});\s*(\w+) = (?:\2 \+ 1|1 \+ \2);\s*\2 = \2 \+ 2;\s*\*\3 = ([^;]+);\s*\*\1 = ([^;]+);', re.S)


def gbi(body):
    """Ghidra's view of a display-list macro -- the head pointer copied,
    bumped by one command, the two words stored -- becomes gRaw(head++, w0,
    w1), the shape every libultra gDP/gSP macro expands to."""
    heads = set()

    def a(m):
        heads.add(m.group(2))
        return 'gRaw(%s++, %s, %s);' % (m.group(2), m.group(4).strip(), m.group(5).strip())

    def b(m):
        heads.add(m.group(2))
        return 'gRaw(%s++, %s, %s);' % (m.group(2), m.group(5).strip(), m.group(4).strip())
    prev = None
    while prev != body:
        prev = body
        body = _DL_A.sub(a, body)
        body = _DL_B.sub(b, body)
    return body, heads


def candidate(va, dtypes, name=None, noproto=(), ptrs=(), opts=()):
    t = draft(va)
    if t is None:
        return None
    body = retype(t)
    # A bare number inside the image is a symbol the linker filled in; one
    # outside it is a fixed address the source wrote as a number.
    body = re.sub(r'\b0x(80[0-9a-fA-F]{6})\b',
                  lambda m: ('(&D_%s)' % m.group(1).upper()
                             if SYMLO <= int(m.group(1), 16) < SYMHI else m.group(0)), body)
    linked = set()
    if 'lowsym' in opts:
        # a global below the image (the low-RAM data at 0x80025C00..) is
        # a linked variable, not a fixed address: the ROM schedules its
        # lui like any other symbol's
        linked |= set(int(x, 16) for x in re.findall(r'\bD_(80[01][0-9A-F]{5})\b', body))
    if 'negsym' in opts:
        linked |= set(0x100000000 - int(x, 16)
                       for x in re.findall(r'(?<![\w.])-\s*0x(7f[0-9a-fA-F]{6})\b', body))
        # Ghidra prints a RAM address it could not tie to a symbol as a
        # negative int (-0x7fc38000 == 0x803C8000); the ROM's lui/addiu pair
        # says it was a linked address
        body = re.sub(r'(?<![\w.])-\s*0x(7f[0-9a-fA-F]{6})\b',
                      lambda m: '(int)&D_%08X' % (0x100000000 - int(m.group(1), 16)), body)
    if 'short' in opts:
        body = toggle(body, 'short')
    if 'char' in opts:
        body = toggle(body, 'char')
    if 'fsuf' in opts:
        body = fsuffix(body)
    dlheads = set()
    if 'gbi' in opts:
        body, dlheads = gbi(body)
    for k in range(1, 5):
        if 'params%d' % k in opts:
            body = re.sub(r'^(\w[\w \*]*\b%s\s*)\(void\)' % ('func_%08X' % va),
                          lambda m: m.group(1) + '(' + ','.join('int arg%d' % i for i in range(k)) + ')',
                          body, count=1, flags=re.M)
    body = re.sub(r'\bs_\w*?_([0-9a-fA-F]{8})\b',
                  lambda m: c_string(int(m.group(1), 16)), body)
    body = re.sub(r'^/\*.*?\*/\s*', '', body, flags=re.S)
    fname = name or ('func_%08X' % va)
    body = re.sub(r'\bfunc_%08X\b' % va, fname, body, count=1)
    decls = ['#include "tgr/common.h"', '#include "tgr/gbi.h"', '']
    callees = sorted(set(int(x, 16) for x in re.findall(r'\bfunc_([0-9A-F]{8})\b', body)) - {va})
    for c in callees:
        if c in noproto:
            decls.append(prototype(c).split('(')[0] + '();')
        else:
            decls.append(prototype(c))
    # Ghidra leaves a global it only ever takes the address of as a one-byte
    # `undefined`, so `&DAT_x + n*K` in a draft is BYTE arithmetic; typed by
    # its widest load it would scale by 4.
    bytewise = set()
    if 'bytes' in opts:
        bytewise = set(int(x, 16) for x in re.findall(r'&D_([0-9A-F]{8})\s*[-+]', body))
    for d in sorted(set(int(x, 16) for x in re.findall(r'\bD_([0-9A-F]{8})\b', body))):
        ty = 'char *' if d in ptrs else dtypes.get(d, 'int')
        if 'D_%08X' % d in dlheads:
            decls.append('extern Gfx *D_%08X;' % d)
            continue
        if d in bytewise and SYMLO <= d < SYMHI:
            body = re.sub(r'(?<!&)\bD_%08X\b' % d, '(*(%s *)&D_%08X)' % (ty, d), body)
            decls.append('extern char D_%08X;' % d)
            continue
        if not (SYMLO <= d < SYMHI) and d not in linked:
            body = re.sub(r'&D_%08X\b' % d, '((%s *)0x%08X)' % (ty, d), body)
            body = re.sub(r'\bD_%08X\b' % d, '(*(%s *)0x%08X)' % (ty, d), body)
            continue
        decls.append('extern %s D_%08X;' % (ty, d))
    for d in sorted(set(int(x, 16) for x in re.findall(r'\bP_([0-9A-F]{8})\b', body))):
        decls.append('extern char *P_%08X;' % d)
        body = body.replace('P_%08X' % d, 'D_%08X' % d)
        decls[-1] = 'extern char *D_%08X;' % d
    decls.append('')
    decls.append('/* @implements 0x%08X tgr %s */' % (va, fname))
    return '\n'.join(decls) + '\n' + body


def grade_candidate(va, src, rom, fmap, syms):
    os.makedirs(CAND, exist_ok=True)
    p = os.path.join(CAND, '0x%08X.c' % va)
    open(p, 'w').write(src)
    obj, err = B.compile_c(p)
    if obj is None:
        return 'CCFAIL', None, err.strip().split('\n')[0][:200]
    pieces = {n: (s, e) for n, s, e in B.carve(obj) if n}
    name = re.search(r'@implements 0x[0-9A-F]{8} tgr (\w+)', src).group(1)
    if name not in pieces:
        return 'NOFN', None, ''
    s, e = pieces[name]
    st, nd, notes, _, _ = B.grade(obj, rom, name, s, e, va, fmap[va], syms, {name: va})
    return st, nd, '; '.join(notes)[:200]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('vas', nargs='*')
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--max-size', type=int, default=10 ** 9)
    ap.add_argument('--report', help='default build/n64/gen.csv for --all; a single-VA run writes none')
    a = ap.parse_args()
    global _rom
    rom, fmap, syms = B.Rom(), B.function_map(), B.load_symbols()
    _rom = rom
    dtypes = data_types(rom)
    done = set()
    vpath = os.path.join(B.OUT, 'verify.csv')
    if os.path.exists(vpath):
        done = {int(r['va'], 16) for r in csv.DictReader(open(vpath)) if r['va']}
    vas = [int(v, 16) for v in a.vas]
    if a.all:
        vas = [v for v in sorted(fmap) if fmap[v] <= a.max_size and v not in done]
    rows = []
    for va in vas:
        src = candidate(va, dtypes)
        if src is None:
            rows.append((va, fmap[va], 'NODRAFT', '', ''))
            continue
        noproto, ptrs = set(), set()
        for attempt in range(6):
            st, nd, note = grade_candidate(va, src, rom, fmap, syms)
            if st != 'CCFAIL':
                break
            m = re.search(r'line (\d+)', note)
            line = src.split('\n')[int(m.group(1)) - 1] if m else ''
            changed = False
            if "number of arguments" in note or 'incompatible with type' in note:
                for c in re.findall(r'func_([0-9A-F]{8})', line) or re.findall(r'func_([0-9A-F]{8})', src):
                    if int(c, 16) not in noproto:
                        noproto.add(int(c, 16)); changed = True
            if any(k in note for k in ('non-pointer', 'non-array', 'Selector', 'non-scalar',
                                        'void type', 'operand of')):
                for d in re.findall(r'D_([0-9A-F]{8})', line):
                    if int(d, 16) not in ptrs:
                        ptrs.add(int(d, 16)); changed = True
            if not changed:
                break
            src = candidate(va, dtypes, noproto=noproto, ptrs=ptrs)
        if st == 'DIFF':
            # try the known near-miss classes and keep whatever helps
            best = (nd, src, st, note, ())
            rw = B.rom_body(rom, va, fmap[va])
            homes = {(w >> 16) & 31 for w in rw
                     if w >> 26 == 0x2B and (w >> 21) & 31 == 29 and 4 <= (w >> 16) & 31 <= 7}
            base_opts = []
            if any((w >> 26) in (0x21, 0x25) for w in rw):
                base_opts.append('short')
            if any((w >> 26) in (0x20, 0x24) for w in rw):
                base_opts.append('char')
            if homes:
                base_opts.append('params%d' % (max(homes) - 3))
            if any(w >> 26 == 0x11 and (w >> 21) & 31 == 0x10 for w in rw) and FLOAT_LIT.search(src):
                base_opts.append('fsuf')
            if re.search(r'&D_[0-9A-F]{8}\s*[-+]', src):
                base_opts.append('bytes')
            if re.search(r'-\s*0x7f[0-9a-fA-F]{6}\b', src):
                base_opts.append('negsym')
            if re.search(r'0x80[01][0-9a-fA-F]{5}\b', src):
                base_opts.append('lowsym')
            if gbi(src)[1]:
                base_opts.append('gbi')
            for _ in range(2):
                improved = False
                for o in base_opts:
                    cur = best[4]
                    trial = tuple(sorted(set(cur) ^ {o}))
                    src2 = candidate(va, dtypes, noproto=noproto, ptrs=ptrs, opts=trial)
                    st2, nd2, note2 = grade_candidate(va, src2, rom, fmap, syms)
                    if st2 in ('EXACT', 'DIFF') and (st2 == 'EXACT' or nd2 < best[0]):
                        best = (0 if st2 == 'EXACT' else nd2, src2, st2, note2, trial)
                        improved = True
                        if st2 == 'EXACT':
                            break
                if not improved or best[2] == 'EXACT':
                    break
            nd, src, st, note = best[0], best[1], best[2], best[3]
            grade_candidate(va, src, rom, fmap, syms)      # leave the best on disk
        rows.append((va, fmap[va], st, nd, note))
        if len(vas) == 1:
            print(src)
    report = a.report or (os.path.join(B.OUT, 'gen.csv') if a.all else None)
    with open(report or os.devnull, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['va', 'size', 'status', 'ndiff', 'note'])
        for r in rows:
            w.writerow(['%08X' % r[0]] + list(r[1:]))
    c = {}
    for r in rows:
        c[r[2]] = c.get(r[2], 0) + 1
    for r in rows[:60] if len(rows) <= 60 else []:
        print('%08X %5d %-7s %4s %s' % r)
    print(' '.join('%s %d' % kv for kv in sorted(c.items())),
          ' EXACT bytes %d' % sum(r[1] for r in rows if r[2] == 'EXACT'))


if __name__ == '__main__':
    main()
