#!/usr/bin/env python3
"""Standalone refine transforms for four recurring Ghidra→VC5 idioms
flagged across the 2026-08-27 fresh DLL batches (VC5-IDIOMS-fresh{1,2,3}.md).

1. STRING-OPS — exploded `repne scasb` / `rep movsd` / `rep stosd` →
   strcpy / strcat / strlen / memset / memcpy, plus `extern char s_*[]`
   (address push, not value). Highest prey.
2. char-width return — orig `mov al,1; pop*; ret` came from `char` /
   `BrBool`, not Ghidra's `int`.
3. extern __int64 globals — add/adc on a 64-bit global; Ghidra splits it
   into two int loads. A *local* __int64 is the `and esp,-8` trap.
4. unsigned-char + live-ebx — `unsigned char` locals / CONCAT31 byte
   args emit `mov bl; push ebx` (zero/one-register pattern).

Each generator yields `(label, mutated_source)` in the `_refine_candidates`
style. This does not write ghidra_work, does not edit ghidra_to_match.py,
and does not commit. Decision logic: docs/gen-fresh-notes.md.

    python3 tools/gen_fresh.py --dry-run
    python3 tools/gen_fresh.py --dry-run --pool untrans
    python3 tools/gen_fresh.py --validate
    python3 tools/gen_fresh.py --validate --from-decomp
    python3 tools/gen_fresh.py --validate --pool untrans --from-decomp
    python3 tools/gen_fresh.py --va 0x100703D0 --gen stringops
    python3 tools/gen_fresh.py --va 0x10054390 --from-decomp --gen charret
"""
from __future__ import print_function

import argparse
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
_VENV_SP = os.path.join(ROOT, '.venv', 'lib')
if os.path.isdir(_VENV_SP):
    import glob
    for _sp in glob.glob(os.path.join(_VENV_SP, 'python*', 'site-packages')):
        if _sp not in sys.path:
            sys.path.insert(0, _sp)

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
WORK_DIR = os.path.join(ROOT, 'build', 'ghidra_work')
GHIDRA_DIR = os.path.join(ROOT, 'build', 'ghidra_decomp')
LEARNINGS_CSV = os.path.join(ROOT, 'build', 'ghidra_learnings.csv')
REPORT_CSV = os.path.join(ROOT, 'build', 'match', 'report.csv')

# Distinguisher bytes (fresh1/2/3).
_SCASB = b'\xf2\xae'          # repne scasb
_MOVSD = b'\xf3\xa5'          # rep movsd
_STOSD = b'\xf3\xab'          # rep stosd
_MOV_AL_IMM = 0xB0           # mov al, imm8
_XOR_AL = (b'\x32\xc0', b'\x30\xc0')
_OR_AL_FF = b'\x0c\xff'
_POPS = frozenset([0x58, 0x59, 0x5A, 0x5B, 0x5D, 0x5E, 0x5F])
_ADC_R = 0x13                # adc r32, r/m32
_ALLMUL_NAMES = frozenset([
    '__allmul', '__aulldiv', '__alldiv', '__allshl', '__allshr',
    '__aullshr', '__allrem', '__aullrem',
])
_INT_RETS = (
    'unsigned int', 'unsigned long', 'undefined4', 'DWORD', 'BOOL',
    'ULONG', 'UINT', 'HRESULT', 'uint', 'int',
)
_C_KEYWORDS = frozenset([
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'case', 'do',
    'void', 'int', 'char', 'short', 'float', 'double', 'long', 'unsigned',
    'struct', 'typedef', 'static', 'const', 'volatile', 'goto', 'break',
    'continue', 'else', 'true', 'false',
])

# Canonical extras (fresh-batch proven sites), always scored even if MATCH.
_EXTRAS = {
    'stringops': [
        '0x1003C430', '0x10054390', '0x100368A0', '0x10055AF0',
        '0x1006FF50', '0x10038DA0', '0x100367C0', '0x1006C4D0',
        '0x1005A080', '0x100703D0', '0x100299A0', '0x10040A90',
        '0x10037C90', '0x1006FCE0', '0x1003A580',
    ],
    'charret': [
        '0x10054390', '0x10069930', '0x10069A80', '0x10069DE0',
        '0x100695C0', '0x1006A080',
    ],
    'i64glob': ['0x1002E186', '0x1006E280'],
    'ucharbx': ['0x10027B60', '0x1006A650', '0x10020900'],
}


# ---------------------------------------------------------------------------
# head/body cut — same as `_refine_candidates`
# ---------------------------------------------------------------------------

def _split(src):
    marker = src.find('Forward declarations')
    if marker < 0:
        return src[:0], src
    head_end = src.find('\n\n', marker)
    if head_end < 0:
        return src, ''
    return src[:head_end], src[head_end:]


def _join(head, body):
    return head + body


def _orig(va):
    p = os.path.join(ORIG_DIR, va + '.bin')
    return open(p, 'rb').read() if os.path.exists(p) else b''


def _strip_nops(b):
    n = len(b)
    while n and b[n - 1] in (0x90, 0xCC):
        n -= 1
    return b[:n]


def _name_live_after(body, name, pos):
    """True if `name` is read after pos before being reassigned."""
    rest = body[pos:]
    m = re.search(r'\b%s\b' % re.escape(name), rest)
    if not m:
        return False
    # An immediate `name = ...` is a kill, not a read.
    after = rest[m.end():]
    if re.match(r'\s*=\s*[^=]', after):
        return False
    return True


def _apply_all(gen, src):
    """Walk gen(src) one edit at a time until a fixed point."""
    labels = []
    changed = True
    while changed:
        changed = False
        for label, cand in gen(src):
            if cand != src:
                src = cand
                labels.append(label)
                changed = True
                break
    return src, labels


# ---------------------------------------------------------------------------
# orig-byte distinguishers
# ---------------------------------------------------------------------------

def _skip_epilogue(b, i):
    """Walk back from a ret over pops and `add esp, imm8` / `pop ecx`."""
    while i > 0:
        if b[i - 1] in _POPS:
            i -= 1
            continue
        if i >= 3 and b[i - 3] == 0x83 and b[i - 2] == 0xC4:
            i -= 3
            continue
        break
    return i


def char_width_orig(b):
    """True if orig returns a byte in AL then pops + ret.

    Distinguisher: `b0 xx 5* c3` (`mov al,imm; pop*; ret`) vs
    `b8 xx 00 00 00 c3` (`mov eax,imm32; ret`). Proven 0x10054390
    (`b0 01 5b c3`) and 0x10069930 (`b0 01 5f 5e 59 c3`).
    """
    b = _strip_nops(b)
    if len(b) < 3:
        return False
    if b[-1] == 0xC3:
        i = len(b) - 1
    elif len(b) >= 3 and b[-3] == 0xC2:
        i = len(b) - 3
    else:
        return False
    i = _skip_epilogue(b, i)
    if i >= 2 and b[i - 2] == _MOV_AL_IMM:
        return True
    if i >= 2 and b[i - 2:i] in _XOR_AL:
        return True
    if i >= 2 and b[i - 2:i] == _OR_AL_FF:
        return True
    # Mid-function byte returns (0x10054390 has `b0 01 5f 5e 5d 5b` as
    # well as the tail). Require the B0 to sit immediately before a pop.
    for j in range(len(b) - 2):
        if b[j] == _MOV_AL_IMM and b[j + 2] in _POPS:
            return True
        if b[j:j + 2] == _OR_AL_FF and j + 2 < len(b) and b[j + 2] in _POPS:
            return True
        if b[j:j + 2] in _XOR_AL and j + 2 < len(b) and b[j + 2] in _POPS:
            return True
    return False


def _has_mov_bl_push_ebx(b):
    """`mov bl, [mem]; push ebx` — 0x10027B60 byte-arg run."""
    i = 0
    n = len(b)
    while i + 1 < n:
        if b[i] == 0x8A:
            modrm = b[i + 1]
            if ((modrm >> 3) & 7) == 3:
                mod, rm = modrm >> 6, modrm & 7
                k = i + 2
                if rm == 4:
                    k += 1
                    if k > n:
                        i += 1
                        continue
                if mod == 1:
                    k += 1
                elif mod == 2 or (mod == 0 and rm == 5):
                    k += 4
                if k < n and b[k] == 0x53:
                    return True
        i += 1
    return False


