#!/usr/bin/env python3
"""Ghidra-to-match pipeline: take Ghidra decompiled C, clean it up, compile
with MSVC5, and check against original bytes.

This is the progressive automation engine. Each run:
1. Reads Ghidra's decompiled C for uncovered functions
2. Applies cleanup transforms (Ghidra-isms → MSVC5-compatible C)
3. Compiles via Wine + MSVC5
4. Diffs against original bytes
5. Records learnings for every function — match, close, or far

Run:
    python3 tools/ghidra_to_match.py                    # all uncovered functions
    python3 tools/ghidra_to_match.py --small             # only <=64 byte functions
    python3 tools/ghidra_to_match.py --report            # print learnings summary
    python3 tools/ghidra_to_match.py --va 0x10001000     # one function
    python3 tools/ghidra_to_match.py --errors-only       # reprocess prior errors only
    python3 tools/ghidra_to_match.py --fallback          # try individual compile on batch failures
    python3 tools/ghidra_to_match.py --refine [--max-diffs N] [--va X]
                                     [--max-rounds N] [--max-cands N]
        # hill-climb CLOSE/DIFF rows through the generator transforms;
        # writes build/ghidra_work/<va>.refined.c. Learnings CSV is written
        # back after every function (crash-safe); each row gets a
        # 'divergence' class stamp for residue grouping.
    python3 tools/ghidra_to_match.py --residue
        # group the unmatched refine candidates by divergence class
"""
import csv
import os
import re
import struct
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from concurrent.futures import ProcessPoolExecutor, as_completed
from datetime import datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

GHIDRA_DIR = os.path.join(ROOT, 'build', 'ghidra_decomp')
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
REPORT_CSV = os.path.join(ROOT, 'build', 'match', 'report.csv')
LEARNINGS_CSV = os.path.join(ROOT, 'build', 'ghidra_learnings.csv')
GLOBALS_CSV = os.path.join(ROOT, 'config', 'globals_learned.csv')
WIDTHS_CSV = os.path.join(ROOT, 'build', 'orig_global_widths.csv')

def matched_vas_all_reports():
    """Every matched VA across the three report CSVs (C, C++ EH, EXE).
    The C++/EXE reports are NOT in report.csv; filtering residue against
    report.csv alone re-hands functions already matched as C++ TUs
    (0x1003D4A0/0x1003D510 sat in the scattered class for days)."""
    out = set()
    for name in ('report.csv', 'report_cpp.csv', 'report_exe.csv'):
        p = os.path.join(ROOT, 'build', 'match', name)
        if os.path.exists(p):
            with open(p) as f:
                out |= {r['va'].lower() for r in csv.DictReader(f)
                        if r.get('status') == 'match'}
    # Fenced VAs (CRT helpers, EH funclets/fragments, thunks) are not
    # hand-C targets either — without this the residue kept offering
    # __alldiv and catch-funclet fragments as long/frame-class work.
    p = os.path.join(ROOT, 'config', 'fenced.csv')
    if os.path.exists(p):
        with open(p) as f:
            out |= {r['va'].lower() for r in csv.DictReader(f)}
    return out


def load_widths():
    """Ground-truth global types derived from the ORIGINAL bytes
    (tools/orig_widths.py): addr -> C type string ('' = address-only)."""
    w = {}
    if os.path.exists(WIDTHS_CSV):
        with open(WIDTHS_CSV) as f:
            for r in csv.DictReader(f):
                if r['ctype']:
                    w[int(r['addr'], 16)] = r['ctype']
    return w

WIDTHS = load_widths()

# ---------------------------------------------------------------------------
# Load reference data
# ---------------------------------------------------------------------------

def load_report_vas():
    """VAs already in report.csv (already have source)."""
    vas = set()
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            for r in csv.DictReader(f):
                vas.add(r['va'].lower())
    return vas

def load_functions():
    """All functions from functions_glide.csv."""
    funcs = []
    with open(os.path.join(ROOT, 'config', 'functions_glide.csv')) as f:
        for r in csv.DictReader(f):
            funcs.append({
                'va': r['va'],
                'size': int(r['size']),
                'name': r.get('name', '').strip(),
            })
    return funcs

def load_globals():
    """Known globals from globals_learned.csv."""
    g = {}
    if os.path.exists(GLOBALS_CSV):
        with open(GLOBALS_CSV) as f:
            for r in csv.DictReader(f):
                addr = r.get('addr', '').strip()
                sym = r.get('symbol', '').strip()
                if addr and sym:
                    g[addr.lower()] = sym
    return g

def load_fn_names():
    """Function VA → name from report.csv and @implements tags."""
    names = {}
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            for r in csv.DictReader(f):
                if r.get('name'):
                    names[r['va'].lower()] = r['name']
    return names


# ---------------------------------------------------------------------------
# Ghidra C cleanup transforms
# ---------------------------------------------------------------------------

def clean_ghidra_types(code):
    """Replace Ghidra type names with MSVC5-compatible ones."""
    subs = [
        # Ghidra's unkbyteN / unkuintN types
        (r'\bunkbyte10\b', 'double'),   # x87 80-bit return/temporary
        (r'\bunkbyte\d+\b', 'int'),
        (r'\bunkuint\d+\b', 'unsigned int'),
        (r'\bunkint\d+\b', 'int'),
        (r'\bundefined4\b', 'int'),
        (r'\bundefined2\b', 'short'),
        (r'\bundefined1\b', 'char'),
        (r'\bundefined8\b', 'double'),
        (r'\bundefined\b', 'char'),
        (r'\bbyte\b', 'unsigned char'),
        (r'\buint\b', 'unsigned int'),
        (r'\bulong\b', 'unsigned long'),
        (r'\bushort\b', 'unsigned short'),
        (r'\bbool\b', 'int'),
        (r'\blonglong\b', '__int64'),
        (r'\bulonglong\b', 'unsigned __int64'),
        (r'\bint64_t\b', '__int64'),
        (r'\buint64_t\b', 'unsigned __int64'),
        (r'\bint32_t\b', 'int'),
        (r'\buint32_t\b', 'unsigned int'),
        (r'\bint16_t\b', 'short'),
        (r'\buint16_t\b', 'unsigned short'),
        (r'\bint8_t\b', 'char'),
        (r'\buint8_t\b', 'unsigned char'),
        (r'\bsize_t\b', 'unsigned int'),
        # Ghidra calling conventions MSVC5 doesn't know
        # __thiscall is rewritten in fix_calling_convention (needs the signature)
        # Ghidra extended float types
        (r'\bfloat10\b', 'double'),
        (r'\bfloat8\b', 'double'),
        # Ghidra int3/int5/int6/int7 types
        (r'\bint3\b', 'int'),
        (r'\bint5\b', 'int'),
        (r'\bint6\b', 'int'),
        (r'\bint7\b', 'int'),
        (r'\buint3\b', 'unsigned int'),
        (r'\buint5\b', 'unsigned int'),
        (r'\buint6\b', 'unsigned int'),
        (r'\buint7\b', 'unsigned int'),
        # Additional Ghidra/Windows types
        (r'\bundefined3\b', 'int'),
        (r'\bSIZE_T\b', 'unsigned int'),
        (r'\bDWORD_PTR\b', 'unsigned long'),
    ]
    for pat, repl in subs:
        code = re.sub(pat, repl, code)
    # PTR_FUN_XXXXXXXX — keep as extern globals (replacing with 0 breaks &PTR_FUN_)
    # They're declared as externs in wrap_for_compile instead
    # PTR_DAT_XXXXXXXX → data pointer refs
    code = re.sub(r'\bPTR_DAT_([0-9a-fA-F]{8})\b', r'DAT_\1', code)
    # Ghidra goto labels — KEEP them (they are the original's control flow;
    # stripping them compiles semantically wrong code). Normalise the three
    # label spellings to valid C identifiers and give each label a statement
    # so `LAB_x: }` parses.
    code = re.sub(r'\bswitchD_([0-9a-fA-F]+)_caseD_([0-9a-fA-F]+)\b',
                  r'LAB_sw\1_\2', code)
    code = re.sub(r'\bjoined_r0x([0-9a-fA-F]+)\b', r'LAB_j\1', code)
    code = re.sub(r'^(\s*)(LAB_\w+)\s*:\s*$', r'\1\2: ;', code, flags=re.M)
    # &LAB_x / LAB_x where x is a known function start: Ghidra failed to make
    # a function there but the reference is a real code pointer — keep it.
    def _lab(m):
        va = '0x' + m.group(1).upper()
        if os.path.exists(os.path.join(ORIG_DIR, va + '.bin')):
            return 'FUN_' + m.group(1).lower()
        return '(void*)0'
    code = re.sub(r'&\s*LAB_([0-9a-fA-F]{8})\b', _lab, code)
    # (bare LAB_ value references in jump tables are left alone; the few
    # functions using them fail loudly instead of compiling wrong code)
    # Ghidra 'code' pointer type → callable function pointer (trailing space
    # prevents concatenation with the variable name, e.g. code *pcVar3 → funcptr pcVar3)
    code = re.sub(r'\bcode\s*\*', 'funcptr ', code)
    # Ghidra cast patterns: (*(code **)(expr)) → ((funcptr*)(expr))
    # Already handled by the above since 'code' is replaced
    # Ghidra struct member access on int: ._0_1_, ._0_2_, ._0_4_ etc
    code = re.sub(r'\._\d+_\d+_', '', code)
    # Ghidra CONCAT patterns
    code = re.sub(r'\bCONCAT\d+\b', '/* CONCAT */', code)
    # Ghidra SUB patterns (SUB41, SUB42 etc)
    code = re.sub(r'\bSUB\d+\(([^,]+),\d+\)', r'(\1)', code)
    # Infinite loops: VC5 /Od compiles `do {} while (1)` to `mov eax,1; test;
    # jne` but `for (;;)` to a bare `jmp` -- the original used for(;;).
    code = re.sub(r'\bwhile\s*\(\s*true\s*\)\s*\{', 'for (;;) {', code)
    lines = code.split('\n')
    stack = []
    for i, ln in enumerate(lines):
        m = re.match(r'^(\s*)do \{\s*$', ln)
        if m:
            stack.append((m.group(1), i))
            continue
        m = re.match(r'^(\s*)\} while\s*\(\s*true\s*\)\s*;\s*$', ln)
        if m and stack and stack[-1][0] == m.group(1):
            ind, j = stack.pop()
            lines[j] = ind + 'for (;;) {'
            lines[i] = ind + '}'
        elif re.match(r'^(\s*)\} while\b', ln) and stack and \
                stack[-1][0] == re.match(r'^(\s*)', ln).group(1):
            stack.pop()
    code = '\n'.join(lines)
    # Inlined strlen (/Oi repne scasb) that Ghidra decompiled to a manual
    # loop.  Replace the loop with a real strlen() call so VC5 re-inlines it.
    # After the loop the counter u holds 0xffffffff-(n+1); Ghidra spells the
    # length as `~u - 1` and n+1 as `~u`.  We set u = strlen(...) and rewrite
    # every `~u` use accordingly.
    def _dlemit_sub(code):
        # The display-list emit family: Ghidra renders the original's
        #   { BrDlCmd *p_ = G++; p_->op = C; p_->arg = A; }
        # (G a struct{int op,arg;}* global) as a 5-statement temp dance.
        # Rewrite it back; the wrapper types the global as BrDlCmd*.
        pat = re.compile(
            r'(\w+) = (\w+);\s*'
            r'(\w+) = \2 \+ 2;\s*'
            r'\*\2 =\s*([^;]+);\s*'
            r'\2 = \3;\s*'
            r'\1\[1\] =\s*([^;]+);')
        n = 0
        gvars = set()
        def rep(m):
            nonlocal n
            n += 1
            gvars.add(m.group(2))
            return ('{ BrDlCmd *pEmit_ = %s++; pEmit_->op = %s; '
                    'pEmit_->arg = %s; }' % (m.group(2),
                                             ' '.join(m.group(4).split()),
                                             ' '.join(m.group(5).split())))
        code = pat.sub(rep, code)
        if n:
            # first-emit variant: p1 = base_expr; ... G = p1 + 2; *p1 = C; p1[1] = A;
            code = ('#ifndef BR_DLCMD_DEFINED\n#define BR_DLCMD_DEFINED\n'
                    'typedef struct BrDlCmd { int op; int arg; } BrDlCmd;\n'
                    '#endif\n'
                    + ''.join('extern BrDlCmd *%s;\n' % g for g in sorted(gvars))
                    + code)
        return code

    def _strcpy_sub(code):
        # Inlined strcpy: strlen loop over SRC, then u = ~u, then a dword
        # copy loop and a residual byte loop into DST -> strcpy(DST, SRC).
        pat = re.compile(
            r'(\w+) = 0xffffffff;\s*'
            r'do \{\s*(\w+) = (\w+);\s*if \(\1 == 0\) break;\s*\1 = \1 - 1;\s*'
            r'\2 = \3 \+ 1;\s*(\w+) = \*\3;\s*\3 = \2;\s*\} while \(\4 != \x27\\0\x27\);\s*'
            r'\1 = ~\1;\s*'
            r'\3 = \2 \+ -\1;\s*'
            r'for \((\w+) = \1 >> 2; \5 != 0; \5 = \5 - 1\) \{\s*'
            r'\*\([\w ]+\*\)(\w+) = \*\([\w ]+\*\)\3;\s*\3 = \3 \+ 4;\s*\6 = \6 \+ 4;\s*\}\s*'
            r'for \(\1 = \1 & 3; \1 != 0; \1 = \1 - 1\) \{\s*'
            r'\*\6 = \*\3;\s*\3 = \3 \+ 1;\s*\6 = \6 \+ 1;\s*\}')
        while True:
            m = pat.search(code)
            if not m:
                return code
            src, dst = m.group(3), m.group(6)
            code = code[:m.start()] + ('strcpy(%s,%s);' % (dst, src)) + code[m.end():]

    def _memset_sub(code):
        # Inlined memset(p, 0, N): dword loop (N>>2 stores of zero) plus
        # residual byte loop (N&3).  Two dword-body shapes are seen.
        pat = re.compile(
            r'for \((?P<u>\w+) = (?P<n>[^;]+?) >> 2; (?P=u) != 0; (?P=u) = (?P=u) - 1\) \{\s*'
            r'(?:(?P<p>\w+)\[0\] = \x27\\0\x27;\s*(?P=p)\[1\] = \x27\\0\x27;\s*'
            r'(?P=p)\[2\] = \x27\\0\x27;\s*(?P=p)\[3\] = \x27\\0\x27;'
            r'|\*\([\w ]+\*\)(?P<p2>\w+) = 0;)\s*'
            r'(?P<pp>\w+) = (?P=pp) \+ 4;\s*\}\s*'
            r'for \((?P<u2>\w+) = (?P=n) & 3; (?P=u2) != 0; (?P=u2) = (?P=u2) - 1\) \{\s*'
            r'\*(?P=pp) = (?:\x27\\0\x27|0);\s*(?P=pp) = (?P=pp) \+ 1;\s*\}')
        while True:
            m = pat.search(code)
            if not m:
                return code
            ptr = m.group('pp')
            n = m.group('n').strip()
            code = code[:m.start()] + ('memset(%s,0,%s);' % (ptr, n)) + code[m.end():]

    def _strlen_sub(code):
        # Find the loop body first (unambiguous), then locate the counter
        # init and the pointer init that precede it, possibly with other
        # statements in between.
        loop_forms = [
            r'do \{\s*if \((\w+) == 0\) break;\s*\1 = \1 - 1;\s*'
            r'(\w+) = \*(\w+);\s*\3 = \3 \+ 1;\s*\} while \(\2 != \x27\\0\x27\);',
            r'do \{\s*(?:\w+ = \w+;\s*)?if \((\w+) == 0\) break;\s*\1 = \1 - 1;\s*'
            r'(\w+) = \*(\w+);\s*(\w+) = \3 \+ 1;\s*\} while \(\2 != \x27\\0\x27\);',
        ]
        changed = True
        while changed:
            changed = False
            for f in loop_forms:
                m = re.search(f, code)
                if not m:
                    continue
                u, ptr = m.group(1), m.group(3)
                # walker may be a copy: the initialized pointer is the one
                # assigned from a non-pointer-arith expression just before.
                pre = code[:m.start()]
                mi = None
                for cand in {ptr, m.group(4) if m.lastindex >= 4 else ptr}:
                    for mm in re.finditer(r'(\w+) = ([^;=]+);\s*$', pre[-400:], re.M):
                        pass
                # last assignments of u and the pointer chain before the loop
                mu = list(re.finditer(re.escape(u) + r' = 0xffffffff;\s*', pre))
                if not mu:
                    break
                mu = mu[-1]
                # find last `X = EXPR;` where X reaches ptr through copies
                names = {ptr}
                if m.lastindex and m.lastindex >= 4:
                    names.add(m.group(4))
                mp = None
                for mm in re.finditer(r'(\w+) = ([^;]+);\s*', pre):
                    if mm.group(1) in names and mm.start() > mu.start() - 800:
                        if not re.match(r'^[\w\s]*\*', mm.group(2)) or True:
                            mp = mm
                if mp is None or mp.start() < mu.start() - 800:
                    break
                expr = mp.group(2)
                if expr.strip() == '0xffffffff' or ' + 1' in expr:
                    break
                # splice: drop the counter init, the pointer init and the
                # loop, and set u = strlen(expr) at the loop's position.
                cuts = sorted([(mp.start(), mp.end()), (mu.start(), mu.end())],
                              reverse=True)
                for cs, ce in cuts:
                    pre = pre[:cs] + pre[ce:]
                tail = code[m.end():]
                stop = re.search(re.escape(u) + r' = 0xffffffff;', tail)
                scope_end = stop.start() if stop else len(tail)
                seg = tail[:scope_end]
                seg = re.sub(r'~%s - 1\b' % re.escape(u), u, seg)
                seg = re.sub(r'~%s\b' % re.escape(u), '(%s + 1)' % u, seg)
                code = (pre + ('%s = strlen(%s);' % (u, expr))
                        + seg + tail[scope_end:])
                changed = True
                break
        return code
    code = _dlemit_sub(code)
    # Drop local declarations left referencing nothing (e.g. the puVar temps
    # the emit rewrite absorbed) — /Od would give dead decls stack slots.
    def _dead_decls(code):
        for m in list(re.finditer(
                r'^\s+(?:unsigned |signed )?(?:int|char|short|long|float|double)'
                r'\s*\**\s*(\w+);\s*$', code, re.M)):
            if len(re.findall(r'\b%s\b' % re.escape(m.group(1)), code)) == 1:
                code = code.replace(m.group(0) + '\n', '', 1)
        return code
    code = _dead_decls(code)
    code = _strcpy_sub(code)
    code = _memset_sub(code)
    code = _strlen_sub(code)
    # /Od branchless ternaries Ghidra rationalised into arithmetic
    code = re.sub(r'-\(unsigned int\)\(([^()]+) != 0\) & (0x[0-9a-fA-F]+|\d+)',
                  r'((\1) ? \2 : 0)', code)
    code = re.sub(r'2 - \(unsigned int\)\(([^()]+) != 0\)',
                  r'((\1) ? 1 : 2)', code)
    # x87 intrinsics Ghidra names by opcode → the CRT names VC5 inlines at /Oi
    code = re.sub(r'\bfcos\s*\(', 'cos(', code)
    code = re.sub(r'\bfsin\s*\(', 'sin(', code)
    code = re.sub(r'\bfsqrt\s*\(', 'sqrt(', code)
    code = re.sub(r'\bfpatan\s*\(', 'atan2(', code)
    # PTR_s_XXXXX (string pointer references) → extern globals (declared in wrapper)
    # PTR_XXXXX_exref (external references) → extern globals
    # These are left in place; wrap_for_compile will declare them.
    return code

