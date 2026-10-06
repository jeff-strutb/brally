#!/usr/bin/env python3
"""What stands between src/brally/core and a native 64-bit build, measured.

The portable core targets any 64-bit compiler: LLP64 (Windows x64, long is
32 bits) and LP64 (macOS, Linux, long is 64 bits). The matching source is
ILP32. This counts every place that difference bites, from the compiler's own
view of the tree rather than from text patterns where it can:

  layout   every struct/union/class the tree defines, laid out three ways
           (i686-pc-windows-msvc = the original, x86_64-pc-windows-msvc,
           x86_64-unknown-linux-gnu); a size change is a record whose
           pointer (or long) fields move everything after them.
  asserts  the tree's own offset/size static asserts that fail in the
           64-bit view: layout contracts the source states and 64-bit breaks.
  casts    pointer <-> 32-bit integer conversions in the LLP64 view, deduped
           by source line (clang -Wpointer-to-int-cast family).
  image    the original BRGlide.dll data the compiled game reads that no
           source definition supplies (the 32-bit lane maps the DLL's own
           sections at run time; a native build cannot), and the pointer
           slots inside it (the DLL's base relocations).
  text     idioms clang does not flag: image-range address literals,
           byte-offset pointer arithmetic, manual vtable indexing, inline asm.

Needs the 32-bit lane built once (build/brally/wasm32: its TU list, lax copies,
objects and symbol maps are the inputs).

Usage: .venv/bin/python ports/brally/tools/lp64audit.py [--jobs N]
Output: build/brally/analysis/lp64audit/{summary.txt,layout.csv,casts.csv,asserts.csv,image.csv}
"""
import argparse
import collections
import concurrent.futures
import csv
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
HERE = os.path.dirname(os.path.abspath(__file__))
LLVM = os.environ.get('BR_WASM_LLVM', '/opt/homebrew/opt/emscripten/libexec/llvm/bin')
WB = os.path.join(ROOT, 'build', 'brally', 'wasm32')
OUT = os.path.join(ROOT, 'build', 'brally', 'analysis', 'lp64audit')
T32, TW64, TL64 = 'i686-pc-windows-msvc', 'x86_64-pc-windows-msvc', 'x86_64-unknown-linux-gnu'

COMMON = ('-fms-extensions -fshort-wchar -fno-builtin -fno-strict-aliasing -fwrapv '
          '-D_M_IX86=500 -D_X86_ -D_WIN32 -DWIN32 -D_MSC_VER=1100 '
          '-D_INTEGRAL_MAX_BITS=64 -DBR_MATCHING_BUILD '
          '-Isrc/brally/include -I%s/inc -Iports/brally-wasm/wasm/inc -Itools/toolchains/msvc5-compat '
          '-include ports/brally-wasm/wasm/inc/msvc_intrinsics.h -Itools/toolchains/msvc5/include' % WB).split()
# 64-bit views: size_t, ptrdiff_t and intptr_t from the compiler, not MSVC5's
W64 = ['-I' + os.path.join(HERE, 'lp64audit_inc'),
       '-include', os.path.join(HERE, 'lp64audit_inc', 'lp64pre.h')]
CASTW = ['-Wpointer-to-int-cast', '-Wint-to-pointer-cast', '-Wvoid-pointer-to-int-cast',
         '-Wint-conversion', '-Wshorten-64-to-32', '-Wno-error']
CAST_KINDS = ('pointer-to-int-cast', 'int-to-pointer-cast', 'int-to-void-pointer-cast',
              'void-pointer-to-int-cast', 'int-conversion', 'shorten-64-to-32')


def tu_job(src):
    n = src.replace('/', '__')
    lax = os.path.join(WB, 'lax', n + '.i')
    cpp = src.endswith('.cpp')
    if os.path.exists(lax):
        inp = lax
        x = ['-x', 'c++-cpp-output' if cpp else 'cpp-output']
    else:
        inp = src
        x = ['-x', 'c++' if cpp else 'c']
    x += ['-std=c++98', '-fno-exceptions', '-fno-rtti'] if cpp else ['-std=gnu89']
    res = {'src': src, 'lax': inp != src}
    for t in (T32, TW64, TL64):
        extra = W64 if t != T32 else []
        p = subprocess.run([LLVM + '/clang', '--target=' + t] + extra + COMMON +
                           ['-w'] + x + ['-fsyntax-only', '-Xclang',
                                         '-fdump-record-layouts-complete', inp],
                           capture_output=True, text=True, errors='replace', cwd=ROOT)
        res[t] = p.stdout
    p = subprocess.run([LLVM + '/clang', '--target=' + TW64] + W64 + COMMON + x +
                       ['-fsyntax-only', '-ferror-limit=0'] + CASTW + [inp],
                       capture_output=True, text=True, errors='replace', cwd=ROOT)
    res['diag'] = p.stderr
    return res


