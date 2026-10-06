#!/usr/bin/env python3
"""globals.py -- one native definition per original data address, and the
table the platform lifts the original's initial values through.

The decomp's TUs only declare the game's data (`extern T D_80XXXXXX;`): in
the matching build the linker resolves every name to its ROM address.  The
native core needs a definition per address and the original's initial
values.  For every data symbol the TUs reference this picks one owning TU
(the one whose declaration has the most complete type), and writes

  build/tgrally/null-null/gen/own/<tu>.c   the TU itself (#include) followed by the
                                 definitions it owns and its rows of the
                                 symbol table: native address and size,
                                 original address and size, and the runs
                                 that lay the original's big-endian bytes
                                 into the native object
  build/tgrally/null-null/gen/tgr_syms.c   the table's spine (each TU's rows), the
                                 runs, and the function table (original
                                 address -> native function) the lift uses
                                 for code pointers held in data
  build/tgrally/null-null/gen/arena.s      symbols outside the program image (fixed
                                 RAM the game uses as buffers) as aliases
                                 into the platform's RDRAM arena
  build/tgrally/null-null/globals.txt      what the source must resolve: an address
                                 declared at different sizes, an object
                                 that overlaps the next symbol

Layouts come from libclang (the `libclang` package in .venv), each TU read
twice: as IDO saw it (mips, ILP32 big-endian) and as the host compiles it.

Regions (original addresses):
  0x80000000..0x80200000, 0x80382BB0..0x80800000   arena (fixed RAM)
  0x80200000..0x8026FAB0   code (data names here are reported)
  0x8026FAB0..0x802AC400   .data/.rodata: defined, values lifted from the ROM
  0x802AC400..0x80382BB0   .bss: defined, zero
"""
import bisect
import csv
import os
import re
import subprocess
from concurrent.futures import ProcessPoolExecutor

import clang.cindex as ci

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(ROOT, os.environ.get('OUT', 'build/tgrally/null-null'))
GEN = os.path.join(OUT, 'gen')
NATIVE = os.environ.get('TGR_TARGET') or subprocess.run(['clang', '-dumpmachine'], capture_output=True,
                                                         text=True).stdout.strip()
N64 = 'mips-linux-gnu'

TEXT_LO, DATA_LO, BSS_LO, BSS_HI = 0x80200000, 0x8026FAB0, 0x802AC400, 0x80382BB0
ARENA_LO, ARENA_HI = 0x80000000, 0x80800000

# names the platform defines (libultra's own data)
PLATFORM = {'osTvType', 'osClockRate', 'osRomBase', 'osResetType', 'osMemSize', 'osAppNMIBuffer',
            'inflate_mask'}
# the microcode the ROM carries inside its code segment: the game only takes
# its addresses, which stay the original's
UCODE = {'rspbootTextStart', 'rspbootTextEnd', 'gspF3DEX_fifoTextStart'}

K = ci.TypeKind


def region(a):
    if a < TEXT_LO or a >= BSS_HI:
        return 'arena'
    if a < DATA_LO:
        return 'code'
    if a < BSS_LO:
        return 'data'
    return 'bss'


def symbol_addresses():
    syms = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'config/tgrally/symbols_tgr.csv'))):
        try:
            syms[r['name']] = int(r['va'], 16)
        except (KeyError, ValueError):
            pass
    return syms


def addr_of(name, syms):
    m = re.match(r'^D_([0-9A-Fa-f]{8})$', name)
    if m:
        return int(m.group(1), 16)
    return syms.get(name)


# ------------------------------------------------------------ the type walk
class Unlayable(Exception):
    pass


