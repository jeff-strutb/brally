#!/usr/bin/env python3
"""errfix.py -- apply the conversions the arena memory model makes routine,
where the compiler points at them.

    errfix.py FILE...      (repeats until a pass changes nothing)

Each core TU is compiled with source-range diagnostics and these errors are
rewritten in place (every other error is left for a person):

  (int)ptr                     tgr_addr32(ptr)     an address the game stores
  (int)Function                TGR_FA(Function)
  (T *)addr                    TGR_PTR(T *, addr)  an address the game holds
  f(addr) where f takes T *    f(TGR_PTR(T *, addr))
  p = addr   (p a T *)         p = TGR_PTR(T *, addr)
  addr = ptr (addr a TgrAddr)  addr = tgr_addr32(ptr)
  be32_t in arithmetic         BEF / BES32 by the other operand's type
  x = be field                 BEF / BE32 / BES32 / BE16 / BES16 / BEPTR by x's type
  f(be field)                  the same, by the parameter's type
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
P = 'ports/tgrally/platform'
FLAGS = ['-O0', '-fsyntax-only', '-std=gnu89', '-Wno-everything', '-Werror=int-conversion',
         '-Werror=pointer-to-int-cast', '-Werror=int-to-pointer-cast', '-Werror=void-pointer-to-int-cast',
         '-Werror=int-to-void-pointer-cast', '-fdiagnostics-print-source-range-info',
         '-fno-caret-diagnostics', '-ferror-limit=0', '-Iports/tgrally/include', '-I%s/include' % P,
         '-include', '%s/include/ultra64.h' % P, '-include', '%s/include/tgr_core.h' % P]

DIAG = re.compile(r'^(.*?):(\d+):(\d+):((?:\{\d+:\d+-\d+:\d+\})*): error: (.*)$')


def be_reader(target, be):
    """the accessor that turns a be16_t/be32_t into a value of type target"""
    t = re.sub(r'\s*\(aka .*\)', '', target).strip()
    t = t.replace('const ', '')
    if be == 'bef_t':
        return 'BEF(%s)'
    if be == 'be32_t':
        if t in ('float', 'double', 'f32'):
            return 'BEF(%s)'
        if t.endswith('*'):
            return 'BEPTR(' + t + ', %s)'
        if t.startswith('unsigned') or t in ('u32', 'uint32_t', 'TgrAddr'):
            return 'BE32(%s)'
        return 'BES32(%s)'
    if t.startswith('unsigned') or t in ('u16', 'uint16_t', 'u32', 'uint32_t'):
        return 'BE16(%s)'
    return 'BES16(%s)'


def primary_end(s, i):
    """the end of the operand expression starting at s[i] (a cast's
    operand): an identifier/number with postfix [] () . -> parts, a
    parenthesised expression, or & / * / - prefixes of those"""
    n = len(s)
    while i < n and s[i] in ' \t':
        i += 1
    while i < n and s[i] in '&*-!~':
        i += 1
    if i < n and s[i] == '(':
        depth = 0
        while i < n:
            if s[i] == '(':
                depth += 1
            elif s[i] == ')':
                depth -= 1
                if depth == 0:
                    i += 1
                    break
            i += 1
    else:
        m = re.match(r'[A-Za-z_0-9.]+', s[i:])
        if not m:
            return -1
        i += m.end()
    while i < n:
        if s[i] in '[(':
            close = ']' if s[i] == '[' else ')'
            depth = 0
            while i < n:
                if s[i] in '[(':
                    depth += 1
                elif s[i] in '])':
                    depth -= 1
                    if depth == 0:
                        i += 1
                        break
                i += 1
            continue
        m = re.match(r'(\.|->)[A-Za-z_]\w*', s[i:])
        if m:
            i += m.end()
            continue
        break
    return i


def cast_at(s, i):
    """(cast text, operand start, operand end) for a C cast at s[i]"""
    if i >= len(s) or s[i] != '(':
        return None
    depth, j = 0, i
    while j < len(s):
        if s[j] == '(':
            depth += 1
        elif s[j] == ')':
            depth -= 1
            if depth == 0:
                break
        j += 1
    cast = s[i + 1:j]
    k = j + 1
    e = primary_end(s, k)
    if e < 0:
        return None
    return cast.strip(), k, e


def fix_file(path, impl):
    p = subprocess.run(['clang'] + FLAGS + [path], capture_output=True, text=True, cwd=ROOT)
    text = open(os.path.join(ROOT, path)).read()
    lines = text.split('\n')
    starts = [0]
    for l in lines[:-1]:
        starts.append(starts[-1] + len(l) + 1)

    def off(line, col):
        return starts[line - 1] + col - 1

    macros = set(re.findall(r'^\s*#\s*define\s+(\w+)\(', text, re.M))
    edits = []
    for d in p.stderr.split('\n'):
        m = DIAG.match(d)
        if not m or os.path.abspath(os.path.join(ROOT, m.group(1))) != os.path.abspath(os.path.join(ROOT, path)):
            continue
        line, col, rng, msg = int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)
        ranges = [tuple(map(int, x)) for x in re.findall(r'\{(\d+):(\d+)-(\d+):(\d+)\}', rng)]

        def rtext(r):
            # clang prints the range's end one past its last character
            b, e = off(r[0], r[1]), off(r[2], r[3])
            # inside a macro's arguments clang reports the whole call: leave it
            mc = re.match(r'([A-Za-z_]\w*)\s*\(', text[b:e])
            if mc and (mc.group(1) in macros or re.match(r'^(g[A-Z]|G$|[A-Z_][A-Z0-9_]*$)', mc.group(1))) \
                    and text[b:e].rstrip().endswith(')') and e - b > 12:
                return None
            return b, e
        mg = re.match(r"controlling expression type '(bef_t|be32_t|be16_t)' not compatible", msg)
        if mg:
            # an accessor of the wrong kind: the field's own
            o = off(line, col)
            am = re.match(r'(BES32|BE32|BEF|BE16|BES16)\(', text[o:])
            if am:
                want = {'bef_t': 'BEF', 'be32_t': 'BES32' if am.group(1) in ('BES16', 'BEF') else am.group(1),
                        'be16_t': 'BE16'}[mg.group(1)]
                if mg.group(1) == 'be32_t' and am.group(1) == 'BEF':
                    edits.append((o, o + 4, 'tgr_rdf(&'))     # a float kept in an integer field
                else:
                    edits.append((o, o + len(am.group(1)), want))
            continue
        if msg.startswith("operand of type 'be32_t' where arithmetic or pointer type is required"):
            o = off(line, col)
            if text.startswith('TGR_PTR(', o):
                edits.append((o, o + len('TGR_PTR('), 'BEPTR('))
            continue
        mm = re.match(r"cast to smaller integer type '[^']*'(?: \(aka '[^']*'\))? from '(.*)'", msg)
        if mm:
            c = cast_at(text, off(line, col))
            if c:
                cast, k, e = c
                operand = text[k:e].strip()
                name = operand.lstrip('&')
                if '(*)' in mm.group(1) or (name in impl and not operand.startswith('&')):
                    new = 'TGR_FA(%s)' % name
                else:
                    new = 'tgr_addr32(%s)' % operand
                edits.append((off(line, col), e, new))
            continue
        mm = re.match(r"cast to '(.*?)'(?: \(aka '[^']*'\))? from smaller integer type", msg)
        if mm:
            c = cast_at(text, off(line, col))
            if c:
                cast, k, e = c
                if '(*)' in cast:
                    new = 'TGR_FN(%s, %s)' % (cast, text[k:e].strip())
                else:
                    new = 'TGR_PTR(%s, %s)' % (cast, text[k:e].strip())
                edits.append((off(line, col), e, new))
            continue
        mm = re.match(r"invalid operands to binary expression \('(.*?)'(?: \(aka '[^']*'\))? and '(.*?)'(?: \(aka '[^']*'\))?\)", msg)
        cop = re.match(r'(\+|-|\*|/|\||&|\^|<<|>>)=', text[off(line, col):])
        if mm and len(ranges) == 2 and cop and mm.group(1) in ('be16_t', 'be32_t'):
            # a compound assignment to a big-endian field
            lb = rtext(ranges[0])
            rb = rtext(ranges[1])
            if lb and rb:
                lhs, rhs = text[lb[0]:lb[1]], text[rb[0]:rb[1]]
                o = mm.group(2) if mm.group(2) not in ('double',) else 'float'
                if mm.group(1) == 'be16_t':
                    new = 'SET16(%s, (short)(int)(BES16(%s) %s (%s)))' % (lhs, lhs, cop.group(1), rhs)
                elif o == 'float':
                    new = 'SETF(%s, BEF(%s) %s (%s))' % (lhs, lhs, cop.group(1), rhs)
                else:
                    new = 'SET32(%s, BES32(%s) %s (%s))' % (lhs, lhs, cop.group(1), rhs)
                edits.append((lb[0], rb[1], new))
            continue
        mm2 = re.match(r"assigning to '(be16_t|be32_t)' from incompatible type '(.*?)'", msg)
        if mm2 and len(ranges) == 1 and text[off(line, col)] == '=':
            # the left side: back from the '=' to the statement's start
            c0 = off(line, col)
            k = c0 - 1
            while k >= 0 and text[k] not in ';{}\n':
                k -= 1
            lb = (k + 1 + len(text[k + 1:c0]) - len(text[k + 1:c0].lstrip()), c0)
            while lb[1] > lb[0] and text[lb[1] - 1] in ' \t':
                lb = (lb[0], lb[1] - 1)
            ranges = [None, ranges[0]]
            rb = rtext(ranges[1])
        elif mm2 and len(ranges) == 2:
            lb = rtext(ranges[0])
            rb = rtext(ranges[1])
        if mm2 and len(ranges) == 2:
            if lb and rb:
                lhs, rhs = text[lb[0]:lb[1]], text[rb[0]:rb[1]]
                t = re.sub(r'\s*\(aka .*\)', '', mm2.group(2))
                if mm2.group(1) == 'be16_t':
                    new = 'SET16(%s, (short)(int)(%s))' % (lhs, rhs)
                elif t in ('float', 'double'):
                    new = 'SETF(%s, %s)' % (lhs, rhs)
                elif t.endswith('*'):
                    new = 'SETPTR(%s, %s)' % (lhs, rhs)
                else:
                    new = 'SET32(%s, %s)' % (lhs, rhs)
                edits.append((lb[0], rb[1], new))
            continue
        if mm and len(ranges) == 2:
            ta, tb = mm.group(1), mm.group(2)
            for t, other, r in ((ta, tb, ranges[0]), (tb, ta, ranges[1])):
                if t in ('be32_t', 'be16_t', 'bef_t'):
                    o = other if other not in ('be32_t', 'be16_t', 'bef_t') else 'int'
                    if o in ('double',):
                        o = 'float'
                    x = rtext(r)
                    if x:
                        edits.append((x[0], x[1], be_reader(o, t) % text[x[0]:x[1]]))
            continue
        mm = re.match(r"(?:assigning to|initializing|returning) '(.*?)'(?: \(aka '[^']*'\))? (?:from|with an expression of) (?:incompatible )?type '(be32_t|be16_t|bef_t)'", msg)
        if mm and ranges:
            x = rtext(ranges[-1])
            if x:
                b, e = x
                edits.append((b, e, be_reader(mm.group(1), mm.group(2)) % text[b:e]))
            continue
        mm = re.match(r"passing '(be32_t|be16_t|bef_t)' to parameter of incompatible type '(.*?)'", msg)
        if mm and ranges:
            x = rtext(ranges[0])
            if x:
                b, e = x
                edits.append((b, e, be_reader(mm.group(2), mm.group(1)) % text[b:e]))
            continue
        mm = re.match(r"incompatible integer to pointer conversion passing '.*?' (?:\(aka '[^']*'\) )?to parameter of type '(.*?)'", msg)
        if mm and ranges:
            x = rtext(ranges[0])
            if x:
                b, e = x
                edits.append((b, e, 'TGR_PTR(%s, %s)' % (mm.group(1), text[b:e])))
            continue
        mm = re.match(r"incompatible integer to pointer conversion (?:assigning to|initializing) '(.*?)'", msg)
        if mm and ranges:
            x = rtext(ranges[-1])
            if not x:
                continue
            b, e = x
            t = mm.group(1)
            if '(*)' in t:
                edits.append((b, e, 'TGR_FN(%s, %s)' % (t, text[b:e])))
            else:
                edits.append((b, e, 'TGR_PTR(%s, %s)' % (t, text[b:e])))
            continue
        mm = re.match(r"incompatible pointer to integer conversion (?:passing|assigning to|initializing|returning) '(.*?)'", msg)
        if mm and ranges:
            # TGR_PTR(T, addr) given where an address is wanted: the address
            b, e = off(ranges[0][0], ranges[0][1]), off(ranges[0][2], ranges[0][3])
            tm = re.match(r'^TGR_PTR\([^,()]*(?:\([^()]*\))?[^,()]*,\s*(.*)\)$', text[b:e], re.S)
            if tm and 'passing' in msg:
                edits.append((b, e, tm.group(1)))
                continue
            r = ranges[0] if 'passing' in msg else ranges[-1]
            x = rtext(r)
            if not x:
                continue
            b, e = x
            src = text[b:e]
            if src.strip() in impl or '(*)' in mm.group(1) and 'passing' not in msg:
                edits.append((b, e, 'TGR_FA(%s)' % src))
            else:
                edits.append((b, e, 'tgr_addr32(%s)' % src))
            continue
    # apply, last first, skipping overlaps
    edits = sorted(set(edits), key=lambda x: (x[0], x[1]), reverse=True)
    out, last = text, None
    done = 0
    for b, e, new in edits:
        if last is not None and e > last:
            continue
        out = out[:b] + new + out[e:]
        last = b
        done += 1
    if done:
        open(os.path.join(ROOT, path), 'w').write(out)
    return done


BEFLAGS = [f for f in FLAGS if not f.startswith('-Werror')] + ['-Wincompatible-pointer-types']
INPLACE = {'BrVec3Normalise', 'BrVec3ScaleBy', 'BrVec3DivBy', 'BrVec3MulAddTo', 'BrVec3AddTo'}


def befix_file(path):
    """cartridge vectors handed to the vector routines: read through a
    native copy (BRV/BRF/BRM); an output parameter is left for a person"""
    p = subprocess.run(['clang'] + BEFLAGS + [path], capture_output=True, text=True, cwd=ROOT)
    text = open(os.path.join(ROOT, path)).read()
    lines = text.split('\n')
    starts = [0]
    for l in lines[:-1]:
        starts.append(starts[-1] + len(l) + 1)
    diags = p.stderr.split('\n')
    edits, manual = [], []
    for n, d in enumerate(diags):
        m = re.match(r'^(.*?):(\d+):(\d+):(\{\d+:\d+-\d+:\d+\}): warning: incompatible pointer types passing \'(.*?)\' to parameter of type \'(.*?)\'', d)
        if not m or os.path.abspath(os.path.join(ROOT, m.group(1))) != os.path.abspath(os.path.join(ROOT, path)):
            continue
        src, dst = m.group(5), m.group(6)
        r = [int(x) for x in re.findall(r'\d+', m.group(4))]
        b, e = starts[r[0] - 1] + r[1] - 1, starts[r[2] - 1] + r[3] - 1
        arg = text[b:e]
        pname = ''
        if n + 1 < len(diags):
            mm = re.search(r"passing argument to parameter '(\w+)' here", diags[n + 1])
            if mm:
                pname = mm.group(1)
        # the callee's name, back from the argument
        k = b - 1
        depth = 0
        while k >= 0:
            if text[k] == ')':
                depth += 1
            elif text[k] == '(':
                if depth == 0:
                    break
                depth -= 1
            k -= 1
        fm = re.search(r'(\w+)\s*$', text[:k])
        callee = fm.group(1) if fm else ''
        out = re.search(r'out', pname, re.I) or (callee in INPLACE and pname in ('pV', 'v', 'pA', 'a'))
        if out:
            manual.append('%s:%d: %s writes %s (%s)' % (path, r[0], callee, arg, pname))
            continue
        if src.startswith('BrVec3be') and 'BrVec3' in dst:
            edits.append((b, e, 'BRV(%s)' % arg))
        elif src.startswith('be32_t') and '[4]' not in src and dst.startswith('float *'):
            edits.append((b, e, 'BRF(%s)' % arg))
        elif src.startswith('be32_t (*)[4]') or src.startswith('be32_t[4][4]'):
            edits.append((b, e, 'BRM(%s)' % arg))
    edits = sorted(set(edits), reverse=True)
    out_t, last = text, None
    for b, e, new in edits:
        if last is not None and e > last:
            continue
        out_t = out_t[:b] + new + out_t[e:]
        last = b
    if edits:
        open(os.path.join(ROOT, path), 'w').write(out_t)
    for x in manual:
        print('  manual: ' + x)
    return len(edits)


def implemented():
    names = set()
    for dp, dn, fn in os.walk(os.path.join(ROOT, 'ports/tgrally/src')):
        for f in fn:
            if f.endswith('.c'):
                names |= set(re.findall(r'@implements 0x[0-9A-Fa-f]+ tgr (\w+)', open(os.path.join(dp, f)).read()))
    return names


def main():
    impl = implemented()
    for path in sys.argv[1:]:
        path = os.path.relpath(os.path.abspath(path), ROOT)
        total = 0
        for _ in range(8):
            n = fix_file(path, impl)
            total += n
            if not n:
                break
        total += befix_file(path)
        print('%s: %d fixes' % (path, total))


if __name__ == '__main__':
    main()
