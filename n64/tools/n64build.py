"""Build the Top Gear Rally decomp and grade every function against the ROM.

    .venv/bin/python n64/tools/n64build.py                 # every file under n64/src
    .venv/bin/python n64/tools/n64build.py n64/src/geometry/vec.c
    .venv/bin/python n64/tools/n64build.py --diff 0x802248C8

This is the N64 lane's T4 gate -- the counterpart of the PC sweep.  Each .c
under n64/src is compiled with IDO, every function in the object is carved out
and matched to its `@implements` tag, and the body is compared against the ROM
WORD FOR WORD WITH EVERY RELOCATION RESOLVED.  Nothing is masked:

  jal / j         the callee must resolve (by name) to the address the ROM calls
  %hi / %lo       the full 32-bit address must equal the ROM's lui/lo pair
  named symbols   resolved through n64/config/symbols_tgr.csv, or the name
                  itself (func_80XXXXXX / D_80XXXXXX)
  section-local   .rodata/.data literals (strings, float constants, jump
                  tables) cannot have a symbol, so the ROM's own address is
                  decoded and the BYTES there must equal the object's -- a jump
                  table's entries must point at the matching code in the ROM

A function whose every word is equal is EXACT (T4).  Anything else is DIFF with
a count, and --diff prints the two side by side.

Rules the builder enforces (they keep grading sound):
  * no `static` functions -- IDO leaves no symbol for them, so the carve would
    be guesswork.  A function that was static in the original is written
    global; IDO -O2 emits the same body either way.
  * one `@implements 0x80XXXXXX tgr Name` per function; the name must be the
    C function's name.
  * `.bss` referenced through a section (a file-static array) cannot be
    pinned; declare it `extern` under its ROM name instead.

Per-file compiler flags: a `/* n64-cflags: -O1 */` line in the file's first
40 lines replaces the default optimisation flags.
"""
import argparse
import csv
import os
import re
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
ROM_PATH = os.environ.get('TGR_ROM', os.path.join(ROOT, 'reference/tgrally/Top Gear Rally (USA).z64'))
CC = os.environ.get('TGR_CC', os.path.join(ROOT, 'tools/ido53/cc'))
SYMS = os.path.join(N64, 'config/symbols_tgr.csv')
OUT = os.path.join(ROOT, 'build/n64')

BASE, ROMOFF = 0x80200000, 0x1000
TEXT_S, TEXT_E = 0x1000, 0x70AB0            # ROM offsets of .text
DATA_E = 0xAD400                            # end of .data/.rodata in ROM
BSS_S, BSS_E = 0x802AC400, 0x802AC400 + 0xD67B0

BASE_FLAGS = ['-c', '-mips2', '-non_shared', '-G', '0', '-w', '-Xfullwarn',
              '-Wab,-r4300_mul',          # the VR4300 multiply-errata nops
              '-I' + os.path.join(N64, 'include')]
DEFAULT_OPT = ['-O2']

TAG = re.compile(r'@implements\s+(0x[0-9A-Fa-f]{8})\s+tgr\s+(\w+)')
T3TAG = re.compile(r'@t3\s+(0x[0-9A-Fa-f]{8})')
STATIC_FN = re.compile(r'^\s*static\s+[^;=]*\([^;]*\)\s*$', re.M)


# ------------------------------------------------------------------- the ROM
class Rom:
    def __init__(self, path=ROM_PATH):
        self.d = open(path, 'rb').read()

    def word(self, va):
        return struct.unpack_from('>I', self.d, self.v2r(va))[0]

    def bytes(self, va, n):
        r = self.v2r(va)
        return self.d[r:r + n]

    @staticmethod
    def v2r(va):
        return va - BASE + ROMOFF

    @staticmethod
    def in_image(va):
        return BASE <= va < BASE + (DATA_E - ROMOFF)


def function_map():
    """ROM function starts, as n64rom.py derives them: jal targets and the
    first word after every `jr ra` + delay slot."""
    sys.path.insert(0, os.path.join(N64, 'tools'))
    import n64rom
    end = n64rom.r2v(n64rom.TEXT_E)
    F = n64rom.F
    return {F[i]: (F[i + 1] if i + 1 < len(F) else end) - F[i] for i in range(len(F))}


def rom_body(rom, va, size):
    ws = [rom.word(va + i) for i in range(0, size, 4)]
    while ws and ws[-1] == 0:
        ws.pop()
    return ws


