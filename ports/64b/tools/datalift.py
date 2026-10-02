#!/usr/bin/env python3
"""Build-time data lift: the original's initialised data, from the user's DLL.

The decompilation's matching build takes .rdata and initialised .data from
the original binary (tools/image_build.py); the source holds code, not those
bytes. The 64-bit core gets the same bytes the same way: at build time, from
the user's own BRGlide.dll, into a generated file under build/ that is never
committed.

For every global src/core/data/br_globals.c defines at an address in the
initialised range (0x10077000..0x100BCE00) this emits, into
build/portable/gen/br_data.c, the statements that give it the original's
value inside `void br_data_lift(void)`:

  - a pointer-free object whose layout is the same at both widths (and the
    extent bytes after any object) is copied from the image;
  - a pointer field is rebuilt from the DLL's base relocations: the address
    the original stored becomes the address of the core symbol at that
    original address (a function, a global, or a field inside one);
  - anything else is laid field by field through the i386 and native record
    layouts.

The lifted vtables g_brVtbl_<VA> (br_vtables.h) are defined here too.
Anything that cannot be expressed is listed in build/portable/gen/
datalift.txt: an address no core symbol covers, a pointer the original
stored without a relocation.

Usage: datalift.py [--dll orig/BRGlide.dll]
"""
import bisect
import json
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe32  # noqa: E402
import viewmerge as vm  # noqa: E402
import globext  # noqa: E402

ROOT = vm.ROOT
LO, HI = 0x10077000, 0x100BCE00
GLOBALS_C = 'ports/64b/src/core/data/br_globals.c'
OUT = 'build/portable/gen'

SCALAR = {'char': 1, 'signed char': 1, 'unsigned char': 1, '_Bool': 1, 'bool': 1,
          'short': 2, 'unsigned short': 2, 'int': 4, 'unsigned int': 4,
          'long': 4, 'unsigned long': 4, 'long long': 8, 'unsigned long long': 8,
          'float': 4, 'double': 8}


# ----------------------------------------------------------------- layouts
def layouts(target):
    """{record: (size, [(off, type, name, is_bitfield)])} -- direct fields."""
    args = ['clang', '-target', target, '-I.'] + vm.FLAGS + ['-x', 'c', '-std=gnu89', '-fsyntax-only',
            '-Xclang', '-fdump-record-layouts-complete', GLOBALS_C]
    p = subprocess.run(args, capture_output=True, text=True, cwd=ROOT, errors='replace')
    recs, cur, stack = {}, None, None
    for line in p.stdout.split('\n'):
        m = re.match(r'^\s*(\d+)(:\d+-\d+)? \| (\s*)(.*)$', line)
        if m:
            off, bit, depth, rest = int(m.group(1)), m.group(2), len(m.group(3)), m.group(4)
            if depth == 0 and cur is None:
                rm = re.match(r'^(?:struct|union) (\S.*)$', rest)
                cur = rm.group(1) if rm else rest
                cur = re.sub(r'^\((?:unnamed )?(?:struct |union )?(?:unnamed )?at ', '(unnamed at ', cur)
                recs[cur] = [None, [], rest.startswith('union')]
                continue
            if cur is not None and depth == 2:
                tm = re.match(r'^(.*?)\s*(\w+)$', rest)
                if tm:
                    recs[cur][1].append((off, tm.group(1).strip(), tm.group(2), bool(bit)))
            continue
        sm = re.match(r'^\s*\| \[sizeof=(\d+)', line)
        if sm and cur is not None:
            recs[cur][0] = int(sm.group(1))
            cur = None
    return recs


