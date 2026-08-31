#!/usr/bin/env python3
"""Score a C++ translation unit against BRGlide.dll orig .text.

Mirrors ghidra_to_match._score_source, but the TU is compiled as C++ with
cl /GX (C++ EH on). The C pipeline cannot emit the 80-function
__CxxFrameHandler unwind class (97,204 B = 20.2% of BRGlide.dll .text).

The comparator is the same reloc-masked .text byte diff as match_sweep.score:
0 diffs on the orig-sized prefix is a .text match. Relocs (vtables, call
targets, handler thunks, fs:[0] if the assembler emitted a DIR32) are
masked; immediates that are NOT relocs (member offsets, array count/size,
trylevel 0/-1) are compared.

C++ names mangle (`??1BrCtl@@UAE@XZ`). parse_coff_obj (via match_diff) is
reused; this file then matches the wanted function by mangled / demangled /
base name because undecorate() leaves `?name@@...` intact.

Verification gap (read this before treating a 0-diff as a class-open):
    FuncInfo (magic 0x19930520), the unwind map, and the outlined unwind
    action live OUTSIDE the function's .text. match_sweep / this scorer's
    .text compare never sees them. A .text match does NOT prove FuncInfo
    matches. This script therefore ALSO parses FuncInfo from the orig DLL
    and from the .obj .rdata and prints a separate structural check. That
    extra check is what makes the C++ workstream verifiable; the C sweep
    cannot do it.

Usage:
    python3 tools/cpp_score.py --va 0x10040D10
    python3 tools/cpp_score.py --va 0x10040D10 --src build/cpp_work/0x10040D10.cpp
    python3 tools/cpp_score.py --va 0x10040D10 --name BrCtl --opt "/O2 /GX /MD"
    python3 tools/cpp_score.py --va 0x10040D10 --list
"""
from __future__ import print_function

import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import match_diff  # noqa: E402
import match_sweep  # noqa: E402
import pe as pelib  # noqa: E402

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
CPP_WORK = os.path.join(ROOT, 'build', 'cpp_work')
OBJ_DIR = os.path.join(ROOT, 'build', 'match', 'obj_cpp')
# Same three opt shapes the C sweep uses, plus /GX (C++ EH) and /MD (the
# binary is /MD; CRT EH helpers dllimport). /GX is the VC5 flag; /EHs is VC6+.
DEFAULT_OPTS = ['/O2 /GX /MD', '/Od /GX /MD', '/O2 /Oy- /GX /MD']

FUNCINFO_MAGIC = 0x19930520

# Compiler-generated EH helpers / vtables we never want to score as the
# function under test (unless the user names them explicitly).
_HELPER_RE = re.compile(
    r'(ehhandler|ehfuncinfo|unwind|unwindfunclet|unwindtable|'
    r'\?\?_7|\?\?_8|\?\?_R)',
    re.I)


# ---------------------------------------------------------------------------
# Compile
# ---------------------------------------------------------------------------

def _ensure_gx_md(opt):
    """Guarantee /GX (and /MD unless the caller opted out)."""
    parts = opt.split()
    upper = [p.upper() for p in parts]
    if '/GX' not in upper and '/EHS' not in upper and '/EHSC' not in upper:
        parts.append('/GX')
    if '/MD' not in upper and '/MT' not in upper and '/ML' not in upper:
        parts.append('/MD')
    return ' '.join(parts)


