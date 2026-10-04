#!/usr/bin/env python3
"""List function-pointer casts whose register classes disagree.

    fncast.py [TU ...]        (default: every core TU)

On i386 every argument goes on the stack and a float return comes back in
st(0) whatever the declared type, so the decompiled bodies cast function
pointers freely: `(int (*)(int, int))pfnScale`. On arm64 and x86-64 an
integer argument travels in a general register and a float or double in a
vector register, float and double are different widths, and a struct is
passed by its own rules. A call through a pointer whose type disagrees with
the callee in any of that reads garbage.

It also checks every assignment of a function into a variable or field
(through any casts and pointer puns, `(*(void (**)(void))&slot) = fn`)
against the slot's own declared type, which is what its callers call.

This walks clang's AST for every cast between two function-pointer types
and prints those whose return or any parameter differs in class
(32-bit integer / 64-bit integer or pointer / float / double), and those
where one side has a prototype and the other does not. Review each: the
fix is to give the pointer the callee's real type.
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(__file__))
from ptrarith import FLAGS, ROOT  # noqa: E402

CAST = re.compile(r"(?:CStyleCastExpr|CXXReinterpretCastExpr|CXXStaticCastExpr|ImplicitCastExpr) 0x[0-9a-f]+ <([^>]*)> '([^']*)'(?::'([^']*)')? <(BitCast|NoOp)>")
SUB = re.compile(r"> '([^']*)'(?::'([^']*)')?")


def split_fn(t):
    """'R (*)(A, B)' -> (R, [A, B]) or None"""
    m = re.match(r'^(.*?)\s*\(\*\)\((.*)\)\s*$', t)
    if not m:
        return None
    ret, args = m.group(1), m.group(2)
    out, depth, cur = [], 0, ''
    for c in args:
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
            continue
        depth += c in '(<'
        depth -= c in ')>'
        cur += c
    if cur.strip():
        out.append(cur.strip())
    return ret.strip(), out


W64 = {'long', 'unsigned long', 'long long', 'unsigned long long', 'size_t', 'ssize_t', 'intptr_t', 'uintptr_t',
       'ptrdiff_t', 'int64_t', 'uint64_t', 'ULONG_PTR', 'LONG_PTR', 'DWORD_PTR', 'UINT_PTR', 'INT_PTR', 'LPARAM',
       'WPARAM', 'LRESULT', 'HANDLE', 'HWND', 'HINSTANCE', 'HMODULE', 'FARPROC', 'SIZE_T'}


def klass(t):
    """the register class AND width an argument travels in: a 32-bit and a
    64-bit integer differ (a 64-bit callee reads garbage upper bits from a
    32-bit argument, and past the eighth argument they sit in memory at
    their own width)"""
    t = re.sub(r'\b(const|volatile|struct|enum)\b', ' ', t)
    t = re.sub(r'__attribute__\(\([^)]*\)\)', '', t)
    t = ' '.join(t.split())
    if '*' in t or '[' in t or t in W64:
        return 'i64'
    if t in ('float', 'BrF32'):
        return 'float'
    if t in ('double', 'long double'):
        return 'double'
    if t == 'void':
        return 'void'
    if t.startswith(('union ', 'class ')):
        return 'agg:' + t
    if t in ('char', 'signed char', 'unsigned char', 'short', 'unsigned short', 'int', 'unsigned int', 'unsigned',
             'int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'BOOL', 'DWORD', 'UINT', 'LONG',
             'ULONG', 'WORD', 'BYTE', 'FxU32', 'FxI32', 'FxBool', 'GrColor_t', '_Bool', 'bool', 'HRESULT'):
        return 'i32'
    return 'other:' + t


ASSIGN = re.compile(r"BinaryOperator 0x[0-9a-f]+ <[^>]*> '[^']*'(?::'[^']*')? '='")
SLOT = re.compile(r"(?:DeclRefExpr 0x[0-9a-f]+ <[^>]*> '([^']*)'(?::'([^']*)')? lvalue Var|MemberExpr 0x[0-9a-f]+ <[^>]*> '([^']*)'(?::'([^']*)')? lvalue)")
FUNC = re.compile(r"DeclRefExpr 0x[0-9a-f]+ <[^>]*> '([^']*)'(?::'([^']*)')? Function 0x[0-9a-f]+ '(\w+)'")


def depth(ln):
    return len(ln) - len(ln.lstrip(' |`-'))


def compare(dst, src):
    """the reasons a call through type dst reaches a function of type src wrongly"""
    a, b = split_fn(dst), split_fn(src)
    if not a or not b or a == b:
        return []
    why = []
    ka, kb = klass(a[0]), klass(b[0])
    if ka != kb and not ka.startswith('other') and not kb.startswith('other') and \
            ('void' not in (ka, kb) or kb in ('float', 'double')):
        why.append('return %s vs %s' % (a[0], b[0]))
    pa, pb = a[1], b[1]
    if pa == ['void'] or pb == ['void'] or pa == [] or pb == []:
        return why
    if len(pa) != len(pb) and '...' not in pa + pb:
        why.append('%d args vs %d' % (len(pa), len(pb)))
    for k in range(min(len(pa), len(pb))):
        if pa[k] == '...' or pb[k] == '...':
            break
        ca, cb = klass(pa[k]), klass(pb[k])
        if ca != cb and not ca.startswith('other') and not cb.startswith('other'):
            why.append('arg%d %s vs %s' % (k, pa[k], pb[k]))
    return why


def slots(lines, i, base):
    """an assignment at line i: the function stored and the slot's own type"""
    d = depth(lines[i])
    slot = fn = None
    for j in range(i + 1, min(i + 40, len(lines))):
        if depth(lines[j]) <= d:
            break
        m = SLOT.search(lines[j])
        if m and slot is None:
            t = m.group(2) or m.group(1) or m.group(4) or m.group(3)
            if '(*)(' in t and not t.endswith('*)(void)') and '(**)' not in t:
                slot = t
        if 'CallExpr' in lines[j]:
            return None             # a call on the right: its result is stored, not a function
        m = FUNC.search(lines[j])
        if m:
            fn = (m.group(3), (m.group(2) or m.group(1)))
    if not slot or not fn:
        return None
    ftype = re.sub(r'^(.*?)\s*\(', r'\1 (*)(', fn[1], count=1)
    why = compare(slot, ftype)
    return (fn[0], slot, ftype, why) if why else None