class Types:
    def __init__(self):
        self.r32 = layouts('i386-apple-macos10.13')
        self.r64 = layouts('arm64-apple-macos11')
        self.typedefs = {}

    def norm(self, t):
        t = re.sub(r'\b(const|volatile|struct|union|enum)\b', ' ', t)
        t = re.sub(r'\s+', ' ', t).strip()
        t = re.sub(r'^\((?:unnamed )?(?:struct |union )?(?:unnamed )?at ', '(unnamed at ', t)
        for _ in range(8):
            if t in self.typedefs:
                t = self.typedefs[t]
            else:
                break
        return t

    def load_typedefs(self, tree):
        """typedef name -> the type it names, from the AST. An anonymous
        record is keyed the way the layout dump names it, by location."""
        def strip(t):
            t = re.sub(r'\b(const|volatile|struct|union|enum)\b', ' ', t)
            t = re.sub(r'\s+', ' ', t).strip()
            return re.sub(r'^\((?:unnamed )?(?:struct |union )?(?:unnamed )?at ', '(unnamed at ', t)
        anon, cur = {}, {'file': None, 'line': None}
        for d in tree.get('inner', []):
            loc = d.get('loc') or {}
            loc = loc.get('expansionLoc', loc)
            if 'file' in loc:
                cur['file'] = loc['file']
            if 'line' in loc:
                cur['line'] = loc['line']
            if d.get('kind') == 'RecordDecl' and not d.get('name') and 'col' in loc:
                anon[d['id']] = '(unnamed at %s:%s:%s)' % (cur['file'], cur['line'], loc['col'])
            if d.get('kind') != 'TypedefDecl' or not d.get('name'):
                continue
            owned = None
            for c in d.get('inner', []):
                if c.get('ownedTagDecl'):
                    owned = c['ownedTagDecl']['id']
            if owned in anon:
                self.typedefs[d['name']] = anon[owned]
                continue
            ty = strip(d['type'].get('desugaredQualType') or d['type']['qualType'])
            if ty.endswith('*') or '(*' in ty:
                self.typedefs[d['name']] = 'void *'
            elif not re.search(r'[\[(]', ty) and ty != d['name']:
                self.typedefs[d['name']] = ty

    def split_array(self, t):
        f = re.match(r'^(.*?)\(\*((?:\[\d+\])+)\)\s*(\(.*\))$', t.strip())
        if f:                     # an array of function pointers: R (*[N])(A)
            return '%s(*)%s' % (f.group(1), f.group(3)), [int(x) for x in re.findall(r'\[(\d+)\]', f.group(2))]
        m = re.match(r'^(.*?)\s*((?:\[\d+\])+)$', t.strip())
        if not m:
            return t.strip(), []
        return m.group(1).strip(), [int(x) for x in re.findall(r'\[(\d+)\]', m.group(2))]

    def is_ptr(self, t):
        b, dims = self.split_array(t)
        if b.endswith('*') or '(*' in b:
            return True
        n = self.norm(b)
        return n.endswith('*') or '(*' in n

    def rec(self, t):
        n = self.norm(t)
        return n if n in self.r32 else None

    def size(self, t, wide):
        b, dims = self.split_array(t)
        n = 1
        for d in dims:
            n *= d
        if self.is_ptr(b):
            e = 8 if wide else 4
        else:
            bn = self.norm(b)
            if bn in SCALAR:
                e = SCALAR[bn] if not (wide and bn in ('long', 'unsigned long')) else 8
            elif bn in (self.r64 if wide else self.r32):
                e = (self.r64 if wide else self.r32)[bn][0]
            else:
                return None
        return e * n

    def ptr_free(self, t, seen=()):
        b, dims = self.split_array(t)
        if self.is_ptr(b):
            return False
        r = self.rec(b)
        if r is None:
            return True
        if r in seen:
            return True
        return all(self.ptr_free(ft, seen + (r,)) for _, ft, _, _ in self.r32[r][1])

    def same_layout(self, t):
        s32, s64 = self.size(t, False), self.size(t, True)
        return s32 is not None and s32 == s64 and self.ptr_free(t)