def _has_add_adc_pair(b):
    """add r,r / adc r,r around two consecutive abs32 stores.

    0x1002E186: `03 c8; a1 HI; 13 c2; 89 0d LO; 89 15 HI`.
    """
    i = 0
    n = len(b)
    while i + 6 < n:
        if b[i] == _ADC_R:
            # look back ~16 bytes for an add, forward ~16 for two 89 0d/15
            window = b[max(0, i - 16):i + 24]
            if b'\x03' in window and (b'\x89\x0d' in window or b'\x89\x15' in window):
                return True
        i += 1
    return False


# ---------------------------------------------------------------------------
# 1. STRING-OPS
# ---------------------------------------------------------------------------

# Import the proven strcpy/strcat / `extern char s_*[]` rewrite rather than
# re-derive it. Wrap's `_strcpy_sub` still misses walker-rewind and strcat
# (docs/gen-structural-notes.md). We add strlen / memset / memcpy on top.
import gen_structural as _gs

# Scan loop. Covers wrap's 0xffffffff form AND the signed `i = -1` form
# wrap's `_strlen_sub` requires 0xffffffff and so misses (0x10054390,
# 0x1003A580). Optional pointer init immediately before the do.
_SCAN = re.compile(
    r'(?P<u>\w+)\s*=\s*(?P<init>-1|0xffffffff|0xffffffffU)\s*;\s*'
    r'(?:(?P<pinit>\w+)\s*=\s*(?P<src>[^;]+);\s*)?'
    r'do\s*\{\s*'
    r'(?:(?P<w>\w+)\s*=\s*(?P<p>\w+)\s*;\s*)?'
    r'if\s*\(\s*(?P=u)\s*==\s*0\s*\)\s*break\s*;\s*'
    r'(?P=u)\s*=\s*(?P=u)\s*(?:-\s*1|\+\s*-1)\s*;\s*'
    r'(?:(?P<w2>\w+)\s*=\s*(?P<p2>\w+)\s*\+\s*1\s*;\s*)?'
    r'(?P<c>\w+)\s*=\s*\*(?P<pstar>\w+)\s*;\s*'
    r'(?P<pset>\w+)\s*=\s*(?P<next>[^;]+);\s*'
    r'\}\s*while\s*\(\s*(?P=c)\s*!=\s*(?:\'\\0\'|0)\s*\)\s*;',
    re.S,
)

# Counted dword zero (rep stosd of N dwords). Wrap's `_memset_sub` requires
# `n >> 2` PLUS a residual byte loop; residue still has `for (i = N; i;)`.
# Proven shape 0x1006C4D0 / 0x100703D0 / 0x10037C90 / 0x1006FCE0.
_MEMSET_DWORD = re.compile(
    r'(?:(?P<dst>\w+)\s*=\s*(?P<base>[^;]+);\s*)?'
    r'for\s*\(\s*(?P<i>\w+)\s*=\s*(?P<n>0x[0-9a-fA-F]+|\d+)\s*;\s*'
    r'(?P=i)\s*!=\s*0\s*;\s*'
    r'(?P=i)\s*=\s*(?P=i)\s*(?:\+\s*-1|-\s*1)\s*\)\s*\{\s*'
    r'\*(?P<p>\w+)\s*=\s*0\s*;\s*'
    r'(?P=p)\s*=\s*(?P=p)\s*\+\s*1\s*;\s*'
    r'\}',
    re.S,
)

# Counted char[4]-zero (rep stosd of N dwords through a char*). Proven
# 0x10055AF0 (`for (i = 0x41; i;) { p[0]..p[3] = 0; p += 4; }`).
_MEMSET_CHARS = re.compile(
    r'(?:(?P<dst>\w+)\s*=\s*(?P<base>[^;]+);\s*)?'
    r'for\s*\(\s*(?P<i>\w+)\s*=\s*(?P<n>0x[0-9a-fA-F]+|\d+)\s*;\s*'
    r'(?P=i)\s*!=\s*0\s*;\s*'
    r'(?P=i)\s*=\s*(?P=i)\s*(?:\+\s*-1|-\s*1)\s*\)\s*\{\s*'
    r'(?P<p>\w+)\s*\[\s*0\s*\]\s*=\s*(?:\'\\0\'|0)\s*;\s*'
    r'(?P=p)\s*\[\s*1\s*\]\s*=\s*(?:\'\\0\'|0)\s*;\s*'
    r'(?P=p)\s*\[\s*2\s*\]\s*=\s*(?:\'\\0\'|0)\s*;\s*'
    r'(?P=p)\s*\[\s*3\s*\]\s*=\s*(?:\'\\0\'|0)\s*;\s*'
    r'(?P=p)\s*=\s*(?P=p)\s*\+\s*4\s*;\s*'
    r'\}',
    re.S,
)

# Wrap's >>2 + residual-byte memset (zero only). Kept so from-decomp of a
# *raw* shape still fires if wrap's `_memset_sub` missed a spelling.
_MEMSET_SHR = re.compile(
    r'for\s*\(\s*(?P<u>\w+)\s*=\s*(?P<n>[^;]+?)\s*>>\s*2\s*;\s*(?P=u)\s*!=\s*0\s*;'
    r'\s*(?P=u)\s*=\s*(?P=u)\s*(?:-\s*1|\+\s*-1)\s*\)\s*\{\s*'
    r'(?:(?P<p>\w+)\s*\[\s*0\s*\]\s*=\s*(?:\'\\0\'|0)\s*;\s*'
    r'(?P=p)\s*\[\s*1\s*\]\s*=\s*(?:\'\\0\'|0)\s*;\s*'
    r'(?P=p)\s*\[\s*2\s*\]\s*=\s*(?:\'\\0\'|0)\s*;\s*'
    r'(?P=p)\s*\[\s*3\s*\]\s*=\s*(?:\'\\0\'|0)\s*;'
    r'|\*\(\s*(?:unsigned\s+)?(?:int|long)\s*\*\s*\)\s*(?P<p2>\w+)\s*=\s*0\s*;'
    r'|\*(?P<p3>\w+)\s*=\s*0\s*;)'
    r'\s*(?P<pp>\w+)\s*=\s*(?P=pp)\s*\+\s*(?:4|1)\s*;\s*\}\s*'
    r'for\s*\(\s*(?P<u2>\w+)\s*=\s*(?P=n)\s*&\s*3\s*;\s*(?P=u2)\s*!=\s*0\s*;'
    r'\s*(?P=u2)\s*=\s*(?P=u2)\s*(?:-\s*1|\+\s*-1)\s*\)\s*\{\s*'
    r'(?:\*(?P=pp)\s*=\s*(?:\'\\0\'|0)|'
    r'\*\(\s*(?:unsigned\s+)?char\s*\*\s*\)\s*(?P=pp)\s*=\s*(?:\'\\0\'|0))\s*;'
    r'\s*(?P=pp)\s*=\s*(?:(?P=pp)\s*\+\s*1|'
    r'\([^;]+\)\s*\(\s*(?:int\s*)?\s*(?P=pp)\s*\+\s*1\s*\))\s*;\s*\}',
    re.S,
)

# Counted dword copy (rep movsd of N dwords) — memcpy, not strcpy (no
# scasb scan). Proven 0x100299A0 (`for (i = 0xaa; i;) { *d = *s; s++; d++; }`).
_MEMCPY_DWORD = re.compile(
    r'(?:(?P<s0>\w+)\s*=\s*(?P<sbase>[^;]+);\s*)?'
    r'(?:(?P<d0>\w+)\s*=\s*(?P<dbase>[^;]+);\s*)?'
    r'for\s*\(\s*(?P<i>\w+)\s*=\s*(?P<n>0x[0-9a-fA-F]+|\d+)\s*;\s*'
    r'(?P=i)\s*!=\s*0\s*;\s*'
    r'(?P=i)\s*=\s*(?P=i)\s*(?:\+\s*-1|-\s*1)\s*\)\s*\{\s*'
    r'\*(?P<d>\w+)\s*=\s*\*(?P<s>\w+)\s*;\s*'
    r'(?P=s)\s*=\s*(?P=s)\s*\+\s*1\s*;\s*'
    r'(?P=d)\s*=\s*(?P=d)\s*\+\s*1\s*;\s*'
    r'\}',
    re.S,
)

