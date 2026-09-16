#!/usr/bin/env python3
"""The reference image as an execution environment for the T3 equivalence
oracle (tools/t3b_verify.py).

WHY THIS EXISTS.  The oracle's first version refused any function whose object
carried a relocation -- that is, any function that reads a global, uses a
string or float constant, or calls anything.  Measured over the whole T2 pile
on 2026-09-10 that was 213 of 233 functions (91%), against 11 rejected for
every other reason put together.  So the oracle answered "don't know" for
essentially every real game function and only ever spoke about pure scalar
maths.  Of the 98 certified T3 tags in the tree, 6 carried a behavioural
verdict and 92 did not.

WHAT REPLACES IT.  Both sides are run against the ORIGINAL DLL, mapped:

  * Our recompiled function's relocations are resolved to the original's own
    addresses (tools/reloc_fill.py), so our code and the original's code name
    the same globals and the same callees.  A symbol with no known address is
    a refusal, never a guess -- see `resolve_bytes`.
  * Globals live where they really live and hold what they really hold, read
    straight out of the image, so `.rdata` float constants, jump tables and
    string literals are the real ones.
  * Calls execute the ORIGINAL's callee bytes on both sides.  That is the
    point: the function under test is the only thing that differs, so a
    difference in the result is a difference in THIS function's logic.  It
    also means calling the wrong helper, or the right helper with the wrong
    arguments, shows up as a divergence instead of being stubbed away.

WHAT IS RANDOMISED.  The stack window, pointer-argument buffers, and the
zero-fill tail of `.data` (true BSS -- uninitialised in the shipped program
too, so a global there may legitimately hold anything, and varying it is what
exercises the branches).  Initialised `.data`, `.rdata` and `.text` are the
real image bytes.  Both runs get byte-identical inputs by construction: the
same seed builds the same memory.

SOUNDNESS IS THE INVARIANT.  Everything here either produces identical inputs
for the two runs or refuses.  Nothing invents an address, and no unresolved
symbol is quietly given one.
"""
from __future__ import print_function
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from capstone import Cs, CS_ARCH_X86, CS_MODE_32          # noqa: E402
from pe_patch import read_pe_text_info                    # noqa: E402
from relocmap import load_maps                            # noqa: E402
import reloc_fill                                         # noqa: E402

_md = Cs(CS_ARCH_X86, CS_MODE_32)
_md.detail = False

PAGE = 0x1000
REF_DLL = os.environ.get('BR_REF_DLL', os.path.join(ROOT, 'orig', 'BRGlide.dll'))


# ------------------------------------------------------------------ image ---

