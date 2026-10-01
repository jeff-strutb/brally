#!/usr/bin/env python3
"""Inventory of the original globals the core names, by original address.

The decompiled files each declare the original globals they use, under
whatever name and type that file chose. The 64-bit core needs one typed
definition per original object, with every other name an alias of it. This
lists, for every file-scope variable any core file declares:

    the original address (from the name, the 32-bit lane's symbol maps, or
    an address comment on the declaration), the name, the type, whether the
    file defines it, and which files declare it.

Usage: globals.py [--jobs N]
Output: build/portable/globals.csv, and a summary on stdout
"""
import argparse
import collections
import concurrent.futures
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402

ROOT = rawscan.ROOT
WB = os.path.join(ROOT, 'build', 'wasm')
LO, HI = 0x10077000, 0x11900000


def addr_in_name(n):
    m = re.search(r'(?<![0-9A-Fa-f])(1[01][0-9a-fA-F]{6})(?![0-9A-Fa-f])', n)
    if m:
        v = int(m.group(1), 16)
        if LO <= v < HI:
            return v
    m = re.search(r'_([0-9A-Fa-f]{6})$', n) or re.search(r'(?<=[a-z_])([0-9A-F]{6})$', n)
    if m:
        v = 0x10000000 + int(m.group(1), 16)
        if LO <= v < HI:
            return v
    return None


def scan(f):
    a = rawscan.ast(f)
    if a is None:
        return []
    out = []
    src_cache = {}

    def text_of(path):
        if path not in src_cache:
            try:
                src_cache[path] = open(os.path.join(ROOT, path), encoding='latin-1').read().split('\n')
            except OSError:
                src_cache[path] = []
        return src_cache[path]
    cur_file = f
    tops = []
    for n in a.get('inner', []):
        if n.get('kind') in ('LinkageSpecDecl', 'NamespaceDecl'):
            tops.extend(n.get('inner', []) or [])
        else:
            tops.append(n)
    for n in tops:
        loc = n.get('loc', {})
        if 'file' in loc:
            cur_file = loc['file']
        elif 'spellingLoc' in loc and 'file' in loc['spellingLoc']:
            cur_file = loc['spellingLoc']['file']
        if n.get('kind') != 'VarDecl' or n.get('storageClass') == 'static' and cur_file != f:
            continue
        if not cur_file.startswith('ports/64b/'):
            continue
        line = loc.get('line') or loc.get('spellingLoc', {}).get('line')
        comment = ''
        if line:
            src = text_of(cur_file)
            if 0 < line <= len(src):
                comment = src[line - 1]
        cva = None
        m = re.search(r'0x(1[0-9A-Fa-f]{7})\b', comment)
        if m and LO <= int(m.group(1), 16) < HI:
            cva = int(m.group(1), 16)
        out.append({'name': n.get('name'), 'type': (n.get('type') or {}).get('qualType', ''),
                    'storage': n.get('storageClass', ''), 'init': 'init' in n,
                    'decl_file': cur_file, 'line': line, 'tu': f, 'comment_va': cva})
    return out


def sizes32(f, names):
    """{name: sizeof in the original 32-bit layout} for globals f declares."""
    import json
    import subprocess
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import recspec
    src = open(os.path.join(ROOT, f), encoding='latin-1').read()
    probe = ''.join('\nchar br_sz_%s[sizeof(%s)];' % (n, n) for n in sorted(names))
    lang = ['-x', 'c++', '-std=c++98'] if f.endswith('.cpp') else ['-x', 'c', '-std=gnu89']
    p = subprocess.run(['clang'] + recspec.I686 + recspec.FLAGS + ['-ferror-limit=0', '-I' + os.path.dirname(f)] +
                       lang + ['-Xclang', '-ast-dump=json', '-Xclang', '-ast-dump-filter=br_sz_', '-'],
                       input=(src + probe).encode('latin-1'), capture_output=True, cwd=ROOT)
    out = {}
    for m in re.finditer(r'"name": "br_sz_(\w+)".*?"qualType": "char\[(\d+)\]"', p.stdout.decode('utf-8', 'replace'), re.S):
        out[m.group(1)] = int(m.group(2))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=os.cpu_count())
    a = ap.parse_args()
    os.chdir(ROOT)
    files = sorted(os.path.relpath(os.path.join(dp, fn), ROOT)
                   for dp, _, fns in os.walk('ports/64b/src/core') for fn in fns if fn.endswith(('.c', '.cpp')))
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        rows = [r for rs in ex.map(scan, files) for r in rs]
    per_tu = collections.defaultdict(set)
    for r in rows:
        per_tu[r['tu']].add(r['name'])
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        szs = dict(zip(per_tu, ex.map(lambda f: sizes32(f, per_tu[f]), per_tu)))
    for r in rows:
        r['size'] = szs.get(r['tu'], {}).get(r['name'])
    # the 32-bit lane's answers
    sym = collections.defaultdict(dict)
    for r in csv.DictReader(open(os.path.join(WB, 'symmap.csv'))):
        sym[r['scope']][re.sub(r'\$S\d+$', '', r['name'])] = int(r['va'], 16)
    decl = {}
    for r in csv.DictReader(open(os.path.join(WB, 'decls.csv'))):
        decl[(os.path.basename(r['src']), r['name'])] = int(r['va'], 16)
    by = {}
    for r in rows:
        base = os.path.splitext(os.path.basename(r['tu']))[0]
        va = (sym.get(base, {}).get(r['name']) or sym['*'].get(r['name']) or
              decl.get((os.path.basename(r['tu']), r['name'])) or addr_in_name(r['name']) or r['comment_va'])
        key = (r['name'], r['type'], r['decl_file'], r['line'])
        e = by.setdefault(key, {'va': va, 'name': r['name'], 'type': r['type'], 'decl_file': r['decl_file'],
                                'line': r['line'], 'storage': r['storage'], 'init': r['init'], 'tus': set(),
                                'size': r['size']})
        if e.get('size') is None:
            e['size'] = r['size']
        e['tus'].add(r['tu'])
        if e['va'] is None and va is not None:
            e['va'] = va
    outp = os.path.join(ROOT, 'build', 'portable', 'globals.csv')
    with open(outp, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['va', 'name', 'type', 'size32', 'storage', 'defines', 'decl_file', 'line', 'tus'])
        for e in sorted(by.values(), key=lambda e: (e['va'] or 0, e['name'])):
            w.writerow(['0x%08X' % e['va'] if e['va'] else '', e['name'], e['type'],
                        '' if e['size'] is None else e['size'], e['storage'],
                        int(e['init'] or e['storage'] not in ('extern',)), e['decl_file'], e['line'],
                        len(e['tus'])])
    vas = collections.defaultdict(lambda: {'names': set(), 'types': set()})
    nova = set()
    for e in by.values():
        if e['va']:
            vas[e['va']]['names'].add(e['name'])
            vas[e['va']]['types'].add(re.sub(r'\s+', ' ', e['type']))
        else:
            nova.add(e['name'])
    multi_n = sum(1 for v in vas.values() if len(v['names']) > 1)
    multi_t = sum(1 for v in vas.values() if len(v['types']) > 1)
    print('declarations: %d; original addresses: %d (%d with several names, %d with several types); '
          'names with no address: %d' % (len(by), len(vas), multi_n, multi_t, len(nova)))
    print('-> %s' % outp)


if __name__ == '__main__':
    main()
