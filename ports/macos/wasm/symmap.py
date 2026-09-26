#!/usr/bin/env python3
"""Every symbol the game's compiled code references -> its ORIGINAL address.

The macOS port runs the Windows-build source against the original image's
data laid out at its original addresses, so each global the code names --
extern or file-static, under whatever name a TU gave it -- must resolve to the
one address the shipped game used. The matching pipeline already answers this
per relocation slot; this collects those answers into one table.

For every MSVC object the image builder would place (tools/image_build.py's
C lane objects and the C++ lane's 4/4 claims), for every function with a VA,
for every DIR32/REL32 slot:
  1. the tree's maps (report.csv, globals_glide.csv, globals_learned.csv) --
     tools/reloc_fill.resolve, exactly as the image builder resolves it;
  2. else, when the body matches the original outside its slots (the
     masked-match property the image's ref-fill relies on), the original
     dword at the same offset, minus the object's addend.

Output (build/wasm/symmap.csv): scope,name,va,how
  scope = source basename for file-local answers, '*' for global names.
A name answered two different ways in one scope is dropped and reported.
"""
import collections
import csv
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from relocmap import REL_DIR32, REL_REL32, load_maps      # noqa: E402
from reloc_fill import parse, resolve                     # noqa: E402
from reloc_learn import live_objs                         # noqa: E402

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')


def undecorate(raw):
    """MSVC COFF symbol -> the C-level name the wasm object will carry."""
    if raw.startswith('?'):
        # ?name@@3... (C++ global data)  /  ?Meth@Class@@... (member)
        m = re.match(r'\?([A-Za-z_$][\w$]*)@(?:([A-Za-z_]\w*)@)?@', raw)
        if not m:
            return raw
        return m.group(2) + '::' + m.group(1) if m.group(2) else m.group(1)
    s = raw
    if s.startswith('@'):
        s = s[1:]
    elif s.startswith('_'):
        s = s[1:]
    return s.split('@')[0]


def reloc_sites(code, relocs, sym_val, n):
    out = []
    for rva, si, rt in relocs:
        off = rva - sym_val
        if 0 <= off <= n - 4 and rt in (REL_DIR32, REL_REL32):
            out.append((off, si, rt))
    return out


def masked_equal(code, orig, sites):
    a, b = bytearray(code), bytearray(orig)
    for off, _, _ in sites:
        a[off:off + 4] = b'\0\0\0\0'
        b[off:off + 4] = b'\0\0\0\0'
    return a == b


def functions_in(path, fnmap, only=None):
    d, secs, syms, relocs = parse(path)
    byidx = {s['idx']: s for s in syms}
    for sy in syms:
        sec = secs.get(sy['sec'])
        if sy['sec'] <= 0 or not sec or not sec['name'].startswith('.text'):
            continue
        if only is not None:
            if sy['name'] not in only:
                continue
            va = only[sy['name']]
        else:
            nm = sy['name'].lstrip('_@').split('@')[0]
            if nm not in fnmap:
                continue
            va = fnmap[nm]
        ob = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
        if not os.path.exists(ob):
            continue
        orig = open(ob, 'rb').read()
        start = sec['praw'] + sy['val']
        n = min(len(orig), sec['size'] - sy['val'])
        code = d[start:start + n]
        sites = reloc_sites(code, relocs[sy['sec']], sy['val'], n)
        yield va, code, orig[:n], sites, byidx


