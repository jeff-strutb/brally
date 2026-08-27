#!/usr/bin/env python3
"""Infer callee calling conventions from original bytes and rewrite Ghidra C.

Ghidra types every callee as cdecl `int f()`. The originals call Win32,
Glide, COM, and DInput with other conventions; the wrong one emits a
spurious `add esp,N`, promotes float args to double, or drops ecx=this.

This is a STANDALONE refine transform. It does not write ghidra_work,
does not edit ghidra_to_match.py, and does not commit. Decision logic
is in docs/gen-callconv-notes.md — fold that into _refine_candidates.

    python3 tools/gen_callconv.py --va 0x1006C6A0
    python3 tools/gen_callconv.py --va 0x1006C6A0 --from-decomp
    python3 tools/gen_callconv.py --validate
"""
from __future__ import print_function

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
# capstone lives in the repo venv (never installed to the host)
_VENV_SP = os.path.join(ROOT, '.venv', 'lib')
if os.path.isdir(_VENV_SP):
    import glob
    for _sp in glob.glob(os.path.join(_VENV_SP, 'python*', 'site-packages')):
        if _sp not in sys.path:
            sys.path.insert(0, _sp)

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
WORK_DIR = os.path.join(ROOT, 'build', 'ghidra_work')
GHIDRA_DIR = os.path.join(ROOT, 'build', 'ghidra_decomp')
DLL_PATH = os.path.join(ROOT, 'orig', 'BRGlide.dll')

# Glide entry points whose non-int parameters the @N decoration cannot
# recover. Everything else is int x (N/4). Proven by 0x1001DFB0 MATCH
# and VC5-IDIOMS (guFogGenerateLinear float near/far).
GLIDE_SIGS = {
    'grClipWindow': ('void', ['int', 'int', 'int', 'int']),
    'grDepthBufferMode': ('void', ['int']),
    'grDepthBufferFunction': ('void', ['int']),
    'grDepthMask': ('void', ['int']),
    'grBufferClear': ('void', ['int', 'int', 'int']),
    'grCullMode': ('void', ['int']),
    'grAlphaCombine': ('void', ['int', 'int', 'int', 'int', 'int']),
    'grAlphaBlendFunction': ('void', ['int', 'int', 'int', 'int']),
    'grAlphaTestFunction': ('void', ['int']),
    'grAlphaTestReferenceValue': ('void', ['int']),
    'grTexFilterMode': ('void', ['int', 'int', 'int']),
    'grConstantColorValue': ('void', ['int']),
    'grColorCombine': ('void', ['int', 'int', 'int', 'int', 'int']),
    'grTexCombine': ('void', ['int', 'int', 'int', 'int', 'int', 'int', 'int']),
    'grFogTable': ('void', ['float *']),
    'grFogMode': ('void', ['int']),
    'grFogColorValue': ('void', ['int']),
    'guFogGenerateLinear': ('void', ['float *', 'float', 'float']),
    'grTexLodBiasValue': ('void', ['int', 'float']),
    'grDrawTriangle': ('void', ['void *', 'void *', 'void *']),
    'grDrawPolygonVertexList': ('void', ['int', 'void *']),
    'grSstWinOpen': ('int', ['int', 'int', 'int', 'int', 'int', 'int', 'int']),
    'grSstWinClose': ('void', []),
    'grSstSelect': ('void', ['int']),
    'grSstQueryHardware': ('int', ['void *']),
    'grGlideInit': ('void', []),
    'grGlideShutdown': ('void', []),
    'grBufferSwap': ('void', ['int']),
    'grBufferNumPending': ('int', []),
    'grTexMinAddress': ('int', ['int']),
    'grTexMaxAddress': ('int', ['int']),
    'grTexSource': ('void', ['int', 'int', 'int', 'void *']),
    'grTexClampMode': ('void', ['int', 'int', 'int']),
    'grTexMipMapMode': ('void', ['int', 'int', 'int']),
    'grTexDownloadMipMap': ('void', ['int', 'int', 'int', 'void *']),
    'grTexCalcMemRequired': ('int', ['int', 'int', 'int', 'int']),
    'grTexTextureMemRequired': ('int', ['int', 'void *']),
    'grLfbWriteRegion': ('void', ['int'] * 8),
}

C_KEYWORDS = frozenset([
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'case', 'do',
    'void', 'int', 'char', 'short', 'float', 'double', 'long', 'unsigned',
    'struct', 'typedef', 'static', 'const', 'volatile', 'goto', 'break',
    'continue', 'else', 'true', 'false',
])
# Already stdcall via windows.h / mmsystem.h — do not redeclare.
WIN32_API = frozenset([
    'CloseHandle', 'CreateEventA', 'CreateThread', 'WaitForMultipleObjects',
    'WaitForSingleObject', 'ReleaseMutex', 'ExitThread', 'Sleep',
    'GlobalAlloc', 'GlobalFree', 'GlobalLock', 'GlobalUnlock', 'GlobalHandle',
    'GlobalReAlloc', 'GlobalSize', 'GlobalFlags', 'GlobalMemoryStatus',
    'LocalAlloc', 'LocalFree', 'LocalLock', 'LocalUnlock',
    'GetProcAddress', 'LoadLibraryA', 'FreeLibrary', 'GetModuleHandleA',
    'SetTimer', 'KillTimer', 'wsprintfA', 'OutputDebugStringA',
    'mciSendCommandA', 'mmioOpenA', 'mmioDescend', 'mmioRead', 'mmioAscend',
    'mmioClose', 'mmioSeek', 'mmioGetInfo', 'mmioSetInfo', 'mmioAdvance',
    'timeGetTime', 'timeBeginPeriod', 'timeEndPeriod',
    'sprintf', 'strcpy', 'strlen', 'memcpy', 'memset', 'memcmp',
    'malloc', 'free', 'exit', 'srand', 'rand', 'qsort',
])