_SIG_RE = r'(^|\n)([^\n;{}/]*?)\b(%s)\s*\(([^)]*)\)\s*\n?\s*\{'

def fix_calling_convention(code, func_name, orig_bytes):
    """Rewrite the defined function's signature from the original bytes:
    - __thiscall(this, a, b) → __fastcall(this, int _edx, a, b): same ecx
      this, same stack args, same callee cleanup (BR_THISCALL1 idiom).
    - trailing `ret imm16` with no convention → __stdcall with imm/4 params
      (pads missing params as int).
    """
    m = re.search(_SIG_RE % re.escape(func_name), code)
    if not m:
        return code
    pre, params = m.group(2), m.group(4).strip()
    plist = [] if (not params or params == 'void') else [
        x.strip() for x in params.split(',')]
    if '__thiscall' in pre:
        pre = pre.replace('__thiscall', '__fastcall')
        if len(plist) >= 2:
            plist.insert(1, 'int _edx_unused')
    elif (len(orig_bytes) >= 3 and orig_bytes[-3] == 0xC2
          and not any(c in pre for c in ('__fastcall', '__stdcall', '__cdecl'))):
        imm = orig_bytes[-2] | (orig_bytes[-1] << 8)
        if imm % 4 == 0 and imm <= 0x40:
            want = imm // 4
            while len(plist) < want:
                plist.append('int _pad_%d' % len(plist))
            pre = pre.rstrip() + ' __stdcall '
    else:
        return code
    new_sig = '%s%s%s(%s)\n{' % (m.group(1), pre, func_name,
                                 ','.join(plist) if plist else 'void')
    return code[:m.start()] + new_sig + code[m.end():]


def clean_ghidra_globals(code, globals_map):
    """Replace DAT_XXXXXXXX with known global names."""
    def repl(m):
        addr = '0x' + m.group(1).lower()
        name = globals_map.get(addr)
        if name:
            return name
        return m.group(0)
    return re.sub(r'(?:_?)DAT_([0-9a-fA-F]{8})', repl, code)

def clean_ghidra_functions(code, fn_names):
    """Replace FUN_XXXXXXXX with known function names."""
    def repl(m):
        addr = '0x' + m.group(1).lower()
        name = fn_names.get(addr)
        if name:
            return name
        return m.group(0)
    return re.sub(r'FUN_([0-9a-fA-F]{8})', repl, code)

def clean_ghidra_warnings(code):
    """Remove Ghidra WARNING comments."""
    code = re.sub(r'/\*\s*WARNING:.*?\*/', '', code, flags=re.DOTALL)
    return code

def strip_ghidra_header(code):
    """Remove the Ghidra header comment we added."""
    return re.sub(r'/\*\s*Ghidra decompilation.*?\*/', '', code, flags=re.DOTALL)

_WIN32_API_NAMES = frozenset([
    'GlobalAlloc', 'GlobalFree', 'GlobalLock', 'GlobalUnlock',
    'GlobalMemoryStatus', 'GlobalReAlloc', 'GlobalSize', 'GlobalHandle',
    'GlobalFlags', 'LocalAlloc', 'LocalFree', 'LocalLock', 'LocalUnlock',
])

def _is_pointer_typed(name, code):
    """Detect whether a DAT_ global is used as a pointer (deref, arith, arrow)."""
    esc = re.escape(name)
    # deref: '*name' where the '*' is unary (preceded by an operator/open
    # paren/statement start), not a multiplication 'a * name'
    if re.search(r'(?:^|[=(,;{}?:!&|<>+\-*/])\s*\*\s*' + esc + r'\b', code, re.M):
        return True
    if re.search(r'\b' + esc + r'\s*\[', code):
        return True
    if re.search(r'\b' + esc + r'\s*->', code):
        return True
    # assigned from a pointer expression: name = (type *)expr
    if re.search(esc + r'\s*=\s*\([^)]*\*\)', code):
        return True
    return False

KNOWN_FN_NAMES = set()

def callee_decl(fn):
    """Declaration for an unresolved FUN_xxxxxxxx callee, read off ITS
    original bytes: a trailing `ret imm16` on a non-this/fastcall function
    means __stdcall with imm/4 word args (caller emits no `add esp`)."""
    m = re.match(r'FUN_([0-9a-fA-F]{8})$', fn)
    if m:
        va_hex = '0x' + m.group(1).upper()
        bin_path = os.path.join(ORIG_DIR, va_hex + '.bin')
        dec_path = os.path.join(GHIDRA_DIR, va_hex + '.c')
        if os.path.exists(bin_path) and os.path.exists(dec_path):
            b = open(bin_path, 'rb').read()
            if len(b) >= 3 and b[-3] == 0xC2:
                imm = b[-2] | (b[-1] << 8)
                sig = re.search(r'\n([^\n/]*\b%s\s*\([^)]*\))' % fn,
                                open(dec_path).read())
                sig = sig.group(1) if sig else ''
                if (imm % 4 == 0 and imm <= 0x40
                        and '__thiscall' not in sig and '__fastcall' not in sig):
                    n = imm // 4
                    return "int __stdcall %s(%s);\n" % (
                        fn, ','.join(['int'] * n) if n else 'void')
    return "int %s();\n" % fn

def wrap_for_compile(func_c, va_hex):
    """Wrap a cleaned Ghidra function in a minimal compilable file."""
    header = """/* Auto-generated from Ghidra decompilation — %s */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
""" % va_hex
    # Ghidra spells the errno read as `_errno()` + deref; without a
    # dllimport declaration that compiles to an E8 near call while the
    # original is FF 15 through the IAT (proven 0x10008DC0/0x10008E10,
    # BrFileCreateChecked/BrFileOpenChecked).
    if re.search(r'\b_errno\s*\(', func_c):
        header += "_CRTIMP int *__cdecl _errno(void);\n"

    # Extract any FUN_ calls that weren't resolved — but skip the function
    # being defined (its name appears in the body as the definition itself)
    defined_funcs = set(re.findall(r'(\w+)\s*\([^)]*\)\s*\n?\s*\{', func_c))
    unresolved = set(re.findall(r'(FUN_[0-9a-fA-F]{8})', func_c)) - defined_funcs
    for fn in sorted(unresolved):
        header += callee_decl(fn)

    # Extract any DAT_ globals that weren't resolved
    # Detect usage patterns: called as funcptr, dereferenced as pointer, or plain int
    called_dats = set(re.findall(r'\(\*(_?DAT_[0-9a-fA-F]{8})\)\s*\(', func_c))
    unresolved_dat = set(re.findall(r'(_?DAT_[0-9a-fA-F]{8})', func_c))
    predeclared = set(re.findall(r'extern BrDlCmd \*(\w+);', func_c))
    unresolved_dat -= predeclared
    for dat in sorted(unresolved_dat):
        if dat in called_dats:
            header += "extern funcptr %s;\n" % dat
        elif re.search(r'&\s*%s\b\s*\+' % re.escape(dat), func_c):
            # used as a byte-arithmetic base (&DAT + ofs): Ghidra emits
            # byte-scaled address expressions for these — must be char.
            header += "extern char %s;\n" % dat
        elif _is_pointer_typed(dat, func_c):
            header += "extern int *%s;\n" % dat
        else:
            m2 = re.match(r'_?DAT_([0-9a-fA-F]{8})$', dat)
            oracle = WIDTHS.get(int(m2.group(1), 16)) if m2 else None
            if oracle:
                header += "extern %s %s;\n" % (oracle, dat)
            elif dat.startswith('_DAT_'):
                # no byte evidence; Ghidra's '_DAT_' overlay is nearly
                # always a float constant in this binary
                header += "extern float %s;\n" % dat
            else:
                header += "extern int %s;\n" % dat

    # PTR_FUN_XXXXXXXX — function pointer globals (not replaced with 0 anymore)
    ptr_funs = set(re.findall(r'(PTR_FUN_[0-9a-fA-F]{8})', func_c))
    for pf in sorted(ptr_funs):
        header += "extern funcptr %s;\n" % pf

    # PTR_PTR_XXXXXXXX — data pointer globals
    ptr_ptrs = set(re.findall(r'(PTR_PTR_[0-9a-fA-F]{8})', func_c))
    for pp in sorted(ptr_ptrs):
        header += "extern int *%s;\n" % pp

    # Ghidra-named globals: s_*, g_*, BrG_*, BrSn*, Glob*, etc.
    # Skip names that collide with Win32 APIs (included via windows.h)
    # Names known to be functions (report.csv) that appear uncalled, e.g.
    # stored into a function-pointer slot: declare as functions, not data.
    known_fns = set(KNOWN_FN_NAMES)
    plain_called = set(re.findall(r'\b(\w+)\s*\(', func_c)) - defined_funcs
    ghidra_globals = set(re.findall(
        r'\b((?:s_|g_|BrG_|BrSn|Glob|Global)\w+)\b', func_c)) - defined_funcs
    ghidra_globals -= plain_called
    ghidra_globals -= known_fns
    # Also detect function-pointer and pointer-typed globals
    called_globals = set(re.findall(r'\(\*(\w+)\)\s*\(', func_c))
    for g in sorted(ghidra_globals):
        if g in _WIN32_API_NAMES:
            continue
        if g in called_globals:
            header += "extern funcptr %s;\n" % g
        elif _is_pointer_typed(g, func_c):
            header += "extern int *%s;\n" % g
        else:
            header += "extern int %s;\n" % g

    # Ghidra compiler temporaries ($T147 etc.) — declare as local ints
    dollar_temps = set(re.findall(r'(\$T\d+)', func_c))
    for t in sorted(dollar_temps):
        safe_name = t.replace('$', '_dollar_')
        func_c = func_c.replace(t, safe_name)
        header += "int %s;\n" % safe_name
    # ...and the ones prepare_function already sanitised to _S_Tnnn
    for t in sorted(set(re.findall(r'\b(_S_T\d+)\b', func_c))):
        header += "float %s;\n" % t

    # Ghidra stack references (stack0xNNNN) — declare as local ints
    stack_vars = set(re.findall(r'(stack0x[0-9a-fA-F]+)', func_c))
    for sv in sorted(stack_vars):
        header += "int %s;\n" % sv

    # PTR_s_XXXXX (Ghidra string pointer refs) — extern globals
    ptr_s_vars = set(re.findall(r'(PTR_s_\w+)', func_c))
    for ps in sorted(ptr_s_vars):
        header += "extern int %s;\n" % ps

    # Ghidra *_exref (external reference stubs)
    exref_vars = set(re.findall(r'(\w+_exref)\b', func_c)) - defined_funcs
    for er in sorted(exref_vars):
        header += "extern int %s;\n" % er

    # Ghidra a_XXXXXXXX (array/data refs)
    a_vars = set(re.findall(r'\b(a_[0-9a-fA-F]{6,8})\b', func_c))
    for av in sorted(a_vars):
        header += "extern int %s;\n" % av

    # Ghidra Br* function names that aren't in report.csv or already declared
    br_funcs = set(re.findall(
        r'\b(Br\w+)\b', func_c)) - defined_funcs - ghidra_globals
    # Only declare those that look like function calls (followed by '(')
    br_called = set(re.findall(r'\b(Br\w+)\s*\(', func_c)) - defined_funcs
    for bf in sorted(br_called):
        header += "int %s();\n" % bf
    # Non-called Br* that aren't globals — declare as extern int
    for bf in sorted(br_funcs - br_called):
        if bf == 'BrDlCmd':
            continue
        if bf in known_fns:
            header += "int %s();\n" % bf
        elif not any(bf.startswith(p) for p in ('BrG_', 'BrSn')):
            header += "extern int %s;\n" % bf

    # br_* lowercase game functions
    br_lower = set(re.findall(r'\b(br_\w+)\b', func_c)) - defined_funcs
    for bl in sorted(br_lower):
        header += "int %s();\n" % bl
    # known function names (from report.csv) referenced without a call
    for kf in sorted((known_fns & set(re.findall(r'\b(\w+)\b', func_c)))
                     - defined_funcs - plain_called - br_called - br_lower):
        if kf.startswith('Br') or kf.startswith('br_'):
            continue  # already declared above
        header += "int %s();\n" % kf

    # Windows struct types that MSVC5 headers may not expose directly
    # _MEMORYSTATUS, _MMCKINFO — use the non-underscore form via windows.h
    func_c = re.sub(r'\b_MEMORYSTATUS\b', 'MEMORYSTATUS', func_c)
    func_c = re.sub(r'\b_MMCKINFO\b', 'MMCKINFO', func_c)

    # __m_cNumPods / __cNumPods style Ghidra globals
    cpod_vars = set(re.findall(r'(__\w*cNumPods_[0-9a-fA-F]+)', func_c))
    for cv in sorted(cpod_vars):
        header += "extern int %s;\n" % cv

    # $-renamed identifiers (kF300_S_S537 etc.) from globals_learned.csv
    s_renamed = set(re.findall(r'\b(\w+_S_\w+)\b', func_c)) - defined_funcs
    already_declared = set()  # track what's already declared above
    for sr in sorted(s_renamed):
        if sr not in already_declared:
            header += "extern int %s;\n" % sr

    # Ghidra _MAX_INST_*, _PtFuncCompare, _MMIOINFO, etc.
    misc_undecl = set(re.findall(
        r'\b(_(?:PtFuncCompare|MMIOINFO|MAX_INST_\w+))\b', func_c))
    for m in sorted(misc_undecl):
        header += "extern int %s;\n" % m

    header += "\n"
    footer = "\n#endif /* BR_MATCHING_BUILD */\n"
    return header + func_c + footer