# ----------------------------------------------------------------- symbols
def cast_to_ptr(qt):
    """a cast to pointer-to-qt from its spelling ('T [4]' -> '(T (*)[4])')"""
    qt = re.sub(r'\bconst\b', '', qt)
    qt = re.sub(r'\s+', ' ', qt).strip()
    m = re.match(r'^(.*?)\s*((?:\[\d+\])+)$', qt)
    base, dims = (m.group(1), m.group(2)) if m else (qt, '')
    f = re.match(r'^(.*?)\(\*((?:\[\d+\])+)\)\s*(\(.*\))$', qt)
    if f:                                  # array of function pointers
        return '(%s(*(*)%s)%s)' % (f.group(1), f.group(2), f.group(3))
    if '(*' in base:                       # function pointer: one more level
        return '(%s)' % base.replace('(*', '(**', 1) if not dims else None
    return '(%s (*)%s)' % (base, dims) if dims else '(%s *)' % base


class Syms:
    """original VA -> core symbol"""
    def __init__(self, types):
        self.types = types
        self.used = set()
        self.qual = {}
        self.g = []          # (va, size32, name, type)
        self.fn = {}
        defined = set()
        for o in os.listdir('build/portable/obj'):
            if o.endswith('.o'):
                out = subprocess.run(['nm', '-g', '-U', 'build/portable/obj/' + o], capture_output=True, text=True).stdout
                for line in out.splitlines():
                    p = line.split()
                    if len(p) == 3:
                        defined.add(p[2].lstrip('_'))
        self.defined = defined
        for line in open('build/wasm/placement.csv'):
            p = line.strip().split(',')
            if len(p) >= 2 and p[0].startswith('0x') and p[1] in defined:
                self.fn[int(p[0], 16)] = p[1]

    def add_global(self, va, size, name, ty, qt=None):
        self.g.append((va, size, name, ty))
        if qt:
            self.qual[name] = qt

    def sym(self, name):
        self.used.add(name)
        return 'br_sym_%s' % name

    def root(self, name):
        """the global as a typed lvalue, through its linker-name alias"""
        c = cast_to_ptr(self.qual.get(name, 'char'))
        if c is None:
            return None
        return '(*%s%s)' % (c, self.sym(name))

    def finish(self):
        self.g.sort()
        self.keys = [x[0] for x in self.g]

    def expr(self, va):
        """C expression for the core address of original address va, or None."""
        if va in self.fn:
            return '(void *)%s' % self.sym(self.fn[va])
        i = bisect.bisect_right(self.keys, va) - 1
        while i >= 0:
            gva, size, name, ty = self.g[i]
            if gva <= va < gva + max(size, 1):
                off = va - gva
                if off == 0:
                    return '(void *)%s' % self.sym(name)
                path = field_path(self.types, ty, off)
                if path is not None:
                    p, rem = path
                    if p == '':
                        return '(void *)(%s + %d)' % (self.sym(name), rem)
                    r = self.root(name)
                    if r is None:
                        return None
                    base = '(char *)&%s%s' % (r, p)
                    return '(void *)(%s + %d)' % (base, rem) if rem else '(void *)(%s)' % base
                return None
            if gva + size <= va and gva < va - 0x100000:
                break
            i -= 1
        return None


def field_path(types, ty, off):
    """(C member path, byte remainder) at i386 offset off of type ty, or None
    when the type has no field there whose 64-bit position is known."""
    if types.same_layout(ty):
        return '', off
    b, dims = types.split_array(ty)
    if dims:
        es = types.size(b + ''.join('[%d]' % d for d in dims[1:]), False)
        if not es:
            return None
        i, rem = divmod(off, es)
        if i >= dims[0]:
            return None
        sub = field_path(types, b + ''.join('[%d]' % d for d in dims[1:]), rem)
        return None if sub is None else ('[%d]%s' % (i, sub[0]), sub[1])
    r = types.rec(b)
    if r is None:
        return ('', off) if not types.is_ptr(b) else (('', off) if off == 0 else None)
    for (o, ft, fn, bit) in reversed(types.r32[r][1]):
        if o <= off:
            fs = types.size(ft, False) or 0
            if off < o + max(fs, 1):
                sub = field_path(types, ft, off - o)
                return None if sub is None else ('.%s%s' % (fn, sub[0]), sub[1])
            return None
    return None


