#!/usr/bin/env python3
"""Record specs: one typed definition per game struct, keyed by the
original 32-bit offsets.

A spec (ports/brally/types/<Record>.spec) lists a struct's fields as the
original laid them out:

    # size 0x2B68
    0x030  BrVec3       pos
    0xE8C  uint8_t *    pEquip        the car's equipment record
    0xF08  fnptr:void(struct BrDriverCar *)  pfnControl

Everything between listed fields is padding. `emit` turns a spec into the C
definition the 64-bit core compiles (fields in order, byte padding between
them, so the compiler lays pointers out at 64 bits). `rewrite.py` uses the
same offsets to turn raw accesses into field names.

Usage:
  recspec.py from-header RECORD FILE...    seed a spec from a declaration
  recspec.py absorb RECORD                 add fields for every raw access
                                           decisions.csv maps to RECORD
  recspec.py emit RECORD                   write the definition into the
                                           header named by the spec
  recspec.py show RECORD
  recspec.py alias VIEW RECORD FILE        make VIEW (declared in FILE) an alias of RECORD
"""
import collections
import csv
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
TYPES = os.path.join(ROOT, 'ports', 'common', 'types')
SDK = subprocess.run(['xcrun', '--show-sdk-path'], capture_output=True, text=True).stdout.strip()
I686 = ['--target=i686-pc-windows-msvc', '-isystem', SDK + '/usr/include', '-D__DARWIN_ONLY_UNIX_CONFORMANCE=1']
FLAGS = ['-fsyntax-only', '-D_FORTIFY_SOURCE=0', '-w', '-fms-extensions', '-fdeclspec', '-Wno-return-mismatch', '-Wno-error=incompatible-pointer-types',
         '-Wno-error=incompatible-function-pointer-types',
         '-Iports/brally/platform/include', '-Iports/brally/include',
         '-include', 'ports/brally/platform/include/win32.h',
         '-include', 'ports/brally/platform/include/glide.h',
         '-include', 'ports/brally/platform/include/br_lp64.h',
         '-include', 'ports/brally/include/br_globals.h',
         '-include', 'ports/brally/include/br_funcs.h']
# sizes of scalar and known types in the ORIGINAL (i686) layout
SIZE32 = {'char': 1, 'unsigned char': 1, 'signed char': 1, 'uint8_t': 1, 'int8_t': 1, 'BYTE': 1,
          'short': 2, 'unsigned short': 2, 'uint16_t': 2, 'int16_t': 2, 'WORD': 2,
          'int': 4, 'unsigned int': 4, 'unsigned': 4, 'long': 4, 'unsigned long': 4,
          'int32_t': 4, 'uint32_t': 4, 'DWORD': 4, 'float': 4, 'BOOL': 4,
          'double': 8, 'int64_t': 8, 'uint64_t': 8, '__int64': 8}


class Field:
    def __init__(self, off, ty, name, note=''):
        self.off, self.ty, self.name, self.note = off, ty, name, note

    def is_ptr(self):
        return self.ty.startswith('fnptr:') or self.ty.rstrip().endswith('*')


def spec_path(rec):
    return os.path.join(TYPES, rec + '.spec')


def load(rec):
    meta, fields = {}, []
    for ln in open(spec_path(rec)):
        ln = ln.rstrip('\n')
        m = re.match(r'#\s*(\w+)\s+(.*)$', ln)
        if m:
            meta[m.group(1)] = m.group(2).strip()
            continue
        if not ln.strip() or ln.lstrip().startswith('#'):
            continue
        parts = re.split(r'\s{2,}|\t', ln.strip(), maxsplit=3)
        off = int(parts[0], 16)
        fields.append(Field(off, parts[1], parts[2], parts[3] if len(parts) > 3 else ''))
    fields.sort(key=lambda f: f.off)
    return meta, fields


def save(rec, meta, fields):
    os.makedirs(TYPES, exist_ok=True)
    with open(spec_path(rec), 'w') as fh:
        for k, v in meta.items():
            fh.write('# %s %s\n' % (k, v))
        for f in sorted(fields, key=lambda f: f.off):
            fh.write('0x%04X  %-28s  %-20s%s\n' % (f.off, f.ty, f.name, ('  ' + f.note) if f.note else ''))


_size_cache = {}


