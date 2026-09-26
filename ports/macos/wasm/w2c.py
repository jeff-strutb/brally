#!/usr/bin/env python3
"""wasm32 relocatable objects -> native C, linked against the ORIGINAL layout.

WHY. The game's source is written for Win32: 32-bit pointers stored in 32-bit
fields, globals that several names reach at one fixed address, data tables
holding 32-bit function and data addresses. A 64-bit native compile breaks all
of that. Compiling the same source for wasm32 keeps the ILP32 data model
exactly (int/long/pointer = 4 bytes, little-endian, IEEE floats); this tool
then turns each wasm OBJECT into plain C that runs natively on arm64 against a
32-bit address space mapped at a fixed host base.

HOW EACH RELOCATION IS LINKED -- this tool is the linker:
  * data symbols with an original address (build/wasm/symmap.csv, scoped by
    source file, then global, then an address carried in the name) resolve to
    THAT address; the original image's own data sits there at run time, so
    every alias of one global is one object again;
  * other data (string literals, port-only globals) is laid out in a port data
    region below the image;
  * a function's ADDRESS is its original VA when it has one, else a synthetic
    address; indirect calls dispatch on that value, so the function pointers
    already in the original data tables work unchanged;
  * direct calls become C calls; calls to nothing in the tree become calls to
    the host layer (Win32/COM/Glide/CRT on macOS), or to a trap stub.

Signature mismatches between a caller's prototype and the callee's definition
(legal-ish C the MSVC build tolerated) are bridged the way x86 cdecl would:
arguments are flattened to 32-bit stack words and re-read in the callee's
types.

Usage: w2c.py --out DIR --symmap CSV --owners CSV obj.o...
  obj names carry their source path: src_core_x_y.c.o (see build_wasm.sh).
"""
import argparse
import csv
import os
import re
import struct
import sys
from collections import defaultdict

# ------------------------------------------------------------------ reader --


class R:
    def __init__(self, b, p=0):
        self.b, self.p = b, p

    def u8(self):
        v = self.b[self.p]
        self.p += 1
        return v

    def uleb(self):
        r = s = 0
        while True:
            c = self.b[self.p]
            self.p += 1
            r |= (c & 0x7F) << s
            s += 7
            if not c & 0x80:
                return r

    def sleb(self, bits=64):
        r = s = 0
        while True:
            c = self.b[self.p]
            self.p += 1
            r |= (c & 0x7F) << s
            s += 7
            if not c & 0x80:
                if c & 0x40:
                    r -= 1 << s
                return r

    def name(self):
        n = self.uleb()
        v = self.b[self.p:self.p + n].decode('utf-8', 'replace')
        self.p += n
        return v

    def bytes(self, n):
        v = self.b[self.p:self.p + n]
        self.p += n
        return v


VT = {0x7F: 'i32', 0x7E: 'i64', 0x7D: 'f32', 0x7C: 'f64', 0x70: 'funcref',
      0x6F: 'externref'}
CT = {'i32': 'u32', 'i64': 'u64', 'f32': 'f32', 'f64': 'f64'}
PFX = {'i32': 'a', 'i64': 'b', 'f32': 'c', 'f64': 'd'}
SIGC = {'i32': 'i', 'i64': 'I', 'f32': 'f', 'f64': 'F'}

F_WEAK, F_LOCAL, F_HIDDEN, F_UNDEF, F_EXPORTED, F_EXPLICIT = \
    1, 2, 4, 0x10, 0x20, 0x40
F_ABSOLUTE = 0x200


def sig_of(ft):
    p, r = ft
    return ''.join(SIGC[t] for t in p) + '_' + ''.join(SIGC[t] for t in r)


class Obj:
    def __init__(self, path, oid):
        self.path, self.oid = path, oid
        b = open(path, 'rb').read()
        assert b[:4] == b'\0asm', path
        self.types, self.imports, self.funcs = [], [], []
        self.fimports, self.gimports = [], []
        self.globals = []
        self.code = None
        self.code_off = 0
        self.bodies = []
        self.data = []          # (payload_off_in_section, bytes)
        self.data_sec = None
        self.syms = []
        self.segs = []          # (name, align, flags)
        self.init_funcs = []
        self.relocs = defaultdict(list)   # section index -> [(type,off,idx,add)]
        self.sections = []
        r = R(b, 8)
        si = 0
        while r.p < len(b):
            sid = r.u8()
            size = r.uleb()
            start = r.p
            body = R(b, start)
            self.sections.append((sid, start, size))
            if sid == 1:
                for _ in range(body.uleb()):
                    assert body.u8() == 0x60
                    ps = [VT[body.u8()] for _ in range(body.uleb())]
                    rs = [VT[body.u8()] for _ in range(body.uleb())]
                    self.types.append((ps, rs))
            elif sid == 2:
                for _ in range(body.uleb()):
                    mod, nm = body.name(), body.name()
                    k = body.u8()
                    if k == 0:
                        self.fimports.append((mod, nm, body.uleb()))
                    elif k == 1:
                        body.u8()
                        fl = body.uleb()
                        body.uleb()
                        if fl & 1:
                            body.uleb()
                    elif k == 2:
                        fl = body.uleb()
                        body.uleb()
                        if fl & 1:
                            body.uleb()
                    elif k == 3:
                        t = VT[body.u8()]
                        mut = body.u8()
                        self.gimports.append((mod, nm, t, mut))
                    elif k == 4:
                        body.u8()
                        body.uleb()
            elif sid == 3:
                self.funcs = [body.uleb() for _ in range(body.uleb())]
            elif sid == 6:
                for _ in range(body.uleb()):
                    t = VT[body.u8()]
                    body.u8()
                    ex = self.const_expr(body)
                    self.globals.append((t, ex))
            elif sid == 10:
                self.code, self.code_sec = b, si
                self.code_off = start
                n = body.uleb()
                for _ in range(n):
                    sz = body.uleb()
                    fs = body.p
                    self.bodies.append((fs, sz))
                    body.p = fs + sz
            elif sid == 11:
                self.data_sec = si
                n = body.uleb()
                for _ in range(n):
                    fl = body.uleb()
                    if fl == 0:
                        self.const_expr(body)
                    elif fl == 2:
                        body.uleb()
                        self.const_expr(body)
                    sz = body.uleb()
                    po = body.p - start
                    self.data.append((po, bytes(body.bytes(sz))))
            elif sid == 0:
                nm = body.name()
                if nm == 'linking':
                    self.parse_linking(body, start + size)
                elif nm.startswith('reloc.'):
                    tgt = body.uleb()
                    for _ in range(body.uleb()):
                        ty = body.u8()
                        off = body.uleb()
                        idx = body.uleb()
                        add = 0
                        if ty in (3, 4, 5, 8, 9, 11, 21, 22):
                            add = body.sleb()
                        self.relocs[tgt].append((ty, off, idx, add))
            r.p = start + size
            si += 1
        self.nfimp = len(self.fimports)
        self.ngimp = len(self.gimports)
        self.b = b

    def const_expr(self, r):
        op = r.u8()
        if op == 0x41:
            v = r.sleb()
        elif op == 0x42:
            v = r.sleb()
        elif op == 0x23:
            v = ('global', r.uleb())
        else:
            raise SystemExit('const expr op %#x' % op)
        assert r.u8() == 0x0B
        return v

    def parse_linking(self, r, end):
        r.uleb()   # version
        while r.p < end:
            k = r.u8()
            sz = r.uleb()
            e = r.p + sz
            if k == 5:
                for _ in range(r.uleb()):
                    self.segs.append((r.name(), r.uleb(), r.uleb()))
            elif k == 6:
                for _ in range(r.uleb()):
                    self.init_funcs.append((r.uleb(), r.uleb()))
            elif k == 8:
                for _ in range(r.uleb()):
                    kind = r.u8()
                    fl = r.uleb()
                    s = {'kind': kind, 'flags': fl, 'name': None}
                    if kind in (0, 2, 4, 5):
                        s['index'] = r.uleb()
                        if not fl & F_UNDEF or fl & F_EXPLICIT:
                            s['name'] = r.name()
                    elif kind == 1:
                        s['name'] = r.name()
                        if not fl & F_UNDEF:
                            s['seg'] = r.uleb()
                            s['off'] = r.uleb()
                            s['size'] = r.uleb()
                    elif kind == 3:
                        s['index'] = r.uleb()
                    self.syms.append(s)
            r.p = e
        # names for undefined function/global symbols come from imports
        for s in self.syms:
            if s['name'] is None and s['kind'] == 0 and s['index'] < len(self.fimports):
                s['name'] = self.fimports[s['index']][1]
            if s['name'] is None and s['kind'] == 2 and s['index'] < len(self.gimports):
                s['name'] = self.gimports[s['index']][1]

    def ftype(self, fidx):
        if fidx < self.nfimp:
            return self.types[self.fimports[fidx][2]]
        return self.types[self.funcs[fidx - self.nfimp]]


# ----------------------------------------------------------------- helpers --

def cident(s):
    return re.sub(r'[^A-Za-z0-9_]', '_', s)


LLVM = os.environ.get('LLVM', '/opt/homebrew/opt/emscripten/libexec/llvm/bin')


def msvc_qual(sym):
    """MSVC decorated name -> 'Class::Method' / 'Name' (demangled form)."""
    m = re.match(r'\?\?([01])([A-Za-z_]\w*)@@', sym)
    if m:
        c = m.group(2)
        return '%s::%s%s' % (c, '~' if m.group(1) == '1' else '', c)
    m = re.match(r'\?([A-Za-z_]\w*)@(?:([A-Za-z_]\w*)@)?@', sym)
    if m:
        return '%s::%s' % (m.group(2), m.group(1)) if m.group(2) else m.group(1)
    return sym.lstrip('_@').split('@')[0]


ADDR_IN_NAME = re.compile(r'(?:^|_)(1[0-9a-fA-F]{7})$')
DATA_LO, DATA_HI = 0x10077000, 0x118F0000
TEXT_LO, TEXT_HI = 0x10001000, 0x10077000


