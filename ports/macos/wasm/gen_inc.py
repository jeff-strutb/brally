#!/usr/bin/env python3
"""Stage the MSVC 5 headers the wasm32 port compile needs, patched.

The Windows-build source includes <windows.h> & co from tools/msvc5/include
(extracted from the VC++ disc by setup.sh, never committed). A handful of
their inline functions are x86 inline assembly, which a wasm32 compile
cannot take. This writes patched copies of just those headers into an
overlay directory searched BEFORE tools/msvc5/include; every other header is
used as shipped.

Usage: gen_inc.py <msvc-include-dir> <overlay-dir>
"""
import os
import re
import sys

SRC, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)

C_BODIES = {
    'Int64ShllMod32': '{ return Value << (ShiftCount & 31); }',
    'Int64ShraMod32': '{ return Value >> (ShiftCount & 31); }',
    'Int64ShrlMod32': '{ return Value >> (ShiftCount & 31); }',
}


def find(name):
    for f in os.listdir(SRC):
        if f.lower() == name:
            return os.path.join(SRC, f)
    raise SystemExit('gen_inc: %s not in %s' % (name, SRC))


t = open(find('winnt.h'), 'rb').read().decode('latin-1').replace('\r\n', '\n')
# fs:-segment fiber accessors: drop them (the game never calls them).
t, n1 = re.subn(r'^#if !defined\(MIDL_PASS\) && defined\(_M_IX86\)$',
                '#if 0 /* wasm port: fs:-segment inline asm */', t, flags=re.M)
n2 = 0
for fn, body in C_BODIES.items():
    pat = re.compile(r'(\n%s \(\n[^)]*\)\n)\{\s*__asm\s*\{[^}]*\}\s*\}' % fn)
    t, k = pat.subn(lambda m: m.group(1) + body, t)
    n2 += k
if n1 != 1 or n2 != 3:
    raise SystemExit('gen_inc: winnt.h patch did not apply (%d, %d)' % (n1, n2))
open(os.path.join(OUT, 'winnt.h'), 'w').write(t)
print('gen_inc: winnt.h patched -> %s' % OUT)