class Image(object):
    """The reference DLL laid out at its image base, with O(1) byte reads.

    A page index maps each 4 KB page to either a file offset (real bytes) or
    the marker BSS (address is inside a section's virtual size but past its
    raw data -- zero in the file, arbitrary in a running game).
    """

    BSS = -1

    def __init__(self, path=REF_DLL):
        self.path = path
        with open(path, 'rb') as f:
            self.data = f.read()
        self.base, secs = read_pe_text_info(path)
        self.sections = []
        self.pages = {}
        for name, rva, vsize, rawoff, rawsize in secs:
            lo = self.base + rva
            hi = lo + max(vsize, rawsize)
            self.sections.append((name, lo, hi, rawoff, rawsize))
            for p in range(lo & ~(PAGE - 1), hi, PAGE):
                off = p - lo
                self.pages[p] = (rawoff + off) if off < rawsize else self.BSS
        t = self.section('.text')
        self.text_lo, self.text_hi = (t[1], t[1] + t[4]) if t else (0, 0)

    def section(self, name):
        for s in self.sections:
            if s[0] == name:
                return s
        return None

    def byte(self, a):
        """The image byte at `a`, or None if `a` is not mapped at all."""
        e = self.pages.get(a & ~(PAGE - 1))
        if e is None:
            return None
        if e is self.BSS or e == self.BSS:
            return 0
        return self.data[e + (a & (PAGE - 1))]

    def is_bss(self, a):
        return self.pages.get(a & ~(PAGE - 1)) == self.BSS

    def mapped(self, a):
        return (a & ~(PAGE - 1)) in self.pages

    def text_bytes(self):
        s = self.section('.text')
        return self.data[s[3]:s[3] + s[4]]

    def _off_to_va(self, off):
        for name, lo, hi, rawoff, rawsize in self.sections:
            if rawoff <= off < rawoff + rawsize:
                return lo + (off - rawoff)
        return None

    def find_bytes(self, needle):
        """VA of the first occurrence of `needle` in the image, or None.
        Used to resolve a compiler string constant to the original's own copy."""
        off = self.data.find(needle)
        return self._off_to_va(off) if off >= 0 else None

    def imports(self):
        """{'__imp__<name>': IAT-slot VA} parsed from the PE import table, so an
        indirect CRT import call `[__imp__memmove]` resolves to its real slot."""
        if getattr(self, '_imports', None) is None:
            self._imports = {}
            d = self.data
            try:
                pe = struct.unpack_from('<I', d, 0x3c)[0]
                opt = pe + 24
                imp_rva = struct.unpack_from('<I', d, opt + 96 + 8)[0]
                def r2o(rva):
                    for name, lo, hi, ro, rs in self.sections:
                        v = lo - self.base
                        if v <= rva < v + (hi - lo):
                            return ro + (rva - v)
                    return None
                off = r2o(imp_rva)
                while off is not None:
                    oft, ts, fw, name_rva, first = struct.unpack_from('<IIIII', d, off)
                    if name_rva == 0:
                        break
                    thunk = first
                    toff = r2o(thunk)
                    while toff is not None:
                        ent = struct.unpack_from('<I', d, toff)[0]
                        if ent == 0:
                            break
                        if not (ent & 0x80000000):
                            noff = r2o(ent & 0x7fffffff)
                            if noff is not None:
                                nm = d[noff + 2:d.index(b'\0', noff + 2)].decode('latin1')
                                self._imports['__imp__' + nm] = self.base + thunk
                        thunk += 4
                        toff = r2o(thunk)
                    off += 20
            except Exception:
                pass
        return self._imports


_IMAGE = None


def image():
    global _IMAGE
    if _IMAGE is None:
        _IMAGE = Image()
    return _IMAGE


# ----------------------------------------------------------------- memory ---

def _mix(x):
    x = (x ^ 0x9E3779B9) & 0xFFFFFFFF
    x = (x * 0x85EBCA6B) & 0xFFFFFFFF
    x ^= x >> 13
    x = (x * 0xC2B2AE35) & 0xFFFFFFFF
    return x ^ (x >> 16)


def bss_byte(seed, a):
    """A deterministic byte for an address in the zero-fill tail of `.data`.

    That tail is uninitialised storage: in a running game a global there holds
    whatever the last frame left, so treating it as arbitrary is the faithful
    choice and it is what exercises the branches -- reading it back as real
    zeros would leave most guards untaken.  Generated per dword as a TAME
    float (roughly +/-100), so a dword read as a float is never inf or NaN and
    an int conversion cannot overflow.  Keyed by (seed, address): the two runs
    of a seed see byte-identical memory by construction.
    """
    base = a & ~3
    h = _mix(seed * 0x01000193 ^ base)
    x = ((h % 2000001) - 1000000) / 10000.0
    bits = struct.unpack('<I', struct.pack('<f', x))[0]
    return (bits >> (8 * (a - base))) & 0xFF


