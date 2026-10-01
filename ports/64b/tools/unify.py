#!/usr/bin/env python3
"""One definition per original global; every other name an alias of it.

Input: build/portable/globals.csv (globals.py). For each original address
the decompiled files reach under several names or types, this chooses the
canonical object -- the largest declaration, preferring a pointer over an
integer, a struct or array over a scalar, a descriptive name over DAT_ -- and
makes every other name a macro for it:

  * same type                         #define A C
  * the canonical is (or holds) a pointer where A said int:
                                      #define A C    (the compiler then names
                                                      every int use; fixint.py)
  * other scalar disagreements        #define A (*(T *)&C)   (the original bits)
  * A lies inside C                   #define A (C.field) / (C[i]) / a byte view

Canonical declarations of scalar/pointer/array-of-scalar type go to
ports/64b/include/br_globals.h (force-included); struct-typed ones are
appended to the header that defines the struct. Definitions go to
ports/64b/src/core/data/br_globals.c (zero-initialised here; the
original's initial values come from the data lift). Every per-file
declaration of a unified name is removed.

Usage: unify.py [--dry] [--report]
"""
import bisect
import collections
import csv
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recspec  # noqa: E402
import rewrite  # noqa: E402

ROOT = recspec.ROOT
DRY = '--dry' in sys.argv
INC = os.path.join(ROOT, 'ports/64b/include')
GH = os.path.join(INC, 'br_globals.h')
GC = os.path.join(ROOT, 'ports/64b/src/core/data/br_globals.c')
SCALARS = {'int', 'unsigned', 'char', 'short', 'long', 'float', 'double', 'void', 'signed', 'uint8_t', 'int8_t',
           'uint16_t', 'int16_t', 'uint32_t', 'int32_t', 'uint64_t', 'int64_t', 'size_t', 'BYTE', 'WORD', 'DWORD',
           'BOOL', 'HANDLE', 'HWND', 'UINT', 'LONG', 'intptr_t', 'uintptr_t', 'const', 'volatile', 'struct',
           '__int64', 'FARPROC', 'HINSTANCE', 'HMODULE', 'HGLOBAL', 'LPVOID', 'funcptr', 'undefined',
           'undefined4', 'CHAR', 'SHORT', 'USHORT', 'ULONG', 'FLOAT', 'HKEY', 'HMMIO', 'LPSTR', 'LPCSTR'}
PTRT = ('HANDLE', 'HWND', 'LPVOID', 'FARPROC', 'funcptr', 'HINSTANCE', 'HMODULE', 'HGLOBAL', 'LPSTR', 'LPCSTR',
        'HKEY', 'HMMIO')


VIEWS = {'BrSnapCar': 'BrDriverCar', 'BrAiCar': 'BrDriverCar', 'BrCar': 'BrDriverCar',
         'BrRbBodyFull': 'BrRbBody'}


def norm(t):
    t = re.sub(r'\b(volatile|extern)\b', '', t)
    for v, c in VIEWS.items():
        t = re.sub(r'\b%s\b' % v, c, t)
    return re.sub(r'\s+', ' ', t).strip()


def kind(t):
    t = re.sub(r'\b(const|volatile|struct)\b', '', t).strip()
    if '(*' in t or t.endswith('*') or re.search(r'\*\s*\[', t) or t in PTRT:
        return 'ptr'
    if '[' in t:
        return 'array'
    words = set(re.findall(r'[A-Za-z_]\w*', t)) - SCALARS
    return 'struct' if words else 'scalar'


def holds_ptr(t):
    return kind(t) == 'ptr' or re.search(r'\*', t)


def type_words(t):
    return set(re.findall(r'[A-Za-z_]\w*', t)) - SCALARS


def header_of_types():
    out = {}
    for p in sorted(glob.glob(os.path.join(INC, '*.h'))):
        s = open(p, encoding='latin-1').read()
        for m in re.finditer(r'\}\s*(\w+)\s*;', s):
            out.setdefault(m.group(1), p)
        for m in re.finditer(r'\bstruct\s+(\w+)\s*\{', s):
            out.setdefault(m.group(1), p)
        for m in re.finditer(r'typedef\s+[^;{]*?\b(\w+)\s*;', s):
            out.setdefault(m.group(1), p)
        for m in re.finditer(r'typedef[^;{]*?\(\s*(?:\w+\s+)?\*\s*(\w+)\s*\)', s):
            out.setdefault(m.group(1), p)
    for p in glob.glob(os.path.join(ROOT, 'ports/64b/platform/include/*.h')):
        s = open(p, encoding='latin-1').read()
        for m in re.finditer(r'\b(\w+)\s*;', s):
            out.setdefault(m.group(1), 'platform')
    return out


def decl_text(name, t):
    """C declarator text for `name` of type t (handles arrays and fn pointers)."""
    t = norm(t)
    m = re.match(r'^(.*?)\(\*\s*(const\s*)?((?:\[\w*\])*)\)(\(.*\))$', t)
    if m:
        return '%s (*%s%s%s)%s' % (m.group(1).strip(), (m.group(2) or '').strip() + ' ' if m.group(2) else '',
                                  name, m.group(3), m.group(4))
    m = re.match(r'^(.*?)\s*((?:\[\w*\])+)$', t)
    if m:
        inner = m.group(1)
        mm = re.match(r'^(.*?)\(\*\)(\(.*\))$', inner)
        if mm:
            return '%s (*%s%s)%s' % (mm.group(1).strip(), name, m.group(2), mm.group(2))
        return '%s %s%s' % (inner, name, m.group(2))
    return '%s %s' % (t, name) if not t.endswith('*') else '%s%s' % (t, name)


