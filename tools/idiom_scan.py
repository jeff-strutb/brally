#!/usr/bin/env python3
"""Bucket every report.csv `diff` by two proven mechanical width idioms.

Dry-run detector. Does not edit source.

The two signatures (proven, then exhausted on FUN_10002830 / FUN_10027710
and BrTextBoxMeasureB / FUN_100298c0):

  byte     orig loads a byte arg as `mov al,[mem]; push eax` (opcodes
           a0/8a into a byte reg). Recomp with the param typed int emits
           movzx / mov eax (and Ghidra CONCAT leftovers). Fix: callee
           param `unsigned char`.

  word16   orig stores/loads a 16-bit local with a 66-prefixed op
           (66 89 / 66 8b, movsx ax, cmp ax,imm). Recomp with the local
           typed int emits the 32-bit form. Fix: local `short` /
           `unsigned short`.

A function is bucketed `byte` or `word16` only when those paired extra/
missing shapes DOMINATE the register-blind (regnorm) gap at >=70%.
Anything else is `neither`. Gap 0 (identical shapes, different bytes) is
`regalloc` — a coloring residue, not a width class.

A bucket with 1-2 members is not a class. A bucket with 8+ homogeneous
members is the bulk win. Empty / heterogeneous is a valid negative
result: do not force fixes.

Compile: same wine+cl path as tools/fnmatch/fn.py, grouped per
(file, opt) so a 500-function scan is ~100 compiles not 500. Fresh
obj_{opt} from the last sweep are reused; --force recompiles.

    .venv/bin/python tools/idiom_scan.py              # dry-run table
    .venv/bin/python tools/idiom_scan.py --detail     # + extra/missing
    .venv/bin/python tools/idiom_scan.py --csv PATH
    .venv/bin/python tools/idiom_scan.py --va 0x...
    .venv/bin/python tools/idiom_scan.py --force      # recompile every file
"""
from __future__ import print_function

import argparse
import csv
import os
import re
import sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from match_diff import parse_coff_obj  # noqa: E402
from match_sweep import VARIANTS, compile_variant  # noqa: E402
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.skipdata = True

REPORT = os.path.join(ROOT, 'build', 'match', 'report.csv')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
THRESHOLD = 0.70

# Same normalisation as tools/fnmatch/fn.py and mdiff2.py.
R32 = r'\b(eax|ebx|ecx|edx|esi|edi|ebp)\b'
R16 = r'\b(ax|bx|cx|dx|si|di|bp)\b'
R8 = r'\b(al|bl|cl|dl|ah|bh|ch|dh)\b'

_OPT = dict(VARIANTS)  # tag -> cl flags


def norm(t, mode):
    t = re.sub(r'esp [+-] 0x[0-9a-f]+', 'esp+S', t)
    t = re.sub(r'0x[0-9a-f]+', 'I', t)
    t = re.sub(r'\b\d+\b', 'I', t)
    if mode in ('regnorm', 'widthnorm'):
        t = re.sub(R32, 'R', t)
        t = re.sub(R16, 'W', t)
        t = re.sub(R8, 'B', t)
    if mode == 'widthnorm':
        t = re.sub(r'\b[WB]\b', 'R', t)
        t = re.sub(r'\b(byte|word|dword) ptr\b', 'ptr', t)
        t = re.sub(r'\bmovzx\b|\bmovsx\b', 'mov', t)
    return t


def bag(code, mode):
    c = Counter()
    for i in md.disasm(code, 0):
        c[norm('%s %s' % (i.mnemonic, i.op_str), mode)] += 1
    return c


def strip_pad(b):
    while b and b[-1] == 0x90:
        b = b[:-1]
    return b


def width_key(shape):
    return norm(shape, 'widthnorm')


def is_byte_narrow(shape):
    """Orig char-arg load: mov al,[mem] / mov r8, r/m8 (a0/8a)."""
    return shape.startswith('mov B,')


def is_byte_wide(shape):
    """Recomp widened byte load: movzx r32, r/m8 (0F B6) or movsx-from-byte."""
    if shape.startswith('movzx R, byte ptr') or shape.startswith('movzx R, B'):
        return True
    if shape.startswith('movsx R, byte ptr'):
        return True
    return False


def is_a1_dword(shape):
    """mov eax, [imm32] — the a1 counterpart of a0 `mov al,[imm32]`."""
    return shape == 'mov R, dword ptr [I]'


def is_a0_byte(shape):
    return shape in ('mov B, byte ptr [I]', 'mov B, [I]')


