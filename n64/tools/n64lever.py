"""Try source-truth levers on the T2 functions already in n64/src.

    .venv/bin/python n64/tools/n64lever.py                 # every T2, every lever
    .venv/bin/python n64/tools/n64lever.py --lever fsuf     # one lever
    .venv/bin/python n64/tools/n64lever.py 0x802238B8       # one function

A lever is a whole-function rewrite that the ROM, not taste, decides -- the
Ghidra draft said one thing and the bytes may say another.  Unlike the
permuter's respellings these can change meaning (`0.0` vs `0.0f`), so a lever
is kept only when the function gets strictly closer to the ROM AND no other
function in the file gets further away (project rule 6: surroundings decide
codegen).  The file is rebuilt with n64build.py either way, so verify.csv
stays current.

Levers:
  fsuf   float literals get an `f` (Ghidra prints single constants as double)
  gbi    Ghidra's display-list stores become gRaw(head++, w0, w1)
  lowsym a dereferenced low-RAM constant (0x800xxxxx) is a linked global
  bytes  a global the function offsets (`&D_x + n*K`) is declared `char`, as
         Ghidra meant it (byte arithmetic); the file's other uses keep their
         width through a cast
  rolit  a float/double the ROM only ever loads (never stores) is a literal in
         the source; IDO rebuilds the literal pool from them in source order
  lstatic a scalar global no other ROM function touches is a function-local
         `static` (IDO never CSEs or hoists a local static's address, and
         does for every extern); its ROM .data word is the initialiser, and
         a .bss one has none
"""
import argparse
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
sys.path.insert(0, os.path.join(N64, 'tools'))
import n64build as B  # noqa: E402
import n64gen as G  # noqa: E402
import n64t3 as T  # noqa: E402


def lever_fsuf(src, body):
    return src.replace(body, G.fsuffix(body))


def lever_bytes(src, body):
    for d in sorted(set(re.findall(r'&(D_[0-9A-F]{8})\s*[-+]', body))):
        m = re.search(r'^extern ([\w ]+?)\s*\b%s;$' % d, src, re.M)
        if not m or m.group(1).strip() == 'char':
            continue
        ty = m.group(1).strip()
        head, sep, rest = src.partition('/* -- end declarations -- */')
        if not sep:
            continue
        rest = re.sub(r'(?<!&)\b%s\b' % d, '(*(%s *)&%s)' % (ty, d), rest)
        head = head.replace(m.group(0), 'extern char %s;' % d)
        src = head + sep + rest
    return src


def lever_lowsym(src, body):
    """`(*(T *)0x800xxxxx)` -> a linked global D_800xxxxx of type T."""
    pat = re.compile(r'\(\*\(([\w ]+?) \*\)0x(80[01][0-9A-Fa-f]{5})\)')
    found = {}
    for m in pat.finditer(body):
        found['D_%08X' % int(m.group(2), 16)] = m.group(1).strip()
    if not found:
        return src
    new = pat.sub(lambda m: 'D_%08X' % int(m.group(2), 16), body)
    new = re.sub(r'\(\(([\w ]+?) \*\)0x(80[01][0-9A-Fa-f]{5})\)',
                 lambda m: '((%s *)&D_%08X)' % (m.group(1), int(m.group(2), 16)), new)
    src = src.replace(body, new)
    decl = ''.join('extern %s %s;\n' % (t, n) for n, t in sorted(found.items())
                   if not re.search(r'\b%s;' % n, src))
    return src.replace('/* -- end declarations -- */', decl + '/* -- end declarations -- */', 1)


def lever_gbi(src, body):
    """Ghidra's display-list stores -> gRaw(head++, w0, w1)."""
    new, heads = G.gbi(body)
    if not heads:
        return src
    src = src.replace(body, new)
    for h in heads:
        src = re.sub(r'^extern [\w ]+\*?\s*%s;$' % h, 'extern Gfx *%s;' % h, src, flags=re.M)
    if 'tgr/gbi.h' not in src:
        src = src.replace('#include "tgr/common.h"', '#include "tgr/common.h"\n#include "tgr/gbi.h"', 1)
    return src


CUR = {}