_SAVED_REGS = frozenset(['ebx', 'ebp', 'esi', 'edi'])
_GP_REGS = frozenset(['eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'])
# Object pointers in this binary live in the image; stosd counts do not.
_THIS_IMM_MIN = 0x10000000
_PTR_PREFIX_RE = re.compile(r'(?:dword|word|byte|qword)\s*ptr', re.I)


def _norm_op(s):
    """Lowercase, drop spaces and `dword ptr` so `[eax]` matches capstone."""
    s = _PTR_PREFIX_RE.sub('', s or '')
    return s.lower().replace(' ', '')


def _cs():
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    md.skipdata = True
    return md


def _pe():
    import pe as pelib
    return pelib.load(DLL_PATH)


def _text_and_va(pe):
    return pe.text()


def parse_imp_name(full):
    """'glide2x.dll!_grClipWindow@16' -> ('grClipWindow', 4, 'glide')."""
    if '!' in full:
        dll, rest = full.split('!', 1)
    else:
        dll, rest = '', full
    arity = None
    m = re.search(r'@(\d+)$', rest)
    if m:
        arity = int(m.group(1)) // 4
        rest = rest[:m.start()]
    name = rest.lstrip('_')
    kind = 'glide' if 'glide' in dll.lower() else 'win32'
    return name, arity, kind


def callee_ret_imm(va):
    """Trailing `ret imm16` of a local function, or None."""
    path = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
    if not os.path.exists(path):
        return None
    b = open(path, 'rb').read()
    if len(b) >= 3 and b[-3] == 0xC2:
        imm = b[-2] | (b[-1] << 8)
        if imm % 4 == 0 and imm <= 0x40:
            return imm
    return None


def _is_add_esp(ins):
    s = ins.op_str.lower()
    return ins.mnemonic == 'add' and s.startswith('esp')


def _add_esp_imm(ins):
    """Immediate of `add esp, N`, or None."""
    m = re.search(r'esp,\s*(0x[0-9a-fA-F]+|\d+)$', ins.op_str, re.I)
    if not m:
        return None
    return int(m.group(1), 0)


def _parse_imm(s):
    s = (s or '').strip().lower().replace(' ', '')
    if re.match(r'^-?(0x[0-9a-f]+|\d+)$', s):
        return int(s, 0)
    return None


def _is_small_imm(src):
    v = _parse_imm(src)
    return v is not None and v < _THIS_IMM_MIN


def _is_this_imm(src):
    v = _parse_imm(src)
    return v is not None and v >= _THIS_IMM_MIN


def _cname(c):
    """Call-info name that is always a str (never None)."""
    return c.get('name') or ''


def _following_add_esp(insns, idx, window=6):
    """add-esp imm if cleanup appears before the next call/push/ret.

    CRT `sprintf` is `FF 15; mov ecx, X; add esp, 8` — the add is delayed
    one flag-setting/mov, not absent. Stop at push/call/ret so a later
    cdecl's cleanup is not attributed to this site.
    """
    for j in range(idx + 1, min(len(insns), idx + 1 + window)):
        ins = insns[j]
        if ins.mnemonic in ('call', 'ret', 'retn', 'push'):
            return None
        if ins.mnemonic == 'pop' and 'esp' in ins.op_str.lower():
            return None
        if ins.mnemonic == 'sub' and ins.op_str.lower().startswith('esp'):
            return None
        if _is_add_esp(ins):
            n = _add_esp_imm(ins)
            # Frame teardown is `add esp, 0x110` / `0x8000`, not cdecl
            # cleanup. Real arg pops are ≤ 16 dwords (0x40).
            if n is not None and n <= 0x40 and n % 4 == 0:
                return n
            return None
    return None


def _recover_push(op_str, reg_imm):
    s = op_str.strip().lower()
    if re.match(r'^-?(0x[0-9a-f]+|\d+)$', s):
        return s
    if s in reg_imm:
        return reg_imm[s]
    return None


def _note_reg(ins, reg_imm):
    """Cheap constant tracker: xor-zero and mov-imm."""
    op = ins.op_str.lower().replace(' ', '')
    if ins.mnemonic == 'xor' and ',' in op:
        a, b = op.split(',', 1)
        if a == b and a in _GP_REGS:
            reg_imm[a] = '0'
            return
    if ins.mnemonic == 'mov' and ',' in op:
        dst, src = op.split(',', 1)
        if dst in _GP_REGS:
            if re.match(r'^-?(0x[0-9a-f]+|\d+)$', src):
                reg_imm[dst] = src
            else:
                reg_imm.pop(dst, None)
            return
    if ins.mnemonic in ('lea', 'add', 'sub', 'inc', 'dec', 'and', 'or',
                        'not', 'neg', 'shl', 'shr', 'sar', 'imul'):
        dst = ins.op_str.split(',')[0].strip().lower()
        if dst in _GP_REGS:
            reg_imm.pop(dst, None)


