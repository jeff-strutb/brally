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

VIRTUAL CALLS TAKE THE ORIGINAL'S VTABLE SLOTS.  Both clang views use the
Itanium C++ ABI, where a virtual destructor fills TWO vtable slots (complete
and deleting) and `delete p` calls the second with `this` alone.  MSVC gives
it ONE slot, the scalar deleting destructor, called with (this, 1); every
virtual after it sits one slot lower.  The objects the port runs on carry
the ORIGINAL vtables (w2c.py maps each constructor's vtable to the MSVC one),
so a clang-numbered call lands on the wrong function -- leaving the options
page called a file scan with no pattern.  Given a third view, the same TU
compiled for i686-pc-windows-msvc (the Microsoft ABI), each virtual call in
the wasm IR takes that view's slot index and any extra constant arguments.

POINTERS TO MEMBER FUNCTIONS ARE MSVC'S TOO.  The tree reads them straight
out of original vtables (`add = *(AddPmf *)(vt + 0x10)`, reproducing the
original's codegen), where MSVC's single-inheritance form is one code
pointer.  clang's is two words, the second an adjustment and virtual flag,
so the port read the NEXT slot as an adjustment and called with a wild
`this`.  Every member-pointer call's second word is taken as 0: no
adjustment, not virtual -- the MSVC reading.

Usage: ccmark.py <wasm.ll> <x86.ll> <out.ll> [<msvc.ll>]
"""
import re
import sys

MAGIC = 0x7EC00000
CALL = re.compile(r'\bcall\b(.*?)\s(%[\w.]+)\((.*)\)(.*)$')
DEF = re.compile(r'^define\b[^@]*@("(?:[^"\\]|\\.)*"|[\w.$]+)\(')


def norm(n):
    n = n.strip('"')
    if n.startswith('\\01'):
        n = n[3:]
        # ONE decoration character ('_' or '@'); C++ names start _Z, so
        # stripping every '_' lost them and no C++ call was ever marked.
        if n[:1] in ('_', '@'):
            n = n[1:]
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


GEP = re.compile(r'^\s*(%[\w.]+) = getelementptr inbounds ptr, ptr (%[\w.]+), i(?:32|64) (\d+)\s*$')
LOADP = re.compile(r'^\s*(%[\w.]+) = load ptr, ptr (%[\w.]+)(, align \d+)?\s*$')


def msvc_qual(sym):
    """MSVC decorated name -> 'Class::Method' (w2c.py's reading)."""
    sym = sym.strip('"').replace('\\01', '')
    m = re.match(r'\?\?([01])([A-Za-z_]\w*)@@', sym)
    if m:
        c = m.group(2)
        return '%s::%s%s' % (c, '~' if m.group(1) == '1' else '', c)
    m = re.match(r'\?([A-Za-z_]\w*)@(?:([A-Za-z_]\w*)@)?@', sym)
    if m:
        return '%s::%s' % (m.group(2), m.group(1)) if m.group(2) else m.group(1)
    return sym.lstrip('_@').split('@')[0]


def vcall(lines, ci):
    """For the indirect call at line ci: (vtable reg, slot, load line, gep
    line or None) when its callee is a slot loaded from a vtable, else None."""
    m = CALL.search(lines[ci])
    f = m.group(2)
    for j in range(ci - 1, max(ci - 8, -1), -1):
        lm = LOADP.match(lines[j])
        if lm and lm.group(1) == f:
            ptr = lm.group(2)
            for k in range(j - 1, max(j - 6, -1), -1):
                gm = GEP.match(lines[k])
                if gm and gm.group(1) == ptr:
                    return gm.group(2), int(gm.group(3)), j, k
            return ptr, 0, j, None
    return None


def msvc_slots(wl, ms_path):
    """Rewrite wl's virtual calls to the Microsoft ABI's slots. Returns the
    number of calls changed."""
    import subprocess
    import os
    try:
        ml = open(ms_path).read().split('\n')
    except OSError:
        return 0
    wi, mi = indirect_calls(wl), indirect_calls(ml)
    llvm = os.environ.get('LLVM', '/opt/homebrew/opt/emscripten/libexec/llvm/bin')
    names = [n for n in wi if n.startswith('_Z')]
    dem = {}
    if names:
        out = subprocess.run([os.path.join(llvm, 'llvm-cxxfilt')], input='\n'.join(names),
                             capture_output=True, text=True).stdout.split('\n')
        dem = {n: re.sub(r'\(.*$', '', d).strip() for n, d in zip(names, out)}
    # MSVC functions by qualified name, unique only
    mby = {}
    for n in mi:
        mby.setdefault(msvc_qual(n), []).append(n)
    wby = {}
    for n in wi:
        wby.setdefault(dem.get(n, n), []).append(n)
    changed, fresh = 0, 0
    edits = {}            # line index -> replacement text (may hold 2 lines)
    for q, wnames in wby.items():
        mnames = mby.get(q)
        if not mnames or len(mnames) != 1:
            continue
        mcalls = mi[mnames[0]]
        cands = [n for n in wnames if len(wi[n]) == len(mcalls)]
        if len(cands) != 1:
            continue
        for wci, mci in zip(wi[cands[0]], mcalls):
            wv, mv = vcall(wl, wci), vcall(ml, mci)
            if not wv or not mv:
                continue
            wargs = split_args(CALL.search(wl[wci]).group(3))
            margs = split_args(CALL.search(ml[mci]).group(3))
            extra = margs[len(wargs):] if len(margs) > len(wargs) else []
            if any(not re.search(r'\bi32\b.*\s-?\d+\s*$', a) for a in extra):
                continue            # only constant ints (the deleting flag)
            if wv[1] == mv[1] and not extra:
                continue
            vt, _, lj, gk = wv
            fresh += 1
            g = '%%brms.%d' % fresh
            lm = LOADP.match(wl[lj])
            edits[lj] = '  %s = getelementptr inbounds ptr, ptr %s, i32 %d\n  %s = load ptr, ptr %s%s' % (
                g, vt, mv[1], lm.group(1), g, lm.group(3) or '')
            if extra:
                cm = CALL.search(wl[wci])
                pre = cm.group(1)
                if re.search(r'\(.*\)\s*$', pre):
                    pre = re.sub(r'\((.*?)\)(\s*)$', lambda z: '(' + z.group(1) + ''.join(
                        ', i32' for _ in extra) + ')' + z.group(2), pre)
                args = cm.group(3) + ''.join(', ' + re.sub(r'\bnoundef\s+', '', a.strip()) for a in extra)
                edits[wci] = wl[wci][:cm.start()] + 'call' + pre + ' ' + cm.group(2) + '(' + args + ')' + cm.group(4)
            changed += 1
    for i, t in edits.items():
        wl[i] = t
    return changed


MPADJ = re.compile(r'^(\s*)(%[\w.]+) = extractvalue \{ i32, i32 \} %[\w.]+, 1\s*$')


def msvc_memptrs(wl):
    """A member-function pointer's second word, where the call sequence
    shifts it for the adjustment, becomes the constant 0."""
    n = 0
    for i, l in enumerate(wl):
        m = MPADJ.match(l)
        if not m:
            continue
        v = re.escape(m.group(2))
        if any(re.search(r'= ashr i32 %s, 1\b' % v, wl[j]) for j in range(i + 1, min(i + 4, len(wl)))):
            wl[i] = '%s%s = add i32 0, 0' % (m.group(1), m.group(2))
            n += 1
    return n


def main():
    wl = open(sys.argv[1]).read().split('\n')
    xl = open(sys.argv[2]).read().split('\n')
    remapped = msvc_slots(wl, sys.argv[4]) if len(sys.argv) > 4 else 0
    memptrs = msvc_memptrs(wl) if len(sys.argv) > 4 else 0
    wl = '\n'.join(wl).split('\n')
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
    # File-static data is NOT private to its file in this tree: the decomp
    # models scattered original globals as one TU-static block (br_sceneprops.c's
    # s17_tuState) or declares a static table with no contents that stands for
    # the original's (s17_colAA5D0), and the linker maps each to the original
    # address, where other code reads and writes it.  At -O2 LLVM would split
    # such a block into scalars and fold loads of fields it never sees written.
    # Listing every internal global in llvm.compiler.used takes its address,
    # so every access stays a real load or store.
    statics = [m.group(1) for m in
               (re.match(r'^(@[\w.$"\\]+) = internal (?:unnamed_addr )?(?:global|constant)\b', l)
                for l in wl) if m]
    if statics and not any(l.startswith('@llvm.compiler.used') for l in wl):
        wl.append('@llvm.compiler.used = appending global [%d x ptr] [%s], section "llvm.metadata"'
                  % (len(statics), ', '.join('ptr ' + g for g in statics)))
    # -O0 frontend output is optnone; the port wants it optimised
    out = '\n'.join(wl)
    out = re.sub(r'\boptnone\b', '', out)
    open(sys.argv[3], 'w').write(out)
    if marked:
        print('ccmark: %s: %d indirect call(s) marked' % (sys.argv[1], marked), file=sys.stderr)
    if remapped:
        print('ccmark: %s: %d virtual call(s) on MSVC slots' % (sys.argv[1], remapped), file=sys.stderr)
    if memptrs:
        print('ccmark: %s: %d member-pointer call(s) read as MSVC' % (sys.argv[1], memptrs), file=sys.stderr)


if __name__ == '__main__':
    main()
