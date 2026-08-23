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
        # hill-climb CLOSE rows over global retyping / comparison flips /
        # local signedness; writes build/ghidra_work/<va>.refined.c
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
    # Ghidra goto labels — replace gotos with no-op statements (not comments,
    # which leave if/else blocks without a body)
    code = re.sub(r'\bgoto\s+LAB_[0-9a-fA-F]+\s*;', '(void)0;', code)
    code = re.sub(r'LAB_[0-9a-fA-F]+\s*:', '/* label */ ;', code)
    # &LAB_x / LAB_x where x is a known function start: Ghidra failed to make
    # a function there but the reference is a real code pointer — keep it.
    def _lab(m):
        va = '0x' + m.group(1).upper()
        if os.path.exists(os.path.join(ORIG_DIR, va + '.bin')):
            return 'FUN_' + m.group(1).lower()
        return '(void*)0'
    code = re.sub(r'&\s*LAB_([0-9a-fA-F]{8})\b', _lab, code)
    # Any remaining LAB_ references (switch tables, address-of) → 0
    code = re.sub(r'\bLAB_[0-9a-fA-F]+\b', '0', code)
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
    for dat in sorted(unresolved_dat):
        if dat in called_dats:
            header += "extern funcptr %s;\n" % dat
        elif _is_pointer_typed(dat, func_c):
            header += "extern int *%s;\n" % dat
        elif dat.startswith('_DAT_'):
            # Ghidra's '_DAT_' prefix marks a typed (non-int) overlay at the
            # address; in this binary that is nearly always a float constant.
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
        for opt in ['/O2', '/Od']:
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
    'opt', 'compile_errors', 'timestamp',
]

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
    with open(orig_file, 'rb') as f:
        orig_bytes = f.read()

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
        for opt in ['/O2', '/Od']:
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
_CMP_RE = re.compile(r'\(([^()<>=!&|]+?)\s*(<=|>=|<|>)\s*([^()<>=!&|]+?)\)')
_CMP_FLIP = {'<': '>', '>': '<', '<=': '>=', '>=': '<='}

def _score_source(src_text, func_name, orig_bytes, opts, tag):
    """Compile src_text and return (diffs, opt) best over opts; (None, '') on error."""
    import match_diff
    import match_sweep
    tmpdir = tempfile.mkdtemp(prefix='ghidra_ref_')
    src_path = os.path.join(tmpdir, 'r.c')
    best = (None, '')
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
                return (0, opt)
            if best[0] is None or nd < best[0]:
                best = (nd, opt)
        return best
    finally:
        import shutil
        shutil.rmtree(tmpdir, ignore_errors=True)


def _refine_candidates(src):
    """Yield (label, new_src) single-edit variants of a wrapped source."""
    head_end = src.find('\n\n', src.find('Forward declarations'))
    head, body = src[:head_end], src[head_end:]
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


def refine_function(row, max_rounds=4, max_cands=80):
    """Hill-climb one CLOSE/DIFF function. Returns an updated learnings row
    and writes build/ghidra_work/<va>.refined.c when it improved."""
    va_hex = row['va']
    work = os.path.join(ROOT, 'build', 'ghidra_work', va_hex + '.c')
    orig_file = os.path.join(ORIG_DIR, va_hex + '.bin')
    if not (os.path.exists(work) and os.path.exists(orig_file)):
        return row
    src = open(work).read()
    orig_bytes = open(orig_file, 'rb').read()
    func_name = row['name']
    opts = [row['opt']] if row.get('opt') else ['/O2', '/Od']
    tag = 'ghidra_ref_' + va_hex[2:]
    cur, cur_opt = _score_source(src, func_name, orig_bytes, opts, tag)
    if cur is None:
        return row
    start = cur
    applied = []
    ncomp = 0
    for _ in range(max_rounds):
        if cur == 0:
            break
        improved = False
        for label, cand in _refine_candidates(src):
            ncomp += 1
            if ncomp > max_cands:
                break
            nd, opt = _score_source(cand, func_name, orig_bytes, opts, tag)
            if nd is not None and nd < cur:
                src, cur, cur_opt = cand, nd, opt
                applied.append(label)
                improved = True
                if cur == 0:
                    break
        if not improved or ncomp > max_cands:
            break
    if cur < start:
        with open(work.replace('.c', '.refined.c'), 'w') as f:
            f.write(src)
        row = dict(row)
        row['diffs'] = cur
        row['opt'] = cur_opt
        row['result'] = 'MATCH' if cur == 0 else (
            'CLOSE(%d)' % cur if cur <= 5 else 'DIFF(%d)' % cur)
        row['compile_errors'] = 'refined: ' + ' '.join(applied)
        row['timestamp'] = datetime.now().isoformat(timespec='seconds')
    return row


def run_refine(max_diffs=5, target_va=None):
    rows = []
    with open(LEARNINGS_CSV) as f:
        rows = list(csv.DictReader(f))
    todo = []
    for r in rows:
        try:
            d = int(r['diffs'])
        except ValueError:
            continue
        if 0 < d <= max_diffs and (not target_va or r['va'].lower() == target_va.lower()):
            todo.append(r)
    print(f'refining {len(todo)} functions with 1..{max_diffs} diffs', flush=True)
    WORKERS = min(os.cpu_count() or 4, 10)
    out = {}
    with ProcessPoolExecutor(max_workers=WORKERS) as pool:
        futs = {pool.submit(refine_function, r): r['va'] for r in todo}
        for fut in as_completed(futs):
            try:
                nr = fut.result()
            except Exception as e:
                print('  !!', futs[fut], e, flush=True)
                continue
            out[nr['va']] = nr
            if nr.get('compile_errors', '').startswith('refined'):
                print(f"    {nr['result']:10s} {nr['va']} {nr['name']}  {nr['compile_errors']}",
                      flush=True)
    merged = [out.get(r['va'], r) for r in rows]
    with open(LEARNINGS_CSV, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=LEARNINGS_FIELDS)
        w.writeheader()
        w.writerows(merged)
    n_match = sum(1 for r in out.values() if r['result'] == 'MATCH')
    print(f'refine: {n_match} new MATCH of {len(todo)}', flush=True)


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
    for i, arg in enumerate(sys.argv[1:], 1):
        if arg == '--va' and i < len(sys.argv) - 1:
            target_va = sys.argv[i + 1]
        if arg == '--max-diffs' and i < len(sys.argv) - 1:
            max_diffs = int(sys.argv[i + 1])

    if '--refine' in sys.argv:
        run_refine(max_diffs=max_diffs, target_va=target_va)
        return

    run(target_va=target_va, small_only=small_only, dry_run=dry_run,
        errors_only=errors_only, do_fallback=do_fallback)


if __name__ == '__main__':
    main()