def compile_cpp(src_path, tag, opt):
    """Compile a .cpp TU with cl /GX. Same wine/cl path as compile_variant.

    Own obj dir (build/match/obj_cpp/) so a live C session's obj_* is
    untouched. Returns (obj_path, [error lines], full_cl_output).
    """
    opt = _ensure_gx_md(opt)
    os.makedirs(OBJ_DIR, exist_ok=True)
    base = os.path.splitext(os.path.basename(src_path))[0]
    safe_tag = re.sub(r'[^A-Za-z0-9_]+', '_', tag).strip('_') or 'cpp'
    obj = os.path.join(OBJ_DIR, '%s_%s.obj' % (base, safe_tag))
    if os.path.exists(obj):
        os.unlink(obj)
    rel_obj = os.path.relpath(obj, ROOT).replace('/', '\\')
    rel_src = os.path.relpath(src_path, ROOT)
    if not rel_src.lower().endswith('.cpp'):
        return None, ['source must be .cpp (cl uses the extension to pick C++)'], ''
    cmd = (['sh', match_sweep.WINE, match_sweep.CL, '/nologo']
           + opt.split()
           + ['/W3', '/I', 'include',
              '/I', 'tools/msvc5-compat', '/I', 'tools/msvc5/include',
              '/DBR_MATCHING_BUILD', '/c', rel_src, '/Fo' + rel_obj])
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           timeout=180)
        out = (p.stdout or '') + (p.stderr or '')
    except subprocess.TimeoutExpired:
        return None, ['cl.exe timed out'], ''
    if not os.path.exists(obj):
        err = [l.strip() for l in out.splitlines() if 'error' in l.lower()]
        return None, err[:8] or ['no obj, no diagnostic'], out
    return obj, [], out


# ---------------------------------------------------------------------------
# COFF: parse_coff_obj plus code sections not named exactly ".text"
# ---------------------------------------------------------------------------

def parse_coff_code(obj_path):
    """Like match_diff.parse_coff_obj, but keep every code-section function.

    VC5 C++ /GX puts some EH helpers in .text$x (and COMDATs still named
    .text). parse_coff_obj drops anything whose section name is not exactly
    `.text`, so a dtor can vanish and score not_in_obj. The reloc-masking
    rules are the same.
    """
    funcs = match_diff.parse_coff_obj(obj_path)
    extra = _parse_coff_code_unfiltered(obj_path)
    # parse_coff_obj keys are undecorate()'d; extra keys are raw. Prefer
    # parse_coff_obj for .text names, then fill gaps from extra.
    for name, pair in extra.items():
        clean = match_diff.undecorate(name)
        if clean not in funcs:
            funcs[clean] = pair
        if name not in funcs:
            funcs[name] = pair
    return funcs


def _coff_read_name(data, name_bytes, strtab_base):
    if name_bytes[:4] == b'\x00\x00\x00\x00':
        str_off = struct.unpack_from('<I', name_bytes, 4)[0]
        end = data.index(b'\x00', strtab_base + str_off)
        return data[strtab_base + str_off:end].decode('latin1')
    return name_bytes.rstrip(b'\x00').decode('latin1')


def _parse_coff_raw(obj_path):
    """Return (data, sections, symbols).

    sections: list of dicts {name, rawoff, rawsize, chars, relocs, relocs_off,
    nrelocs}. relocs is a set of section-relative byte offsets (DIR32/REL32).
    symbols: list of dicts {name, value, sec_num, stype, sclass}.
    """
    with open(obj_path, 'rb') as f:
        data = f.read()
    machine, nsec, ts, symtab_off, nsyms, opthdr_sz, chars = \
        struct.unpack_from('<HHIIIHH', data, 0)
    if machine != 0x14C:
        raise ValueError('not i386 COFF (machine 0x%04x)' % machine)
    strtab_base = symtab_off + nsyms * 18
    sections = []
    off = 20
    for _ in range(nsec):
        name_bytes = data[off:off + 8]
        vsize, vaddr, rawsize, rawoff = struct.unpack_from('<IIII', data, off + 8)
        relocs_off = struct.unpack_from('<I', data, off + 24)[0]
        nrelocs = struct.unpack_from('<H', data, off + 32)[0]
        sec_chars = struct.unpack_from('<I', data, off + 36)[0]
        relocs = set()
        reloc_entries = []
        for r in range(nrelocs):
            r_off = relocs_off + r * 10
            r_vaddr = struct.unpack_from('<I', data, r_off)[0]
            r_sym = struct.unpack_from('<I', data, r_off + 4)[0]
            r_type = struct.unpack_from('<H', data, r_off + 8)[0]
            reloc_entries.append((r_vaddr, r_sym, r_type))
            if r_type in (0x06, 0x14):  # DIR32, REL32
                for b in range(4):
                    relocs.add(r_vaddr + b)
        sections.append({
            'name': _coff_read_name(data, name_bytes, strtab_base),
            'rawoff': rawoff, 'rawsize': rawsize, 'chars': sec_chars,
            'relocs': relocs, 'reloc_entries': reloc_entries,
        })
        off += 40

    symbols = []
    i = 0
    sym_off = symtab_off
    while i < nsyms:
        entry = data[sym_off:sym_off + 18]
        name_bytes = entry[:8]
        value = struct.unpack_from('<I', entry, 8)[0]
        sec_num = struct.unpack_from('<h', entry, 12)[0]
        stype = struct.unpack_from('<H', entry, 14)[0]
        sclass = entry[16]
        naux = entry[17]
        sname = _coff_read_name(data, name_bytes, strtab_base)
        symbols.append({
            'name': sname, 'value': value, 'sec_num': sec_num,
            'stype': stype, 'sclass': sclass, 'index': i,
        })
        sym_off += 18 * (1 + naux)
        i += 1 + naux
    return data, sections, symbols