# ---------------------------------------------------------------------------
# Compile and check
# ---------------------------------------------------------------------------

def compile_and_check(src_text, func_name, va_hex, orig_bytes):
    """Compile source, extract function bytes, compare against original.

    Uses match_sweep.compile_variant for compilation (same path as the real
    matching pipeline), so Wine/MSVC5 paths are handled correctly.

    Returns dict with result info.
    """
    import match_diff
    import match_sweep

    tmpdir = tempfile.mkdtemp(prefix='ghidra_match_')
    src_path = os.path.join(tmpdir, 'ghidra_test.c')

    try:
        with open(src_path, 'w') as f:
            f.write(src_text)

        last_errors = ''
        for opt in ['/O2', '/Od', '/O2 /Oy-']:
            obj, errs = match_sweep.compile_variant(src_path, 'ghidra_auto', opt)
            if obj is None:
                last_errors = ' | '.join(errs)[:200]
                continue

            try:
                funcs = match_diff.parse_coff_obj(obj)
            except Exception:
                continue

            # Try to find our function under various decorated names
            candidates = [func_name, '_' + func_name, func_name.lstrip('_')]
            found_name = None
            for cand in candidates:
                if cand in funcs:
                    found_name = cand
                    break

            if found_name is None:
                if len(funcs) == 1:
                    found_name = list(funcs.keys())[0]
                else:
                    continue

            recomp_bytes, relocs = funcs[found_name]
            is_match, ndiff, recomp_sz = match_sweep.score(
                orig_bytes, recomp_bytes, relocs)

            return {
                'match': is_match,
                'diffs': ndiff,
                'orig_size': len(orig_bytes),
                'recomp_size': recomp_sz,
                'opt': opt,
                'compile_errors': '',
            }

        return {
            'match': False,
            'diffs': -1,
            'orig_size': len(orig_bytes),
            'recomp_size': 0,
            'opt': '',
            'compile_errors': last_errors,
        }

    finally:
        import shutil
        shutil.rmtree(tmpdir, ignore_errors=True)


# ---------------------------------------------------------------------------
# Main pipeline
# ---------------------------------------------------------------------------

LEARNINGS_FIELDS = [
    'va', 'size', 'name', 'result', 'diffs', 'orig_size', 'recomp_size',
    'opt', 'compile_errors', 'timestamp', 'divergence',
]

def write_learnings(rows):
    """Rewrite the learnings CSV. Cheap (~ms); called after every refine
    completion so a wedged multi-hour run loses at most one function."""
    with open(LEARNINGS_CSV, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=LEARNINGS_FIELDS, restval='',
                           extrasaction='ignore')
        w.writeheader()
        w.writerows(rows)

def prepare_function(func, globals_map, fn_names):
    """Clean a single Ghidra decompilation and return (func_name, cleaned_c)."""
    va_hex = f'0x{int(func["va"], 16):08X}'
    ghidra_file = os.path.join(GHIDRA_DIR, f'{va_hex}.c')

    with open(ghidra_file) as f:
        ghidra_c = f.read()

    cleaned = ghidra_c
    cleaned = strip_ghidra_header(cleaned)
    cleaned = clean_ghidra_warnings(cleaned)
    cleaned = clean_ghidra_types(cleaned)
    cleaned = clean_ghidra_globals(cleaned, globals_map)
    cleaned = clean_ghidra_functions(cleaned, fn_names)
    # Sanitize $ in identifiers AFTER globals/functions substitution,
    # since globals_learned.csv names can contain $ (e.g. kF300$S537)
    cleaned = re.sub(r'(\w*)\$(\w+)', lambda m: m.group(1) + '_S_' + m.group(2), cleaned)

    fname_match = re.search(r'(\w+)\s*\(', cleaned)
    func_name = fname_match.group(1) if fname_match else f'FUN_{va_hex[2:]}'

    # Rename thunk_ functions to unique names (they collide in batches)
    if func_name.startswith('thunk_'):
        unique = f'THUNK_{va_hex[2:]}'
        cleaned = cleaned.replace(func_name, unique, 1)
        func_name = unique

    return func_name, cleaned


def single_compile_and_check(func, globals_map, fn_names):
    """Compile one function individually, check against original.

    Designed to run in a worker process — all imports happen inside.
    Returns a result dict.
    """
    import match_diff
    import match_sweep

    va_hex = f'0x{int(func["va"], 16):08X}'
    try:
        func_name, cleaned = prepare_function(func, globals_map, fn_names)
    except Exception as e:
        return {
            'va': va_hex, 'size': func['size'], 'name': f'FUN_{va_hex[2:]}',
            'match': False, 'diffs': -1, 'orig_size': 0, 'recomp_size': 0,
            'opt': '', 'compile_errors': str(e)[:200],
        }

    if cleaned is None:
        return {
            'va': va_hex, 'size': func['size'], 'name': f'FUN_{va_hex[2:]}',
            'match': False, 'diffs': -1, 'orig_size': 0, 'recomp_size': 0,
            'opt': '', 'compile_errors': 'prepare failed',
        }

    KNOWN_FN_NAMES.update(fn_names.values())
    # Wrapped source is kept for inspection in build/ghidra_work/<va>.c
    work_dir = os.path.join(ROOT, 'build', 'ghidra_work')
    os.makedirs(work_dir, exist_ok=True)

    orig_file = os.path.join(ORIG_DIR, f'{va_hex}.bin')
    if not os.path.exists(orig_file):
        return {
            'va': va_hex, 'size': func['size'], 'name': func_name,
            'match': False, 'diffs': -1, 'orig_size': 0, 'recomp_size': 0,
            'opt': '', 'compile_errors': 'no orig bin',
        }
    import match_sweep
    orig_bytes = match_sweep.load_orig(orig_file, va_hex)

    cleaned = fix_calling_convention(cleaned, func_name, orig_bytes)
    compilable = wrap_for_compile(cleaned, va_hex)
    with open(os.path.join(work_dir, f'{va_hex}.c'), 'w') as f:
        f.write(compilable)

    tmpdir = tempfile.mkdtemp(prefix='ghidra_ind_')
    src_path = os.path.join(tmpdir, f'g_{va_hex}.c')
    tag = 'ghidra_' + os.path.basename(tmpdir)

    try:
        with open(src_path, 'w') as f:
            f.write(compilable)

        last_errors = ''
        best = None
        for opt in ['/O2', '/Od', '/O2 /Oy-']:
            obj, errs = match_sweep.compile_variant(src_path, tag, opt)
            if obj is None:
                last_errors = ' | '.join(errs)[:200]
                continue

            try:
                funcs = match_diff.parse_coff_obj(obj)
            except Exception:
                continue

            # decorated names: _f, @f@8, _f@8
            found_name = None
            for cand in funcs:
                base = cand.lstrip('_@').split('@')[0]
                if base == func_name:
                    found_name = cand
                    break
            if found_name is None and len(funcs) == 1:
                found_name = list(funcs.keys())[0]
            if found_name is None:
                continue

            recomp_bytes, relocs = funcs[found_name]
            is_match, ndiff, recomp_sz = match_sweep.score(
                orig_bytes, recomp_bytes, relocs)

            r = {
                'va': va_hex, 'size': func['size'], 'name': func_name,
                'match': is_match, 'diffs': ndiff,
                'orig_size': len(orig_bytes), 'recomp_size': recomp_sz,
                'opt': opt, 'compile_errors': '',
            }
            if is_match:
                return r
            if best is None or ndiff < best['diffs']:
                best = r
        if best is not None:
            return best

        return {
            'va': va_hex, 'size': func['size'], 'name': func_name,
            'match': False, 'diffs': -1,
            'orig_size': len(orig_bytes), 'recomp_size': 0,
            'opt': '', 'compile_errors': last_errors,
        }
    finally:
        import shutil
        shutil.rmtree(tmpdir, ignore_errors=True)

# ---------------------------------------------------------------------------
# Refinement: hill-climb a CLOSE function over the edits Ghidra cannot know
# ---------------------------------------------------------------------------

_REFINE_TYPES = ['char', 'unsigned char', 'short', 'unsigned short',
                 'unsigned int', 'int', 'float', 'double']
# Ghidra wraps the originally-first operand of a flipped compare in an
# identity (int)/(unsigned int) cast, which hid the flip from this regex —
# `if (bound <= (int)idx)` never offered the `idx >= bound` candidate that
# matches (proven 0x10008F90 BrObjSelCycle, found by the Grok pass).
_SW = re.compile(
    r'switch\s*\((\w+)\)\s*\{\s*'
    r'((?:\s*case\s+(?:0x[0-9a-fA-F]+|\d+):\s*)+)'
    r'(\s*return\s+[^;]+;)\s*default:\s*return\s+([^;]+);\s*\}', re.S)
_CASTINT = r'(?:\((?:unsigned )?int\)\s*)?'
_CMP_RE = re.compile(r'\(' + _CASTINT + r'([^()<>=!&|]+?)\s*(<=|>=|<|>)\s*'
                     + _CASTINT + r'([^()<>=!&|]+?)\)')
_CMP_FLIP = {'<': '>', '>': '<', '<=': '>=', '>=': '<='}

def _score_source(src_text, func_name, orig_bytes, opts, tag):
    """Compile src_text and return (diffs, opt, recomp_bytes, relocs) best
    over opts; (None, '', None, None) on error."""
    import match_diff
    import match_sweep
    tmpdir = tempfile.mkdtemp(prefix='ghidra_ref_')
    src_path = os.path.join(tmpdir, 'r.c')
    best = (None, '', None, None)
    try:
        with open(src_path, 'w') as f:
            f.write(src_text)
        for opt in opts:
            obj, errs = match_sweep.compile_variant(src_path, tag, opt)
            if obj is None:
                continue
            try:
                funcs = match_diff.parse_coff_obj(obj)
            except Exception:
                continue
            found = None
            for cand in funcs:
                if cand.lstrip('_@').split('@')[0] == func_name:
                    found = cand
                    break
            if found is None and len(funcs) == 1:
                found = list(funcs)[0]
            if found is None:
                continue
            rb, relocs = funcs[found]
            ok, nd, _ = match_sweep.score(orig_bytes, rb, relocs)
            if ok:
                return (0, opt, rb, relocs)
            if best[0] is None or nd < best[0]:
                best = (nd, opt, rb, relocs)
        return best
    finally:
        import shutil
        shutil.rmtree(tmpdir, ignore_errors=True)


def _classify_divergence(orig_bytes, rb, relocs):
    """Coarse failure class for residue grouping, spelled <class>@<first>/<n>.
    Classes: short±N (recomp too small — structural, code missing),
    long+N (extra code emitted), frame (diverges inside the first 0x10
    bytes — prologue/frame shape wrong), dense (>=5% of bytes differ —
    structural rewrite needed), scattered (localized diffs — encoding or
    register-allocation, the hill-climbable band)."""
    if rb is None:
        return 'error'
    # Strip trailing .obj 16-byte-alignment padding before sizing — 65 of
    # the first wide run's 137 'long' rows were nothing but this artifact
    # (spotted on 0x10063DB0, proven corpus-wide by the Grok pass).
    n = len(rb)
    while n > len(orig_bytes) and rb[n - 1] in (0x90, 0xCC):
        n -= 1
    rb = rb[:n]
    delta = len(rb) - len(orig_bytes)
    if delta < 0:
        return 'short%d' % delta
    trimmed = rb[:len(orig_bytes)]
    pos = [i for i in range(len(orig_bytes))
           if i not in relocs and trimmed[i] != orig_bytes[i]]
    if not pos:
        return 'match'
    first = pos[0]
    if delta > 8:
        klass = 'long+%d' % delta
    elif first < 0x10:
        klass = 'frame'
    elif len(pos) >= 20 and len(pos) >= 0.05 * len(orig_bytes):
        klass = 'dense'
    else:
        klass = 'scattered'
    return '%s@0x%x/%d' % (klass, first, len(pos))