# --------------------------------------------------------------- symbol table
def load_symbols():
    """name -> VA.  n64/config/symbols_tgr.csv is the authority; the
    address-bearing names (func_/D_) resolve without an entry."""
    out = {}
    if os.path.exists(SYMS):
        for r in csv.DictReader(open(SYMS)):
            out[r['name']] = int(r['va'], 16)
    return out


ADDR_NAME = re.compile(r'^(?:func|D|jtbl|L)_([0-9A-Fa-f]{8})$')


def resolve(name, syms):
    if name in syms:
        return syms[name]
    m = ADDR_NAME.match(name)
    if m:
        return int(m.group(1), 16)
    return None


# ------------------------------------------------------------------ ELF32 BE
class Obj:
    """Just enough of an IDO relocatable object."""

    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        shoff, = struct.unpack_from('>I', d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from('>HHH', d, 0x2e)
        self.secs = []
        for i in range(shnum):
            f = struct.unpack_from('>10I', d, shoff + i * shentsize)
            self.secs.append(dict(name=f[0], type=f[1], off=f[4], size=f[5],
                                  link=f[6], info=f[7]))
        st = self.secs[shstrndx]
        for s in self.secs:
            s['name'] = self._str(st, s['name'])
        self.byname = {s['name']: i for i, s in enumerate(self.secs)}
        self.syms = []
        for s in self.secs:
            if s['type'] == 2:
                ss = self.secs[s['link']]
                for k in range(s['size'] // 16):
                    nm, val, size, info, other, shndx = struct.unpack_from(
                        '>IIIBBH', d, s['off'] + k * 16)
                    self.syms.append(dict(name=self._str(ss, nm), value=val,
                                          size=size, bind=info >> 4,
                                          type=info & 0xf, shndx=shndx))
        self.rels = {}                      # target section index -> [(off, type, symidx)]
        for s in self.secs:
            if s['type'] == 9:
                lst = self.rels.setdefault(s['info'], [])
                for k in range(s['size'] // 8):
                    o, inf = struct.unpack_from('>II', d, s['off'] + k * 8)
                    lst.append((o, inf & 0xff, inf >> 8))

    def _str(self, sec, o):
        b = sec['off'] + o
        return self.d[b:self.d.index(b'\0', b)].decode('latin1')

    def sec(self, name):
        i = self.byname.get(name)
        if i is None:
            return None, b''
        s = self.secs[i]
        if s['type'] == 8:
            return i, b'\0' * s['size']
        return i, self.d[s['off']:s['off'] + s['size']]


def words(b):
    return [struct.unpack_from('>I', b, i)[0] for i in range(0, len(b) - 3, 4)]


def carve(obj):
    """-> [(name, start, end)] over .text, one per FUNC symbol.

    Every function in n64/src is global (IDO writes no symbol for a static
    one; build_file refuses `static` definitions), so the symbol table is the
    whole truth.  A function with an early return has two `jr ra`, so the
    ROM map's jr-ra rule cannot be used here."""
    ti, text = obj.sec('.text')
    if ti is None:
        return []
    named = sorted((s['value'], s['name']) for s in obj.syms
                   if s['shndx'] == ti and s['type'] == 2)
    out = []
    for k, (v, n) in enumerate(named):
        e = named[k + 1][0] if k + 1 < len(named) else len(text)
        out.append((n, v, e))
    if named and named[0][0] != 0:
        out.insert(0, (None, 0, named[0][0]))
    return out


# -------------------------------------------------------------------- grading
_REFS = None


def rom_refs():
    """ROM address -> {function VAs that reach it by a %hi/%lo pair}."""
    global _REFS
    if _REFS is None:
        _REFS = {}
        rom = Rom()
        for va, size in function_map().items():
            hi = {}
            for a in range(va, va + size, 4):
                w = rom.word(a)
                op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
                if op == 0x0F:
                    hi[rt] = (w & 0xffff) << 16
                    continue
                if rs in hi and op in (0x09, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B,
                                       0x31, 0x35, 0x39, 0x3D):
                    _REFS.setdefault((hi[rs] + sext16(w & 0xffff)) & 0xffffffff, set()).add(va)
    return _REFS


def sext16(x):
    return x - 0x10000 if x & 0x8000 else x


def grade(obj, rom, name, start, end, va, rom_size, syms, fnvas):
    """Compare one carved function with the ROM at va.
    -> (status, ndiff, notes, ours_resolved, theirs)."""
    ti, text = obj.sec('.text')
    ours = words(text[start:end])
    while ours and ours[-1] == 0:
        ours.pop()
    theirs = rom_body(rom, va, rom_size)
    notes = []
    rels = [(o, t, si) for o, t, si in obj.rels.get(ti, []) if start <= o < end]
    allrels = obj.rels.get(ti, [])
    fixed = list(ours)

    def sym_target(si, rel_off, typ):
        """Resolved address of symbol si, or None + note."""
        s = obj.syms[si]
        if s['type'] == 3:                                  # section symbol
            secname = obj.secs[s['shndx']]['name']
            return ('section', secname)
        if s['shndx'] == ti and s['name']:                  # defined in this obj
            v = fnvas.get(s['name'])
            if v is None:
                v = resolve(s['name'], syms)
            if v is None:
                return ('unresolved', s['name'])
            return ('addr', v)          # relocated against the symbol itself
        v = resolve(s['name'], syms)
        if v is None:
            return ('unresolved', s['name'])
        return ('addr', v)

    def text_off_to_va(off):
        """.text offset in our object -> ROM VA via the carved functions."""
        for fname, s, e in carve(obj):
            if s <= off < e and fname in fnvas:
                return fnvas[fname] + (off - s)
        return None

    def rom_pair_addr(i_hi, lo_i):
        hi = theirs[i_hi] & 0xffff if i_hi < len(theirs) else 0
        lo = theirs[lo_i] & 0xffff if lo_i < len(theirs) else 0
        return ((hi << 16) + sext16(lo)) & 0xffffffff

    LOAD_WIDTH = {0x20: 1, 0x24: 1, 0x28: 1, 0x21: 2, 0x25: 2, 0x29: 2, 0x23: 4, 0x2B: 4,
                  0x31: 4, 0x39: 4, 0x35: 8, 0x3D: 8}

    def check_section_bytes(secname, addend, rom_va, width=None):
        """A section-local literal: the ROM bytes at rom_va must equal ours.
        A literal reached by a load or store is compared at that access's
        width, so the section's own alignment padding after the last literal
        is not mistaken for data."""
        si, blob = obj.sec(secname) if secname in obj.byname else (None, b'')
        if secname == '.bss' or secname.startswith('.sbss'):
            # a static with no initialiser: nothing to compare, so prove it
            # is private to this file instead -- it lands in the ROM's .bss,
            # every .bss static of this object lands at one base, and no
            # function outside this file touches that address
            if not BSS_S <= rom_va < BSS_E:
                return 'file-static %s+0x%X -> %08X is not in the ROM .bss' % (secname, addend, rom_va)
            base = obj.__dict__.setdefault('bss_base', {}).setdefault(secname, rom_va - addend)
            if rom_va - addend != base:
                return 'file-static %s+0x%X -> %08X: this object\'s other .bss statics sit at base %08X' % (
                    secname, addend, rom_va, base)
            outside = sorted(rom_refs().get(rom_va, set()) - set(fnvas.values()))
            if outside:
                return 'file-static %s+0x%X -> %08X is also used by %s, outside this file' % (
                    secname, addend, rom_va, ', '.join('%08X' % v for v in outside[:3]))
            return None
        if not rom.in_image(rom_va):
            return '%s+0x%X -> %08X is outside the ROM image' % (secname, addend, rom_va)
        # extent: up to the next referenced offset in that section, else end
        cut = len(blob)
        for (o, t, sj) in allrels:
            ss = obj.syms[sj]
            if ss['type'] == 3 and obj.secs[ss['shndx']]['name'] == secname and t == 6:
                a = sext16(words(text[o:o + 4])[0] & 0xffff)
                if addend < a < cut:
                    cut = a
        if width:
            cut = min(cut, addend + width)
        seg = blob[addend:cut]
        # R_MIPS_32 relocs inside [addend, cut): jump tables (against .text)
        # and pointer initialisers (against a symbol)
        jt = {o: sj for (o, t, sj) in obj.rels.get(si, []) if t == 2 and addend <= o < cut}
        mism = 0
        for k in range(0, len(seg) - 3 if jt else 0, 4):
            if addend + k in jt:
                tgt = struct.unpack_from('>I', seg, k)[0]
                ss = obj.syms[jt[addend + k]]
                if ss['type'] == 3 and obj.secs[ss['shndx']]['name'] == '.text':
                    want = text_off_to_va(tgt)
                elif ss['type'] != 3 and ss['name']:
                    # a pointer initialiser: the symbol's address plus the
                    # addend stored in place
                    v = fnvas.get(ss['name'], resolve(ss['name'], syms))
                    want = None if v is None else (v + tgt) & 0xffffffff
                else:
                    want = None
                got = struct.unpack_from('>I', rom.bytes(rom_va + k, 4), 0)[0]
                if want is None or want != got:
                    mism += 1
            else:
                if seg[k:k + 4] != rom.bytes(rom_va + k, 4):
                    mism += 1
        if not jt:
            # strings: compare only through the first NUL when it looks textual
            if secname == '.rodata' and b'\0' in seg and all(32 <= c < 127 or c in (9, 10, 13) for c in seg[:seg.index(b'\0')]) and seg.index(b'\0') > 0:
                seg = seg[:seg.index(b'\0') + 1]
            if seg != rom.bytes(rom_va, len(seg)):
                mism += 1
        if mism:
            return '%s+0x%X -> %08X: %d literal word(s) differ from the ROM' % (secname, addend, rom_va, mism)
        return None

    for k, (o, typ, si) in enumerate(rels):
        i = (o - start) // 4
        if i >= len(fixed):
            continue
        w = ours[i]
        kind, val = sym_target(si, o, typ)
        if kind == 'unresolved':
            notes.append('unresolved symbol %s' % val)
            continue
        if typ == 4:                                        # R_MIPS_26
            if kind == 'section':
                tgt = text_off_to_va((w & 0x03FFFFFF) << 2)
                if tgt is None:
                    notes.append('jal into an untagged piece of .text')
                    continue
            else:
                tgt = val + ((w & 0x03FFFFFF) << 2)
            fixed[i] = (w & 0xFC000000) | ((tgt >> 2) & 0x03FFFFFF)
        elif typ in (5, 6):                                 # HI16 / LO16
            # pair: a HI's addend comes from the next LO against the same symbol
            if typ == 5:
                lo_o = next((o2 for (o2, t2, s2) in allrels
                             if o2 > o and t2 == 6 and s2 == si), None)
                if lo_o is None:
                    notes.append('HI16 at +0x%X without a LO16' % (o - start))
                    continue
                lo_w = words(text[lo_o:lo_o + 4])[0]
                addend = ((w & 0xffff) << 16) + sext16(lo_w & 0xffff)
                lo_i = (lo_o - start) // 4
            else:
                hi_o = max((o2 for (o2, t2, s2) in allrels
                            if o2 < o and t2 == 5 and s2 == si), default=None)
                hi_w = words(text[hi_o:hi_o + 4])[0] if hi_o is not None else 0
                addend = ((hi_w & 0xffff) << 16) + sext16(w & 0xffff)
            if kind == 'section':
                if val == '.text':
                    tgt = text_off_to_va(addend)
                    if tgt is None:
                        notes.append('%%hi/%%lo into untagged .text')
                        continue
                else:
                    # decode what the ROM points at, then prove the bytes there
                    if typ == 5:
                        rom_va = rom_pair_addr(i, lo_i)
                    else:
                        hi_i = (hi_o - start) // 4 if hi_o is not None else None
                        rom_va = rom_pair_addr(hi_i, i) if hi_i is not None else None
                    if rom_va is None:
                        notes.append('LO16 without HI16')
                        continue
                    lo_w = words(text[(start + 4 * (lo_i if typ == 5 else i)):(start + 4 * (lo_i if typ == 5 else i)) + 4])[0] \
                        if (lo_i if typ == 5 else i) is not None else 0
                    bad = check_section_bytes(val, addend, rom_va, LOAD_WIDTH.get(lo_w >> 26))
                    if bad:
                        notes.append(bad)
                        continue
                    tgt = rom_va
            else:
                tgt = (val + addend) & 0xffffffff
            if typ == 5:
                fixed[i] = (w & 0xFFFF0000) | (((tgt + 0x8000) >> 16) & 0xffff)
            else:
                fixed[i] = (w & 0xFFFF0000) | (tgt & 0xffff)
        elif typ == 2:
            notes.append('R_MIPS_32 inside .text')
        else:
            notes.append('relocation type %d not handled' % typ)

    n = max(len(fixed), len(theirs))
    ndiff = sum(1 for k in range(n)
                if k >= len(fixed) or k >= len(theirs) or fixed[k] != theirs[k])
    if len(fixed) != len(theirs):
        notes.append('size %d vs ROM %d bytes' % (len(fixed) * 4, len(theirs) * 4))
    status = 'EXACT' if ndiff == 0 and not notes else 'DIFF'
    return status, ndiff, notes, fixed, theirs


# ---------------------------------------------------------------- compiling
def cflags_for(src_text):
    m = re.search(r'n64-cflags:\s*([^*]+)\*/', '\n'.join(src_text.split('\n')[:40]))
    return m.group(1).split() if m else DEFAULT_OPT


def compile_c(path, extra=()):
    src = open(path, encoding='utf-8', errors='replace').read()
    o = tempfile.NamedTemporaryFile(suffix='.o', delete=False).name
    flags = cflags_for(src)
    base = [f for f in BASE_FLAGS if f != '-mips2'] if '-mips3' in flags else BASE_FLAGS
    cmd = [CC] + base + flags + list(extra) + ['-o', o, path]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode:
        os.unlink(o)
        return None, (p.stderr or p.stdout)
    obj = Obj(o)
    os.unlink(o)
    return obj, None


def static_bases(obj, fnvas, rom=None, fmap=None):
    """-> {section: ROM VA} for this object's .data/.bss (its file statics).

    A static is referenced through its section symbol plus an addend, so its
    ROM home is the section's base plus that addend.  Every %hi/%lo pair
    against the section in a tagged function votes for a base: where our
    `lui` has the same destination as the ROM's word at the same index and
    the paired low instruction has the same opcode and base register, the
    ROM's pair decodes the static's address.  The base with the most votes
    wins (a T2 body whose code has shifted votes at random and is outvoted by
    the aligned references)."""
    rom = rom or Rom()
    fmap = fmap or function_map()
    ti, text = obj.sec('.text')
    allrels = obj.rels.get(ti, [])
    votes = {}
    for fname, s, e in carve(obj):
        va = fnvas.get(fname)
        if va is None or va not in fmap:
            continue
        theirs = rom_body(rom, va, fmap[va])
        for o, typ, si in allrels:
            if not (s <= o < e) or typ != 5:
                continue
            sym = obj.syms[si]
            if sym['type'] != 3:
                continue
            sec = obj.secs[sym['shndx']]['name']
            if sec not in ('.data', '.bss', '.sdata', '.sbss'):
                continue
            lo_o = next((o2 for (o2, t2, s2) in allrels if o2 > o and t2 == 6 and s2 == si), None)
            if lo_o is None:
                continue
            i, li = (o - s) // 4, (lo_o - s) // 4
            if li >= len(theirs):
                continue
            hw, lw = words(text[o:o + 4])[0], words(text[lo_o:lo_o + 4])[0]
            rh, rl = theirs[i], theirs[li]
            if rh >> 16 != hw >> 16 or (rl >> 21) != (lw >> 21):
                continue
            addend = ((hw & 0xffff) << 16) + sext16(lw & 0xffff)
            rom_va = (((rh & 0xffff) << 16) + sext16(rl & 0xffff)) & 0xffffffff
            key = (sec, (rom_va - addend) & 0xffffffff)
            votes[key] = votes.get(key, 0) + 1
    out = {}
    for (sec, base), n in sorted(votes.items(), key=lambda kv: -kv[1]):
        out.setdefault(sec, base)
    return out


def tags_in(src):
    """-> [(va, name, line)] for every @implements in the file."""
    out = []
    for i, l in enumerate(src.split('\n')):
        m = TAG.search(l)
        if m:
            out.append((int(m.group(1), 16), m.group(2), i + 1))
    return out


def build_file(path, rom, fmap, syms, want_diff=None):
    src = open(path, encoding='utf-8', errors='replace').read()
    rel = os.path.relpath(path, ROOT)
    rows = []
    obj, err = compile_c(path)
    tags = tags_in(src)
    if obj is None:
        for va, name, ln in tags:
            rows.append(dict(va='%08X' % va, name=name, file=rel, status='CCFAIL',
                             ndiff='', size=fmap.get(va, 0), note=err.strip().split('\n')[0][:160]))
        return rows
    pieces = carve(obj)
    for m in re.finditer(r'^static\s+[^;=]*?\b(\w+)\s*\([^;]*?\)\s*\{', src, re.M):
        rows.append(dict(va='', name=m.group(1), file=rel, status='ERROR', ndiff='', size=0,
                         note='static function %s: write it global (IDO leaves no symbol)' % m.group(1)))
    fnvas = {name: va for va, name, ln in tags}
    have = {p[0]: p for p in pieces if p[0]}
    for p in pieces:
        if p[0] is None:
            rows.append(dict(va='', name='', file=rel, status='ERROR', ndiff='', size=0,
                             note='unnamed piece at .text+0x%X: a static function? '
                                  'write it global' % p[1]))
    for va, name, ln in tags:
        if va not in fmap:
            rows.append(dict(va='%08X' % va, name=name, file=rel, status='ERROR', ndiff='',
                             size=0, note='%08X is not a function start in the ROM map' % va))
            continue
        if name not in have:
            rows.append(dict(va='%08X' % va, name=name, file=rel, status='ERROR', ndiff='',
                             size=fmap[va], note='tag names %s, which the object does not define' % name))
            continue
        _, s, e = have[name]
        st, nd, notes, ours, theirs = grade(obj, rom, name, s, e, va, fmap[va], syms, fnvas)
        rows.append(dict(va='%08X' % va, name=name, file=rel, status=st, ndiff=nd,
                         size=fmap[va], note='; '.join(notes)[:300]))
        if want_diff is not None and want_diff == va:
            print_diff(va, ours, theirs)
    for name, p in have.items():
        if name not in fnvas:
            rows.append(dict(va='', name=name, file=rel, status='UNTAGGED', ndiff='',
                             size=p[2] - p[1], note='defined but carries no @implements tag'))
    return rows


def print_diff(va, ours, theirs):
    from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_BIG_ENDIAN
    md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)

    def dis(w, a):
        for i in md.disasm(struct.pack('>I', w), a):
            return '%-7s %s' % (i.mnemonic, i.op_str)
        return '.word 0x%08X' % w
    n = max(len(ours), len(theirs))
    for k in range(n):
        a = va + 4 * k
        o = ours[k] if k < len(ours) else None
        t = theirs[k] if k < len(theirs) else None
        mark = '  ' if o == t else '!!'
        print('%08X %s %-34s | %s' % (a, mark, dis(o, a) if o is not None else '',
                                      dis(t, a) if t is not None else ''))


def all_sources():
    out = []
    for dp, dn, fn in os.walk(os.path.join(N64, 'src')):
        out += [os.path.join(dp, f) for f in fn if f.endswith(('.c', '.s'))]
    return sorted(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*')
    ap.add_argument('--diff', help='print the side-by-side diff for this VA')
    ap.add_argument('--csv', default=os.path.join(OUT, 'verify.csv'))
    ap.add_argument('-q', action='store_true', help='summary only')
    a = ap.parse_args()
    rom, fmap, syms = Rom(), function_map(), load_symbols()
    want = int(a.diff, 16) if a.diff else None
    files = [os.path.abspath(f) for f in a.files] or all_sources()
    if want is not None and not a.files:
        files = [f for f in all_sources()
                 if any(v == want for v, _, _ in tags_in(open(f).read()))]
    rows = []
    for f in files:
        rows += build_file(f, rom, fmap, syms, want)
    # a partial run refreshes only its files' rows in the shared ledger
    os.makedirs(OUT, exist_ok=True)
    keep = []
    if a.files and os.path.exists(a.csv):
        done = {os.path.relpath(f, ROOT) for f in files}
        keep = [r for r in csv.DictReader(open(a.csv)) if r['file'] not in done]
    cols = ['va', 'name', 'file', 'status', 'ndiff', 'size', 'note']
    with open(a.csv, 'w', newline='') as fh:
        w = csv.DictWriter(fh, cols)
        w.writeheader()
        w.writerows(sorted(keep + rows, key=lambda r: (r['va'] or 'Z', r['name'])))
    if not a.q:
        for r in rows:
            print('%-8s %-6s %4s %-32s %s  %s' % (r['va'], r['status'], r['ndiff'],
                                                 r['name'], r['file'], r['note']))
    st = {}
    for r in rows:
        st[r['status']] = st.get(r['status'], 0) + 1
    print(' '.join('%s %d' % kv for kv in sorted(st.items())))
    return 0 if not any(r['status'] in ('ERROR', 'CCFAIL') for r in rows) else 1


if __name__ == '__main__':
    sys.exit(main())