# >>2 + residual memcpy of a *known* byte length (no scasb). Proven
# 0x1005A080 (`h*w*4 & 0x3fffffff` dword loop + 0-iter residual).
_MEMCPY_SHR = re.compile(
    r'for\s*\(\s*(?P<u>\w+)\s*=\s*(?P<n>[^;]+?)\s*(?:>>\s*2|&\s*0x3fffffff)\s*;'
    r'\s*(?P=u)\s*!=\s*0\s*;\s*(?P=u)\s*=\s*(?P=u)\s*(?:-\s*1|\+\s*-1)\s*\)\s*\{\s*'
    r'\*(?P<d>\w+)\s*=\s*\*(?P<s>\w+)\s*;\s*'
    r'(?P=s)\s*=\s*(?P=s)\s*\+\s*1\s*;\s*'
    r'(?P=d)\s*=\s*(?P=d)\s*\+\s*1\s*;\s*'
    r'\}\s*'
    r'(?:for\s*\(\s*(?P<r>\w+)\s*=\s*(?:0|[^;]*&\s*3)\s*;\s*(?P=r)\s*!=\s*0\s*;'
    r'\s*(?P=r)\s*=\s*(?P=r)\s*(?:-\s*1|\+\s*-1)\s*\)\s*\{[^}]*\}\s*)?',
    re.S,
)

_IF_M2 = re.compile(
    r'if\s*\(\s*(?P<u>\w+)\s*(?P<op>==|!=)\s*-2\s*\)'
)


def _scan_src_expr(m, pre):
    """Source pointer expression of a scan loop."""
    if m.group('src'):
        expr = m.group('src').strip()
        if expr and expr not in ('0xffffffff', '-1') and ' + 1' not in expr \
                and ' + -' not in expr:
            return expr
    pstar = m.group('pstar')
    inits = list(re.finditer(
        r'\b%s\s*=\s*([^;]+);' % re.escape(pstar), pre[-400:]))
    if inits:
        expr = inits[-1].group(1).strip()
        if expr not in ('0xffffffff', '-1') and ' + 1' not in expr:
            return expr
    return pstar


def _scan_followed_by_copy(body, end):
    """True if a >>2 dword copy sits right after this scan (strcpy/strcat)."""
    rest = body[end:end + 400]
    return bool(re.search(r'>>\s*2', rest)) and bool(_gs._COPY_LOOPS.search(rest))


def gen_strlen(src):
    """Ghidra exploded `repne scasb; not ecx; dec ecx` → `strlen`.

    Distinguisher: orig `or ecx,-1; f2 ae; not ecx; dec ecx` (strlen) vs
    the copy form which then `shr ecx,2; f3 a5` (strcpy — strarr). Wrap's
    `_strlen_sub` requires `u = 0xffffffff` and misses signed `i = -1`
    (0x10054390 `if (strlen(s) != 0)`, 0x1003A580 `== 0`).
    """
    head, body = _split(src)
    for i, m in enumerate(_SCAN.finditer(body)):
        if _scan_followed_by_copy(body, m.end()):
            continue
        src_expr = _scan_src_expr(m, body[:m.start()])
        u = m.group('u')
        rest = body[m.end():]
        # `if (i != -2)` / `== -2`  ↔  strlen != 0 / == 0.
        stripped = rest.lstrip()
        im = _IF_M2.match(stripped)
        if im and im.group('u') == u:
            skip = len(rest) - len(stripped)
            cond = '!=' if im.group('op') == '!=' else '=='
            repl = 'if (strlen(%s) %s 0)' % (src_expr, cond)
            nb = body[:m.start()] + repl + body[m.end() + skip + im.end():]
            yield ('strlen:cmp:%d' % i, _join(head, nb))
            continue
        # Length form: `u = ~u` then `~u - 1` is strlen, `~u` is n+1.
        tilde = re.match(
            r'\s*%s\s*=\s*~\s*%s\s*;' % (re.escape(u), re.escape(u)), rest)
        if tilde:
            after = rest[tilde.end():]
            # Don't steal a strcpy — strarr wants the ~u + copy.
            if _gs._COPY_LOOPS.search(after[:300]):
                continue
            stop = re.search(
                r'\b%s\s*=\s*(?:-1|0xffffffff)' % re.escape(u), after)
            cut = stop.start() if stop else len(after)
            scope = after[:cut]
            scope = re.sub(r'~%s\s*-\s*1\b' % re.escape(u), u, scope)
            scope = re.sub(r'~%s\b' % re.escape(u), '(%s + 1)' % u, scope)
            nb = (body[:m.start()] + '%s = strlen(%s);' % (u, src_expr)
                  + scope + after[cut:])
            yield ('strlen:len:%d' % i, _join(head, nb))
            continue
        # Bare scan whose next use is `~u - 1` without an explicit `u = ~u`.
        if re.match(r'\s*\w+\s*=\s*~%s\s*-\s*1' % re.escape(u), rest):
            am = re.match(
                r'\s*(\w+)\s*=\s*~%s\s*-\s*1\s*;' % re.escape(u), rest)
            if am:
                repl = '%s = strlen(%s);' % (am.group(1), src_expr)
                nb = body[:m.start()] + repl + rest[am.end():]
                yield ('strlen:len:%d' % i, _join(head, nb))


def _memset_dest(m):
    p = m.group('p')
    if m.group('base') and m.group('dst') == p:
        return m.group('base').strip()
    return p


def _advance_if_live(body, end, walker, n_elems):
    """If the walker is read after the loop, keep `w = w + N` (memset/memcpy
    do not advance; the exploded loop does)."""
    if walker and _name_live_after(body, walker, end):
        return '%s = %s + %s;' % (walker, walker, n_elems)
    return ''


def gen_memset(src):
    """Ghidra exploded `rep stosd` → `memset`.

    Distinguisher: orig `xor eax,eax; mov ecx,N; f3 ab`. Wrap requires
    `n>>2` + residual bytes; dword-only counted loops remain (0x100703D0
    at 2 diffs, 0x1006C4D0 15-dword desc, 0x10037C90, 0x1006FCE0).
    """
    head, body = _split(src)
    sites = []
    for i, m in enumerate(_MEMSET_DWORD.finditer(body)):
        dest = _memset_dest(m)
        n = m.group('n')
        try:
            nbytes = '%d' % (int(n, 0) * 4)
        except ValueError:
            nbytes = '(%s) * 4' % n
        adv = _advance_if_live(body, m.end(), m.group('p'), n)
        repl = 'memset(%s, 0, %s);%s' % (dest, nbytes, adv)
        sites.append(('memset:imm:%d' % i, m.start(), m.end(), repl))
    for i, m in enumerate(_MEMSET_CHARS.finditer(body)):
        dest = _memset_dest(m)
        n = m.group('n')
        try:
            nbytes = '%d' % (int(n, 0) * 4)
        except ValueError:
            nbytes = '(%s) * 4' % n
        adv = _advance_if_live(body, m.end(), m.group('p'), '(%s)*4' % n)
        repl = 'memset(%s, 0, %s);%s' % (dest, nbytes, adv)
        sites.append(('memset:ch:%d' % i, m.start(), m.end(), repl))
    for i, m in enumerate(_MEMSET_SHR.finditer(body)):
        p = m.group('pp')
        n = m.group('n').strip()
        repl = 'memset(%s, 0, %s);' % (p, n)
        sites.append(('memset:shr:%d' % i, m.start(), m.end(), repl))
    sites.sort(key=lambda s: s[1])
    # One site per yield so the climb can combine.
    for label, start, end, repl in sites:
        nb = body[:start] + repl + body[end:]
        yield (label, _join(head, nb))