def leaves(t, off=0):
    """[(off, kind, size)] of a type: kind 'b' a unit of `size` bytes to
    swap, 'p' a pointer."""
    t = t.get_canonical()
    k = t.kind
    if k in (K.POINTER, K.BLOCKPOINTER):
        return [(off, 'p', t.get_size())]
    if k == K.CONSTANTARRAY:
        es = t.element_type.get_canonical().get_size()
        el = leaves(t.element_type)
        return [(off + i * es + o, kd, s) for i in range(t.element_count) for o, kd, s in el]
    if k == K.RECORD:
        decl = t.get_declaration()
        fields = list(t.get_fields())
        if decl.kind == ci.CursorKind.UNION_DECL:
            fields = fields[:1]
        out = []
        for f in fields:
            if f.is_bitfield():
                raise Unlayable('bitfield %s' % f.spelling)
            out += leaves(f.type, off + f.get_field_offsetof() // 8)
        return out
    if k in (K.INCOMPLETEARRAY, K.FUNCTIONPROTO, K.FUNCTIONNOPROTO, K.VOID):
        raise Unlayable('type %s has no layout' % t.spelling)
    s = t.get_size()
    if s <= 0:
        raise Unlayable('type %s has no size' % t.spelling)
    return [(off, 'b', s)]


def paths(t, path='', off=0):
    """[(off, size, path)] of a type's scalar and pointer members, the path
    as C spells it after the object's name (".w", "[3].x")."""
    t = t.get_canonical()
    k = t.kind
    if k == K.CONSTANTARRAY:
        es = t.element_type.get_canonical().get_size()
        el = paths(t.element_type)
        out = [(off + i * es + o, z, '%s[%d]%s' % (path, i, p)) for i in range(t.element_count)
               for o, z, p in el]
        return out
    if k == K.RECORD:
        out = []
        for f in t.get_fields():
            if f.is_bitfield():
                continue
            out += paths(f.type, path + '.' + f.spelling, off + f.get_field_offsetof() // 8)
        return out
    return [(off, max(1, t.get_size()), path)]


def runs(n64, nat):
    """Pair the two leaf lists into strided runs:
    [(n64off, natoff, size (0 = pointer), count, n64stride, natstride)]."""
    if len(n64) != len(nat):
        raise Unlayable('leaf count differs (%d/%d)' % (len(n64), len(nat)))
    out = []
    for (o0, k0, s0), (o1, k1, s1) in zip(n64, nat):
        if k0 != k1 or (k0 == 'b' and s0 != s1):
            raise Unlayable('leaf kinds differ at %d' % o0)
        s = s0 if k0 == 'b' else 0
        if out:
            a0, a1, asz, n, st0, st1 = out[-1]
            if asz == s:
                if n == 1 and o0 > a0 and o1 > a1:
                    out[-1] = (a0, a1, asz, 2, o0 - a0, o1 - a1)
                    continue
                if n > 1 and o0 == a0 + n * st0 and o1 == a1 + n * st1:
                    out[-1] = (a0, a1, asz, n + 1, st0, st1)
                    continue
        out.append((o0, o1, s, 1, 0, 0))
    return out


def tus():
    out = []
    for dp, dn, fn in os.walk(os.path.join(ROOT, 'ports/tgrally/src')):
        out += [os.path.relpath(os.path.join(dp, f), ROOT) for f in fn if f.endswith('.c')]
    return sorted(out)


def parse(tu, target):
    idx = ci.Index.create()
    inc = os.path.join(ROOT, 'ports/tgrally/platform/include')
    extra = ['-nostdinc', '-I' + os.path.join(ROOT, 'ports/tgrally/tools/include')] if target == N64 else []
    return idx.parse(os.path.join(ROOT, tu), args=['-target', target, '-std=gnu89', '-w', '-DTGR_NO_LIBC'] + extra + [
                                                    '-I' + os.path.join(ROOT, 'ports/tgrally/include'),
                                                    '-I' + inc, '-include', os.path.join(inc, 'ultra64.h'),
                                                    '-include', os.path.join(inc, 'tgr_core.h')])


def scan(tu):
    """Every top-level data variable a TU declares, read at both ABIs."""
    syms = symbol_addresses()
    out = {}
    seen = [{}, {}]
    used = set()
    for abi, target in enumerate((N64, NATIVE)):
        tree = parse(tu, target)
        if abi == 0:
            used = {c.spelling for c in tree.cursor.walk_preorder()
                    if c.kind == ci.CursorKind.DECL_REF_EXPR}
        top = list(tree.cursor.get_children())
        local = [c for c in tree.cursor.walk_preorder()
                 if c.kind == ci.CursorKind.VAR_DECL and c.storage_class == ci.StorageClass.EXTERN
                 and c.lexical_parent and c.lexical_parent.kind != ci.CursorKind.TRANSLATION_UNIT]
        for c in top + local:
            if c.kind != ci.CursorKind.VAR_DECL:
                continue
            if addr_of(c.spelling, syms) is None or c.spelling in PLATFORM:
                continue
            if c.storage_class == ci.StorageClass.EXTERN and c.spelling not in used:
                continue
            seen[abi].setdefault(c.spelling, []).append(c)
    recs = {}
    for abi, target in enumerate((N64, NATIVE)):
        for c in parse(tu, target).cursor.walk_preorder():
            if c.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL) and c.is_definition():
                f = c.location.file.name if c.location.file else ''
                if '/ports/tgrally/' not in f:
                    continue
                key = (os.path.relpath(f, ROOT), c.location.line, c.spelling or c.type.spelling)
                recs.setdefault(key, [None, None])[abi] = c.type.get_size()
    out['__records__'] = recs
    for name, cs0 in seen[0].items():
        cs1 = seen[1].get(name, cs0)

        def pick(cs):
            done = [c for c in cs if c.type.get_canonical().kind != K.INCOMPLETEARRAY]
            return (done or cs)[-1]
        c0, c1 = pick(cs0), pick(cs1)
        t0, t1 = c0.type.get_canonical(), c1.type.get_canonical()
        if any(c.storage_class == ci.StorageClass.STATIC for c in cs0):
            st = 'static'
        elif all(c.storage_class == ci.StorageClass.EXTERN for c in cs0):
            st = 'extern'
        else:
            st = ''
        complete = t0.kind != K.INCOMPLETEARRAY
        err = l0 = l1 = el = els = None
        if complete:
            try:
                l0, l1 = leaves(t0), leaves(t1)
            except Unlayable as e:
                err = str(e)
        else:
            el = c0.type.element_type.spelling
            els = t0.element_type.get_canonical().get_size()
            nels = t1.element_type.get_canonical().get_size()
            try:
                l0, l1 = leaves(t0.element_type), leaves(t1.element_type)
            except Unlayable as e:
                err = str(e)
        try:
            pth = paths(t0) if complete and t0.get_size() <= 0x10000 else None
        except Exception:
            pth = None
        tt = t0
        while tt.kind in (K.CONSTANTARRAY, K.INCOMPLETEARRAY):
            tt = tt.element_type.get_canonical()
        ptr = tt.kind in (K.POINTER, K.BLOCKPOINTER)
        # what a pointer here points at: an object in .data reached only
        # through it is lifted with this layout (tgr_lift)
        pl = psize = None
        if tt.kind == K.POINTER:
            pt = tt.get_pointee().get_canonical()
            if pt.kind not in (K.VOID, K.FUNCTIONPROTO, K.FUNCTIONNOPROTO) and pt.get_size() > 1:
                try:
                    pl = [(o, 4 if k == 'p' else z) for o, k, z in leaves(pt) if (4 if k == 'p' else z) > 1]
                    psize = pt.get_size()
                except Unlayable:
                    pl = None
        out[name] = dict(ptr=ptr, paths=pth, spelling=c0.type.spelling, st=st, complete=complete,
                         n64=t0.get_size() if complete else None, nat=t1.get_size() if complete else None,
                         nat_el=None if complete else t1.element_type.get_canonical().get_size(),
                         l0=l0, l1=l1, elem=el, elsize=els, natelsize=None if complete else nels, err=err,
                         pl=pl, psize=psize)
    return tu, out


def implemented():
    """{original address: function name} from the core's @implements tags."""
    out = {}
    for tu in tus():
        for m in re.finditer(r'@implements 0x([0-9A-Fa-f]+) tgr (\w+)', open(os.path.join(ROOT, tu)).read()):
            out[int(m.group(1), 16)] = m.group(2)
    return out


def declare(spelling, name, count=None):
    """A definition of name, the type spelled as clang spells it."""
    t = spelling
    if count is not None:
        t = t.replace('[]', '[%d]' % count, 1)
    if '(*' in t:
        return t.replace('(*', '(*' + name, 1) + ';'
    m = re.match(r'^(.*?)\s*((?:\[\d*\])+)$', t)
    if m:
        return '%s %s%s;' % (m.group(1), name, m.group(2))
    return '%s %s;' % (t, name)


def tag_of(tu):
    return re.sub(r'\W', '_', os.path.relpath(tu, 'ports/tgrally/src')[:-2])


def rel(path, frm):
    return os.path.relpath(os.path.join(ROOT, path), frm)


def main():
    syms = symbol_addresses()
    with ProcessPoolExecutor(14) as ex:
        per_tu = dict(ex.map(scan, tus()))

    report, layout = [], {}
    decl = {}
    for tu, rows in per_tu.items():
        for key, (n0, n1) in rows.pop('__records__').items():
            if n0 != n1:
                layout[key] = (n0, n1)
        for name, row in rows.items():
            row['tu'] = tu
            decl.setdefault(name, []).append(row)
    for (f, line, name), (n0, n1) in sorted(layout.items()):
        if 'zlib' in f or 'zinfl' in f:
            continue                    # zlib's own state: native, never in game memory
        report.append('%s:%d: %s is %s bytes natively, %s on the N64 (a native pointer in game memory)'
                      % (f, line, name, n1, n0))
    allv = sorted({addr_of(n, syms) for n in decl})

    def extent(a):
        i = bisect.bisect_right(allv, a)
        lim = TEXT_LO if a < TEXT_LO else BSS_LO if DATA_LO <= a < BSS_LO else \
            BSS_HI if a < BSS_HI else ARENA_HI
        nxt = allv[i] if i < len(allv) else lim
        return min(nxt, lim) - a

    arena, lift, native, allunits = [], [], [], []
    for name in sorted(decl, key=lambda n: addr_of(n, syms)):
        a = addr_of(name, syms)
        rows = decl[name]
        reg = region(a)
        if reg == 'code' and name in UCODE:
            arena.append((name, a))
            continue
        if reg == 'code':
            report.append('%s: a data name inside .text (%s)' % (
                name, ', '.join(sorted({os.path.basename(r['tu']) for r in rows}))))
            continue
        is_native = any(r['ptr'] for r in rows) and reg != 'arena'
        for r in rows:
            if r['st'] != 'extern':
                report.append('%s: defined in %s (game data lives in the arena: declare it extern)' % (
                    name, r['tu']))
            if r['nat'] is not None and r['nat'] != r['n64'] and not is_native:
                report.append('%s: %s is %s bytes natively, %s on the N64 (%s)' % (
                    name, r['spelling'], r['nat'], r['n64'], os.path.basename(r['tu'])))
        n64s = sorted({r['n64'] for r in rows if r['n64'] is not None})
        if any(r['ptr'] for r in rows) and reg != 'arena':
            # a pointer variable or a table of pointers: a native object (its
            # values native pointers), outside the arena
            if not all(r['ptr'] for r in rows):
                report.append('%s: a pointer in some files, not in others: %s' % (name, '; '.join(
                    '%s %s' % (os.path.basename(r['tu']), r['spelling']) for r in rows)))
            n0 = max(n64s) if n64s else 0
            if not any(r['complete'] for r in rows):
                n0 = extent(a)
            pu = {}
            for r in rows:
                for o, z in r['pl'] or []:
                    pu.setdefault(o, set()).add(z)
            punits = sorted((o, min(zs)) for o, zs in pu.items() if len(zs) == 1)
            psize = max([r['psize'] or 0 for r in rows] or [0])
            native.append((name, a, max(1, n0 // 4), reg == 'data', punits, psize))
            continue
        units = {}
        for r in rows:
            if r['l0'] and not re.search(r'char \[|char\[|be(16|32)_t|^char$', r['spelling']):
                for o, k, z in r['l0']:
                    units.setdefault(o, set()).add(4 if k == 'p' else z)
        clash = sorted(o for o, zs in units.items() if len(zs) > 1)
        if clash:
            report.append('%s: +0x%X read at different widths: %s' % (name, clash[0], '; '.join(
                '%s %s' % (os.path.basename(r['tu']), r['spelling']) for r in rows)))
        arena.append((name, a))
        if reg == 'arena':
            continue
        owner = max(rows, key=lambda r: (r['complete'], r['n64'] or 0, '*' in r['spelling'], -len(r['tu'])))
        ext = extent(a)
        if owner['err']:
            report.append('%s: cannot lay out %s: %s' % (name, owner['spelling'], owner['err']))
            continue
        lv = owner['l0'] or []
        n0 = owner['n64']
        if not owner['complete']:
            es = owner['elsize'] or 1
            if ext < 2 * es:
                report.append('%s: %s[] runs to the next name, %d bytes on: give it its size (%s)' % (
                    name, owner['elem'], ext, os.path.basename(owner['tu'])))
            cnt = max(1, ext // es)
            lv = [(i * es + o, k, z) for i in range(cnt) for o, k, z in lv]
            n0 = cnt * es
        elif n0 > ext:
            report.append('%s: %s is %d bytes, the next symbol %d bytes on (%s)' % (
                name, owner['spelling'], n0, ext, os.path.basename(owner['tu'])))
        units = [(o, 4 if k == 'p' else z) for o, k, z in lv if (4 if k == 'p' else z) > 1]
        # what other declarations know of bytes the owner leaves as padding
        # (one TU's view of a record often names fields another pads over)
        if owner['complete']:
            taken = bytearray(n0 or 0)
            for o, z in units:
                taken[o:o + z] = b'\x01' * z
            more = {}
            for r in rows:
                if r is owner or not r['complete'] or not r['l0'] or \
                        re.search(r'char \[|char\[|be(16|32)_t|^char$', r['spelling']):
                    continue
                for o, k, z in r['l0']:
                    z = 4 if k == 'p' else z
                    if z > 1 and o + z <= len(taken) and not any(taken[o:o + z]):
                        more.setdefault(o, set()).add(z)
            for o, zs in sorted(more.items()):
                z = zs.pop()
                if zs or any(taken[o:o + z]):
                    continue            # declarations disagree here: left to the report
                taken[o:o + z] = b'\x01' * z
                units.append((o, z))
            units.sort()
        allunits.append((name, a, n0, units))
        if reg == 'data':
            lift.append((a, n0, units))

    # a name inside another object (D_802723D4 in D_802723D0[3]) names
    # units its container already swaps: each byte is swapped once, by the
    # first (containing) symbol; a unit laid out differently is reported
    claimed = {}
    for i, (name, a, n0, units) in enumerate(allunits):
        keep = []
        for o, z in units:
            u = a + o
            have = [claimed.get(u + k) for k in range(z)]
            if not any(have):
                keep.append((o, z))
                for k in range(z):
                    claimed[u + k] = (u, z, name)
            elif have[0] is None or have[0][:2] != (u, z) or any(h != have[0] for h in have):
                report.append('%s: +0x%X (%d bytes) overlaps %s laid out differently' % (
                    name, o, z, next(h for h in have if h)[2]))
        allunits[i] = (name, a, n0, keep)
    lift = [(a, n0, units) for name, a, n0, units in allunits if region(a) == 'data']

    # names inside another object: what the source must spell as a member
    spans = sorted((a, n, nm) for nm in decl for (a, n) in
                   [(addr_of(nm, syms), max([r['n64'] or 0 for r in decl[nm]] or [0]))])
    starts = [x[0] for x in spans]
    aliases = []
    for name in decl:
        a = addr_of(name, syms)
        i = bisect.bisect_right(starts, a) - 1
        while i >= 0:
            sa, sz, sn = spans[i]
            if sn != name and sa < a < sa + sz:
                own = max(decl[sn], key=lambda r: r['n64'] or 0)
                d = a - sa
                pth = [p for o, z, p in (own['paths'] or []) if o <= d < o + z]
                aliases.append('%s %s %s +0x%X' % (name, sn, pth[0] if pth else '?', d))
                break
            if sa + 0x10000 < a:
                break
            i -= 1
    open(os.path.join(OUT, 'aliases.txt'), 'w').write('\n'.join(sorted(aliases)) + '\n')

    # a pointer variable must be its own object: one inside another object
    # is a member of it, in game memory, and holds an original address
    natset = {x[0] for x in native}
    for line in aliases:
        nm, owner = line.split()[:2]
        if nm in natset:
            report.append('%s: a pointer-typed member of %s (%s): declare it TgrAddr' % (nm, owner, line))

    # the lift: each .data symbol's multi-byte units, as strided runs
    os.makedirs(GEN, exist_ok=True)
    runlist, rows_out = [], []
    for a, n, units in lift:
        r0 = len(runlist)
        for o, z in units:
            if runlist and len(runlist) > r0:
                ro, rz, rc, rs = runlist[-1]
                if rz == z and (rc == 1 and o > ro or rc > 1 and o == ro + rc * rs):
                    runlist[-1] = (ro, rz, rc + 1, rs if rc > 1 else o - ro)
                    continue
            runlist.append((o, z, 1, 0))
        rows_out.append((a, n, r0, len(runlist) - r0))
    syms_h = 'ports/tgrally/platform/include/tgr_syms.h'
    lines = ['/* generated by ports/tgrally/tools/globals.py: the game\'s initialised data, */',
             '/* lifted from the ROM into the arena at start-up (platform/os/lift.c) */',
             '#include "%s"' % rel(syms_h, GEN), '', 'const TgrRun tgr_runs[] = {']
    lines += ['    {0x%X, %d, %d, %d},' % r for r in runlist]
    lines += ['    {0, 0, 0, 0}', '};', '', 'const TgrSym tgr_syms[] = {']
    lines += ['    {0x%08X, %d, %d, %d},' % r for r in rows_out]
    lines += ['    {0, 0, 0, 0}', '};', 'const int tgr_nsyms = %d;' % len(rows_out), '']
    lines += ['', '/* pointer variables and tables of pointers: native objects; the ROM\'s */',
              '/* original addresses in them become native pointers at start-up */']
    lines += ['void *%s[%d];' % (x[0], x[2]) for x in native]
    # the runs of what each one points at, after the symbols' runs
    nat_rows = []
    for nm, a, n, d, punits, psize in native:
        r0 = len(runlist)
        for o, z in punits:
            runlist.append((o, z, 1, 0))
        nat_rows.append((nm, a, n, d, r0, len(runlist) - r0))
    lines = lines[:lines.index('const TgrRun tgr_runs[] = {') + 1] + \
        ['    {0x%X, %d, %d, %d},' % r for r in runlist] + \
        lines[lines.index('    {0, 0, 0, 0}'):]
    lines += ['const TgrNat tgr_natives[] = {']
    lines += ['    {%s, 0x%08X, %d, %d, %d, %d},' % r for r in nat_rows]
    lines += ['    {0, 0, 0, 0, 0, 0}', '};', 'const int tgr_nnatives = %d;' % len(native), '']
    impl = implemented()
    fns = sorted(impl)
    lines += ['extern char %s[];' % impl[va] for va in fns]
    lines += ['const TgrFn tgr_fns[] = {'] + ['    {0x%08X, (void *)%s},' % (va, impl[va]) for va in fns] + \
             ['    {0, 0}', '};', 'const int tgr_nfns = %d;' % len(fns), '']
    open(os.path.join(GEN, 'tgr_syms.c'), 'w').write('\n'.join(lines))

    # C symbols carry a leading underscore on Mach-O and 32-bit Windows, not on x64 Windows or ELF
    us = '_' if any(x in NATIVE for x in ('apple', 'darwin')) or \
        (any(x in NATIVE for x in ('mingw', 'windows')) and not NATIVE.startswith('x86_64')) else ''
    lines = ['/* generated by ports/tgrally/tools/globals.py: the RDRAM arena (8 MB, the N64\'s RAM) */',
             '/* and every data symbol of the game, an alias at its original address in it */']
    if 'apple' in NATIVE or 'darwin' in NATIVE:
        lines += ['.globl _tgr_rdram', '.zerofill __DATA,__bss,_tgr_rdram,0x800000,12']
    elif 'mingw' in NATIVE or 'windows' in NATIVE:
        lines += ['.globl %stgr_rdram' % us, '.bss', '.p2align 12', '%stgr_rdram:' % us, '.space 0x800000']
    else:
        lines += ['.globl tgr_rdram', '.bss', '.p2align 12', '.type tgr_rdram, @object',
                  'tgr_rdram:', '.space 0x800000', '.size tgr_rdram, 0x800000']
    for name, a in arena:
        lines += ['.globl %s%s' % (us, name), '%s%s = %stgr_rdram + 0x%X' % (us, name, us, a - ARENA_LO)]
    open(os.path.join(GEN, 'arena.s'), 'w').write('\n'.join(lines) + '\n')

    # every .data/.bss symbol's multi-byte units, for tools/lockstep.py to read
    # the port's memory back as the original's bytes
    import json
    json.dump([{'name': n, 'addr': a, 'size': z, 'units': u} for n, a, z, u in allunits],
              open(os.path.join(OUT, 'symunits.json'), 'w'))
    json.dump([{'name': nm, 'addr': a, 'count': n, 'units': pu} for nm, a, n, d, pu, ps in native if d and pu],
              open(os.path.join(OUT, 'natunits.json'), 'w'))
    open(os.path.join(OUT, 'globals.txt'), 'w').write('\n'.join(report) + '\n')
    print('globals: %d symbols in the arena, %d native, %d lifted in %d runs, %d to resolve (%s)' % (
        len(arena), len(native), len(rows_out), len(runlist), len(report),
        os.path.relpath(os.path.join(OUT, 'globals.txt'), ROOT)))


if __name__ == '__main__':
    main()