# Ghidra `local_N` ebp-relative offsets. sizeof for the types wrap emits.
_STACKSHRED_SIZE = {
    'char': 1, 'unsigned char': 1,
    'short': 2, 'unsigned short': 2,
    'int': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4,
    'float': 4, 'double': 8,
}
_LOCAL_DECL_RE = re.compile(
    r'^([ \t]+)((?:unsigned[ \t]+)?(?:char|short|int|long|float|double))'
    r'[ \t]+(local_([0-9a-fA-F]+))([ \t]*\[[ \t]*(\d+)[ \t]*\])?[ \t]*;'
    r'[ \t]*$',
    re.M)
# Cap: C++ objects and the 0x760-class frames are not this idiom. 0x100027E0
# is 0x10; 0x1002B3F0 is 0x1c. Proven MATCH 0x100027E0 at 0x10.
_STACKSHRED_MAX = 0x40


def _orig_sub_esp(orig):
    """Imm of the first `sub esp, N` in the prologue, or None.

    Skips a hoisted `mov eax,[g]`, `push ebp; mov ebp,esp`, and the
    callee-saved pushes. 0x100027E0 is `mov eax,[g]; sub esp, 0x10`.
    """
    if not orig:
        return None
    i, n = 0, min(48, len(orig))
    while i + 2 < n:
        b = orig[i]
        if b == 0x83 and orig[i + 1] == 0xEC:  # sub esp, imm8
            return orig[i + 2]
        if b == 0x81 and orig[i + 1] == 0xEC and i + 5 < n:  # sub esp, imm32
            return struct.unpack_from('<I', orig, i + 2)[0]
        if b == 0x55 and i + 2 < n and orig[i + 1] == 0x8B and orig[i + 2] == 0xEC:
            i += 3  # push ebp; mov ebp, esp
            continue
        if b in (0x53, 0x56, 0x57, 0x55):  # push ebx/esi/edi/ebp
            i += 1
            continue
        if b == 0xA1 and i + 5 <= n:  # mov eax, [imm32]
            i += 5
            continue
        break
    return None


def transform_stackshred(src, orig=None):
    """Rebuild Ghidra-shredded stack structs into one struct of the frame size.

    Ghidra prints `char local_10[4]; int local_c; int local_8;` for a 16-byte
    stack object (the 4th dword unread) and /O2 then emits `sub esp, 0xc`.
    Orig `sub esp, 0x10`. Fold into one struct, pad holes (and up to orig's
    `sub esp` when given), keep Ghidra's field names so uses rewrite as
    `_fr.local_N`. Address-taken of the lowest local becomes `&_fr` via
    array decay / first-field address.

    Same family as BrTex3dCreate / 0x1002DEC3 (VC5-IDIOMS.md) and the
    BrTex3dExpand `sub esp, 0x68` frame. Proven MATCH 0x100027E0 (80 B,
    MCI_STATUS shred, /O2).

    Tight: 2..8 `local_N` decls, frame <= 0x40, at least one address-taken,
    no C++ EH (`unaff_FS_OFFSET`). No-op on 0x10002580 (no locals; its
    frame@0xe/68 stamp is a jcc-displacement classifier artifact over a
    store-burst coloring wall).
    """
    if 'unaff_FS_OFFSET' in src or 'puStack_' in src:
        return src, False
    decls = list(_LOCAL_DECL_RE.finditer(src))
    if not (2 <= len(decls) <= 8):
        return src, False
    parsed = []
    for m in decls:
        typ = re.sub(r'\s+', ' ', m.group(2).strip())
        name, hexoff = m.group(3), m.group(4)
        n_elem = int(m.group(6)) if m.group(6) else 1
        unit = _STACKSHRED_SIZE.get(typ)
        if unit is None:
            return src, False
        off = int(hexoff, 16)
        parsed.append({
            'm': m, 'typ': typ, 'name': name, 'off': off,
            'nbytes': unit * n_elem, 'arr': m.group(5), 'unit': unit,
        })
    # address-taken: &local_N, (unsigned long)local_N array-decay, or
    # local_N passed as a call argument.
    taken = False
    for p in parsed:
        n = p['name']
        if (re.search(r'&' + n + r'\b', src)
                or re.search(r'\(\s*(?:unsigned[ \t]+long|DWORD(?:_PTR)?)\s*\)\s*'
                             + n + r'\b', src)
                or re.search(r'[\(,]\s*' + n + r'\s*[,)]', src)):
            taken = True
            break
    if not taken:
        return src, False
    max_off = max(p['off'] for p in parsed)
    orig_frame = _orig_sub_esp(orig) if orig else None
    frame = orig_frame or max_off
    if frame < max_off or frame > _STACKSHRED_MAX or frame < 8:
        return src, False
    # Occupancy from the bottom of the frame (ebp - frame).
    occ = [None] * frame  # byte -> name
    byname = {}
    for p in parsed:
        start = frame - p['off']
        if start < 0 or start + p['nbytes'] > frame:
            return src, False
        for i in range(p['nbytes']):
            if occ[start + i] is not None:
                return src, False  # overlap
            occ[start + i] = p['name']
        byname[p['name']] = p
    # Reject a giant hole between two objects (buf[0x400] + a couple of
    # ints is not one struct). A 4-byte unread field is the 0x100027E0 hole.
    hole_run = 0
    max_hole = 0
    for b in occ:
        if b is None:
            hole_run += 1
            if hole_run > max_hole:
                max_hole = hole_run
        else:
            hole_run = 0
    if max_hole > 16:
        return src, False
    # Emit fields low-address first (declaration order = address order).
    fields = []
    i = 0
    seen = set()
    pad_n = 0
    while i < frame:
        nm = occ[i]
        if nm is None:
            n = 0
            while i + n < frame and occ[i + n] is None:
                n += 1
            while n >= 4:
                fields.append('  int _pad_%d;' % pad_n)
                pad_n += 1
                n -= 4
                i += 4
            if n:
                fields.append('  char _pad_%d[%d];' % (pad_n, n))
                pad_n += 1
                i += n
            continue
        if nm in seen:
            i += 1
            continue
        seen.add(nm)
        p = byname[nm]
        arr = p['arr'] if p['arr'] else ''
        fields.append('  %s %s%s;' % (p['typ'], p['name'], arr or ''))
        i += p['nbytes']
    struct_decl = ('  struct {\n' + '\n'.join(fields) + '\n  } _fr;\n')
    first, last = decls[0], decls[-1]
    # Only rewrite when the decls are a contiguous block (nothing but
    # whitespace between them). Interleaved iVar decls stay put.
    between = src[first.end():last.start()]
    if re.search(r'\S', between):
        # allow other local_ decls only
        if re.search(r'\S', _LOCAL_DECL_RE.sub('', between)):
            return src, False
    new_src = src[:first.start()] + struct_decl + src[last.end():]
    # Prefix remaining uses. Struct field names are inside `_fr { ... }`
    # and must not be rewritten.
    def _pfx(m, _names=set(p['name'] for p in parsed)):
        return '_fr.' + m.group(0) if m.group(0) in _names else m.group(0)
    # Split at the struct we just inserted so the field decls stay bare.
    mark = struct_decl
    ins = new_src.find(mark)
    if ins < 0:
        return src, False
    head, tail = new_src[:ins + len(mark)], new_src[ins + len(mark):]
    tail = re.sub(r'\blocal_[0-9a-fA-F]+\b', _pfx, tail)
    return head + tail, True