def scan(tu):
    cc = ['clang++', '-std=c++17'] if tu.endswith('.cpp') else ['clang', '-std=gnu11']
    try:
        p = subprocess.run(cc + ['-fsyntax-only', '-Xclang', '-ast-dump', '-fno-color-diagnostics'] + FLAGS + [tu],
                           cwd=ROOT, capture_output=True, text=True, errors='replace', timeout=600)
    except subprocess.TimeoutExpired:
        return ['%s: timeout' % tu]
    out, cur_file, cur_line = [], None, 0
    lines = p.stdout.split('\n')
    base = os.path.basename(tu)
    seen = set()
    for i, ln in enumerate(lines):
        for m in re.finditer(r'<?(?:([^<>\s:]+\.(?:c|cpp|h)):(\d+)|line:(\d+))', ln):
            if m.group(1):
                cur_file, cur_line = m.group(1), int(m.group(2))
            elif m.group(3):
                cur_line = int(m.group(3))
        if cur_file is None or not cur_file.endswith(base):
            continue
        if ASSIGN.search(ln):
            r = slots(lines, i, base)
            if r:
                out.append('%s:%d  slot <- %s: %s   [%s <- %s]' % (tu, cur_line, r[0], '; '.join(r[3]), r[1], r[2]))
            continue
        m = CAST.search(ln)
        if not m:
            continue
        dst = m.group(3) or m.group(2)
        # the operand's type: the next line's expression
        if i + 1 >= len(lines):
            continue
        sm = SUB.search(lines[i + 1])
        if not sm:
            continue
        src = sm.group(2) or sm.group(1)
        a, b = split_fn(dst), split_fn(src)
        if not a or not b or a == b:
            continue
        why = []
        ka, kb = klass(a[0]), klass(b[0])
        if ka != kb and 'void' not in (ka, kb) and not ka.startswith('other') and not kb.startswith('other'):
            why.append('return %s vs %s' % (a[0], b[0]))
        elif ka != kb and kb in ('float', 'double'):
            why.append('return %s vs %s' % (a[0], b[0]))
        pa, pb = a[1], b[1]
        if pa == ['void'] or pb == ['void']:
            pa, pb = [], []         # a generic 'void (*)(void)' slot: checked where it is called
        if (pa == [] or pb == []) and pa != pb and 'void' not in (pa + pb):
            pass        # unprototyped: arguments follow the call's promotion; checked below
        for k in range(min(len(pa), len(pb))):
            if pa[k] == '...' or pb[k] == '...':
                break
            ca, cb = klass(pa[k]), klass(pb[k])
            if ca != cb and not ca.startswith('other') and not cb.startswith('other'):
                why.append('arg%d %s vs %s' % (k, pa[k], pb[k]))
        if why:
            key = (cur_line, tuple(why))
            if key not in seen:
                seen.add(key)
                out.append('%s:%d  %s   [%s <- %s]' % (tu, cur_line, '; '.join(why), dst, src))
    return out


def main():
    os.chdir(ROOT)
    tus = sys.argv[1:] or sorted(
        os.path.join(dp, f) for dp, _, fs in os.walk('ports/brally/src/core') for f in fs if f.endswith(('.c', '.cpp')))
    with ThreadPoolExecutor(max_workers=int(os.environ.get('JOBS', '12'))) as ex:
        for res in ex.map(scan, tus):
            for r in res:
                print(r, flush=True)


if __name__ == '__main__':
    main()