def _memcpy_size_from_shr(nexpr):
    """Byte length of a `n >> 2` / `n & 0x3fffffff` dword loop."""
    nexpr = nexpr.strip()
    m = re.match(r'^(.*)\s*>>\s*2\s*$', nexpr)
    if m:
        return m.group(1).strip()
    m = re.match(r'^(.*)\s*&\s*0x3fffffff\s*$', nexpr)
    if m:
        inner = m.group(1).strip()
        # 0x1005A080: Ghidra prints `h*w*4 & 0x3fffffff` as the dword
        # count of a byte length — MATCH spelling is `h*w*4` bytes.
        if re.search(r'\*\s*4\b', inner):
            return inner
        return '(%s) * 4' % inner
    return nexpr


def gen_memcpy(src):
    """Ghidra exploded `rep movsd` of a *known* size → `memcpy`.

    Distinguisher: orig `mov ecx,N; f3 a5` with no preceding `f2 ae`.
    scasb+movsd is strcpy (strarr). Proven 0x100299A0 (0xaa dwords),
    0x1005A080 (`memcpy(p, src, h*w*4)` re-spelled after malloc).
    """
    head, body = _split(src)
    # Skip spans that strarr still owns (scan + copy).
    strarr_spans = []
    for site in _gs._find_string_copy_sites(body):
        strarr_spans.append((site['start'], site['end']))

    def _owned(pos):
        return any(a <= pos < b for a, b in strarr_spans)

    for i, m in enumerate(_MEMCPY_DWORD.finditer(body)):
        if _owned(m.start()):
            continue
        n = m.group('n')
        try:
            nbytes = '%d' % (int(n, 0) * 4)
        except ValueError:
            nbytes = '(%s) * 4' % n
        d = m.group('dbase').strip() if m.group('dbase') and m.group('d0') == m.group('d') \
            else m.group('d')
        s = m.group('sbase').strip() if m.group('sbase') and m.group('s0') == m.group('s') \
            else m.group('s')
        # If the inits were captured, the match start includes them — dest/src
        # expressions are the bases. Walkers after memcpy: keep if live.
        adv = _advance_if_live(body, m.end(), m.group('d'), n)
        adv2 = _advance_if_live(body, m.end(), m.group('s'), n)
        repl = 'memcpy(%s, %s, %s);%s%s' % (d, s, nbytes, adv, adv2)
        nb = body[:m.start()] + repl + body[m.end():]
        yield ('memcpy:imm:%d' % i, _join(head, nb))
    for i, m in enumerate(_MEMCPY_SHR.finditer(body)):
        if _owned(m.start()):
            continue
        # Don't steal strcpy: a 0xffffffff scan immediately before.
        pre = body[max(0, m.start() - 250):m.start()]
        if re.search(r'0xffffffff', pre) and _SCAN.search(pre):
            continue
        nexpr = m.group('n').strip()
        # The regex's n group is the LHS of >>2 / & 0x3fffffff.
        nbytes = _memcpy_size_from_shr(nexpr + (
            ' >> 2' if '>>' in m.group(0)[:80] else ' & 0x3fffffff'))
        # Prefer the raw group: pull operator from the matched text.
        mm = re.search(
            r'=\s*([^;]+?)\s*(>>\s*2|&\s*0x3fffffff)', m.group(0))
        if mm:
            nbytes = _memcpy_size_from_shr(mm.group(1) + ' ' + mm.group(2))
        d, s = m.group('d'), m.group('s')
        repl = 'memcpy(%s, %s, %s);' % (d, s, nbytes)
        nb = body[:m.start()] + repl + body[m.end():]
        yield ('memcpy:shr:%d' % i, _join(head, nb))


def gen_stringops(src):
    """strcpy/strcat/`extern char s[]` + strlen + memset + memcpy.

    One-edit yields so the climb can combine with other generators.
    Fold recipe: transform_stringops (all edits).
    """
    for item in _gs.gen_strarr(src):
        yield item
    for item in gen_strlen(src):
        yield item
    for item in gen_memset(src):
        yield item
    for item in gen_memcpy(src):
        yield item


def transform_stringops(src, orig=None):
    """Apply every string-op edit. strarr first so memcpy does not steal
    scasb copies."""
    src, labels = _gs.transform_strarr(src)
    s2, l2 = _apply_all(gen_strlen, src)
    src, labels = s2, labels + l2
    s2, l2 = _apply_all(gen_memset, src)
    src, labels = s2, labels + l2
    s2, l2 = _apply_all(gen_memcpy, src)
    return s2, labels + l2


def prey_stringops(src, orig, va=None):
    if orig and (_SCASB in orig or _MOVSD in orig or _STOSD in orig):
        return True
    try:
        next(gen_stringops(src))
        return True
    except StopIteration:
        return False


# ---------------------------------------------------------------------------
# 2. char-width return
# ---------------------------------------------------------------------------

_DEF_SIG = re.compile(
    r'(?P<ret>unsigned\s+int|unsigned\s+long|undefined4|DWORD|BOOL|'
    r'ULONG|UINT|HRESULT|uint|int)'
    r'(?P<cc>\s+(?:__stdcall|__fastcall|__cdecl))?'
    r'\s+(?P<name>(?:FUN_|THUNK_)[0-9a-fA-F]+|\w+)\s*\([^;{]*\)\s*\n?\s*\{'
)

_RET_CONST = re.compile(
    r'return\s+([^;]+);'
)


def _char_sized_returns(body, sig_end):
    """True if every `return` after the def is a char-sized constant."""
    ok = False
    for m in _RET_CONST.finditer(body[sig_end:]):
        expr = m.group(1).strip()
        expr = re.sub(r'^\(\s*(?:unsigned\s+)?char\s*\)\s*', '', expr)
        if expr in ('0', '1', '-1', '0xff', '0xFF', '0x00', '(char)0xff',
                    '(char)0xFF', '(char)-1', 'true', 'false'):
            ok = True
            continue
        return False
    return ok


def gen_charret(src, orig=None):
    """`int f(...)` whose orig ends `mov al,1; pop*; ret` → `char f(...)`.

    Distinguisher: `b0 01 5b c3` vs `b8 01 00 00 00 c3`. Proven MATCH
    0x10054390 (`char __fastcall`, `return (char)0xff` / `return 1`).
    Ghidra prints `undefined4` → wrap `int`. Yields `charret:char` and
    `charret:uchar` (BrBool is either). Also rewrites `return 0xff` to
    `(char)0xff` so orig `or al,0xff` stays a byte or.
    """
    if orig is not None and orig and not char_width_orig(orig):
        return
    head, body = _split(src)
    m = _DEF_SIG.search(body)
    if not m:
        return
    if m.group('ret').strip() in ('char', 'unsigned char'):
        return
    if orig is None and not _char_sized_returns(body, m.end()):
        return
    cc = m.group('cc') or ''
    name = m.group('name')
    # Keep the rest of the signature (`(args)\n{`).
    tail = body[m.end(0) - 1:]  # from `{`
    # Reconstruct from the original match, swapping the ret type.
    prefix = body[:m.start()]
    args_and_brace = body[m.end('name'):]
    for ty, tag in (('char', 'char'), ('unsigned char', 'uchar')):
        nb = prefix + '%s%s %s' % (ty, cc, name) + args_and_brace
        # Byte-or of 0xff: `return 0xff` as char is `or al,0xff` (0x10054390).
        if ty == 'char':
            nb = re.sub(r'\breturn\s+0xff\s*;', 'return (char)0xff;', nb)
            nb = re.sub(r'\breturn\s+0xFF\s*;', 'return (char)0xff;', nb)
        yield ('charret:%s' % tag, _join(head, nb))


def transform_charret(src, orig=None):
    labels = []
    for label, cand in gen_charret(src, orig=orig):
        if label.endswith(':char') and cand != src:
            return cand, [label]
    return src, labels


def prey_charret(src, orig, va=None):
    if orig and char_width_orig(orig):
        return True
    try:
        next(gen_charret(src, orig=orig if orig else None))
        return True
    except StopIteration:
        return False


# ---------------------------------------------------------------------------
# 3. extern unsigned __int64 globals
# ---------------------------------------------------------------------------