class ImgMem(object):
    """Byte memory layered over the image: writes and randomised bytes go in
    an overlay, everything else reads through to the real DLL.

    Presents the two methods x87emu needs (`get`, `__setitem__`) and records
    every address touched, so an access to memory nobody set up is caught and
    the run rejected rather than silently compared.
    """
    __slots__ = ('img', 'over', 'touched', 'written', 'unmapped', 'seed')

    def __init__(self, img, seed=0):
        self.img = img
        self.over = {}
        self.touched = set()
        self.written = set()
        self.unmapped = set()
        self.seed = seed

    def get(self, a, d=0):
        self.touched.add(a)
        v = self.over.get(a)
        if v is not None:
            return v
        if self.img.is_bss(a):
            return bss_byte(self.seed, a)
        b = self.img.byte(a)
        if b is None:
            self.unmapped.add(a)
            return d
        return b

    def __setitem__(self, a, v):
        self.touched.add(a)
        self.written.add(a)
        self.over[a] = v & 0xFF

    # -- setup helpers (bypass `touched`: this is the harness, not the run) --
    def put(self, a, v):
        self.over[a] = v & 0xFF

    def put_dword(self, a, v):
        for k in range(4):
            self.over[a + k] = (v >> (8 * k)) & 0xFF

    def peek(self, a):
        v = self.over.get(a)
        if v is not None:
            return v
        b = self.img.byte(a)
        return 0 if b is None else b


# ------------------------------------------------------------------- code ---

def disasm(code, base):
    return [(i.address, i.mnemonic, i.op_str) for i in _md.disasm(code, base)]


def code_at(addr, window=8192):
    """Disassemble one function's worth of bytes starting at `addr` from the
    image, for on-demand splicing when the shared linear sweep missed an entry
    (a target after embedded data).  Linear from addr; the caller stops at ret."""
    img = image()
    if not (img.text_lo <= addr < img.text_hi):
        return []
    off = addr - img.text_lo
    return disasm(img.text_bytes()[off:off + window], addr)


_TEXT_PROG = None


def text_program():
    """Disassemble the whole original .text ONCE and index it by address.

    Every intra-DLL call target is then already mapped, so a call executes the
    original's real callee instead of raising 'call to unmapped'.  Built once
    and shared by every run; the per-function overlay is applied in
    `program_for`.
    """
    global _TEXT_PROG
    if _TEXT_PROG is None:
        img = image()
        prog = disasm(img.text_bytes(), img.text_lo)
        _TEXT_PROG = (prog, {a: i for i, (a, _, _) in enumerate(prog)})
    return _TEXT_PROG


def program_for(va, code):
    """(listing, index) with `code` substituted for the original bytes at `va`.

    The substituted function is appended rather than spliced, and its
    addresses win in the index, so the shared .text listing is never copied.
    Instructions of the original function that `code` does not cover stay in
    the listing but are unreachable -- control enters at `va` and leaves by
    `ret`.
    """
    base, base_idx = text_program()
    own = disasm(code, va)
    prog = base + own

    idx = _ChainIndex(base_idx, {a: len(base) + i for i, (a, _, _) in enumerate(own)})
    return prog, idx


class _ChainIndex(object):
    """Address -> listing position, own entries shadowing the shared ones.

    A plain dict copy of the 130k-entry .text index per run is what made the
    first version of this unusably slow; this keeps construction O(size of the
    function under test).
    """
    __slots__ = ('base', 'own')

    def __init__(self, base, own):
        self.base = base
        self.own = own

    def __getitem__(self, a):
        v = self.own.get(a)
        return v if v is not None else self.base[a]

    def __contains__(self, a):
        return a in self.own or a in self.base

    def get(self, a, d=None):
        v = self.own.get(a)
        if v is not None:
            return v
        return self.base.get(a, d)


# ------------------------------------------------------------- relocations ---

_MAPS = None


def maps():
    global _MAPS
    if _MAPS is None:
        _MAPS = load_maps()
    return _MAPS