# ----------------------------------------------------------------- emission
class Lift:
    def __init__(self, dll):
        self.pe = pe32.PE(dll)
        self.img = self.image(LO, HI)
        self.relocs = set(self.pe.relocs())
        self.types = Types()
        self.syms = Syms(self.types)
        self.out, self.notes = [], []
        self.pool_refs = []

    def image(self, lo, hi):
        """the bytes the loader maps at [lo, hi): section by section, since
        a section's file and memory layouts differ (zeros past its raw data)"""
        out = bytearray(hi - lo)
        for name, rva, vsz, raw, rsz in self.pe.secs:
            va = self.pe.base + rva
            a, b = max(lo, va), min(hi, va + max(vsz, rsz))
            if a >= b:
                continue
            n = max(0, min(b, va + rsz) - a)
            out[a - lo:a - lo + n] = self.pe.d[raw + (a - va):raw + (a - va) + n]
        return bytes(out)

    def read(self, va, n):
        return self.img[va - LO:va - LO + n] if LO <= va and va + n <= HI else self.image(va, va + n)

    def dword(self, va):
        return struct.unpack('<I', self.read(va, 4))[0]

    def ptr_stmt(self, lhs, va):
        """assignment of the pointer the original holds at va"""
        v = self.dword(va)
        if v == 0:
            return
        if va not in self.relocs:
            # not an address: the field is typed as a pointer but the
            # original keeps a number there; its four bytes, low half
            self.notes.append('0x%08X %s: 0x%08X stored without a relocation' % (va, lhs, v))
            self.out.append('    memcpy(&%s, k_img + 0x%X, 4);' % (lhs, va - LO))
            return
        e = self.syms.expr(v)
        if e is None and LO <= v < HI:
            self.pool_refs.append((va, lhs, v))
            return
        if e is None:
            self.notes.append('0x%08X %s -> 0x%08X: no core symbol there' % (va, lhs, v))
            return
        self.out.append('    *(void **)&%s = %s;' % (lhs, e))

    def bytes_stmt(self, lhs_addr, va, n):
        if n <= 0:
            return
        blob = self.read(va, n)
        if blob.count(0) == n:
            return
        self.out.append('    memcpy(%s, k_img + 0x%X, %d);' % (lhs_addr, va - LO, n))

    def walk(self, lhs, ty, va):
        t = self.types
        if t.same_layout(ty):
            self.bytes_stmt('&%s' % lhs if not t.split_array(ty)[1] else lhs, va, t.size(ty, False))
            return
        b, dims = t.split_array(ty)
        if dims:
            inner = b + ''.join('[%d]' % d for d in dims[1:])
            es = t.size(inner, False)
            if not es:
                self.notes.append('0x%08X %s: element type %s not laid out' % (va, lhs, inner))
                return
            for i in range(dims[0]):
                self.walk('%s[%d]' % (lhs, i), inner, va + i * es)
            return
        if t.is_ptr(b):
            self.ptr_stmt(lhs, va)
            return
        r = t.rec(b)
        if r is None:
            if t.norm(b) in SCALAR:
                self.bytes_stmt('&%s' % lhs, va, SCALAR[t.norm(b)])   # e.g. long: the low half
                return
            self.notes.append('0x%08X %s: type %s not laid out' % (va, lhs, ty))
            return
        size, fields, is_union = t.r32[r]
        if is_union:
            # the member the bytes are: a pointer where the original relocated
            ptrs = [f for f in fields if t.is_ptr(f[1])]
            if ptrs and va + ptrs[0][0] in self.relocs:
                self.walk('%s.%s' % (lhs, ptrs[0][2]), ptrs[0][1], va + ptrs[0][0])
            else:
                plain = [f for f in fields if t.ptr_free(f[1])]
                f = max(plain, key=lambda f: t.size(f[1], False) or 0) if plain else fields[0]
                self.walk('%s.%s' % (lhs, f[2]), f[1], va + f[0])
            return
        for (o, ft, fn, bit) in fields:
            if bit:
                continue        # bitfields live in pointer-free words copied whole below
            self.walk('%s.%s' % (lhs, fn), ft, va + o)
        bits = [f for f in fields if f[3]]
        if bits:
            self.notes.append('0x%08X %s: bitfields in a record with pointers not lifted' % (va, lhs))

    def run(self):
        os.chdir(ROOT)
        text = open(GLOBALS_C).read()
        tree = vm.ast(GLOBALS_C, [])
        self.types.load_typedefs(tree)
        decl_ty, decl_qt = {}, {}
        for d in tree.get('inner', []):
            if d.get('kind') == 'VarDecl' and d.get('name') and not d['name'].endswith('__extent'):
                ty = d['type'].get('desugaredQualType') or d['type']['qualType']
                # the definition's type (sized) wins over an extern's
                if d.get('storageClass') != 'extern' or d['name'] not in decl_ty:
                    decl_ty[d['name']] = ty
                    decl_qt[d['name']] = d['type']['qualType']
        for m in globext.PADDED.finditer(text):
            n, dims = m.group('name'), m.group('dims').strip()
            if n in decl_ty and '[]' in decl_ty[n] and dims:
                decl_ty[n] = decl_ty[n].replace('[]', dims.split(']')[0] + ']', 1)
                decl_qt[n] = decl_qt[n].replace('[]', dims.split(']')[0] + ']', 1)
        here = globext.defs(text)
        padded = [(int(m.group('va'), 16), m.group('name'), int(m.group('pad'), 16)) for m in globext.PADDED.finditer(text)]
        pad = {n: p for _, n, p in padded}
        names = [(va, n) for va, n, _ in here] + [(va, n) for va, n, _ in padded]
        for va, n in names:
            ty = decl_ty.get(n)
            if ty is None:
                continue
            s32 = self.types.size(ty, False)
            if s32 is None:
                continue
            self.syms.add_global(va, s32 + pad.get(n, 0), n, ty, decl_qt.get(n))
        self.syms.finish()
        lifted = 0
        for va, n in sorted(names):
            if not (LO <= va < HI):
                continue
            ty = decl_ty.get(n)
            if ty is None:
                self.notes.append('0x%08X %s: no declaration' % (va, n))
                continue
            s32 = self.types.size(ty, False)
            if s32 is None:
                self.notes.append('0x%08X %s: size of %s unknown' % (va, n, ty))
                continue
            self.out.append('    /* 0x%08X %s */' % (va, n))
            root = self.syms.root(n)
            if root is None:
                self.notes.append('0x%08X %s: type %s cannot be spelled' % (va, n, ty))
                continue
            self.walk(root, ty, va)
            if pad.get(n):
                s64 = self.types.size(ty, True)
                self.bytes_stmt('%s + %d' % (self.syms.sym(n), s64), va + s32, pad[n])
            lifted += 1
        self.pools()
        self.vtables()
        self.initterm()
        self.write(lifted)

    def pools(self):
        """Addresses the original stored that no core global covers (strings
        and records an over-long table swallowed, data no code names): each
        maximal run of them becomes a writable copy of the image here,
        reached only through the pointers being lifted."""
        starts = sorted(set(g[0] for g in self.syms.g))
        spans = []
        for va, lhs, v in self.pool_refs:
            i = bisect.bisect_right(starts, v)
            nxt = starts[i] if i < len(starts) else HI
            end = min(nxt, v + 0x1000, HI)
            raw = self.read(v, end - v)
            z = raw.find(0)
            if 0 <= z < len(raw) and nxt - v > 0x1000:
                end = v + z + 1           # a string: up to its terminator
            spans.append([v, max(end, v + 1)])
        spans.sort()
        merged = []
        for a, b in spans:
            if merged and a <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a, b])
        self.pool = merged
        keys = [m[0] for m in merged]
        for va, lhs, v in self.pool_refs:
            a, b = merged[bisect.bisect_right(keys, v) - 1]
            self.out.append('    *(void **)&%s = (void *)(k_pool_%08X + %d);' % (lhs, a, v - a))

    def initterm(self):
        """the C++ static initialisers: the CRT's _initterm table, from the
        first relocated slot of .data (0x1007B000) while slots relocate"""
        self.init = []
        va = 0x1007B004
        while va in self.relocs:
            tgt = self.dword(va)
            if tgt in self.syms.fn:
                self.init.append(self.syms.sym(self.syms.fn[tgt]))
            else:
                self.notes.append('initterm 0x%08X -> 0x%08X: no core function' % (va, tgt))
            va += 4

    def vtables(self):
        """g_brVtbl_<VA>: the function pointers from VA up to the first
        non-relocated slot"""
        names = set()
        for f in ['ports/64b/include/br_vtables.h']:
            names |= set(re.findall(r'\bg_brVtbl_([0-9A-Fa-f]{8})\b', open(f).read()))
        self.vt = []
        for h in sorted(names):
            va = int(h, 16)
            ents = []
            while va + 4 * len(ents) in self.relocs:
                tgt = self.dword(va + 4 * len(ents))
                ents.append(self.syms.sym(self.syms.fn[tgt]) if tgt in self.syms.fn else None)
                if ents[-1] is None:
                    self.notes.append('vtable 0x%s slot %d -> 0x%08X: no core function' % (h, len(ents) - 1, tgt))
            self.vt.append((h, ents))

    def write(self, lifted):
        os.makedirs(OUT, exist_ok=True)
        blob = self.img
        with open(OUT + '/br_data.c', 'w') as f:
            f.write('/* GENERATED by ports/64b/tools/datalift.py from the user\'s BRGlide.dll.\n'
                    ' * Not source: never commit. */\n#include <string.h>\n#include "br_vtables.h"\n')
            for inc in re.findall(r'^#include "[^"]+"', open(GLOBALS_C).read(), re.M):
                f.write(inc + '\n')
            f.write('\n#define BR_SYM_STR2(x) #x\n#define BR_SYM_STR(x) BR_SYM_STR2(x)\n'
                    '#define BR_SYM(n) __asm__(BR_SYM_STR(__USER_LABEL_PREFIX__) n)\n')
            for n in sorted(self.syms.used):
                f.write('extern char br_sym_%s[] BR_SYM("%s");\n' % (n, n))
            f.write('\n')
            f.write('static const unsigned char k_img[0x%X] = {\n' % len(blob))
            for i in range(0, len(blob), 24):
                f.write('    ' + ','.join(str(b) for b in blob[i:i + 24]) + ',\n')
            f.write('};\n\n')
            for a, b in self.pool:
                f.write('static unsigned char k_pool_%08X[%d];   /* 0x%08X..0x%08X, from the image */\n' % (a, b - a, a, b))
            f.write('\n')
            for h, ents in self.vt:
                f.write('void *const g_brVtbl_%s[%d] = {\n' % (h, max(len(ents), 1)))
                for e in ents:
                    f.write('    (void *)%s,\n' % (e if e else '0'))
                f.write('};\n\n')
            f.write('void br_data_initterm(void)\n{\n')
            for fn in self.init:
                f.write('    ((void (*)(void))%s)();\n' % fn)
            f.write('}\n\n')
            f.write('void br_data_lift(void)\n{\n')
            for a, b in self.pool:
                f.write('    memcpy(k_pool_%08X, k_img + 0x%X, %d);\n' % (a, a - LO, b - a))
            f.write('\n'.join(self.out))
            f.write('\n}\n')
        open(OUT + '/datalift.txt', 'w').write('\n'.join(self.notes) + '\n')
        print('lifted %d globals, %d statements, %d vtables; %d notes (%s/datalift.txt)'
              % (lifted, len(self.out), len(self.vt), len(self.notes), OUT))


if __name__ == '__main__':
    dll = 'orig/BRGlide.dll'
    if '--dll' in sys.argv:
        dll = sys.argv[sys.argv.index('--dll') + 1]
    Lift(dll).run()