def parse_layouts(txt):
    out = {}
    for blk in txt.split('*** Dumping AST Record Layout')[1:]:
        lines = blk.strip('\n').split('\n')
        m = re.match(r'\s*\d+ \| (.*)$', lines[0])
        sz = re.search(r'\[sizeof=(\d+)', blk)
        if m and sz:
            out[m.group(1).strip()] = int(sz.group(1))
    return out


def tree_tags():
    tags = set()
    for root in ('src/brally/include', 'src/brally'):
        for p in glob.glob(os.path.join(ROOT, root, '**', '*'), recursive=True):
            if p.endswith(('.h', '.c', '.cpp')):
                s = open(p, errors='replace').read()
                for m in re.finditer(r'\b(?:struct|union|class)\s+([A-Za-z_]\w*)\s*(?::[^{;]*)?\{', s):
                    tags.add(m.group(1))
    return tags


def ours(name, tags):
    m = re.match(r'(?:struct|union|class) (.*)$', name)
    if not m:
        return False
    n = m.group(1)
    if n.startswith(('(unnamed', '(anonymous')):
        # a lax TU is preprocessed: its unnamed records cannot be told from
        # the SDK headers', so only unnamed records the tree names count
        return ' at src/brally/include/' in n or ' at src/brally/' in n
    return n in tags


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), s, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    return re.sub(r'"(?:\\.|[^"\\\n])*"', '""', s)


TEXT_PATTERNS = [
    ('image-range address literal', r'\b0x1(?:0[0-9A-Fa-f]|1[0-8])[0-9A-Fa-f]{5}\b'),
    ('image address cast to a pointer', r'\(\s*[A-Za-z_][\w ]*\*+\s*\)\s*\(?\s*0x1(?:0[0-9A-Fa-f]|1[0-8])[0-9A-Fa-f]{5}'),
    ('byte-offset pointer arithmetic', r'\(\s*(?:const\s+)?(?:unsigned\s+)?(?:char|BYTE|u8|uint8_t|UCHAR)\s*\*\s*\)\s*\(?[\w>.\-\[\]]+\)?\s*\+\s*(?:0x[0-9A-Fa-f]+|\d+)\b'),
    ('manual vtable / object word indexing', r'\(\s*\*\s*\(\s*(?:void|int|DWORD|unsigned)\s*\*\*\*?\s*\)'),
    ('inline asm', r'\b__asm\b|\b_asm\b'),
    ('file read call', r'\b(?:fread|ReadFile|_read|_lread|mmioRead)\s*\('),
    ('file write call', r'\b(?:fwrite|WriteFile|_write|_lwrite)\s*\('),
]


