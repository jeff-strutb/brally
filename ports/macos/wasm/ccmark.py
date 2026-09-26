#!/usr/bin/env python3
"""Carry each INDIRECT call's x86 calling convention into the wasm object.

wasm has no calling conventions, but the tree spells MSVC ones in whatever
form reproduces the x86 bytes, so a call through a pointer can disagree with
its target at the C level and agree on x86 (a C++ virtual thiscall method
reached through a `__fastcall (*)(void *, int edx, ...)` pointer). The
callee's convention is known from its definition; the CALLER's is known only
at the call instruction. This pass pairs the call instructions of the wasm32
IR and the i686 IR of the same TU (both unoptimised frontend output, so the
k-th indirect call of a function is the same source call in both), and where
the i686 call is thiscall or fastcall appends one constant argument:

    MAGIC | cc << 20 | inreg_mask      (cc 1 thiscall, 2 fastcall, 3 stdcall)

w2c.py recognises and strips it. It also marks every definition noinline, so
each function the original has stays a function the port can address.

Usage: ccmark.py <wasm.ll> <x86.ll> <out.ll>
"""
import re
import sys

MAGIC = 0x7EC00000
CALL = re.compile(r'\bcall\b(.*?)\s(%[\w.]+)\((.*)\)(.*)$')
DEF = re.compile(r'^define\b[^@]*@("(?:[^"\\]|\\.)*"|[\w.$]+)\(')


def norm(n):
    n = n.strip('"')
    if n.startswith('\\01'):
        n = n[3:].lstrip('_@')
        n = re.sub(r'@\d+$', '', n)
    return n


def split_args(s):
    out, d, cur = [], 0, ''
    for ch in s:
        if ch in '([{<':
            d += 1
        elif ch in ')]}>':
            d -= 1
        if ch == ',' and d == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def indirect_calls(lines):
    """{function name: [line index of each indirect call, in order]}"""
    out, cur = {}, None
    for i, l in enumerate(lines):
        m = DEF.match(l)
        if m:
            cur = norm(m.group(1))
            out[cur] = []
            continue
        if l.startswith('}'):
            cur = None
            continue
        if cur and ' call ' in l:
            m = CALL.search(l)
            if m and not m.group(2).startswith('@'):
                out[cur].append(i)
    return out


def main():
    wl = open(sys.argv[1]).read().split('\n')
    xl = open(sys.argv[2]).read().split('\n')
    wi, xi = indirect_calls(wl), indirect_calls(xl)
    marked = 0
    for fn, wcalls in wi.items():
        xcalls = xi.get(fn)
        if not xcalls or len(xcalls) != len(wcalls):
            continue
        for wline, xline in zip(wcalls, xcalls):
            x = xl[xline]
            head = x.split(' call ')[1].split('%')[0]
            cc = 1 if 'x86_thiscallcc' in head else 2 if 'x86_fastcallcc' in head else \
                3 if 'x86_stdcallcc' in head else 0
            if not cc:
                continue
            m = CALL.search(x)
            args = split_args(m.group(3))
            mask = 0
            for k, a in enumerate(args):
                if ' inreg ' in ' ' + a + ' ':
                    mask |= 1 << k
            l = wl[wline]
            m2 = CALL.search(l)
            wargs = m2.group(3)
            sep = ', ' if wargs.strip() else ''
            new_args = wargs + sep + 'i32 %d' % (MAGIC | cc << 20 | mask)
            # the call's function type, when spelled, must take it too
            pre = m2.group(1)
            pre = re.sub(r'\((.*?)\)(\s*)$', lambda q: '(' + q.group(1) +
                         (', ' if q.group(1).strip() else '') + 'i32)' + q.group(2), pre) \
                if re.search(r'\(.*\)\s*$', pre) else pre
            start = m2.start()
            wl[wline] = l[:start] + 'call' + pre + ' ' + m2.group(2) + '(' + new_args + ')' + m2.group(4)
            marked += 1
    # every definition noinline (the original has each as its own function)
    for i, l in enumerate(wl):
        if l.startswith('define ') and ' noinline' not in l:
            wl[i] = re.sub(r'\)(\s*(?:#\d+\s*)?)\{\s*$', lambda q: ') noinline' + q.group(1) + '{', l)
    # -O0 frontend output is optnone; the port wants it optimised
    out = '\n'.join(wl)
    out = re.sub(r'\boptnone\b', '', out)
    open(sys.argv[3], 'w').write(out)
    if marked:
        print('ccmark: %s: %d indirect call(s) marked' % (sys.argv[1], marked), file=sys.stderr)


if __name__ == '__main__':
    main()
