#!/usr/bin/env python3
"""stub_uncompilable.py -- iteratively replace the body of any FUN_ that fails
to compile with a no-op stub, until drive_sandbox.c compiles clean.

The clean ~half keeps real bodies and real constants; the messy half becomes
`return 0` stubs. Prints how many survived so the honesty of the result is
visible (a race driven by a half-stubbed physics is not the real game).
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'ports', 'macos', 'drive_sandbox.c')
CC = ['clang', '-std=c99', '-w', '-ferror-limit=0', '-Wno-int-conversion',
      '-Wno-implicit-function-declaration', '-Wno-int-to-pointer-cast',
      '-c', SRC, '-o', '/dev/null']

def func_spans(text):
    """Return (va, hdr_start_idx, body_open_idx, body_close_idx) for each
    `static ... FUN_<va>(...)` DEFINITION with a `{...}` body, brace-matched.

    Handles both prototype-style `FUN_x(params)\n{` and OLD-STYLE (K&R)
    `FUN_x(names) type name; ... {` definitions the generator now emits. A
    PROTOTYPE (`FUN_x(...);`) is skipped via the `(?!\s*;)` after `)`."""
    spans = []
    for m in re.finditer(
            r'^static [^\n]*?\bFUN_([0-9a-f]{8})\s*\([^)]*\)\s*(?!\s*;)[^{]*?\{',
            text, re.M):
        va = m.group(1)
        open_br = text.index('{', m.start())
        depth = 0; i = open_br
        while i < len(text):
            c = text[i]
            if c == '{': depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    spans.append((va, m.start(), open_br, i)); break
            i += 1
    return spans

def line_to_offset(text, line):
    return sum(len(l) + 1 for l in text.split('\n')[:line - 1])

def compile_errors():
    r = subprocess.run(CC, capture_output=True, text=True)
    return r.stderr

def main():
    stubbed = set()
    for it in range(40):
        text = open(SRC).read()
        err = compile_errors()
        elines = sorted(set(int(m.group(1))
                            for m in re.finditer(r'drive_sandbox\.c:(\d+):', err)))
        if not elines:
            spans = func_spans(text)
            total = len(spans)
            print(f"CLEAN after {it} passes. stubbed {len(stubbed)} of "
                  f"{total + len(stubbed)} functions "
                  f"({100*(total)//(total+len(stubbed))}% real bodies kept)")
            return 0
        spans = func_spans(text)
        # offset of each error line
        offs = [line_to_offset(text, l) for l in elines]
        # which functions contain an error
        to_stub = []
        for va, hs, bo, bc in spans:
            if va in stubbed:
                continue
            if any(bo <= o <= bc for o in offs):
                to_stub.append((va, hs, bo, bc))
        if not to_stub:
            # errors not inside a locatable body: often "conflicting types for
            # 'FUN_x'" (proto vs def). Stub any FUN_ named in such messages.
            named = set(re.findall(r"conflicting types for 'FUN_([0-9a-f]{8})'", err))
            named |= set(re.findall(r"types for 'FUN_([0-9a-f]{8})'", err))
            hit = [(va, hs, bo, bc) for (va, hs, bo, bc) in spans
                   if va in named and va not in stubbed]
            if hit:
                hit.sort(key=lambda s: s[1], reverse=True)
                for va, hs, bo, bc in hit:
                    hdr = text[hs:bo]
                    ret = hdr.split('FUN_')[0].replace('static', '').strip()
                    body = '{ return 0; }' if ret not in ('void',) else '{ }'
                    text = text[:bo] + body + text[bc+1:]
                    stubbed.add(va)
                open(SRC, 'w').write(text)
                print(f"pass {it}: stubbed {len(hit)} conflicting "
                      f"(total {len(stubbed)})", file=sys.stderr)
                continue
            # conflicting types whose definition func_spans still cannot bound:
            # drop the conflicting forward-declaration line instead.
            if named:
                new = []
                dropped = 0
                for ln in text.split('\n'):
                    mm = re.match(r'\s*static .*\bFUN_([0-9a-f]{8})\s*\(\s*\)\s*;\s*$', ln)
                    if mm and mm.group(1) in named:
                        dropped += 1
                        continue
                    new.append(ln)
                if dropped:
                    text = '\n'.join(new)
                    open(SRC, 'w').write(text)
                    print(f"pass {it}: dropped {dropped} conflicting protos",
                          file=sys.stderr)
                    continue
            print(f"no progress at pass {it}; {len(elines)} errors remain "
                  f"outside any function body (global scope). Showing 8:",
                  file=sys.stderr)
            for l in elines[:8]:
                print("  " + text.split('\n')[l-1].strip()[:100], file=sys.stderr)
            return 1
        # rewrite from the end so offsets stay valid
        to_stub.sort(key=lambda s: s[1], reverse=True)
        for va, hs, bo, bc in to_stub:
            hdr = text[hs:bo]
            ret = hdr.split('FUN_')[0].replace('static', '').strip()
            body = '{ return 0; }' if ret not in ('void',) else '{ }'
            text = text[:bo] + body + text[bc+1:]
            stubbed.add(va)
        open(SRC, 'w').write(text)
        print(f"pass {it}: stubbed {len(to_stub)} more "
              f"(total {len(stubbed)}), {len(elines)} errors", file=sys.stderr)
    print("gave up after 40 passes", file=sys.stderr)
    return 1

sys.exit(main())
