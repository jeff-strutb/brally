#!/usr/bin/env python3
"""Rewrite one function's raw-offset record accesses onto named fields.

    rawfield.py FILE FUNC VAR=Record [VAR=Record ...] [--include HDR] [--dry]

The decompiled bodies address records as `*(T *)(base + K)` with the
original's i386 byte offsets, the base often an `int`. For each VAR given
(a parameter or local holding a pointer to Record), this rewrites, inside
FUNC only and innermost first:

    *(T *)(VAR + K)        ->  (*(T *)&VAR->path)          K resolved in Record's
                                                            i386 layout (clang)
    *(int *)(VAR + K)      ->  VAR->path                   when the field is a
                                                            pointer to a record:
                                                            the result is itself a
                                                            typed base for the
                                                            enclosing access
    VAR + K  (an address)  ->  ((void *)((char *)&VAR->path + rem))

An `int` local assigned such a record pointer (`iVar9 = *(int *)(p + 4);`)
gets a typed twin (`pR_iVar9`) that carries the pointer until the local is
next assigned something else; its uses as a base go through the twin.

Field resolution is viewmerge.resolve on clang's i386 record layouts, so
every offset is measured by the compiler. Offsets that resolve to nothing
are left alone and listed. Review the diff: the rewrite is textual.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmerge as vm  # noqa: E402

NUM = r'(?:0x[0-9a-fA-F]+|\d+)'


def func_span(s, name):
    m = re.search(r'^[^\n;{}]*\b%s\s*\([^;{]*\)\s*\{' % re.escape(name), s, re.M)
    if not m:
        sys.exit('no definition of %s' % name)
    i, depth = m.end() - 1, 0
    while True:
        c = s[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return m.start(), i + 1
        i += 1


def recname(t):
    t = re.sub(r'\b(const|volatile|struct|union|class)\b', '', t).strip()
    return t


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    dry = '--dry' in sys.argv
    inc = []
    if '--include' in sys.argv:
        inc = ['-include', sys.argv[sys.argv.index('--include') + 1]]
        args = [a for a in args if a != sys.argv[sys.argv.index('--include') + 1]]
    path, func = args[0], args[1]
    bases = dict(a.split('=', 1) for a in args[2:])
    os.chdir(vm.ROOT)
    recs, sizes = vm.layouts(path, inc)
    for r in bases.values():
        if r not in recs:
            sys.exit('no layout for %s (try --include)' % r)
    s = open(path, encoding='latin-1').read()
    a, b = func_span(s, func)
    body = s[a:b]
    toks = {}          # placeholder -> (text, record or None)
    unresolved = []
    twins = {}         # int local -> (twin name, record) while it holds a pointer
    decls = []

    def tok(text, rec):
        k = '\x01%d\x02' % len(toks)
        toks[k] = (text, rec)
        return k

    def base_re():
        names = [re.escape(n) for n in list(bases) + list(twins)]
        alts = names + [re.escape(k) for k in toks if toks[k][1]]
        return '(?:%s)' % '|'.join(sorted(alts, key=len, reverse=True)) if alts else None

    def resolve(bname, off):
        if bname in toks:
            text, rec = toks[bname]
        elif bname in twins:
            text, rec = twins[bname]
        else:
            text, rec = bname, bases[bname]
        r = vm.resolve(recs, sizes, rec, off, None)
        if r is None:
            return None
        return text, rec, r

    changed = True
    while changed:
        changed = False
        br = base_re()
        if not br:
            break
        # *(T *)(BASE + K)
        def deref(m):
            nonlocal changed
            t, bname, off = m.group(1).strip(), m.group(2), int(m.group(3), 0)
            res = resolve(bname, off)
            if res is None:
                unresolved.append('%s + 0x%X' % (toks.get(bname, (bname,))[0], off))
                return m.group(0)
            text, rec, (fpath, ftype, rem) = res
            changed = True
            leaf = recname(ftype)
            if rem == 0 and ftype.strip().endswith('*') and recname(ftype[:-1]) in recs and t in ('int', 'unsigned int', 'uint32_t', 'int32_t', 'void *'):
                return tok('%s->%s' % (text, fpath), recname(ftype[:-1]))
            if rem == 0 and ftype.strip().endswith('*') and t in ('int', 'unsigned int', 'uint32_t', 'int32_t'):
                # a pointer field read through an int view: the pointer itself
                return tok('(%s->%s)' % (text, fpath), None)
            if rem == 0:
                return tok('(*(%s *)&%s->%s)' % (t, text, fpath), None)
            return tok('(*(%s *)((char *)&%s->%s + %d))' % (t, text, fpath, rem), None)
        body2 = re.sub(r'\*\(([\w ]+?\*?)\s*\*\)\((%s)\s*\+\s*(%s)\)' % (br, NUM), deref, body)
        if body2 != body:
            body = body2
            continue
        # VAR = <record pointer token>;   (an int local carrying a pointer)
        def assign(m):
            nonlocal changed
            var, k = m.group(1), m.group(2)
            if var in bases:
                return m.group(0)
            rec = toks[k][1]
            twin = 'pR_%s' % var
            twins[var] = (twin, rec)
            if twin not in [d[0] for d in decls]:
                decls.append((twin, rec))
            changed = True
            return '%s = %s;' % (twin, toks[k][0])
        tk = '|'.join(re.escape(k) for k in toks if toks[k][1])
        if tk:
            body2 = re.sub(r'\b(\w+)\s*=\s*(%s)\s*;' % tk, assign, body)
            if body2 != body:
                body = body2
                continue
        # a twin's local reassigned to anything else ends the twin
        for var in list(twins):
            if re.search(r'\b%s\s*=(?!=)' % re.escape(var), body):
                pass
        # BASE + K as an address
        def addr(m):
            nonlocal changed
            bname, off = m.group(1), int(m.group(2), 0)
            res = resolve(bname, off)
            if res is None:
                unresolved.append('&%s + 0x%X' % (toks.get(bname, (bname,))[0], off))
                return m.group(0)
            text, rec, (fpath, ftype, rem) = res
            changed = True
            if rem:
                return tok('((void *)((char *)&%s->%s + %d))' % (text, fpath, rem), None)
            return tok('((void *)&%s->%s)' % (text, fpath), None)
        abr = '(?:%s)' % '|'.join(sorted([re.escape(n) for n in bases] + [re.escape(k) for k in toks if toks[k][1]], key=len, reverse=True))
        body2 = re.sub(r'(?<![\w\x02])(%s)\s*\+\s*(%s)(?![\w\x01])' % (abr, NUM), addr, body)
        if body2 != body:
            body = body2
            continue
    # twins: uses of the int local as a base were rewritten through the
    # twin while it was bound; replace the base occurrences now
    for var, (twin, rec) in twins.items():
        pass
    for _ in range(len(toks) + 1):
        for k, (text, rec) in toks.items():
            body = body.replace(k, text)
    for var, (twin, rec) in twins.items():
        body = body.replace('%s->' % twin, '%s->' % twin)
    if decls:
        brace = body.index('{') + 1
        body = body[:brace] + ''.join('\n  %s *%s;' % (rec, n) for n, rec in decls) + body[brace:]
    if unresolved:
        print('unresolved:', ', '.join(sorted(set(unresolved))))
    print('%s: %s rewritten' % (path, func))
    if not dry:
        open(path, 'w', encoding='latin-1').write(s[:a] + body + s[b:])
    else:
        sys.stdout.write(body)


if __name__ == '__main__':
    main()
