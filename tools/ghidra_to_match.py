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


def refine_function(row, max_rounds=4, max_cands=80):
    """Hill-climb one CLOSE/DIFF function. Returns an updated learnings row
    and writes build/ghidra_work/<va>.refined.c when it improved."""
    va_hex = row['va']
    work = os.path.join(ROOT, 'build', 'ghidra_work', va_hex + '.c')
    orig_file = os.path.join(ORIG_DIR, va_hex + '.bin')
    if not (os.path.exists(work) and os.path.exists(orig_file)):
        return row
    src = open(work).read()
    import match_sweep
    orig_bytes = match_sweep.load_orig(orig_file, va_hex)
    func_name = row['name']
    # Initial score tries ALL variants — the row's recorded opt can be a
    # stale artifact of a divergent best (a /Od row whose real match is /O2
    # walled 0x1001E220 until this).  The climb then stays in the winner.
    opts = ['/O2', '/Od', '/O2 /Oy-']
    tag = 'ghidra_ref_' + va_hex[2:]
    cur, cur_opt, cur_rb, cur_rl = _score_source(
        src, func_name, orig_bytes, opts, tag)
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
    tree_matched = set()
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            tree_matched = {r['va'].lower() for r in csv.DictReader(f)
                            if r['status'] == 'match'}
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
        if 0 < d <= max_diffs and (not target_va or r['va'].lower() == target_va.lower()):
            todo.append(r)
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
        futs = {pool.submit(refine_function, r, max_rounds, max_cands): r['va']
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
    tree_matched = set()
    if os.path.exists(REPORT_CSV):
        with open(REPORT_CSV) as f:
            tree_matched = {r['va'].lower() for r in csv.DictReader(f)
                            if r['status'] == 'match'}
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
