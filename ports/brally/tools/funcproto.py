#!/usr/bin/env python3
"""The true prototype of every original function, in one header.

A function's definition is the truth. This reads every core definition of a
function the verified build placed (build/wasm/placement.csv) from clang's
syntax tree and writes its prototype -- types spelled through struct tags so
the header needs only forward declarations -- to
ports/brally/include/br_funcs.h, which every core file includes. Prototypes
written for alias names (names funcs.py maps to the real function) are then
removed everywhere: each call is checked against the definition.

Usage: funcproto.py [--no-purge]
"""
import collections
import concurrent.futures
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402

ROOT = rawscan.ROOT
OUT = os.path.join(ROOT, 'ports/brally/include/br_funcs.h')


def defs_in(f):
    a = rawscan.ast(f)
    if a is None:
        return []
    out = []
    fabs = os.path.abspath(os.path.join(ROOT, f))
    # this file's own typedefs: a prototype in the shared header spells
    # them out
    tdefs = {}
    for n in a.get('inner', []):
        if n.get('kind') == 'TypedefDecl' and not n.get('isImplicit'):
            loc = n.get('loc', {})
            fl = loc.get('file') or loc.get('spellingLoc', {}).get('file') or loc.get('includedFrom', {}).get('file')
            t = n.get('type', {})
            tdefs[n.get('name')] = (t.get('desugaredQualType') or t.get('qualType'), fl)

    def expand(t):
        for _ in range(8):
            def rep(m):
                w = m.group(0)
                d = tdefs.get(w)
                if d is None or re.search(r'\b(struct|union|enum)\s+$', t[:m.start()]):
                    return w
                u = d[0]
                if '(unnamed' in u or '(anonymous' in u:
                    return w
                return u if not re.search(r'[()\[]', u) else w
            n2 = re.sub(r'\b[A-Za-z_]\w*\b', rep, t)
            if n2 == t:
                break
            t = n2
        return t

    def visit(nodes):
        for n in nodes:
            k = n.get('kind')
            if k == 'LinkageSpecDecl':
                visit(n.get('inner', []) or [])
                continue
            if k != 'FunctionDecl' or 'inner' not in n or n.get('storageClass') == 'static':
                continue
            if not any(c.get('kind') == 'CompoundStmt' for c in n['inner']):
                continue
            loc = n.get('loc', {})
            fl = loc.get('file') or loc.get('spellingLoc', {}).get('file')
            if fl and os.path.abspath(os.path.join(ROOT, fl)) != fabs:
                continue
            t = n.get('type', {})
            qt = t.get('desugaredQualType') or t.get('qualType')
            params = [p for p in n['inner'] if p.get('kind') == 'ParmVarDecl']
            ptypes = [(p.get('type', {}).get('desugaredQualType') or p.get('type', {}).get('qualType'))
                      for p in params]
            ret = re.match(r'^(.*?)\s*\(', qt)
            ptypes = [expand(t) for t in ptypes]
            out.append((n.get('name'), expand(ret.group(1).strip()) if ret else 'int', ptypes,
                        n.get('variadic', False) or '...' in qt, f))
    visit(a.get('inner', []))
    return out


def tags_of(t):
    return set(re.findall(r'\b(?:struct|union|enum)\s+(\w+)', t))