_ADDR_IN_NAME = re.compile(r'^(?:DAT|FUN|PTR|UNK|LAB|SUB|s|u)_?([0-9A-Fa-f]{8})$')
_ADDR_SUFFIX = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*?_?(1[0-9A-Fa-f]{7})$')
# Project naming conventions (docs/): a `g_<HEX>` global lives at 0x10000000+HEX
# (the low bytes are the name); a `sub_<HEX>` / `m_<HEX>` callee is at the full
# 0x<HEX>.  C++ output mangles them as `?g_<HEX>@@3..` / `?m_<HEX>@Obj@@..`.
_CONV_GLOBAL = re.compile(r'^\??g_([0-9A-Fa-f]{5,8})(?:@@|$)')
_CONV_CALLEE = re.compile(r'^\??(?:sub_|m_)([0-9A-Fa-f]{7,8})(?:@|$)')
# A C++ EH object's ctor/dtor cannot spell a VA the way a `?m_<HEX>@` method
# can (their mangled names are `??0<Class>@@` / `??1<Class>@@`).  When the CLASS
# name ends `_<CTORVA>_<DTORVA>`, read the ctor's address from the first group
# and the dtor's from the second -- one class carries both.
_CONV_CTOR = re.compile(r'^\?\?0\w*?_([0-9A-Fa-f]{7,8})_([0-9A-Fa-f]{7,8})@@')
_CONV_DTOR = re.compile(r'^\?\?1\w*?_([0-9A-Fa-f]{7,8})_([0-9A-Fa-f]{7,8})@@')


def _convention_addr(s):
    m = _CONV_GLOBAL.match(s)
    if m:
        return 0x10000000 + int(m.group(1), 16)
    m = _CONV_CALLEE.match(s)
    if m:
        return int(m.group(1), 16)
    m = _CONV_CTOR.match(s)
    if m:
        return int(m.group(1), 16)
    m = _CONV_DTOR.match(s)
    if m:
        return int(m.group(2), 16)
    return None


def address_in_name(sym):
    """The address a Ghidra-style name spells out, if it spells one.

    `DAT_10697a58`, `FUN_10024490` and `BrSub10071130` all carry their own
    address; the tree simply never wrote those into a map because nothing
    needed to look them up by name.  Reading it back off the name is not a
    guess -- but it is only ACCEPTED if the address lands inside a section the
    image really has, and a FUN_/SUB_ name must land in .text.  A name whose
    number is not a mapped address resolves to nothing, exactly as before.
    """
    s = sym.lstrip('_')
    a = _convention_addr(s)
    if a is None:
        m = _ADDR_IN_NAME.match(s) or _ADDR_SUFFIX.match(s)
        if not m:
            return None
        a = int(m.group(1), 16)
    img = image()
    if not (img.mapped(a) or img.is_bss(a)):   # BSS globals are addressable too
        return None
    if s[:3].upper() in ('FUN', 'SUB') and not (img.text_lo <= a < img.text_hi):
        return None
    return a


def augment_maps(obj_path, name, size):
    """(fnmap, glmap) with address-bearing symbol names of THIS object added.

    Returned as copies: the shared maps that the byte-exactness pipeline uses
    are never touched, because a wrong address there would be written into a
    patched image, where this oracle only ever reads.
    """
    fnmap, glmap = maps()
    gl = dict(glmap)
    try:
        d, secs, syms, relocs = reloc_fill.parse(obj_path)
    except Exception:
        return fnmap, gl
    img = image()
    imports = img.imports()
    for sy in syms:
        n = sy['name']
        base = n.lstrip('_')
        u = reloc_fill._undecorate(n)                 # @Name@N -> Name (fastcall/stdcall)
        if base in fnmap or base in gl or u in fnmap or u in gl:
            continue
        a = address_in_name(n)
        if a is None and n.startswith('??_C'):
            a = img.find_bytes(_decode_cstr(n))          # string constant -> orig's copy
        if a is None and n in imports:
            a = imports[n]                                # CRT import -> its IAT slot
        if a is None and '@' in n:                       # decorated stdcall import
            a = imports.get(n.split('@', 1)[0])          # __imp__Foo@8 -> __imp__Foo
        if a is None and base == 'except_list':
            a = 0                                        # fs:[0] SEH-chain head
        if a is None:
            a = _declared_va().get(base) or _declared_va().get(u)  # `Name(...); /* 0x<VA> */`
        if a is None:
            a = _declared_data_va().get(base) or _declared_data_va().get(u)  # `extern T Name; /* 0x<VA> */`
        if a is not None:
            gl[base] = a
            gl[u] = a                                    # so resolve() finds the undecorated form
    return fnmap, gl