FUNC_STARTS = set()      # filled from build/match/orig/<VA>.bin names
MAGIC = 0x7EC00000       # ccmark.py's convention marker
THUNKS = {}              # VA of a `jmp [IAT]` stub in the original -> import name
IAT = {}                 # IAT slot VA -> import name
JMPS = {}                # function start that is `jmp rel32` -> its target
HOST_NAMES = set('''memcpy memmove memset memcmp memchr strlen strcpy strncpy strcat
    strcmp strncmp strchr strrchr strstr sprintf vsprintf snprintf sscanf printf fprintf
    fopen fclose fread fwrite fseek ftell getc ungetc malloc calloc realloc free
    sqrt sin cos tan asin atan2 pow floor rint isfinite atoi toupper qsort rand srand
    exit getenv strerror'''.split())


def load_thunks(dll):
    """Every `FF 25 <IAT slot>` stub in the original's .text, by address."""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..', '..', 'tools'))
    import pe
    p = pe.load(os.path.normpath(dll))
    text, base = p.text()
    i = text.find(b'\xff\x25')
    while i != -1:
        slot = struct.unpack_from('<I', text, i + 2)[0]
        nm = p.imports.get(slot)
        if nm:
            n = nm.split('!')[1]
            n = re.sub(r'^_|@\d+$', '', n)
            THUNKS[base + i] = {'??2@YAPAXI@Z': '_Znwm', '??3@YAXPAX@Z': '_ZdlPv'}.get(
                nm.split('!')[1], n)
        i = text.find(b'\xff\x25', i + 1)
    for st in FUNC_STARTS or []:
        pass
    # every `E9 rel32` function start: a jump thunk
    for va in list(FUNC_STARTS):
        o = va - base
        if 0 <= o < len(text) - 5 and text[o] == 0xE9:
            JMPS[va] = (va + 5 + struct.unpack_from('<i', text, o + 1)[0]) & 0xFFFFFFFF
    for slot, nm in p.imports.items():
        n = nm.split('!')[1]
        IAT[slot] = {'??2@YAPAXI@Z': '_Znwm', '??3@YAXPAX@Z': '_ZdlPv'}.get(
            n, re.sub(r'^_|@\d+$', '', n))
        HOST_NAMES.add(IAT[slot])


def addr_from_name(n, lo, hi):
    """An address the name itself carries, in the tree's conventions:
    DAT_1007b264 / BrSub10002580 (full VA) or g_0773A4 / Page043050 (short,
    implicitly 0x10xxxxxx).  A code address counts only if it is a known
    function start; a data address only inside the image's data sections."""
    n = n.split('::')[-1] if '::' in n else n
    m = re.search(r'([0-9A-Fa-f]{4,8})$', n)
    if not m:
        return None
    h = m.group(1)
    cands = []
    if len(h) == 8 and h[0] == '1':
        cands.append(int(h, 16))
    for k in (7, 6, 5, 4):
        if len(h) >= k:
            t = h[-k:]
            v = int(t, 16)
            cands.append(0x10000000 + v if k < 8 else v)
            if k == 7 and t[0] == '1':
                cands.append(0x10000000 | v)
    for a in cands:
        if not (lo <= a < hi):
            continue
        if lo == TEXT_LO and a not in FUNC_STARTS:
            continue
        return a
    return None


# ------------------------------------------------- x86 calling conventions --
# The source spells MSVC conventions in whatever form reproduces the x86
# bytes, so two sides of one call can disagree at the C level and agree on
# x86: a thiscall member implemented as a __stdcall C function that ignores
# ecx, a __fastcall with a dummy edx. wasm has no conventions, so such a call
# would hand the callee the wrong argument. The i686 IR of each TU
# (wcc.sh) records the convention of every declaration and definition; a
# call whose x86 frames differ goes through an adapter that builds the real
# frame (ecx, edx, stack words) from the caller's view and reads the callee's
# parameters out of it.

IR_FN = re.compile(r'^(define|declare)\b[^@]*?\b(x86_\w+cc)?\s*[^@(]*@("(?:[^"\\]|\\.)*"|[\w.$]+)\((.*)\)')


def ir_norm(n):
    n = n.strip('"')
    if n.startswith('\\01'):
        n = n[3:]
        n = n.lstrip('_@')
        n = re.sub(r'@\d+$', '', n)
    return n


def split_params(ps):
    out, depth, cur = [], 0, ''
    for ch in ps:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [p.strip() for p in out if p.strip() and p.strip() != '...']


def load_conv(path):
    """{name: (kind, cc, [inreg flags])} from one TU's i686 IR."""
    out = {}
    try:
        lines = open(path, errors='replace').read().split('\n')
    except OSError:
        return out
    for l in lines:
        m = IR_FN.match(l)
        if not m:
            continue
        kind, name, ps = m.group(1), ir_norm(m.group(3)), m.group(4)
        mc = re.search(r'\bx86_\w+cc\b', l.split('@', 1)[0])
        cc = mc.group(0) if mc else 'c'
        flags = ['inreg' in p.split() for p in split_params(ps)]
        prev = out.get(name)
        if prev is None or (kind == 'define' and prev[0] != 'define'):
            out[name] = (kind, cc, flags)
    return out


def x86_frame(types, cc, inreg):
    """Where each wasm parameter lives on x86: ('ecx'|'edx', 0) or
    ('stk', first_word). None if the views cannot be lined up."""
    if len(inreg) != len(types):
        return None
    regs = ['ecx', 'edx']
    ri, w, out = 0, 0, []
    for i, t in enumerate(types):
        if cc == 'x86_thiscallcc' and i == 0:
            out.append(('ecx', 0))
            ri = 2
            continue
        if cc == 'x86_fastcallcc' and inreg[i] and ri < 2 and t == 'i32':
            out.append((regs[ri], 0))
            ri += 1
            continue
        out.append(('stk', w))
        w += 2 if t in ('i64', 'f64') else 1
    return tuple(out)


# -------------------------------------------------------------- the linker --