def main():
    fnmap, glmap = load_maps()
    objs, _ = live_objs()
    claims = []
    for p in objs:
        claims.append((p, None))
    try:
        import image_build
        for obj, only, _nm in image_build.cpp_claims():
            claims.append((obj, only))
    except Exception as e:                      # C++ lane optional
        print('symmap: C++ lane skipped (%s)' % e, file=sys.stderr)

    seen = collections.defaultdict(lambda: collections.defaultdict(set))
    sites_seen = collections.defaultdict(set)
    for path, only in claims:
        base = os.path.splitext(os.path.basename(path))[0]
        base = re.sub(r'_sweep_[0-9A-F]{8}_\d+$', '', base)
        try:
            fns = list(functions_in(path, fnmap, only))
        except Exception as e:
            print('symmap: %s: %s' % (os.path.basename(path), e),
                  file=sys.stderr)
            continue
        for va, code, orig, sites, byidx in fns:
            exact = masked_equal(code, orig, sites)
            for off, si, rt in sites:
                t = byidx.get(si)
                if not t or t['name'].lstrip('_').startswith('$'):
                    continue
                name = undecorate(t['name'])
                if name.startswith('??_C@') or name.startswith('__real@'):
                    continue            # literals: the port keeps its own
                addend = struct.unpack_from('<i', code, off)[0]
                a = resolve(t['name'], fnmap, glmap)
                how = 'map'
                if a is None:
                    if not exact:
                        continue
                    dw = struct.unpack_from('<I', orig, off)[0]
                    if rt != REL_DIR32:
                        dw = (dw + va + off + 4) & 0xFFFFFFFF
                    # The EXACT target of this site. The source sometimes
                    # models scattered original globals as one C struct
                    # (br_menucb.c's g_menu), so symbol+addend is the key;
                    # a per-symbol base is only derived where every site
                    # agrees on one.
                    sites_seen[(base, name, addend)].add(dw)
                    a = (dw - addend) & 0xFFFFFFFF
                    how = 'ref'
                # statics are per file; a mapped name is global
                scope = '*' if how == 'map' else base
                seen[(scope, name)][a].add(how)

    rows, clash = [], 0
    for (scope, name), addrs in sorted(seen.items()):
        if len(addrs) != 1:
            clash += 1
            print('symmap: %s:%s -> %s (per-site only)' % (
                scope, name, ' '.join('%#x' % a for a in sorted(addrs))),
                file=sys.stderr)
            continue
        (a, hows), = addrs.items()
        rows.append((scope, name, '0x%08X' % a, '+'.join(sorted(hows))))
    # plain global names from the maps too, for code no placed function covers
    have = {(s, n) for s, n, _, _ in rows}
    # and the tree's declaration comments -- `extern T name; /* 0x<VA> */` and
    # `T name(...); /* 0x<VA> */` -- exactly as the T3 build's per-object
    # resolution (tools/t3b_env.augment_maps) reads them
    try:
        import t3b_env
        import pe
        img = pe.load(os.path.join(ROOT, 'orig', 'BRGlide.dll'))
        imports = set(re.sub(r'^_|@\d+$', '', v.split('!')[1]) for v in img.imports.values())
        starts = set(int(f[2:10], 16) for f in os.listdir(ORIG_DIR)
                     if re.match(r'0x[0-9A-Fa-f]{8}\.bin$', f))
        for tab, isfn in ((t3b_env._declared_data_va(), False), (t3b_env._declared_va(), True)):
            for n, a in tab.items():
                if ('*', n) in have or n in fnmap or n in glmap or n in imports:
                    continue
                # a function answer must be a function START: the comment
                # form also matches call statements annotated with the
                # call's own address
                if isfn and a not in starts:
                    continue
                rows.append(('*', n, '0x%08X' % a, 'decl'))
                have.add(('*', n))
    except Exception as e:
        print('symmap: declaration maps skipped (%s)' % e, file=sys.stderr)
    for src in (fnmap, glmap):
        for n, a in src.items():
            if ('*', n) not in have:
                rows.append(('*', n, '0x%08X' % a, 'map'))
                have.add(('*', n))
    os.makedirs(os.path.join(ROOT, 'build', 'wasm'), exist_ok=True)
    # Per-file address annotations on data declarations AND definitions --
    # `char g_DeviceGuid[16];  /* 0x10078708 */` -- the tree's own record of
    # where a global lives, for code the verified build does not place (T1/T2
    # functions, whose original bytes stand in for them in the Windows image).
    ann = re.compile(r'^[ \t]*(?:extern[ \t]+(?:"C"[ \t]+)?)?(?:(?:const|volatile|static|unsigned|signed|struct|enum)[ \t]+)*'
                     r'[A-Za-z_][\w]*[ \t\*]+(?:\*[ \t]*)*([A-Za-z_]\w*)[ \t]*(?:\[[^\]]*\][ \t]*)*'
                     r'(?:=[^;]*)?;[ \t]*/[*/][ \t]*0x([0-9A-Fa-f]{8})\b([^\n]*)', re.M)
    n_ann = 0
    with open(os.path.join(ROOT, 'build', 'wasm', 'decls.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['src', 'name', 'va'])
        for dp, _, fs in os.walk(os.path.join(ROOT, 'src', 'core')):
            for fn in sorted(fs):
                if not fn.endswith(('.c', '.cpp')):
                    continue
                path = os.path.join(dp, fn)
                rel = os.path.relpath(path, ROOT)
                text = open(path, encoding='latin-1').read()
                for m in ann.finditer(text):
                    # `/* 0x10680598 / 0x105BC740 */` names a D3D and a
                    # Glide address: ambiguous, so not an answer
                    if re.search(r'0x[0-9A-Fa-f]{6,8}', m.group(3)):
                        continue
                    a = int(m.group(2), 16)
                    if 0x10077000 <= a < 0x118F0000:
                        w.writerow([rel, m.group(1), '0x%08X' % a])
                        n_ann += 1
    print('symmap: %d annotated data declarations' % n_ann)
    with open(os.path.join(ROOT, 'build', 'wasm', 'symsites.csv'), 'w',
              newline='') as f:
        w = csv.writer(f)
        w.writerow(['scope', 'name', 'addend', 'va'])
        bad = 0
        for (scope, name, add), vs in sorted(sites_seen.items()):
            if len(vs) != 1:
                bad += 1
                continue
            w.writerow([scope, name, add, '0x%08X' % next(iter(vs))])
    print('symmap: %d symbol+offset sites (%d ambiguous)' % (
        len(sites_seen) - bad, bad))
    out = os.path.join(ROOT, 'build', 'wasm', 'symmap.csv')
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['scope', 'name', 'va', 'how'])
        w.writerows(sorted(rows))
    print('symmap: %d symbols (%d scoped), %d clashes dropped -> %s'
          % (len(rows), sum(1 for r in rows if r[0] != '*'), clash, out))


if __name__ == '__main__':
    main()