_DECL_VA = None
_DECL_RE = re.compile(r'\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*;[^\n]*?\b0x([0-9A-Fa-f]{8})\b')
_DECL_TAG_RE = re.compile(r'\b0x([0-9A-Fa-f]{8})\s+glide\s+([A-Za-z_]\w*)')


def _declared_va():
    """{callee name -> VA} read from `Type Name(args);  /* 0x<VA> ... */`
    prototypes across src/.  The tree declares each hand-named callee with its
    address in a trailing comment; a name the maps never recorded (an
    unimplemented helper, or one implemented under a different symbol) resolves
    from that.  Accepted only when the VA lands in the image's .text -- a wrong
    address would be a guess, and the oracle refuses those.  Cached once."""
    global _DECL_VA
    if _DECL_VA is not None:
        return _DECL_VA
    _DECL_VA = {}
    img = image()
    src = os.path.join(ROOT, 'src')
    for dp, _, fs in os.walk(src):
        for fn in fs:
            if not fn.endswith(('.c', '.cpp', '.h')):
                continue
            try:
                text = open(os.path.join(dp, fn), 'r', errors='ignore').read()
            except Exception:
                continue
            for m in _DECL_RE.finditer(text):
                nm, hexva = m.group(1), m.group(2)
                if nm in _DECL_VA:
                    continue
                a = int(hexva, 16)
                if img.text_lo <= a < img.text_hi:
                    _DECL_VA[nm] = a
            # Also the transcription-marker form `0x<VA> glide <Name>` / the
            # `@implements 0x<VA> glide <Name>` tag: a hand-named function the
            # address map holds only by VA (nameless CSV row) resolves from it,
            # so a call to a transcribed-but-not-byte-exact callee (e.g. the
            # recursive BrAiScanCorridor) reaches the original's own bytes.
            for m in _DECL_TAG_RE.finditer(text):
                hexva, nm = m.group(1), m.group(2)
                a = int(hexva, 16)
                if nm not in _DECL_VA and img.text_lo <= a < img.text_hi:
                    _DECL_VA[nm] = a
    return _DECL_VA


_DECL_DATA_VA = None
_DECL_DATA_RE = re.compile(
    r'\bextern\b[^;{}=()]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;[^\n]*?\b0x([0-9A-Fa-f]{8})\b')


def _declared_data_va():
    """{global name -> VA} read from `extern Type Name;  /* 0x<VA> */` data
    declarations across src/.  The tree annotates each hand-named global with its
    address in a trailing comment (g_brAiBiasPos /* 0x10B1CBE8 */); a data symbol
    the maps never recorded resolves from that, so a function that reads/writes a
    named AI global runs in the image where the original's global really lives.
    Accepted only when the VA is MAPPED and OUTSIDE .text (a data address); the
    annotation is the tree's ground truth, verified during matching, and the
    read/write it enables is compared like any other global."""
    global _DECL_DATA_VA
    if _DECL_DATA_VA is not None:
        return _DECL_DATA_VA
    _DECL_DATA_VA = {}
    img = image()
    src = os.path.join(ROOT, 'src')
    for dp, _, fs in os.walk(src):
        for fn in fs:
            if not fn.endswith(('.c', '.cpp', '.h')):
                continue
            try:
                text = open(os.path.join(dp, fn), 'r', errors='ignore').read()
            except Exception:
                continue
            for m in _DECL_DATA_RE.finditer(text):
                nm, hexva = m.group(1), m.group(2)
                if nm in _DECL_DATA_VA:
                    continue
                a = int(hexva, 16)
                if (img.text_lo <= a < img.text_hi) or not img.mapped(a):
                    continue
                # Reject a STALE annotation: the g_<HEX> convention name derived
                # from this VA has a DIFFERENT learned address.  A friendly alias
                # (g_brCdPlaying /* 0x10220CD0 */) whose real global lives
                # elsewhere (g_220CD0 -> 0x1021C800 in globals_learned.csv) would
                # otherwise send our writes to the wrong object -> a false DIFF.
                learned_a = _learned_conv_addr(a)
                if learned_a is not None and learned_a != a:
                    continue
                _DECL_DATA_VA[nm] = a
    return _DECL_DATA_VA