_DAT_NAME = re.compile(r'\b((?:_?DAT_)([0-9a-fA-F]{8}))\b')
_CARRY4 = re.compile(
    r'(?P<c>\w+)\s*=\s*CARRY4\s*\(\s*(?P<lo>_?DAT_[0-9a-fA-F]{8})\s*,\s*(?P<e>[^)]+)\)\s*;\s*'
    r'(?P=lo)\s*=\s*(?P=lo)\s*\+\s*\((?P=e)\)\s*;\s*'
    r'(?P<hi>_?DAT_[0-9a-fA-F]{8})\s*=\s*(?P=hi)\s*\+\s*'
    r'\(\s*(?:unsigned\s+int|uint)\s*\)\s*(?P=c)\s*;',
    re.S,
)
_CARRY4_LOOSE = re.compile(
    r'(?P<c>\w+)\s*=\s*CARRY4\s*\(\s*(?P<lo>_?DAT_[0-9a-fA-F]{8})\s*,\s*(?P<e>[^)]+)\)\s*;'
)
_SPLIT_STORE = re.compile(
    r'(?P<lo>_?DAT_[0-9a-fA-F]{8})\s*=\s*\(\s*(?:unsigned\s+int|uint)\s*\)\s*(?P<v>\w+)\s*;\s*'
    r'(?P<hi>_?DAT_[0-9a-fA-F]{8})\s*=\s*\(\s*int\s*\)\s*'
    r'\(\s*\(\s*(?:unsigned\s+__int64|ulonglong|unsigned\s+int)\s*\)\s*(?P=v)\s*'
    r'>>\s*(?:0x20|32)\s*\)\s*;',
    re.S,
)
_ALLMUL = re.compile(
    r'__allmul\s*\(\s*(_?DAT_[0-9a-fA-F]{8})\s*,\s*(_?DAT_[0-9a-fA-F]{8})\s*,'
    r'\s*([^,]+),\s*([^)]+)\)'
)
_CONCAT_PAIR = re.compile(
    r'(?:CONCAT44|/\*\s*CONCAT\s*\*/)\s*\(\s*(_?DAT_[0-9a-fA-F]{8})\s*,\s*'
    r'(_?DAT_[0-9a-fA-F]{8})\s*\)'
)
_EXTERN_DAT_LINE = re.compile(
    r'^extern (?:unsigned )?(?:int|long|__int64) (\*?)(_?DAT_[0-9a-fA-F]{8})\s*;\s*$',
    re.M,
)


def _dat_addr(name):
    m = re.search(r'([0-9a-fA-F]{8})$', name)
    return int(m.group(1), 16) if m else None


def _paired_dats(text):
    """Yield (lo_name, hi_name) for DAT_X / DAT_(X+4) both present."""
    names = {}
    for m in _DAT_NAME.finditer(text):
        names[_dat_addr(m.group(1))] = m.group(1)
    pairs = []
    for addr, name in names.items():
        if addr is None:
            continue
        hi = names.get(addr + 4)
        if hi:
            pairs.append((name, hi))
    return pairs


def _is_allmul_func(src):
    m = re.search(
        r'\b(__allmul|__aulldiv|__alldiv|__allshl|__allshr|__aullshr|'
        r'__allrem|__aullrem)\s*\([^;]*\)\s*\{', src)
    return bool(m)


def gen_i64glob(src):
    """Ghidra split `DAT_lo`/`DAT_lo+4` → `extern unsigned __int64 DAT_lo`.

    Distinguisher: orig `add ecx,eax; adc eax,edx` on two consecutive
    globals, frame `sub esp,0x10`, no `and esp,-8`. A *local* __int64
    is the 8-byte-align trap (0x10019A70). Proven 0x1002E186 (22 leftover
    is fild-slot packing, not the type). Skip CRT `__allmul` itself.
    """
    if _is_allmul_func(src):
        return
    head, body = _split(src)
    pairs = _paired_dats(head + body)
    if not pairs:
        return
    # Only pairs that actually participate in a 64-bit idiom.
    used = []
    blob = head + body
    for lo, hi in pairs:
        if re.search(r'CARRY4\s*\(\s*%s\b' % re.escape(lo), blob) \
                or re.search(r'__allmul\s*\(\s*%s\s*,\s*%s\b' % (
                    re.escape(lo), re.escape(hi)), blob) \
                or re.search(
                    r'(?:CONCAT44|/\*\s*CONCAT\s*\*/)\s*\(\s*%s\s*,\s*%s\b' % (
                        re.escape(hi), re.escape(lo)), blob):
            used.append((lo, hi))
    if not used:
        return
    nh, nb = head, body
    labels = []
    for lo, hi in used:
        # Retype lo to unsigned __int64; drop hi's scalar extern.
        pat_lo = re.compile(
            r'^extern (?:unsigned )?(?:int|long|__int64) \*?%s\s*;\s*\n'
            % re.escape(lo), re.M)
        ml = pat_lo.search(nh)
        if ml:
            nh = (nh[:ml.start()] + 'extern unsigned __int64 %s;\n' % lo
                  + nh[ml.end():])
            labels.append('i64:%s' % lo[-8:])
        pat_hi = re.compile(
            r'^extern (?:unsigned )?(?:int|long|__int64) \*?%s\s*;\s*\n'
            % re.escape(hi), re.M)
        mh = pat_hi.search(nh)
        if mh:
            nh = nh[:mh.start()] + nh[mh.end():]
        # CARRY4 add/adc pair → 64-bit += of the low addend.
        def _carry_sub(mm, lo=lo, hi=hi):
            if mm.group('lo') != lo:
                return mm.group(0)
            return '%s += (unsigned int)(%s);' % (lo, mm.group('e').strip())
        nb = _CARRY4.sub(_carry_sub, nb)
        # Loose CARRY4 leftover (if the stores didn't match tightly).
        nb = re.sub(
            r'\b\w+\s*=\s*CARRY4\s*\(\s*%s\s*,\s*([^)]+)\)\s*;' % re.escape(lo),
            r'/* carry of %s */' % lo, nb)
        # CONCAT44(hi, lo) / /* CONCAT */(hi, lo) → the 64-bit object.
        nb = re.sub(
            r'(?:CONCAT44|/\*\s*CONCAT\s*\*/)\s*\(\s*%s\s*,\s*%s\s*\)'
            % (re.escape(hi), re.escape(lo)),
            lo, nb)
        # __allmul(lo, hi, a, b) with b==0 → lo * (unsigned __int64)a
        def _mul(mm, lo=lo, hi=hi):
            a, b = mm.group(3).strip(), mm.group(4).strip()
            if mm.group(1) == lo and mm.group(2) == hi:
                if b in ('0', '0x0'):
                    return '(%s * (unsigned __int64)%s)' % (lo, a)
                return '(%s * ((unsigned __int64)%s + ((unsigned __int64)%s << 32)))' % (
                    lo, a, b)
            return mm.group(0)
        nb = _ALLMUL.sub(_mul, nb)
        # Split store of a 64-bit temp back into lo/hi.
        nb = re.sub(
            r'%s\s*=\s*\(\s*(?:unsigned\s+int|uint)\s*\)\s*(\w+)\s*;\s*'
            r'%s\s*=\s*\(\s*int\s*\)\s*\(\s*\(\s*(?:unsigned\s+__int64|'
            r'ulonglong|unsigned\s+int)\s*\)\s*\1\s*>>\s*(?:0x20|32)\s*\)\s*;'
            % (re.escape(lo), re.escape(hi)),
            r'%s = \1;' % lo, nb)
        # `lo == 0 && hi == 0` → `lo == 0`
        nb = re.sub(
            r'%s\s*==\s*0\s*&&\s*%s\s*==\s*0' % (re.escape(lo), re.escape(hi)),
            '%s == 0' % lo, nb)
        # `hi = 0; lo = x` → `lo = x`
        nb = re.sub(
            r'%s\s*=\s*0\s*;\s*%s\s*=' % (re.escape(hi), re.escape(lo)),
            '%s =' % lo, nb)
        # Remaining hi reads: high dword of the 64-bit object.
        nb = re.sub(r'\b%s\b' % re.escape(hi),
                    '(unsigned int)(%s >> 32)' % lo, nb)
    # Wrap types `__allmul`'s undefined8 result as `double`; assigning the
    # 64-bit rewrite into it is C2520. Promote those locals, and spell
    # `__aulldiv(x, imm, 0)` as a 64-bit `/` (MATCH 0x1002E186).
    for m in re.finditer(
            r'(\w+)\s*=\s*\([^;]*unsigned\s+__int64[^;]*\)\s*;', nb):
        name = m.group(1)
        nb = re.sub(
            r'^(\s+)double\s+%s\s*;' % re.escape(name),
            r'\1unsigned __int64 %s;' % name, nb, count=1, flags=re.M)
    nb = re.sub(
        r'__aulldiv\s*\(\s*([^,]+),\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*0\s*\)',
        r'((\1) / (unsigned __int64)\2)', nb)
    nb = re.sub(
        r'__aulldiv\s*\(\s*([^,]+),\s*(_?DAT_[0-9a-fA-F]{8})\s*,\s*'
        r'\(unsigned int\)\(\2\s*>>\s*32\)\s*\)',
        r'((\1) / \2)', nb)
    if nh != head or nb != body:
        # One combined candidate (pairs are one type decision).
        yield ('i64glob', _join(nh, nb))