def score(r):
    t = r['type']
    k = kind(t)
    s = int(r['size32'] or 0) * 10
    s += {'struct': 4, 'array': 3, 'ptr': 2, 'scalar': 1}[k]
    if not re.match(r'^(DAT|PTR|_DAT|g_?[0-9A-F]{6}|BrG_|g_br[0-9A-F])', r['name']):
        s += 5
    if re.search(r'_[0-9A-F]{8}$', r['name']):
        s -= 6          # a name vafix.py made unique: any other name is better
    if '/' in r.get('decl_file', '') and r['decl_file'].endswith('.h') and kind(t) == 'ptr' and \
            not re.match(r'^(void|int|char|unsigned)', t):
        s += 1          # a header's typed pointer over a file's own view
    s += int(r['tus']) * 0
    return s


def main():
    os.chdir(ROOT)
    rows = [r for r in csv.DictReader(open('build/portable/globals.csv')) if r['va']]
    by = collections.defaultdict(list)
    for r in rows:
        by[int(r['va'], 16)].append(r)
    hdr_for = header_of_types()
    # an unsized array's extent is the distance to the next known object
    order = sorted(by)
    nxt = {va: (order[i + 1] if i + 1 < len(order) else va + 4) for i, va in enumerate(order)}
    for va, rs in by.items():
        for r in rs:
            if not r['size32'] and re.search(r'\[\]$', norm(r['type'])):
                r['size32'] = str(min(nxt[va] - va, 0x100000))
                r['extent_guess'] = '1'
    # canonical per address
    canon = {}
    for va, rs in by.items():
        best = max(rs, key=score)
        canon[va] = best
    forced = {}
    ov = os.path.join(ROOT, 'ports/64b/types/globals_override.csv')
    if os.path.exists(ov):
        for o in csv.DictReader(open(ov)):
            va = int(o['va'], 16)
            row = {'name': o['name'], 'type': o['type'], 'size32': o['size32'], 'tus': '1'}
            by[va].append(dict(row, decl_file='', line='', storage='extern', defines='0'))
            canon[va] = row
            forced[va] = int(o['size32'])
    # containment: an address inside a bigger canonical object
    vas = sorted(canon)
    parent = {}
    RAW = re.compile(r'^(_?DAT_|PTR_|g_?br?[0-9A-F]{6}$|g_[0-9A-F]{6}$|BrG_[0-9A-F]{6}$|g_br[A-Z]?[0-9A-F]{6}$)')

    def extent(vb):
        return max([int(r['size32'] or 0) for r in by[vb]] or [0])

    def absorbable(vb, end):
        if vb + extent(vb) > end:
            return False          # it would run past the object it sits in
        return all(RAW.match(r['name']) or kind(r['type']) == 'scalar' for r in by[vb])
    for va in vas:
        c = canon[va]
        size = int(c['size32'] or 0)
        for vb in vas:
            if va < vb < va + size and vb not in parent:
                if va in forced or absorbable(vb, va + size):
                    parent[vb] = va
                else:
                    break           # a separately named object ends this one
    plan = []   # (va, canonical row, [(alias name, alias type, expr)])
    for va in vas:
        if va in parent:
            continue
        c = canon[va]
        aliases = []
        names = {(r['name'], norm(r['type'])) for r in by[va]}
        for nm, t in sorted(names):
            if nm == c['name']:
                continue
            aliases.append((nm, t, alias_expr(c, t, 0)))
        plan.append([va, c, aliases])
    index = {p[0]: p for p in plan}
    for vb, va in sorted(parent.items()):
        top = va
        while top in parent:
            top = parent[top]
        p = index[top]
        c = p[1]
        d = vb - top
        for nm, t in sorted({(r['name'], norm(r['type'])) for r in by[vb]}):
            if nm == c['name']:
                continue
            p[2].append((nm, t, alias_expr(c, t, d)))
    if '--specs' in sys.argv:
        prepare_specs(plan, hdr_for)
        return plan
    if '--report' in sys.argv or DRY:
        n_alias = sum(len(p[2]) for p in plan)
        n_bytes = sum(1 for p in plan for a in p[2] if 'BR_LP64' in a[2])
        print('canonical objects: %d; aliases: %d (%d through a byte view that may not survive 64-bit layout)' % (
            len(plan), n_alias, n_bytes))
        if DRY:
            for p in plan[:0]:
                pass
            return plan
    # the canonical objects, for imagemap.py (the original data image)
    with open(os.path.join(ROOT, 'build/portable/canon.csv'), 'w', newline='') as fh:
        cw = csv.writer(fh)
        cw.writerow(['va', 'name', 'type', 'size32', 'forced'])
        for va, c, _ in plan:
            cw.writerow(['0x%08X' % va, c['name'], norm(c['type']), c.get('size32') or '', int(va in forced)])
    write(plan, hdr_for)
    return plan