def _refine_candidates(src):
    """Yield (label, new_src) single-edit variants of a wrapped source."""
    # Combined folds first (one candidate each, not a search) so they always
    # take a slot in the max_cands budget — (a) retype alone can emit 80+
    # cands. Decision: docs/gen-structural2-notes.md (retnotemp/ge0;
    # lebound is NOT folded, 3 prey / 184, 0 MATCH) and
    # docs/gen-fresh-notes.md (stringops; charret is orig-gated in
    # refine_function next to callconv).
    import gen_structural2 as _gs2
    # (t) Ghidra-shredded stack struct -> one struct of the frame size.
    #     Orig-gated padding (sub esp) is applied in refine_function next
    #     to callconv; this candidate uses max(local_N) so 0x100027E0
    #     still fires without orig (max 0x10 == orig frame, hole at +0x4).
    _new, _ok = transform_stackshred(src)
    if _ok and _new != src:
        yield ('stackshred', _new)
    # (q) `i = f(...); return i != 0;` -> `return f(...) != 0;` (and `== 0`,
    #     and Ghidra's `return (uint)(i != 0)`). Orig of the call-in-the-
    #     return expression is `neg; sbb; neg` (`f7 d8 1b c0 f7 d8`); the
    #     temp form is `xor r,r; test eax; setne r; mov eax,r` (+3). `== 0`
    #     is `neg; sbb; inc` (`f7 d8 1b c0 40`). Adjacent call-then-return
    #     only — does not fold a load + intervening store (0x1006B530, orig
    #     `setne`). Unused `int tmp;` decl stays (/Od slots). Proven MATCH
    #     0x1006BAA0 / 0x1006B6E0 / 0x1006BB10 / 0x1006B4F0 / 0x10058F90.
    _new, _ = _gs2.transform_ret_notemp(src)
    if _new != src:
        yield ('retnotemp', _new)
    # (r) `-1 < x` / `x > -1` -> `x >= 0`. Orig `if (x >= 0)` is
    #     `test r,r; jl` (`85 xx 7c`); Ghidra's spelling is `cmp r,-1; jle`
    #     (`83 xx ff 7e`). Pushes may sit between test and jl (0x1006E0A0).
    #     Skips Ghidra char/short temps (`cVar*`/`sVar*` — ASCII window,
    #     0x100541B0 / 0x10054280). Does not touch `i + -1 < bound` or `<<`.
    #     Hill-climb rejects a real `x > -1` (would move away from orig).
    #     Proven moved 0x1006E130 / 0x10006460 / 0x100356B0 / 0x100031D0 /
    #     0x1006E0A0 (0 MATCH as a standalone; fold for the climb).
    _new, _ = _gs2.transform_ge0(src)
    if _new != src:
        yield ('ge0', _new)
    # (s) exploded `repne scasb` / `rep movsd` / `rep stosd` -> strcpy /
    #     strcat / strlen / memset / memcpy, plus `extern char s_*[]`
    #     (address push). Wrap's _strcpy_sub / _strlen_sub / _memset_sub
    #     miss walker-rewind, signed `i = -1`, dest-scan strcat, and
    #     dword-only stosd/movsd. One candidate, not a search. strarr
    #     first so memcpy does not steal scasb copies. Decision:
    #     docs/gen-fresh-notes.md. Proven MATCH 0x10038490 / 0x10038550 /
    #     0x100387C0 (strlen:cmp), 0x10023900 / 0x10033C90 (memcpy:imm),
    #     0x100418C0 (memset:imm), 0x10055AF0 (strcpy/strcat + memset
    #     0x104 + char[]). Do not convert a stride-loop inner copy
    #     (0x100013F0) or a comparison-only scasb (0x10040A90).
    import gen_fresh as _gf
    _new, _ = _gf.transform_stringops(src)
    if _new != src:
        yield ('stringops', _new)
    # (u) missing-code combined: shredded 16-byte mmioRead dest ->
    #     PCMWAVEFORMAT, `if (h==0) err; else BODY` -> goto, success
    #     `*out=h; return 0` -> `goto TEMPCLEANUP` joining cleanup's
    #     store+return. One candidate. Orig-gate lives in
    #     refine_function; the candidate still fires so a work file
    #     that already has the struct can climb. Proven 0x1006FFC0.
    _new, _labs = transform_misscode(src)
    if _labs and _new != src:
        yield ('misscode', _new)
    # (v) Ghidra's counter-fold across a two-store budget-checked loop body:
    #     `*p = A; if (c + N >= b) EXIT; c = c + 2N; p[1] = B; p = p + 2;
    #      if (c >= b) EXIT;` -> the original's two `c += N` halves, pOut
    #     advanced by ONE element per store, one budget check per store on its
    #     own control edge. The two forms are semantically identical (the
    #     counter on the first exit path is dead, that path returns), but the
    #     folded one lets VC5 batch the pair (`mov [r]; mov [r+2]; add r,4`),
    #     which frees the output pointer's register and rotates the allocation
    #     across the whole function. One candidate, not a search. Generic
    #     artifact -- any decompiled loop writing two elements per iteration
    #     under a running byte budget comes back folded. Proven 0x100250D0
    #     (12 sites fire unaided: -160 B / -24 insns; 15 sites with the
    #     ping-pong ones hand-finished: +1152 -> +512 B, +234 -> +81 insns).
    #     Decision: docs/idioms-A.md.
    import gen_countfold as _gcf
    _new, _n = _gcf.transform_countfold(src)
    if _n and _new != src:
        yield ('countfold', _new)
    # (x) single-use call temp -> nested call (frees the temp's callee-saved
    #     home; fixes the hoisted-import-pointer register rotation).
    #     Proven MATCH 0x1006C6A0.
    _new, _ok = transform_calltemp(src)
    if _ok and _new != src:
        yield ('calltemp', _new)
    # (h) dead re-zero after the COM failure-hr assignment.
    #     Proven with 0x100356B0.
    _new, _ok = transform_deadnull(src)
    if _ok and _new != src:
        yield ('deadnull', _new)
    # (g) walker-rewind strcpy explosion -> strcpy(dst, src).
    #     Proven MATCH 0x100367C0; ~23 work files.
    _new, _ok = transform_walkerstrcpy(src)
    if _ok and _new != src:
        yield ('walkerstrcpy', _new)
    # (f) discarded float call + bare `ftol()` -> `(int)CALL(...)`.
    #     Proven MATCH 0x1002A490 (floor clamp); ~50 work files carry the
    #     artifact.
    _new, _ok = transform_ftolfuse(src)
    if _ok and _new != src:
        yield ('ftolfuse', _new)
    # (z) single-assignment scale temp -> inline `X * K` at uses (SIB
    #     base/index pairing). Proven MATCH 0x1006E130.
    _new, _ok = transform_scaletemp(src)
    if _ok and _new != src:
        yield ('scaletemp', _new)
    # (y) zero store written before the memset group in source, sunk below
    #     it by the scheduler (C7-05 literal form vs A3 eax-reuse).
    #     Proven MATCH 0x100703D0. k=1 and k=2 variants.
    for _k in (1, 2):
        _new, _ok = transform_zerohoist(src, _k)
        if _ok and _new != src:
            yield ('zerohoist%d' % _k, _new)
    head_end = src.find('\n\n', src.find('Forward declarations'))
    head, body = src[:head_end], src[head_end:]
    # (w) signed -> unsigned char pointer: a byte read through Ghidra's
    #     `char *` widens with movsx; the original's `xor r,r; mov r8,[..]`
    #     (default promotion to an unprototyped callee — the char-window
    #     entry in docs/VC5-IDIOMS.md) needs UNSIGNED char. Ghidra types
    #     byte pointers signed by default, so byte-indexed params come
    #     back movsx-shaped. One candidate per declared identifier plus an
    #     all-at-once fold. Proven MATCH 0x10020CF0 / 0x10020D30.
    _UCP_RE = re.compile(r'(?<!unsigned )\bchar(\s*\*+\s*)(\w+)([,)\[;])')
    ucp = list(_UCP_RE.finditer(body))
    if len(ucp) > 1:
        yield ('ucharall', head + _UCP_RE.sub(r'unsigned char\1\2\3', body))
    for m in ucp:
        nb = (body[:m.start()] + 'unsigned char%s%s%s'
              % (m.group(1), m.group(2), m.group(3)) + body[m.end():])
        yield ('uchar:%s' % m.group(2)[:16], head + nb)
    # (a) retype one extern global
    for m in re.finditer(r'^extern (char|unsigned char|short|unsigned short|'
                         r'unsigned int|int|float|double) (\w+);$', head, re.M):
        cur, name = m.group(1), m.group(2)
        for t in _REFINE_TYPES:
            if t == cur:
                continue
            nh = head[:m.start()] + 'extern %s %s;' % (t, name) + head[m.end():]
            yield ('%s:%s' % (name, t), nh + body)
    # (b) flip one comparison's operand order
    for m in _CMP_RE.finditer(body):
        a, op, b = m.group(1).strip(), m.group(2), m.group(3).strip()
        nb = body[:m.start()] + '(%s %s %s)' % (b, _CMP_FLIP[op], a) + body[m.end():]
        yield ('flip:%s%s%s' % (a, op, b), head + nb)
    # (c) make one local unsigned / signed
    for m in re.finditer(r'^(\s+)(int|unsigned int) (\w+);$', body, re.M):
        t = 'unsigned int' if m.group(2) == 'int' else 'int'
        nb = body[:m.start()] + '%s%s %s;' % (m.group(1), t, m.group(3)) + body[m.end():]
        yield ('local:%s:%s' % (m.group(3), t), head + nb)
    # (p) un-fold default-equivalent switch cases Ghidra dropped: a
    #     contiguous case-run returning X plus default returning Y compiles
    #     to a 2-cmp range check, but the original's two-level jump table
    #     needs its high labels kept even when they return Y's value. A
    #     singleton extra case is folded again; a pair, or a fill through
    #     the high bound, survives. Proven 0x10024DF0 (Grok pass). The
    #     jump TABLE is data the scorer can't see — a swspan match needs
    #     its table verified against the DLL by hand (see VC5-IDIOMS.md).
    for m in _SW.finditer(body):
        nums = [int(c, 0) for c in
                re.findall(r'case\s+(0x[0-9a-fA-F]+|\d+)', m.group(2))]
        if not nums:
            continue
        lo, hi, y = min(nums), max(nums), m.group(4).strip()
        if hi - lo + 1 != len(set(nums)):
            continue  # not a contiguous run
        for high in (8, 10, 12, 15):
            if high <= hi:
                continue
            fill = ''.join('  case %d:\n' % k for k in range(hi + 1, high + 1))
            pair = ('  case %d:\n  case %d:\n' % (high - 1, high)
                    if high - 1 > hi else fill)
            # pair first: it ties with fill on code bytes (the table is
            # invisible to the scorer) but is the spelling whose emitted
            # table matched the DLL's actual bytes on 0x10024DF0
            for kind, extra in (('pair', pair), ('fill', fill)):
                repl = (m.group(0)[:m.group(0).find('default:')]
                        + extra + '    return %s;\n  default:\n'
                        '    return %s;\n  }' % (y, y))
                nb = body[:m.start()] + repl + body[m.end():]
                yield ('swspan:%s:%s:%d' % (kind, m.group(1), high),
                       head + nb)
    # (o) string global as char array: Ghidra externs a referenced string
    #     literal as `extern int s_*`, which loads its VALUE and pushes a
    #     register (`mov r,[s]; push r`); the original pushes the ADDRESS
    #     (`push offset s`), reached from `extern char s_*[]` (proven
    #     BrFileReadChecked/WriteChecked 0x10008E60/E90 by the Grok pass).
    for m in re.finditer(r'^extern int (s_\w+);$', head, re.M):
        nh = (head[:m.start()] + 'extern char %s[];' % m.group(1)
              + head[m.end():])
        yield ('strarr:%s' % m.group(1)[:20], nh + body)
    # (n) fold a trailing assignment into the return and make the function
    #     return int: Ghidra prints `void f() { g = expr; return; }` when
    #     callers ignore the result, but the original `return g = expr;`
    #     forces the value into EAX (mov eax,edx after a magic divide, and
    #     the short-form a3 store to a global).  Proven
    #     BrReplayCountFromBytes 0x10063DB0.
    vm = re.search(r'\bvoid(\s+(?:__\w+\s+)?\w+\s*\()', body)
    tails = list(re.finditer(
        r'\n(\s*)([\w\*\(\)\[\]\. \t>-]+?)\s*=\s*([^;=<>!][^;]*);\s*\n\s*return;\s*\n\}',
        body))
    if vm and tails:
        t = tails[-1]
        if t.start() > vm.start():
            nb = (body[:vm.start()] + 'int' + body[vm.start() + 4:t.start()] +
                  '\n%sreturn %s = %s;\n}' % (t.group(1), t.group(2).strip(),
                                              t.group(3).strip()) +
                  body[t.end():])
            yield ('retassign:%s' % t.group(2).strip()[:16], head + nb)
    # --- idiom transforms proven 2026-08-25 (see docs/VC5-IDIOMS.md) ---
    # (d) `(X != 0) - 1` (and Ghidra's `(X == 0) - 1`) -> branchless ternary:
    #     the ternary compiles to neg/sbb/neg/dec, the arithmetic form to setne.
    for m in re.finditer(r'\(([^()]+?)\s*(!=|==)\s*0\)\s*-\s*1', body):
        tern = ('((%s != 0) ? 0 : -1)' if m.group(2) == '!=' else
                '((%s == 0) ? 0 : -1)') % m.group(1).strip()
        nb = body[:m.start()] + tern + body[m.end():]
        yield ('tern:%s' % m.group(1).strip()[:20], head + nb)
    # (e) char-constant equality -> open range test: Ghidra prints the
    #     range-collapsed `X == '\x7f'` where the original compared `X > 0x7e`
    #     (proven BrTextBoxMeasureA/B). Only kept when the byte diff drops.
    for m in re.finditer(r"\((\w+)\s*==\s*'\\x([0-9a-fA-F]{2})'\)", body):
        c = int(m.group(2), 16)
        if c:
            nb = (body[:m.start()] +
                  "(%s > '\\x%02x')" % (m.group(1), c - 1) + body[m.end():])
            yield ('rng:%s>%02x' % (m.group(1), c - 1), head + nb)
    # (f) pointer-temp loop latch -> value temp: `pA = pB + N; pB = pB + M;`
    #     followed by `while (*pA ...)` materialises a dead lea; the value
    #     temp does not (proven BrHudTextListDraw).
    for m in re.finditer(
            r'(\w+) = (\w+) \+ (0x[0-9a-fA-F]+|\d+);\s*\n(\s*)\2 = \2 \+ '
            r'(0x[0-9a-fA-F]+|\d+);\s*\n(\s*)\} while \(\*\1( != 0)?\);', body):
        pA, pB, n, ind1, adv, ind2, nz = (m.group(1), m.group(2), m.group(3),
                                          m.group(4), m.group(5), m.group(6),
                                          m.group(7) or '')
        repl = ('%sv_%s = %s[%s];\n%s%s = %s + %s;\n%s} while (v_%s%s);'
                % ('', pA, pB, n, ind1, pB, pB, adv, ind2, pA, nz))
        nb = body[:m.start()] + repl + body[m.end():]
        # declare the value temp next to the pointer temp's decl
        nb = re.sub(r'^(\s+)(?:\w+[\w \*]*?)\*\s*%s;$' % pA,
                    r'\g<0>\n\1int v_%s;' % pA, nb, count=1, flags=re.M)
        yield ('valtemp:%s' % pA, head + nb)
    # (g) dword byte-swap rotation -> one temp holding the HIGH byte per pair
    #     (proven BrTrackFixupSegList / BrTrackSwapRec28). Ghidra's rotation:
    #       tA = E0; E0 = E3; tB = E2; E3 = tA; E2 = E1; E1 = tB;
    #     The original's:
    #       tA = E3; E3 = E0; E0 = tA; tA = E2; E2 = E1; E1 = tA;
    swp = re.compile(
        r'(\w+) = ([^;\n]+?);\s*\n(\s*)([^;\n]+?) = ([^;\n]+?);\s*\n'
        r'\3(\w+) = ([^;\n]+?);\s*\n\3([^;\n]+?) = \1;\s*\n'
        r'\3([^;\n]+?) = ([^;\n]+?);\s*\n\3([^;\n]+?) = \6;')
    for m in swp.finditer(body):
        tA, e0a, ind, e0b, e3a, tB, e2a, e3b, e2b, e1a, e1b = m.groups()
        # shape check: tA=E0; E0=E3; tB=E2; E3=tA; E2=E1; E1=tB
        if not (e0a.strip() == e0b.strip() and e3a.strip() == e3b.strip()
                and e2a.strip() == e2b.strip() and e1b.strip() == e1a.strip()):
            continue
        E0, E3, E2, E1 = e0a.strip(), e3a.strip(), e2a.strip(), e1a.strip()
        repl = ('%s = %s;\n%s%s = %s;\n%s%s = %s;\n%s%s = %s;\n%s%s = %s;\n'
                '%s%s = %s;' % (tA, E3, ind, E3, E0, ind, E0, tA, ind,
                                tA, E2, ind, E2, E1, ind, E1, tA))
        nb = body[:m.start()] + repl + body[m.end():]
        yield ('swaprot:%s' % tA, head + nb)
    # (i) `X > C` -> `X >= C+1` (and via flipped spellings): the original
    #     clamp compares against the power-of-two bound (proven BrSpanExtend:
    #     `>= 0x40` where Ghidra prints `0x3f < x`).
    for m in re.finditer(r'\(([^()<>=!&|]+?)\s*>\s*(0x[0-9a-fA-F]+|\d+)\)', body):
        x, c = m.group(1).strip(), int(m.group(2), 0)
        nb = (body[:m.start()] + '(%s >= 0x%x)' % (x, c + 1) + body[m.end():])
        yield ('geq:%s>=%x' % (x[:16], c + 1), head + nb)
    # (j) drop one narrowing cast in a masked compare: `((char)X & M)` /
    #     `((unsigned char)X & M)` -> `(X & M)` — the plain int compare
    #     narrows only the cmp (proven BrNetPeerMsgCancel).
    for m in re.finditer(r'\((?:unsigned )?char\)\s*(\*?\w+(?:\[\w+\])?)', body):
        nb = body[:m.start()] + m.group(1) + body[m.end():]
        yield ('dropcast:%s' % m.group(1)[:16], head + nb)
    # (k) short sentinel encoding: `X == -1` -> `(unsigned short)X == 0xffff`
    #     (imm8 vs imm16 compare, proven BrTextBoxMeasureA).
    for m in re.finditer(r'\((\w+)\s*==\s*-1\)', body):
        nb = (body[:m.start()] +
              '((unsigned short)%s == 0xffff)' % m.group(1) + body[m.end():])
        yield ('imm16:%s' % m.group(1), head + nb)
    # --- idiom transforms proven 2026-08-25, 0x1000EAF0 fourth pass
    #     (see the VC5-IDIOMS.md fourth-pass entry) ---
    # (l) retype an int local to unsigned short: a uint16_t variable loaded
    #     from a word array compiles to the original's `mov r16, [mem];
    #     and r32, 0xffff` (load-into-live + mask) and flips `mov; shl 3`
    #     back to `lea [r*8]` in stride scaling; an int local gives
    #     `xor r,r; mov r16, [mem]` instead (proven BrSceneDlBuild idx).
    for m in re.finditer(r'^(\s+)(int|unsigned int) (\w+);$', body, re.M):
        nb = (body[:m.start()] + '%sunsigned short %s;'
              % (m.group(1), m.group(3)) + body[m.end():])
        yield ('u16:%s' % m.group(3), head + nb)
    # (m) widen a ushort mask global to int and truncate the test: the
    #     original's `mov eax, [mask]; mov dx, [flags]; and edx, eax;
    #     test dx, dx` means the mask global was int-sized and the AND
    #     result compared 16-bit -- `(unsigned short)(flags & mask) == 0`
    #     (proven BrSceneDlBuild flag gate). Ghidra types the global
    #     ushort and the compiler then tests memory-direct instead.
    for m in re.finditer(r'^extern (unsigned short|short) (\w+);$',
                         head, re.M):
        g = m.group(2)
        um = re.search(r'\(((?:[^()]|\((?:[^()]|\([^()]*\))*\))*&\s*%s)\)'
                       r'\s*==\s*0' % re.escape(g), body)
        if um is None:
            continue
        nh = head[:m.start()] + 'extern int %s;' % g + head[m.end():]
        nb = (body[:um.start()] +
              '((unsigned short)(%s)) == 0' % um.group(1) +
              body[um.end():])
        yield ('maskw:%s' % g, nh + nb)


# ---------------------------------------------------------------------------
# misscode: shredded stack struct + else-flatten + shared-tail goto
# Orig-gated, only-if-better (charret slot). Proven 0x1006FFC0 (WaveOpenFile).
# ---------------------------------------------------------------------------

def _brace_end(s, i):
    """i points at '{'. Return index just past matching '}' or -1."""
    if i >= len(s) or s[i] != '{':
        return -1
    depth = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n and s[i] != q:
                i += 2 if s[i] == '\\' else 1
            i += 1
            continue
        if c == '/' and i + 1 < n:
            if s[i + 1] == '/':
                i = s.find('\n', i)
                if i < 0:
                    return -1
                continue
            if s[i + 1] == '*':
                j = s.find('*/', i + 2)
                if j < 0:
                    return -1
                i = j + 2
                continue
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def _dup_epilogue_orig(b):
    """True if two `ret`s share an 8-byte tail (DX5 TEMPCLEANUP duplicate)."""
    if not b:
        return False
    rets = [i for i, x in enumerate(b) if x == 0xC3]
    if len(rets) < 2:
        return False
    for i in range(len(rets)):
        for j in range(i + 1, len(rets)):
            a, c = rets[i], rets[j]
            n = 8
            if a >= n and c >= n and b[a - n:a + 1] == b[c - n:c + 1]:
                return True
    return False


def misscode_orig(b):
    """Orig-gate: duplicated epilogue, or WaveOpenFile's 0x24 frame + `cmp ,0x10; jb`."""
    if not b:
        return False
    if _dup_epilogue_orig(b):
        return True
    # `sub esp, 0x24` ... `cmp [esp+disp], 0x10; jb` (PCMWAVEFORMAT cksize).
    return (b'\x83\xec\x24' in b) and (b'\x10\x0f\x82' in b)


