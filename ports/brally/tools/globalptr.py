#!/usr/bin/env python3
"""Globals the decompiler typed int that hold pointers.

From the last build's diagnostics: a pointer assigned to a global, a global
compared with a pointer, or a global assigned to a pointer variable marks
that global as a pointer. Each one is recorded in
ports/brally/types/globals_override.csv as `void *` (every file then sees
its own pointer view through its alias header); unify.py --no-remove
regenerates the declarations.

Usage: globalptr.py [--dry]
"""
import csv
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OV = os.path.join(ROOT, 'ports/brally/types/globals_override.csv')


def main():
    os.chdir(ROOT)
    rows = [r for r in csv.DictReader(open('ports/brally/types/globals.csv')) if r['va']]
    va_of = {}
    for r in rows:
        va_of.setdefault(r['name'], r['va'])
    ov = list(csv.DictReader(open(OV))) if os.path.exists(OV) else []
    have = {o['va'] for o in ov}
    found = {}
    for ef in glob.glob('build/brally/null-soft/obj/*.err'):
        t = open(ef, errors='replace').read()
        for m in re.finditer(r"^(\S+?):(\d+):(\d+): error: ([^\n]*)$", t, re.M):
            f, ln, col, msg = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
            try:
                line = open(f, encoding='latin-1').read().split('\n')[ln - 1]
            except (OSError, IndexError):
                continue
            before, after = line[:col - 1], line[col - 1:]
            side = None
            mm = re.match(r"comparison between pointer and integer \('([^']*)'(?: \(aka '[^']*'\))? and '([^']*)'", msg)
            if mm:
                # the int operand is the global that should be a pointer
                left_int = not mm.group(1).rstrip().endswith('*')
                if left_int:
                    w = re.search(r'([A-Za-z_]\w*)\s*\)*\s*$', before)
                else:
                    w = re.match(r'\s*(?:==|!=|<=|>=|<|>)\s*\(*\s*([A-Za-z_]\w*)\b', after)
                side = w.group(1) if w else None
            elif re.match(r"(?:incompatible pointer to integer conversion )?assigning to '(?:int|unsigned int|int32_t|uint32_t)'"
                          r"(?: \(aka '[^']*'\))? from (?:incompatible type )?'[^']*\*'", msg):
                eq = before.rfind('=')
                w = re.search(r'([A-Za-z_]\w*)\s*$', before[:eq]) if eq != -1 and before[eq - 1:eq] not in '=!<>' else None
                side = w.group(1) if w else None
            elif re.match(r"(?:incompatible integer to pointer conversion )?assigning to '[^']*\*'.*from (?:incompatible type )?'(?:int|unsigned int|int32_t|uint32_t)'", msg):
                w = re.match(r'\s*=?\s*\(*\s*([A-Za-z_]\w*)\s*\)*\s*;', after)
                side = w.group(1) if w else None
            if side and side in va_of:
                found.setdefault(side, va_of[side])
    added = []
    for nm, va in sorted(found.items(), key=lambda x: x[1]):
        if va in have:
            continue
        canon = [r for r in rows if r['va'] == va]
        size = max([int(r['size32'] or 4) for r in canon] or [4])
        if size != 4:
            continue        # not a lone 4-byte slot: a person decides
        best = sorted(canon, key=lambda r: (r['name'].startswith(('DAT_', 'g_br', 'g_')), r['name']))[0]['name']
        added.append({'va': va, 'name': best, 'type': 'void *', 'size32': '4',
                      'note': 'holds a pointer (from the code: %s)' % nm})
        have.add(va)
    print('globals that hold pointers: %d new' % len(added))
    for a in added:
        print('  %s %s  (%s)' % (a['va'], a['name'], a['note']))
    if added and '--dry' not in sys.argv:
        with open(OV, 'a', newline='') as fh:
            w = csv.DictWriter(fh, ['va', 'name', 'type', 'size32', 'note'])
            for a in added:
                w.writerow(a)


if __name__ == '__main__':
    main()
