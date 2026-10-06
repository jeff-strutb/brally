#!/usr/bin/env python3
"""Propagate pointer types into variables the decompiler left as int.

Once a struct is typed, the compiler names every int that holds one of its
pointers. For each failing core file this reads clang's diagnostics and,
where the int is a named local, parameter or global declared in this file,
gives it the pointer type it actually holds:

  initializing 'int' with an expression of type 'T *'       -> the variable
  assigning to 'int' from 'T *'                             -> the variable
  passing 'T *' to parameter of type 'int'  (+ note: parameter here)
                                                            -> that parameter
  passing 'int' to parameter of type 'T *'  (argument is a variable)
                                                            -> the variable
  assigning to 'T *' from 'int'  (right side is a variable) -> the variable

Repeat with the build until nothing changes. Byte arithmetic left on a
newly typed struct pointer is rewrite.py's job (run it after).

Usage: fixint.py [--dry] [FILE...]
"""
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402

ROOT = rawscan.ROOT
DRY = '--dry' in sys.argv
INTS = ('int', 'unsigned int', 'int32_t', 'uint32_t', 'DWORD', 'uint', 'long', 'unsigned long', 'UINT',
        'LONG', 'undefined4', 'uintptr_t', 'intptr_t')


def diags(f):
    lang = ['-x', 'c++', '-std=c++98'] if f.endswith('.cpp') else ['-x', 'c', '-std=gnu89']
    p = subprocess.run(['clang'] + rawscan.flags_for(f) + ['-ferror-limit=0'] + lang + [f],
                       capture_output=True, text=True, errors='replace', cwd=ROOT)
    return p.stderr


def offset_of(text, line, col):
    lines = text.split('\n')
    return sum(len(l) + 1 for l in lines[:line - 1]) + col - 1


AKA = {}


def ptr_type(t):
    t = t.strip()
    if not t.endswith('*'):
        return t if AKA.get(t, '').rstrip().endswith('*') else None
    if re.match(r'^(const\s+)?void\s*\*$', t):
        return 'char *'
    t = re.sub(r'^const\s+', '', t)
    return t


REMOTE = []


def retype_remote():
    """Retype parameter idx of each function's definition to pt."""
    import glob as _g
    done = 0
    want = {}
    for fn, idx, pt in REMOTE:
        want.setdefault((fn, idx), pt)
    if not want:
        return 0
    names = {fn for fn, _ in want}
    for p in _g.glob(os.path.join(ROOT, 'ports/brally/src/**/*.c*'), recursive=True):
        s = open(p, encoding='latin-1').read()
        o = s
        for fn in names:
            m = re.search(r'^[^\n;{}#]*\b%s\s*\(([^;{}]*)\)\s*\{' % re.escape(fn), s, re.M)
            if not m:
                continue
            params = m.group(1)
            parts, depth, cur = [], 0, ''
            for ch in params:
                if ch in '([':
                    depth += 1
                elif ch in ')]':
                    depth -= 1
                if ch == ',' and depth == 0:
                    parts.append(cur)
                    cur = ''
                else:
                    cur += ch
            parts.append(cur)
            changed = False
            for (wfn, idx), pt in want.items():
                if wfn != fn or idx >= len(parts):
                    continue
                pm = re.match(r'^(\s*)((?:unsigned\s+|signed\s+)?\w+)(\s+)(\w+)\s*$', parts[idx])
                if pm and pm.group(2) in INTS:
                    parts[idx] = '%s%s %s' % (pm.group(1), pt if not pt.endswith('*') else pt[:-1].rstrip() + ' *',
                                              pm.group(4))
                    changed = True
            if changed:
                s = s[:m.start(1)] + ','.join(parts) + s[m.end(1):]
                done += 1
        if s != o:
            open(p, 'w', encoding='latin-1').write(s)
    return done


