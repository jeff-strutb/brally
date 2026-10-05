#!/usr/bin/env python3
"""bevars.py -- inside one function, make named pointer variables read and
write big-endian memory (display lists, vertices: tgr_core.h).

    bevars.py FILE FUNCTION VAR:KIND [VAR:KIND ...]

KIND is u32, s16 or u16.  Within FUNCTION:

    VAR[e] = x;        ->  tgr_wr32(&VAR[e], x);   (tgr_wr16 for 16-bit)
    VAR[e] op= x;      ->  tgr_wr32(&VAR[e], tgr_rd32(&VAR[e]) op (x));
    VAR[e] (a read)    ->  tgr_rd32(&VAR[e])        ((short)tgr_rd16 for s16)
    *VAR (a read)      ->  tgr_rd32(VAR)

&VAR[e] and the pointer itself are left alone.
"""
import re
import sys


def func_span(src, name):
    m = re.search(r'^[A-Za-z_][^\n;{}()]*\b%s\s*\([^;{]*?\)\s*\{' % re.escape(name), src, re.M | re.S)
    if not m:
        sys.exit('no function %s' % name)
    i, depth = m.end() - 1, 0
    while i < len(src):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return m.end(), i
        i += 1
    sys.exit('unbalanced %s' % name)


def bracket_end(s, i):
    """index just past the ']' matching the '[' at s[i]"""
    depth = 0
    while i < len(s):
        if s[i] == '[':
            depth += 1
        elif s[i] == ']':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def rewrite(body, var, kind):
    rd = {'u32': 'tgr_rd32(%s)', 'u16': 'tgr_rd16(%s)', 's16': '(short)tgr_rd16(%s)'}[kind]
    wr = {'u32': 'tgr_wr32', 'u16': 'tgr_wr16', 's16': 'tgr_wr16'}[kind]
    out, i = [], 0
    pat = re.compile(r'(?<![\w.>&])%s\s*\[' % re.escape(var))
    while True:
        m = pat.search(body, i)
        if not m:
            out.append(body[i:])
            break
        start = m.start()
        # &VAR[e]: an address, untouched
        j = start - 1
        while j >= 0 and body[j] in ' \t':
            j -= 1
        lb = m.end() - 1
        rb = bracket_end(body, lb)
        lv = body[start:rb]
        if j >= 0 and body[j] == '&' and not (j >= 1 and body[j - 1] == '&'):
            out.append(body[i:rb])
            i = rb
            continue
        rest = body[rb:]
        am = re.match(r'\s*(\+|-|\*|/|\||&|\^|<<|>>)?=(?!=)', rest)
        if am:
            # an assignment: find the statement's end
            k = rb + am.end()
            depth = 0
            e = k
            while e < len(body):
                ch = body[e]
                if ch in '([{':
                    depth += 1
                elif ch in ')]}':
                    if depth == 0:
                        break
                    depth -= 1
                elif ch == ';' and depth == 0:
                    break
                elif ch == ',' and depth == 0:
                    break
                e += 1
            rhs = body[k:e].strip()
            rhs = rewrite(rhs, var, kind)
            if am.group(1):
                rhs = '%s %s (%s)' % (rd % ('&' + lv), am.group(1), rhs)
            if kind == 's16':
                rhs = '(short)(int)(%s)' % rhs
            # the statement may be "VAR[e] = x" inside a larger expression;
            # only plain statements are rewritten
            pre = body[i:start]
            out.append(pre + '%s(&%s, %s)' % (wr, lv, rhs))
            i = e
            continue
        inner = rewrite(lv, var, kind) if lv.count('[') > 1 else lv
        out.append(body[i:start] + rd % ('&' + inner))
        i = rb
    return ''.join(out)


def main():
    path, func = sys.argv[1], sys.argv[2]
    src = open(path).read()
    a, b = func_span(src, func)
    body = src[a:b]
    for spec in sys.argv[3:]:
        var, kind = spec.split(':')
        body = rewrite(body, var, kind)
    open(path, 'w').write(src[:a] + body + src[b:])


if __name__ == '__main__':
    main()