class Linker:
    def __init__(self, objs, symmap, owners, srcof, sites=(), placement=()):
        self.objs = objs
        self.sites = {(sc, n, a): va for sc, n, a, va in sites}
        self.owners = owners
        self.placed = defaultdict(dict)    # src base -> {name: va}
        self.placed_va = {}                # va -> src base
        self.placed_any = {}               # name -> va, placements with no src
        for va, name, base in placement:
            for nm in {name, msvc_qual(name), name.lstrip('_')}:
                if base:
                    self.placed[base][nm] = va
                else:
                    self.placed_any[nm] = va
            self.placed_va[va] = base
            # the verified build's choice decides duplicate definitions
            if base:
                owners[msvc_qual(name)] = base
        self.unverified_ok = True
        self.unverified = []
        self.srcof = srcof
        self.scoped = defaultdict(dict)
        self.glob = {}
        for scope, name, va in symmap:
            if scope == '*':
                self.glob[name] = va
            else:
                self.scoped[scope][name] = va
        self.owners = owners         # function name -> preferred source base
        self.warn = []

    def base_of(self, o):
        return os.path.splitext(os.path.basename(self.srcof[o.oid]))[0]

    def path_of(self, o):
        return self.srcpath[o.oid]

    def load_names(self):
        """Itanium demangling (clang's C++ names) and the tree's own
        @implements / @cpp_symbol tags, which give each function its VA."""
        import subprocess
        mangled = set()
        for o in self.objs:
            for s in o.syms:
                if s['name'] and s['name'].startswith('_Z'):
                    mangled.add(s['name'])
        mangled = sorted(mangled)
        self.dem = {}
        if mangled:
            out = subprocess.run([os.path.join(LLVM, 'llvm-cxxfilt')],
                                 input='\n'.join(mangled), capture_output=True,
                                 text=True).stdout.split('\n')
            for m, d in zip(mangled, out):
                self.dem[m] = re.sub(r'\(.*$', '', d).strip()
        self.impl = defaultdict(dict)    # source base -> {name: va}
        for o in self.objs:
            src = self.srcpath[o.oid]
            base = src
            try:
                t = open(src, encoding='latin-1').read()
            except OSError:
                continue
            for m in re.finditer(r'@implements\s+0x([0-9A-Fa-f]{8})\s+glide\s+(\S+)'
                                 r'(?:(?!@implements).)*?@cpp_symbol\s+(\S+)|'
                                 r'@implements\s+0x([0-9A-Fa-f]{8})\s+glide\s+(\S+)',
                                 t, re.S):
                if m.group(1):
                    va = int(m.group(1), 16)
                    q = msvc_qual(m.group(3))
                    self.impl[base][q] = va
                    self.impl[base][m.group(2)] = va
                    # MSVC may qualify where clang's spelling does not
                    # (?Hook@Ui85@@ vs a global Hook): the last component too
                    self.impl[base].setdefault(q.split('::')[-1], va)
                else:
                    self.impl[base][m.group(5)] = int(m.group(4), 16)

    def plain(self, nm):
        """C-level name: a demangled C++ name, else the name itself."""
        return self.dem.get(nm, nm)

    def run(self):
        self.load_names()
        # ---- global symbol tables
        self.gfunc = {}     # name -> (obj, sym)
        self.gdata = {}
        for o in self.objs:
            for i, s in enumerate(o.syms):
                if s['flags'] & (F_UNDEF | F_LOCAL) or not s['name']:
                    continue
                tab = self.gfunc if s['kind'] == 0 else (
                    self.gdata if s['kind'] == 1 else None)
                if tab is None:
                    continue
                prev = tab.get(s['name'])
                if prev is None:
                    tab[s['name']] = (o, s)
                    continue
                po, ps = prev
                weak_new = bool(s['flags'] & F_WEAK)
                weak_old = bool(ps['flags'] & F_WEAK)
                if weak_old and not weak_new:
                    tab[s['name']] = (o, s)
                elif weak_new == weak_old and not weak_new:
                    pref = self.owners.get(self.plain(s['name'])) or self.owners.get(s['name'])
                    if pref and pref in (self.path_of(o), self.base_of(o)):
                        tab[s['name']] = (o, s)
                    if s['kind'] == 0:
                        self.warn.append('dup function %s: %s vs %s -> %s' % (
                            s['name'], self.base_of(po), self.base_of(o),
                            self.base_of(tab[s['name']][0])))
        # ---- function addresses and C names
        self.fname = {}      # (oid, fidx) -> C name
        self.fdef = {}       # (oid, fidx) -> (obj, definition name)
        self.conv = {}       # obj path -> load_conv()
        self.fva = {}        # (oid, fidx) -> address (u32)
        self.fsig = {}
        syn = 0xF0000000
        self.va_used = {}
        for o in self.objs:
            base = self.base_of(o)
            for i, s in enumerate(o.syms):
                if s['kind'] != 0 or s['flags'] & F_UNDEF:
                    continue
                fi = s['index']
                nm = s['name'] or ('f%d' % fi)
                canon = (not s['flags'] & F_LOCAL and
                         self.gfunc.get(nm, (None,))[0] is o)
                if not canon and not s['flags'] & F_LOCAL:
                    # a losing duplicate: keep it addressable if the verified
                    # build placed it (shouldn't happen once owners follow
                    # placement) -- treat as local
                    pass
                cn = ('f_' + cident(nm)) if canon else \
                    ('l%d_%s' % (o.oid, cident(nm)))
                if (o.oid, fi) in self.fname:
                    continue
                self.fname[(o.oid, fi)] = cn
                self.fsig[(o.oid, fi)] = sig_of(o.ftype(fi))
                self.fdef[(o.oid, fi)] = (o, nm)
                va = None
                if canon or s['flags'] & F_LOCAL:
                    pn = self.plain(nm)
                    # The verified T3 build decides ownership: this function
                    # gets a VA only if that build placed THIS file's symbol
                    # there.
                    path = self.path_of(o)
                    pl = self.placed.get(path, {})
                    va = pl.get(pn, pl.get(nm))
                    if va is None and pn in self.placed_any:
                        va = self.placed_any[pn]
                    if va is None:
                        iv = self.impl[path].get(pn)
                        if iv is not None and self.placed_va.get(iv) == path:
                            va = iv
                    if va is None and self.unverified_ok:
                        # outside the verified build (T1/T2): only a VA the
                        # build left to original bytes, claimed by this file
                        iv = self.impl[path].get(pn)
                        if iv is not None and iv not in self.placed_va:
                            va = iv
                            self.unverified.append((iv, base, pn))
                if va is not None and not (TEXT_LO <= va < TEXT_HI):
                    va = None
                if va is not None and va in self.va_used:
                    self.warn.append('VA %#x claimed by %s and %s' % (
                        va, self.va_used[va], cn))
                    va = None
                if va is None:
                    va = syn
                    syn += 8
                    if syn >= 0xF0040000:
                        raise SystemExit('w2c: synthetic address range full')
                else:
                    self.va_used[va] = cn
                self.fva[(o.oid, fi)] = va
        # functions by address, for resolving FUN_xxxxxxxx-style imports
        self.by_va = {}
        for k, va in self.fva.items():
            if va < 0xF0000000:
                self.by_va[va] = k
        # canonical functions by their plain (demangled) name
        self.by_plain = {}
        for nm, (go, gs) in self.gfunc.items():
            pn = self.plain(nm)
            if pn != nm:
                self.by_plain.setdefault(pn, (go.oid, gs['index']))
        # ---- data layout
        self.seg_addr = {}   # (oid, seg) -> address, or None if mapped away
        self.dsym_addr = {}  # (oid, symidx) -> address
        self.port_segs = []  # (oid, seg, addr)
        cur = 0x00010000
        mapped_segs = {}
        for o in self.objs:
            base = self.base_of(o)
            for i, s in enumerate(o.syms):
                if s['kind'] != 1 or s['flags'] & F_UNDEF:
                    continue
                nm = s['name']
                if not s['flags'] & F_LOCAL and self.gdata.get(nm, (None,))[0] is not o:
                    continue    # a non-canonical duplicate: never referenced
                va = self.scoped[base].get(nm)
                if va is None and not s['flags'] & F_LOCAL:
                    va = self.glob.get(nm)
                if va is None:
                    va = self.sites.get(('decl:' + self.path_of(o), nm, 0))
                if va is None and not nm.startswith('.L'):
                    va = addr_from_name(nm, DATA_LO, DATA_HI)
                if va is not None and not (DATA_LO <= va < DATA_HI):
                    va = None
                if va is not None:
                    mapped_segs[(o.oid, s['seg'])] = va - s['off']
        for o in self.objs:
            for si, (po, data) in enumerate(o.data):
                k = (o.oid, si)
                if k in mapped_segs:
                    self.seg_addr[k] = mapped_segs[k]
                    continue
                al = 1 << o.segs[si][1] if si < len(o.segs) else 4
                al = max(al, 1)
                cur = (cur + al - 1) & ~(al - 1)
                self.seg_addr[k] = cur
                self.port_segs.append((o.oid, si, cur))
                cur += max(len(data), 1)
        self.port_end = cur
        if cur > 0x00F00000:
            raise SystemExit('w2c: port data overflows its region (%#x)' % cur)
        self.orphans = {}
        self.orphan_cur = 0x00F00000
        # ---- host imports actually referenced
        self.host = {}       # C name -> sig
        self.missing = {}    # m_<VA> -> (sig, name, va): game code not in the tree
        self.stubs = {}
        return self

    # ---- resolution of one object's symbol reference
    def data_addr(self, o, si, add=0):
        """Address of symbol si + add.  A per-site answer (symbol+offset, from
        the original image) wins: the source sometimes models scattered
        original globals as one struct."""
        s = o.syms[si]
        path, base = self.path_of(o), 'base:' + self.base_of(o)
        va = None
        for k in (path, base):
            va = self.sites.get((k, s['name'], add))
            if va is None:
                va = self.sites.get((k, self.plain(s['name']), add))
            if va is not None:
                return va
        return (self._data_base(o, si) + add) & 0xFFFFFFFF

    def _data_base(self, o, si):
        s = o.syms[si]
        if not s['flags'] & F_UNDEF:
            if not s['flags'] & F_LOCAL:
                go, gs = self.gdata[s['name']]
                return self.seg_addr[(go.oid, gs['seg'])] + gs['off']
            return self.seg_addr[(o.oid, s['seg'])] + s['off']
        nm = s['name']
        if nm in self.gdata:
            go, gs = self.gdata[nm]
            return self.seg_addr[(go.oid, gs['seg'])] + gs['off']
        base = self.base_of(o)
        va = self.scoped[base].get(nm)
        if va is None:
            va = self.glob.get(nm)
        if va is None:
            va = addr_from_name(nm, DATA_LO, DATA_HI)
        if va is None:
            # last: the file's own address annotation on the declaration
            va = self.sites.get(('decl:' + self.path_of(o), nm, 0))
        if va is not None:
            return va
        if nm not in self.orphans:
            self.orphans[nm] = self.orphan_cur
            self.orphan_cur += 0x1000
            if self.orphan_cur > 0x02000000:
                raise SystemExit('w2c: orphan region full')
        return self.orphans[nm]

    def func_ref(self, o, symidx):
        """(C name, address, sig-of-definition or None for host)."""
        self.last_k = None
        r = self._func_ref(o, symidx)
        return r

    def conv_of(self, o):
        c = self.conv.get(o.oid)
        if c is None:
            c = load_conv(o.path + '.x86.ll')
            self.conv[o.oid] = c
        return c

    def x86_bridge(self, o, symidx, csig, k):
        """(caller frame, callee frame) when they differ, else None."""
        if k is None or k not in self.fdef:
            return None
        co, cname = self.fdef[k]
        s = o.syms[symidx]
        mine = self.conv_of(o).get(s['name'])
        theirs = self.conv_of(co).get(cname)
        if not mine or not theirs:
            return None
        rt = {'i': 'i32', 'I': 'i64', 'f': 'f32', 'F': 'f64'}
        cps = [rt[c] for c in csig.split('_')[0]]
        dps = [rt[c] for c in self.fsig[k].split('_')[0]]
        fa = x86_frame(cps, mine[1], mine[2])
        fb = x86_frame(dps, theirs[1], theirs[2])
        if fa is None or fb is None or (fa == fb and cps == dps):
            return None
        if mine[1] == theirs[1] and cps == dps:
            return None
        return fa, fb

    def _func_ref(self, o, symidx):
        s = o.syms[symidx]
        fi = s['index']
        # the C library and the original's imports are the host's, whatever
        # a map or an annotation says (a `sqrt` resolved through a comment to
        # BrSqrtF, which calls sqrt, recursed forever)
        if s['flags'] & F_UNDEF and s['name'] and self.plain(s['name']) in HOST_NAMES:
            nm = self.plain(s['name'])
            cn = 'h_' + cident(nm)
            if cn not in self.host:
                self.host[cn] = (sig_of(o.ftype(fi)), nm, None)
            return cn, None, None
        # The verified build's answer for this file's reference wins even over
        # a definition in the same file: br_objlife.c defines the D3D twin
        # BrInstall_1001BAE0, but its references resolve to 0x1001E080, the
        # Glide installer, in the certified image.
        if s['name']:
            path = self.path_of(o)
            sva = (self.sites.get((path, self.plain(s['name']), 0))
                   or self.sites.get((path, s['name'], 0)))
            if sva is not None and sva in self.by_va:
                k = self.by_va[sva]
                self.last_k = k
                return self.fname[k], self.fva[k], self.fsig[k]
        if not s['flags'] & F_UNDEF:
            if not s['flags'] & F_LOCAL and s['name'] in self.gfunc:
                go, gs = self.gfunc[s['name']]
                k = (go.oid, gs['index'])
            else:
                k = (o.oid, fi)
            self.last_k = k
            return self.fname[k], self.fva[k], self.fsig[k]
        nm = s['name']
        # The verified build's answer for THIS file's call first: a name can
        # be defined twice in the tree (br_pod.c's C BrPodOpen vs the C++
        # BrPodFile::Open the original has at 0x10008AB0), and the address
        # the certified image resolved the call to is the one that counts.
        path = self.path_of(o)
        sva = (self.sites.get((path, self.plain(nm), 0)) or self.sites.get((path, nm, 0)))
        if sva is not None:
            if sva in self.by_va:
                k = self.by_va[sva]
                self.last_k = k
                return self.fname[k], self.fva[k], self.fsig[k]
            if sva in THUNKS:
                nm = THUNKS[sva]
                cn = 'h_' + cident(nm)
                if cn not in self.host:
                    self.host[cn] = (sig_of(o.ftype(fi)), nm, None)
                return cn, None, None
        for cand in (nm, self.plain(nm)):
            if cand in self.gfunc:
                go, gs = self.gfunc[cand]
                k = (go.oid, gs['index'])
                self.last_k = k
                return self.fname[k], self.fva[k], self.fsig[k]
        # a C++ reference to a function defined under another spelling
        pn = self.plain(nm)
        k = self.by_plain.get(pn)
        if k is not None:
            self.last_k = k
            return self.fname[k], self.fva[k], self.fsig[k]
        base = self.base_of(o)
        path = self.path_of(o)
        va = (self.sites.get((path, pn, 0)) or self.sites.get((path, nm, 0))
              or self.scoped[base].get(pn) or self.scoped[base].get(nm)
              or self.glob.get(nm) or self.glob.get(pn)
              or addr_from_name(pn, TEXT_LO, TEXT_HI))
        if va is not None and va in self.by_va:
            k = self.by_va[va]
            self.last_k = k
            return self.fname[k], self.fva[k], self.fsig[k]
        if va is not None and va in THUNKS:
            # the original's import thunk: the call is to that import
            pn = THUNKS[va]
            va = None
        # a `jmp rel32` thunk in the original: the call is to its target
        hops = 0
        while va is not None and va in JMPS and va not in self.by_va and hops < 4:
            va = JMPS[va]
            hops += 1
            if va in self.by_va:
                k = self.by_va[va]
                self.last_k = k
                return self.fname[k], self.fva[k], self.fsig[k]
            if va in THUNKS:
                pn = THUNKS[va]
                va = None
        if va is not None and TEXT_LO <= va < TEXT_HI:
            # a game function the tree does not have: a trap naming its VA,
            # at that VA, so the original data tables that point there too
            # land on the same report
            cn = 'm_%08X' % va
            if cn not in self.missing:
                self.missing[cn] = (sig_of(o.ftype(fi)), pn, va)
            return cn, va, self.missing[cn][0]
        nm = pn
        cn = 'h_' + cident(nm)
        sig = sig_of(o.ftype(fi))
        prev = self.host.get(cn)
        if prev is None:
            self.host[cn] = (sig, nm, va)
        return cn, None, None