def main():
    os.chdir(ROOT)
    rows = list(csv.DictReader(open('build/wasm/placement.csv')))
    place = {r['name'] for r in rows}
    home = {r['name']: os.path.join('ports/brally', r['src']) for r in rows}
    if os.path.exists('build/portable/method_fwd.csv'):
        place |= {r['forwarder'] for r in csv.DictReader(open('build/portable/method_fwd.csv'))}
    files = sorted(os.path.relpath(os.path.join(dp, fn), ROOT)
                   for dp, _, fns in os.walk('ports/brally/src/core') for fn in fns if fn.endswith(('.c', '.cpp')))
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as ex:
        all_defs = [d for ds in ex.map(defs_in, files) for d in ds]
    import glob as _g
    rec_names, typedefs = set(), set()
    platform_names = {'FILE', 'va_list', 'time_t'}
    tag_of = {}
    for p in _g.glob('ports/brally/src/**/*', recursive=True) + _g.glob('ports/brally/include/*.h') + \
            _g.glob('ports/brally/platform/include/*.h'):
        if not p.endswith(('.c', '.cpp', '.h')):
            continue
        t = open(p, encoding='latin-1').read()
        rec_names |= set(re.findall(r'\b(?:struct|class|union)\s+(\w+)\s*(?::[^{;]*)?\{', t))
        if '/platform/' in p:
            platform_names.update(re.findall(r'\}\s*(\w+)\s*[,;]', t))
            platform_names.update(re.findall(r'[,]\s*\**\s*(\w+)\s*[,;]', t))
            platform_names.update(re.findall(r'typedef\s+[^;{]*?\b(\w+)\s*[;,]', t))
            platform_names.update(re.findall(r'typedef[^;{]*?\(\s*(?:\w+\s+)?\*\s*(\w+)\s*\)', t))
            platform_names.update(re.findall(r'BR_DECLARE_HANDLE\((\w+)\)', t))
        typedefs |= set(re.findall(r'typedef\s+[^;{]*?\b(\w+)\s*[;,]', t))
        typedefs |= set(re.findall(r'\}\s*(\w+)\s*;', t))
        for m in re.finditer(r'typedef\s+(?:struct|union|class)\s+(\w+)\s*(?:\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\})?\s*(\w+)\s*;', t):
            tag_of.setdefault(m.group(2), m.group(1))
        for m in re.finditer(r'typedef\s+(?:struct|union)\s*\{', t):
            pass
    BUILTIN = {'void', 'char', 'short', 'int', 'long', 'float', 'double', 'signed', 'unsigned', 'const',
               'volatile', 'struct', 'union', 'enum', 'int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t',
               'uint32_t', 'int64_t', 'uint64_t', 'size_t', 'intptr_t', 'uintptr_t', '_Bool', 'bool', 'wchar_t'}

    def fix_type(t):
        def word(m):
            w = m.group(0)
            if w in BUILTIN or w in platform_names:
                return w
            pre = t[:m.start()].rstrip()
            if pre.endswith(('struct', 'union', 'enum', 'class')):
                return w
            if w in tag_of:
                return 'struct ' + tag_of[w]
            if w in rec_names and w not in typedefs:
                return 'struct ' + w
            return w
        t = re.sub(r'\bclass\b', 'struct', t)
        return re.sub(r'\b[A-Za-z_]\w*\b', word, t)

    known_c = typedefs | rec_names | BUILTIN
    protos, tags, skipped, why = {}, set(), [], {}
    # the placed file's definition first: a twin elsewhere never names the type
    all_defs.sort(key=lambda d: d[4] != home.get(d[0], d[4]))
    for name, ret, ptypes, var, f in all_defs:
        if name not in place or name in protos or '::' in name:
            continue
        ret = fix_type(ret)
        ptypes = [fix_type(t) for t in ptypes]

        def unknown_to_void(t):
            return t        # never approximate: a function whose types cannot be named is skipped
        ret = unknown_to_void(ret)
        ptypes = [unknown_to_void(t) for t in ptypes]
        left = [w for t in [ret] + ptypes
                for w in re.findall(r'(?<!struct )(?<!union )(?<!enum )\b([A-Za-z_]\w*)\b', t)
                if w not in BUILTIN and w not in platform_names and w not in ('bool',)
                and not re.search(r'\b(struct|union|enum)\s+%s\b' % w, t)]
        if left:
            skipped.append(name)
            why[name] = 'names ' + ' '.join(sorted(set(left)))
            continue
        types = [ret] + ptypes
        if any('(unnamed' in t or '(anonymous' in t for t in types):
            skipped.append(name)
            why[name] = 'unnamed type'
            continue
        # by-value struct parameters need the full type: leave those to
        # their own headers
        if any(re.match(r'^(const\s+)?(struct|union)\s+\w+$', t.strip()) for t in types):
            skipped.append(name)
            why[name] = 'by-value struct'
            continue
        for t in types:
            tags |= tags_of(t)
        args = ', '.join(ptypes) if ptypes else 'void'
        if var:
            args += ', ...'
        protos[name] = '%s %s(%s);' % (ret, name, args)
    with open(OUT, 'w') as fh:
        fh.write('/* br_funcs.h: the true prototype of every original function, from its\n'
                 ' * definition (generated by ports/brally/tools/funcproto.py). */\n'
                 '#ifndef BR_FUNCS_H\n#define BR_FUNCS_H\n#include <stdint.h>\n#include <stddef.h>\n#include <stdio.h>\n#ifndef __cplusplus\n#include <stdbool.h>\n#endif\n')
        for t in sorted(tags):
            fh.write('struct %s;\n' % t)
        fh.write('#ifdef __cplusplus\nextern "C" {\n#endif\n')
        for name in sorted(protos):
            # an alias macro must not rename the prototype itself
            fh.write('#pragma push_macro("%s")\n#undef %s\n%s\n#pragma pop_macro("%s")\n' % (
                name, name, protos[name], name))
        fh.write('#ifdef __cplusplus\n}\n#endif\n#endif\n')
    # every other file-scope prototype of these names goes: br_funcs.h is the
    # one declaration (a stale header prototype conflicts with the definition)
    import stubaudit
    purged = 0
    names_rx = re.compile(r'^[ \t]*(?:extern\s+(?:"C"\s+)?)?[A-Za-z_][\w \t\*]*?[\s\*](%s)\s*\([^;{}()]*(?:\([^;{}()]*\)[^;{}()]*)*\)\s*;'
                          % '|'.join(map(re.escape, sorted(protos, key=len, reverse=True))), re.M)
    for p in _g.glob('ports/brally/include/*.h') + files:
        if os.path.basename(p) in ('br_funcs.h', 'br_globals.h'):
            continue
        t = open(p, encoding='latin-1').read()
        edits = []
        for m in names_rx.finditer(t):
            enc = stubaudit.enclosing(t, m.start())
            if enc is not None and not re.search(r'extern\s+"C"\s*$', enc.rstrip()):
                continue        # inside a function or a class: not a file-scope prototype
            if re.match(r'\s*(return|else|case)\b', m.group(0)):
                continue
            edits.append((m.start(), m.end(), '/* %s: prototype in br_funcs.h */' % m.group(1)))
        for a0, z, txt in reversed(edits):
            t = t[:a0] + txt + t[z:]
        if edits:
            open(p, 'w', encoding='latin-1').write(t)
            purged += len(edits)
    print('stray prototypes removed: %d' % purged)
    with open(os.path.join(ROOT, 'build/portable/funcproto_skipped.txt'), 'w') as fh:
        for nm in sorted(why):
            fh.write('%s: %s\n' % (nm, why[nm]))
    print('true prototypes: %d (%d left to their own headers)' % (len(protos), len(skipped)))


if __name__ == '__main__':
    main()