_LEARNED_MAP = None


def _learned_conv_addr(a):
    """Learned address of the `g_<HEX>` convention name derived from VA `a`
    (HEX = low 24 bits), or None.  Used to detect a stale friendly-alias
    annotation whose real global lives at a different address."""
    global _LEARNED_MAP
    if _LEARNED_MAP is None:
        try:
            _LEARNED_MAP = reloc_fill.load_learned()
        except Exception:
            _LEARNED_MAP = {}
    return _LEARNED_MAP.get('g_%06X' % (a & 0xFFFFFF))


_ZERO_VA = None


def _mapped_zero_va(img):
    """A MAPPED image VA holding at least 8 zero bytes (for a 0.0f/0.0 constant).
    find_bytes alone lands in the unmapped PE header; skip each zero run whose
    file offset does not map to a section VA.  Cached."""
    global _ZERO_VA
    if _ZERO_VA is not None:
        return _ZERO_VA
    data = img.data
    off = data.find(b'\0' * 8)
    while off >= 0:
        va = img._off_to_va(off)
        if va is not None and img.mapped(va):
            _ZERO_VA = va
            return va
        nz = off
        while nz < len(data) and data[nz] == 0:
            nz += 1
        off = data.find(b'\0' * 8, nz)
    return None


_CSTR_ESC = {'?4': '.', '?2': '\\', '?5': ' ', '?3': ':', '?1': '/', '?0': '@'}


def _decode_cstr(name):
    """MSVC string-literal symbol -> its bytes: ??_C@_0LEN@HASH@<encoded>@ ."""
    body = name.split('@')[3] if name.count('@') >= 4 else ''
    for k, v in _CSTR_ESC.items():
        body = body.replace(k, v)
    body = body.replace('?$AA', '').rstrip('@')
    return body.encode('latin1') + b'\0'


