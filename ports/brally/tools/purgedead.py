#!/usr/bin/env python3
"""Remove C functions that are not the original game's and that nothing
from the original game reaches.

The fork came from a tree that also held an earlier hand-written port, so
many .c files carry untagged helpers (port harness, test resets, a second
definition of a matched function under the same name).  Roots are every
function tagged `@implements` plus every name used outside a function body
(tables, initialisers, macros).  An untagged function survives only if a
root reaches it through name references.  .cpp files are left alone.

Usage: purgedead.py [--dry]
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
CORE = os.path.join(ROOT, 'ports/brally/src/core')
IDENT = re.compile(r'[A-Za-z_]\w*')


def blank_comments(s):
    """Same length text with comments and literals blanked (offsets kept)."""
    out, i, n = list(s), 0, len(s)
    while i < n:
        if s.startswith('//', i):
            j = s.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif s.startswith('/*', i):
            j = s.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif s[i] in '"\'':
            q, j = s[i], i + 1
            while j < n and s[j] != q:
                j += 2 if s[j] == '\\' else 1
            for k in range(i + 1, min(j, n)):
                if out[k] != '\n':
                    out[k] = ' '
            i = j + 1
        else:
            i += 1
    return ''.join(out)


TAGS = {}


def functions(path):
    s = open(path, encoding='latin-1').read()
    TAGS[path] = set(re.findall(r'@implements\s+0x[0-9A-Fa-f]+\s+\w+\s+(\w+)', s))
    b = blank_comments(s)
    funcs, depth, i, last_end = [], 0, 0, 0
    n = len(b)
    while i < n:
        c = b[i]
        if c == '{':
            if depth == 0:
                head = b[last_end:i]
                m = re.search(r'(\w+)\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*$', head)
                pre = head[:m.start()] if m else ''
                is_fn = (m is not None and not re.search(r'[=;]\s*$', pre.rstrip())
                         and m.group(1) not in ('if', 'while', 'for', 'switch', 'sizeof')
                         and re.search(r'[\w*]\s*$', pre))
                start_body = i
                d, j = 0, i
                while j < n:
                    if b[j] == '{':
                        d += 1
                    elif b[j] == '}':
                        d -= 1
                        if d == 0:
                            break
                    j += 1
                if is_fn:
                    # declaration start: after the previous top-level ; or }
                    # the declaration starts after the last preprocessor
                    # line or comment that precedes it
                    k = last_end
                    raw = s[last_end:i]
                    for pm in re.finditer(r'^[ \t]*#[^\n]*(?:\\\n[^\n]*)*\n|\*/', raw, re.M):
                        k = last_end + pm.end()
                    while k < i and s[k] in ' \t\r\n':
                        k += 1
                    decl_start = k
                    name = m.group(1)
                    lead = s[last_end:decl_start + (m.start() - (decl_start - last_end))]
                    tagged = name in TAGS.get(path, ()) 
                    static = bool(re.search(r'\bstatic\b', pre))
                    funcs.append(dict(name=name, start=last_end, sig=decl_start, body=start_body, end=j + 1,
                                      tagged=tagged, static=static, path=path))
                i = j + 1
                last_end = i
                continue
        elif c == ';' and depth == 0:
            last_end = i + 1
        i += 1
    # a name defined twice in one file: the tagged one is the definition the
    # @implements comment precedes (after the previous function's end)
    names = {}
    for fn in funcs:
        names.setdefault(fn['name'], []).append(fn)
    prev_end = 0
    for fn in funcs:
        if fn['tagged'] and len(names[fn['name']]) > 1:
            fn['tagged'] = bool(re.search(r'@implements\s+0x[0-9A-Fa-f]+\s+\w+\s+%s\b' % fn['name'],
                                          s[prev_end:fn['body']]))
        prev_end = fn['end']
    return s, b, funcs


def uses_outside(t):
    """Names a top-level text USES: initialiser right-hand sides and macro
    bodies, not the declarations themselves."""
    out = set()
    for m in re.finditer(r'^\s*#\s*define\s+\w+(?:\([^)]*\))?([^\n]*(?:\\\n[^\n]*)*)', t, re.M):
        out.update(IDENT.findall(m.group(1)))
    t = re.sub(r'^\s*#[^\n]*(?:\\\n[^\n]*)*', '', t, flags=re.M)
    for stmt in t.split(';'):
        if '=' in stmt:
            out.update(IDENT.findall(stmt[stmt.index('='):]))
    return out


def main():
    dry = '--dry' in sys.argv
    files = {}
    for dp, _, fs in os.walk(CORE):
        for f in fs:
            if f.endswith('.c'):
                p = os.path.join(dp, f)
                files[p] = functions(p)
    for dp, _, fs in os.walk(CORE):
        for f in fs:
            if f.endswith('.cpp'):
                p = os.path.join(dp, f)
                s = open(p, encoding='latin-1').read()
                files[p] = (s, blank_comments(s), [])
    allf = [fn for (_, _, fl) in files.values() for fn in fl]
    by_name = {}
    for fn in allf:
        by_name.setdefault(fn['name'], []).append(fn)
    tagged_names = {fn['name'] for fn in allf if fn['tagged']}

    # references made by each function body, and roots outside bodies
    refs = {}
    roots = set()
    for p, (s, b, fl) in files.items():
        spans = [(fn['body'], fn['end']) for fn in fl]
        outside, pos = [], 0
        for a, z in sorted(spans):
            outside.append(b[pos:a])
            pos = z
        outside.append(b[pos:])
        roots.update(uses_outside(''.join(outside) if not p.endswith('.cpp') else b))
        for fn in fl:
            refs[id(fn)] = set(IDENT.findall(b[fn['body']:fn['end']]))
    for dp, _, fs in os.walk(os.path.join(ROOT, 'ports/brally/include')):
        for f in fs:
            t = blank_comments(open(os.path.join(dp, f), encoding='latin-1').read())
            # macros in headers can call functions
            for m in re.finditer(r'^\s*#\s*define[^\n]*(?:\\\n[^\n]*)*', t, re.M):
                roots.update(IDENT.findall(m.group(0)))

    live = set()
    work = [fn for fn in allf if fn['tagged']]
    # names referenced outside bodies make every same-named untagged def live
    # only when no tagged definition of the name exists
    for name in roots:
        for fn in by_name.get(name, []):
            if not fn['tagged'] and name not in tagged_names:
                work.append(fn)
    while work:
        fn = work.pop()
        if id(fn) in live:
            continue
        live.add(id(fn))
        for name in refs[id(fn)]:
            cands = by_name.get(name, [])
            same = [c for c in cands if c['path'] == fn['path'] and c['static']]
            for c in (same or cands):
                if c['tagged'] or name not in tagged_names:
                    work.append(c)

    dead = [fn for fn in allf if id(fn) not in live]
    dup = [fn for fn in allf if not fn['tagged'] and fn['name'] in tagged_names and id(fn) in live]
    print('functions %d, tagged %d, removed %d (untagged duplicates of tagged names: %d)'
          % (len(allf), len(tagged_names), len(dead),
             sum(1 for fn in dead if fn['name'] in tagged_names)))
    if dup:
        print('live untagged duplicates (kept):', ' '.join(sorted({f['name'] for f in dup})))
    if dry:
        for fn in sorted(dead, key=lambda f: (f['path'], f['start'])):
            print('  %s %s' % (os.path.relpath(fn['path'], ROOT), fn['name']))
        return
    for p, (s, b, fl) in files.items():
        cut = sorted([fn for fn in fl if id(fn) not in live], key=lambda f: -f['start'])
        if not cut:
            continue
        for fn in cut:
            # keep the leading comment block attached to the next item
            s = s[:fn['sig']] + '/* (port-only %s removed) */\n' % fn['name'] + s[fn['end']:]
        open(p, 'w', encoding='latin-1').write(s)


if __name__ == '__main__':
    main()
