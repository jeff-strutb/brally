#!/usr/bin/env python3
"""Find struct-pointer arithmetic with byte-sized literals in the 64-bit core.

    ptrarith.py [TU ...]        (default: every core TU)

The decompiled bodies often add an original BYTE offset to a pointer that,
in this tree, is typed as a record (`pCar + 0xE28`). C scales that by the
record's size, so the access lands far outside the object; on the original
the variable was an int and the add was a byte add. The compiler accepts it
silently. This walks clang's AST for every `+`/`-` whose result is a
pointer to a record (or to any type wider than a byte) and whose other
operand is an integer literal of at least 8, and prints file:line and the
expression's type. Review each: a genuine element index (`aCars + 2`) is
fine; a byte offset needs the named field.
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
FLAGS = ('-D_FORTIFY_SOURCE=0 -w -fms-extensions -fdeclspec -Iports/64b/platform/include -Iports/64b/include '
         '-include ports/64b/platform/include/win32.h -include ports/64b/platform/include/glide.h '
         '-include ports/64b/platform/include/br_x87.h -include ports/64b/include/br_crt.h '
         '-include ports/64b/include/br_addr32.h -include ports/64b/platform/include/br_lp64.h '
         '-include ports/64b/include/br_globals.h -include ports/64b/include/br_funcs.h').split()
BYTE = re.compile(r"'(?:const )?(?:unsigned )?(?:char|uint8_t|int8_t|void|BYTE|undefined1?|signed char) \*'")
OP = re.compile(r"(BinaryOperator|CompoundAssignOperator) 0x[0-9a-f]+ <([^>]*)> '([^']+\*)'(?::'[^']*')? '(\+|-|\+=|-=)'")
LIT = re.compile(r"IntegerLiteral 0x[0-9a-f]+ <[^>]*> '[^']+' (\d+)")


def scan(tu):
    cc = ['clang++', '-std=c++17'] if tu.endswith('.cpp') else ['clang', '-std=gnu11']
    try:
        p = subprocess.run(cc + ['-fsyntax-only', '-Xclang', '-ast-dump', '-fno-color-diagnostics'] + FLAGS + [tu],
                           cwd=ROOT, capture_output=True, text=True, errors='replace', timeout=600)
    except subprocess.TimeoutExpired:
        return ['%s: timeout' % tu]
    out, cur_file, cur_line, in_main = [], None, 0, False
    lines = p.stdout.split('\n')
    base = os.path.basename(tu)
    for i, ln in enumerate(lines):
        for m in re.finditer(r'<?(?:([^<>\s:]+\.(?:c|cpp|h)):(\d+)|line:(\d+))', ln):
            if m.group(1):
                cur_file, cur_line = m.group(1), int(m.group(2))
            elif m.group(3):
                cur_line = int(m.group(3))
        in_main = cur_file is not None and cur_file.endswith(base)
        m = OP.search(ln)
        if not m or not in_main or BYTE.search("'" + m.group(3) + "'"):
            continue
        # the literal operand: one of the next few child lines at this depth+1
        depth = len(ln) - len(ln.lstrip(' |`-'))
        for j in range(i + 1, min(i + 8, len(lines))):
            d2 = len(lines[j]) - len(lines[j].lstrip(' |`-'))
            if d2 <= depth:
                break
            lm = LIT.search(lines[j])
            if lm and int(lm.group(1)) >= 8:
                out.append('%s:%d  %s %s %s' % (tu, cur_line, m.group(3), m.group(4), lm.group(1)))
                break
    return out


def main():
    os.chdir(ROOT)
    tus = sys.argv[1:] or sorted(
        os.path.join(dp, f) for dp, _, fs in os.walk('ports/64b/src/core') for f in fs if f.endswith(('.c', '.cpp')))
    with ThreadPoolExecutor(max_workers=int(os.environ.get('JOBS', '12'))) as ex:
        for res in ex.map(scan, tus):
            for r in res:
                print(r, flush=True)


if __name__ == '__main__':
    main()
