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
    m = _ADDR_IN_NAME.match(s) or _ADDR_SUFFIX.match(s)
    if not m:
        return None
    a = int(m.group(1), 16)
    img = image()
    if not img.mapped(a):
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
    for sy in syms:
        n = sy['name']
        base = n.lstrip('_')
        if base in fnmap or base in gl:
            continue
        a = address_in_name(n)
        if a is not None:
            gl[base] = a
    return fnmap, gl


def resolve_bytes(obj_path, name, va, size):
    """Our recompiled bytes with every relocation resolved to the original's
    own addresses, or (None, why).

    Refuses outright if any relocation names a symbol no map gives an address
    for.  That refusal is the soundness boundary: a guessed address would make
    our code read a different global from the original's and the comparison
    would be meaningless.
    """
    fnmap, glmap = augment_maps(obj_path, name, size)
    try:
        code = reloc_fill.fill_function(obj_path, name, va, fnmap, glmap, size)
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