def _parse_coff_code_unfiltered(obj_path):
    data, sections, symbols = _parse_coff_raw(obj_path)
    funcs = {}
    IMAGE_SCN_CNT_CODE = 0x00000020
    for sym in symbols:
        if sym['sclass'] not in (2, 3) or sym['sec_num'] <= 0:
            continue
        if not (sym['stype'] & 0x20):
            continue
        sec = sections[sym['sec_num'] - 1]
        if not (sec['chars'] & IMAGE_SCN_CNT_CODE) and not sec['name'].startswith('.text'):
            continue
        value = sym['value']
        blob = data[sec['rawoff'] + value: sec['rawoff'] + sec['rawsize']]
        relocs = {r - value for r in sec['relocs']}
        funcs[sym['name']] = (blob, relocs)
    return funcs


def list_coff_symbols(obj_path):
    """Human-readable symbol table: name, section, value, nbytes, kind."""
    data, sections, symbols = _parse_coff_raw(obj_path)
    rows = []
    for sym in symbols:
        if sym['sec_num'] <= 0:
            rows.append((sym['name'], '', 0, 0, 'undef' if sym['sec_num'] == 0 else 'abs'))
            continue
        sec = sections[sym['sec_num'] - 1]
        nbytes = 0
        if sym['stype'] & 0x20:
            nbytes = sec['rawsize'] - sym['value']
        kind = classify_symbol(sym['name'])
        rows.append((sym['name'], sec['name'], sym['value'], nbytes, kind))
    return rows


# ---------------------------------------------------------------------------
# MSVC mangling (just enough to pick ctor/dtor/helpers)
# ---------------------------------------------------------------------------

def classify_symbol(name):
    if name.startswith('??1'):
        return 'dtor'
    if name.startswith('??0'):
        return 'ctor'
    if name.startswith('??_G'):
        return 'scalar_deleting'
    if name.startswith('??_E'):
        return 'vector_deleting'
    if name.startswith('??_7'):
        return 'vtable'
    if _HELPER_RE.search(name):
        return 'eh_helper'
    if name.startswith('?'):
        return 'method'
    return 'c'


def find_symbol(funcs, want, prefer=None):
    """Pick a parse_coff_obj key for `want`.

    `want` may be a mangled `?name@@...`, a C identifier, or a class name
    (then prefer='dtor'/'ctor' selects ??1/??0). Returns the key or None.
    """
    if not want and prefer:
        # Fall through to prefer-kind search.
        pass
    elif want in funcs:
        return want
    elif want:
        clean = match_diff.undecorate(want)
        if clean in funcs:
            return clean
        # Substring / class-name match on mangled keys.
        hits = []
        for k in funcs:
            if want == k or want in k:
                hits.append(k)
                continue
            if k.startswith('?') and ('@@%s@@' % want in '@@%s@@' % k
                                      or k.startswith('??0' + want + '@@')
                                      or k.startswith('??1' + want + '@@')
                                      or k.startswith('??_G' + want + '@@')
                                      or k.startswith('?' + want + '@@')):
                hits.append(k)
        if len(hits) == 1:
            return hits[0]
        if hits and prefer:
            kind_hits = [h for h in hits if classify_symbol(h) == prefer]
            if len(kind_hits) == 1:
                return kind_hits[0]
            if kind_hits:
                hits = kind_hits
        if hits:
            # Prefer the named kind, then the longest body (the real function
            # vs a 10-byte handler thunk sitting in the same COMDAT).
            def rank(k):
                kind = classify_symbol(k)
                kind_rank = 0 if (prefer and kind == prefer) else 1
                if kind == 'eh_helper':
                    kind_rank = 9
                return (kind_rank, -len(funcs[k][0]), k)
            hits.sort(key=rank)
            return hits[0]

    if prefer:
        kind_hits = [k for k in funcs if classify_symbol(k) == prefer]
        if len(kind_hits) == 1:
            return kind_hits[0]
        if kind_hits:
            kind_hits.sort(key=lambda k: -len(funcs[k][0]))
            return kind_hits[0]

    # Last resort: the largest non-helper .text symbol.
    bodies = [(k, len(funcs[k][0])) for k in funcs
              if classify_symbol(k) not in ('eh_helper', 'vtable')]
    if len(bodies) == 1:
        return bodies[0][0]
    return None