# -------------------------------------------------------------- translator --

LOADS = {
    0x28: ('i32', 'u32', 'u32'), 0x29: ('i64', 'u64', 'u64'),
    0x2A: ('f32', 'f32', 'f32'), 0x2B: ('f64', 'f64', 'f64'),
    0x2C: ('i32', 's8', 'u32'), 0x2D: ('i32', 'u8', 'u32'),
    0x2E: ('i32', 's16', 'u32'), 0x2F: ('i32', 'u16', 'u32'),
    0x30: ('i64', 's8', 'u64'), 0x31: ('i64', 'u8', 'u64'),
    0x32: ('i64', 's16', 'u64'), 0x33: ('i64', 'u16', 'u64'),
    0x34: ('i64', 's32', 'u64'), 0x35: ('i64', 'u32', 'u64'),
}
STORES = {0x36: ('i32', 'u32'), 0x37: ('i64', 'u64'), 0x38: ('f32', 'f32'),
          0x39: ('f64', 'f64'), 0x3A: ('i32', 'u8'), 0x3B: ('i32', 'u16'),
          0x3C: ('i64', 'u8'), 0x3D: ('i64', 'u16'), 0x3E: ('i64', 'u32')}

# (operand types, result type, C template with {0},{1})
BIN = {
    0x46: ('i32', 'i32', '(u32)({0} == {1})'), 0x47: ('i32', 'i32', '(u32)({0} != {1})'),
    0x48: ('i32', 'i32', '(u32)((s32){0} < (s32){1})'), 0x49: ('i32', 'i32', '(u32)({0} < {1})'),
    0x4A: ('i32', 'i32', '(u32)((s32){0} > (s32){1})'), 0x4B: ('i32', 'i32', '(u32)({0} > {1})'),
    0x4C: ('i32', 'i32', '(u32)((s32){0} <= (s32){1})'), 0x4D: ('i32', 'i32', '(u32)({0} <= {1})'),
    0x4E: ('i32', 'i32', '(u32)((s32){0} >= (s32){1})'), 0x4F: ('i32', 'i32', '(u32)({0} >= {1})'),
    0x51: ('i64', 'i32', '(u32)({0} == {1})'), 0x52: ('i64', 'i32', '(u32)({0} != {1})'),
    0x53: ('i64', 'i32', '(u32)((s64){0} < (s64){1})'), 0x54: ('i64', 'i32', '(u32)({0} < {1})'),
    0x55: ('i64', 'i32', '(u32)((s64){0} > (s64){1})'), 0x56: ('i64', 'i32', '(u32)({0} > {1})'),
    0x57: ('i64', 'i32', '(u32)((s64){0} <= (s64){1})'), 0x58: ('i64', 'i32', '(u32)({0} <= {1})'),
    0x59: ('i64', 'i32', '(u32)((s64){0} >= (s64){1})'), 0x5A: ('i64', 'i32', '(u32)({0} >= {1})'),
    0x5B: ('f32', 'i32', '(u32)({0} == {1})'), 0x5C: ('f32', 'i32', '(u32)({0} != {1})'),
    0x5D: ('f32', 'i32', '(u32)({0} < {1})'), 0x5E: ('f32', 'i32', '(u32)({0} > {1})'),
    0x5F: ('f32', 'i32', '(u32)({0} <= {1})'), 0x60: ('f32', 'i32', '(u32)({0} >= {1})'),
    0x61: ('f64', 'i32', '(u32)({0} == {1})'), 0x62: ('f64', 'i32', '(u32)({0} != {1})'),
    0x63: ('f64', 'i32', '(u32)({0} < {1})'), 0x64: ('f64', 'i32', '(u32)({0} > {1})'),
    0x65: ('f64', 'i32', '(u32)({0} <= {1})'), 0x66: ('f64', 'i32', '(u32)({0} >= {1})'),
    0x6A: ('i32', 'i32', '({0} + {1})'), 0x6B: ('i32', 'i32', '({0} - {1})'),
    0x6C: ('i32', 'i32', '({0} * {1})'), 0x6D: ('i32', 'i32', 'w_divs32({0}, {1})'),
    0x6E: ('i32', 'i32', 'w_divu32({0}, {1})'), 0x6F: ('i32', 'i32', 'w_rems32({0}, {1})'),
    0x70: ('i32', 'i32', 'w_remu32({0}, {1})'), 0x71: ('i32', 'i32', '({0} & {1})'),
    0x72: ('i32', 'i32', '({0} | {1})'), 0x73: ('i32', 'i32', '({0} ^ {1})'),
    0x74: ('i32', 'i32', '({0} << ({1} & 31))'), 0x75: ('i32', 'i32', '(u32)((s32){0} >> ({1} & 31))'),
    0x76: ('i32', 'i32', '({0} >> ({1} & 31))'), 0x77: ('i32', 'i32', 'w_rotl32({0}, {1})'),
    0x78: ('i32', 'i32', 'w_rotr32({0}, {1})'),
    0x7C: ('i64', 'i64', '({0} + {1})'), 0x7D: ('i64', 'i64', '({0} - {1})'),
    0x7E: ('i64', 'i64', '({0} * {1})'), 0x7F: ('i64', 'i64', 'w_divs64({0}, {1})'),
    0x80: ('i64', 'i64', 'w_divu64({0}, {1})'), 0x81: ('i64', 'i64', 'w_rems64({0}, {1})'),
    0x82: ('i64', 'i64', 'w_remu64({0}, {1})'), 0x83: ('i64', 'i64', '({0} & {1})'),
    0x84: ('i64', 'i64', '({0} | {1})'), 0x85: ('i64', 'i64', '({0} ^ {1})'),
    0x86: ('i64', 'i64', '({0} << ({1} & 63))'), 0x87: ('i64', 'i64', '(u64)((s64){0} >> ({1} & 63))'),
    0x88: ('i64', 'i64', '({0} >> ({1} & 63))'), 0x89: ('i64', 'i64', 'w_rotl64({0}, {1})'),
    0x8A: ('i64', 'i64', 'w_rotr64({0}, {1})'),
    0x92: ('f32', 'f32', '({0} + {1})'), 0x93: ('f32', 'f32', '({0} - {1})'),
    0x94: ('f32', 'f32', '({0} * {1})'), 0x95: ('f32', 'f32', '({0} / {1})'),
    0x96: ('f32', 'f32', 'w_fminf({0}, {1})'), 0x97: ('f32', 'f32', 'w_fmaxf({0}, {1})'),
    0x98: ('f32', 'f32', 'copysignf({0}, {1})'),
    0xA0: ('f64', 'f64', '({0} + {1})'), 0xA1: ('f64', 'f64', '({0} - {1})'),
    0xA2: ('f64', 'f64', '({0} * {1})'), 0xA3: ('f64', 'f64', '({0} / {1})'),
    0xA4: ('f64', 'f64', 'w_fmin({0}, {1})'), 0xA5: ('f64', 'f64', 'w_fmax({0}, {1})'),
    0xA6: ('f64', 'f64', 'copysign({0}, {1})'),
}
UN = {
    0x45: ('i32', 'i32', '(u32)({0} == 0)'), 0x50: ('i64', 'i32', '(u32)({0} == 0)'),
    0x67: ('i32', 'i32', 'w_clz32({0})'), 0x68: ('i32', 'i32', 'w_ctz32({0})'),
    0x69: ('i32', 'i32', '(u32)__builtin_popcount({0})'),
    0x79: ('i64', 'i64', 'w_clz64({0})'), 0x7A: ('i64', 'i64', 'w_ctz64({0})'),
    0x7B: ('i64', 'i64', '(u64)__builtin_popcountll({0})'),
    0x8B: ('f32', 'f32', 'fabsf({0})'), 0x8C: ('f32', 'f32', '(-{0})'),
    0x8D: ('f32', 'f32', 'ceilf({0})'), 0x8E: ('f32', 'f32', 'floorf({0})'),
    0x8F: ('f32', 'f32', 'truncf({0})'), 0x90: ('f32', 'f32', 'nearbyintf({0})'),
    0x91: ('f32', 'f32', 'sqrtf({0})'),
    0x99: ('f64', 'f64', 'fabs({0})'), 0x9A: ('f64', 'f64', '(-{0})'),
    0x9B: ('f64', 'f64', 'ceil({0})'), 0x9C: ('f64', 'f64', 'floor({0})'),
    0x9D: ('f64', 'f64', 'trunc({0})'), 0x9E: ('f64', 'f64', 'nearbyint({0})'),
    0x9F: ('f64', 'f64', 'sqrt({0})'),
    0xA7: ('i64', 'i32', '(u32){0}'),
    0xA8: ('f32', 'i32', 'w_trunc_s32_f({0})'), 0xA9: ('f32', 'i32', 'w_trunc_u32_f({0})'),
    0xAA: ('f64', 'i32', 'w_trunc_s32_d({0})'), 0xAB: ('f64', 'i32', 'w_trunc_u32_d({0})'),
    0xAC: ('i32', 'i64', '(u64)(s64)(s32){0}'), 0xAD: ('i32', 'i64', '(u64){0}'),
    0xAE: ('f32', 'i64', 'w_trunc_s64_f({0})'), 0xAF: ('f32', 'i64', 'w_trunc_u64_f({0})'),
    0xB0: ('f64', 'i64', 'w_trunc_s64_d({0})'), 0xB1: ('f64', 'i64', 'w_trunc_u64_d({0})'),
    0xB2: ('i32', 'f32', '(f32)(s32){0}'), 0xB3: ('i32', 'f32', '(f32){0}'),
    0xB4: ('i64', 'f32', '(f32)(s64){0}'), 0xB5: ('i64', 'f32', '(f32){0}'),
    0xB6: ('f64', 'f32', '(f32){0}'),
    0xB7: ('i32', 'f64', '(f64)(s32){0}'), 0xB8: ('i32', 'f64', '(f64){0}'),
    0xB9: ('i64', 'f64', '(f64)(s64){0}'), 0xBA: ('i64', 'f64', '(f64){0}'),
    0xBB: ('f32', 'f64', '(f64){0}'),
    0xBC: ('f32', 'i32', 'w_bits_f({0})'), 0xBD: ('f64', 'i64', 'w_bits_d({0})'),
    0xBE: ('i32', 'f32', 'w_f_bits({0})'), 0xBF: ('i64', 'f64', 'w_d_bits({0})'),
    0xC0: ('i32', 'i32', '(u32)(s32)(s8){0}'), 0xC1: ('i32', 'i32', '(u32)(s32)(s16){0}'),
    0xC2: ('i64', 'i64', '(u64)(s64)(s8){0}'), 0xC3: ('i64', 'i64', '(u64)(s64)(s16){0}'),
    0xC4: ('i64', 'i64', '(u64)(s64)(s32){0}'),
}
SAT = {0: ('f32', 'i32', 'w_sat_s32_f({0})'), 1: ('f32', 'i32', 'w_sat_u32_f({0})'),
       2: ('f64', 'i32', 'w_sat_s32_d({0})'), 3: ('f64', 'i32', 'w_sat_u32_d({0})'),
       4: ('f32', 'i64', 'w_sat_s64_f({0})'), 5: ('f32', 'i64', 'w_sat_u64_f({0})'),
       6: ('f64', 'i64', 'w_sat_s64_d({0})'), 7: ('f64', 'i64', 'w_sat_u64_d({0})')}