def is_word16(shape):
    """66-prefixed 16-bit op. `qword ptr` must not match as `word ptr`."""
    if 'qword ptr' in shape or 'dword ptr' in shape:
        return bool(re.search(r'\bW\b', shape))
    if 'word ptr' in shape:
        return True
    return bool(re.search(r'\bW\b', shape))


def is_int32_op(shape):
    """32-bit integer op that can be the wide twin of a 16-bit local op."""
    if is_word16(shape) or is_byte_narrow(shape) or is_byte_wide(shape):
        return False
    mnem = shape.split()[0]
    return mnem in (
        'mov', 'movzx', 'movsx', 'cmp', 'and', 'or', 'xor', 'add', 'sub',
        'test', 'shl', 'shr', 'sar', 'inc', 'dec', 'neg', 'not',
    )


def classify_bags(extra, miss):
    """Return (bucket, gap, byte_n, word_n, byte_pct, word_pct).

    Pair extra/missing by widthnorm key so a 16-bit addressing shuffle
    (`mov W,[I]` vs `mov W,[R+I]`) does not count as 16-vs-32, and a
    leftover `mov R, dword ptr [R+I]` does not steal byte-load credit.
    """
    gap = sum(extra.values()) + sum(miss.values())
    if gap == 0:
        return 'regalloc', 0, 0, 0, 0.0, 0.0

    e_by = defaultdict(Counter)
    m_by = defaultdict(Counter)
    for s, c in extra.items():
        e_by[width_key(s)][s] += c
    for s, c in miss.items():
        m_by[width_key(s)][s] += c

    byte_n = 0
    word_n = 0
    for wn in set(e_by) | set(m_by):
        e = e_by[wn]
        m = m_by[wn]
        e_wide = sum(c for s, c in e.items() if is_byte_wide(s))
        e_a1 = sum(c for s, c in e.items() if is_a1_dword(s))
        e_xor = e.get('xor R, R', 0)
        e_i32 = sum(c for s, c in e.items() if is_int32_op(s))
        e_w = sum(c for s, c in e.items() if is_word16(s))
        m_b = sum(c for s, c in m.items() if is_byte_narrow(s))
        m_a0 = sum(c for s, c in m.items() if is_a0_byte(s))
        m_w = sum(c for s, c in m.items() if is_word16(s))

        # byte: movzx<->mov B, a1<->a0, xor-zero-extend<->mov B
        b_pair = min(e_wide, m_b)
        m_b_left = m_b - b_pair
        e_wide_left = e_wide - b_pair
        a_pair = min(e_a1, m_a0)
        xor_pair = min(e_xor, m_b_left)
        byte_n += (b_pair + a_pair + xor_pair) * 2
        byte_n += e_wide_left  # leftover movzx is still the widened side

        # word16: orig 16-bit vs recomp 32-bit of the SAME widthnorm key.
        # Both-sides-already-16-bit (addressing shuffle) does not pair.
        w_pair = min(m_w, e_i32)
        word_n += w_pair * 2

    byte_pct = byte_n / float(gap)
    word_pct = word_n / float(gap)
    if byte_pct >= THRESHOLD and byte_pct >= word_pct:
        bucket = 'byte'
    elif word_pct >= THRESHOLD:
        bucket = 'word16'
    else:
        bucket = 'neither'
    return bucket, gap, byte_n, word_n, byte_pct, word_pct


def opcode_counts(orig, rc):
    """Cheap opcode tallies for the report's supporting columns."""
    def kind(i):
        b = bytes(i.bytes)
        k = 0
        saw66 = False
        while k < len(b) and b[k] in (0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65,
                                       0x66, 0x67, 0xF0, 0xF2, 0xF3):
            if b[k] == 0x66:
                saw66 = True
            k += 1
        if k >= len(b):
            return None
        op = b[k]
        if op in (0xA0, 0x8A):
            return 'bl'
        if k + 1 < len(b) and op == 0x0F and b[k + 1] == 0xB6:
            return 'zx8'
        if saw66:
            return 'op66'
        return None

    o = Counter(kind(i) for i in md.disasm(orig, 0))
    r = Counter(kind(i) for i in md.disasm(rc, 0))
    return (o['bl'], r['bl'], o['zx8'], r['zx8'], o['op66'], r['op66'])


def load_diffs(va_filter):
    rows = []
    with open(REPORT) as f:
        for r in csv.DictReader(f):
            if r.get('status') != 'diff':
                continue
            if va_filter and r['va'].lower() != va_filter.lower() \
                    and r['name'].lower() != va_filter.lower():
                continue
            rows.append(r)
    return rows