def parse_implements_name(src_path, va):
    """Return (name, cpp_symbol, prefer_kind) from comments in the .cpp."""
    if not src_path or not os.path.exists(src_path):
        return None, None, None
    name = None
    cpp_symbol = None
    prefer = None
    impl = re.compile(r'@implements\s+0x([0-9A-Fa-f]+)\s+\w+\s+(\S+)')
    sym = re.compile(r'@cpp_symbol\s+(\S+)')
    kind = re.compile(r'@cpp_kind\s+(dtor|ctor|method|scalar_deleting)')
    with open(src_path) as f:
        for line in f:
            m = impl.search(line)
            if m and int(m.group(1), 16) == va:
                name = m.group(2)
            m = sym.search(line)
            if m:
                cpp_symbol = m.group(1)
            m = kind.search(line)
            if m:
                prefer = m.group(1)
    return name, cpp_symbol, prefer


# ---------------------------------------------------------------------------
# FuncInfo (lives in .rdata, NOT in the .text the sweep compares)
# ---------------------------------------------------------------------------

def _parse_funcinfo_blob(buf, base_va=0):
    """Parse a 0x19930520 FuncInfo. Pointers are stored as-is (VA or obj offset)."""
    if buf is None or len(buf) < 28:
        return None
    magic, max_state, p_unwind, n_try, p_try, n_ip, p_ip = \
        struct.unpack_from('<IIIIIII', buf, 0)
    if magic != FUNCINFO_MAGIC:
        return None
    return {
        'magic': magic,
        'maxState': max_state,
        'pUnwindMap': p_unwind,
        'nTryBlocks': n_try,
        'pTryBlockMap': p_try,
        'nIPMapEntries': n_ip,
        'pIPtoStateMap': p_ip,
        'base': base_va,
    }


def orig_handler_va(code):
    """Handler thunk VA from a C++ EH prologue (`push -1; push handler`)."""
    n = min(len(code) - 6, 24)
    for i in range(n):
        if code[i] == 0x6A and code[i + 1] == 0xFF and code[i + 2] == 0x68:
            return struct.unpack_from('<I', code, i + 3)[0]
    return None


def orig_funcinfo(pe, func_va, code):
    """Walk orig .text → handler thunk (`mov eax, FuncInfo; jmp`) → FuncInfo."""
    handler = orig_handler_va(code)
    if handler is None:
        return None, None, None
    thunk = pe.read(handler, 10)
    if not thunk or thunk[0] != 0xB8:
        return handler, None, None
    fi_va = struct.unpack_from('<I', thunk, 1)[0]
    blob = pe.read(fi_va, 28)
    info = _parse_funcinfo_blob(blob, fi_va)
    if info is None:
        return handler, fi_va, None
    unwinds = []
    for i in range(info['maxState']):
        raw = pe.read(info['pUnwindMap'] + i * 8, 8)
        if not raw or len(raw) < 8:
            break
        to_state, action = struct.unpack('<iI', raw)
        ab = pe.read(action, 64) or b''
        unwinds.append({'toState': to_state, 'action': action, 'bytes': ab})
    info['unwinds'] = unwinds
    info['handler'] = handler
    info['va'] = fi_va
    return handler, fi_va, info


def _sym_by_index(symbols):
    """COFF relocs name a symbol-table index, not a compacted-list index.

    Aux records occupy table slots, so `symbols[r_sym]` is the wrong symbol.
    Keys here are the table index stored on each parsed record.
    """
    return {sy['index']: sy for sy in symbols}