def transform_i64glob(src, orig=None):
    for label, cand in gen_i64glob(src):
        if cand != src:
            return cand, [label]
    return src, []


def prey_i64glob(src, orig, va=None):
    if _is_allmul_func(src):
        return False
    if orig and _has_add_adc_pair(orig) and _paired_dats(src):
        return True
    try:
        next(gen_i64glob(src))
        return True
    except StopIteration:
        return False


# ---------------------------------------------------------------------------
# 4. unsigned-char + live-ebx
# ---------------------------------------------------------------------------

_BVAR_DECL = re.compile(
    r'^(\s+)(?:int|bool)\s+(bVar\d+)\s*;\s*$', re.M)
# Wrap strips CONCAT31 to `/* CONCAT */(hi, byte)`. Byte is the low 8 of a
# live dword (0x10027B60 maskOdd in ebx, then `mov bl, [p+N]; push ebx`).
# Byte form after wrap: `*(char *)(p + N)` — nested parens, not `[^,()]+`.
_CONCAT_BYTE = re.compile(
    r'(?:CONCAT31|/\*\s*CONCAT\s*\*/)\s*\(\s*([^,()]+)\s*,\s*'
    r'(\*\s*(?:\([^)]*\)\s*)*(?:\w+|\([^)]+\)))\s*\)'
)


def _call_arg_span(body, call_start):
    """Return (args_text, end_idx) of `name(args)` at call_start (name's '(')."""
    if call_start >= len(body) or body[call_start] != '(':
        return None, None
    depth = 0
    for i in range(call_start, len(body)):
        if body[i] == '(':
            depth += 1
        elif body[i] == ')':
            depth -= 1
            if depth == 0:
                return body[call_start + 1:i], i + 1
    return None, None


def _split_args(args):
    out = []
    depth = 0
    start = 0
    for i, ch in enumerate(args):
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        elif ch == ',' and depth == 0:
            out.append(args[start:i].strip())
            start = i + 1
    tail = args[start:].strip()
    if tail:
        out.append(tail)
    return out


def gen_ucharbx(src):
    """`unsigned char` locals / CONCAT31 byte args → `mov bl; push ebx`.

    Distinguisher: orig `8a 9e xx; 53` not `0f be` / `0f b6`. Proven
    0x10027B60 (eight hi/lo bytes as `unsigned char` params after a live
    dword in ebx) and 0x1006A650 (`char skip` / `mov bl,1`). Ghidra
    `bool bVar` wraps to `int bVar` — retype to `unsigned char`.
    """
    if _is_allmul_func(src):
        return
    head, body = _split(src)
    # (a) one bVar at a time
    for m in _BVAR_DECL.finditer(body):
        nb = (body[:m.start()] + '%sunsigned char %s;'
              % (m.group(1), m.group(2)) + body[m.end():])
        yield ('ucharbx:bvar:%s' % m.group(2), _join(head, nb))
    # (b) CONCAT31(hi, byte) → (unsigned char)(byte)
    for i, m in enumerate(_CONCAT_BYTE.finditer(body)):
        lo = m.group(2).strip()
        if not lo.startswith('*'):
            continue
        repl = '(unsigned char)(%s)' % lo
        nb = body[:m.start()] + repl + body[m.end():]
        yield ('ucharbx:concat:%d' % i, _join(head, nb))
    # (c) a run of `(unsigned char)(*(…+N))` or leftover CONCAT call args →
    # callee proto with unsigned char at those slots. Empty `int f();` only.
    for cm in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', body):
        name = cm.group(1)
        if name in _C_KEYWORDS:
            continue
        args, end = _call_arg_span(body, cm.end() - 1)
        if args is None:
            continue
        alist = _split_args(args)
        slots = [i for i, a in enumerate(alist)
                 if a.startswith('(unsigned char)')
                 or a.startswith('/* CONCAT */')
                 or a.startswith('CONCAT31')]
        if len(slots) < 2:
            continue
        dm = re.search(
            r'^int %s\(\);\s*$' % re.escape(name), head, re.M)
        if not dm:
            continue
        sig = []
        for i, a in enumerate(alist):
            if i in slots:
                sig.append('unsigned char')
            else:
                sig.append('int')
        nh = (head[:dm.start()]
              + 'int %s(%s);' % (name, ', '.join(sig))
              + head[dm.end():])
        yield ('ucharbx:proto:%s' % name, _join(nh, body))


def transform_ucharbx(src, orig=None):
    return _apply_all(lambda s: gen_ucharbx(s), src)


def prey_ucharbx(src, orig, va=None):
    if orig and _has_mov_bl_push_ebx(orig):
        return True
    if orig and b'\xb3\x01' in orig:  # mov bl, 1
        return True
    try:
        next(gen_ucharbx(src))
        return True
    except StopIteration:
        return False


# ---------------------------------------------------------------------------
# Registry
# ---------------------------------------------------------------------------

GENERATORS = [
    ('stringops', gen_stringops, transform_stringops),
    ('charret', lambda s: gen_charret(s), transform_charret),
    ('i64glob', gen_i64glob, transform_i64glob),
    ('ucharbx', gen_ucharbx, transform_ucharbx),
]

PREY_FN = {
    'stringops': prey_stringops,
    'charret': prey_charret,
    'i64glob': prey_i64glob,
    'ucharbx': prey_ucharbx,
}


# ---------------------------------------------------------------------------
# Load / score — never writes ghidra_work
# ---------------------------------------------------------------------------

def residue_rows():
    if not os.path.exists(LEARNINGS_CSV):
        return []
    tree_matched = set()
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            tree_matched = {r['va'].lower() for r in csv.DictReader(f)
                            if r.get('status') == 'match'}
    out = []
    with open(LEARNINGS_CSV) as f:
        for r in csv.DictReader(f):
            if not r.get('divergence') or r['divergence'] in ('', 'match'):
                continue
            if r['va'].lower() in tree_matched:
                continue
            out.append(r)
    return out


def tree_matched_vas():
    s = set()
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            s = {r['va'].lower() for r in csv.DictReader(f)
                 if r.get('status') == 'match'}
    return s


def work_vas():
    if not os.path.isdir(WORK_DIR):
        return set()
    return {fn[:-2].lower() for fn in os.listdir(WORK_DIR)
            if fn.endswith('.c') and '.refined.' not in fn}


def untrans_vas():
    """Decomp exists, no work file, not a tree MATCH — never transcribed."""
    tree = tree_matched_vas()
    work = work_vas()
    out = []
    if not os.path.isdir(GHIDRA_DIR):
        return out
    for fn in os.listdir(GHIDRA_DIR):
        if not fn.endswith('.c'):
            continue
        va = fn[:-2]
        if va.lower() in work or va.lower() in tree:
            continue
        out.append(va if va.startswith('0x') else '0x' + va)
    out.sort()
    return out