def _rewrite_pcmstruct(src):
    """Rebuild 4 shredded ints that mmioRead(..., 0x10) writes as PCMWAVEFORMAT.

    Ghidra's `int local_24..18` + `mmioRead(..., &local_24, 0x10)` is only
    known to touch the first dword, so /O2 drops 12 bytes of frame (short-25
    vs orig `sub esp, 0x24`) and copies garbage. A real PCMWAVEFORMAT makes
    the 16-byte write visible. Also `0xf < cksize` -> `cksize >= 0x10`
    (`cmp ,0x10; jb`)."""
    labels = []
    four = re.compile(
        r'^(\s+)(?:int|unsigned int|undefined4|DWORD) (local_\w+);\s*\n'
        r'\1(?:int|unsigned int|undefined4|DWORD) (local_\w+);\s*\n'
        r'\1(?:int|unsigned int|undefined4|DWORD) (local_\w+);\s*\n'
        r'\1(?:int|unsigned int|undefined4|DWORD) (local_\w+);',
        re.M)
    m = None
    for cand in four.finditer(src):
        a = cand.group(2)
        if re.search(r'mmioRead\s*\([^;]*&\s*%s\b[^;]*,\s*(?:0x10|16)\s*\)'
                     % re.escape(a), src):
            m = cand
            break
    if m:
        a, b, c, d = m.group(2), m.group(3), m.group(4), m.group(5)
        ind = m.group(1)
        src = src[:m.start()] + '%sPCMWAVEFORMAT pcmWaveFormat;' % ind + src[m.end():]
        src = re.sub(r'&\s*%s\b' % re.escape(a), '&pcmWaveFormat', src)
        src = re.sub(
            r'\(short\)%s\s*==\s*1' % re.escape(a),
            'pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM', src)
        src = re.sub(
            r'\*(\w+) = %s;\s*\n(\s*)\1\[1\] = %s;\s*\n'
            r'\2\1\[2\] = %s;\s*\n\2\1\[3\] = %s;'
            % (re.escape(a), re.escape(b), re.escape(c), re.escape(d)),
            r'*(PCMWAVEFORMAT *)\1 = pcmWaveFormat;', src)
        # leftover scalar uses of the first dword
        src = re.sub(r'\b%s\b' % re.escape(a), 'pcmWaveFormat', src)
        labels.append('pcmstruct')
    src2, n = re.subn(
        r'\(0xf\s*<\s*([^)<>=!&|]+?\.cksize)\)',
        r'(\1 >= 0x10)', src)
    if n:
        src = src2
        labels.append('cksize>=16')
    return src, labels


def _rewrite_zerohandle(src):
    """After `mmioClose(h, 0);` orig `xor r,r` is `h = 0` before `*out = h`."""
    labels = []
    new, n = re.subn(
        r'(mmioClose\s*\(\s*(\w+)\s*,\s*0\s*\)\s*;)'
        r'(?!\s*\2\s*=)',
        r'\1\n    \2 = 0;', src)
    if n:
        src = new
        labels.append('zerohandle')
    return src, labels


def _rewrite_elsetogoto(src):
    """Ghidra `if (h == 0) { err = K; } else { BODY }` + cleanup label ->
    `if (h == 0) { err = K; goto L; } BODY`. DX5 WaveOpenFile is gotos;
    the else wrapping claims esi for the handle (35-diff colouring wall)."""
    labels = []
    pat = re.compile(
        r'\n( +)if \((\w+) == (?:\([^)]+\)\s*)?(?:0x0|NULL|0)\) \{\n'
        r'\1  (\w+) = (0x[0-9a-fA-F]+|\d+);\n'
        r'\1\}\n'
        r'\1else \{')
    m = pat.search(src)
    if not m:
        return src, labels
    else_open = m.end() - 1  # '{'
    else_end = _brace_end(src, else_open)
    if else_end < 0:
        return src, labels
    rest = src[else_end:]
    lm = re.match(r'\s*(\w+)\s*:', rest)
    if not lm:
        return src, labels
    lab = lm.group(1)
    ind = m.group(1)
    handle, err, imm = m.group(2), m.group(3), m.group(4)
    body = src[else_open + 1:else_end - 1]
    repl = ('\n%sif (%s == 0) {\n%s  %s = %s;\n%s  goto %s;\n%s}\n'
            % (ind, handle, ind, err, imm, ind, lab, ind))
    src = src[:m.start()] + repl + body + src[else_end:]
    labels.append('elsetogoto')
    return src, labels


def _rewrite_sharedtail(src):
    """Success `*out = h; return 0;` -> `goto TAIL` joining cleanup's
    `*out = h; return err`. Orig duplicates `mov eax, esi` not `xor eax,eax`
    (the join is not proven-0). DX5 `goto TEMPCLEANUP`."""
    labels = []
    # last `*name = expr; return var;`
    tails = list(re.finditer(
        r'\n([ \t]+)\*\s*(\w+)\s*=\s*([^;]+);\s*\n'
        r'\1return\s+(\w+|0|0x0)\s*;', src))
    if len(tails) < 2:
        return src, labels
    last = tails[-1]
    out = last.group(2)
    # earlier success stores to the same out-param
    early = None
    for t in tails[:-1]:
        if t.group(2) == out:
            early = t
    if early is None:
        return src, labels
    handle = early.group(3).strip()
    handle = re.sub(r'^\([^)]+\)\s*', '', handle)
    retvar = last.group(4)
    ind_e = early.group(1)
    # collapse a wrapping `if (ret == 0) { store; return; } goto CLEAN;`
    wrap = re.search(
        r'\n([ \t]+)if \((\w+) == 0\) \{\s*'
        + re.escape(early.group(0)) +
        r'\s*\}\s*\n\1goto (\w+)\s*;', src)
    if wrap and wrap.group(2) == retvar:
        # Keep the error test: failed mmioAscend must still hit cleanup
        # (free + close), not the success tail. Orig:
        #   if ((nError = mmioAscend(...)) != 0) goto ERROR;
        #   goto TEMPCLEANUP;
        src = src[:wrap.start()] + (
            '\n%sif (%s != 0) goto %s;\n%sgoto TEMPCLEANUP;\n'
            % (wrap.group(1), wrap.group(2), wrap.group(3), wrap.group(1))
        ) + src[wrap.end():]
        labels.append('sharedtail')
    else:
        src = src[:early.start()] + (
            '\n%sgoto TEMPCLEANUP;' % ind_e
        ) + src[early.end():]
        labels.append('sharedtail')
    # re-find last tail (offsets shifted)
    tails = list(re.finditer(
        r'\n([ \t]+)\*\s*%s\s*=\s*([^;]+);\s*\n'
        r'\1return\s+(\w+|0|0x0)\s*;' % re.escape(out), src))
    if not tails:
        return src, labels
    last = tails[-1]
    ind = last.group(1)
    rhs = last.group(2).strip()
    # `*out = 0` -> `*out = handle` (handle already zeroed on the error path)
    if re.match(r'(?:\([^)]+\)\s*)?(?:0|0x0|NULL)$', rhs):
        new_rhs = handle
    else:
        new_rhs = rhs
    tail_block = ('\nTEMPCLEANUP:\n%s*%s = %s;\n%sreturn %s;'
                  % (ind, out, new_rhs, ind, last.group(3)))
    src = src[:last.start()] + tail_block + src[last.end():]
    return src, labels


_CALLTEMP_ASSIGN_RE = re.compile(
    r'^(\s*)([a-z]{1,2}Var\d+)\s*=\s*([A-Za-z_][A-Za-z0-9_]*\s*\(.*\));\s*$')


def transform_calltemp(src):
    """Collapse a Ghidra single-use call temp into a nested call.

    `pvVar1 = F(x); G(pvVar1);` -> `G(F(x));` when the very next line uses
    the temp exactly once and the temp's next mention after that line (if
    any) is a reassignment.  Named temps force VC5 to allocate the call
    result a callee-saved home, which rotates the hoisted-pointer registers
    (2-diff class); the nested spelling frees it. Proven MATCH 0x1006C6A0
    (GlobalUnlock(GlobalHandle(p)) x4).
    """
    lines = src.split('\n')
    changed = False
    i = 0
    while i < len(lines) - 1:
        m = _CALLTEMP_ASSIGN_RE.match(lines[i])
        if m:
            temp, callexpr = m.group(2), m.group(3)
            nxt = lines[i + 1]
            tok = re.compile(r'\b%s\b' % re.escape(temp))
            if len(tok.findall(nxt)) == 1 and '=' not in nxt.split('(')[0]:
                # next mention below must be a reassignment (or nothing)
                ok = True
                for later in lines[i + 2:]:
                    if tok.search(later):
                        ok = bool(re.match(r'\s*%s\s*=[^=]' %
                                           re.escape(temp), later))
                        break
                if ok:
                    lines[i + 1] = tok.sub(callexpr, nxt)
                    del lines[i]
                    changed = True
                    continue
        i += 1
    return ('\n'.join(lines), changed) if changed else (src, False)


_ZEROFILL_LOOP_RE = re.compile(
    r'^(?:\s*[a-z]{1,2}Var\d+\s*=\s*&?DAT_\w+;\n'
    r'\s*for\s*\([^)]*\)\s*\{\n'
    r'\s*\*[a-z]{1,2}Var\d+\s*=\s*0;\n'
    r'\s*[a-z]{1,2}Var\d+\s*=\s*[a-z]{1,2}Var\d+\s*\+\s*1;\n'
    r'\s*\}\n'
    r'|\s*memset\s*\(&?DAT_\w+\s*,\s*0\s*,[^;]*\);\n)+', re.M)


def transform_zerohoist(src, k=1):
    """Hoist the first k `DAT_x = 0;` lines that follow a zero-fill group
    (Ghidra stosd loops or memset calls) to just above the group.

    In source order the store sits BEFORE the memsets (where eax is not
    zero, forcing the 10-byte `C7 05` literal form) and VC5's scheduler
    sinks it below the intrinsics; written after the memsets it reuses the
    intrinsic's zeroed eax as a 5-byte `A3` store.  The C7-vs-A3 split in
    the orig picks the spelling. Proven MATCH 0x100703D0.
    """
    m = _ZEROFILL_LOOP_RE.search(src)
    if not m:
        return src, False
    tail = src[m.end():]
    hoisted = []
    zre = re.compile(r'^\s*DAT_\w+\s*=\s*0;\n')
    while len(hoisted) < k:
        zm = zre.match(tail)
        if not zm:
            break
        hoisted.append(zm.group(0))
        tail = tail[zm.end():]
    if not hoisted:
        return src, False
    return src[:m.start()] + ''.join(hoisted) + src[m.start():m.end()] + tail, True


_V = r'[a-z]{1,2}Var\d+'
_WALKER_STRCPY_RE = re.compile(
    r'(?P<len>' + _V + r') = 0xffffffff;\s*\n'
    r'\s*(?P<pw>' + _V + r') = (?P<src>[^;]+);\s*\n'
    r'\s*do \{\s*\n'
    r'\s*(?P<pe>' + _V + r') = (?P=pw);\s*\n'
    r'\s*if \((?P=len) == 0\) break;\s*\n'
    r'\s*(?P=len) = (?P=len) - 1;\s*\n'
    r'\s*(?P=pe) = (?P=pw) \+ 1;\s*\n'
    r'\s*(?P<ch>' + _V + r') = \*(?P=pw);\s*\n'
    r'\s*(?P=pw) = (?P=pe);\s*\n'
    r"\s*\} while \((?P=ch) != '\\0'\);\s*\n"
    r'\s*(?P=len) = ~(?P=len);\s*\n'
    r'\s*(?P=pw) = (?P=pe) \+ -(?P=len);\s*\n'
    r'\s*(?P=pe) = (?P<dst>[^;]+);\s*\n'
    r'\s*for \((?P<c2>' + _V + r') = (?P=len) >> 2; (?P=c2) != 0; (?P=c2) = (?P=c2) - 1\) \{\s*\n'
    r'\s*\*\(int \*\)(?P=pe) = \*\(int \*\)(?P=pw);\s*\n'
    r'\s*(?P=pw) = (?P=pw) \+ 4;\s*\n'
    r'\s*(?P=pe) = (?P=pe) \+ 4;\s*\n'
    r'\s*\}\s*\n'
    r'\s*for \((?P=len) = (?P=len) & 3; (?P=len) != 0; (?P=len) = (?P=len) - 1\) \{\s*\n'
    r'\s*\*(?P=pe) = \*(?P=pw);\s*\n'
    r'\s*(?P=pw) = (?P=pw) \+ 1;\s*\n'
    r'\s*(?P=pe) = (?P=pe) \+ 1;\s*\n'
    r'\s*\}\s*\n')


def transform_walkerstrcpy(src):
    """Ghidra's walker-rewind strcpy explosion -> `strcpy(dst, src)`.

    The exploded shape gen_fresh's strarr matcher misses: an inline
    strlen walker (0xffffffff countdown), `len = ~len`, pointer rewind
    `p = end + -len`, then the `>>2` dword loop and `&3` byte-tail loop.
    Proven MATCH 0x100367C0 (strlen-guarded copy). ~23 work files carry
    the shape.
    """
    out, changed = src, False
    while True:
        m = _WALKER_STRCPY_RE.search(out)
        if not m:
            break
        ind = re.match(r'\s*', out[:m.start()].rsplit('\n', 1)[-1]).group(0)
        repl = 'strcpy(%s, %s);\n' % (m.group('dst').strip(),
                                      m.group('src').strip())
        out = out[:m.start()] + repl + out[m.end():]
        changed = True
    return (out, changed) if changed else (src, False)


_DEADNULL_RE = re.compile(
    r'(?P<keep>^\s*\w+ = -0x7ff8fff2;\s*\n)'
    r'^\s*\w+ = (?:\([A-Za-z_]\w*\s*\*?\))?0x?0?;\s*\n', re.M)


def transform_deadnull(src):
    """Drop Ghidra's dead re-zero on the COM out-of-memory failure path.

    `if ((g = CreateEventA(...)) == 0) hr = E_OUTOFMEMORY;` comes back as
    `g = CALL(); if (g != 0) return; hr = -0x7ff8fff2; g = (HANDLE)0x0;` —
    the trailing store re-writes the zero already stored from eax and has
    no encoding in the original. Proven MATCH 0x100356B0 (with the
    `extern char s_*[]` string-address fix); 6 files carry the shape.
    """
    new, n = _DEADNULL_RE.subn(lambda m: m.group('keep'), src)
    return (new, True) if n else (src, False)


_FTOLFUSE_CALL_RE = re.compile(
    r'^(\s*)([A-Za-z_]\w*\s*\([^;]*\));\s*\n'
    r'\s*(\w+)\s*=\s*ftol\s*\(\s*\);\s*\n', re.M)