def obj_path(src, opt):
    base = os.path.splitext(os.path.basename(src))[0]
    return os.path.join(ROOT, 'build', 'match', 'obj_' + opt, base + '.obj')


def need_compile(src, opt, force):
    obj = obj_path(src, opt)
    if force or not os.path.exists(obj):
        return True
    try:
        return os.path.getmtime(os.path.join(ROOT, src)) > os.path.getmtime(obj)
    except OSError:
        return True


def compile_group(src, opt, force):
    """Compile one (file, opt) with the fn.py / match_sweep cl invocation."""
    if not need_compile(src, opt, force):
        return obj_path(src, opt), []
    flags = _OPT.get(opt, '/O2')
    obj, err = compile_variant(os.path.join(ROOT, src), opt, flags)
    return obj, err


def measure(row, coff):
    name = row['name']
    va = row['va']
    if not coff or name not in coff:
        return dict(va=va, name=name, file=row['file'], opt=row.get('opt', ''),
                    bucket='no-sym', gap=-1, extra=0, miss=0,
                    byte_n=0, word_n=0, byte_pct=0.0, word_pct=0.0,
                    o_bl=0, r_bl=0, o_zx=0, r_zx=0, o_66=0, r_66=0,
                    extra_bag=Counter(), miss_bag=Counter(),
                    orig_size=row.get('orig_size', ''),
                    diffs=row.get('diffs', ''), err='symbol not in obj')
    rc = strip_pad(coff[name][0])
    ob = os.path.join(ORIG_DIR, va + '.bin')
    if not os.path.exists(ob):
        return dict(va=va, name=name, file=row['file'], opt=row.get('opt', ''),
                    bucket='no-orig', gap=-1, extra=0, miss=0,
                    byte_n=0, word_n=0, byte_pct=0.0, word_pct=0.0,
                    o_bl=0, r_bl=0, o_zx=0, r_zx=0, o_66=0, r_66=0,
                    extra_bag=Counter(), miss_bag=Counter(),
                    orig_size=row.get('orig_size', ''),
                    diffs=row.get('diffs', ''), err='no orig bin')
    orig = open(ob, 'rb').read()
    O = bag(orig, 'regnorm')
    R = bag(rc, 'regnorm')
    extra, miss = R - O, O - R
    bucket, gap, byte_n, word_n, bp, wp = classify_bags(extra, miss)
    o_bl, r_bl, o_zx, r_zx, o_66, r_66 = opcode_counts(orig, rc)
    return dict(va=va, name=name, file=row['file'], opt=row.get('opt', ''),
                bucket=bucket, gap=gap,
                extra=sum(extra.values()), miss=sum(miss.values()),
                byte_n=byte_n, word_n=word_n, byte_pct=bp, word_pct=wp,
                o_bl=o_bl, r_bl=r_bl, o_zx=o_zx, r_zx=r_zx,
                o_66=o_66, r_66=r_66,
                extra_bag=extra, miss_bag=miss,
                orig_size=row.get('orig_size', ''),
                diffs=row.get('diffs', ''), err='')


def rank_key(m):
    # Bucketed hits first, then larger shape-gap (more signal).
    order = {'byte': 0, 'word16': 1, 'regalloc': 3, 'neither': 2}
    return (order.get(m['bucket'], 9), -m['gap'], m['file'], m['va'])