def _norm_va(va):
    if not va.lower().startswith('0x'):
        va = '0x' + va
    return '0x%08X' % int(va, 16)


def load_wrapped(va_hex, from_decomp=False):
    """Return (src, func_name). Never writes ghidra_work."""
    import ghidra_to_match as g
    work = os.path.join(WORK_DIR, va_hex + '.c')
    if not from_decomp and os.path.exists(work):
        src = open(work).read()
        m = re.search(
            r'\b(FUN_[0-9a-fA-F]+|THUNK_[0-9a-fA-F]+)\s*\([^;]*\)\s*\n?\s*\{',
            src)
        name = m.group(1) if m else 'FUN_' + va_hex[2:]
        return src, name
    funcs = g.load_functions()
    target = None
    for f in funcs:
        if int(f['va'], 16) == int(va_hex, 16):
            target = f
            break
    if target is None:
        raise SystemExit('VA %s not in functions_glide.csv' % va_hex)
    gmap = g.load_globals()
    fn_names = g.load_fn_names()
    func_name, cleaned = g.prepare_function(target, gmap, fn_names)
    orig_file = os.path.join(ORIG_DIR, va_hex + '.bin')
    if os.path.exists(orig_file):
        import match_sweep
        orig_bytes = match_sweep.load_orig(orig_file, va_hex)
        cleaned = g.fix_calling_convention(cleaned, func_name, orig_bytes)
    src = g.wrap_for_compile(cleaned, va_hex)
    return src, func_name


def load_raw_decomp(va_hex):
    """Raw `build/ghidra_decomp/<VA>.c` (no wrap). For prey detection on
    untranscribed VAs; scoring still goes through load_wrapped."""
    p = os.path.join(GHIDRA_DIR, va_hex + '.c')
    return open(p).read() if os.path.exists(p) else ''


def score_src(src, func_name, va_hex, tag):
    import ghidra_to_match as g
    import match_sweep
    orig = match_sweep.load_orig(
        os.path.join(ORIG_DIR, va_hex + '.bin'), va_hex)
    r = g._score_source(src, func_name, orig,
                        ['/O2', '/Od', '/O2 /Oy-'], tag)
    return r[0], r[1]


def run_one(va_hex, gen_name=None, from_decomp=False, verbose=True,
            score=True):
    va_hex = _norm_va(va_hex)
    src, fname = load_wrapped(va_hex, from_decomp=from_decomp)
    orig = _orig(va_hex)
    gens = GENERATORS
    if gen_name:
        gens = [g for g in GENERATORS if g[0] == gen_name]
        if not gens:
            raise SystemExit('unknown generator %s' % gen_name)
    report = []
    new_src = src
    for name, gen, transform in gens:
        if not PREY_FN[name](new_src, orig, va_hex) and gen_name is None:
            continue
        # charret / i64 need orig to avoid firing on every int function.
        if name in ('charret', 'i64glob'):
            out, labels = transform(new_src, orig=orig)
        else:
            out, labels = transform(new_src)
        n_yield = sum(1 for _ in (
            gen_charret(new_src, orig=orig) if name == 'charret'
            else gen(new_src)))
        report.append((name, labels, n_yield, out != new_src))
        if verbose:
            print('  %s: %d yields, applied %s' % (
                name, n_yield, labels or '(no-op)'))
        new_src = out
    if not score:
        return {
            'va': va_hex, 'before': None, 'after': None,
            'report': report, 'src': new_src, 'fname': fname,
        }
    tag = ('gfd' if from_decomp else 'gfw') + va_hex[-6:]
    before = score_src(src, fname, va_hex, tag + 'b')
    after = score_src(new_src, fname, va_hex, tag + 'a')
    if verbose:
        print('  diffs  %s → %s   opt %s → %s' % (
            before[0], after[0], before[1], after[1]))
        if after[0] == 0:
            print('  MATCH')
    return {
        'va': va_hex,
        'before': before[0],
        'after': after[0],
        'bopt': before[1],
        'aopt': after[1],
        'report': report,
        'match': after[0] == 0,
        'moved': (after[0] is not None and before[0] is not None
                  and after[0] < before[0]),
        'fname': fname,
        'src': new_src,
    }


def _pool_rows(pool):
    if pool == 'untrans':
        return [{'va': va} for va in untrans_vas()]
    if pool == 'both':
        seen = set()
        rows = []
        for r in residue_rows():
            seen.add(r['va'].lower())
            rows.append(r)
        for va in untrans_vas():
            if va.lower() not in seen:
                rows.append({'va': va})
        return rows
    return residue_rows()


def _prey_vas(gen_name, from_decomp=False, pool='residue'):
    vas = []
    for r in _pool_rows(pool):
        va = _norm_va(r['va'])
        work = os.path.join(WORK_DIR, va + '.c')
        decomp = os.path.join(GHIDRA_DIR, va + '.c')
        if from_decomp or pool == 'untrans':
            if not os.path.exists(decomp):
                continue
            try:
                src, _ = load_wrapped(va, from_decomp=True)
            except Exception:
                continue
        else:
            if not os.path.exists(work):
                # Untranscribed residue-less VA: still count via decomp wrap
                # when the caller asked for residue-only.
                continue
            src = open(work).read()
        orig = _orig(va)
        if PREY_FN[gen_name](src, orig, va):
            vas.append(va)
    return vas


def _score_pair(args):
    va, gen_name, from_decomp = args
    import ghidra_to_match as g  # noqa: F401
    src, fname = load_wrapped(va, from_decomp=from_decomp)
    orig = _orig(va)
    transform = dict((n, t) for n, _g, t in GENERATORS)[gen_name]
    if gen_name in ('charret', 'i64glob'):
        new_src, labels = transform(src, orig=orig)
    else:
        new_src, labels = transform(src)
    gen = dict((n, g) for n, g, _t in GENERATORS)[gen_name]
    n_yield = sum(1 for _ in (
        gen_charret(src, orig=orig) if gen_name == 'charret' else gen(src)))
    if new_src == src:
        return {
            'va': va, 'before': None, 'after': None, 'noop': True,
            'labels': labels, 'yields': n_yield,
        }
    tag = ('gfd' if from_decomp else 'gfw') + va[-6:] + gen_name[:2]
    b = score_src(src, fname, va, tag + 'b')
    a = score_src(new_src, fname, va, tag + 'a')
    return {
        'va': va,
        'before': b[0], 'after': a[0],
        'bopt': b[1], 'aopt': a[1],
        'noop': False,
        'labels': labels,
        'yields': n_yield,
        'match': a[0] == 0,
        'moved': (a[0] is not None and b[0] is not None and a[0] < b[0]),
        'worse': (a[0] is not None and b[0] is not None and a[0] > b[0]),
    }