_FTOLFUSE_ASSIGN_RE = re.compile(
    r'^(\s*)([a-z]{1,2}Var\d+|local_\w+|dVar\d+|fVar\d+)\s*=\s*([^;]+);\s*\n'
    r'\s*(\w+)\s*=\s*ftol\s*\(\s*\);\s*\n', re.M)


def transform_ftolfuse(src):
    """Fuse Ghidra's discarded-float-call + bare `ftol()` artifact.

    `floor(EXPR); iVar = ftol();` is really `iVar = (int)floor(EXPR);` —
    the decompiler splits the x87 result from the __ftol call because the
    conversion has no source operand.  Also handles the assignment shape
    `dVar1 = EXPR; iVar = ftol();` -> `iVar = (int)(EXPR);` when the float
    temp has no other use.  Proven MATCH 0x1002A490 (floor clamp).
    """
    out, changed = src, False
    while True:
        m = _FTOLFUSE_CALL_RE.search(out)
        if not m:
            break
        out = (out[:m.start()] + '%s%s = (int)%s;\n'
               % (m.group(1), m.group(3), m.group(2)) + out[m.end():])
        changed = True
    pos = 0
    while True:
        m = _FTOLFUSE_ASSIGN_RE.search(out, pos)
        if not m:
            break
        temp = m.group(2)
        rest = out[:m.start()] + out[m.end():]
        # the float temp must have no use outside this pair (decl aside)
        uses = [u for u in re.finditer(r'\b%s\b' % re.escape(temp), rest)
                if not re.search(r'(?:float|double)\s+%s\s*;'
                                 % re.escape(temp),
                                 rest[max(0, u.start() - 40):u.end()])]
        if uses:
            pos = m.end()
            continue
        out = (out[:m.start()] + '%s%s = (int)(%s);\n'
               % (m.group(1), m.group(4), m.group(3)) + out[m.end():])
        changed = True
        pos = m.start()
    return (out, changed) if changed else (src, False)


_SCALETEMP_RE = re.compile(
    r'^\s*([a-z]{1,2}Var\d+)\s*=\s*'
    r'((?:\w+|\([^()]*\))\s*(?:\*|<<)\s*(?:0x[0-9a-fA-F]+|\d+));\s*\n', re.M)


def transform_scaletemp(src):
    """Inline a single-assignment `iVarN = X * K` scale temp at its uses.

    Ghidra names the scaled index; the named temp gets its own callee-saved
    home and flips the SIB base/index pairing against the original's CSE'd
    form (2-diff class: `8b 04 37` vs `8b 04 3e`). Deleting the temp and
    repeating `X * K` lets VC5's own CSE pick the original allocation.
    Proven MATCH 0x1006E130.
    """
    out, changed = src, False
    for m in list(_SCALETEMP_RE.finditer(src)):
        temp, expr = m.group(1), m.group(2)
        tok = re.compile(r'\b%s\b' % re.escape(temp))
        # single assignment only: no other `temp =` anywhere
        assigns = re.findall(r'\b%s\s*=[^=]' % re.escape(temp), src)
        if len(assigns) != 1:
            continue
        body = out.replace(m.group(0), '')
        # drop the temp's own declaration BEFORE substituting uses
        body = re.sub(r'^\s*(?:unsigned\s+)?(?:int|uint|long)\s+%s\s*;\s*\n'
                      % re.escape(temp), '', body, count=1, flags=re.M)
        body = tok.sub('(%s)' % expr, body)
        out, changed = body, True
    return (out, changed) if changed else (src, False)


def transform_misscode(src, orig=None):
    """Combined missing-code rewrite. Orig-gated when orig is supplied."""
    if orig is not None and orig and not misscode_orig(orig):
        return src, []
    labels = []
    # sharedtail before elsetogoto: the success `*out=h; return` must still
    # be nested in `if (err == 0)` for the wrap matcher.
    for fn in (_rewrite_pcmstruct, _rewrite_zerohandle,
               _rewrite_sharedtail, _rewrite_elsetogoto):
        src, lab = fn(src)
        labels.extend(lab)
    return src, labels


_CC_PE = []
def _cc_pe():
    """Parse the reference PE once per process (gen_callconv re-parses it per
    call otherwise — expensive across a wide batch). Cached in a workers-safe
    way: each spawned worker builds its own on first use."""
    if not _CC_PE:
        import gen_callconv
        _CC_PE.append(gen_callconv._pe())
    return _CC_PE[0]


def _is_callshape(va_hex, pe):
    """True if orig has the callconv-class shape (FF15 stdcall, Glide
    thunk, COM/thiscall vtable, .data funcptr)."""
    try:
        import gen_callconv as cc
        calls = cc.analyze_orig(int(va_hex, 16), pe)
    except Exception:
        return False
    for c in calls:
        n = c.get('name') or ''
        if c['kind'] in ('glide-thunk', 'thunk') and n.startswith(('gr', 'gu')):
            return True
        if c['kind'] == 'funcptr' and c['conv'] == 'stdcall':
            return True
        if (c['kind'] == 'vtable'
                and c['conv'] in ('thiscall', 'stdcall-com', 'stdcall')):
            return True
        if c['kind'] == 'import' and c['conv'] == 'stdcall':
            return True
    return False


def _wrap_va_to_work(va_hex):
    """Write build/ghidra_work/<va>.c from a decomp wrap. Returns
    (func_name, orig_size) or None. Wrap skipped these when report.csv
    already had a DIFF tag (load_report_vas is all rows, not just match)."""
    import match_sweep
    funcs = load_functions()
    target = None
    for f in funcs:
        if int(f['va'], 16) == int(va_hex, 16):
            target = f
            break
    if target is None:
        return None
    gmap = load_globals()
    fn_names = load_fn_names()
    KNOWN_FN_NAMES.update(fn_names.values())
    func_name, cleaned = prepare_function(target, gmap, fn_names)
    if cleaned is None:
        return None
    orig_file = os.path.join(ORIG_DIR, va_hex + '.bin')
    orig_bytes = match_sweep.load_orig(orig_file, va_hex)
    cleaned = fix_calling_convention(cleaned, func_name, orig_bytes)
    src = wrap_for_compile(cleaned, va_hex)
    work_dir = os.path.join(ROOT, 'build', 'ghidra_work')
    os.makedirs(work_dir, exist_ok=True)
    with open(os.path.join(work_dir, va_hex + '.c'), 'w') as f:
        f.write(src)
    return func_name, len(orig_bytes)


def refine_function(row, max_rounds=4, max_cands=80, max_diffs=None):
    """Hill-climb one CLOSE/DIFF function. Returns an updated learnings row
    and writes build/ghidra_work/<va>.refined.c when it improved."""
    va_hex = row['va']
    work = os.path.join(ROOT, 'build', 'ghidra_work', va_hex + '.c')
    orig_file = os.path.join(ORIG_DIR, va_hex + '.bin')
    if not os.path.exists(orig_file):
        return row
    if not os.path.exists(work):
        wrapped = _wrap_va_to_work(va_hex)
        if wrapped is None:
            return row
        row = dict(row)
        row['name'] = wrapped[0]
    src = open(work).read()
    import match_sweep
    orig_bytes = match_sweep.load_orig(orig_file, va_hex)
    func_name = row['name']
    # Calling-convention seed transform (gen_callconv): recover stdcall/
    # thiscall/COM-vtable convention+arity from the orig bytes and rewrite the
    # callee decls before the hill-climb, so the convention fix COMBINES with
    # every other generator. No-op when the function has no indirect/import
    # calls; failures are swallowed (never break a refine). ~223 functions
    # share this call shape.
    # Initial score tries ALL variants — the row's recorded opt can be a
    # stale artifact of a divergent best (a /Od row whose real match is /O2
    # walled 0x1001E220 until this).  The climb then stays in the winner.
    opts = ['/O2', '/Od', '/O2 /Oy-']
    tag = 'ghidra_ref_' + va_hex[2:]
    cur, cur_opt, cur_rb, cur_rl = _score_source(
        src, func_name, orig_bytes, opts, tag)
    # Try the calling-convention transform, but ADOPT it only if it does not
    # make the function worse — it mis-fires on some shapes (poisoned an
    # as-is 0-diff match, 0x100368A0, to 312). Only-if-better keeps the win
    # on the ~223 it helps without corrupting the ones it hurts.
    applied = []
    if cur is not None:
        try:
            import gen_callconv
            cc_calls = gen_callconv.analyze_orig(int(va_hex, 16), _cc_pe())
            cc_src, cc_report = gen_callconv.transform(src, cc_calls)
            if cc_report and cc_src != src:
                cc = _score_source(cc_src, func_name, orig_bytes, opts, tag)
                if cc[0] is not None and cc[0] <= cur:
                    src = cc_src
                    cur, cur_opt, cur_rb, cur_rl = cc
                    applied.append('callconv')
        except Exception:
            pass
        # char-width return: orig `mov al,1; pop*; ret` (`b0 01 5b c3`)
        # came from `char` / BrBool, not Ghidra's `undefined4` -> wrap
        # `int` (`b8 01 00 00 00 c3`). Orig-gated — an ungated int->char
        # would compile every `return 1` as `mov al,1` and burn the cand
        # budget. Only-if-better, same as callconv. Decision:
        # docs/gen-fresh-notes.md. Proven MATCH 0x10054390 (already
        # tree) and 0x10069930. Skips fnstsw helpers whose AL is a
        # status nibble (0x10006A10; _DEF_SIG already skips ushort).
        try:
            import gen_fresh as _gf
            if _gf.char_width_orig(orig_bytes):
                cr_src, cr_labs = _gf.transform_charret(src, orig=orig_bytes)
                if cr_labs and cr_src != src:
                    cr = _score_source(cr_src, func_name, orig_bytes, opts, tag)
                    if cr[0] is not None and cr[0] <= cur:
                        src = cr_src
                        cur, cur_opt, cur_rb, cur_rl = cr
                        applied.append('charret')
        except Exception:
            pass
        # stack-shred: Ghidra `local_N` cluster whose address escapes, padded
        # to orig `sub esp, N`. Only-if-better — a giant hole or a
        # coincidental local_ pair must not poison the climb. Proven MATCH
        # 0x100027E0 (MCI_STATUS, 80 B /O2). 0x10002580 is a no-op (no
        # locals). Decision: frame residue class, 2026-08-27.
        try:
            sh_src, sh_ok = transform_stackshred(src, orig=orig_bytes)
            if sh_ok and sh_src != src:
                sh = _score_source(sh_src, func_name, orig_bytes, opts, tag)
                if sh[0] is not None and sh[0] <= cur:
                    src = sh_src
                    cur, cur_opt, cur_rb, cur_rl = sh
                    applied.append('stackshred')
        except Exception:
            pass
        # missing-code: shredded PCMWAVEFORMAT + else-flatten +
        # shared-tail goto (DX5 WaveOpenFile TEMPCLEANUP). Orig-gated
        # (dup epilogue / `sub esp,0x24` + `cmp ,0x10; jb`). Only-if-
        # better, same as callconv/charret. Proven MATCH 0x1006FFC0.
        try:
            if misscode_orig(orig_bytes):
                mc_src, mc_labs = transform_misscode(src, orig=orig_bytes)
                if mc_labs and mc_src != src:
                    mc = _score_source(
                        mc_src, func_name, orig_bytes, opts, tag)
                    if mc[0] is not None and mc[0] <= cur:
                        src = mc_src
                        cur, cur_opt, cur_rb, cur_rl = mc
                        applied.extend(mc_labs)
        except Exception:
            pass
    if os.environ.get('BR_REFINE_DEBUG'):
        print('DBG', va_hex, 'origlen', len(orig_bytes), 'initial', cur,
              cur_opt, 'rb', len(cur_rb) if cur_rb else None, flush=True)
    if cur is None:
        row = dict(row)
        row['divergence'] = 'error'
        return row
    if cur > 0:
        opts = [cur_opt]
    start = cur
    ncomp = 0
    # Hill-climb only inside the diffs cap. Newly wrapped call-shape rows
    # enter with a dummy diffs=1 so as-is MATCH is recorded; a 300-diff
    # wrap must not burn 80 cands.
    do_climb = cur == 0 or max_diffs is None or cur <= max_diffs
    cc_calls = None
    try:
        import gen_callconv as _cc_mod
        cc_calls = _cc_mod.analyze_orig(int(va_hex, 16), _cc_pe())
    except Exception:
        cc_calls = None

    def _cands(src):
        # Re-offer callconv each round so it COMBINES with generators
        # (notes recipe: one candidate, not a search). The seed above
        # already applied it when better-or-equal; this catches the
        # case a later transform changes the body and the convention
        # rewrite can fire again. only-if-better is nd < cur below.
        if cc_calls is not None:
            try:
                import gen_callconv as _cc
                _csrc, _crep = _cc.transform(src, cc_calls)
                if _crep and _csrc != src:
                    yield ('callconv', _csrc)
            except Exception:
                pass
        for item in _refine_candidates(src):
            yield item

    for _ in range(max_rounds if do_climb else 0):
        if cur == 0:
            break
        improved = False
        for label, cand in _cands(src):
            ncomp += 1
            if ncomp > max_cands:
                break
            nd, opt, rb, rl = _score_source(
                cand, func_name, orig_bytes, opts, tag)
            if nd is not None and nd < cur:
                src, cur, cur_opt, cur_rb, cur_rl = cand, nd, opt, rb, rl
                applied.append(label)
                improved = True
                if cur == 0:
                    break
        if not improved or ncomp > max_cands:
            break
    row = dict(row)
    # Always record THIS run's truth — the old improvement-only condition
    # (`cur < start`) left a row that scored 0 on its very first compile
    # stuck at its stale DIFF result (0x1001E220 after the preamble fix).
    row['divergence'] = _classify_divergence(orig_bytes, cur_rb, cur_rl)
    row['diffs'] = cur
    row['opt'] = cur_opt
    row['result'] = 'MATCH' if cur == 0 else (
        'CLOSE(%d)' % cur if cur <= 5 else 'DIFF(%d)' % cur)
    row['timestamp'] = datetime.now().isoformat(timespec='seconds')
    if applied:
        with open(work.replace('.c', '.refined.c'), 'w') as f:
            f.write(src)
        row['compile_errors'] = 'refined: ' + ' '.join(applied)
    elif cur == 0:
        row['compile_errors'] = 'refined: as-is'
        stale = work.replace('.c', '.refined.c')
        if os.path.exists(stale):
            os.remove(stale)  # autofile must take the .c, not a bad climb
    return row


def run_refine(max_diffs=5, target_va=None, max_rounds=4, max_cands=80,
               min_size=0):
    # Two refine runs write-back-race over the learnings CSV (a live wide
    # run clobbered a targeted run's rows on 2026-08-25). One at a time.
    lock = LEARNINGS_CSV + '.lock'
    if os.path.exists(lock):
        try:
            os.kill(int(open(lock).read().strip() or 0), 0)
            sys.exit('another refine run is live (%s) — wait for it or '
                     'delete the lock if it crashed' % lock)
        except (OSError, ValueError):
            pass  # stale lock from a dead run
    with open(lock, 'w') as f:
        f.write(str(os.getpid()))
    try:
        _run_refine_locked(max_diffs, target_va, max_rounds, max_cands,
                           min_size)
    finally:
        os.remove(lock)