def size32(ty, hdrs):
    """Size of a type in the original layout."""
    t = ty.strip()
    if t.startswith('fnptr:') or t.endswith('*'):
        return 4
    if re.match(r'.*\*\s*\[', t):
        t = re.sub(r'^.*?\*\s*\[', 'void *[', t)
    m = re.match(r'(.*?)\s*\[(\w+)\]$', t)
    if m and re.match(r'(0x[0-9a-fA-F]+|\d+)$', m.group(2)):
        return size32(m.group(1), hdrs) * int(m.group(2), 0)
    base = re.sub(r'\b(const|volatile|struct|union)\b', '', t).strip()
    if base in SIZE32:
        return SIZE32[base]
    sp = os.path.join(TYPES, base + '.spec')
    if re.match(r'^\w+$', base) and os.path.exists(sp):
        for l in open(sp):
            m = re.match(r'#\s*size\s+(0x[0-9a-fA-F]+)', l)
            if m:
                return int(m.group(1), 16)
    key = (t, tuple(hdrs))
    if key not in _size_cache:
        src = ''.join('#include "%s"\n' % h for h in hdrs) + 'char br_sz[sizeof(%s)];\n' % t
        p = subprocess.run(['clang'] + I686 + FLAGS + ['-x', 'c', '-std=gnu89',
                           '-Xclang', '-ast-dump=json', '-'], input=src.encode(), capture_output=True, cwd=ROOT)
        j = json.loads(p.stdout.decode() or '{}')
        sz = None
        for n in j.get('inner', []):
            if n.get('name') == 'br_sz':
                ty = n['type']
                m2 = re.search(r'\[(\d+)\]', ty.get('desugaredQualType') or ty['qualType']) or \
                    re.search(r'\[(\d+)\]', ty['qualType'])
                if m2:
                    sz = int(m2.group(1))
                else:
                    raise SystemExit('recspec: sizeof(%s) gave %r' % (t, ty))
        if sz is None:
            raise SystemExit('recspec: cannot size %r (headers %s)' % (t, hdrs))
        _size_cache[key] = sz
    return _size_cache[key]


def decl(f):
    """C declaration text for a field."""
    t = f.ty
    if t.startswith('fnptr:'):
        m = re.match(r'fnptr:(.*?)\((.*)\)$', t)
        return '%s (*%s)(%s)' % (m.group(1).strip(), f.name, m.group(2))
    m = re.match(r'(.*?)\s*((?:\[\w+\])+)$', t)
    if m:
        return '%s %s%s' % (m.group(1), f.name, m.group(2))
    return '%s %s' % (t, f.name) if not t.endswith('*') else '%s%s' % (t, f.name)


def emit_text(rec, meta, fields):
    hdrs = meta.get('headers', '').split()
    size = int(meta['size'], 16)
    out = []
    tag = meta.get('tag', rec)
    out.append('typedef struct %s {' % tag)
    cur = 0
    groups = []
    for f in fields:
        if groups and groups[-1][0].off == f.off:
            groups[-1].append(f)
        else:
            groups.append([f])
    for g in groups:
        f = g[0]
        if f.off < cur:
            raise SystemExit('recspec: %s: field %s at 0x%X overlaps the one before (ends 0x%X)' % (
                rec, f.name, f.off, cur))
        if f.off > cur:
            out.append('    uint8_t _pad%04X[0x%X];' % (cur, f.off - cur))
        if len(g) == 1:
            d = decl(f) + ';'
            note = ('  ' + f.note) if f.note else ''
            out.append('    %-40s /* +0x%04X%s */' % (d, f.off, note))
        else:
            out.append('    union {                                  /* +0x%04X */' % f.off)
            for u in g:
                note = ('  /* %s */' % u.note) if u.note else ''
                out.append('        %s;%s' % (decl(u), note))
            out.append('    };')
        cur = f.off + max(size32(u.ty, hdrs) for u in g)
    if cur < size:
        out.append('    uint8_t _pad%04X[0x%X];' % (cur, size - cur))
    elif cur > size:
        raise SystemExit('recspec: %s: fields run to 0x%X past the size 0x%X' % (rec, cur, size))
    out.append('} %s;' % rec)
    return '\n'.join(out)