def prepare_specs(plan, hdr_for):
    """Give every struct type that has aliases inside it a spec, and turn
    each alias that lands in its padding into a named field."""
    import subprocess
    need = collections.defaultdict(list)    # struct type -> [(offset, alias type, alias name)]
    for va, c, aliases in plan:
        base = re.sub(r'\b(const|volatile|struct)\b', '', norm(c['type'])).strip()
        am = re.match(r'^(\w+)\s*((?:\[\w*\])*)$', base)
        if not am or am.group(1) not in hdr_for or hdr_for[am.group(1)] == 'platform':
            continue
        st = am.group(1)
        if kind(st) != 'struct':
            continue
        for nm, at, ex in aliases:
            if 'BR_LP64_BYTE_VIEW' not in ex:
                continue
            d = int(re.search(r'\+ 0x([0-9A-F]+)\)\)', ex).group(1), 16)
            need[st].append((d, at, nm, bool(am.group(2))))
    made = 0
    for st, lst in need.items():
        if not rewrite.spec(st):
            hdr = os.path.relpath(hdr_for[st], ROOT)
            p = subprocess.run([sys.executable, os.path.join(ROOT, 'ports/64b/tools/recspec.py'),
                                'from-header', st, hdr], capture_output=True, text=True, cwd=ROOT)
            if p.returncode != 0:
                print('  no spec for %s: %s' % (st, p.stderr.strip()[-200:]))
                continue
            rewrite._specs.pop(st, None)
            made += 1
        try:
            added = absorb_aliases(st, lst)
        except SystemExit as e:
            print('  %s: %s' % (st, e))
        rewrite._specs.pop(st, None)
    print('specs created: %d; struct types touched: %d' % (made, len(need)))


def absorb_aliases(st, lst):
    if True:
        meta, fields = rewrite.spec(st)
        if not meta.get('headers', '').strip() and meta.get('header', '').endswith('.h'):
            meta['headers'] = os.path.basename(meta['header'])
        hdrs = meta.get('headers', '').split()
        size = int(meta['size'], 16)
        added = 0
        for d, at, nm, is_array in lst:
            if is_array:
                d %= size
            if any(f.off <= d < f.off + recspec.size32(f.ty, hdrs) for f in fields):
                continue
            if kind(at) == 'array' and re.search(r'\[\]$', at):
                continue
            try:
                recspec.size32(at, hdrs)
            except SystemExit:
                continue
            fields.append(recspec.Field(d, norm(at), nm))
            added += 1
        if added:
            recspec.save(st, meta, fields)
            recspec.cmd_emit(st)
        return added


_CHANGES = None


def layout_changes():
    global _CHANGES
    if _CHANGES is None:
        _CHANGES = {}
        p = os.path.join(ROOT, 'build/lp64audit/layout.csv')
        if os.path.exists(p):
            for r in csv.DictReader(open(p)):
                _CHANGES[r['record'].split(' ', 1)[-1]] = r['i686'] != r['llp64']
    return _CHANGES


def elem_size(elem):
    sp = rewrite.spec(elem.strip())
    if sp:
        return int(sp[0]['size'], 16)
    try:
        return recspec.size32(elem, [])
    except SystemExit:
        return None


def field_view(lv, fty, t):
    """A file's view (type t) of the field lv (type fty)."""
    if rewrite.same_type(fty, t):
        return '(%s)' % lv
    if kind(fty) == 'ptr' and kind(t) == 'ptr':
        return '(*(%s)&%s)' % (ptr_type_of(t), lv)     # the same pointer, this file's pointee
    if kind(fty) == 'ptr' and kind(t) == 'scalar':
        return '(%s)' % lv          # an integer view of a pointer: the compiler names each use
    return '(*(%s)&%s)' % (ptr_type_of(t), lv)