def print_table(rows, detail):
    n = len(rows)
    tally = Counter(m['bucket'] for m in rows)
    print('scanned %d diff rows  (threshold %.0f%% of regnorm gap)'
          % (n, THRESHOLD * 100))
    for k in ('byte', 'word16', 'regalloc', 'neither', 'no-sym', 'no-orig'):
        if tally[k]:
            print('  %-10s %4d' % (k, tally[k]))
    print()

    def homogeneous(name):
        c = tally[name]
        if c == 0:
            return '%s: 0 members — empty (negative result)' % name
        if c < 8:
            return '%s: %d member%s — not a class (need 8+ homogeneous)' \
                   % (name, c, '' if c == 1 else 's')
        return '%s: %d members — bulk-win candidate' % (name, c)

    print('class test:')
    print('  ' + homogeneous('byte'))
    print('  ' + homogeneous('word16'))
    print()

    hdr = '%-11s %-28s %-36s %-8s %5s %6s %6s %s' % (
        'VA', 'name', 'file', 'bucket', 'gap', 'byte%', 'word%', 'opt')
    print(hdr)
    print('-' * len(hdr))

    shown = 0
    for m in rows:
        interesting = m['bucket'] in ('byte', 'word16') \
            or (m['bucket'] == 'neither' and max(m['byte_pct'], m['word_pct']) >= 0.40)
        if m['bucket'] in ('byte', 'word16', 'regalloc') or interesting:
            print('%s %-28s %-36s %-8s %5d %5.0f%% %5.0f%% %s' % (
                m['va'], m['name'][:28], m['file'][-36:], m['bucket'],
                m['gap'], m['byte_pct'] * 100, m['word_pct'] * 100, m['opt']))
            shown += 1
            if detail and m['bucket'] in ('byte', 'word16'):
                print('    EXTRA  %s' % m['extra_bag'].most_common(8))
                print('    MISS   %s' % m['miss_bag'].most_common(8))
    if shown == 0:
        print('(no byte/word16 members, no near-misses at 40%)')
    print()
    print('opcode support (orig-vs-recomp, not the bucket test):')
    more8a = sum(1 for m in rows if m['o_bl'] > m['r_bl'])
    morezx = sum(1 for m in rows if m['r_zx'] > m['o_zx'])
    both = sum(1 for m in rows if m['o_bl'] > m['r_bl'] and m['r_zx'] > m['o_zx'])
    more66 = sum(1 for m in rows if m['o_66'] > m['r_66'])
    print('  orig more 8a/a0: %d   recomp more movzx8: %d   both (true byte idiom): %d'
          % (more8a, morezx, both))
    print('  orig more 66-prefix: %d' % more66)


def write_csv(path, rows):
    fields = ['va', 'name', 'file', 'opt', 'bucket', 'gap', 'extra', 'miss',
              'byte_n', 'word_n', 'byte_pct', 'word_pct',
              'o_bl', 'r_bl', 'o_zx', 'r_zx', 'o_66', 'r_66',
              'orig_size', 'diffs']
    with open(path, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        w.writeheader()
        for m in rows:
            out = dict(m)
            out['byte_pct'] = '%.3f' % m['byte_pct']
            out['word_pct'] = '%.3f' % m['word_pct']
            w.writerow(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--force', action='store_true',
                    help='recompile every file (ignore fresh objs)')
    ap.add_argument('--detail', action='store_true',
                    help='print extra/missing bags for bucket members')
    ap.add_argument('--csv', metavar='PATH', help='write full ranked table')
    ap.add_argument('--va', metavar='VA-or-name', help='one function only')
    a = ap.parse_args()

    diffs = load_diffs(a.va)
    if not diffs:
        sys.exit('no status=diff rows%s' % (' for %s' % a.va if a.va else ''))

    groups = defaultdict(list)
    for r in diffs:
        groups[(r['file'], r.get('opt') or 'O2')].append(r)

    coff_by = {}
    n_comp = n_reuse = n_fail = 0
    for i, ((src, opt), members) in enumerate(sorted(groups.items()), 1):
        stale = need_compile(src, opt, a.force)
        obj, err = compile_group(src, opt, a.force)
        note = 'compile' if stale else 'reuse'
        if err or obj is None:
            n_fail += 1
            print('[%3d/%3d] %-40s %s FAIL %s' % (i, len(groups), src, opt,
                                                  '; '.join(err)[:80]),
                  flush=True)
            coff_by[(src, opt)] = None
            continue
        try:
            coff_by[(src, opt)] = parse_coff_obj(obj)
        except Exception as e:
            n_fail += 1
            print('[%3d/%3d] %-40s %s FAIL parse: %s' % (i, len(groups), src, opt, e),
                  flush=True)
            coff_by[(src, opt)] = None
            continue
        if stale:
            n_comp += 1
        else:
            n_reuse += 1
        print('[%3d/%3d] %-40s %s %s  (%d fns)' % (i, len(groups), src, opt,
                                                   note, len(members)),
              flush=True)

    rows = [measure(r, coff_by.get((r['file'], r.get('opt') or 'O2')))
            for r in diffs]
    rows.sort(key=rank_key)

    print()
    print('compile: %d reused, %d compiled, %d failed  (of %d file/opt groups)'
          % (n_reuse, n_comp, n_fail, len(groups)))
    print()
    print_table(rows, a.detail)
    if a.csv:
        write_csv(a.csv, rows)
        print('wrote', a.csv)
    return 0


if __name__ == '__main__':
    sys.exit(main())