def _section_code_blobs(data, sections, symbols):
    """Bytes + function-relative relocs for every label/function in a code sec.

    Unwind actions (`$Lnnn`) are IMAGE_SYM_CLASS_LABEL (6), not functions, so
    parse_coff_obj never returns them. Clip each blob at the next label in
    the same section (handler thunk sits at +0x1B after a 27-byte action).
    """
    IMAGE_SCN_CNT_CODE = 0x00000020
    by_sec = {}
    for sy in symbols:
        if sy['sec_num'] <= 0:
            continue
        if sy['sclass'] not in (2, 3, 6):
            continue
        if sy['name'].startswith('.'):
            continue
        by_sec.setdefault(sy['sec_num'], []).append(sy)
    blobs = {}
    for sec_num, syms in by_sec.items():
        sec = sections[sec_num - 1]
        if not (sec['chars'] & IMAGE_SCN_CNT_CODE) and not sec['name'].startswith('.text'):
            continue
        ordered = sorted(syms, key=lambda s: s['value'])
        raw = data[sec['rawoff']:sec['rawoff'] + sec['rawsize']]
        for i, sy in enumerate(ordered):
            start = sy['value']
            end = ordered[i + 1]['value'] if i + 1 < len(ordered) else len(raw)
            if end <= start:
                continue
            relocs = {r - start for r in sec['relocs'] if start <= r < end}
            blobs[sy['name']] = (raw[start:end], relocs)
    return blobs


def obj_funcinfos(obj_path):
    """Every 0x19930520 record in the .obj, with unwind maps resolved via relocs."""
    data, sections, symbols = _parse_coff_raw(obj_path)
    idx = _sym_by_index(symbols)
    blobs = _section_code_blobs(data, sections, symbols)
    out = []
    for sec in sections:
        raw = data[sec['rawoff']:sec['rawoff'] + sec['rawsize']]
        reloc_at = {r_vaddr: (r_sym, r_type)
                    for r_vaddr, r_sym, r_type in sec['reloc_entries']}
        off = 0
        while off + 28 <= len(raw):
            magic = struct.unpack_from('<I', raw, off)[0]
            if magic != FUNCINFO_MAGIC:
                off += 4
                continue
            info = _parse_funcinfo_blob(raw[off:off + 28], off)
            if info is None:
                off += 4
                continue
            unwinds = []
            # pUnwindMap at +8 is a DIR32 reloc to the unwind table.
            if (off + 8) in reloc_at:
                r_sym, _ = reloc_at[off + 8]
                sy = idx.get(r_sym)
                if sy and sy['sec_num'] > 0:
                    uw_sec = sections[sy['sec_num'] - 1]
                    uw_off = sy['value']
                    uw_raw = data[uw_sec['rawoff']:uw_sec['rawoff'] + uw_sec['rawsize']]
                    uw_rel = {rv: (rs, rt) for rv, rs, rt in uw_sec['reloc_entries']}
                    for i in range(info['maxState']):
                        eoff = uw_off + i * 8
                        if eoff + 8 > len(uw_raw):
                            break
                        to_state = struct.unpack_from('<i', uw_raw, eoff)[0]
                        action_bytes = b''
                        action_relocs = set()
                        action_name = None
                        act_key = eoff + 4
                        if act_key in uw_rel:
                            a_sym, _ = uw_rel[act_key]
                            asy = idx.get(a_sym)
                            if asy:
                                action_name = asy['name']
                                if action_name in blobs:
                                    action_bytes, action_relocs = blobs[action_name]
                                elif asy['sec_num'] > 0:
                                    asec = sections[asy['sec_num'] - 1]
                                    action_bytes = data[asec['rawoff'] + asy['value']:
                                                        asec['rawoff'] + asec['rawsize']]
                                    action_relocs = {r - asy['value'] for r in asec['relocs']}
                        unwinds.append({
                            'toState': to_state,
                            'action_name': action_name,
                            'bytes': action_bytes,
                            'relocs': action_relocs,
                        })
            info['unwinds'] = unwinds
            info['section'] = sec['name']
            info['offset'] = off
            # Handler thunk is the `mov eax, FuncInfo; jmp __CxxFrameHandler`
            # that lives next to the unwind action in .text$x. Find any code
            # blob that DIR32-relocs to this FuncInfo record's section offset.
            info['handler_name'] = None
            info['handler_bytes'] = b''
            info['handler_relocs'] = set()
            for bname, (bb, br) in blobs.items():
                if len(bb) == 10 and bb[0] == 0xB8:
                    info['handler_name'] = bname
                    info['handler_bytes'] = bb
                    info['handler_relocs'] = br
                    break
            out.append(info)
            off += 28
    return out