def fconst32(bits):
    return 'w_f_bits(0x%08XU)' % bits


def fconst64(bits):
    return 'w_d_bits(0x%016XULL)' % bits


class Fn:
    """Translate one function body."""

    def __init__(self, L, o, fidx, out):
        self.L, self.o, self.fidx, self.out = L, o, fidx, out
        self.ft = o.ftype(fidx)
        self.decls = set()
        self.lines = []
        self.label = 0
        self.relat = {}
        # reloc lookup by absolute file offset
        for ty, off, idx, add in o.relocs.get(o.code_sec, []):
            self.relat[o.code_off + off] = (ty, idx, add)

    def var(self, t, d):
        v = '%s%d' % (PFX[t], d)
        self.decls.add((t, v))
        return v

    def emit(self, s):
        self.lines.append('  ' + s)

    def run(self, start, size):
        o, r = self.o, R(self.o.b, start)
        end = start + size
        params, results = self.ft
        self.locals = list(params)
        for _ in range(r.uleb()):
            n = r.uleb()
            t = VT[r.u8()]
            self.locals += [t] * n
        self.np = len(params)
        st = []                   # stack of types
        # control stack entries: dict(kind, label, height, results, params, dead)
        ctl = [{'kind': 'func', 'label': 'Lret', 'height': 0,
                'results': results, 'params': []}]
        dead = False
        L = self.L

        cval = {}          # stack depth -> constant pushed there (i32.const, no reloc)

        def push(t):
            cval.pop(len(st), None)
            v = self.var(t, len(st))
            st.append(t)
            return v

        def pop():
            t = st.pop()
            return '%s%d' % (PFX[t], len(st))

        def top_types(n):
            return st[len(st) - n:]

        def blocktype():
            b = o.b[r.p]
            if b == 0x40:
                r.p += 1
                return [], []
            if b in VT:
                r.p += 1
                return [], [VT[b]]
            ti = r.sleb()
            return list(o.types[ti][0]), list(o.types[ti][1])

        def branch_copy(c):
            # move the top len(arity) values to the target's slots
            ar = c['params'] if c['kind'] == 'loop' else c['results']
            n = len(ar)
            for k in range(n):
                src = '%s%d' % (PFX[st[len(st) - n + k]], len(st) - n + k)
                dst = self.var(ar[k], c['height'] + k)
                if src != dst:
                    self.emit('%s = %s;' % (dst, src))

        def target(n):
            return ctl[len(ctl) - 1 - n]

        def goto(c):
            if c['kind'] == 'func':
                if c['results']:
                    return 'return %s%d;' % (PFX[c['results'][0]], c['height'])
                return 'return;'
            return 'goto %s;' % (c['label'] if c['kind'] != 'loop' else c['label'] + 'top')

        def newlabel():
            self.label += 1
            return 'L%d' % self.label

        def imm_reloc(pos):
            return self.relat.get(pos)

        def memarg():
            r.uleb()
            pos = r.p
            off = r.uleb()
            rel = imm_reloc(pos)
            if rel:
                ty, idx, add = rel
                off = L.data_addr(o, idx, add)
            return off

        def i32const():
            pos = r.p
            v = r.sleb() & 0xFFFFFFFF
            rel = imm_reloc(pos)
            if rel:
                ty, idx, add = rel
                if ty in (3, 4, 5, 11, 21):        # memory address
                    v = L.data_addr(o, idx, add)
                elif ty in (1, 2, 18):             # table index (func addr)
                    cn, va, sig = L.func_ref(o, idx)
                    if va is None:
                        hs = L.host[cn][0]
                        self.out.externs[cn] = hs
                        return 'w_addr_of_host((void *)&%s, "%s", "%s")' % (
                            cn, hs, L.host[cn][1])
                    v = va
                else:
                    raise SystemExit('%s: reloc type %d in i32.const'
                                     % (o.path, ty))
            return '0x%XU' % v

        while r.p < end:
            op = r.u8()
            if dead and op not in (0x02, 0x03, 0x04, 0x05, 0x0B):
                # skip immediates of dead instructions
                self.skip_imm(r, op)
                continue
            if op == 0x00:
                self.emit('w_trap("unreachable in %s");' % self.cname)
                dead = True
            elif op == 0x01:
                pass
            elif op in (0x02, 0x03):
                if dead:
                    bt = blocktype()
                    ctl.append({'kind': 'dead', 'label': None, 'height': len(st),
                                'results': [], 'params': []})
                    continue
                ps, rs = blocktype()
                h = len(st) - len(ps)
                lab = newlabel()
                c = {'kind': 'block' if op == 0x02 else 'loop', 'label': lab,
                     'height': h, 'results': rs, 'params': ps}
                ctl.append(c)
                if op == 0x03:
                    self.lines.append('%stop:;' % lab)
            elif op == 0x04:
                if dead:
                    blocktype()
                    ctl.append({'kind': 'dead', 'label': None, 'height': len(st),
                                'results': [], 'params': []})
                    continue
                ps, rs = blocktype()
                cond = pop()
                h = len(st) - len(ps)
                lab = newlabel()
                c = {'kind': 'if', 'label': lab, 'height': h, 'results': rs,
                     'params': ps, 'saved': list(st)}
                ctl.append(c)
                self.emit('if (!%s) goto %selse;' % (cond, lab))
            elif op == 0x05:
                c = ctl[-1]
                if c['kind'] == 'dead':
                    continue
                if not dead:
                    branch_copy(c)
                    self.emit('goto %s;' % c['label'])
                self.lines.append('%selse:;' % c['label'])
                c['kind'] = 'else'
                st[:] = c['saved']
                dead = False
            elif op == 0x0B:
                c = ctl.pop()
                if c['kind'] == 'dead':
                    continue
                if c['kind'] == 'func':
                    if not dead:
                        if c['results']:
                            self.emit('return %s;' % pop())
                    break
                if c['kind'] == 'if':
                    # no else: the false edge lands here
                    self.lines.append('%selse:;' % c['label'])
                if c['kind'] != 'loop':
                    self.lines.append('%s:;' % c['label'])
                st[:] = st[:c['height']]
                for t in c['results']:
                    push(t)
                dead = False
            elif op == 0x0C:
                c = target(r.uleb())
                branch_copy(c)
                self.emit(goto(c))
                dead = True
            elif op == 0x0D:
                c = target(r.uleb())
                cond = pop()
                self.emit('if (%s) {' % cond)
                branch_copy(c)
                self.emit(goto(c) + ' }')
            elif op == 0x0E:
                n = r.uleb()
                tg = [r.uleb() for _ in range(n + 1)]
                idx = pop()
                self.emit('switch (%s) {' % idx)
                for k, t in enumerate(tg):
                    c = target(t)
                    self.emit('%s' % ('default:' if k == n else 'case %d:' % k))
                    branch_copy(c)
                    self.emit(goto(c))
                self.emit('}')
                dead = True
            elif op == 0x0F:
                c = ctl[0]
                if c['results']:
                    self.emit('return %s;' % pop())
                else:
                    self.emit('return;')
                dead = True
            elif op == 0x10:
                pos = r.p
                fi = r.uleb()
                rel = imm_reloc(pos)
                assert rel and rel[0] == 0, 'call without reloc in %s' % o.path
                cn, va, dsig = L.func_ref(o, rel[1])
                ps, rs = o.ftype(fi)
                args = [pop() for _ in ps][::-1]
                csig = sig_of((ps, rs))
                br = L.x86_bridge(o, rel[1], csig, L.last_k) if dsig is not None else None
                if br is not None:
                    key = (csig, br[0], dsig, br[1])
                    aid = L.x86_adapters.setdefault(key, len(L.x86_adapters))
                    self.out.x86[key] = aid
                    call = 'w_x86_%d(%s%s)' % (aid, '(void *)&' + cn, ''.join(', ' + a for a in args))
                    self.out.externs[cn] = dsig
                elif dsig is not None and dsig != csig:
                    call = 'w_adapt_%s_%s(%s%s)' % (
                        csig, dsig, '(void *)&' + cn, ''.join(', ' + a for a in args))
                    self.out.adapters.add((csig, dsig))
                    self.out.externs[cn] = dsig
                else:
                    call = '%s(%s)' % (cn, ', '.join(args))
                    self.out.externs[cn] = csig if dsig is None else dsig
                if rs:
                    v = push(rs[0])
                    self.emit('%s = %s;' % (v, call))
                else:
                    self.emit('%s;' % call)
            elif op == 0x11:
                ti = r.uleb()
                r.uleb()
                ps, rs = o.types[ti]
                addr = pop()
                mk = cval.get(len(st) - 1) if ps and ps[-1] == 'i32' else None
                args = [pop() for _ in ps][::-1]
                if mk is not None and (mk & 0xFFC00000) == MAGIC:
                    # the caller's x86 convention, carried by ccmark.py
                    ps = ps[:-1]
                    args = args[:-1]
                    sig = sig_of((ps, rs))
                    self.out.isigs.add(sig)
                    call = 'w_icallx_%s(%s, 0x%XU%s)' % (sig, addr, mk & 0x3FFFFF,
                                                        ''.join(', ' + a for a in args))
                else:
                    sig = sig_of((ps, rs))
                    self.out.isigs.add(sig)
                    call = 'w_icall_%s(%s%s)' % (sig, addr, ''.join(', ' + a for a in args))
                if rs:
                    v = push(rs[0])
                    self.emit('%s = %s;' % (v, call))
                else:
                    self.emit('%s;' % call)
            elif op == 0x1A:
                pop()
            elif op in (0x1B, 0x1C):
                if op == 0x1C:
                    for _ in range(r.uleb()):
                        r.u8()
                c = pop()
                b = pop()
                a = pop()
                t = st[-0] if False else None
                # type of result = type of a
                ta = a[0]
                tt = {v: k for k, v in PFX.items()}[ta]
                v = push(tt)
                self.emit('%s = %s ? %s : %s;' % (v, c, a, b))
            elif op == 0x20:
                i = r.uleb()
                t = self.locals[i]
                v = push(t)
                self.emit('%s = %s;' % (v, self.lname(i)))
            elif op == 0x21:
                i = r.uleb()
                self.emit('%s = %s;' % (self.lname(i), pop()))
            elif op == 0x22:
                i = r.uleb()
                self.emit('%s = %s%d;' % (self.lname(i), PFX[st[-1]], len(st) - 1))
            elif op == 0x23:
                pos = r.p
                gi = r.uleb()
                v = push(self.gtype(gi))
                self.emit('%s = %s;' % (v, self.gname(gi, pos)))
            elif op == 0x24:
                pos = r.p
                gi = r.uleb()
                self.emit('%s = %s;' % (self.gname(gi, pos), pop()))
            elif op in LOADS:
                t, mt, ct = LOADS[op]
                off = memarg()
                a = pop()
                v = push(t)
                self.emit('%s = (%s)W_LD(%s, %s, 0x%XULL);' % (v, CT[t], mt, a, off))
            elif op in STORES:
                t, mt = STORES[op]
                off = memarg()
                val = pop()
                a = pop()
                self.emit('W_ST(%s, %s, 0x%XULL, %s);' % (mt, a, off, val))
            elif op == 0x3F:
                r.uleb()
                v = push('i32')
                self.emit('%s = w_memory_size();' % v)
            elif op == 0x40:
                r.uleb()
                a = pop()
                v = push('i32')
                self.emit('%s = w_memory_grow(%s);' % (v, a))
            elif op == 0x41:
                e = i32const()
                v = push('i32')
                self.emit('%s = %s;' % (v, e))
                if re.match(r'0x[0-9A-F]+U$', e):
                    cval[len(st) - 1] = int(e[2:-1], 16)
            elif op == 0x42:
                val = r.sleb() & 0xFFFFFFFFFFFFFFFF
                v = push('i64')
                self.emit('%s = 0x%XULL;' % (v, val))
            elif op == 0x43:
                bits = struct.unpack('<I', r.bytes(4))[0]
                v = push('f32')
                self.emit('%s = %s;' % (v, fconst32(bits)))
            elif op == 0x44:
                bits = struct.unpack('<Q', r.bytes(8))[0]
                v = push('f64')
                self.emit('%s = %s;' % (v, fconst64(bits)))
            elif op in BIN:
                ta, tr, tpl = BIN[op]
                b = pop()
                a = pop()
                v = push(tr)
                self.emit('%s = %s;' % (v, tpl.format(a, b)))
            elif op in UN:
                ta, tr, tpl = UN[op]
                a = pop()
                v = push(tr)
                self.emit('%s = %s;' % (v, tpl.format(a)))
            elif op == 0xFC:
                sub = r.uleb()
                if sub in SAT:
                    ta, tr, tpl = SAT[sub]
                    a = pop()
                    v = push(tr)
                    self.emit('%s = %s;' % (v, tpl.format(a)))
                elif sub == 10:
                    r.u8()
                    r.u8()
                    n = pop()
                    s = pop()
                    d = pop()
                    self.emit('memmove(W_P(%s), W_P(%s), %s);' % (d, s, n))
                elif sub == 11:
                    r.u8()
                    n = pop()
                    val = pop()
                    d = pop()
                    self.emit('memset(W_P(%s), (int)%s, %s);' % (d, val, n))
                else:
                    raise SystemExit('%s: 0xFC %d' % (o.path, sub))
            else:
                raise SystemExit('%s: opcode %#x in %s' % (o.path, op, self.cname))
        # body text
        return self.lines

    def skip_imm(self, r, op):
        if op in (0x0C, 0x0D, 0x10, 0x20, 0x21, 0x22, 0x23, 0x24, 0x3F, 0x40):
            r.uleb()
        elif op == 0x0E:
            for _ in range(r.uleb() + 1):
                r.uleb()
        elif op == 0x11:
            r.uleb()
            r.uleb()
        elif op in LOADS or op in STORES:
            r.uleb()
            r.uleb()
        elif op == 0x41 or op == 0x42:
            r.sleb()
        elif op == 0x43:
            r.p += 4
        elif op == 0x44:
            r.p += 8
        elif op == 0x1C:
            for _ in range(r.uleb()):
                r.u8()
        elif op == 0xFC:
            sub = r.uleb()
            if sub == 10:
                r.p += 2
            elif sub == 11:
                r.p += 1

    def lname(self, i):
        return 'p%d' % i if i < self.np else 'v%d' % i

    def gtype(self, gi):
        o = self.o
        if gi < o.ngimp:
            return o.gimports[gi][2]
        return o.globals[gi - o.ngimp][0]

    def gname(self, gi, pos):
        o = self.o
        if gi < o.ngimp:
            nm = o.gimports[gi][1]
            if nm == '__stack_pointer':
                return 'w_sp'
            raise SystemExit('%s: imported global %s' % (o.path, nm))
        raise SystemExit('%s: defined global %d' % (o.path, gi))