def resolve_bytes(obj_path, name, va, size):
    """Our recompiled bytes with every relocation resolved to the original's
    own addresses, or (None, why).

    Refuses outright if any relocation names a symbol no map gives an address
    for.  That refusal is the soundness boundary: a guessed address would make
    our code read a different global from the original's and the comparison
    would be meaningless.
    """
    fnmap, glmap = augment_maps(obj_path, name, size)
    img = image()
    # The C++ EH-handler push (`push OFFSET $Lnnn`) names a `$` label in a
    # separate .text$x funclet section: unnameable globally, but its only effect
    # is the address it puts on the SEH chain -- frame state the oracle never
    # compares.  Resolve it to the function's own (mapped) address so the run
    # proceeds; a same-section `$L` (jump table) is resolved normally and is not
    # overridden here.
    extra = {}
    try:
        d, secs, syms, relocs = reloc_fill.parse(obj_path)
        fn = next((s for s in syms
                   if reloc_fill._undecorate(s['name']) == reloc_fill._undecorate(name)
                   and secs.get(s['sec'], {}).get('name', '').startswith('.text')), None)
        if fn is not None:
            for rva, si, rt in relocs.get(fn['sec'], []):
                off = rva - fn['val']
                if not (0 <= off < size - 3):
                    continue
                ts = next((s for s in syms if s['idx'] == si), None)
                # ONLY the C++ EH-handler funclet ($L… in a .text$x section):
                # that address only reaches the SEH chain, never runs on the
                # normal path, so a dummy is sound.  Do NOT touch $T EH-state
                # labels or anything else -- blanket-resolving them to a dummy
                # corrupts functions that read them and manufactures false DIFFs
                # (measured on BrCarStateLerp), so leave those UNCLASSIFIED.
                if (ts and ts['name'].lstrip('_').startswith('$L')
                        and ts['sec'] != fn['sec']
                        and secs.get(ts['sec'], {}).get('name', '').startswith('.text')):
                    extra[(va, off)] = va
                    continue
                # A `$T` float/double literal in .rdata/.data: MSVC names each
                # constant $T<n>.  The function READS it, so a dummy would corrupt
                # behaviour -- instead resolve it to the ORIGINAL's identical copy
                # by finding this object's own constant bytes in the image.  Sound:
                # a wrong transcribed constant is either absent (-> unresolved,
                # UNCLASSIFIED) or reads a different value than the original's
                # (-> DIFF); it can never manufacture a false EQUIVALENT.
                if (ts and ts['name'].lstrip('_').startswith('$T')
                        and secs.get(ts['sec'], {}).get('name', '')
                            .startswith(('.rdata', '.data'))):
                    sec2 = secs[ts['sec']]
                    nexts = [s['val'] for s in syms
                             if s['sec'] == ts['sec'] and s['val'] > ts['val']]
                    end = min(nexts) if nexts else sec2['size']
                    gap = end - ts['val']
                    length = 8 if gap >= 8 else 4
                    cstart = sec2['praw'] + ts['val']
                    # Find this obj's own constant bytes in the image (the
                    # original's identical copy).  Try the slot length, then fall
                    # back to a 4-byte float: a wrong-length match reads a wrong
                    # value and surfaces as DIFF, never a false EQUIVALENT.
                    img_addr = None
                    for ln in (length, 4):
                        const = d[cstart:cstart + ln]
                        if len(const) != ln:
                            continue
                        if const == b'\0' * ln:
                            # 0.0f/0.0: find_bytes would land in the PE header
                            # (unmapped); resolve to a mapped zeroed address so
                            # both sides read 0 identically.
                            img_addr = _mapped_zero_va(img)
                        else:
                            img_addr = img.find_bytes(const)
                        if img_addr is not None:
                            break
                    if img_addr is not None:
                        praw = secs[fn['sec']]['praw']
                        addend = struct.unpack_from('<i', d, praw + off)[0]
                        extra[(va, off)] = (img_addr + addend) & 0xFFFFFFFF
    except Exception:
        extra = {}
    try:
        code = reloc_fill.fill_function(obj_path, name, va, fnmap, glmap, size, extra)
    except Exception as e:
        return None, 'reloc fill raised %s' % type(e).__name__
    if code is None:
        return None, 'a relocation names a symbol with no known address'
    return code, None


def shadows_a_neighbour(va, size):
    """Does substituting `size` bytes at `va` bury another function's entry?

    Our recompiled function is usually a few bytes longer than the original's.
    Those extra bytes sit at addresses belonging to whatever the linker put
    next, and the substituted listing shadows them -- so if the run reached
    that neighbour it would execute OUR tail as if it were the neighbour's
    head.  Any function entry strictly inside the substituted range makes the
    environment untrustworthy, so the caller refuses rather than compares.
    """
    fnmap, _ = maps()
    hit = [n for n, a in fnmap.items() if va < a < va + size]
    return sorted(hit)


def unresolved_symbols(obj_path, name, size):
    """Which symbols blocked `resolve_bytes` -- for reporting, not for use."""
    fnmap, glmap = augment_maps(obj_path, name, size)
    out = []
    try:
        d, secs, syms, relocs = reloc_fill.parse(obj_path)
    except Exception:
        return out
    for sy in syms:
        if sy['sec'] <= 0 or sy['sec'] not in secs:
            continue
        if not secs[sy['sec']]['name'].startswith('.text'):
            continue
        if reloc_fill._undecorate(sy['name']) != name:
            continue
        for rva, si, rt in relocs[sy['sec']]:
            off = rva - sy['val']
            if not (0 <= off < size - 3):
                continue
            ts = next((s for s in syms if s['idx'] == si), None)
            if ts is None or reloc_fill.resolve(ts['name'], fnmap, glmap) is None:
                out.append(ts['name'] if ts else '?')
        break
    return sorted(set(out))