def analyze_orig(va, pe=None, gmap=None):
    """Return a list of CallInfo dicts for every call in the original body."""
    if pe is None:
        pe = _pe()
    if gmap is None:
        try:
            import ghidra_to_match as g
            gmap = g.load_globals()
        except Exception:
            gmap = {}
    text, text_va = _text_and_va(pe)
    orig_path = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
    orig = open(orig_path, 'rb').read()
    md = _cs()
    insns = list(md.disasm(orig, va))
    calls = []
    pending = 0  # dwords pushed since last baseline
    arg_stack = []  # parallel recovered C literals (or None)
    reg_imm = {}
    i = 0
    skip_saved = True  # prologue / post-ret shrink-wrap
    seen_arg_push = False  # saved-reg push AFTER an arg push is itself an arg
    while i < len(insns):
        ins = insns[i]
        nxt = insns[i + 1] if i + 1 < len(insns) else None
        _note_reg(ins, reg_imm)
        if skip_saved:
            if ins.mnemonic == 'sub' and ins.op_str.lower().startswith('esp'):
                i += 1
                continue
            if ins.mnemonic == 'push' and ins.op_str.lower() in _SAVED_REGS:
                i += 1
                continue
            skip_saved = False
        # Frame `sub esp, N` may follow a hoisted load (0x100704E0).
        if (not calls and ins.mnemonic == 'sub'
                and ins.op_str.lower().startswith('esp')):
            i += 1
            continue
        # Saved-reg pushes before the first *argument* push are prologue,
        # even after a hoisted load (0x100703D0 `mov eax,[g]; push edi`)
        # and even after an earlier basic-block's calls (0x10002F70
        # shrink-wrap `push esi` following a `ret` in the other arm).
        # Exception: if the register currently holds a tracked immediate
        # (`xor esi,esi` then `push esi`), it is an argument — 0x10023B70
        # pushes a zeroed esi as grAlphaCombine's last stack arg before
        # any `push imm`.
        if (not seen_arg_push and ins.mnemonic == 'push'
                and ins.op_str.lower() in _SAVED_REGS
                and ins.op_str.lower() not in reg_imm):
            i += 1
            continue
        if ins.mnemonic == 'push':
            seen_arg_push = True
            pending += 1
            arg_stack.append(_recover_push(ins.op_str, reg_imm))
        elif ins.mnemonic == 'pop':
            pending = max(0, pending - 1)
            if arg_stack:
                arg_stack.pop()
        elif _is_add_esp(ins):
            n = _add_esp_imm(ins)
            if n is not None:
                k = n // 4
                pending = max(0, pending - k)
                arg_stack = arg_stack[:-k] if k and k <= len(arg_stack) else (
                    [] if k else arg_stack)
        elif ins.mnemonic == 'sub' and ins.op_str.lower().startswith('esp'):
            m = re.search(r'esp,\s*(0x[0-9a-fA-F]+|\d+)$', ins.op_str, re.I)
            if m:
                k = int(m.group(1), 0) // 4
                pending += k
                arg_stack.extend([None] * k)
        elif ins.mnemonic == 'call':
            info = classify_call(
                ins, insns, i, pending, nxt, pe, text, text_va, gmap,
                arg_stack)
            calls.append(info)
            if info['conv'] in ('stdcall', 'stdcall-com'):
                pending = 0
                arg_stack = []
            elif info['conv'] == 'thiscall':
                # this is in ecx; stack args if any are cdecl-cleaned later
                # (see 0x1006B0E0) or callee-cleaned. Do not assume.
                pass
            # cdecl: pending stays until the following add esp
        elif ins.mnemonic in ('ret', 'retn'):
            pending = 0
            arg_stack = []
            skip_saved = True
            seen_arg_push = False
        i += 1
    return calls