class Out:
    def __init__(self):
        self.externs = {}
        self.adapters = set()
        self.isigs = set()
        self.x86 = {}


def csig_decl(sig, name, static=False):
    p, r = sig.split('_')
    rt = {'i': 'u32', 'I': 'u64', 'f': 'f32', 'F': 'f64'}
    ret = rt[r] if r else 'void'
    args = ', '.join(rt[c] for c in p) or 'void'
    return '%s%s %s(%s)' % ('static ' if static else '', ret, name, args)


def sig_types(sig):
    rt = {'i': 'u32', 'I': 'u64', 'f': 'f32', 'F': 'f64'}
    p, r = sig.split('_')
    return [rt[c] for c in p], (rt[r] if r else 'void')


def translate(L, o, outdir):
    out = Out()
    body = []
    defined = []
    for i, s in enumerate(o.syms):
        if s['kind'] != 0 or s['flags'] & F_UNDEF:
            continue
    for bi, (fs, sz) in enumerate(o.bodies):
        fidx = o.nfimp + bi
        cn = L.fname.get((o.oid, fidx))
        if cn is None:
            cn = 'l%d_anon%d' % (o.oid, fidx)
            L.fname[(o.oid, fidx)] = cn
            L.fsig[(o.oid, fidx)] = sig_of(o.ftype(fidx))
            L.fva[(o.oid, fidx)] = None
        f = Fn(L, o, fidx, out)
        f.cname = cn
        lines = f.run(fs, sz)
        ps, rs = o.ftype(fidx)
        static = cn.startswith('l')
        hdr = '%s%s %s(%s)' % ('static ' if static and False else '',
                                CT[rs[0]] if rs else 'void', cn,
                                ', '.join('%s p%d' % (CT[t], k) for k, t in enumerate(ps)) or 'void')
        dec = ['  %s v%d = 0;' % (CT[t], k) for k, t in enumerate(f.locals) if k >= len(ps)]
        dec += ['  %s %s;' % (CT[t], v) for t, v in sorted(f.decls)]
        tr = ['  W_TRACE("%s");' % cn]
        body.append(hdr + ' {\n' + '\n'.join(dec + tr + lines) + '\n}\n')
        defined.append(cn)
    src = ['/* generated by ports/macos/wasm/w2c.py from %s -- do not edit */'
           % os.path.basename(o.path), '#include "w2c_rt.h"']
    for sig in sorted(out.isigs):
        src.append('W_ICALL_DECL(%s)' % sig)
    for a, b in sorted(out.adapters):
        src.append('W_ADAPT_DECL(%s, %s)' % (a, b))
    for cn, sig in sorted(out.externs.items()):
        if cn in defined:
            continue
        src.append(csig_decl(sig, cn) + ';')
    for cn in defined:
        sig = L.fsig[(o.oid, o.nfimp + defined.index(cn))] if False else None
    # forward-declare every function of this object
    for bi in range(len(o.bodies)):
        fidx = o.nfimp + bi
        src.append(csig_decl(L.fsig[(o.oid, fidx)], L.fname[(o.oid, fidx)]) + ';')
    src += body
    path = os.path.join(outdir, 'o%04d.c' % o.oid)
    open(path, 'w').write('\n'.join(src) + '\n')
    return out