def load_pe():
    cand = os.environ.get('BR_REF', os.path.join(ROOT, 'orig', 'BRGlide.dll'))
    if not os.path.isabs(cand):
        cand = os.path.join(ROOT, cand)
    return pelib.load(cand)


def funcinfo_struct_equal(orig, recomp):
    """Compare FuncInfo fields that are source-shaped, not linked addresses."""
    if orig is None and recomp is None:
        return True, 'no EH'
    if orig is None or recomp is None:
        return False, 'missing FuncInfo on one side'
    reasons = []
    for k in ('magic', 'maxState', 'nTryBlocks', 'nIPMapEntries'):
        if orig.get(k) != recomp.get(k):
            reasons.append('%s orig=%s recomp=%s' % (k, orig.get(k), recomp.get(k)))
    ou = orig.get('unwinds') or []
    ru = recomp.get('unwinds') or []
    if len(ou) != len(ru):
        reasons.append('unwind-count orig=%d recomp=%d' % (len(ou), len(ru)))
    else:
        for i, (a, b) in enumerate(zip(ou, ru)):
            if a.get('toState') != b.get('toState'):
                reasons.append('unwind[%d].toState orig=%s recomp=%s'
                               % (i, a.get('toState'), b.get('toState')))
    return (not reasons), ('; '.join(reasons) if reasons else 'ok')


# ---------------------------------------------------------------------------
# Scoring (same contract as ghidra_to_match._score_source)
# ---------------------------------------------------------------------------

def score_source(src_text, func_name, orig_bytes, opts, tag):
    """Compile src_text as C++ /GX; return (diffs, opt, recomp_bytes, relocs).

    Same tuple as ghidra_to_match._score_source. Writes a temp .cpp (never
    .c — cl would compile C and silently drop the EH frame). `func_name` is
    a C identifier, a mangled `?name@@...`, or a class name.
    """
    tmpdir = tempfile.mkdtemp(prefix='cpp_score_')
    src_path = os.path.join(tmpdir, 'r.cpp')
    best = (None, '', None, None)
    try:
        with open(src_path, 'w') as f:
            f.write(src_text)
        for opt in opts:
            obj, errs, _out = compile_cpp(src_path, tag, opt)
            if obj is None:
                continue
            try:
                funcs = parse_coff_code(obj)
            except Exception:
                continue
            found = find_symbol(funcs, func_name, prefer='dtor')
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


def load_orig_bytes(va):
    path = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
    if not os.path.exists(path):
        sys.exit('no orig bin %s (not a Glide-keyed VA?)' % path)
    return match_sweep.load_orig(path, va)


def first_diff(orig, recomp, relocs):
    n = min(len(orig), len(recomp))
    for i in range(n):
        if i in relocs:
            continue
        if orig[i] != recomp[i]:
            return i
    if len(recomp) < len(orig):
        return n
    return None


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def default_src(va):
    return os.path.join(CPP_WORK, '0x%08X.cpp' % va)