def run_validate(from_decomp=False, gens=None, max_prey=None, workers=4,
                 pool='residue'):
    from concurrent.futures import ProcessPoolExecutor, as_completed
    gens = gens or [g[0] for g in GENERATORS]
    n_res = len(residue_rows())
    n_un = len(untrans_vas())
    print('residue unmatched: %d   untranscribed: %d   pool=%s' % (
        n_res, n_un, pool), flush=True)
    print('source: %s' % ('decomp-wrap' if from_decomp or pool == 'untrans'
                          else 'ghidra_work'), flush=True)
    summary = []
    for gen_name in gens:
        fd = from_decomp or pool == 'untrans'
        vas = _prey_vas(gen_name, from_decomp=fd, pool=pool)
        for e in _EXTRAS.get(gen_name, []):
            e = _norm_va(e)
            if e not in vas:
                work = os.path.join(WORK_DIR, e + '.c')
                decomp = os.path.join(GHIDRA_DIR, e + '.c')
                if fd and os.path.exists(decomp):
                    vas.append(e)
                elif (not fd) and os.path.exists(work):
                    vas.append(e)
        if max_prey:
            vas = vas[:max_prey]
        print('\n======== %s  prey %d ========' % (gen_name, len(vas)),
              flush=True)
        rows = []
        jobs = [(va, gen_name, fd) for va in vas]
        if workers <= 1:
            for job in jobs:
                print('  scoring %s' % job[0], flush=True)
                try:
                    rows.append(_score_pair(job))
                except Exception as e:
                    print('  ERROR', job[0], type(e).__name__, e, flush=True)
                    rows.append({'va': job[0], 'error': str(e)})
        else:
            with ProcessPoolExecutor(max_workers=workers) as pool_ex:
                futs = {pool_ex.submit(_score_pair, job): job[0] for job in jobs}
                for fut in as_completed(futs):
                    va = futs[fut]
                    try:
                        r = fut.result()
                    except Exception as e:
                        print('  ERROR', va, type(e).__name__, e, flush=True)
                        r = {'va': va, 'error': str(e)}
                    rows.append(r)
                    if r.get('error'):
                        continue
                    mark = ''
                    if r.get('noop'):
                        mark = ' no-op'
                    elif r.get('match') and r.get('moved'):
                        mark = ' MATCH'
                    elif r.get('match'):
                        mark = ' already-MATCH'
                    elif r.get('moved'):
                        mark = ' MOVE'
                    elif r.get('worse'):
                        mark = ' worse'
                    print('  %-12s %8s → %8s  %s%s' % (
                        r['va'], r.get('before'), r.get('after'),
                        ' '.join(r.get('labels') or [])[:40], mark),
                          flush=True)
        n_prey = len(vas)
        n_fire = sum(1 for r in rows if not r.get('noop') and not r.get('error')
                     and r.get('labels'))
        n_move = sum(1 for r in rows if r.get('moved'))
        n_match_abs = sum(1 for r in rows if r.get('match'))
        n_match_new = sum(1 for r in rows if r.get('match') and r.get('moved'))
        n_worse = sum(1 for r in rows if r.get('worse'))
        n_err = sum(1 for r in rows if r.get('error'))
        examples_move = [r['va'] for r in rows if r.get('moved')][:12]
        examples_match = [r['va'] for r in rows
                          if r.get('match') and r.get('moved')][:12]
        summary.append({
            'gen': gen_name, 'prey': n_prey, 'fired': n_fire,
            'moved': n_move, 'matched': n_match_abs,
            'matched_new': n_match_new, 'worse': n_worse,
            'err': n_err, 'ex_move': examples_move, 'ex_match': examples_match,
            'rows': rows,
        })
        print('  prey=%d fired=%d moved=%d matched=%d (new %d) worse=%d err=%d'
              % (n_prey, n_fire, n_move, n_match_abs, n_match_new, n_worse,
                 n_err), flush=True)
        if examples_move:
            print('  moved VAs: %s' % ' '.join(examples_move), flush=True)
        if examples_match:
            print('  MATCH VAs: %s' % ' '.join(examples_match), flush=True)
        # Per-VA table for notes.
        moved_rows = [r for r in rows if r.get('moved') and not r.get('error')]
        moved_rows.sort(key=lambda r: (r.get('after') or 99) - (r.get('before') or 0))
        for r in moved_rows[:20]:
            print('    %s  %s → %s  %s' % (
                r['va'], r.get('before'), r.get('after'),
                ' '.join(r.get('labels') or [])[:50]), flush=True)
    print('\n======== summary ========')
    print('%-12s %5s %5s %5s %5s  hit-rate' % (
        'gen', 'prey', 'move', 'MATCH', 'worse'))
    for s in summary:
        rate = (s['moved'] / s['prey']) if s['prey'] else 0
        print('%-12s %5d %5d %5d %5d  %d/%d = %.0f%%' % (
            s['gen'], s['prey'], s['moved'], s['matched_new'], s['worse'],
            s['moved'], s['prey'], 100 * rate))
    return summary


def run_dry(from_decomp=False, gens=None, max_prey=None, pool='residue'):
    gens = gens or [g[0] for g in GENERATORS]
    n_res = len(residue_rows())
    n_un = len(untrans_vas())
    print('residue unmatched: %d   untranscribed: %d   pool=%s' % (
        n_res, n_un, pool))
    # Also count raw-decomp prey (untranscribed, no wrap) for stringops.
    if pool in ('untrans', 'both') or True:
        raw_hits = {'stringops': 0, 'charret': 0, 'i64glob': 0, 'ucharbx': 0}
        for va in (untrans_vas() if pool != 'residue' else []):
            raw = load_raw_decomp(_norm_va(va))
            orig = _orig(_norm_va(va))
            if not raw:
                continue
            if _SCASB in orig or _MOVSD in orig or _STOSD in orig \
                    or '0xffffffff' in raw and '>> 2' in raw \
                    or re.search(r"while \(.*\\\\0", raw) \
                    or '= -1;' in raw and "!= '\\0'" in raw:
                raw_hits['stringops'] += 1
            if char_width_orig(orig):
                raw_hits['charret'] += 1
            if 'CARRY4' in raw or '__allmul' in raw and 'DAT_' in raw:
                raw_hits['i64glob'] += 1
            if 'CONCAT31' in raw or 'bool bVar' in raw:
                raw_hits['ucharbx'] += 1
        if pool != 'residue':
            print('raw-decomp distinguisher hits (untrans): %s' % raw_hits)
    for gen_name in gens:
        fd = from_decomp or pool == 'untrans'
        vas = _prey_vas(gen_name, from_decomp=fd, pool=pool)
        for e in _EXTRAS.get(gen_name, []):
            e = _norm_va(e)
            if e not in vas:
                vas.append(e)
        if max_prey:
            vas = vas[:max_prey]
        print('\n======== %s  prey %d (dry) ========' % (gen_name, len(vas)))
        gen = dict((n, g) for n, g, _t in GENERATORS)[gen_name]
        transform = dict((n, t) for n, _g, t in GENERATORS)[gen_name]
        n_fire = 0
        for va in vas:
            try:
                src, _ = load_wrapped(va, from_decomp=fd)
            except Exception as e:
                print('  %s  LOAD %s' % (va, e))
                continue
            orig = _orig(va)
            if gen_name == 'charret':
                yields = list(gen_charret(src, orig=orig))
                out, labels = transform(src, orig=orig)
            elif gen_name == 'i64glob':
                yields = list(gen(src))
                out, labels = transform(src, orig=orig)
            else:
                yields = list(gen(src))
                out, labels = transform(src)
            if out != src:
                n_fire += 1
            print('  %s  yields=%d  applied=%s  changed=%s' % (
                va, len(yields), labels or '-', out != src))
            for lab, cand in yields[:6]:
                print('      - %s  Δ%d chars' % (lab, abs(len(cand) - len(src))))
        print('  fired %d / %d' % (n_fire, len(vas)))


def main():
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except Exception:
        pass
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--va', help='single VA')
    ap.add_argument('--gen', choices=[g[0] for g in GENERATORS],
                    help='restrict to one generator')
    ap.add_argument('--from-decomp', action='store_true',
                    help='wrap build/ghidra_decomp (isolates the transform)')
    ap.add_argument('--validate', action='store_true',
                    help='score every prey of each generator')
    ap.add_argument('--dry-run', action='store_true',
                    help='print yields, no MSVC score')
    ap.add_argument('--no-score', action='store_true')
    ap.add_argument('--max-prey', type=int, default=0)
    ap.add_argument('--workers', type=int, default=4)
    ap.add_argument('--pool', choices=['residue', 'untrans', 'both'],
                    default='residue',
                    help='residue (refine unmatched), untrans (decomp, no '
                         'work file), or both')
    args = ap.parse_args()

    if args.dry_run and not args.va:
        run_dry(from_decomp=args.from_decomp,
                gens=[args.gen] if args.gen else None,
                max_prey=args.max_prey or None,
                pool=args.pool)
        return
    if args.validate:
        run_validate(from_decomp=args.from_decomp,
                     gens=[args.gen] if args.gen else None,
                     max_prey=args.max_prey or None,
                     workers=args.workers,
                     pool=args.pool)
        return
    if not args.va:
        ap.print_help()
        return
    run_one(args.va, gen_name=args.gen, from_decomp=args.from_decomp,
            verbose=True, score=not args.no_score)


if __name__ == '__main__':
    main()