def _run_refine_locked(max_diffs, target_va, max_rounds, max_cands, min_size):
    rows = []
    with open(LEARNINGS_CSV) as f:
        rows = list(csv.DictReader(f))
    # Learnings rows go stale when a function lands in the tree by hand:
    # never re-climb a VA report.csv already calls matched (19 such rows
    # burned compiles in the first wide run). --va overrides for testing.
    tree_matched = matched_vas_all_reports()
    report_rows = []
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            report_rows = list(csv.DictReader(f))
    # Wrap call-shape VAs that never entered learnings. wrap_for_compile
    # skips anything already in report.csv (DIFF tags included), so the
    # seed never saw them. 0x10017F10 BrFadeRelease wraps 0-diff as-is;
    # callconv used to C2444 because wrap renamed FUN_ → BrFadeRelease.
    learned = {r['va'].lower() for r in rows}
    if report_rows and not target_va:
        pe = _cc_pe()
        seeded = 0
        for rr in report_rows:
            if rr['status'] == 'match':
                continue
            va = rr['va']
            if va.lower() in learned:
                continue
            orig_file = os.path.join(ORIG_DIR, va + '.bin')
            decomp = os.path.join(GHIDRA_DIR, va + '.c')
            if not (os.path.exists(orig_file) and os.path.exists(decomp)):
                continue
            try:
                osz = os.path.getsize(orig_file)
            except OSError:
                continue
            if osz < min_size:
                continue
            if not _is_callshape(va, pe):
                continue
            work = os.path.join(ROOT, 'build', 'ghidra_work', va + '.c')
            name = rr.get('name') or ('FUN_' + va[2:])
            if not os.path.exists(work):
                wrapped = _wrap_va_to_work(va)
                if wrapped is None:
                    continue
                name, osz = wrapped
            rows.append({
                'va': va, 'size': str(osz), 'name': name,
                'result': 'DIFF(1)', 'diffs': '1',
                'orig_size': str(osz), 'recomp_size': '0',
                'opt': '/O2', 'compile_errors': '',
                'timestamp': '', 'divergence': '',
            })
            learned.add(va.lower())
            seeded += 1
        if seeded:
            print('seeded %d unlearned call-shape wraps into refine'
                  % seeded, flush=True)
    todo = []
    for r in rows:
        try:
            d = int(r['diffs'])
        except ValueError:
            continue
        if int(r.get('orig_size') or 0) < min_size:
            continue  # tiny junk/thunks: unreachable from C, don't climb them
        if r['va'].lower() in tree_matched and not target_va:
            continue
        # --va tests one function regardless of the diffs cap (the
        # representative 0x1006FFC0 sat at 339, above --max-diffs 200).
        if target_va:
            if r['va'].lower() == target_va.lower() and d != 0:
                todo.append(r)
        elif 0 < d <= max_diffs:
            todo.append(r)
    if target_va and not todo:
        # --va of an unlearned DIFF-tagged function: wrap from decomp
        # and climb regardless of the diffs cap.
        tv = '0x%08X' % int(target_va, 16)
        work = os.path.join(ROOT, 'build', 'ghidra_work', tv + '.c')
        orig_file = os.path.join(ORIG_DIR, tv + '.bin')
        if os.path.exists(orig_file) and not any(
                r['va'].lower() == tv.lower() for r in rows):
            if not os.path.exists(work):
                wrapped = _wrap_va_to_work(tv)
                if wrapped:
                    name, osz = wrapped
                    rows.append({
                        'va': tv, 'size': str(osz), 'name': name,
                        'result': 'DIFF(1)', 'diffs': '1',
                        'orig_size': str(osz), 'recomp_size': '0',
                        'opt': '/O2', 'compile_errors': '',
                        'timestamp': '', 'divergence': '',
                    })
            if os.path.exists(work):
                rec = next((r for r in rows
                            if r['va'].lower() == tv.lower()), None)
                if rec is None:
                    rec = {
                        'va': tv, 'size': '0',
                        'name': 'FUN_' + tv[2:],
                        'result': 'DIFF(1)', 'diffs': '1',
                        'orig_size': str(os.path.getsize(orig_file)),
                        'recomp_size': '0', 'opt': '/O2',
                        'compile_errors': '', 'timestamp': '',
                        'divergence': '',
                    }
                    rows.append(rec)
                try:
                    d = int(rec.get('diffs') or 1)
                except ValueError:
                    d = 1
                if d != 0:
                    todo.append(rec)
    # Biggest payoff first, so an interrupted run banked the valuable half.
    todo.sort(key=lambda r: -int(r.get('orig_size') or 0))
    print(f'refining {len(todo)} functions with 1..{max_diffs} diffs '
          f'(rounds={max_rounds} cands={max_cands})', flush=True)
    WORKERS = min(os.cpu_count() or 4, 10)
    out = {}
    done = 0
    with ProcessPoolExecutor(max_workers=WORKERS) as pool:
        # max_rounds/max_cands ride along explicitly: workers are spawned
        # (macOS), so mutated parent globals would never reach them.
        futs = {pool.submit(refine_function, r, max_rounds, max_cands,
                            max_diffs): r['va']
                for r in todo}
        for fut in as_completed(futs):
            try:
                nr = fut.result()
            except Exception as e:
                print('  !!', futs[fut], e, flush=True)
                continue
            out[nr['va']] = nr
            done += 1
            # Crash-safe: merge back after EVERY completion, not at the end.
            write_learnings([out.get(r['va'], r) for r in rows])
            if nr.get('compile_errors', '').startswith('refined'):
                print(f"    {nr['result']:10s} {nr['va']} {nr['name']}  {nr['compile_errors']}",
                      flush=True)
            if done % 25 == 0:
                print(f'  ... {done}/{len(todo)}', flush=True)
    n_match = sum(1 for r in out.values() if r['result'] == 'MATCH')
    print(f'refine: {n_match} new MATCH of {len(todo)}', flush=True)
    if n_match:
        print('file them: python3 tools/autofile.py', flush=True)


def print_residue():
    """Group refined-but-unmatched rows by divergence class so the hand pass
    starts at 'which wall is biggest', not at raw diffs."""
    with open(LEARNINGS_CSV) as f:
        rows = list(csv.DictReader(f))
    # Stale learnings rows for functions the tree already matched by hand
    # poisoned a hand-off once (0x10008F90 was recommended as a target a day
    # after it was committed) — filter them out of the report.
    tree_matched = matched_vas_all_reports()
    att = [r for r in rows if r.get('divergence')
           and r['divergence'] not in ('', 'match')
           and r['va'].lower() not in tree_matched]
    if not att:
        print('no divergence data yet — run --refine first')
        return
    groups = defaultdict(list)
    for r in att:
        groups[r['divergence'].split('@')[0].split('+')[0].rstrip(
            '-0123456789')].append(r)
    print(f'{len(att)} unmatched refine candidates by divergence class:\n')
    for k in sorted(groups, key=lambda k: -sum(
            int(r.get('orig_size') or 0) for r in groups[k])):
        g = groups[k]
        tot = sum(int(r.get('orig_size') or 0) for r in g)
        print(f'  {k:10s} {len(g):4d} fns  {tot / 1024.0:7.1f} KB')
        for r in sorted(g, key=lambda r: -int(r.get('orig_size') or 0))[:5]:
            print(f'      {r["va"]}  {r["orig_size"]:>6s}B  {r["diffs"]:>4s} diffs'
                  f'  {r["divergence"]:20s}  {r["name"]}')
    print('\nscattered = hill-climbable (needs a new generator idiom);')
    print('frame/short/dense = structural — hand-solve one, mint a transform.')


def load_prior_errors():
    """Load VAs that were ERROR in the last run."""
    if not os.path.exists(LEARNINGS_CSV):
        return set()
    vas = set()
    with open(LEARNINGS_CSV) as f:
        for r in csv.DictReader(f):
            if r['result'] == 'ERROR':
                vas.add(r['va'].lower())
    return vas


def run(target_va=None, small_only=False, dry_run=False,
        errors_only=False, do_fallback=False):
    covered_vas = load_report_vas()
    all_funcs = load_functions()
    globals_map = load_globals()
    fn_names = load_fn_names()

    if errors_only:
        prior_errors = load_prior_errors()

    targets = []
    for f in all_funcs:
        va = f['va'].lower()
        if va in covered_vas:
            continue
        if errors_only and va not in prior_errors:
            continue
        ghidra_file = os.path.join(GHIDRA_DIR, f'0x{int(va, 16):08X}.c')
        if not os.path.exists(ghidra_file):
            continue
        orig_file = os.path.join(ORIG_DIR, f'0x{int(va, 16):08X}.bin')
        if not os.path.exists(orig_file):
            continue
        targets.append(f)

    if target_va:
        tv = target_va.lower()
        targets = [f for f in targets if f['va'].lower() == tv]

    if small_only:
        targets = [f for f in targets if f['size'] <= 64]

    targets.sort(key=lambda f: f['size'])

    print(f'{len(targets)} functions to process', flush=True)
    if dry_run:
        for f in targets[:20]:
            print(f"  {f['va']}  {f['size']:5d}b  {f.get('name','')}")
        if len(targets) > 20:
            print(f"  ... and {len(targets)-20} more")
        return

    # Merge: keep prior rows for every VA not being reprocessed this run, so a
    # --va / --small / --errors-only run never clobbers the rest of the table.
    prior_results = []
    target_vas = {f['va'].lower() for f in targets}
    if os.path.exists(LEARNINGS_CSV):
        with open(LEARNINGS_CSV) as f:
            for r in csv.DictReader(f):
                if r['va'].lower() in target_vas or r['va'].lower() in covered_vas:
                    continue
                prior_results.append(r)

    learnings = list(prior_results)
    matches = sum(1 for r in prior_results if r['result'] == 'MATCH')
    close = sum(1 for r in prior_results if r['result'].startswith('CLOSE'))
    far = sum(1 for r in prior_results if r['result'].startswith('DIFF'))
    errors = sum(1 for r in prior_results if r['result'] == 'ERROR')

    WORKERS = min(os.cpu_count() or 4, 10)

    print(f'  {len(targets)} functions, {WORKERS} workers', flush=True)

    done = 0
    with ProcessPoolExecutor(max_workers=WORKERS) as pool:
        futures = {
            pool.submit(single_compile_and_check, func, globals_map,
                        fn_names): func
            for func in targets
        }
        for fut in as_completed(futures):
            done += 1
            try:
                r = fut.result()
            except Exception as e:
                continue

            if r['match']:
                matches += 1
                status = 'MATCH'
            elif r['diffs'] < 0:
                errors += 1
                status = 'ERROR'
            elif r['diffs'] <= 5:
                close += 1
                status = f"CLOSE({r['diffs']})"
            else:
                far += 1
                status = f"DIFF({r['diffs']})"

            learnings.append({
                'va': r['va'],
                'size': r['size'],
                'name': r['name'],
                'result': status,
                'diffs': r['diffs'],
                'orig_size': r['orig_size'],
                'recomp_size': r['recomp_size'],
                'opt': r['opt'],
                'compile_errors': r.get('compile_errors', ''),
                'timestamp': datetime.now().isoformat(timespec='seconds'),
            })

            if r['match']:
                print(f'    MATCH  {r["va"]} {r["size"]:4d}b {r["name"]}',
                      flush=True)
            if done % 100 == 0:
                print(f'  [{done}/{len(targets)}] done, '
                      f'{matches} matches so far', flush=True)

    os.makedirs(os.path.dirname(LEARNINGS_CSV), exist_ok=True)
    with open(LEARNINGS_CSV, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=LEARNINGS_FIELDS)
        w.writeheader()
        w.writerows(learnings)

    print(f'\n{"=" * 60}', flush=True)
    print(f'  MATCH : {matches:4d}')
    print(f'  CLOSE : {close:4d}  (<=5 diffs)')
    print(f'  FAR   : {far:4d}')
    print(f'  ERROR : {errors:4d}  (compile failures)')
    print(f'  TOTAL : {matches+close+far+errors:4d}')
    print(f'{"=" * 60}')
    print(f'Learnings written to: {LEARNINGS_CSV}', flush=True)


def print_report():
    if not os.path.exists(LEARNINGS_CSV):
        print('No learnings yet. Run: python3 tools/ghidra_to_match.py')
        return
    with open(LEARNINGS_CSV) as f:
        rows = list(csv.DictReader(f))

    results = Counter()
    for r in rows:
        if r['result'] == 'MATCH':
            results['match'] += 1
        elif r['result'].startswith('CLOSE'):
            results['close'] += 1
        elif r['result'].startswith('DIFF'):
            results['diff'] += 1
        elif r['result'] == 'ERROR':
            results['error'] += 1

    print(f'\n{"=" * 60}')
    print(f'  Ghidra pipeline learnings ({len(rows)} functions)')
    print(f'{"=" * 60}')
    for k in ['match', 'close', 'diff', 'error']:
        print(f'  {k:10s} {results.get(k, 0):4d}')

    # Show close functions (best leads)
    close = [r for r in rows if r['result'].startswith('CLOSE')]
    if close:
        print(f'\nClosest leads (<=5 diffs):')
        close.sort(key=lambda r: int(r['diffs']))
        for r in close[:20]:
            print(f"  {r['va']}  {r['size']:>5s}b  {r['diffs']:>2s} diffs  {r['name']}")


def main():
    if '--report' in sys.argv:
        print_report()
        return

    target_va = None
    small_only = '--small' in sys.argv
    dry_run = '--dry-run' in sys.argv
    errors_only = '--errors-only' in sys.argv
    do_fallback = '--fallback' in sys.argv

    max_diffs = 5
    max_rounds = 4
    max_cands = 80
    min_size = 0
    for i, arg in enumerate(sys.argv[1:], 1):
        if arg == '--va' and i < len(sys.argv) - 1:
            target_va = sys.argv[i + 1]
        if arg == '--max-diffs' and i < len(sys.argv) - 1:
            max_diffs = int(sys.argv[i + 1])
        if arg == '--max-rounds' and i < len(sys.argv) - 1:
            max_rounds = int(sys.argv[i + 1])
        if arg == '--max-cands' and i < len(sys.argv) - 1:
            max_cands = int(sys.argv[i + 1])
        if arg == '--min-size' and i < len(sys.argv) - 1:
            min_size = int(sys.argv[i + 1])

    if '--residue' in sys.argv:
        print_residue()
        return

    if '--refine' in sys.argv:
        run_refine(max_diffs=max_diffs, target_va=target_va,
                   max_rounds=max_rounds, max_cands=max_cands,
                   min_size=min_size)
        return

    run(target_va=target_va, small_only=small_only, dry_run=dry_run,
        errors_only=errors_only, do_fallback=do_fallback)


if __name__ == '__main__':
    main()