def alias_expr(c, t, d):
    cn, ct = c['name'], norm(c['type'])
    ck, ak = kind(ct), kind(t)
    if d == 0:
        if norm(t) == ct or re.sub(r'\[\w*\]$', '[]', norm(t)) == re.sub(r'\[\w*\]$', '[]', ct):
            return cn
        # the first field of the first element of an array of spec'd records
        am1 = re.match(r'^(?:struct\s+)?(\w+)\s*\[\w*\]$', re.sub(r'\b(const|volatile)\b', '', ct).strip())
        if am1 and rewrite.spec(am1.group(1)) and '[' not in t:
            esz = elem_size(am1.group(1))
            try:
                asz = recspec.size32(t, [])
            except SystemExit:
                asz = None
            if asz and esz and asz < esz:
                pth, fty = rewrite.resolve(am1.group(1), 0, t)
                if pth:
                    if rewrite.same_type(fty, t):
                        return '(%s[0].%s)' % (cn, pth)
                    if kind(fty) == 'ptr' and kind(t) == 'ptr':
                        return '(*(%s)&%s[0].%s)' % (ptr_type_of(t), cn, pth)
                    if kind(fty) == 'ptr' or kind(t) == 'ptr':
                        return '(%s[0].%s)' % (cn, pth)
                    return '(*(%s)&%s[0].%s)' % (ptr_type_of(t), cn, pth)
        am0 = re.match(r'^(.*?)\s*\[\w*\]$', ct)
        if am0 and ak in ('scalar', 'ptr') and not re.search(r'\[', t):
            # a scalar view of an array's first element: element 0, so that
            # `(&NAME)[i]` is element i
            esz = elem_size(am0.group(1))
            try:
                asz = recspec.size32(t, [])
            except SystemExit:
                asz = None
            if esz and asz and esz == asz:
                return '(%s[0])' % cn
        if ck == 'ptr' and ak == 'ptr' and '[' not in t and '[' not in ct:
            return '(*(%s)&%s)' % (ptr_type_of(t), cn)      # the same address, this file's pointee type
        if '[' in t and '[' in ct and re.search(r'\*\s*\[', t) and re.search(r'\*\s*\[', ct):
            return '(*(%s)&%s)' % (ptr_type_of(t), cn)      # a table of pointers, this file's pointee
        if ck == 'struct' and ak in ('scalar', 'ptr'):
            return '(*(%s)&%s)' % (ptr_type_of(t), cn)      # the first field: offset 0 holds on any layout
        if ck == 'ptr' and re.match(r'^(float|double)$', norm(t)):
            return '(*(%s)&%s)' % (ptr_type_of(t), cn)      # a float kept in a pointer-sized slot
        if ck == 'ptr' or ak == 'ptr':
            return cn
        if ck == 'array' and ak == 'scalar':
            elem = re.sub(r'\[\w*\]$', '', ct)
            if norm(elem) == norm(t):
                return '(%s[0])' % cn
        if ck == 'array' and ak == 'array':
            celem = re.sub(r'\[\w*\]$', '', ct).strip()
            aelem = re.sub(r'\[\w*\]$', '', norm(t)).strip()
            if kind(celem) == 'ptr' and kind(aelem) != 'ptr':
                return cn       # an integer view of a pointer table: use the table
            return '(*(%s)&%s)' % (ptr_type_of(t), cn)
        return '(*(%s)&%s)' % (ptr_type_of(t), cn)
    # inside a bigger object
    base = re.sub(r'\b(const|volatile|struct)\b', '', ct).strip()
    am = re.match(r'^(.*?)\s*\[(\w*)\]((?:\[\w*\])*)$', base)
    if am:
        elem = am.group(1) + am.group(3)
        esz = elem_size(elem)
        if esz:
            i, d2 = divmod(d, esz)
            if d2 == 0:
                if norm(elem) == norm(t) or kind(elem) == 'ptr' or kind(t) == 'ptr':
                    return '(%s[%d])' % (cn, i)
                return '(*(%s)&%s[%d])' % (ptr_type_of(t), cn, i)
            if rewrite.spec(elem.strip()):
                pth, fty = rewrite.resolve(elem.strip(), d2, t)
                if pth:
                    return field_view('%s[%d].%s' % (cn, i, pth), fty, t)
    if rewrite.spec(base):
        pth, fty = rewrite.resolve(base, d, t)
        if pth:
            return field_view('%s.%s' % (cn, pth), fty, t)
    st = re.match(r'(\w+)', base).group(1) if re.match(r'(\w+)', base) else ''
    exact = st in SCALARS or re.match(r'^(u?int\d+_t|int|char|short|float|unsigned)', st) or \
        layout_changes().get(st) is False
    mark = '/* byte view: the object holds no pointers */' if exact else '/* BR_LP64_BYTE_VIEW */'
    return '(*(%s)((char *)&%s + 0x%X)) %s' % (ptr_type_of(t), cn, d, mark)


def ptr_type_of(t):
    t = norm(t)
    m = re.match(r'^(.*?)\s*((?:\[\w*\])+)$', t)
    if m:
        return '%s (*)%s' % (m.group(1), m.group(2))
    m = re.match(r'^(.*?)\(\*\)(\(.*\))$', t)
    if m:
        return '%s (**)%s' % (m.group(1).strip(), m.group(2))
    return t + ' *'


def pick_alias(variants):
    """One expression per alias name: a plain (typed) one if any variant has
    it, else the reinterpretation the most declarations asked for."""
    plain = [ex for t, ex in variants if not ex.startswith('(*(')]
    if plain:
        return collections.Counter(plain).most_common(1)[0][0]
    return collections.Counter(ex for t, ex in variants).most_common(1)[0][0]


FORWARD = set()
KNOWN_CORE = set()