def write_link(L, outs, outdir):
    """Dispatch registration, data images, host stubs, adapters."""
    # the host layer calls into the game through these (WndProc, thread
    # start routines, qsort comparators, atexit handlers)
    allsig = {'_', '_i', 'i_i', 'ii_i', 'iii_i', 'iiii_i', 'iiiii_i', 'i_'}
    adapters = set()
    for out in outs:
        allsig |= out.isigs
        adapters |= out.adapters
    lines = ['/* generated by w2c.py: the link -- do not edit */',
             '#include "w2c_rt.h"', '']
    # every function's declaration
    funcs = sorted(((L.fname[k], L.fsig[k], L.fva.get(k)) for k in L.fname),
                   key=lambda x: x[0])
    for cn, sig, va in funcs:
        lines.append(csig_decl(sig, cn) + ';')
    # host imports: weak trap stubs the host layer overrides
    for cn, (sig, nm, va) in sorted(L.host.items()):
        ps, rt = sig_types(sig)
        args = ', '.join('%s x%d' % (t, i) for i, t in enumerate(ps)) or 'void'
        ret = '' if rt == 'void' else ' return 0;'
        lines.append('__attribute__((weak)) %s %s(%s) { w_missing("%s");%s }'
                     % (rt, cn, args, nm, ret))
    for cn, (sig, nm, va) in sorted(L.missing.items()):
        ps, rt = sig_types(sig)
        args = ', '.join('%s x%d' % (t, i) for i, t in enumerate(ps)) or 'void'
        ret = '' if rt == 'void' else ' return 0;'
        lines.append('%s %s(%s) { w_missing_game(0x%08XU, "%s");%s }'
                     % (rt, cn, args, va, nm, ret))
    lines.append('')
    lines.append('const w_fentry w_functions[] = {')
    ccof = {}
    for k, cn in L.fname.items():
        if k in L.fdef:
            co, nm = L.fdef[k]
            c = L.conv_of(co).get(nm)
            if c:
                code = {'x86_thiscallcc': 1, 'x86_fastcallcc': 2, 'x86_stdcallcc': 3}.get(c[1], 0)
                mask = sum(1 << i for i, f in enumerate(c[2]) if f)
                ccof[cn] = (code << 20) | mask
    for cn, sig, va in funcs:
        if va is None:
            continue
        lines.append('  { 0x%08XU, "%s", (void *)&%s, "%s", 0x%XU },' % (
            va, sig, cn, cn, ccof.get(cn, 0)))
    for cn, (sig, nm, va) in sorted(L.missing.items()):
        lines.append('  { 0x%08XU, "%s", (void *)&%s, "%s", 0 },' % (va, sig, cn, nm))
    lines.append('  { 0, 0, 0, 0, 0 } };')
    # host function addresses (address taken of a host import)
    lines.append('const w_hentry w_host_functions[] = {')
    for cn, (sig, nm, va) in sorted(L.host.items()):
        lines.append('  { "%s", "%s", (void *)&%s },' % (nm, sig, cn))
    lines.append('  { 0, 0, 0 } };')
    # port data segments
    blob = bytearray()
    segrows = []
    relrows = []
    for o in L.objs:
        rel = defaultdict(list)
        for ty, off, idx, add in o.relocs.get(o.data_sec, []) if o.data_sec is not None else []:
            rel_seg = None
            for si, (po, data) in enumerate(o.data):
                if po <= off < po + len(data):
                    rel_seg = si
                    break
            if rel_seg is None:
                continue
            rel[rel_seg].append((ty, off - o.data[rel_seg][0], idx, add))
        for si, (po, data) in enumerate(o.data):
            addr = L.seg_addr[(o.oid, si)]
            if (o.oid, si, addr) not in L.port_set:
                # mapped onto the original image: its bytes come from there
                continue
            d = bytearray(data)
            for ty, off, idx, add in rel.get(si, []):
                if ty == 5:          # MEMORY_ADDR_I32
                    v = L.data_addr(o, idx, add)
                    struct.pack_into('<I', d, off, v)
                elif ty == 2:        # TABLE_INDEX_I32
                    cn, va, sig = L.func_ref(o, idx)
                    if va is None:
                        relrows.append((addr + off, cn))
                        v = 0
                    else:
                        v = va
                    struct.pack_into('<I', d, off, v)
                else:
                    raise SystemExit('%s: data reloc type %d' % (o.path, ty))
            if any(d):
                segrows.append((addr, len(blob), len(d)))
                blob += d
    open(os.path.join(outdir, 'portdata.bin'), 'wb').write(bytes(blob))
    lines.append('const w_segentry w_port_segments[] = {')
    for a, off, n in segrows:
        lines.append('  { 0x%08XU, %d, %d },' % (a, off, n))
    lines.append('  { 0, 0, 0 } };')
    lines.append('const w_hostfix w_port_hostfix[] = {')
    for a, cn in relrows:
        lines.append('  { 0x%08XU, (void *)&%s },' % (a, cn))
    lines.append('  { 0, 0 } };')
    lines.append('const u32 w_port_end = 0x%08XU;' % L.port_end)
    lines.append('const w_iatent w_iat[] = {')
    for slot, n in sorted(IAT.items()):
        lines.append('  { 0x%08XU, "%s" },' % (slot, n))
    lines.append('  { 0, 0 } };')
    # init functions (C++ static constructors), priority order
    inits = []
    for o in L.objs:
        for prio, symidx in o.init_funcs:
            cn, va, sig = L.func_ref(o, symidx)
            inits.append((prio, cn))
    lines.append('void w_run_inits(void) {')
    for prio, cn in sorted(inits):
        lines.append('  %s();' % cn)
    lines.append('}')
    # orphans, for the report
    lines.append('const w_orphan w_orphans[] = {')
    for nm, a in sorted(L.orphans.items(), key=lambda x: x[1]):
        lines.append('  { 0x%08XU, "%s" },' % (a, nm))
    lines.append('  { 0, 0 } };')
    open(os.path.join(outdir, 'w2c_link.c'), 'w').write('\n'.join(lines) + '\n')
    # indirect-call and adapter thunks
    th = ['/* generated by w2c.py: call thunks -- do not edit */',
          '#include "w2c_rt.h"', '']
    for sig in sorted(allsig):
        ps, rt = sig_types(sig)
        args = ''.join(', %s x%d' % (t, i) for i, t in enumerate(ps))
        call = ', '.join('x%d' % i for i in range(len(ps)))
        # w_icallx_<sig>: a caller with a thiscall/fastcall convention
        th.append('%s w_icallx_%s(u32 a, u32 cc%s) {' % (rt, sig, args))
        th.append('  const w_fentry *e = w_lookup(a);')
        th.append('  if (e->cc == cc && !strcmp(e->sig, "%s")) { %s }' % (
            sig, ('((%s (*)(%s))e->fn)(%s); return;' % (rt, ', '.join(ps) or 'void', call)) if rt == 'void'
            else ('return ((%s (*)(%s))e->fn)(%s);' % (rt, ', '.join(ps) or 'void', call))))
        th.append('  { u32 w[32]; int n = 0;')
        for i, t in enumerate(ps):
            th.append('    n = w_flat_%s(w, n, x%d);' % (t, i))
        conv = {'u32': '(u32)', 'u64': '', 'f32': 'w_f_bits((u32)', 'f64': 'w_d_bits('}
        if rt == 'void':
            th.append('    w_x86_call(e, cc, "%s", w); }' % sig)
        else:
            th.append('    return %sw_x86_call(e, cc, "%s", w)%s; }' % (
                conv[rt], sig, ')' if rt in ('f32', 'f64') else ''))
        th.append('}')
        th.append('%s w_icall_%s(u32 a%s) {' % (rt, sig, args))
        th.append('  const w_fentry *e = w_lookup(a);')
        fcall = '((%s (*)(%s))e->fn)(%s)' % (rt, ', '.join(ps) or 'void', call)
        th.append('  if (e->cc == 0 && !strcmp(e->sig, "%s")) { %s }' % (
            sig, ('%s; return;' % fcall) if rt == 'void' else ('return %s;' % fcall)))
        th.append('  { u32 w[32]; int n = 0;')
        for i, t in enumerate(ps):
            th.append('    n = w_flat_%s(w, n, x%d);' % (t, i))
        conv = {'u32': '(u32)', 'u64': '', 'f32': 'w_f_bits((u32)', 'f64': 'w_d_bits('}
        if rt == 'void':
            th.append('    w_x86_call(e, 0, "%s", w); }' % sig)
        else:
            th.append('    return %sw_x86_call(e, 0, "%s", w)%s; }' % (
                conv[rt], sig, ')' if rt in ('f32', 'f64') else ''))
        th.append('}')
    for a, b in sorted(adapters):
        ps, rt = sig_types(a)
        pd, rd = sig_types(b)
        args = ''.join(', %s x%d' % (t, i) for i, t in enumerate(ps))
        th.append('%s w_adapt_%s_%s(void *fn%s) {' % (rt, a, b, args))
        th.append('  u32 w[32] = {0}; int n = 0;')
        for i, t in enumerate(ps):
            th.append('  n = w_flat_%s(w, n, x%d);' % (t, i))
        k = 0
        cargs = []
        for t in pd:
            cargs.append('w_unflat_%s(w, %d)' % (t, k))
            k += 2 if t in ('u64', 'f64') else 1
        call = '((%s (*)(%s))fn)(%s)' % (rd, ', '.join(pd) or 'void', ', '.join(cargs))
        if rt == 'void':
            th.append('  %s;' % call)
        elif rd == 'void':
            th.append('  %s; return 0;' % call)
        else:
            th.append('  return w_conv_%s_%s(%s);' % (rd, rt, call))
        th.append('}')
    open(os.path.join(outdir, 'w2c_thunks.c'), 'w').write('\n'.join(th) + '\n')
    # generic callers: call a function of signature S with arguments read
    # from 32-bit stack words (the mismatch path of an indirect call)
    dsigs = set(sig for _cn, sig, _va in funcs) | set(v[0] for v in L.host.values())
    dsigs |= set(v[0] for v in L.missing.values())
    for sig in sorted(dsigs):
        ps, rt = sig_types(sig)
        k = 0
        args = []
        for t in ps:
            args.append('w_unflat_%s(w, %d)' % (t, k))
            k += 2 if t in ('u64', 'f64') else 1
        call = '((%s (*)(%s))fn)(%s)' % (rt, ', '.join(ps) or 'void', ', '.join(args))
        if rt == 'void':
            body = '%s; return 0;' % call
        elif rt == 'f32':
            body = 'return w_bits_f(%s);' % call
        elif rt == 'f64':
            body = 'return w_bits_d(%s);' % call
        else:
            body = 'return (u64)%s;' % call
        th.append('static u64 w_gc_%s(void *fn, const u32 *w) { %s }' % (sig, body))
    th.append('const w_gcentry w_gcalls[] = {')
    for sig in sorted(dsigs):
        th.append('  { "%s", w_gc_%s },' % (sig, sig))
    th.append('  { 0, 0 } };')
    open(os.path.join(outdir, 'w2c_thunks.c'), 'w').write('\n'.join(th) + '\n')
    hd = ['/* generated by w2c.py -- do not edit */']
    for sig in sorted(allsig):
        ps, rt = sig_types(sig)
        hd.append('%s w_icall_%s(u32 a%s);' % (rt, sig, ''.join(', ' + t for t in ps)))
        hd.append('%s w_icallx_%s(u32 a, u32 cc%s);' % (rt, sig, ''.join(', ' + t for t in ps)))
    for a, b in sorted(adapters):
        ps, rt = sig_types(a)
        hd.append('%s w_adapt_%s_%s(void *fn%s);' % (rt, a, b, ''.join(', ' + t for t in ps)))
    # x86-frame adapters (see x86_frame)
    CT_ = {'i32': 'u32', 'i64': 'u64', 'f32': 'f32', 'f64': 'f64'}
    rtm = {'i': 'i32', 'I': 'i64', 'f': 'f32', 'F': 'f64'}
    xa = []
    for (csig, fa, dsig, fb), aid in sorted(L.x86_adapters.items(), key=lambda x: x[1]):
        cps = [rtm[c] for c in csig.split('_')[0]]
        crt = csig.split('_')[1]
        dps = [rtm[c] for c in dsig.split('_')[0]]
        drt = dsig.split('_')[1]
        cret = CT_[rtm[crt]] if crt else 'void'
        dret = CT_[rtm[drt]] if drt else 'void'
        args = ''.join(', %s x%d' % (CT_[t], i) for i, t in enumerate(cps))
        hd.append('%s w_x86_%d(void *fn%s);' % (cret, aid, ''.join(', ' + CT_[t] for t in cps)))
        xa.append('%s w_x86_%d(void *fn%s) {' % (cret, aid, args))
        xa.append('  u32 ecx = 0, edx = 0, s[64] = {0};')
        for i, (t, (where, w)) in enumerate(zip(cps, fa)):
            if where in ('ecx', 'edx'):
                xa.append('  %s = (u32)x%d;' % (where, i))
            else:
                xa.append('  w_flat_%s(s, %d, x%d);' % (CT_[t], w, i))
        cargs = []
        for t, (where, w) in zip(dps, fb):
            if where in ('ecx', 'edx'):
                cargs.append(where if t == 'i32' else '(%s)%s' % (CT_[t], where))
            else:
                cargs.append('w_unflat_%s(s, %d)' % (CT_[t], w))
        call = '((%s (*)(%s))fn)(%s)' % (dret, ', '.join(CT_[t] for t in dps) or 'void',
                                       ', '.join(cargs))
        if cret == 'void':
            xa.append('  %s;' % call)
        elif dret == 'void':
            xa.append('  %s; return 0;' % call)
        else:
            xa.append('  return w_conv_%s_%s(%s);' % (dret, cret, call))
        xa.append('}')
    open(os.path.join(outdir, 'w2c_x86.c'), 'w').write(
        '/* generated by w2c.py: x86-convention call bridges -- do not edit */\n'
        '#include "w2c_rt.h"\n' + '\n'.join(xa) + '\n')
    open(os.path.join(outdir, 'w2c_icall.h'), 'w').write('\n'.join(hd) + '\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--symmap', required=True)
    ap.add_argument('--owners', required=True)
    ap.add_argument('objs', nargs='+')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    od = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(a.symmap))),
                      'match', 'orig')
    for fn in os.listdir(od) if os.path.isdir(od) else []:
        m = re.match(r'0x([0-9A-Fa-f]{8})\.bin$', fn)
        if m:
            FUNC_STARTS.add(int(m.group(1), 16))
    load_thunks(os.path.join(os.path.dirname(od), '..', '..', 'orig', 'BRGlide.dll'))
    def norm(n):
        # MSVC suffixes file statics with a uniquifier (s_args$S292); clang
        # does not
        return re.sub(r'\$S\d+$', '', n)
    symmap = []
    for r in csv.DictReader(open(a.symmap)):
        symmap.append((r['scope'], norm(r['name']), int(r['va'], 16)))
    owners = {}
    for r in csv.DictReader(open(a.owners)):
        owners[r['name']] = r['base']
    objs, srcof, srcpath = [], {}, {}
    for i, p in enumerate(sorted(a.objs)):
        o = Obj(p, i)
        objs.append(o)
        # object file name encodes the source path: src_core_dir_file.c.o
        srcof[i] = re.sub(r'\.(c|cpp)\.o$', '', os.path.basename(p)).split('__')[-1]
        srcpath[i] = os.path.basename(p)[:-2].replace('__', '/')
    # The verified T3 build (t3manifest.py) first; symmap.py's per-site
    # answers only for what that build does not cover.
    sites, placement = [], []
    wd = os.path.dirname(a.symmap)
    sp = os.path.join(wd, 'symsites.csv')
    if os.path.exists(sp):
        for r in csv.DictReader(open(sp)):
            sites.append(('base:' + r['scope'], norm(r['name']), int(r['addend']), int(r['va'], 16)))
    nbad = 0
    for r in csv.DictReader(open(os.path.join(wd, 'sites.csv'))):
        va = int(r['va'], 16)
        # a slot read past a T3 body's placed extent reads the original's
        # bytes there: keep only answers that are a real target
        if not ((TEXT_LO <= va < TEXT_HI and (va in FUNC_STARTS or va in THUNKS)) or
                DATA_LO <= va < 0x118F2000):
            nbad += 1
            continue
        sites.append((r['src'], norm(r['name']), int(r['addend']), va))
    if nbad:
        print('w2c: %d verified sites dropped (no real target)' % nbad)
    for r in csv.DictReader(open(os.path.join(wd, 'placement.csv'))):
        placement.append((int(r['va'], 16), r['name'], r['src']))
    # annotated declarations: per-file, below the verified sites
    dp = os.path.join(wd, 'decls.csv')
    if os.path.exists(dp):
        for r in csv.DictReader(open(dp)):
            sites.append(('decl:' + r['src'], r['name'], 0, int(r['va'], 16)))
    L = Linker(objs, symmap, owners, srcof, sites, placement)
    L.srcpath = srcpath
    L.run()
    L.port_set = set(L.port_segs)
    L.x86_adapters = {}
    outs = [translate(L, o, a.out) for o in objs]
    write_link(L, outs, a.out)
    with open(os.path.join(a.out, 'w2c_report.txt'), 'w') as f:
        for w in L.warn:
            f.write(w + '\n')
        f.write('outside the verified build (T1/T2 source): %d\n' % len(L.unverified))
        for va, base, pn in sorted(L.unverified):
            f.write('  unverified 0x%08X %s %s\n' % (va, base, pn))
        placed_have = set(v for v in L.fva.values() if v in L.placed_va)
        f.write('verified placements with no port function: %d\n'
                % (len(L.placed_va) - len(placed_have)))
        for va in sorted(set(L.placed_va) - placed_have):
            f.write('  unplaced 0x%08X %s\n' % (va, L.placed_va[va]))
        f.write('host imports: %d\n' % len(L.host))
        for cn, (sig, nm, va) in sorted(L.host.items()):
            f.write('  host %s %s\n' % (nm, sig))
        f.write('missing game functions: %d\n' % len(L.missing))
        for cn, (sig, nm, va) in sorted(L.missing.items()):
            f.write('  missing 0x%08X %s %s\n' % (va, nm, sig))
        f.write('orphans: %d\n' % len(L.orphans))
        for nm in sorted(L.orphans):
            f.write('  orphan %s\n' % nm)
    nf = sum(len(o.bodies) for o in objs)
    nva = sum(1 for v in L.fva.values() if v is not None and v < 0xF0000000)
    print('w2c: %d objects, %d functions (%d at original VAs), %d host imports, '
          '%d missing game functions, %d orphan data symbols, port data %#x' % (
              len(objs), nf, nva, len(L.host), len(L.missing), len(L.orphans),
              L.port_end))


if __name__ == '__main__':
    main()
