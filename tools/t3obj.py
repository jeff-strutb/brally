#!/usr/bin/env python3
"""t3obj.py -- which compiled object implements a VA, and its bytes.

Lifted out of the retired synthetic-seed oracle (t3b_verify.py) because the
selection logic is sound and the live oracle (tools/t3live.py, object mode)
needs exactly it: a name can be shared across TUs, with a D3D twin, or with a
superseded C-lane twin of a C++-lane row, and the transcription under test
must be the one that OWNS the VA.
"""
from __future__ import print_function
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

_OBJ_INDEX = None
_VA_OWNER = None
OBJ_DIRS = ('obj_O2', 'obj_O2y', 'obj_O2p', 'obj_Od', 'obj_cpp')


def va_owner(va_hex):
    """How to select the obj that OWNS this VA -> (kind, key), or None.

    C lane: ('base', '<file>.obj') -- a source compiles to its basename.obj.
    C++ lane: ('cppdir', None) -- report_cpp.csv OVERRIDES report.csv, so a
    superseded C twin never wins."""
    global _VA_OWNER
    if _VA_OWNER is None:
        _VA_OWNER = {}
        rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
        if os.path.exists(rep):
            for r in csv.DictReader(open(rep)):
                f = r.get('file') or ''
                if f:
                    _VA_OWNER[r['va'].lower()] = ('base', os.path.splitext(os.path.basename(f))[0] + '.obj')
        repc = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
        if os.path.exists(repc):
            for r in csv.DictReader(open(repc)):
                if r.get('file'):
                    _VA_OWNER[r['va'].lower()] = ('cppdir', None)
    return _VA_OWNER.get(va_hex.lower())


def obj_index():
    """name -> [(obj path, compiled size)], demangled method names included."""
    global _OBJ_INDEX
    if _OBJ_INDEX is None:
        from match_diff import parse_coff_obj
        import reloc_fill
        _OBJ_INDEX = {}
        for d in OBJ_DIRS:
            p = os.path.join(ROOT, 'build', 'match', d)
            if not os.path.isdir(p):
                continue
            for f in sorted(os.listdir(p)):
                if not f.endswith('.obj'):
                    continue
                try:
                    parsed = parse_coff_obj(os.path.join(p, f))
                except Exception:
                    continue
                for n, rec in parsed.items():
                    code = rec[0] if isinstance(rec, tuple) else rec
                    ent = (os.path.join(p, f), len(code))
                    _OBJ_INDEX.setdefault(n, []).append(ent)
                    dm = reloc_fill._demangle_method(n)
                    if dm:
                        _OBJ_INDEX.setdefault(dm, []).append(ent)
    return _OBJ_INDEX


def load_orig(va_hex):
    for v in (va_hex, va_hex.lower()):
        p = os.path.join(ROOT, 'build', 'match', 'orig', v + '.bin')
        if os.path.exists(p):
            return open(p, 'rb').read()
    return None


def pick(va_hex, name, orig_len):
    """(obj path, size) of the transcription that implements VA, or None."""
    cands = obj_index().get(name)
    if not cands:
        return None
    owner = va_owner(va_hex)
    owned = []
    if owner and owner[0] == 'cppdir':
        owned = [c for c in cands if os.path.basename(os.path.dirname(c[0])) == 'obj_cpp']
    elif owner and owner[0] == 'base':
        owned = [c for c in cands if os.path.basename(c[0]) == owner[1]]
    return min(owned or cands, key=lambda e: abs(e[1] - orig_len))


def nparams(src_file, name):
    """Stack-argument dword count of `name` from its C/C++ definition in
    `src_file`, or None.  Used only to bound the incoming-argument slots the
    live oracle leaves out of its comparison (the callee owns them); a
    __fastcall/BR_THISCALL1 definition's first register-eligible arguments are
    not on the stack and are not counted."""
    import re
    try:
        text = open(os.path.join(ROOT, src_file), errors='replace').read()
    except OSError:
        return None
    short = name.split('::')[-1]
    for m in re.finditer(r'^[A-Za-z_][\w \t\*]*?\b(?:\w+\s*::\s*)?%s\s*\(([^)]*)\)\s*\{'
                         % re.escape(short), text, re.M):
        head = text[m.start():m.end()]
        params = [p for p in m.group(1).split(',') if p.strip() and p.strip() != 'void']
        n = 0
        for p in params:
            if re.search(r'\bdouble\b', p) and '*' not in p:
                n += 2
            else:
                n += 1
        if re.search(r'__fastcall|BR_THISCALL1|BR_FASTCALL', head):
            n = max(0, n - 2)
        return n
    return None
