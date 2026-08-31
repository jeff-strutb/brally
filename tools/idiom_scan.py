#!/usr/bin/env python3
"""Bucket every report.csv `diff` by proven mechanical idioms.

Dry-run detector. Does not edit source.

The signatures (each proven, then exhausted on the functions named):

  byte       orig loads a byte arg as `mov al,[mem]; push eax` (opcodes
             a0/8a into a byte reg). Recomp with the param typed int emits
             movzx / mov eax (and Ghidra CONCAT leftovers). Fix: callee
             param `unsigned char`. Proven FUN_10002830 / FUN_10027710.

  word16     orig stores/loads a 16-bit local with a 66-prefixed op
             (66 89 / 66 8b, movsx ax, cmp ax,imm). Recomp with the local
             typed int emits the 32-bit form. Fix: local `short` /
             `unsigned short`. Proven BrTextBoxMeasureB / FUN_100298c0.

  float-abs  recomp emits consecutive `fstp st(i); fld; fchs` (store-
             reload-negate) where orig has fewer. That is `t = x; if
             (t < Z) t = -t` — dest already exists, so the negate is a
             NEW value. Fix: `t = (x < Z) ? -x : x` (in-place fchs).
             Proven 0x10067470 (REGNORM extra 16→4). Extra fchs is
             usually 0: orig has the same fchs, just without the shuffle.

  port-guard recomp is LARGER and its EXTRA shapes are dominated by a
             guard the original lacks: `test R,R` or `cmp R,imm` /
             `cmp [esp+S],imm` PLUS a jcc, with few or zero MISSING
             shapes. That is a port-added NULL/bounds early-out
             (`if (!p) return;` / `if (pRow != NULL)`). Matching
             build drops it (or wraps `#ifndef BR_MATCHING_BUILD`).
             Proven 0x10001240 (BrSurfFromBitmap, extra test/jne/xor/
             ret) and 0x100476C0 (BrUiHook85_1004E810, extra test/je).
             Consecutive is usual but not required — VC5 can schedule
             a store between the extra test and jcc. If orig has the
             equivalent branch, it is not this class.

`byte` / `word16` land only when the paired extra/missing shapes
DOMINATE the register-blind (regnorm) gap at >=70%. `float-abs` lands
when the extra shuffle clusters dominate the EXTRA bag at >=70% (the
miss side is usually unrelated fxch/slot drain — requiring 70% of
extra+miss would have rejected the proven case itself, 12/37).
`port-guard` lands when extra test/cmp+jcc pairs (plus the xor/ret
early-out companions) dominate the EXTRA bag at >=70%, miss <= 2,
and recomp is larger than orig. Anything else is `neither`. Gap 0
(identical shapes, different bytes) is `regalloc` — a coloring
residue, not a width class.

A bucket with 1-2 members is not a class. A bucket with 8+ homogeneous
members is the bulk win. Empty / heterogeneous is a valid negative
result: do not force fixes.

Compile: same wine+cl path as tools/fnmatch/fn.py, grouped per
(file, opt) so a 500-function scan is ~100 compiles not 500. Fresh
obj_{opt} from the last sweep are reused; --force recompiles.

    .venv/bin/python tools/idiom_scan.py              # dry-run table
    .venv/bin/python tools/idiom_scan.py --detail     # + extra/missing
    .venv/bin/python tools/idiom_scan.py --csv PATH
    .venv/bin/python tools/idiom_scan.py --float-csv PATH
    .venv/bin/python tools/idiom_scan.py --port-csv PATH
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
FLOAT_CSV = os.path.join(ROOT, 'build', 'match', 'float_worklist.csv')
PORT_CSV = os.path.join(ROOT, 'build', 'match', 'portguard_worklist.csv')
THRESHOLD = 0.70
# "few or zero MISSING shapes": a port-added guard does not remove orig
# ops. Polarity flips and spelling swaps (test vs cmp) land above this.
MISS_FEW = 2

JCC = frozenset((
    'je', 'jne', 'jz', 'jnz', 'jl', 'jle', 'jg', 'jge',
    'ja', 'jae', 'jb', 'jbe', 'js', 'jns', 'jo', 'jno',
    'jp', 'jnp', 'jecxz',
))
# Capstone register names that `cmp R, imm` may use.
_GPREG = frozenset((
    'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp',
    'ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp', 'sp',
    'al', 'bl', 'cl', 'dl', 'ah', 'bh', 'ch', 'dh',
))

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


def is_fstp_st(mnem, op):
    """x87 stack store-and-pop: `fstp st(i)`, not `fstp dword [mem]`."""
    return mnem == 'fstp' and op.startswith('st')


def count_float_abs_clusters(code):
    """Count consecutive `fstp st(i); fld; fchs` triples.

    That is the reassignment-abs shuffle: dest already lives (on the x87
    stack or as a named temp), so `t = -t` pops the dest, reloads, and
    negates. In-place `t = (x < Z) ? -x : x` is just `je; fchs`. Proven
    on 0x10067470 (six such triples, extra 16→4 once rewritten).

    Consecutive is the proven form. A leftover `fstp st(0)` that discards
    a compared copy, or a post-store stack drain, is NOT this cluster.
    """
    seq = [(i.mnemonic, i.op_str) for i in md.disasm(code, 0)]
    n = 0
    i = 0
    while i + 2 < len(seq):
        m0, o0 = seq[i]
        m1 = seq[i + 1][0]
        m2 = seq[i + 2][0]
        if is_fstp_st(m0, o0) and m1 == 'fld' and m2 == 'fchs':
            n += 1
            i += 3
            continue
        i += 1
    return n


def extra_fstp_st(extra):
    return sum(c for s, c in extra.items() if s.startswith('fstp st'))


def extra_fld(extra):
    return sum(c for s, c in extra.items() if s.split()[0] == 'fld')


def extra_fchs(extra):
    return sum(c for s, c in extra.items() if s.split()[0] == 'fchs')


def is_jcc_mnem(mnem):
    return mnem in JCC


def is_jcc_shape(shape):
    return shape.split()[0] in JCC


def _is_imm(tok):
    """Immediate operand as capstone prints it (`0`, `0x10`, `-1`)."""
    t = tok.strip()
    if t.startswith('-'):
        t = t[1:]
    return t.startswith('0x') or t.isdigit()


def is_guard_test_raw(mnem, op):
    """NULL / zero / bounds test: `test R,R` or `cmp R,imm` / `cmp [esp],imm`.

    Field compares (`cmp word [esi+0x12], 18`) are NOT this — those are
    original logic. Stack-arg `cmp [esp+N], 0` is the NULL test that
    never loads the pointer.
    """
    parts = [p.strip() for p in op.split(',')]
    if len(parts) != 2:
        return False
    a, b = parts
    if mnem == 'test':
        return a == b
    if mnem != 'cmp':
        return False
    if not _is_imm(b):
        return False
    if a in _GPREG:
        return True
    # stack-arg vs imm. `[eax+N]` is a field; `[eax+ebp+4]` is indexed.
    # Only a frame/stack slot (`[esp+N]`, `[ebp-N]`) is a NULL-arg test.
    if re.search(r'\[(e?sp|e?bp)\b', a):
        return True
    return False


def is_guard_test_shape(shape):
    """Register-blind form of is_guard_test_raw (0 and 24 both become I)."""
    if shape in ('test R, R', 'test W, W', 'test B, B'):
        return True
    if shape in ('cmp R, I', 'cmp W, I', 'cmp B, I'):
        return True
    if not shape.startswith('cmp '):
        return False
    if '[esp+S]' not in shape and '[ebp+S]' not in shape:
        return False
    return shape.endswith(', I')


def is_earlyout_shape(shape):
    """Companion ops of `if (!p) return 0/NULL;`: xor-zero and extra ret."""
    mnem = shape.split()[0]
    if mnem == 'ret':
        return True
    return shape in ('xor R, R', 'xor W, W', 'xor B, B')


def count_guard_clusters(code):
    """Count consecutive `test/cmp-guard ; jcc` pairs.

    That is the port-added NULL/bounds early-out: orig dereferences (or
    indexes) unconditionally; the transcription wraps it in `if (p)` /
    `if (i < n)`. Consecutive is the proven form (0x10001240 test/je
    before the 24bpp cmp; 0x100476C0 test/je before the load). A lone
    `test` that feeds a setcc, or a `cmp [mem+disp],imm` field test, is
    not this cluster.
    """
    seq = [(i.mnemonic, i.op_str) for i in md.disasm(code, 0)]
    n = 0
    i = 0
    while i + 1 < len(seq):
        if is_guard_test_raw(*seq[i]) and is_jcc_mnem(seq[i + 1][0]):
            n += 1
            i += 2
            continue
        i += 1
    return n


def extra_guard_tests(extra):
    return sum(c for s, c in extra.items() if is_guard_test_shape(s))


def extra_jcc(extra):
    return sum(c for s, c in extra.items() if is_jcc_shape(s))


def extra_earlyout(extra):
    return sum(c for s, c in extra.items() if is_earlyout_shape(s))


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


def _empty_measure(row, bucket, err):
    return dict(va=row['va'], name=row['name'], file=row['file'],
                opt=row.get('opt', ''), bucket=bucket, gap=-1,
                extra=0, miss=0, byte_n=0, word_n=0, byte_pct=0.0, word_pct=0.0,
                cluster_count=0, cluster_orig=0, cluster_recomp=0,
                float_n=0, float_pct=0.0, extra_fstp_st=0, extra_fld_n=0,
                extra_fchs_n=0,
                guard_count=0, guard_orig=0, guard_recomp=0,
                guard_n=0, guard_pct=0.0, extra_test_n=0, extra_jcc_n=0,
                orig_nbytes=0, recomp_nbytes=0, size_delta=0,
                o_bl=0, r_bl=0, o_zx=0, r_zx=0, o_66=0, r_66=0,
                extra_bag=Counter(), miss_bag=Counter(),
                orig_size=row.get('orig_size', ''),
                diffs=row.get('diffs', ''), err=err)


def measure(row, coff):
    name = row['name']
    va = row['va']
    if not coff or name not in coff:
        return _empty_measure(row, 'no-sym', 'symbol not in obj')
    rc = strip_pad(coff[name][0])
    ob = os.path.join(ORIG_DIR, va + '.bin')
    if not os.path.exists(ob):
        return _empty_measure(row, 'no-orig', 'no orig bin')
    orig = open(ob, 'rb').read()
    O = bag(orig, 'regnorm')
    R = bag(rc, 'regnorm')
    extra, miss = R - O, O - R
    bucket, gap, byte_n, word_n, bp, wp = classify_bags(extra, miss)
    o_bl, r_bl, o_zx, r_zx, o_66, r_66 = opcode_counts(orig, rc)

    cluster_orig = count_float_abs_clusters(orig)
    cluster_recomp = count_float_abs_clusters(rc)
    cluster_count = cluster_recomp - cluster_orig
    if cluster_count < 0:
        cluster_count = 0
    extra_n = sum(extra.values())
    miss_n = sum(miss.values())
    # Each excess cluster is extra `fstp st` + extra `fld`. Orig usually
    # already has the fchs (in-place), so extra fchs is not the signal.
    float_n = cluster_count * 2
    float_pct = (float_n / float(extra_n)) if extra_n else 0.0
    if bucket in ('neither', 'regalloc') and cluster_count > 0 \
            and float_pct >= THRESHOLD:
        bucket = 'float-abs'

    guard_orig = count_guard_clusters(orig)
    guard_recomp = count_guard_clusters(rc)
    seq_extra = guard_recomp - guard_orig
    if seq_extra < 0:
        seq_extra = 0
    e_test = extra_guard_tests(extra)
    e_jcc = extra_jcc(extra)
    bag_pair = min(e_test, e_jcc)
    # extra-guard-count is extra test/cmp PLUS extra jcc (the signature
    # the original lacks). Consecutive clusters are usual but not
    # required: VC5 can schedule a store between the extra test and
    # jcc, so seq_extra can be 0 while the extra bag is the guard.
    guard_count = bag_pair
    e_early = extra_earlyout(extra) if guard_count else 0
    # Pair shapes (test+jcc) plus the early-out xor/ret that a
    # `return NULL` companion emits. Unpaired extra jcc do not count —
    # a rewritten ladder is not this class.
    guard_n = guard_count * 2 + e_early
    guard_pct = (guard_n / float(extra_n)) if extra_n else 0.0
    orig_nbytes = len(orig)
    recomp_nbytes = len(rc)
    size_delta = recomp_nbytes - orig_nbytes
    if bucket in ('neither', 'regalloc') and guard_count > 0 \
            and extra_n and guard_pct >= THRESHOLD \
            and miss_n <= MISS_FEW and size_delta > 0:
        bucket = 'port-guard'

    return dict(va=va, name=name, file=row['file'], opt=row.get('opt', ''),
                bucket=bucket, gap=gap,
                extra=extra_n, miss=miss_n,
                byte_n=byte_n, word_n=word_n, byte_pct=bp, word_pct=wp,
                cluster_count=cluster_count, cluster_orig=cluster_orig,
                cluster_recomp=cluster_recomp, float_n=float_n,
                float_pct=float_pct,
                extra_fstp_st=extra_fstp_st(extra), extra_fld_n=extra_fld(extra),
                extra_fchs_n=extra_fchs(extra),
                guard_count=guard_count, guard_orig=guard_orig,
                guard_recomp=guard_recomp, guard_n=guard_n,
                guard_pct=guard_pct, extra_test_n=e_test, extra_jcc_n=e_jcc,
                orig_nbytes=orig_nbytes, recomp_nbytes=recomp_nbytes,
                size_delta=size_delta,
                o_bl=o_bl, r_bl=r_bl, o_zx=o_zx, r_zx=r_zx,
                o_66=o_66, r_66=r_66,
                extra_bag=extra, miss_bag=miss,
                orig_size=row.get('orig_size', ''),
                diffs=row.get('diffs', ''), err='')


def rank_key(m):
    # Bucketed hits first, then larger shape-gap (more signal).
    order = {'port-guard': 0, 'float-abs': 1, 'byte': 2, 'word16': 3,
             'neither': 4, 'regalloc': 5}
    return (order.get(m['bucket'], 9), -m.get('guard_count', 0),
            -m.get('cluster_count', 0), -m['gap'], m['file'], m['va'])


def float_rank_key(m):
    # Excess shuffle clusters first, then larger register-blind gap.
    return (-m['cluster_count'], -m['gap'], m['file'], m['va'])


def port_rank_key(m):
    # Extra guard pairs first, then more extra bytes, then gap.
    return (-m.get('guard_count', 0), -m.get('size_delta', 0),
            -m['gap'], m['file'], m['va'])


def print_table(rows, detail):
    n = len(rows)
    tally = Counter(m['bucket'] for m in rows)
    print('scanned %d diff rows  (width: %.0f%% of regnorm gap;  '
          'float-abs / port-guard: %.0f%% of extra bag;  '
          'port-guard miss<=%d and recomp>orig)'
          % (n, THRESHOLD * 100, THRESHOLD * 100, MISS_FEW))
    for k in ('port-guard', 'float-abs', 'byte', 'word16', 'regalloc',
              'neither', 'no-sym', 'no-orig'):
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
    print('  ' + homogeneous('port-guard'))
    print('  ' + homogeneous('float-abs'))
    print('  ' + homogeneous('byte'))
    print('  ' + homogeneous('word16'))
    print()

    hdr = '%-11s %-28s %-36s %-9s %5s %6s %6s %5s %s' % (
        'VA', 'name', 'file', 'bucket', 'gap', 'byte%', 'word%', 'clst', 'opt')
    print(hdr)
    print('-' * len(hdr))

    shown = 0
    for m in rows:
        interesting = m['bucket'] in ('byte', 'word16', 'float-abs',
                                      'port-guard') \
            or (m['bucket'] == 'neither'
                and max(m['byte_pct'], m['word_pct'], m['float_pct'],
                        m.get('guard_pct', 0.0)) >= 0.40)
        if m['bucket'] in ('byte', 'word16', 'float-abs', 'port-guard',
                           'regalloc') or interesting:
            print('%s %-28s %-36s %-9s %5d %5.0f%% %5.0f%% %5d %s' % (
                m['va'], m['name'][:28], m['file'][-36:], m['bucket'],
                m['gap'], m['byte_pct'] * 100, m['word_pct'] * 100,
                m['cluster_count'], m['opt']))
            shown += 1
            if detail and m['bucket'] in ('byte', 'word16', 'float-abs',
                                          'port-guard'):
                print('    EXTRA  %s' % m['extra_bag'].most_common(8))
                print('    MISS   %s' % m['miss_bag'].most_common(8))
    if shown == 0:
        print('(no byte/word16/float-abs/port-guard members, '
              'no near-misses at 40%)')
    print()
    print('opcode support (orig-vs-recomp, not the bucket test):')
    more8a = sum(1 for m in rows if m['o_bl'] > m['r_bl'])
    morezx = sum(1 for m in rows if m['r_zx'] > m['o_zx'])
    both = sum(1 for m in rows if m['o_bl'] > m['r_bl'] and m['r_zx'] > m['o_zx'])
    more66 = sum(1 for m in rows if m['o_66'] > m['r_66'])
    print('  orig more 8a/a0: %d   recomp more movzx8: %d   both (true byte idiom): %d'
          % (more8a, morezx, both))
    print('  orig more 66-prefix: %d' % more66)

    print()
    print_float_worklist(rows, detail)
    print()
    print_portguard_worklist(rows, detail)


def print_float_worklist(rows, detail):
    """Ranked excess `fstp st(i); fld; fchs` clusters (recomp minus orig)."""
    cands = [m for m in rows if m['cluster_count'] > 0]
    cands.sort(key=float_rank_key)
    print('float-abs worklist  (excess fstp-st / fld / fchs clusters, '
          'then regnorm gap):')
    if not cands:
        extra_st = [m for m in rows if m.get('extra_fstp_st', 0) > 0]
        extra_st.sort(key=lambda m: (-m['extra_fstp_st'], -m['gap'],
                                     m['file'], m['va']))
        print('  (empty — no status=diff row has more of the consecutive')
        print('   store-reload-negate shuffle than orig.)')
        if extra_st:
            print('  %d row%s extra `fstp st(i)` without the fld+fchs '
                  'cluster — leftover stack discards, not this idiom:'
                  % (len(extra_st), '' if len(extra_st) == 1 else 's'))
            for m in extra_st[:15]:
                print('    %s %-28s fstp-st+%d  fld+%d  fchs+%d  gap %d  %s'
                      % (m['va'], m['name'][:28], m['extra_fstp_st'],
                         m.get('extra_fld_n', 0), m.get('extra_fchs_n', 0),
                         m['gap'], m['file'][-36:]))
        return
    hdr = '%-11s %-28s %-36s %5s %5s %5s %5s %5s' % (
        'VA', 'name', 'file', 'clst', 'gap', 'x-st', 'x-fld', 'xfchs')
    print(hdr)
    print('-' * len(hdr))
    for m in cands[:15]:
        print('%s %-28s %-36s %5d %5d %5d %5d %5d' % (
            m['va'], m['name'][:28], m['file'][-36:],
            m['cluster_count'], m['gap'],
            m.get('extra_fstp_st', 0), m.get('extra_fld_n', 0),
            m.get('extra_fchs_n', 0)))
        if detail:
            print('    EXTRA  %s' % m['extra_bag'].most_common(8))
            print('    MISS   %s' % m['miss_bag'].most_common(8))
    if len(cands) > 15:
        print('  ... %d more' % (len(cands) - 15))
    print('  %d candidate%s' % (len(cands), '' if len(cands) == 1 else 's'))


def portguard_candidates(rows):
    """Bucket members only — the worklist is the class, not every extra jcc."""
    cands = [m for m in rows if m['bucket'] == 'port-guard']
    cands.sort(key=port_rank_key)
    return cands


def print_portguard_worklist(rows, detail):
    """Ranked extra NULL/bounds guards (recomp larger, miss few, extra dominated)."""
    cands = portguard_candidates(rows)
    print('port-guard worklist  (extra test/cmp+jcc, recomp>orig, miss<=%d, '
          'then extra bytes):' % MISS_FEW)
    if not cands:
        # Near-misses: extra guards that failed dominance / size / miss.
        # Rank small-extra first — those are the proven-case shape; huge
        # extra bags with many leftover jcc are not this class.
        near = [m for m in rows if m.get('guard_count', 0) > 0]
        near.sort(key=lambda m: (m['extra'], m['miss'],
                                 -m.get('guard_count', 0), m['file']))
        print('  (empty — no status=diff row has extra test/cmp+jcc')
        print('   dominating the extra bag, few/zero miss, and recomp>orig.)')
        if near:
            print('  %d row%s extra test/cmp+jcc that failed the class test '
                  '(dominance / miss / size) — not this idiom:'
                  % (len(near), '' if len(near) == 1 else 's'))
            for m in near[:15]:
                print('    %s %-28s g+%d  extra %d  miss %d  dB %+d  gap %d  %s'
                      % (m['va'], m['name'][:28], m['guard_count'],
                         m['extra'], m['miss'], m.get('size_delta', 0),
                         m['gap'], m['file'][-36:]))
        return
    hdr = '%-11s %-28s %-36s %5s %6s %5s %5s' % (
        'VA', 'name', 'file', 'gcnt', 'dB', 'miss', 'gap')
    print(hdr)
    print('-' * len(hdr))
    show = cands[:20]
    for m in show:
        print('%s %-28s %-36s %5d %+6d %5d %5d' % (
            m['va'], m['name'][:28], m['file'][-36:],
            m['guard_count'], m.get('size_delta', 0),
            m['miss'], m['gap']))
        if detail:
            print('    EXTRA  %s' % m['extra_bag'].most_common(8))
            print('    MISS   %s' % m['miss_bag'].most_common(8))
    if len(cands) > 20:
        print('  ... %d more' % (len(cands) - 20))
    print('  %d candidate%s' % (len(cands), '' if len(cands) == 1 else 's'))
    if len(cands) < 8:
        print('  class test: scattered/small — do not force source edits')
    else:
        print('  class test: %d homogeneous members — bulk-win candidate'
              % len(cands))


def write_csv(path, rows):
    fields = ['va', 'name', 'file', 'opt', 'bucket', 'gap', 'extra', 'miss',
              'byte_n', 'word_n', 'byte_pct', 'word_pct',
              'cluster_count', 'cluster_orig', 'cluster_recomp',
              'float_n', 'float_pct',
              'extra_fstp_st', 'extra_fld_n', 'extra_fchs_n',
              'guard_count', 'guard_orig', 'guard_recomp',
              'guard_n', 'guard_pct', 'extra_test_n', 'extra_jcc_n',
              'orig_nbytes', 'recomp_nbytes', 'size_delta',
              'o_bl', 'r_bl', 'o_zx', 'r_zx', 'o_66', 'r_66',
              'orig_size', 'diffs']
    with open(path, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        w.writeheader()
        for m in rows:
            out = dict(m)
            out['byte_pct'] = '%.3f' % m['byte_pct']
            out['word_pct'] = '%.3f' % m['word_pct']
            out['float_pct'] = '%.3f' % m['float_pct']
            out['guard_pct'] = '%.3f' % m.get('guard_pct', 0.0)
            w.writerow(out)


def write_float_csv(path, rows):
    """Ranked worklist: VA, name, file, cluster-count, gap.

    cluster_count is the excess consecutive `fstp st(i); fld; fchs`
    triples in recomp over orig. Empty is a valid negative result.
    """
    cands = [m for m in rows if m['cluster_count'] > 0]
    cands.sort(key=float_rank_key)
    fields = ['va', 'name', 'file', 'cluster_count', 'gap']
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    with open(path, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        w.writeheader()
        for m in cands:
            w.writerow({
                'va': m['va'],
                'name': m['name'],
                'file': m['file'],
                'cluster_count': m['cluster_count'],
                'gap': m['gap'],
            })
    return len(cands)


def write_port_csv(path, rows):
    """Ranked worklist: VA, name, file, extra-guard-count, size delta, gap.

    extra-guard-count is extra `test R,R`/`cmp R,imm` paired with extra
    jcc in the register-blind bag. Empty is a valid negative result.
    """
    cands = portguard_candidates(rows)
    fields = ['va', 'name', 'file', 'extra-guard-count',
              'recomp_minus_orig_bytes', 'gap']
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    with open(path, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for m in cands:
            w.writerow({
                'va': m['va'],
                'name': m['name'],
                'file': m['file'],
                'extra-guard-count': m['guard_count'],
                'recomp_minus_orig_bytes': m.get('size_delta', 0),
                'gap': m['gap'],
            })
    return len(cands)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--force', action='store_true',
                    help='recompile every file (ignore fresh objs)')
    ap.add_argument('--detail', action='store_true',
                    help='print extra/missing bags for bucket members')
    ap.add_argument('--csv', metavar='PATH', help='write full ranked table')
    ap.add_argument('--float-csv', metavar='PATH', default=FLOAT_CSV,
                    help='write ranked float-abs worklist (default: %s)'
                         % os.path.relpath(FLOAT_CSV, ROOT))
    ap.add_argument('--no-float-csv', action='store_true',
                    help='skip the float-abs worklist CSV')
    ap.add_argument('--port-csv', metavar='PATH', default=PORT_CSV,
                    help='write ranked port-guard worklist (default: %s)'
                         % os.path.relpath(PORT_CSV, ROOT))
    ap.add_argument('--no-port-csv', action='store_true',
                    help='skip the port-guard worklist CSV')
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
    if not a.no_float_csv:
        n = write_float_csv(a.float_csv, rows)
        print('wrote %s  (%d float-abs candidate%s)'
              % (a.float_csv, n, '' if n == 1 else 's'))
    if not a.no_port_csv:
        n = write_port_csv(a.port_csv, rows)
        print('wrote %s  (%d port-guard candidate%s)'
              % (a.port_csv, n, '' if n == 1 else 's'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