def image_census():
    """Original image data the compiled game reads, by provenance."""
    sys.path.insert(0, os.path.join(ROOT, 'ports', 'brally-wasm', 'wasm'))
    sys.path.insert(0, os.path.join(ROOT, 'tools', 'brally'))
    import w2c
    import pe as pemod
    P = pemod.load(os.path.join(ROOT, 'reference', 'brally', 'orig', 'BRGlide.dll'))
    IB = P.image_base
    text = next(s for s in P.sections if s.name == '.text')
    tlo, thi = IB + text.vaddr, IB + text.vaddr + text.vsize
    slots = []
    for rva in sorted(P.relocs):
        s = P.sect_for_rva(rva)
        if s is None or s.name == '.text':
            continue
        tgt = P.u32(IB + rva)
        slots.append((IB + rva, s.name, 'function' if tgt is not None and tlo <= tgt < thi else 'data'))

    od = os.path.join(ROOT, 'build', 'brally', 'win32', 'match', 'orig')
    for fn in os.listdir(od):
        m = re.match(r'0x([0-9A-Fa-f]{8})\.bin$', fn)
        if m:
            w2c.FUNC_STARTS.add(int(m.group(1), 16))
    w2c.load_thunks(os.path.join(ROOT, 'reference', 'brally', 'orig', 'BRGlide.dll'))
    norm = lambda n: re.sub(r'\$S\d+$', '', n)
    symmap = [(r['scope'], norm(r['name']), int(r['va'], 16))
              for r in csv.DictReader(open(os.path.join(WB, 'symmap.csv')))]
    owners = {r['name']: r['base'] for r in csv.DictReader(open(os.path.join(WB, 'owners.csv')))}
    objs, srcof, srcpath = [], {}, {}
    for i, p in enumerate(sorted(glob.glob(os.path.join(WB, 'obj', '*.o')))):
        objs.append(w2c.Obj(p, i))
        srcof[i] = re.sub(r'\.(c|cpp)\.o$', '', os.path.basename(p)).split('__')[-1]
        srcpath[i] = os.path.basename(p)[:-2].replace('__', '/')
    sites = []
    for r in csv.DictReader(open(os.path.join(WB, 'sites.csv'))):
        va = int(r['va'], 16)
        if ((w2c.TEXT_LO <= va < w2c.TEXT_HI and (va in w2c.FUNC_STARTS or va in w2c.THUNKS))
                or w2c.DATA_LO <= va < 0x118F2000):
            sites.append((r['src'], norm(r['name']), int(r['addend']), va))
    placement = [(int(r['va'], 16), r['name'], r['src'])
                 for r in csv.DictReader(open(os.path.join(WB, 'placement.csv')))]
    for r in csv.DictReader(open(os.path.join(WB, 'decls.csv'))):
        sites.append(('decl:' + r['src'], r['name'], 0, int(r['va'], 16)))
    L = w2c.Linker(objs, symmap, owners, srcof, sites, placement)
    L.srcpath = srcpath
    L.run()
    port = set(L.port_segs)
    own = set()
    for o in objs:
        for si, (po, data) in enumerate(o.data):
            addr = L.seg_addr[(o.oid, si)]
            if (o.oid, si, addr) not in port:
                own.update(range(addr, addr + max(len(data), 1)))
    refs = collections.defaultdict(set)
    for o in objs:
        for ty, off, idx, add in o.relocs.get(o.code_sec, []):
            if ty not in (3, 4, 5, 11, 14) or o.syms[idx]['kind'] != 1:
                continue
            va = L.data_addr(o, idx, 0)
            if IB <= va < 0x11900000:
                refs[va].add(srcpath[o.oid])
    init = {}
    for sname in ('.rdata', '.data'):
        s = next((x for x in P.sections if x.name == sname), None)
        if s:
            lo, n = IB + s.vaddr, min(s.raw_size, s.vsize)
            body = P.data[s.raw_ptr:s.raw_ptr + n]
            nz = [lo + i for i, b in enumerate(body) if b]
            init[sname] = (len(nz), sum(1 for a in nz if a in own))
    return {
        'refs': refs, 'own': own, 'slots': slots, 'init': init,
        'sec': lambda va: (P.sect_for_rva(va - IB).name if P.sect_for_rva(va - IB) else '?'),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--jobs', type=int, default=os.cpu_count())
    a = ap.parse_args()
    os.chdir(ROOT)
    tus = [l.strip() for l in open(os.path.join(WB, 'tus.txt')) if l.strip()]
    if not tus:
        sys.exit('lp64audit: build the 32-bit lane first (ports/brally-wasm/wasm/build_wasm.sh)')
    os.makedirs(OUT, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        results = list(ex.map(tu_job, tus))

    tags = tree_tags()
    recs, where = {}, collections.defaultdict(set)
    casts = collections.defaultdict(set)     # (file, line, kind) -> TUs
    asserts, failed_tus = set(), 0
    for r in results:
        per = {t: parse_layouts(r[t]) for t in (T32, TW64, TL64)}
        for name, sz in per[T32].items():
            if ours(name, tags):
                recs.setdefault(name, (sz, per[TW64].get(name), per[TL64].get(name)))
                where[name].add(r['src'])
        diag = r['diag']
        for m in re.finditer(r'^(.+?):(\d+):\d+: (?:warning|error): .*\[-W(%s)\]$' % '|'.join(CAST_KINDS), diag, re.M):
            f = r['src'] if '/lax/' in m.group(1) else m.group(1)
            line = int(m.group(2)) if '/lax/' not in m.group(1) else -int(m.group(2))
            casts[(f, line, m.group(3))].add(r['src'])
        for m in re.finditer(r'^(.+?):(\d+):\d+: error: .*(?:array size is negative|negative size)', diag, re.M):
            f = r['src'] if '/lax/' in m.group(1) else m.group(1)
            asserts.add((f, int(m.group(2))))
        if re.search(r'fatal error|too many errors', diag):
            failed_tus += 1

    # text idioms
    files = [p for d in ('src/brally',) for p in glob.glob(d + '/**/*', recursive=True)
             if p.endswith(('.c', '.cpp', '.h'))]
    text = collections.Counter()
    text_files = collections.defaultdict(set)
    total_asserts = 0
    for p in files:
        s = strip_comments(open(p, errors='replace').read())
        total_asserts += len(re.findall(r'typedef\s+char\s+\w+\s*\[', s))
        for k, rx in TEXT_PATTERNS:
            n = len(re.findall(rx, s))
            if n:
                text[k] += n
                text_files[k].add(p)

    img = image_census()

    # ---- write
    with open(os.path.join(OUT, 'layout.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['record', 'i686', 'llp64', 'lp64', 'tus'])
        for k in sorted(recs):
            w.writerow([k, *[('' if v is None else v) for v in recs[k]], len(where[k])])
    with open(os.path.join(OUT, 'casts.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['file', 'line', 'kind'])
        for (fl, ln, k) in sorted(casts):
            w.writerow([fl, ln if ln > 0 else 'preprocessed:%d' % -ln, k])
    with open(os.path.join(OUT, 'asserts.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['file', 'line'])
        for fl, ln in sorted(asserts):
            w.writerow([fl, ln])
    with open(os.path.join(OUT, 'image.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['va', 'section', 'source_defined', 'files'])
        for va in sorted(img['refs']):
            w.writerow(['0x%08X' % va, img['sec'](va), int(va in img['own']),
                        ';'.join(sorted(img['refs'][va]))])

    ch64 = [k for k, v in recs.items() if v[1] is not None and v[1] != v[0]]
    chl = [k for k, v in recs.items() if v[1] is not None and v[2] is not None and v[2] != v[1]]
    kinds = collections.Counter(k for (_, _, k) in casts)
    cfiles = {fl for (fl, _, _) in casts}
    refs, own = img['refs'], img['own']
    img_only = [v for v in refs if v not in own]
    slotc = collections.Counter((sec, kind, 'source' if va in own else 'image-only')
                                for va, sec, kind in img['slots'])
    L = []
    L.append('TUs audited: %d of %d (%d hit the error limit)' % (len(results), len(tus), failed_tus))
    L.append('')
    L.append('layout: records the tree defines: %d' % len(recs))
    L.append('  size changes with 64-bit pointers (LLP64): %d (C++ class views %d)' % (
        len(ch64), sum(1 for k in ch64 if k.startswith('class '))))
    L.append('  further size changes with 64-bit long (LP64): %d  %s' % (len(chl), ', '.join(sorted(chl))))
    L.append('asserts: offset/size static asserts failing in the 64-bit view: %d of %d in the tree' % (
        len(asserts), total_asserts))
    L.append('casts: pointer/32-bit-integer conversion sites: %d in %d files' % (len(casts), len(cfiles)))
    for k in CAST_KINDS:
        L.append('  %-28s %d' % (k, kinds[k]))
    L.append('image: original data addresses the game reads: %d; with a source definition %d; image-only %d' % (
        len(refs), len(refs) - len(img_only), len(img_only)))
    for sname, (nz, nzown) in img['init'].items():
        L.append('  %s nonzero initialized bytes: %d, supplied by source: %d' % (sname, nz, nzown))
    L.append('  pointer slots in initialized data (base relocs): %d' % len(img['slots']))
    for k in sorted(slotc):
        L.append('    %-6s -> %-8s %-10s %d' % (k[0], k[1], k[2], slotc[k]))
    L.append('text idioms (comments stripped):')
    for k, _ in TEXT_PATTERNS:
        L.append('  %-38s %5d sites in %3d files' % (k, text[k], len(text_files[k])))
    L.append('  inline asm files: %s' % ', '.join(sorted(text_files['inline asm'])))
    s = '\n'.join(L) + '\n'
    open(os.path.join(OUT, 'summary.txt'), 'w').write(s)
    print(s, end='')


if __name__ == '__main__':
    main()