def score_va(va, src_path, name, opts, list_syms=False, prefer=None):
    orig = load_orig_bytes(va)
    if not os.path.exists(src_path):
        sys.exit('no source %s' % src_path)

    impl_name, cpp_symbol, impl_kind = parse_implements_name(src_path, va)
    if not name:
        name = cpp_symbol or impl_name
    if not prefer:
        prefer = impl_kind or 'dtor'

    print('== cpp_score 0x%08X ==' % va)
    print('src:     %s' % os.path.relpath(src_path, ROOT))
    print('orig:    %d bytes  %s' % (len(orig), orig[:16].hex()))
    print('want:    %s  (prefer %s)' % (name or '(auto)', prefer))

    best = None  # (ndiff, opt, obj, found, rb, relocs, funcs)
    last_out = ''
    last_err = []
    for i, opt in enumerate(opts):
        tag = 'va%08X_%d' % (va, i)
        obj, errs, out = compile_cpp(src_path, tag, opt)
        last_out = out
        if obj is None:
            last_err = errs
            print('  COMPILE FAIL  %s  %s' % (opt, '; '.join(errs)))
            if out.strip():
                for line in out.splitlines()[-20:]:
                    print('    %s' % line)
            continue
        try:
            funcs = parse_coff_code(obj)
        except Exception as e:
            print('  COFF FAIL     %s  %s' % (opt, e))
            continue
        if list_syms:
            print('  -- symbols (%s) --' % opt)
            for sname, sec, val, nbytes, kind in list_coff_symbols(obj):
                print('    %-12s %-8s +0x%04X %4d  %s'
                      % (kind, sec, val, nbytes, sname))
        found = find_symbol(funcs, name, prefer=prefer)
        if found is None:
            print('  NOT IN OBJ    %s  %d symbols: %s'
                  % (opt, len(funcs), ', '.join(sorted(funcs)[:12])))
            continue
        rb, relocs = funcs[found]
        ok, nd, rlen = match_sweep.score(orig, rb, relocs)
        nreloc = sum(1 for j in range(len(orig)) if j in relocs)
        print('  %s  %s  symbol=%s  orig=%d recomp=%d diffs=%s reloc=%d'
              % ('MATCH' if ok else 'DIFF ', opt, found,
                 len(orig), rlen, nd, nreloc))
        rec = (0 if ok else 1, nd, abs(rlen - len(orig)),
               opt, obj, found, rb, relocs, funcs)
        if best is None or rec[:3] < best[:3]:
            best = rec
        if ok:
            break

    if best is None:
        print('result: compile_error / not_in_obj')
        if last_err:
            print('cl: %s' % '; '.join(last_err))
        if last_out.strip():
            print(last_out[-2000:])
        return 2

    _okrank, nd, _ds, opt, obj, found, rb, relocs, funcs = best
    pos = first_diff(orig, rb, relocs)
    if nd != 0:
        print()
        print('best: %s  %d diffs  first @ +0x%X' % (opt, nd, pos or 0))
        print(match_diff.hex_diff(orig, rb[:len(orig)], relocs, max_lines=16))
    else:
        print()
        print('best: %s  0 diffs on .text (%d bytes)' % (opt, len(orig)))

    # --- FuncInfo sidecar (NOT covered by the .text score) -----------------
    print()
    print('-- FuncInfo (.xdata$x / .rdata, outside the function .text) --')
    pe = load_pe()
    handler, fi_va, oi = orig_funcinfo(pe, va, orig)
    print('orig handler thunk: %s' % ('0x%08X' % handler if handler else 'not found'))
    print('orig FuncInfo:      %s' % ('0x%08X' % fi_va if fi_va else 'not found'))
    if oi:
        print('  magic=0x%08X maxState=%d nTryBlocks=%d nIPMapEntries=%d'
              % (oi['magic'], oi['maxState'], oi['nTryBlocks'], oi['nIPMapEntries']))
        for i, u in enumerate(oi.get('unwinds') or []):
            print('  unwind[%d] toState=%s action=0x%08X'
                  % (i, u['toState'], u['action']))

    fis = obj_funcinfos(obj)
    print('recomp FuncInfo records in .obj: %d' % len(fis))
    ri = fis[0] if len(fis) == 1 else None
    if ri is None and oi and fis:
        for cand in fis:
            if cand.get('maxState') == oi.get('maxState'):
                ri = cand
                break
        if ri is None:
            ri = fis[0]
    if ri:
        print('  magic=0x%08X maxState=%d nTryBlocks=%d nIPMapEntries=%d'
              % (ri['magic'], ri['maxState'], ri['nTryBlocks'], ri['nIPMapEntries']))
        for i, u in enumerate(ri.get('unwinds') or []):
            print('  unwind[%d] toState=%s action=%s (%d bytes)'
                  % (i, u.get('toState'), u.get('action_name'),
                     len(u.get('bytes') or b'')))

    equal, why = funcinfo_struct_equal(oi, ri)
    print('FuncInfo structural: %s (%s)' % ('MATCH' if equal else 'DIFF', why))

    sidecar_ok = equal
    if oi and ri:
        for i, (ou, ru) in enumerate(zip(oi.get('unwinds') or [],
                                         ri.get('unwinds') or [])):
            act_va = ou['action']
            act_path = os.path.join(ORIG_DIR, '0x%08X.bin' % act_va)
            if not os.path.exists(act_path):
                print('unwind[%d] action orig bin missing (0x%08X) — not scored'
                      % (i, act_va))
                sidecar_ok = False
                continue
            act_orig = match_sweep.load_orig(act_path, act_va)
            act_rb = ru.get('bytes') or b''
            act_relocs = ru.get('relocs') or set()
            ok_a, nd_a, rlen_a = match_sweep.score(act_orig, act_rb, act_relocs)
            print('unwind[%d] action .text vs 0x%08X (%d B): %s diffs=%s recomp=%d'
                  % (i, act_va, len(act_orig),
                     'MATCH' if ok_a else 'DIFF', nd_a, rlen_a))
            sidecar_ok = sidecar_ok and ok_a
            if not ok_a:
                print(match_diff.hex_diff(act_orig, act_rb[:len(act_orig)],
                                          act_relocs, max_lines=8))

        # Handler thunk is its own map entry (10 B: mov eax, FuncInfo; jmp).
        if handler:
            hpath = os.path.join(ORIG_DIR, '0x%08X.bin' % handler)
            hb = ri.get('handler_bytes') or b''
            hr = ri.get('handler_relocs') or set()
            if os.path.exists(hpath) and hb:
                horig = match_sweep.load_orig(hpath, handler)
                ok_h, nd_h, rlen_h = match_sweep.score(horig, hb, hr)
                print('handler thunk .text vs 0x%08X (%d B): %s diffs=%s recomp=%d'
                      % (handler, len(horig),
                         'MATCH' if ok_h else 'DIFF', nd_h, rlen_h))
                sidecar_ok = sidecar_ok and ok_h
            elif os.path.exists(hpath):
                print('handler thunk orig bin 0x%08X present, no .obj thunk found'
                      % handler)
                sidecar_ok = False

    print()
    print('verification gap:')
    print('  The C sweep / match_sweep.score compares ONLY the function .text.')
    print('  FuncInfo (magic 0x19930520), the unwind map, and the outlined')
    print('  unwind action / handler thunk are separate (.xdata$x / .text$x,')
    print('  own map VAs). A .text match of this function does NOT imply')
    print('  those match — they have to be checked separately.')
    if nd == 0 and equal and sidecar_ok:
        print('  This run: function .text MATCH, FuncInfo structural MATCH,')
        print('  unwind-action .text MATCH, handler thunk MATCH — all four')
        print('  independently. cpp_score.py is what makes the class verifiable;')
        print('  match_sweep as it stands is not.')
    elif nd == 0:
        print('  This run: function .text MATCH, sidecar %s.'
              % ('MATCH' if sidecar_ok else 'DIFF or incomplete'))
    else:
        print('  This run: function .text DIFF; sidecar check still ran.')
    return 0 if nd == 0 else 1


def main():
    ap = argparse.ArgumentParser(
        description='Compile a .cpp /GX and reloc-mask-score it against orig .text.')
    ap.add_argument('--va', required=True, help='Glide VA, e.g. 0x10040D10')
    ap.add_argument('--src', help='.cpp path (default build/cpp_work/<VA>.cpp)')
    ap.add_argument('--name', help='mangled symbol, C name, or class name')
    ap.add_argument('--opt', action='append', dest='opts',
                    help='cl flags (repeatable). /GX /MD are added if missing.')
    ap.add_argument('--list', action='store_true', help='dump .obj symbols')
    ap.add_argument('--kind', choices=('dtor', 'ctor', 'method', 'scalar_deleting'),
                    help='which mangled kind to prefer when --name is a class')
    args = ap.parse_args()
    va = int(args.va, 16)
    src = os.path.abspath(args.src) if args.src else default_src(va)
    opts = args.opts or list(DEFAULT_OPTS)
    sys.exit(score_va(va, src, args.name, opts, list_syms=args.list,
                      prefer=args.kind))


if __name__ == '__main__':
    main()