def classify_call(ins, insns, idx, pending, nxt, pe, text, text_va, gmap=None,
                  arg_stack=None):
    """Byte pattern → {kind, conv, arity, name, disp, c_args, notes}."""
    gmap = gmap or {}
    arg_stack = arg_stack or []
    raw = bytes(ins.bytes)
    delayed = _following_add_esp(insns, idx)
    after_is_cdecl = delayed is not None
    cdecl_n = delayed // 4 if after_is_cdecl else None

    def slice_args(n):
        if not n:
            return []
        chunk = arg_stack[-n:] if n <= len(arg_stack) else list(arg_stack)
        return list(reversed(chunk))

    info = {
        'addr': ins.address,
        'text': '%s %s' % (ins.mnemonic, ins.op_str),
        'raw': raw.hex(),
        'kind': 'unknown',
        'conv': 'cdecl' if after_is_cdecl else None,
        'arity': cdecl_n if after_is_cdecl else pending,
        'name': None,
        'disp': None,
        'thiscall': False,
        'c_args': slice_args(cdecl_n if after_is_cdecl else pending),
        'notes': [],
    }

    # FF 15 disp32  — call [imm32]  (IAT or function-pointer global)
    if len(raw) >= 6 and raw[0] == 0xFF and raw[1] == 0x15:
        target = int.from_bytes(raw[2:6], 'little')
        imp = pe.imports.get(target)
        info['kind'] = 'ff15'
        info['target'] = target
        if imp:
            name, arity_dec, kind = parse_imp_name(imp)
            info['name'] = name
            info['imp'] = imp
            info['kind'] = 'import'
            if after_is_cdecl:
                info['conv'] = 'cdecl'
                info['arity'] = cdecl_n
                info['notes'].append('import followed by add esp — cdecl CRT')
            else:
                info['conv'] = 'stdcall'
                if arity_dec is not None:
                    info['arity'] = arity_dec
                    info['c_args'] = slice_args(arity_dec)
                if kind == 'win32':
                    info['notes'].append('win32/crt dllimport (windows.h)')
        else:
            info['kind'] = 'funcptr'
            mapped = (gmap.get(('0x%x' % target).lower())
                      or gmap.get('0x%08x' % target)
                      or gmap.get('0x%08X' % target))
            dat = mapped or ('DAT_%08x' % target)
            info['name'] = dat
            info['dat'] = dat
            info['conv'] = 'cdecl' if after_is_cdecl else 'stdcall'
            if after_is_cdecl:
                info['arity'] = cdecl_n

    # E8 rel32
    elif raw and raw[0] == 0xE8 and len(raw) == 5:
        rel = int.from_bytes(raw[1:5], 'little', signed=True)
        tgt = ins.address + 5 + rel
        info['kind'] = 'e8'
        info['target'] = tgt
        off = tgt - text_va
        thunk = text[off:off + 6] if 0 <= off < len(text) else b''
        if len(thunk) == 6 and thunk[0] == 0xFF and thunk[1] == 0x25:
            imp_addr = int.from_bytes(thunk[2:6], 'little')
            imp = pe.imports.get(imp_addr)
            info['kind'] = ('glide-thunk'
                            if imp and 'glide' in (imp or '').lower()
                            else 'thunk')
            if imp:
                name, arity_dec, kind = parse_imp_name(imp)
                info['name'] = name
                info['imp'] = imp
                if after_is_cdecl:
                    info['conv'] = 'cdecl'
                    info['arity'] = cdecl_n
                else:
                    info['conv'] = 'stdcall'
                    if arity_dec is not None:
                        info['arity'] = arity_dec
                        info['c_args'] = slice_args(arity_dec)
        else:
            info['name'] = 'FUN_%08x' % tgt
            retn = callee_ret_imm(tgt)
            if after_is_cdecl:
                info['conv'] = 'cdecl'
                info['arity'] = cdecl_n
            elif retn is not None:
                info['conv'] = 'stdcall'
                info['arity'] = retn // 4
                info['c_args'] = slice_args(retn // 4)
            else:
                # Callee ends `ret` (c3): it popped nothing. Those
                # pending pushes are NOT this call's stdcall args
                # (stdcall N>0 is `ret 4N`). 0-arg cdecl and 0-arg
                # stdcall are identical at the caller — leave cdecl.
                if _ecx_loaded_recently(insns, idx):
                    info['conv'] = 'thiscall'
                    info['thiscall'] = True
                    info['arity'] = 0
                    info['c_args'] = []
                    info['notes'].append(
                        'E8 + mov ecx + callee ret → thiscall 0-stack; '
                        'pending pushes belong to a later call')
                else:
                    info['conv'] = 'cdecl'
                    info['arity'] = 0
                    if pending > 0:
                        info['notes'].append(
                            'E8 callee-ret, no add-esp: 0-arg cdecl '
                            '(pending=%d kept for later call)' % pending)

    # call [reg+disp] / call [reg]  — vtable (FF /2, not FF 15 / not call r32)
    elif (raw and raw[0] == 0xFF and len(raw) >= 2
          and (raw[1] & 0x38) == 0x10
          and not (0xD0 <= raw[1] <= 0xD7)
          and not (len(raw) >= 6 and raw[1] == 0x15)):
        disp = 0
        m = re.search(
            r'\[(?:e[abcd]x|esi|edi|ebp|esp)\s*\+\s*(0x[0-9a-fA-F]+|\d+)\]',
            ins.op_str, re.I)
        if m:
            disp = int(m.group(1), 0)
        info['disp'] = disp
        info['kind'] = 'vtable'
        obj_pushed, ecx_is_obj = _vtable_shape(insns, idx)
        if after_is_cdecl:
            info['conv'] = 'cdecl'
            info['notes'].append('vtable call cleaned by add esp')
        elif ecx_is_obj and not obj_pushed:
            info['conv'] = 'thiscall'
            info['thiscall'] = True
            info['arity'] = pending  # stack args, this in ecx
        elif obj_pushed:
            info['conv'] = 'stdcall-com'
            info['arity'] = pending  # includes this
        elif pending == 0:
            info['conv'] = 'thiscall'
            info['thiscall'] = True
            info['arity'] = 0
            info['notes'].append('vtable 0-stack defaulted to thiscall')
        else:
            info['conv'] = 'stdcall-com'
            info['arity'] = pending
            info['notes'].append(
                'vtable with stack args, this-push not proven; stdcall-com')

    # call reg  (FF D0-D7) — typically a loaded import
    elif raw and raw[0] == 0xFF and len(raw) == 2 and 0xD0 <= raw[1] <= 0xD7:
        info['kind'] = 'callreg'
        info['conv'] = 'cdecl' if after_is_cdecl else 'stdcall'
        info['notes'].append('call-reg (loaded import); windows.h covers it')

    else:
        info['notes'].append('unclassified encoding %s' % raw.hex())
        info['conv'] = info['conv'] or 'cdecl'

    return info


def _ecx_loaded_recently(insns, idx, window=8):
    """True if ecx was loaded with a this-pointer (not a stosd count)."""
    for j in range(idx - 1, max(-1, idx - window) - 1, -1):
        ins = insns[j]
        if ins.mnemonic == 'call':
            return False
        if ins.mnemonic == 'mov' and ins.op_str.lower().startswith('ecx,'):
            src = _norm_op(ins.op_str.split(',', 1)[1])
            # `mov ecx, 0x80` for rep stosd is NOT a this pointer.
            # `mov ecx, 0x11849f30` (data VA) IS — 0x1006B0E0.
            if _is_small_imm(src):
                continue
            return True
    return False


def _vtable_shape(insns, idx, window=12):
    """Return (object_was_pushed, ecx_holds_object).

    stdcall COM:  mov eax, [obj]; mov ecx, [eax]; push eax; call [ecx+N]
                  vtable in the call's base register, this is pushed.
                  Also `push eax; mov edx, [eax]; call [edx+N]` (edx = vtable).
    thiscall:     mov ecx, obj;   mov edx, [ecx]; call [edx+N]
                  ecx = object, this is not pushed.

    Capstone spells the vtable load `dword ptr [eax]`; strip that or
    `[eax]` never matches and every COM site becomes thiscall.
    Prologue `push esi` / `push edi` must NOT count as a this-push.
    """
    pushed_regs = set()
    last_mov = {}  # dst -> most-recent src
    for j in range(idx - 1, max(-1, idx - window) - 1, -1):
        ins = insns[j]
        if ins.mnemonic == 'call':
            break
        op = _norm_op(ins.op_str)
        if ins.mnemonic == 'push':
            pushed_regs.add(ins.op_str.lower().strip())
        if ins.mnemonic == 'mov' and ',' in op:
            dst, src = op.split(',', 1)
            if dst not in last_mov:
                last_mov[dst] = src

    call_op = _norm_op(insns[idx].op_str)
    mbase = re.search(r'\[(e[abcd]x|esi|edi|ebp)', call_op)
    base = mbase.group(1) if mbase else None
    base_src = last_mov.get(base) if base else None

    def _is_vtable_load(src):
        return bool(re.match(r'\[(e[abcd]x|esi|edi|ebp)\]$', src or ''))

    def _vtable_objreg(src):
        m = re.match(r'\[(e[abcd]x|esi|edi|ebp)\]$', src or '')
        return m.group(1) if m else None

    # stdcall COM: call [vtable+N] where vtable = [objreg] and objreg pushed
    objreg = _vtable_objreg(base_src)
    if objreg and objreg in pushed_regs:
        return True, False
    ecx_src = last_mov.get('ecx')
    objreg = _vtable_objreg(ecx_src)
    if objreg and objreg in pushed_regs:
        return True, False

    ecx_is_obj = False
    if ecx_src is not None:
        if _is_vtable_load(ecx_src):
            ecx_is_obj = False
        elif _is_small_imm(ecx_src):
            ecx_is_obj = False
        else:
            # ecx = [global] / ebx / [eax+disp] / data-VA imm
            ecx_is_obj = True
    # thiscall: vtable loaded from [ecx] and ecx was not pushed
    if _vtable_objreg(base_src) == 'ecx' and 'ecx' not in pushed_regs:
        ecx_is_obj = True
    return False, ecx_is_obj


# ---------------------------------------------------------------------------
# Source rewrite
# ---------------------------------------------------------------------------

# Ghidra/wrap form: (**(funcptr *)(*OBJ + DISP))(ARGS)
# Note TWO adjacent stars, then the cast — not (*(*(funcptr *)...)).
_VTBL_RE = re.compile(
    r'\(\s*\*\s*\*\s*\(\s*(?:funcptr|code)\s*\*\s*\)\s*'
    r'\(\s*\*\s*(?P<obj>[^;]+?)\s*\+\s*(?P<disp>0x[0-9a-fA-F]+|\d+)\s*\)\s*'
    r'\)\s*\(\s*(?P<args>[^;]*?)\s*\)',
    re.S)

# (**(funcptr *)(**(int **)(INNER) + DISP))(ARGS)
_VTBL_INDIRECT_RE = re.compile(
    r'\(\s*\*\s*\*\s*\(\s*(?:funcptr|code)\s*\*\s*\)\s*'
    r'\(\s*\*\*\(\s*int\s*\*\*\s*\)\s*\((?P<inner>[^;]+?)\)\s*'
    r'\+\s*(?P<disp>0x[0-9a-fA-F]+|\d+)\s*\)\s*\)\s*'
    r'\(\s*(?P<args>[^;]*?)\s*\)',
    re.S)

# (**(funcptr *)*OBJ)(ARGS)  — vtable +0, Ghidra drops the `+ 0`
# Proven 0x1003FDA0: (**(funcptr *)*g_brPhaseAA2904)(1)
_VTBL_STAR_RE = re.compile(
    r'\(\s*\*\s*\*\s*\(\s*(?:funcptr|code)\s*\*\s*\)\s*'
    r'\*\s*(?P<obj>[A-Za-z_]\w*)\s*'
    r'\)\s*\(\s*(?P<args>[^;]*?)\s*\)',
    re.S)

# (**(funcptr *)(*OBJ))(ARGS)  — vtable +0 with inner parens, no disp
_VTBL_NODISP_RE = re.compile(
    r'\(\s*\*\s*\*\s*\(\s*(?:funcptr|code)\s*\*\s*\)\s*'
    r'\(\s*\*\s*(?P<obj>[^;]+?)\s*\)\s*'
    r'\)\s*\(\s*(?P<args>[^;]*?)\s*\)',
    re.S)

_FUNCPTR_CALL_RE = re.compile(
    r'\(\s*\*\s*(?P<name>(?:DAT_|g_|PTR_FUN_|_DAT_)[0-9a-fA-FxA-F]+|\w+)\s*\)\s*'
    r'\(\s*(?P<args>[^;]*?)\s*\)')


def _n_args_c(args):
    args = (args or '').strip()
    if not args:
        return 0
    depth = 0
    n = 1
    for ch in args:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        elif ch == ',' and depth == 0:
            n += 1
    return n


def _ints(n):
    return ', '.join(['int'] * n) if n else 'void'


def _fastcall_sig(stack_n):
    """thiscall with stack_n stack args → __fastcall prototype args."""
    if stack_n <= 0:
        return 'void *this'
    return 'void *this, int _edx_unused' + (
        ', ' + _ints(stack_n) if stack_n else '')


def _iter_c_calls(body, name):
    """Yield (start, end, args_str) for name(...) in body."""
    if name in C_KEYWORDS:
        return
    for m in re.finditer(r'\b%s\s*\(' % re.escape(name), body):
        k = m.end()
        depth = 1
        end = k
        while end < len(body) and depth:
            if body[end] == '(':
                depth += 1
            elif body[end] == ')':
                depth -= 1
            end += 1
        yield m.start(), end, body[k:end - 1]


def _pad_args(args, want, recovered, types=None):
    """Build a C arg list of length `want`. Prefer orig-recovered immediates."""
    n = _n_args_c(args)
    rec = recovered or []
    if rec and len(rec) >= want and all(x is not None for x in rec[:want]):
        out = list(rec[:want])
    elif n >= want:
        return args.strip()
    elif args.strip():
        out = [a.strip() for a in args.split(',')] + ['0'] * (want - n)
    else:
        out = ['0'] * want
    if types:
        for i, t in enumerate(types):
            if i < len(out) and t == 'float' and out[i] in ('0', '0x0'):
                out[i] = '0.0f'
    return ', '.join(out)


def transform(src, calls):
    """Rewrite wrapped Ghidra C. Returns (new_src, report_lines)."""
    report = []
    marker = src.find('Forward declarations')
    body_start = src.find('{', marker if marker >= 0 else 0)
    if body_start < 0:
        body_start = src.find('{')
    head, body = src[:body_start], src[body_start:]

    vtable_calls = [c for c in calls if c['kind'] == 'vtable']
    glide_calls = [c for c in calls
                   if _cname(c).startswith('gr') or _cname(c).startswith('gu')]
    funcptr_calls = [c for c in calls if c['kind'] == 'funcptr']
    fun_std = [c for c in calls
               if c['kind'] == 'e8' and c['conv'] == 'stdcall'
               and _cname(c).startswith('FUN_')]
    thiscall_funs = [c for c in calls
                     if c['conv'] == 'thiscall' and _cname(c).startswith('FUN_')]

    # --- vtable sites: pair by displacement, in order ---
    vtbl_by_disp = {}
    for c in vtable_calls:
        d = c['disp'] if c['disp'] is not None else 0
        vtbl_by_disp.setdefault(d, []).append(c)

    typedefs = []
    typedef_index = {}  # ('std'|'fast', arity) -> typename

    def tname(conv, arity):
        kind = 'fast' if conv == 'thiscall' else 'std'
        key = (kind, arity)
        if key not in typedef_index:
            if kind == 'fast':
                tn = 'CC_fast_%d' % arity
                typedefs.append(
                    'typedef int (__fastcall *%s)(%s);'
                    % (tn, _fastcall_sig(arity)))
            else:
                tn = 'CC_std_%d' % arity
                typedefs.append(
                    'typedef int (__stdcall *%s)(%s);' % (tn, _ints(arity)))
            typedef_index[key] = tn
        return typedef_index[key]

    used_vtbl = {}  # disp -> how many consumed

    def pick_vtbl(disp):
        d = disp if isinstance(disp, int) else int(disp, 0)
        lst = vtbl_by_disp.get(d, [])
        k = used_vtbl.get(d, 0)
        used_vtbl[d] = k + 1
        if k < len(lst):
            return lst[k]
        if lst:
            return lst[-1]
        return None

    def rewrite_vtbl(m, obj_expr, disp=None):
        if disp is None:
            disp = int(m.group('disp'), 0)
        args = (m.group('args') or '').strip()
        ci = pick_vtbl(disp)
        if ci is None:
            report.append('vtable +0x%x: no orig call with this displacement'
                          % disp)
            return m.group(0)
        conv = ci['conv']
        arity = ci['arity'] if ci['arity'] is not None else _n_args_c(args)
        if conv == 'thiscall':
            tn = tname('thiscall', arity)
            parts = [a.strip() for a in args.split(',')] if args else []
            if parts and parts[0].replace(' ', '') == obj_expr.replace(' ', ''):
                stack = parts[1:]
            else:
                stack = parts
            if arity > 0:
                # __fastcall(this, edx_unused, stack...): C is f(this, 0, stack)
                if len(stack) < arity:
                    rec = ci.get('c_args') or []
                    if rec and len(rec) >= arity and all(
                            x is not None for x in rec[:arity]):
                        stack = rec[:arity]
                    else:
                        stack = stack + ['0'] * (arity - len(stack))
                        report.append(
                            'vtable +0x%x thiscall: padded %d stack args'
                            % (disp, arity - len(parts)))
                new_args = obj_expr + ', 0' + (
                    ', ' + ', '.join(stack) if stack else '')
            else:
                new_args = obj_expr
            report.append('vtable +0x%x → __fastcall thiscall arity=%d'
                          % (disp, arity))
            return '(*(%s *)(*(int *)(%s) + %d))(%s)' % (
                tn, obj_expr, disp, new_args)
        else:
            tn = tname('stdcall-com', arity)
            parts = [a.strip() for a in args.split(',')] if args else []
            if not parts or parts[0].replace(' ', '') != obj_expr.replace(' ', ''):
                new_args = obj_expr + (', ' + args if args else '')
            else:
                new_args = args
            n = _n_args_c(new_args)
            if arity and n < arity:
                rec = ci.get('c_args') or []
                new_args = _pad_args(new_args, arity, rec)
                report.append(
                    'vtable +0x%x stdcall-COM padded %d→%d' % (disp, n, arity))
            report.append('vtable +0x%x → __stdcall COM arity=%d'
                          % (disp, arity))
            return '(*(%s *)(*(int *)(%s) + %d))(%s)' % (
                tn, obj_expr, disp, new_args)

    def sub_direct(m):
        return rewrite_vtbl(m, m.group('obj').strip())

    def sub_indirect(m):
        inner = m.group('inner').strip()
        obj = '*(int **)(%s)' % inner
        return rewrite_vtbl(m, obj)

    def sub_star(m):
        return rewrite_vtbl(m, m.group('obj').strip(), disp=0)

    def sub_nodisp(m):
        return rewrite_vtbl(m, m.group('obj').strip(), disp=0)

    new_body = _VTBL_INDIRECT_RE.sub(sub_indirect, body)
    new_body = _VTBL_RE.sub(sub_direct, new_body)
    new_body = _VTBL_STAR_RE.sub(sub_star, new_body)
    new_body = _VTBL_NODISP_RE.sub(sub_nodisp, new_body)

    # --- pad named stdcall/glide calls Ghidra under-arited ---
    # Pair orig sites of each name with C call sites in order. When Ghidra
    # turned stdcall stack args into fake locals (`grAlphaCombine();`),
    # fill from orig push immediates so the stdcall prototype compiles
    # and the caller emits N pushes + no add-esp.
    # Pad only identifier-called stdcall names (glide thunks / local
    # stdcall). Funcptr sites are `(*g_pfn)(args)` — the decl rewrite
    # is what matters; looking for `g_pfn(` is a false CSE FLAG.
    by_name = {}
    for c in calls:
        n = _cname(c)
        if not n or n in C_KEYWORDS or n in WIN32_API:
            continue
        if c['kind'] in ('glide-thunk', 'thunk') or n in GLIDE_SIGS:
            by_name.setdefault(n, []).append(c)
        elif c['kind'] == 'e8' and c['conv'] == 'stdcall':
            by_name.setdefault(n, []).append(c)

    for m in re.finditer(r'\b((?:gr|gu)[A-Z]\w*)\s*\(', new_body):
        n = m.group(1)
        by_name.setdefault(n, [])

    for n, sites in list(by_name.items()):
        c_sites = list(_iter_c_calls(new_body, n))
        want_default = None
        types = None
        if n in GLIDE_SIGS:
            types = GLIDE_SIGS[n][1]
            want_default = len(types)
        if sites and len(c_sites) != len(sites):
            report.append(
                'FLAG: %s C sites %d vs orig %d (CSE/desync)'
                % (n, len(c_sites), len(sites)))
        if not c_sites:
            continue
        # rewrite from the end so offsets stay valid
        for i, (start, end, args) in reversed(list(enumerate(c_sites))):
            ci = sites[i] if i < len(sites) else (sites[-1] if sites else None)
            want = want_default
            rec = None
            if ci is not None:
                if want is None:
                    want = ci.get('arity') or 0
                rec = ci.get('c_args')
            if not want:
                continue
            n_have = _n_args_c(args)
            if n_have >= want:
                continue
            new_args = _pad_args(args, want, rec, types)
            new_body = (new_body[:start] + '%s(%s)' % (n, new_args)
                        + new_body[end:])
            report.append('pad %s() %d→%d args' % (n, n_have, want))

    # --- declarations to insert/replace in the head ---
    decl_lines = []

    # Glide: one decl per unique name, arity from @N / table
    seen_glide = set()
    for c in glide_calls:
        n = _cname(c)
        if not n or n in seen_glide:
            continue
        seen_glide.add(n)
        if n in GLIDE_SIGS:
            ret, args_t = GLIDE_SIGS[n]
            alist = ', '.join(args_t) if args_t else 'void'
        else:
            ret = 'void'
            alist = _ints(c['arity'] or 0)
        # if the C site uses the return value, don't declare void
        if ret == 'void' and re.search(r'=\s*%s\s*\(' % re.escape(n), new_body):
            ret = 'int'
        decl_lines.append('%s __stdcall %s(%s);' % (ret, n, alist))
        report.append('glide %s → __stdcall %s(%s)' % (n, ret, alist))

    # named gr*/gu* that appear in C even if orig pairing used a thunk
    for m in re.finditer(r'\b((?:gr|gu)[A-Z]\w*)\s*\(', new_body):
        n = m.group(1)
        if n in seen_glide:
            continue
        seen_glide.add(n)
        if n in GLIDE_SIGS:
            ret, args_t = GLIDE_SIGS[n]
            alist = ', '.join(args_t) if args_t else 'void'
        else:
            k = m.end()
            depth = 1
            end = k
            while end < len(new_body) and depth:
                if new_body[end] == '(':
                    depth += 1
                elif new_body[end] == ')':
                    depth -= 1
                end += 1
            alist = _ints(_n_args_c(new_body[k:end - 1]))
            ret = 'void'
        if ret == 'void' and re.search(r'=\s*%s\s*\(' % re.escape(n), new_body):
            ret = 'int'
        decl_lines.append('%s __stdcall %s(%s);' % (ret, n, alist))
        report.append('glide(C-name) %s → __stdcall %s(%s)' % (n, ret, alist))

    # funcptr globals
    seen_fp = set()
    for c in funcptr_calls:
        dat = c.get('dat') or c.get('name')
        if not dat or dat in seen_fp:
            continue
        seen_fp.add(dat)
        arity = c['arity'] or 0
        if c['conv'] == 'stdcall':
            decl_lines.append(
                'extern int (__stdcall *%s)(%s);' % (dat, _ints(arity)))
            report.append('funcptr %s → __stdcall arity=%d' % (dat, arity))
            head = re.sub(
                r'^extern (?:funcptr|int|int \*) %s;\s*\n' % re.escape(dat),
                '', head, flags=re.M)

    # local FUN_ stdcall (ret imm16) — wrap_for_compile may already have
    # done this; re-assert.
    seen_fun = set()
    for c in fun_std:
        n = _cname(c)
        parts = n.split('_', 1)
        nlow = 'FUN_' + parts[1] if len(parts) == 2 else n
        if nlow in seen_fun:
            continue
        seen_fun.add(nlow)
        arity = c['arity'] or 0
        new_decl = 'int __stdcall %s(%s);' % (nlow, _ints(arity))
        if re.search(r'\b%s\s*\(' % nlow, head):
            head = re.sub(
                r'^int(?:\s+__stdcall)?\s+%s\s*\([^;]*\);' % nlow,
                new_decl, head, flags=re.M)
            report.append('local %s → __stdcall arity=%d' % (nlow, arity))
        else:
            decl_lines.append(new_decl)
            report.append('local %s → __stdcall arity=%d (inserted)'
                          % (nlow, arity))

    # thiscall FUN_ (ecx=this, ret). Declaring `__fastcall(this)` does
    # not insert `this` at `FUN_x()` — VC5 will not load ecx — and
    # Ghidra often attached a later cdecl's pushes (`FUN_x(1,1)`),
    # which then C2198 against a 1-arg prototype. Flag; leave cdecl.
    seen_tc = set()
    for c in thiscall_funs:
        n = _cname(c)
        parts = n.split('_', 1)
        nlow = 'FUN_' + parts[1] if len(parts) == 2 else n
        if nlow in seen_tc:
            continue
        seen_tc.add(nlow)
        report.append(
            'FLAG: %s thiscall stack=%d: C still needs ecx=this at each '
            'call site. Decl left cdecl (empty FUN_x() will not set ecx; '
            'extra C args are a later call\'s pushes — 0x1006B0E0).'
            % (nlow, c['arity'] or 0))

    # splice typedefs + decls into the forward-decl block
    extra = ''
    if typedefs:
        extra += '\n'.join(typedefs) + '\n'
    if decl_lines:
        extra += '\n'.join(decl_lines) + '\n'
    if extra:
        # Insert immediately before the function definition — NOT inside
        # `#ifndef NAN` (math.h defines NAN, so decls placed there vanish).
        sigs = list(re.finditer(
            r'\n[^\n]*\b(?:FUN_|THUNK_)[0-9a-fA-F]+\s*\(', head))
        if sigs:
            pos = sigs[-1].start()
            head = head[:pos] + '\n' + extra + head[pos:]
        else:
            head = head + extra

    return head + new_body, report


# ---------------------------------------------------------------------------
# Scoring / CLI
# ---------------------------------------------------------------------------

def load_wrapped(va_hex, from_decomp=False):
    """Return (src, func_name). Never writes ghidra_work."""
    import ghidra_to_match as g
    work = os.path.join(WORK_DIR, va_hex + '.c')
    if not from_decomp and os.path.exists(work):
        src = open(work).read()
        m = re.search(
            r'\n(?:\w+[\w \*]*)\b(FUN_[0-9a-fA-F]+|THUNK_[0-9a-fA-F]+)\s*\(',
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
    gmap = g.load_globals() if hasattr(g, 'load_globals') else {}
    fn_names = g.load_fn_names() if hasattr(g, 'load_fn_names') else {}
    func_name, cleaned = g.prepare_function(target, gmap, fn_names)
    src = g.wrap_for_compile(cleaned, va_hex)
    return src, func_name


def score_src(src, func_name, va_hex, tag):
    import ghidra_to_match as g
    import match_sweep
    orig = match_sweep.load_orig(
        os.path.join(ORIG_DIR, va_hex + '.bin'), va_hex)
    r = g._score_source(src, func_name, orig,
                        ['/O2', '/Od', '/O2 /Oy-'], tag)
    if r[0] is None:
        import tempfile
        tmp = tempfile.mkdtemp(prefix='ghidra_ref_',
                               dir=os.path.join(ROOT, 'build'))
        path = os.path.join(tmp, 'r.c')
        open(path, 'w').write(src)
        obj, errs = match_sweep.compile_variant(path, tag + '_err', '/O2')
        print('  COMPILE FAIL:', errs[:8] if errs else 'no obj')
    return r[0], r[1]


def _fmt_call(c):
    disp = c.get('disp')
    name = c.get('name') or (
        ('+0x%x' % disp) if disp is not None else '')
    return '  0x%x  %-14s  %-12s  arity=%s  %s  %s' % (
        c['addr'], c['kind'], c['conv'], c['arity'], name,
        '; '.join(c['notes'][:1]))


def run_one(va_hex, from_decomp=False, verbose=True, score=True):
    pe = _pe()
    va = int(va_hex, 16)
    src, fname = load_wrapped(va_hex, from_decomp=from_decomp)
    calls = analyze_orig(va, pe)
    if verbose:
        print('== %s  (%s)  %d orig calls' % (
            va_hex, 'decomp' if from_decomp else 'work', len(calls)))
        for c in calls:
            print(_fmt_call(c))
    new_src, report = transform(src, calls)
    if verbose:
        print('  transform:')
        for line in report:
            print('   -', line)
    if not score:
        return {
            'va': va_hex, 'before': None, 'after': None,
            'ncalls': len(calls), 'report': report, 'src': new_src,
        }
    before = score_src(src, fname, va_hex, 'ccb' + va_hex)
    after = score_src(new_src, fname, va_hex, 'cca' + va_hex)
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
        'ncalls': len(calls),
        'report': report,
        'match': after[0] == 0,
    }


PREY = [
    '0x1006C6A0', '0x100703D0', '0x10003030', '0x10002F70', '0x1006B0E0',
    '0x1003FDA0', '0x100583C0', '0x100704E0', '0x10023B70', '0x1001DFB0',
    '0x10002580',
]


def main():
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except Exception:
        pass
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--va', help='single VA, e.g. 0x1006C6A0')
    ap.add_argument('--from-decomp', action='store_true',
                    help='wrap build/ghidra_decomp (isolates the convention fix)')
    ap.add_argument('--validate', action='store_true',
                    help='run the prey list from decomp and print a table')
    ap.add_argument('--no-score', action='store_true',
                    help='classify + transform only (no MSVC score)')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args()

    if args.validate:
        rows = []
        for va in PREY:
            decomp = os.path.join(GHIDRA_DIR, va + '.c')
            orig = os.path.join(ORIG_DIR, va + '.bin')
            if not os.path.exists(orig) or not os.path.exists(decomp):
                print('skip %s (missing orig/decomp)' % va)
                continue
            print('\n######## %s ########' % va)
            try:
                rows.append(run_one(va, from_decomp=True,
                                    verbose=not args.quiet,
                                    score=not args.no_score))
            except Exception as e:
                print('  ERROR', type(e).__name__, e)
                rows.append({'va': va, 'before': None, 'after': None,
                             'error': str(e)})
        print('\n======== convention-fix delta (from decomp wrap) ========')
        print('%-12s %8s %8s %8s' % ('VA', 'before', 'after', 'delta'))
        for r in rows:
            if r.get('before') is None and r.get('error'):
                print('%-12s  ERROR %s' % (r['va'], r.get('error', '')[:60]))
                continue
            if r.get('before') is None:
                print('%-12s  (no score)  %d calls' % (
                    r['va'], r.get('ncalls', 0)))
                continue
            b, a = r['before'], r['after']
            d = (a - b) if (a is not None and b is not None) else None
            mark = ' MATCH' if r.get('match') else ''
            print('%-12s %8s %8s %8s%s' % (
                r['va'], b, a, d, mark))
        return

    if not args.va:
        ap.print_help()
        return
    va = args.va
    if not va.lower().startswith('0x'):
        va = '0x' + va
    va = '0x%08X' % int(va, 16)
    run_one(va, from_decomp=args.from_decomp, verbose=True,
            score=not args.no_score)


if __name__ == '__main__':
    main()
