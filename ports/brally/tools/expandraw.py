#!/usr/bin/env python3
"""Expand a source file's own raw-offset accessor macros in place.

Decompiler-style files wrap `*(T *)((char *)p + off)` in local macros
(CAR_F(p, off), DW(p, o), PF(sym, T) ...). rewrite.py cannot see through a
macro, so each use is replaced by the macro's body with the arguments
substituted, and the definition is dropped once nothing uses it. Only
single-line function-like macros whose body casts to a pointer and adds an
offset are touched; anything with # or ## is left alone.

Usage: expandraw.py FILE...
"""
import re
import sys

DEF = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)\(([^)]*)\)[ \t]+(.*?)[ \t]*(/\*.*\*/)?[ \t]*$', re.M)


def is_raw(body):
    if '#' in body:
        return False
    # a bare pointer cast of its argument (CAR_BYTES(c)): the arithmetic is
    # at the use site, so it has to be inline for rewrite.py to see it
    if re.match(r'^\(*\s*\((?:const\s+)?(?:u?int8_t|(?:unsigned\s+)?char)\s*\*\s*\)\s*(?:\(\s*void\s*\*\s*\)\s*)?\(?\s*\w+\s*\)?\s*\)*$', body):
        return True
    cast_add = '+' in body and re.search(r'\(\s*(?:const\s+|volatile\s+)*[\w ]+\*+\s*\)', body)
    byte_sub = re.search(r'\)\s*\[\s*0x[0-9a-fA-F]+\s*\]', body)
    return bool(cast_add or byte_sub)


def args_at(s, i):
    """s[i] == '(': return (list of args, index after ')')."""
    depth, cur, out = 0, [], []
    j = i
    while j < len(s):
        c = s[j]
        if c == '(':
            depth += 1
            if depth > 1:
                cur.append(c)
        elif c == ')':
            depth -= 1
            if depth == 0:
                out.append(''.join(cur).strip())
                return out, j + 1
            cur.append(c)
        elif c == ',' and depth == 1:
            out.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        j += 1
    return None, i


def expand_file(path):
    s = open(path, encoding='latin-1').read()
    total = 0
    for _ in range(4):          # macros that use other macros
        macros = {}
        defs = [m for m in DEF.finditer(s) if not m.group(3).endswith('\\')]
        for m in defs:
            if is_raw(m.group(3)):
                params = [p.strip() for p in m.group(2).split(',') if p.strip()]
                macros[m.group(1)] = (params, m.group(3), m.span())
        # raw through another raw macro (CAR_I32 -> CAR_AT)
        grew = True
        while grew:
            grew = False
            for m in defs:
                if m.group(1) not in macros and any(re.search(r'\b%s\s*\(' % re.escape(n), m.group(3))
                                                    for n in macros):
                    params = [p.strip() for p in m.group(2).split(',') if p.strip()]
                    macros[m.group(1)] = (params, m.group(3), m.span())
                    grew = True
        if not macros:
            break
        changed = 0
        for name, (params, body, span) in macros.items():
            out, i = [], 0
            rx = re.compile(r'\b%s\s*\(' % re.escape(name))
            while True:
                m = rx.search(s, i)
                if not m:
                    out.append(s[i:])
                    break
                # skip the definition line itself
                ls = s.rfind('\n', 0, m.start()) + 1
                if re.match(r'[ \t]*#[ \t]*define', s[ls:m.start()]):
                    out.append(s[i:m.end()])
                    i = m.end()
                    continue
                args, end = args_at(s, m.end() - 1)
                if args is None or len(args) != len(params):
                    out.append(s[i:m.end()])
                    i = m.end()
                    continue
                text = body
                for p, a in zip(params, args):
                    # a type argument (used as `(P *)` or `(P)` in a cast) goes in bare
                    is_type = re.search(r'\(\s*%s\s*\**\s*\)' % re.escape(p), body)
                    text = re.sub(r'\b%s\b' % re.escape(p),
                                  (lambda _m, a=a: a) if is_type else (lambda _m, a=a: '(%s)' % a), text)
                out.append(s[i:m.start()])
                out.append('(%s)' % text)
                i = end
                changed += 1
            s = ''.join(out)
        # drop definitions nothing uses any more
        for name, (params, body, span) in macros.items():
            # a use anywhere but its own definition line keeps it (another
            # macro's body counts)
            own = re.compile(r'^[ \t]*#[ \t]*define[ \t]+%s\([^\n]*\n' % re.escape(name), re.M)
            if not re.search(r'\b%s\s*\(' % re.escape(name), own.sub('', s)):
                s = re.sub(r'^[ \t]*#[ \t]*define[ \t]+%s\([^\n]*\n' % re.escape(name), '', s, flags=re.M)
        total += changed
        if not changed:
            break
    open(path, 'w', encoding='latin-1').write(s)
    return total


def main():
    n = 0
    for f in sys.argv[1:]:
        k = expand_file(f)
        if k:
            print('%s: %d uses expanded' % (f, k))
        n += k
    print('expanded %d macro uses' % n)


if __name__ == '__main__':
    main()