def write(plan, hdr_for):
    core = open(os.path.join(INC, 'br_coretypes.h'), encoding='latin-1').read()
    KNOWN_CORE.update(re.findall(r'\}\s*(\w+)\s*;', core))
    KNOWN_CORE.update(re.findall(r'typedef\s+[^;{]*?\b(\w+)\s*;', core))
    KNOWN_CORE.update(re.findall(r'typedef[^;{]*?\(\s*(?:\w+\s+)?\*\s*(\w+)\s*\)', core))
    KNOWN_CORE.update({'FILE', 'size_t', 'va_list', 'time_t', 'ptrdiff_t', 'wchar_t', 'fpos_t', 'div_t'})
    for wp in glob.glob(os.path.join(ROOT, 'ports/64b/platform/include/*.h')):
        w = open(wp, encoding='latin-1').read()
        KNOWN_CORE.update(re.findall(r'\}\s*(\w+)\s*[,;]', w))
        KNOWN_CORE.update(re.findall(r'typedef\s+[^;{]*?\b(\w+)\s*[,;]', w))
        KNOWN_CORE.update(re.findall(r'typedef[^;{]*?\(\s*(?:\w+\s+)?\*\s*(\w+)\s*\)', w))
        KNOWN_CORE.update(re.findall(r'BR_DECLARE_HANDLE\((\w+)\)', w))
    global_decls, struct_decls, defs = [], collections.defaultdict(list), []
    canon_vas = sorted({v for v, _, _ in plan})
    macros = collections.OrderedDict()
    homes = {}                  # every unified name -> header its object lives in
    unified = set()
    for va, c, aliases in plan:
        t = norm(c['type'])
        home = None
        for w in type_words(t):
            h = hdr_for.get(w)
            if h and h != 'platform':
                home = h
        for w in type_words(t):
            if w not in hdr_for and w not in KNOWN_CORE and not re.match(r'^(struct)$', w):
                t = re.sub(r'(?<!struct )\b%s\b' % re.escape(w), 'struct ' + w, t)
                FORWARD.add(w)
        line = ('#pragma push_macro("%s")\n#undef %s\nextern %s;  /* 0x%08X */\n#pragma pop_macro("%s")'
                % (c['name'], c['name'], decl_text(c['name'], t), va, c['name']))
        (struct_decls[home] if home else global_decls).append(line)
        unified.add(c['name'])
        if home:
            homes[c['name']] = home
        byname = collections.defaultdict(list)
        for nm, at, ex in aliases:
            byname[nm].append((at, ex))
        for nm, variants in byname.items():
            if nm in macros or nm == c['name']:
                continue
            macros[nm] = pick_alias(variants)
            unified.add(nm)
            if home:
                homes[nm] = home
        dt = t
        if re.search(r'\[\]$', t):
            elem = re.sub(r'\[\]$', '', t).strip()
            esz = elem_size(elem) or 0
            if not esz:
                try:
                    esz = recspec.size32(elem, [os.path.basename(home)] if home else ['br_globals.h'])
                except SystemExit:
                    esz = 0
            # unsized: as many elements as fit before the next original
            # object (an empty [] would define ONE element)
            k = bisect.bisect_right(canon_vas, va)
            gap = (canon_vas[k] - va) if k < len(canon_vas) else 0
            n = int(c['size32']) if c.get('size32') and int(c['size32']) > esz else gap
            if esz and n:
                dt = '%s[%d]' % (elem, max(1, n // esz))
            elif esz:
                print('unsized with nothing after it: %s at 0x%08X' % (c['name'], va))
        defs.append((home, '%s;  /* 0x%08X */' % (decl_text(c['name'], dt), va)))
    fwd = ''.join('struct %s;\n' % w for w in sorted(FORWARD))
    if DRY:
        return
    hdrs = sorted({h for h, _ in defs if h})
    with open(GH, 'w') as fh:
        fh.write('/* br_globals.h: the original game\'s globals, one declaration per object\n'
                 ' * (generated by ports/64b/tools/unify.py; the names the decompiled\n'
                 ' * files used are aliases of these). Struct-typed ones are declared at the\n'
                 ' * end of the header that defines the struct; a file using one includes it. */\n'
                 '#ifndef BR_GLOBALS_H\n#define BR_GLOBALS_H\n#include <stdint.h>\n#include "br_coretypes.h"\n\n')
        fh.write(fwd + '\n')
        fh.write('#ifdef __cplusplus\nextern "C" {\n#endif\n')
        fh.write('\n'.join(global_decls) + '\n')
        fh.write('#ifdef __cplusplus\n}\n#endif\n\n')
        fh.write('/* The other names the decompiled files used for these objects are\n'
                 ' * defined per file (build/portable/alias/<file>.h), each with the type\n'
                 ' * that file gave it. */\n')
        fh.write('\n#endif\n')
    for h in set(list(struct_decls) + [os.path.join(INC, x) for x in os.listdir(INC) if x.endswith('.h')]):
        s = open(h, encoding='latin-1').read()
        s2 = re.sub(r'\n/\* BR_GLOBALS_BEGIN.*?/\* BR_GLOBALS_END \*/\n', '\n', s, flags=re.S)
        lines = struct_decls.get(h)
        if lines:
            block = '\n/* BR_GLOBALS_BEGIN: generated by ports/64b/tools/unify.py */\n' + \
                '#ifdef __cplusplus\nextern "C" {\n#endif\n' + '\n'.join(lines) + \
                '\n#ifdef __cplusplus\n}\n#endif\n/* BR_GLOBALS_END */\n'
            k = s2.rfind('#endif')
            s2 = s2[:k] + block + s2[k:] if k != -1 else s2 + block
        if s2 != s:
            open(h, 'w', encoding='latin-1').write(s2)
    os.makedirs(os.path.dirname(GC), exist_ok=True)
    with open(GC, 'w') as fh:
        fh.write('/* br_globals.c: the one definition of each original global (generated by\n'
                 ' * ports/64b/tools/unify.py). Initial values: the data lift. */\n')
        for h in hdrs:
            fh.write('#include "%s"\n' % os.path.basename(h))
        fh.write('\n')
        for h, d in defs:
            fh.write(d + '\n')
    open(os.path.join(ROOT, 'build/portable/unified.txt'), 'w').write('\n'.join(sorted(unified)) + '\n')
    with open(os.path.join(ROOT, 'build/portable/homes.csv'), 'w') as fh:
        for nm, h in sorted(homes.items()):
            fh.write('%s,%s\n' % (nm, os.path.basename(h)))
    if '--no-remove' not in sys.argv:
        remove_decls(unified)
    purge_decls(unified)
    purge_defs(unified)
    alias_headers(plan, macros)
    add_home_includes(homes)
    print('br_globals.h: %d declarations, %d aliases; struct headers: %d; definitions: %d' % (
        len(global_decls), len(macros), len(struct_decls), len(defs)))


def purge_decls(unified):
    """Remove every remaining `extern` declaration of a unified name, at any
    scope, in any core file or header (br_globals.h and the generated blocks
    excepted)."""
    names = sorted(unified, key=len, reverse=True)
    rx = re.compile(r'(?<![\w])extern\s+(?:"C"\s+)?[^;{}()]*?\b(%s)\b\s*(?:\[[^\]]*\])*\s*;[^\n]*' %
                    '|'.join(map(re.escape, names)))
    n = 0
    files = [os.path.join(dp, fn) for d in ('ports/64b/src/core', 'ports/64b/include')
             for dp, _, fns in os.walk(os.path.join(ROOT, d)) for fn in fns if fn.endswith(('.c', '.cpp', '.h'))]
    for p in files:
        if os.path.basename(p) in ('br_globals.h', 'br_globals.c', 'br_coretypes.h'):
            continue
        s = open(p, encoding='latin-1').read()
        keep = {}
        for i, m in enumerate(re.finditer(r'/\* BR_GLOBALS_BEGIN.*?BR_GLOBALS_END \*/', s, re.S)):
            keep['@@BRG%d@@' % i] = m.group(0)
        for k, v in keep.items():
            s = s.replace(v, k)
        s2, k2 = rx.subn('/* 64-bit core: declared once, in br_globals.h or its struct\'s header */', s)
        for k, v in keep.items():
            s2 = s2.replace(k, v)
        if k2:
            open(p, 'w', encoding='latin-1').write(s2)
            n += k2
    print('remaining declarations purged: %d' % n)


def alias_headers(plan, macros):
    """build/portable/alias/<file>.h: every alias name a core file uses,
    defined with the type THAT file (or a header it includes) declared."""
    exprs = collections.defaultdict(dict)     # name -> {type: expr}
    canon_t = {}
    for va, c, aliases in plan:
        for nm, at, ex in aliases:
            exprs[nm].setdefault(norm(at), ex)
        canon_t[c['name']] = norm(c['type'])
    # a file may name the canonical object itself with its own type: a
    # self-referential macro (never re-expanded) gives it that view
    for va, c, aliases in plan:
        exprs[c['name']].setdefault(norm(c['type']), c['name'])
    rows0 = [r for r in csv.DictReader(open('build/portable/globals.csv')) if r['va']]
    plan_by_name = {c['name']: c for va, c, al in plan}
    for r in rows0:
        nm, t = r['name'], norm(r['type'])
        if nm in plan_by_name and t not in exprs[nm]:
            exprs[nm][t] = alias_expr(plan_by_name[nm], t, 0)
    rows = [r for r in csv.DictReader(open('build/portable/globals.csv')) if r['va']]
    # files whose code was retyped to the canonical object: their own view goes
    retired = set()
    rp = os.path.join(ROOT, 'ports/64b/types/views_retired.csv')
    if os.path.exists(rp):
        retired = {(r['file'], r['name']) for r in csv.DictReader(open(rp))}
    rows = [r for r in rows if (r['decl_file'], r['name']) not in retired]
    own = collections.defaultdict(dict)       # file -> {name: type}
    hdr = collections.defaultdict(dict)       # header basename -> {name: type}
    hdr_text = {}
    hdr_names = {}
    for r in rows:
        if r['name'] not in exprs:
            continue
        f = r['decl_file']
        if f.startswith('ports/64b/src/'):
            own[f].setdefault(r['name'], norm(r['type']))
        elif f.endswith('.h') and 'br_globals' not in f:
            # only what the header declares today (the inventory remembers
            # declarations a header has since given up)
            if f not in hdr_text:
                hdr_text[f] = open(os.path.join(ROOT, f), encoding='latin-1').read() \
                    if os.path.exists(os.path.join(ROOT, f)) else ''
            if not re.search(r'^extern[^;]*\b%s\b' % re.escape(r['name']), hdr_text[f], re.M):
                continue
            hdr[os.path.basename(f)].setdefault(r['name'], norm(r['type']))
    outdir = os.path.join(ROOT, 'build/portable/alias')
    os.makedirs(outdir, exist_ok=True)
    for old in glob.glob(os.path.join(outdir, '*.h')):
        os.remove(old)
    names_rx = re.compile(r'\b(%s)\b' % '|'.join(map(re.escape, sorted(exprs, key=len, reverse=True))))
    n = 0
    for dp, _, fns in os.walk(os.path.join(ROOT, 'ports/64b/src/core')):
        for fn in fns:
            if not fn.endswith(('.c', '.cpp')):
                continue
            path = os.path.join(dp, fn)
            rel = os.path.relpath(path, ROOT)
            s = open(path, encoding='latin-1').read()
            incs = set(re.findall(r'#\s*include\s*"([^"]+)"', s))
            used = set(names_rx.findall(s))
            # names used through macros in included headers: every name a
            # header this file includes mentions in its code
            for h in incs:
                if h not in hdr_names:
                    hp = os.path.join(ROOT, 'ports/64b/include', h)
                    ht = open(hp, encoding='latin-1').read() if os.path.exists(hp) else ''
                    ht = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', ht, flags=re.S)
                    ht = re.sub(r'#pragma (push|pop)_macro\("\w+"\)|#undef \w+', ' ', ht)
                    hdr_names[h] = set(names_rx.findall(ht))
                used |= hdr_names[h] | set(hdr.get(h, {}))
            lines = []
            for nm in sorted(used):
                # a plain word (`slots`, `table`) is a member or local name as
                # often as a global: alias it only where this file, or a
                # header it includes, declared the global
                if re.match(r'^[a-z]+$', nm) and nm not in own[rel] and \
                        not any(nm in hdr.get(h, {}) for h in incs):
                    continue
                ty = own[rel].get(nm)
                if ty is None:
                    for h in incs:
                        if nm in hdr.get(h, {}):
                            ty = hdr[h][nm]
                            break
                ex = exprs[nm].get(ty) if ty else None
                if ex is None:
                    ex = macros.get(nm)
                if ex is None or ex == nm:
                    continue
                lines.append('#define %s %s' % (nm, ex))
            if lines:
                key = rel[len('ports/64b/src/core/'):].replace('/', '__')
                open(os.path.join(outdir, key + '.h'), 'w').write(
                    '/* generated by ports/64b/tools/unify.py */\n' + '\n'.join(lines) + '\n')
                n += 1
    print('per-file alias headers: %d' % n)


def scalarize_array_uses(macros, plan):
    """A file that declared NAME as an array, where NAME now means a scalar,
    indexes it: NAME[0] -> NAME, NAME[k] -> (&NAME)[k]."""
    scalar = {}
    for va, c, aliases in plan:
        if kind(norm(c['type'])) == 'scalar':
            scalar[c['name']] = True
            for nm, at, ex in aliases:
                if macros.get(nm) == c['name'] or macros.get(nm, '').startswith('('):
                    pass
    rows = [r for r in csv.DictReader(open('build/portable/globals.csv')) if r['va']]
    want = collections.defaultdict(set)
    for r in rows:
        nm = r['name']
        target = macros.get(nm, nm)
        bare = re.match(r'^\(?([A-Za-z_]\w*)\)?$', target)
        if not bare or bare.group(1) not in scalar:
            continue
        if kind(norm(r['type'])) == 'array' and r['tus'] and r['decl_file'].startswith('ports/64b/src/'):
            want[r['decl_file']].add(nm)
    n = 0
    for f, names in want.items():
        s = open(f, encoding='latin-1').read()
        for nm in names:
            s, k1 = re.subn(r'\b%s\s*\[\s*0\s*\]' % re.escape(nm), nm, s)
            s, k2 = re.subn(r'(?<![&\w])\b%s\s*\[' % re.escape(nm), '(&%s)[' % nm, s)
            n += k1 + k2
        open(f, 'w', encoding='latin-1').write(s)
    print('array-style uses of scalars rewritten: %d' % n)


def purge_defs(unified):
    """Remove file-scope DEFINITIONS of unified names (stand-in copies the
    decompiled files kept, often inside `extern "C" { }`): the one definition
    is in br_globals.c. Only text at brace depth 0, or directly inside an
    extern "C" block, is considered -- never a function's locals."""
    names = set(unified)
    n = 0
    for dp, _, fns in os.walk(os.path.join(ROOT, 'ports/64b/src/core')):
        for fn in fns:
            if not fn.endswith(('.c', '.cpp')) or fn == 'br_globals.c':
                continue
            path = os.path.join(dp, fn)
            src = open(path, encoding='latin-1').read()
            # mark positions that are file scope
            scope = []          # stack of 'C' (extern "C" block) / 'B' (other)
            ok = bytearray(len(src))
            i, L = 0, len(src)
            while i < L:
                c = src[i]
                if src.startswith('/*', i):
                    j = src.find('*/', i + 2)
                    j = L if j == -1 else j + 2
                    for k in range(i, j):
                        ok[k] = 2
                    i = j
                    continue
                if src.startswith('//', i):
                    j = src.find('\n', i)
                    j = L if j == -1 else j
                    i = j
                    continue
                if c == '"' or c == "'":
                    j = i + 1
                    while j < L and src[j] != c:
                        j += 2 if src[j] == '\\' else 1
                    i = j + 1
                    continue
                if c == '{':
                    pre = src[max(0, i - 20):i]
                    scope.append('C' if re.search(r'extern\s+"C"\s*$', pre) else 'B')
                elif c == '}':
                    if scope:
                        scope.pop()
                ok[i] = 1 if all(x == 'C' for x in scope) else 0
                i += 1
            out, last = [], 0
            rx = re.compile(r'^[ \t]*(?:static\s+|const\s+|volatile\s+|unsigned\s+|signed\s+|struct\s+|class\s+)*'
                            r'[A-Za-z_][\w:]*[\s\*&]+(?:const\s+)?\*?\s*([A-Za-z_]\w*)\s*(?:\[[^\];]*\])*\s*'
                            r'(?:=[^;]*)?;[^\n]*$', re.M)
            for m in rx.finditer(src):
                if m.group(1) not in names or not ok[m.start(1)]:
                    continue
                if '(' in m.group(0).split('=')[0]:
                    continue
                out.append(src[last:m.start()])
                out.append('/* 64-bit core: %s is defined once, in br_globals.c */' % m.group(1))
                last = m.end()
                n += 1
            if out:
                out.append(src[last:])
                open(path, 'w', encoding='latin-1').write(''.join(out))
    print('stand-in definitions removed: %d' % n)


def add_home_includes(homes):
    """A file that names an object declared in a struct's header includes it."""
    by_home = collections.defaultdict(set)
    for nm, h in homes.items():
        by_home[os.path.basename(h)].add(nm)
    rx = {h: re.compile(r'\b(%s)\b' % '|'.join(map(re.escape, sorted(names)))) for h, names in by_home.items()}
    n = 0
    for dp, _, fns in os.walk(os.path.join(ROOT, 'ports/64b/src/core')):
        for fn in fns:
            if not fn.endswith(('.c', '.cpp')):
                continue
            p = os.path.join(dp, fn)
            s = open(p, encoding='latin-1').read()
            rel = os.path.relpath(p, ROOT)
            al = os.path.join(ROOT, 'build/portable/alias', rel[len('ports/64b/src/core/'):].replace('/', '__') + '.h')
            # the objects' headers go ahead of the first top-level #include,
            # so every use in the file sees them; recomputed from what the
            # code names (an alias line counts only when the code uses it)
            body = re.sub(r'^#include "[^"]+"   /\* br_globals: its objects \*/\n', '', s, flags=re.M)
            code = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', body, flags=re.S)
            seen = code
            if os.path.exists(al):
                for m in re.finditer(r'^#define (\w+) (.*)$', open(al).read(), re.M):
                    if re.search(r'\b%s\b' % re.escape(m.group(1)), code):
                        seen += '\n' + m.group(2)
            need = sorted(h for h, r in rx.items() if r.search(seen)
                          and not re.search(r'#\s*include\s*"%s"' % re.escape(h), body))
            at, depth, pos = None, 0, 0
            in_c = False
            for line in body.split('\n'):
                t = line.strip()
                if not in_c:
                    if depth == 0 and re.match(r'#\s*include\b', t):
                        at = pos
                        break
                    if re.match(r'#\s*if', t):
                        depth += 1
                    elif re.match(r'#\s*endif', t):
                        depth -= 1
                # track block comments so a commented #include is not taken
                k = 0
                while True:
                    if in_c:
                        e = line.find('*/', k)
                        if e == -1:
                            break
                        in_c, k = False, e + 2
                    else:
                        b2 = line.find('/*', k)
                        if b2 == -1:
                            break
                        in_c, k = True, b2 + 2
                pos += len(line) + 1
            if at is None:
                at = 0
            new = body[:at] + ''.join('#include "%s"   /* br_globals: its objects */\n' % h for h in need) + body[at:]
            if new == s:
                continue
            s = new
            open(p, 'w', encoding='latin-1').write(s)
            n += 1
    print('files given a header include: %d' % n)


def stmt_span(text, pos):
    """(start, end) of the file-scope declaration statement around pos."""
    st = text.rfind('\n', 0, pos) + 1
    while st > 0:
        prev_nl = text.rfind('\n', 0, st - 1) + 1
        prev = text[prev_nl:st - 1].rstrip()
        if not prev or prev.endswith((';', '}', '*/')) or prev.lstrip().startswith(('#', '//')):
            break
        st = prev_nl
    depth, i = 0, pos
    while i < len(text):
        c = text[i]
        if c in '({[':
            depth += 1
        elif c in ')}]':
            depth -= 1
        elif c == ';' and depth <= 0:
            return st, i + 1
        i += 1
    return st, len(text)


def remove_decls(unified):
    rows = [r for r in csv.DictReader(open('build/portable/globals.csv')) if r['name'] in unified and r['line']]
    by = collections.defaultdict(set)
    for r in rows:
        if r['decl_file'].startswith('ports/64b/') and '/platform/' not in r['decl_file'] \
                and not r['decl_file'].endswith('br_globals.h'):
            by[r['decl_file']].add((int(r['line']), r['name']))
    n = skipped = 0
    for f, items in by.items():
        path = os.path.join(ROOT, f)
        text = open(path, encoding='latin-1').read()
        lines = text.split('\n')
        spans = set()
        for line, name in items:
            if line > len(lines):
                continue
            off = sum(len(l) + 1 for l in lines[:line - 1])
            m = re.search(r'\b%s\b' % re.escape(name), lines[line - 1])
            if not m:
                skipped += 1
                continue
            a, b = stmt_span(text, off + m.start())
            stmt = text[a:b]
            if '(' in stmt.split('=')[0] and not re.search(r'\(\s*\*', stmt.split('=')[0]):
                skipped += 1      # a function, not a variable
                continue
            spans.add((a, b))
        for a, b in sorted(spans, reverse=True):
            stmt = text[a:b]
            names = set(re.findall(r'[A-Za-z_]\w*', re.sub(r'=.*', '', stmt, flags=re.S)))
            text = text[:a] + '/* 64-bit core: declared once, in br_globals.h or its struct\'s header */' + text[b:]
            n += 1
        open(path, 'w', encoding='latin-1').write(text)
    print('declarations removed: %d (%d skipped)' % (n, skipped))


if __name__ == '__main__':
    main()
