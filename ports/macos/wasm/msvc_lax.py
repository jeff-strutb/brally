#!/usr/bin/env python3
"""Make a TU that MSVC 5 accepts compile under clang (wasm32) -- port-side.

MSVC's C compiler warns where clang errors: conflicting prototypes, calls with
the wrong number of arguments, redeclared externs of a different type, `return
voidcall();` in an int function, and it takes x86 inline assembly and SEH.
The decomp source is written against MSVC and is NEVER edited for the port.
Instead this preprocesses the pristine file into build/, compiles that text,
and applies one mechanical rewrite per diagnostic clang reports, repeating
until the text compiles:

  conflicting types            the non-definition of the pair becomes a K&R
                               declaration (same return type, no prototype)
  too many / too few args      the call goes through an unprototyped cast
  redeclaration, other type    the later extern declaration is dropped
  return of void expression    `return e;` -> `return (e), 0;`
  C++ `T x[];`                 -> `extern T x[];`
  C++ operator new(unsigned)   declaration dropped (implicitly declared)
  C++ void* = function         explicit (void *) cast
  x87 `fld a / fistp b`        `b = w_fistp(a)` (round-to-nearest, as x87)
  SEH __try/__except/__finally the guarded body runs; the handler never
                               does; a __finally block runs after it

Each rewrite is logged beside the output so what the port compiled is
reviewable against the source.

Usage: msvc_lax.py <clang> <src> <out.i> -- <clang flags...>
"""
import re
import subprocess
import sys

# Per-file fixes to the RAW text before preprocessing, for defects that are
# not MSVC-vs-clang differences but plain breakage in the tree.
RAW_FIXES = {
    # A comment block closes early ("... census no (...) */") and the next
    # lines of notes become code; MSVC fails on it too (the sweep's COMPIL 1).
    'src/core/generated/0x100311C0.c': [
        (r'\n \* 2026-09-13: A3 is down', '\n/*\n * 2026-09-13: A3 is down'),
    ],
}


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, p.stderr


def stmt_span(lines, ln):
    """(start, end) line indices of the declaration statement at line ln --
    back to the previous statement's end (headers split one declaration
    over several lines: WINUSERAPI / LONG / WINAPI / GetWindowLongA(...)."""
    s = ln
    while s > 0:
        p = lines[s - 1].rstrip()
        if not p or p.endswith((';', '}', '{')) or p.lstrip().startswith('#'):
            break
        s -= 1
    e = ln
    while e < len(lines) and ';' not in lines[e] and '{' not in lines[e]:
        e += 1
    return s, e


def depth_at(lines, ln):
    """Brace depth at the start of line ln (0 = file scope). Good enough on
    preprocessed text: strings holding braces are rare and only make a line
    look nested, which errs toward NOT rewriting it as a declaration."""
    d = 0
    for l in lines[:ln]:
        d += l.count('{') - l.count('}')
    return d


def is_definition(lines, ln):
    s, e = stmt_span(lines, ln)
    seg = ' '.join(lines[s:e + 1])
    b = seg.find('{')
    c = seg.find(';')
    return b != -1 and (c == -1 or b < c)


def knr_of(lines, ln, name):
    """Turn the prototype starting at line ln into `RET name();`."""
    s, e = stmt_span(lines, ln)
    seg = '\n'.join(lines[s:e + 1])
    i = seg.find(name)
    j = seg.find('(', i)
    # matching paren
    depth, k = 0, j
    while k < len(seg):
        if seg[k] == '(':
            depth += 1
        elif seg[k] == ')':
            depth -= 1
            if depth == 0:
                break
        k += 1
    head = seg[:i].split(';')[-1].split('}')[-1]
    new = seg[:i] + name + '()' + seg[k + 1:]
    lines[s:e + 1] = new.split('\n') + [''] * 0
    return head.strip()


def ret_type_at(lines, ln, name):
    s, e = stmt_span(lines, ln)
    seg = ' '.join(lines[s:e + 1])
    i = seg.find(name)
    head = seg[:i]
    head = re.sub(r'__declspec\([^)]*\)', '', head)
    head = re.sub(r'\b(extern|static|__stdcall|__cdecl|__fastcall|_stdcall|'
                  r'_cdecl|_fastcall|WINAPI|__inline|inline|APIENTRY)\b', '', head)
    head = head.split(';')[-1].split('}')[-1].strip()
    return head or 'int'