def find_decl(rec, path):
    """(start, end) character offsets of `typedef struct ... } REC;` in path."""
    s = open(os.path.join(ROOT, path), encoding='latin-1').read()
    m = re.search(r'typedef\s+struct\s+(\w+)?\s*\{', s)
    for m in re.finditer(r'typedef\s+struct\s*(\w*)\s*\{', s):
        depth, i = 0, m.end() - 1
        while i < len(s):
            if s[i] == '{':
                depth += 1
            elif s[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        t = re.match(r'\}\s*(\w+)\s*;', s[i:])
        if t and t.group(1) == rec:
            return s, m.start(), i + t.end()
    return s, None, None


def layout32(rec, path, hdrs):
    """Top-level fields of REC in the original layout, via clang."""
    s0, a0, b0 = find_decl(rec, path)
    if a0 is None:
        raise SystemExit('recspec: no typedef of %s in %s' % (rec, path))
    tm = re.match(r'typedef\s+struct\s*(\w*)', s0[a0:b0])
    tag = tm.group(1) or None
    if path.endswith(('.c', '.cpp')):
        lang = ['-x', 'c++', '-std=c++98'] if path.endswith('.cpp') else ['-x', 'c', '-std=gnu89']
        src = s0 + '\n%s *br_force_%s;\nint br_use_%s(void) { return sizeof(*br_force_%s); }\n' % (rec, rec, rec, rec)
    else:
        lang = ['-x', 'c', '-std=gnu89']
        src = ''.join('#include "%s"\n' % h for h in hdrs) + '#include "%s"\n' % os.path.basename(path)
        src += '%s *br_force;\nint br_use(void) { return sizeof(*br_force); }\n' % rec
    p = subprocess.run(['clang'] + I686 + FLAGS + ['-I' + os.path.dirname(os.path.join(ROOT, path))] + lang +
                       ['-Xclang', '-fdump-record-layouts-complete', '-'], input=src.encode('latin-1'),
                       capture_output=True, cwd=ROOT)
    txt = p.stdout.decode('utf-8', 'replace')
    for blk in txt.split('*** Dumping AST Record Layout')[1:]:
        lines = blk.strip('\n').split('\n')
        m = re.match(r'\s*\d+ \| (?:struct|class) (.+)$', lines[0])
        if not m or m.group(1) not in (rec, tag) and not (tag is None and '(unnamed' in m.group(1)):
            continue
        sz = int(re.search(r'\[sizeof=(\d+)', blk).group(1))
        fields = []
        for ln in lines[1:]:
            fm = re.match(r'\s*(\d+) \|   (\S.*)$', ln)
            if fm:
                ty, name = fm.group(2).rsplit(' ', 1)
                fields.append((int(fm.group(1)), ty, name))
        return sz, fields
    raise SystemExit('recspec: no layout for %s: %s' % (rec, p.stderr.decode()[:2000]))


PAD = re.compile(r'^_?(pad|rest|gap|unk|res)', re.I)


def cmd_from_header(rec, paths):
    path = paths[0]
    hdrs = paths[1:]
    sz, fl = layout32(rec, path, hdrs)
    fields = []
    for off, ty, name in fl:
        if PAD.match(name) and re.match(r'(unsigned )?char\[|uint8_t\[|char\[', ty):
            continue
        ty = re.sub(r'^(.*?)\[(\d+)\]$', r'\1[\2]', ty)
        m = re.match(r'(.*)\(\*\)\((.*)\)$', ty)
        if m:
            ty = 'fnptr:%s(%s)' % (m.group(1).strip(), m.group(2))
        fields.append(Field(off, ty, name))
    own = [os.path.basename(path)] if path.endswith('.h') else []
    meta = collections.OrderedDict([('size', '0x%X' % sz), ('header', path),
                                    ('headers', ' '.join(own + list(hdrs)))])
    save(rec, meta, fields)
    print('%s: %d fields, size 0x%X -> %s' % (rec, len(fields), sz, spec_path(rec)))


def cmd_emit(rec):
    meta, fields = load(rec)
    text = emit_text(rec, meta, fields)
    path = meta['header']
    s, a, b = find_decl(rec, path)
    if a is None:
        raise SystemExit('recspec: %s has no typedef in %s' % (rec, path))
    s = s[:a] + text + s[b:]
    open(os.path.join(ROOT, path), 'w', encoding='latin-1').write(s)
    print('%s: emitted into %s' % (rec, path))


def cmd_show(rec):
    meta, fields = load(rec)
    print(emit_text(rec, meta, fields))


def main():
    os.chdir(ROOT)
    cmd, rec = sys.argv[1], sys.argv[2]
    if cmd == 'from-header':
        cmd_from_header(rec, sys.argv[3:])
    elif cmd == 'emit':
        cmd_emit(rec)
    elif cmd == 'show':
        cmd_show(rec)
    elif cmd == 'alias':
        alias_view(rec, sys.argv[3], sys.argv[4])
    else:
        raise SystemExit(__doc__)



def alias_view(view, rec, path):
    """Replace VIEW's definition in path with an alias of REC, and point every
    `struct VIEW` in the core at `struct REC`."""
    import glob
    s, a, b = find_decl(view, path)
    if a is None:
        raise SystemExit('recspec: no typedef of %s in %s' % (view, path))
    s = s[:a] + 'typedef struct %s %s;   /* the same object as %s */' % (rec, view, rec) + s[b:]
    open(os.path.join(ROOT, path), 'w', encoding='latin-1').write(s)
    n = 0
    for p in glob.glob(os.path.join(ROOT, 'ports/brally/src/**/*'), recursive=True) + \
            glob.glob(os.path.join(ROOT, 'ports/brally/include/*.h')):
        if not p.endswith(('.c', '.cpp', '.h')):
            continue
        t = open(p, encoding='latin-1').read()
        t2, k = re.subn(r'\bstruct\s+%s\b' % re.escape(view), 'struct %s' % rec, t)
        if k:
            open(p, 'w', encoding='latin-1').write(t2)
            n += k
    print('%s: now an alias of %s (%d tag uses repointed)' % (view, rec, n))


if __name__ == '__main__':
    main()