def main():
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    os.chdir(ROOT)
    if not files:
        files = []
        for ln in open('build/brally/null-soft/compile.txt'):
            if ln.startswith('FAIL '):
                files.append(ln.split(None, 1)[1].strip())
    total = 0
    for f in files:
        text = open(f, encoding='latin-1').read()
        d = diags(f)
        for m in re.finditer(r"'([^']+)' \(aka '([^']+)'\)", d):
            AKA[m.group(1)] = m.group(2)
        want = {}     # offset of a name reference -> pointer type
        loc_rx = r'^%s:(\d+):(\d+): error: ' % re.escape(f)
        for m in re.finditer(loc_rx + r"incompatible pointer to integer conversion (?:initializing|assigning to) "
                             r"'([^']+)'(?: \(aka '[^']+'\))? (?:with an expression of type|from) '([^']+)'", d, re.M):
            if m.group(3) in INTS or m.group(3).split()[-1] in INTS:
                pt = ptr_type(m.group(4))
                if pt:
                    want[('lhs', int(m.group(1)), int(m.group(2)))] = pt
        for m in re.finditer(loc_rx + r"incompatible integer to pointer conversion (?:passing|assigning to) "
                             r"'([^']+)'(?: \(aka '[^']+'\))? (?:to parameter of type|from) '([^']+)'", d, re.M):
            a, b = m.group(3), m.group(4)
            # passing 'int' to parameter of type 'T *'  /  assigning to 'T *' from 'int'
            if 'assigning to' in m.group(0):
                pt, it = ptr_type(a), b
            else:
                pt, it = ptr_type(b), a
            if pt and (it in INTS or it.split()[-1] in INTS):
                want[('arg', int(m.group(1)), int(m.group(2)))] = pt
        for m in re.finditer(loc_rx + r"incompatible pointer to integer conversion passing '([^']+)'"
                             r"(?: \(aka '[^']+'\))? to parameter of type '([^']+)'.*\n(?:.*\n)*?"
                             r"(\S+?):(\d+):(\d+): note: passing argument to parameter", d, re.M):
            pt = ptr_type(m.group(3))
            if pt and (m.group(4) in INTS or m.group(4).split()[-1] in INTS):
                want[('param', m.group(5), int(m.group(6)), int(m.group(7)))] = pt
        if not want:
            continue
        a = rawscan.ast(f)
        if a is None:
            continue
        # every DeclRefExpr / VarDecl / ParmVarDecl by source offset
        refs, decls = {}, {}

        def walk(n):
            k = n.get('kind')
            r = n.get('range', {}).get('begin', {})
            if k == 'DeclRefExpr' and 'offset' in r and 'spellingLoc' not in r:
                refs.setdefault(r['offset'], n.get('referencedDecl', {}))
            if k in ('VarDecl', 'ParmVarDecl'):
                decls[n.get('id')] = n
                lo = n.get('loc', {})
                if 'offset' in lo:
                    refs.setdefault(lo['offset'], {'id': n.get('id'), 'kind': k, 'name': n.get('name')})
            for c in n.get('inner', []) or []:
                if isinstance(c, dict):
                    walk(c)
        walk(a)
        retype = {}   # decl id -> pointer type
        for key, pt in want.items():
            if key[0] in ('lhs', 'arg'):
                off = offset_of(text, key[1], key[2])
                cand = None
                if key[0] == 'arg':
                    # the argument must be exactly one variable
                    r = refs.get(off)
                    if r and r.get('name') and text[off:off + len(r['name'])] == r['name'] and \
                            re.match(r'\s*[,)]', text[off + len(r['name']):]):
                        cand = r
                else:
                    # initializing: the declaration's own name; assigning: the
                    # left-hand side name immediately before '='
                    for o in sorted(refs, reverse=True):
                        if o > off:
                            continue
                        r = refs[o]
                        nm = r.get('name') or ''
                        between = text[o + len(nm):off]
                        if text[o:o + len(nm)] == nm and re.match(r'^\s*=?\s*$', between):
                            cand = r
                        break
                if cand and cand.get('id') in decls:
                    retype[cand['id']] = pt
            else:
                _, pf, pl, pc = key
                if os.path.abspath(pf) != os.path.abspath(os.path.join(ROOT, f)):
                    # declared in the prototypes header: retype the parameter
                    # in the function's definition (br_funcs.h is regenerated)
                    if pf.endswith('br_funcs.h'):
                        try:
                            hl = open(os.path.join(ROOT, pf)).read().split('\n')[pl - 1]
                        except (OSError, IndexError):
                            continue
                        fm = re.search(r'\b(\w+)\s*\(', hl)
                        if fm:
                            # parameter index = commas before the column
                            idx = hl[:pc - 1][hl.index('(') + 1:].count(',')
                            REMOTE.append((fm.group(1), idx, pt))
                    continue
                off = offset_of(text, pl, pc)
                for did, dn in decls.items():
                    if dn.get('kind') == 'ParmVarDecl' and dn.get('loc', {}).get('offset') == off:
                        retype[did] = pt
        edits = []
        for did, pt in retype.items():
            dn = decls[did]
            b = dn.get('range', {}).get('begin', {})
            lo = dn.get('loc', {})
            if 'offset' not in b or 'offset' not in lo or 'spellingLoc' in b:
                continue
            cur = text[b['offset']:lo['offset']]
            if '*' in cur or ',' in text[b['offset']:lo['offset']]:
                continue
            if not re.search(r'\b(%s)\b' % '|'.join(map(re.escape, INTS)), cur):
                continue
            # keep storage-class words, swap the type
            sc = ' '.join(w for w in cur.split() if w in ('static', 'extern', 'register', 'volatile'))
            edits.append((b['offset'], lo['offset'], (sc + ' ' if sc else '') + pt + ('' if pt.endswith('*') else ' ')))
        for st, en, t in sorted(edits, reverse=True):
            text = text[:st] + t + text[en:]
        if edits:
            total += len(edits)
            print('%s: %d retyped' % (f, len(edits)))
            if not DRY:
                open(f, 'w', encoding='latin-1').write(text)
    r = retype_remote()
    print('fixint: %d variables retyped; %d function definitions given pointer parameters' % (total, r))


if __name__ == '__main__':
    main()