def main():
    clang, src, out = sys.argv[1], sys.argv[2], sys.argv[3]
    flags = sys.argv[sys.argv.index('--') + 1:]
    text = open(src, encoding='latin-1').read()
    log = []
    for pat, rep in RAW_FIXES.get(src, []):
        text, n = re.subn(pat, rep, text, count=1)
        log.append('raw fix %r x%d' % (pat, n))
    raw = out + '.src'
    open(raw, 'w', encoding='latin-1').write(text)
    import os
    rc, err = run([clang] + flags + ['-E', '-P', '-I', os.path.dirname(src),
                                     raw, '-o', out])
    if rc:
        sys.stderr.write(err)
        sys.exit(1)
    t = open(out, encoding='latin-1').read()
    # x87 round-to-int and SEH, textually
    t, n1 = re.subn(r'__asm\s*\{\s*fld\s+([^}]+?)\s*\}\s*__asm\s*\{\s*fistp\s+([^}]+?)\s*\}',
                    r'(\2) = w_fistp(\1);', t)
    t, n2 = re.subn(r'__asm\s+fld\s+(\w+)\s*__asm\s+fistp\s+(\w+)',
                    r'(\2) = w_fistp(\1);', t)
    t, n3 = re.subn(r'\b__try\b', '', t)
    t, n4 = re.subn(r'\b__except\s*\(', 'if (0 && (', t)
    if n4:
        # close the extra paren we opened: __except (expr) -> if (0 && (expr))
        t = re.sub(r'if \(0 && \((.*?)\)\s*\{', r'if (0 && (\1)) {', t)
    if n1 or n2:
        t = 'extern int w_fistp(double);\n' + t
    t = t.replace('GetExceptionInformation()', '0').replace('_exception_info()', '0')
    t, n5 = re.subn(r'\b__finally\b', '', t)   # the finally block just runs
    if n5:
        log.append('__finally x%d' % n5)
    for n, what in ((n1 + n2, 'x87 fld/fistp'), (n3, '__try'), (n4, '__except')):
        if n:
            log.append('%s x%d' % (what, n))
    lines = t.split('\n')
    for rnd in range(60):
        open(out, 'w', encoding='latin-1').write('\n'.join(lines))
        rc, err = run([clang] + flags + ['-fsyntax-only', '-x',
                                         'c++-cpp-output' if src.endswith('.cpp')
                                         else 'cpp-output', out])
        if rc == 0:
            break
        diags = re.findall(r'^[^:\n]*:(\d+):(\d+): (error|note): (.*)$', err, re.M)
        fixed = False
        i = 0
        while i < len(diags):
            ln, col, kind, msg = diags[i]
            ln, col = int(ln) - 1, int(col) - 1
            note = diags[i + 1] if i + 1 < len(diags) and diags[i + 1][2] == 'note' else None
            i += 1
            if kind != 'error':
                continue
            m = re.match(r"conflicting types for '(\w+)'", msg)
            if m and note:
                name = m.group(1)
                nl = int(note[0]) - 1
                # only a FILE-SCOPE declaration is ever rewritten; a clash
                # reported inside a body is a call site, handled as a call
                cand = [x for x in (nl, ln) if depth_at(lines, x) == 0
                        and not is_definition(lines, x)]
                if not cand:
                    k = lines[ln].find(name)
                    if k >= 0 and depth_at(lines, ln) > 0:
                        lines[ln] = (lines[ln][:k] + '((%s (*)())%s)' % (
                            ret_type_at(lines, nl, name), name) + lines[ln][k + len(name):])
                        log.append('unprototyped call %s line %d (clash)' % (name, ln + 1))
                        fixed = True
                        break
                    sys.stderr.write(err)
                    sys.exit(1)
                tgt = cand[0]
                if lines[tgt].rstrip().endswith('();') or '%s()' % name in lines[tgt]:
                    # already K&R: drop it outright
                    s, e = stmt_span(lines, tgt)
                    for k in range(s, e + 1):
                        lines[k] = ''
                    log.append('drop decl %s line %d' % (name, tgt + 1))
                else:
                    knr_of(lines, tgt, name)
                    log.append('K&R %s line %d' % (name, tgt + 1))
                fixed = True
                break
            m = re.match(r"too (many|few) arguments to function call", msg)
            if m:
                # find the callee name: last identifier followed by '(' before col
                seg = lines[ln][:col]
                cands = list(re.finditer(r'\b(\w+)\s*\(', seg))
                # the call whose args contain col: scan from the right
                name = None
                for c in reversed(cands):
                    depth = 0
                    for ch in lines[ln][c.end() - 1:col]:
                        depth += ch == '('
                        depth -= ch == ')'
                    if depth > 0:
                        name, cs = c.group(1), c.start(1)
                        break
                decl = None
                if note:
                    decl = int(note[0]) - 1
                if name is None:
                    break
                rt = ret_type_at(lines, decl, name) if decl is not None else 'int'
                lines[ln] = (lines[ln][:cs] + '((%s (*)())%s)' % (rt, name)
                             + lines[ln][cs + len(name):])
                log.append('unprototyped call %s line %d' % (name, ln + 1))
                fixed = True
                break
            m = re.match(r"redeclaration of '(\w+)' with a different type", msg)
            if m:
                s, e = stmt_span(lines, ln)
                for k in range(s, e + 1):
                    lines[k] = ''
                log.append('drop redecl %s line %d' % (m.group(1), ln + 1))
                fixed = True
                break
            if msg.startswith("returning 'void' from a function with incompatible"):
                seg = lines[ln]
                k = seg.rfind('return', 0, col + 1)
                j = seg.find(';', col)
                lines[ln] = seg[:k] + 'return (' + seg[k + 6:j] + '), 0' + seg[j:]
                log.append('void return line %d' % (ln + 1))
                fixed = True
                break
            if msg.startswith('definition of variable with array type needs'):
                lines[ln] = 'extern ' + lines[ln]
                log.append('extern array line %d' % (ln + 1))
                fixed = True
                break
            if "'operator new' takes type size_t" in msg:
                lines[ln] = '/* w2c: dropped */'
                log.append('drop operator new decl line %d' % (ln + 1))
                fixed = True
                break
            m = re.match(r"assigning to 'void \*' from incompatible type", msg)
            if m:
                seg = lines[ln]
                k = seg.find('=', 0)
                lines[ln] = seg[:k + 1] + ' (void *)' + seg[k + 1:]
                log.append('void* cast line %d' % (ln + 1))
                fixed = True
                break
        if not fixed:
            sys.stderr.write(err)
            sys.exit(1)
    else:
        sys.stderr.write('msvc_lax: gave up after 60 rounds\n' + err)
        sys.exit(1)
    open(out + '.log', 'w').write('\n'.join(log) + '\n')


if __name__ == '__main__':
    main()