def lever_rolit(src, body):
    lits = G.rodata_literals(B.Rom())

    def lit(m):
        v = lits.get(int(m.group(1), 16))
        if v is None:
            return m.group(0)
        return '(%s)' % v if v.startswith('-') else v
    nbody = re.sub(r'\bD_([0-9A-F]{8})\b', lit, body)
    if nbody == body:
        return src
    src = src.replace(body, nbody, 1)
    for d in set(re.findall(r'\bD_[0-9A-F]{8}\b', body)) - set(re.findall(r'\bD_[0-9A-F]{8}\b', nbody)):
        if len(re.findall(r'\b%s\b' % d, src)) == 1:           # only its extern is left
            src = re.sub(r'^extern [\w ]+\b%s;\n' % d, '', src, flags=re.M)
    return src


def lever_lstatic(src, body):
    va = CUR.get('va')
    refs = B.rom_refs()
    rom = B.Rom()
    head, sep, rest = src.partition('/* -- end declarations -- */')
    if not sep:
        return src
    decls = []
    for d in sorted(set(re.findall(r'\b(D_([0-9A-F]{8}))\b', body))):
        name, addr = d[0], int(d[1], 16)
        m = re.search(r'^extern ([\w ]+?)\s*\b%s;\n' % name, head, re.M)
        if not m or refs.get(addr, set()) - {va} or addr in G.rodata_literals(rom):
            continue                                # shared, or a read-only literal
        if re.search(r'\b%s\b' % name, rest.replace(body, '')):
            continue                                # the file uses it elsewhere
        ty = m.group(1).strip()
        width = {'char': 1, 'unsigned char': 1, 'short': 2, 'unsigned short': 2}.get(ty, 4)
        if ty in ('double', 'long long', 'unsigned long long'):
            continue
        init = ''
        if addr < B.BSS_S:
            raw = rom.bytes(addr, width)
            if ty == 'float':
                import struct
                init = ' = %rf' % struct.unpack('>f', raw)[0]
            else:
                init = ' = %d' % int.from_bytes(raw, 'big', signed=not ty.startswith('unsigned'))
        head = head.replace(m.group(0), '', 1)
        decls.append('  static %s %s%s;\n' % (ty, name, init))
    if not decls:
        return src
    i = body.index('{\n') + 2
    nbody = body[:i] + ''.join(decls) + body[i:]
    return head + sep + rest.replace(body, nbody, 1)


LEVERS = {'lstatic': lever_lstatic, 'rolit': lever_rolit, 'fsuf': lever_fsuf, 'bytes': lever_bytes, 'lowsym': lever_lowsym, 'gbi': lever_gbi}


def grade_file(path):
    """-> {va: ndiff} for every tagged function in path (0 = EXACT)."""
    out = subprocess.run([sys.executable, os.path.join(N64, 'tools/n64build.py'), path,
                          '--csv', os.devnull], cwd=ROOT, capture_output=True, text=True).stdout
    res = {}
    for line in out.split('\n'):
        p = line.split()
        if len(p) >= 3 and len(p[0]) == 8 and p[1] in ('EXACT', 'DIFF', 'CCFAIL', 'ERROR'):
            try:
                va = int(p[0], 16)
            except ValueError:
                continue
            res[va] = 0 if p[1] == 'EXACT' else (int(p[2]) if p[2].isdigit() else 10 ** 6)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('vas', nargs='*')
    ap.add_argument('--lever', action='append', choices=sorted(LEVERS))
    a = ap.parse_args()
    levers = a.lever or sorted(LEVERS)
    if a.vas:
        vas = [int(v, 16) for v in a.vas]
    else:
        vas = [int(r['va'], 16) for r in csv.DictReader(open(os.path.join(B.OUT, 'verify.csv')))
               if r['va'] and r['status'] == 'DIFF']
    touched = set()
    for va in vas:
        path, name, _ = T.source_of(va)
        if not path:
            continue
        for lv in levers:
            src = open(path).read()
            body = T.function_text(src, name)
            CUR['va'] = va
            new = LEVERS[lv](src, body)
            if new == src:
                continue
            before = grade_file(path)
            open(path, 'w').write(new)
            after = grade_file(path)
            worse = [v for v in before if after.get(v, 10 ** 6) > before[v]]
            if after.get(va, 10 ** 6) < before.get(va, 10 ** 6) and not worse:
                print('%08X %-28s %-5s %d -> %d%s' % (va, name, lv, before[va], after[va],
                                                   '  EXACT' if after[va] == 0 else ''))
                touched.add(path)
            else:
                open(path, 'w').write(src)
    for path in sorted(touched):
        subprocess.run([sys.executable, os.path.join(N64, 'tools/n64build.py'), path, '-q'], cwd=ROOT)


if __name__ == '__main__':
    main()
