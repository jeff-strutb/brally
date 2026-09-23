#!/usr/bin/env python3
"""t3live.py -- the LIVE T3 oracle: original vs transcription, on real inputs.

The original game runs headless (tools/brbox.py) under a scripted input
timeline (tools/brbox_drive.py).  Every T3 function's entry is hooked.  When
the game calls one, the machine stops at the entry, and from that EXACT state
-- registers, x87 stack, all of memory, all host state -- both bodies run to
their return:

    1. the ORIGINAL body (the bytes at the VA) -- its end state is kept, and
       the game continues from it, so the drive is the original's drive;
    2. the T3 body -- the compiled transcription, relocated to the original's
       own addresses (t3b_env.resolve_bytes) and placed in a scratch region --
       then every one of its effects is undone.

Everything the caller could observe is compared, whatever the declared
return type:

    * every byte either side wrote outside its own stack frame and incoming
      argument slots (globals, heap, the caller's frame, buffers passed in),
      including bytes written for it by import models (fread, sprintf, ...);
    * the import calls each side made, in order, with their arguments
      (pointers into the callee's own frame normalised, since two compilations
      lay a frame out differently by design);
    * esp after return (the calling convention), the callee-saved registers,
      the x87 stack depth and, when the function leaves a value on it, st(0)
      bit for bit (Unicorn's x87 is 80-bit softfloat -- there is no rounding
      excuse any more: a differing float IS a differing result);
    * eax and edx -- when the caller actually reads them.  That is decided
      dynamically: after the capture the game runs on, and the next few
      hundred instructions it really executes are decoded until each register
      is read (live: the difference counts) or overwritten (dead).  A void
      transcription of a function whose callers use its return is caught
      here, which the declared signature never could.

Verdicts go to config/t3_live.csv (tools/t3ledger.py).  A function no script
reaches is UNCOVERED, never passed.

    .venv/bin/python tools/t3live.py run tools/brbox_scripts/*.txt
    .venv/bin/python tools/t3live.py run S.txt --only 0x10019A70 --per-fn 20
    .venv/bin/python tools/t3live.py report
"""
from __future__ import print_function

import argparse
import collections
import datetime
import json
import os
import struct
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_PROT_ALL, UcError  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX,  # noqa: E402
                               UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
                               UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX)
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402
from capstone import x86_const as X  # noqa: E402

import brbox  # noqa: E402
from brbox import GuestFault, Stop  # noqa: E402

ALT_LO, ALT_SIZE = 0x60000000, 0x01000000       # where T3 bodies are placed
OUT_DIR = os.path.join(ROOT, 'build', 'brbox', 'live')
_md = Cs(CS_ARCH_X86, CS_MODE_32)
_md.detail = True
EAX_FAM = {X.X86_REG_EAX, X.X86_REG_AX, X.X86_REG_AL, X.X86_REG_AH}
EDX_FAM = {X.X86_REG_EDX, X.X86_REG_DX, X.X86_REG_DL, X.X86_REG_DH}


# ============================================================ T3 bodies ====

def t3_targets(only=None):
    """[(va, name, file)] for every @t3 VA (or `only`)."""
    import t3
    cert = t3.certified()
    rows = t3.report_rows()
    out = []
    for va in sorted(cert):
        if va.startswith('?') or not cert[va]['ok']:
            continue
        if only and va not in only:
            continue
        r = rows.get(va)
        out.append((int(va, 16), r['name'] if r else '', r['file'] if r else cert[va]['file'], r))
    return out


def build_body(va, row):
    """The T3 transcription's bytes resolved for execution at `va`, plus its
    relocation list: (code, relocs, why).  relocs = [(off, type, internal)]."""
    import t3
    import t3b_env as ENV
    import reloc_fill
    if row is None:
        return None, None, 'no report row (unswept)'
    obj = t3._find_obj(row)
    if not obj:
        return None, None, 'no sweep object for %s' % row['file']
    name = row['name']
    if row.get('cpp'):
        import cpp_score
        _n, cpp_symbol, _k = cpp_score.parse_implements_name(os.path.join(ROOT, row['file']), va)
        if cpp_symbol:
            name = cpp_symbol
    sym = t3._sym(obj, name)
    if not sym:
        return None, None, 'symbol %s not in %s' % (row['name'], os.path.basename(obj))
    from match_diff import parse_coff_obj
    code0 = parse_coff_obj(obj)[sym]
    code0 = code0[0] if isinstance(code0, tuple) else code0
    size = len(code0)
    code, why = ENV.resolve_bytes(obj, sym, va, size)
    if code is None:
        blocked = ENV.unresolved_symbols(obj, sym, size)
        return None, None, 'unresolved: %s (%s)' % (why, ', '.join(blocked[:3]))
    d, secs, syms, relocs = reloc_fill.parse(obj)
    fn = next((s for s in syms if reloc_fill.func_symbol_matches(s['name'], sym)
               and secs.get(s['sec'], {}).get('name', '').startswith('.text')), None)
    rl = []
    for rva, si, rt in relocs.get(fn['sec'], []):
        off = rva - fn['val']
        if not (0 <= off < size - 3):
            continue
        ts = next((s for s in syms if s['idx'] == si), None)
        rl.append((off, rt, bool(ts and ts['sec'] == fn['sec'])))
    stale = ''
    try:
        if os.path.getmtime(os.path.join(ROOT, row['file'])) > os.path.getmtime(obj):
            stale = 'object older than %s' % row['file']
    except OSError:
        pass
    return code, rl, stale


def rebase(code, relocs, va, alt):
    """Move resolved code from `va` to `alt`: a rel32 to anything outside the
    function keeps its target; a dir32 into the function follows it."""
    import reloc_fill
    b = bytearray(code)
    delta = alt - va
    for off, rt, internal in relocs:
        v = struct.unpack_from('<I', b, off)[0]
        if rt == reloc_fill.REL_REL32 and not internal:
            v = (v - delta) & 0xFFFFFFFF
        elif rt == reloc_fill.REL_DIR32 and internal:
            v = (v + delta) & 0xFFFFFFFF
        struct.pack_into('<I', b, off, v)
    return bytes(b)


# ============================================================== oracle =====

class Target(object):
    def __init__(self, va, name, src):
        self.va, self.name, self.src = va, name, src
        self.alt = None
        self.calls = 0
        self.captures = []
        self.sites = set()
        self.hook = None
        self.placed = None
        self.note = ''
        self.nargs = None
        self.done = False
        self.nested = 0
        self.strikes = 0


class Oracle(object):
    def __init__(self, box, targets, per_fn=6, log=print, image=None):
        self.box = box
        self.log = log
        self.per_fn = per_fn
        self.t = {}
        self.probe = None
        self.pending_live = []
        box.uc.mem_map(ALT_LO, ALT_SIZE, UC_PROT_ALL)
        nxt = ALT_LO
        timg = self._map_image(image) if image else None
        for va, name, src, row in targets:
            t = Target(va, name, src)
            if timg is not None:
                placed = self._placed(timg, va, row)
                if placed is not None:
                    t.placed = placed
                    t.alt = va
                    import t3obj
                    t.nargs = t3obj.nparams(src, name)
                    t.hook = box.uc.hook_add(UC_HOOK_CODE, self._entry, begin=va, end=va, user_data=t)
                    self.t[va] = t
                    continue
            code, relocs, note = build_body(va, row)
            if timg is not None:
                note = ('not placed in the image; object mode' + (' -- ' + note if note else '')) \
                    if code is not None else note
            if code is None:
                t.note = note
                t.done = True
                self.t[va] = t
                continue
            t.note = note or ''
            t.alt = nxt
            box.uc.mem_write(nxt, rebase(code, relocs, va, nxt))
            nxt = (nxt + len(code) + 0x40) & ~0xF
            import t3obj
            t.nargs = t3obj.nparams(src, name)
            t.hook = box.uc.hook_add(UC_HOOK_CODE, self._entry, begin=va, end=va, user_data=t)
            self.t[va] = t

    # ------------------------------------------------------- image mode --
    def _map_image(self, path):
        """Map the sections a built T3 image adds (the .t3x annex) at their
        VAs; the image's own .text stays the ORIGINAL's until a capture swaps
        one span in."""
        import pe as PE
        box = self.box
        timg = PE.load(path)
        for sec in timg.sections:
            lo = timg.image_base + sec.vaddr
            if box.pe.section_by_name(sec.name) is not None:
                continue
            hi = lo + max(sec.vsize, sec.raw_size)
            if hi > box.img_hi:
                box.uc.mem_map(box.img_hi, ((hi + 0xFFFF) & ~0xFFFF) - box.img_hi, UC_PROT_ALL)
                box.img_hi = (hi + 0xFFFF) & ~0xFFFF
            box.uc.mem_write(lo, timg.data[sec.raw_ptr:sec.raw_ptr + sec.raw_size])
            self.log('image %s: mapped %s at %08X (%d B)' % (os.path.basename(path), sec.name,
                                                             lo, sec.raw_size))
        return timg

    mutate = False

    def _placed(self, timg, va, row):
        size = int(row['orig_size']) if row and row.get('orig_size') else 0
        if not size:
            return None
        placed = timg.read(va, size)
        orig = self.box.rd(va, size)
        if placed is None or placed == orig:
            return None
        if self.mutate:
            placed = self._plant_bug(timg, va, placed)
        return (orig, placed)

    def _plant_bug(self, timg, va, placed):
        """Negative control: invert the first conditional branch of the T3
        body (following an annex thunk if the span is one).  A working oracle
        must call the function DIVERGENT whenever that branch is reached."""
        b = bytearray(placed)
        tgt, base = b, va
        if b[0] == 0xE9:                                   # jmp annex
            a_va = (va + 5 + struct.unpack_from('<i', b, 1)[0]) & 0xFFFFFFFF
            tgt = bytearray(self.box.rd(a_va, 0x400))
            base = a_va
        from capstone import Cs as _Cs
        md = _Cs(CS_ARCH_X86, CS_MODE_32)
        for ins in md.disasm(bytes(tgt), base):
            op = ins.bytes
            if 0x70 <= op[0] <= 0x7F or (op[0] == 0x0F and 0x80 <= op[1] <= 0x8F):
                k = ins.address - base + (0 if op[0] != 0x0F else 1)
                tgt[k] ^= 1
                break
        if tgt is not b:
            self.box.uc.mem_write(base, bytes(tgt))   # the annex copy is the T3's alone
            return bytes(b)
        return bytes(b)

    def _swap(self, t, which):
        """Put the original's (0) or the shipped image's (1) bytes at the VA."""
        b = t.placed[which]
        self.box.uc.mem_write(t.va, b)
        self.box.uc.ctl_remove_cache(t.va, t.va + len(b))

    # ------------------------------------------------------------ entry --
    def _entry(self, uc, addr, size, t):
        box = self.box
        if box.subrun:
            t.nested += 1            # reached inside another capture's sub-run
            return
        if self.skip_once == addr:
            self.skip_once = None    # the rolled-back call, now running for real
            return
        t.calls += 1
        if t.done:
            if t.calls > 50000 and t.hook is not None:
                uc.hook_del(t.hook)      # hot and finished: stop paying for the hook
                t.hook = None
            return
        n = len(t.captures)
        if n >= self.per_fn or self.probe is not None:
            return                   # (a liveness probe owns the next instructions)
        if self.explain_frame is not None:
            if box.hs.frame < self.explain_frame:
                return
            box.intermission = lambda b, _t=t: self.capture(_t)
            uc.emu_stop()
            return
        ret = box.rd32(uc.reg_read(UC_X86_REG_ESP))
        # the first call, the first call from each new site, then a spread
        if n == 0 or ret not in t.sites or t.calls in (2, 4, 8, 16, 64, 256, 1024, 4096):
            box.intermission = lambda b, _t=t: self.capture(_t)
            uc.emu_stop()

    # ---------------------------------------------------------- one side --
    def run_side(self, start, ret, esp0, budget_s, trace=None, body=None, perturb=False):
        box, uc = self.box, self.box.uc
        wlog = []
        pend_calls = []      # perturb: (return address, esp at the call, saved regs)
        hcall = None
        calls = []
        hprobe = []
        side_env = 'T3LIVE_PROBE_O' if (body and body[0][1] - body[0][0] == len(self._cur_t.placed[0])
                                         and start == self._cur_t.va and not self._cur_side_t3) \
            else 'T3LIVE_PROBE_T'
        probes = [int(x, 16) for x in os.environ.get(side_env, '').split(',') if x] \
            if body is not None else []
        for pa in probes:
            def on_probe(uc_, a, sz, ud):
                top = box.fpu_top()
                box.log_probe.append((a, [round(box.st(k), 9) for k in range(4)],
                                      {n: uc_.reg_read(r) for n, r in (('eax', UC_X86_REG_EAX),
                                                                       ('ebx', UC_X86_REG_EBX),
                                                                       ('ecx', UC_X86_REG_ECX),
                                                                       ('esi', UC_X86_REG_ESI),
                                                                       ('edi', UC_X86_REG_EDI),
                                                                       ('ebp', UC_X86_REG_EBP),
                                                                       ('esp', UC_X86_REG_ESP))}))
            hprobe.append(uc.hook_add(UC_HOOK_CODE, on_probe, begin=pa, end=pa))
            uc.ctl_remove_cache(pa, pa + 1)
        if body is not None:
            box.log_probe = []
            # every `call` the function's OWN code executes,
            # with its resolved target, in order
            md = Cs(CS_ARCH_X86, CS_MODE_32)
            cache = {}

            def on_body(uc_, a, sz, ud):
                ins = cache.get(a)
                if ins is None:
                    try:
                        ins = next(md.disasm(bytes(uc_.mem_read(a, 16)), a))
                    except StopIteration:
                        ins = False
                    cache[a] = ins
                if perturb and pend_calls and a == pend_calls[-1][0] and \
                        uc_.reg_read(UC_X86_REG_ESP) >= pend_calls[-1][1]:
                    for k, v in pend_calls.pop()[2]:
                        uc_.reg_write(k, v)
                if ins and ins.mnemonic == 'call' and perturb:
                    # the callee-saved registers are the caller's private
                    # values; VC5 code never takes them as inputs, only saves
                    # and restores them -- so a callee's result can depend on
                    # them only through an uninitialised read of a slot a
                    # prologue pushed them into.  Perturb them for the call,
                    # restore them at the return.
                    saved = [(k, uc_.reg_read(k)) for k in PERTURB_REGS]
                    for k, v in saved:
                        uc_.reg_write(k, v ^ 0x5AC3A55C)
                    pend_calls.append((a + ins.size, uc_.reg_read(UC_X86_REG_ESP), saved))
                if ins and ins.mnemonic == 'call' and not perturb:
                    op = ins.op_str
                    if op.startswith('0x'):
                        tgt = int(op, 16)
                    elif 'ptr [' in op and '+' not in op and '*' not in op:
                        tgt = struct.unpack('<I', bytes(uc_.mem_read(int(op.split('[')[1][:-1], 16), 4)))[0]
                    else:
                        tgt = op
                    esp = uc_.reg_read(UC_X86_REG_ESP)
                    words = struct.unpack('<16I', bytes(uc_.mem_read(esp, 64)))
                    blobs = []
                    for w in words[:3]:
                        try:
                            blobs.append(bytes(uc_.mem_read(w, 36)))
                        except UcError:
                            blobs.append(None)
                    calls.append((a, tgt, words, len(trace) if trace is not None else 0, blobs))
                    if os.environ.get('T3LIVE_SCRUB'):
                        # diagnostic: zero the dead stack below the call so
                        # callees that read uninitialised stack see the same
                        # thing on both sides
                        n = int(os.environ['T3LIVE_SCRUB'], 0)
                        uc_.mem_write(esp - n, bytes(n))
            hcall = [uc.hook_add(UC_HOOK_CODE, on_body, begin=lo, end=hi - 1) for lo, hi in body]
            for lo, hi in body:
                uc.ctl_remove_cache(lo, hi)

        if trace is None:
            def on_write(uc_, access, a, sz, v, ud):
                wlog.append((a, bytes(uc_.mem_read(a, sz))))
        else:
            def on_write(uc_, access, a, sz, v, ud):
                wlog.append((a, bytes(uc_.mem_read(a, sz))))
                trace.append((uc_.reg_read(UC_X86_REG_EIP), a, sz, v & ((1 << (8 * sz)) - 1)))
        h = uc.hook_add(UC_HOOK_MEM_WRITE, on_write)
        hr = None
        if not (brbox.THUNK_LO <= ret < brbox.THUNK_HI):
            def at_ret(uc_, a, sz, ud):
                if uc_.reg_read(UC_X86_REG_ESP) > esp0:
                    box.sub_done = True
                    uc_.emu_stop()
            hr = uc.hook_add(UC_HOOK_CODE, at_ret, begin=ret, end=ret)
            # a code hook added now does not reach blocks already translated
            uc.ctl_remove_cache(ret, ret + 1)
        box.subrun = True
        box.sub_stop = (ret, esp0)
        box.sub_done = False
        box.wlog = wlog
        box.icall_log = []
        pend = len(box.pending)
        box.fault = None
        t0 = time.time()
        status, why = 'ok', ''
        try:
            uc.emu_start(start, 0xFFFFFFF0, timeout=int(budget_s * 1e6))
            if box.fault is not None:
                status, why = 'fault', str(box.fault).split('\n')[0]
            elif not box.sub_done:
                status, why = 'timeout', 'no return within %.1fs (eip %08X)' % (
                    budget_s, uc.reg_read(UC_X86_REG_EIP))
        except UcError as e:
            status, why = 'crash', '%s at %08X' % (e, uc.reg_read(UC_X86_REG_EIP))
        dt = time.time() - t0
        for hc in (hcall or []) + hprobe:
            uc.hook_del(hc)
        uc.hook_del(h)
        if hr is not None:
            uc.hook_del(hr)
        icalls = box.icall_log
        box.subrun = False
        box.sub_stop = None
        box.wlog = None
        box.icall_log = None
        box.fault = None
        del box.pending[pend:]
        r = {'status': status, 'why': why, 'time': dt, 'wlog': wlog, 'icalls': icalls,
             'calls': calls, 'probe': list(getattr(box, 'log_probe', []))}
        if status == 'ok':
            r['regs'] = {n: uc.reg_read(k) for n, k in (
                ('eax', UC_X86_REG_EAX), ('edx', UC_X86_REG_EDX), ('ebx', UC_X86_REG_EBX),
                ('esi', UC_X86_REG_ESI), ('edi', UC_X86_REG_EDI), ('ebp', UC_X86_REG_EBP),
                ('esp', UC_X86_REG_ESP))}
            r['top'] = box.fpu_top()
            r['st0'] = box.st_raw(0)
        pre = {}
        for a, old in reversed(wlog):
            for i, byte in enumerate(old):
                pre[a + i] = byte
        r['pre'] = pre
        r['final'] = {a: box.rd8(a) for a in pre} if pre else {}
        return r

    def undo(self, wlog):
        uc = self.box.uc
        for a, old in reversed(wlog):
            uc.mem_write(a, old)

    # ----------------------------------------------------------- capture --
    def capture(self, t):
        box, uc = self.box, self.box.uc
        ctx0 = uc.context_save()
        esp0 = uc.reg_read(UC_X86_REG_ESP)
        ret = box.rd32(esp0)
        regs0 = {n: uc.reg_read(k) for n, k in (('ebx', UC_X86_REG_EBX), ('esi', UC_X86_REG_ESI),
                                                 ('edi', UC_X86_REG_EDI), ('ebp', UC_X86_REG_EBP))}
        top0 = box.fpu_top()
        hs0 = box.hs.snapshot()
        pend0 = list(box.pending)
        exp = self.explain is not None and self.explain[0] == t.va and \
            len(t.captures) == self.explain[1]
        to, tr = ([], []) if exp else (None, None)
        ob = [(t.va, t.va + len(t.placed[0]))] if (exp and t.placed) else None
        self._cur_t, self._cur_side_t3 = t, False
        o = self.run_side(t.va, ret, esp0, budget_s=30.0 if not exp else 600.0, trace=to, body=ob)
        if o['status'] != 'ok':
            # the ORIGINAL cannot be contained (it never returns in budget,
            # or needs a thread switch): stop watching this function and let
            # the game run it for real
            # (a load that runs for minutes under the write hook, or a thread
            # switch): roll back, let the game run this call for real, and
            # try a later call -- three strikes and the function is dropped
            self.undo(o['wlog'])
            uc.context_restore(ctx0)
            box.hs.restore(hs0)
            box.pending[:] = pend0
            t.strikes += 1
            t.note = 'original not containable: %s %s' % (o['status'], o['why'])
            if t.strikes >= 3:
                t.done = True
                t.captures.append({'result': 'UNRUNNABLE', 'detail': t.note,
                                   'frame': box.hs.frame, 'site': ret})
            self.skip_once = t.va
            return t.va
        ctx_o = uc.context_save()
        hs_o = box.hs.snapshot()
        pend_o = list(box.pending)
        self.undo(o['wlog'])
        uc.context_restore(ctx0)
        box.hs.restore(hs0)
        box.pending[:] = pend0
        if getattr(t, 'placed', None) is not None:
            self._swap(t, 1)
        try:
            rb = None
            self._cur_side_t3 = True
            if exp and t.placed:
                rb = [(t.va, t.va + len(t.placed[1])), (0x1190D000, 0x11940000)]
            r = self.run_side(t.alt, ret, esp0, budget_s=max(5.0, 20 * o['time']), trace=tr, body=rb)
        finally:
            if getattr(t, 'placed', None) is not None:
                self._swap(t, 0)
        self.undo(r['wlog'])
        # re-establish the ORIGINAL's end state and let the game go on from it
        for a, v in o['final'].items():
            uc.mem_write(a, bytes([v]))
        uc.context_restore(ctx_o)
        box.hs.restore(hs_o)
        box.pending[:] = pend_o
        cap = self.compare(t, o, r, esp0, ret, regs0, top0)
        if cap['result'] == 'DIFF' and cap.get('mdiff') and r['status'] == 'ok' \
                and getattr(t, 'placed', None) is not None:
            # Shadow runs: which output bytes are UNDEFINED -- derived from
            # uninitialised reads of the caller's saved registers?  Rerun
            # both sides with those registers perturbed at every call the
            # body makes; bytes that move are masked, the rest must agree.
            undef = self._undefined(t, ret, esp0, ctx0, hs0, pend0, o, r, ctx_o, hs_o, pend_o)
            if undef:
                cap2 = self.compare(t, o, r, esp0, ret, regs0, top0, mask=undef)
                cap2['masked'] = len(undef)
                if cap2['result'] == 'SAME':
                    cap2['detail'] = '%d undefined byte(s) masked (uninitialised reads)' % len(undef)
                cap = cap2
        cap.pop('mdiff', None)
        if exp:
            self.explained = self.explain_writes(t, o, r, to, tr, esp0, ret)
            self.explained['calls'] = (o['calls'], r['calls'])
            self.explained['probe'] = (o['probe'], r['probe'])
        cap.update({'frame': box.hs.frame, 'site': ret, 't_orig': round(o['time'], 4),
                    't_t3': round(r['time'], 4)})
        t.captures.append(cap)
        t.sites.add(ret)
        if cap.get('live_check'):
            self.pending_live.append((t, cap))
            self.start_probe()
        if len(t.captures) >= self.per_fn:
            t.done = True
        return uc.reg_read(UC_X86_REG_EIP)

    def _undefined(self, t, ret, esp0, ctx0, hs0, pend0, o, r, ctx_o, hs_o, pend_o):
        box, uc = self.box, self.box.uc
        ob = [(t.va, t.va + len(t.placed[0]))]
        rb = [(t.va, t.va + len(t.placed[1])), (0x1190D000, 0x11940000)]
        # back to the entry state
        self.undo(o['wlog'])
        uc.context_restore(ctx0)
        box.hs.restore(hs0)
        box.pending[:] = pend0
        os_ = self.run_side(t.va, ret, esp0, budget_s=max(5.0, 20 * o['time']), body=ob, perturb=True)
        self.undo(os_['wlog'])
        uc.context_restore(ctx0)
        box.hs.restore(hs0)
        box.pending[:] = pend0
        self._swap(t, 1)
        try:
            self._cur_side_t3 = True
            rs_ = self.run_side(t.alt, ret, esp0, budget_s=max(5.0, 20 * o['time']), body=rb, perturb=True)
        finally:
            self._swap(t, 0)
        self.undo(rs_['wlog'])
        # the original's end state again
        for a, v in o['final'].items():
            uc.mem_write(a, bytes([v]))
        uc.context_restore(ctx_o)
        box.hs.restore(hs_o)
        box.pending[:] = pend_o
        undef = set()
        for x, y in ((o, os_), (r, rs_)):
            if y['status'] != 'ok':
                return set()          # a perturbed run that fails proves nothing
            pre = dict(y['pre'])
            pre.update(x['pre'])
            for a in pre:
                if x['final'].get(a, pre[a]) != y['final'].get(a, pre[a]):
                    undef.add(a)
        return undef

    # ----------------------------------------------------------- compare --
    def compare(self, t, o, r, esp0, ret, regs0, top0, mask=None):
        if r['status'] != 'ok':
            return {'result': 'DIFF', 'detail': 'T3 side %s: %s' % (r['status'], r['why'])}
        diffs = []
        # calling convention: esp after return
        if o['regs']['esp'] != r['regs']['esp']:
            diffs.append('esp after return %+d vs %+d' % (o['regs']['esp'] - esp0,
                                                          r['regs']['esp'] - esp0))
        for k in ('ebx', 'esi', 'edi', 'ebp'):
            if r['regs'][k] != o['regs'][k]:
                diffs.append('callee-saved %s %08X vs %08X (entry %08X)' % (
                    k, o['regs'][k], r['regs'][k], regs0[k]))
        if o['top'] != r['top']:
            diffs.append('x87 depth after return differs (TOP %d vs %d)' % (o['top'], r['top']))
        elif o['top'] != top0 and o['st0'] != r['st0']:
            diffs.append('st(0) %s vs %s' % (_f80(o['st0']), _f80(r['st0'])))
        # memory: everything outside the callee's frame and argument slots
        popped = (o['regs']['esp'] - esp0 - 4) & 0xFFFFFFFF
        argb = max(popped if popped < 0x1000 else 0, self._caller_cleanup(ret),
                   4 * (t.nargs or 0))
        lo_ex = brbox.STACK_LO if brbox.STACK_LO <= esp0 < brbox.STACK_HI else (esp0 & ~0xFFFFF)
        hi_ex = esp0 + 4 + argb
        pre = dict(r['pre'])
        pre.update(o['pre'])
        mdiff = []
        for a in sorted(pre):
            if lo_ex <= a < hi_ex or (mask and a in mask):
                continue
            vo = o['final'].get(a, pre[a])
            vr = r['final'].get(a, pre[a])
            if vo != vr:
                mdiff.append((a, vo, vr))
        if mdiff and os.environ.get('T3LIVE_MDIFF'):
            runs = []
            for a, vo, vr in mdiff:
                if runs and a - runs[-1][1] <= 8:
                    runs[-1][1] = a
                else:
                    runs.append([a, a])
            print('  mdiff runs: ' + ' '.join('%08X..%08X' % (x, y) for x, y in runs[:60]))
        if mdiff:
            a, vo, vr = mdiff[0]
            diffs.append('memory: %d byte(s) differ, first %08X orig %02X t3 %02X%s' % (
                len(mdiff), a, vo, vr, _span(mdiff)))
        # import calls
        io_ = [_norm_call(c, lo_ex, esp0) for c in o['icalls'] if not _intrinsic(c[0])]
        ir_ = [_norm_call(c, lo_ex, esp0) for c in r['icalls'] if not _intrinsic(c[0])]
        if io_ != ir_:
            k = next((i for i in range(min(len(io_), len(ir_))) if io_[i] != ir_[i]),
                     min(len(io_), len(ir_)))
            diffs.append('import call #%d: %s vs %s (%d vs %d calls)' % (
                k, _fmt_call(io_[k]) if k < len(io_) else '(none)',
                _fmt_call(ir_[k]) if k < len(ir_) else '(none)', len(io_), len(ir_)))
        cap = {'result': 'DIFF' if diffs else 'SAME', 'detail': '; '.join(diffs),
               'mdiff': bool(mdiff)}
        # eax / edx: only if the caller reads them (decided by the live probe)
        regdiff = [k for k in ('eax', 'edx') if o['regs'][k] != r['regs'][k]]
        if regdiff:
            cap['live_check'] = {k: (o['regs'][k], r['regs'][k]) for k in regdiff}
        return cap

    explain = None
    explained = None
    skip_once = None
    explain_frame = None

    def explain_writes(self, t, o, r, to, tr, esp0, ret):
        """For each observable address whose final byte differs, the last
        instruction on each side that wrote it."""
        popped = (o['regs']['esp'] - esp0 - 4) & 0xFFFFFFFF
        argb = max(popped if popped < 0x1000 else 0, self._caller_cleanup(ret), 4 * (t.nargs or 0))
        lo_ex = brbox.STACK_LO
        hi_ex = esp0 + 4 + argb
        pre = dict(r['pre'])
        pre.update(o['pre'])

        def last_writer(trace):
            lw = {}
            for eip, a, sz, v in trace:
                for k in range(sz):
                    lw[a + k] = (eip, a, sz, v)
            return lw
        lwo, lwr = last_writer(to), last_writer(tr)
        out = []
        seen = set()
        for a in sorted(pre):
            if lo_ex <= a < hi_ex:
                continue
            vo = o['final'].get(a, pre[a])
            vr = r['final'].get(a, pre[a])
            if vo == vr:
                continue
            key = (lwo.get(a, (None, a))[1], lwr.get(a, (None, a))[1])
            if key in seen:
                continue
            seen.add(key)
            out.append({'addr': a, 'orig': lwo.get(a), 't3': lwr.get(a)})
        # first divergence in WRITE ORDER over observable addresses
        fo = [(a, sz, v) for eip, a, sz, v in to if not (lo_ex <= a < hi_ex)]
        fr = [(a, sz, v) for eip, a, sz, v in tr if not (lo_ex <= a < hi_ex)]
        k = next((i for i in range(min(len(fo), len(fr))) if fo[i] != fr[i]), None)
        watch = [int(x, 16) for x in os.environ.get('T3LIVE_WATCH', '').split(',') if x]
        wo = [(i, e) for i, e in enumerate(to) if any(e[1] <= w < e[1] + e[2] for w in watch)]
        wr = [(i, e) for i, e in enumerate(tr) if any(e[1] <= w < e[1] + e[2] for w in watch)]
        return {'watch': (wo, wr), 'final': out, 'order_first': k, 'n_orig': len(fo), 'n_t3': len(fr),
                'order_orig': [e for e in to if not (lo_ex <= e[1] < hi_ex)][k:k + 6] if k is not None else [],
                'order_t3': [e for e in tr if not (lo_ex <= e[1] < hi_ex)][k:k + 6] if k is not None else []}

    def _caller_cleanup(self, ret):
        try:
            code = self.box.rd(ret, 8)
        except UcError:
            return 0
        if code[:2] == b'\x83\xC4':
            return code[2]
        if code[:2] == b'\x81\xC4':
            return struct.unpack_from('<I', code, 2)[0]
        return 0

    # ---------------------------------------------- dynamic eax/edx liveness --
    # Tracked per BYTE LANE (al, ah, and the upper word as two lanes): the
    # caller `mov ax,[m]; test eax,0x4A4` never observes the upper half of the
    # returned eax -- the partial write replaced the low half and the mask
    # drops the rest -- so only differing lanes that are READ count.
    def start_probe(self):
        if self.probe is not None:
            return
        box = self.box
        caps = self.pending_live
        lanes = {'eax': set(), 'edx': set()}
        for _t, cap in caps:
            for k, (vo, vr) in cap['live_check'].items():
                x = vo ^ vr
                lanes[k] |= {i for i in range(4) if (x >> (8 * i)) & 0xFF}
        state = {'n': 0, 'eax': None if lanes['eax'] else 'dead',
                 'edx': None if lanes['edx'] else 'dead', 'lanes': lanes}
        self.probe = state
        cache = {}

        def step(uc, addr, size, _ud):
            if box.subrun:
                return
            state['n'] += 1
            if brbox.THUNK_LO <= addr < brbox.THUNK_HI:
                for k in ('eax', 'edx'):     # an import returns in eax/edx
                    if state[k] is None:
                        state[k] = 'dead'
            else:
                ins = cache.get(addr)
                if ins is None:
                    try:
                        ins = next(_md.disasm(box.rd(addr, 16), addr))
                    except (StopIteration, UcError):
                        ins = False
                    cache[addr] = ins
                if ins:
                    for k in ('eax', 'edx'):
                        if state[k] is None:
                            state[k] = _lane_step(ins, k, state['lanes'][k])
            if (state['eax'] is not None and state['edx'] is not None) or state['n'] > 2000:
                box.intermission = lambda b: self.end_probe()
                uc.emu_stop()
        state['hook'] = box.uc.hook_add(UC_HOOK_CODE, step, begin=1, end=0)
        box.uc.ctl_flush_tb()

    def end_probe(self):
        st = self.probe
        self.box.uc.hook_del(st['hook'])
        self.probe = None
        # every capture waiting on this probe was taken at the SAME return
        # (the probe starts at once), so they share the verdict
        for t, cap in self.pending_live:
            chk = cap.pop('live_check')
            live = [k for k in chk if st[k] in ('live', None)]
            if live:
                txt = ', '.join('%s %08X vs %08X (%s)' % (
                    k, chk[k][0], chk[k][1],
                    'caller reads it' if st[k] == 'live' else 'not proven dead') for k in live)
                cap['result'] = 'DIFF'
                cap['detail'] = '; '.join(x for x in (cap['detail'], txt) if x)
            cap['liveness'] = {k: st[k] or 'unresolved' for k in ('eax', 'edx')}
        self.pending_live = []
        return None

    # ------------------------------------------------------------ report --
    def results(self):
        out = {}
        for va, t in self.t.items():
            caps = t.captures
            if any(c['result'] == 'DIFF' for c in caps):
                v = 'DIVERGENT'
                c = next(c for c in caps if c['result'] == 'DIFF')
                detail = 'frame %d site %08X: %s' % (c['frame'], c['site'], c['detail'])
            elif any(c['result'] == 'SAME' for c in caps):
                v = 'EQUIVALENT'
                detail = '%d real call(s) agree' % sum(1 for c in caps if c['result'] == 'SAME')
            elif caps:
                v = 'UNRUNNABLE'
                detail = caps[0]['detail']
            elif t.alt is None:
                v = 'UNRUNNABLE'
                detail = t.note
            else:
                v = 'UNCOVERED'
                detail = 'never called by this script'
            if t.note and v not in ('UNRUNNABLE',):
                detail += ' [%s]' % t.note
            out['0x%08x' % va] = {'name': t.name, 'verdict': v, 'calls': t.calls,
                                  'captured': len(caps), 'contexts': len(t.sites),
                                  'detail': detail, 'captures': caps, 'nested': t.nested,
                                  'done': t.done and len(caps) < self.per_fn}
        return out


_LANES = {}
for _fam, _k in ((('eax', 'ax', 'al', 'ah'), 'eax'), (('edx', 'dx', 'dl', 'dh'), 'edx')):
    _LANES[getattr(X, 'X86_REG_' + _fam[0].upper())] = (_k, {0, 1, 2, 3})
    _LANES[getattr(X, 'X86_REG_' + _fam[1].upper())] = (_k, {0, 1})
    _LANES[getattr(X, 'X86_REG_' + _fam[2].upper())] = (_k, {0})
    _LANES[getattr(X, 'X86_REG_' + _fam[3].upper())] = (_k, {1})


def _lane_step(ins, k, pending):
    """Advance one register's lane liveness over one instruction: 'live' if
    a pending (differing) lane is read, 'dead' once every pending lane has
    been overwritten, None to keep going.  `pending` is mutated."""
    rd, wr = ins.regs_access()
    ops = ins.operands
    zeroing = ins.mnemonic in ('xor', 'sub') and len(ops) == 2 and \
        ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG and ops[0].reg == ops[1].reg
    read = set()
    if not zeroing:
        for r in rd:
            ln = _LANES.get(r)
            if ln and ln[0] == k:
                read |= ln[1]
        # test/and REG, imm reads only the lanes the immediate keeps
        if ins.mnemonic in ('test', 'and') and len(ops) == 2 and ops[1].type == X.X86_OP_IMM \
                and ops[0].type == X.X86_OP_REG and _LANES.get(ops[0].reg, ('', ))[0] == k:
            imm = ops[1].imm & 0xFFFFFFFF
            read &= {i for i in range(4) if (imm >> (8 * i)) & 0xFF}
    if read & pending:
        return 'live'
    if ins.mnemonic in ('test', 'cmp', 'bt', 'push'):
        wr = ()                      # capstone lists `test eax,imm`'s eax as written
    for r in wr:
        ln = _LANES.get(r)
        if ln and ln[0] == k:
            pending -= ln[1]
    return 'dead' if not pending else None


def _f80(v):
    mant, exp = v
    return '%04X:%016X(%g)' % (exp, mant, brbox._f80_to_float(mant, exp))


def _span(md):
    first = md[0][0]
    last = md[-1][0]
    return ' (span %08X..%08X)' % (first, last) if last != first else ''


# CRT functions MSVC 5.0 may expand inline (/Oi and #pragma intrinsic).
# Whether a body calls one through the import table or inlines it is
# codegen, not behaviour: its whole effect is memory (compared byte for
# byte above) and a return value (compared via the registers / the memory
# it flows into).  Everything else -- file, window, COM, heap, clock -- stays
# in the call-sequence comparison.
INTRINSICS = frozenset('''memcpy memset memcmp strcpy strcat strcmp strlen
    abs labs fabs sin cos tan atan atan2 asin acos exp log log10 pow sqrt
    fmod _rotl _rotr _lrotl _lrotr _strset _inp _outp _inpw _outpw'''.split())


def _intrinsic(name):
    return name.split('!')[-1] in INTRINSICS


def _norm_call(c, lo_ex, esp0):
    name, args, ret = c
    na = tuple(('frame' if lo_ex <= a < esp0 else a) for a in (args or ()))
    return (name, na)


def _fmt_call(c):
    name, args = c
    return '%s(%s)' % (name.split('!')[-1], ', '.join(
        x if isinstance(x, str) else '%X' % x for x in args))


PERTURB_REGS = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


# ================================================================ runs =====

DEFAULT_IMAGE = os.path.join(ROOT, 'build', 'brbox', 'image', 'BRGlide.T3.dll')


def _one_pass(script, only, per_fn, shots, log, image):
    import brbox_drive
    box = brbox_drive.make_box(log=lambda m: None)
    drv = brbox_drive.Driver(brbox_drive.parse_script(script), shots=shots, log=lambda m: None)
    brbox_drive.attach(box, drv)
    targets = t3_targets(only)
    orc = Oracle(box, targets, per_fn=per_fn, log=log, image=image)
    end = 'ok'
    try:
        box.boot()
        box.rally_main()
        end = 'RallyMain returned'
    except Stop as e:
        end = str(e)
    except GuestFault as e:
        end = 'FAULT ' + str(e)
    return orc.results(), end, box.hs.frame, drv.marks, orc


def _merge(a, b):
    """Fold pass `b`'s result for one function into pass `a`'s."""
    if a is None:
        return b
    caps = a['captures'] + b['captures']
    m = dict(a)
    m['captures'] = caps
    m['captured'] = len(caps)
    m['calls'] = max(a['calls'], b['calls'])
    m['contexts'] = a['contexts'] + b['contexts']
    rank = {'DIVERGENT': 4, 'EQUIVALENT': 3, 'UNRUNNABLE': 2, 'UNCOVERED': 1}
    if rank[b['verdict']] > rank[a['verdict']]:
        m['verdict'], m['detail'] = b['verdict'], b['detail']
    if m['verdict'] == 'EQUIVALENT':
        m['detail'] = '%d real call(s) agree' % sum(1 for c in caps if c['result'] == 'SAME')
    return m


def run_script(script, only=None, per_fn=6, shots=None, log=print, image=DEFAULT_IMAGE,
               max_passes=3):
    """Run a script under the oracle.  A function called only from INSIDE
    another captured function is never captured itself (its calls happen in a
    sub-run); a second pass hooks only those functions, so their callers run
    for real and the calls land.  The drive is identical in every pass."""
    t0 = time.time()
    if image and not os.path.exists(image):
        log('no image at %s -- object mode for every target' % image)
        image = None
    res, end, frames, marks = {}, None, 0, []
    want = only
    for p in range(1, max_passes + 1):
        r, e, fr, mk, orc = _one_pass(script, want, per_fn, shots if p == 1 else None, log, image)
        if p == 1:
            end, frames, marks = e, fr, mk
            log('%s: %d targets (%d not runnable: %s)' % (
                os.path.basename(script), len(orc.t),
                sum(1 for t in orc.t.values() if t.alt is None),
                '; '.join(sorted(set(t.note.split(':')[0] for t in orc.t.values()
                                     if t.alt is None)))))
        for va, v in r.items():
            res[va] = _merge(res.get(va), v) if p > 1 else v
        nxt = {va for va, v in r.items()
               if v['nested'] > 0 and res[va]['captured'] == 0 and not v['done']}
        c = collections.Counter(v['verdict'] for v in res.values())
        log('%s pass %d: %s after %d frames, %.0fs wall -- %s; %d reached only nested' % (
            os.path.basename(script), p, e.split('\n')[0], fr, time.time() - t0,
            ', '.join('%s %d' % kv for kv in sorted(c.items())), len(nxt)))
        if e.startswith('FAULT'):
            log(e)
        if not nxt or nxt == want:
            break
        want = nxt
    os.makedirs(OUT_DIR, exist_ok=True)
    # a targeted (--only) run is evidence about those functions only: it
    # must never replace the suite's record for the script
    sub = OUT_DIR if not only else os.path.join(OUT_DIR, 'only')
    os.makedirs(sub, exist_ok=True)
    out = os.path.join(sub, os.path.splitext(os.path.basename(script))[0] + '.json')
    json.dump({'script': script, 'end': end, 'frames': frames,
               'date': datetime.datetime.now().isoformat(timespec='seconds'),
               'marks': marks, 'results': res}, open(out, 'w'), indent=1, default=str)
    return res, end


def aggregate(write=True):
    """Fold every script's results into the ledger: DIVERGENT anywhere wins,
    else EQUIVALENT if any script captured agreeing calls, else UNRUNNABLE /
    UNCOVERED."""
    import t3ledger
    rank = {'DIVERGENT': 4, 'EQUIVALENT': 3, 'UNRUNNABLE': 2, 'UNCOVERED': 1}
    agg = {}
    for fn in sorted(os.listdir(OUT_DIR)) if os.path.isdir(OUT_DIR) else []:
        if not fn.endswith('.json'):
            continue
        d = json.load(open(os.path.join(OUT_DIR, fn)))
        for va, r in d['results'].items():
            a = agg.setdefault(va, {'name': r['name'], 'verdict': 'UNCOVERED', 'calls': 0,
                                    'captured': 0, 'contexts': 0, 'detail': '', 'scripts': []})
            a['calls'] += r['calls']
            a['captured'] += r['captured']
            a['contexts'] += r['contexts']
            if r['captured'] or r['calls']:
                a['scripts'].append(os.path.splitext(fn)[0])
            if rank[r['verdict']] > rank[a['verdict']]:
                a['verdict'] = r['verdict']
                a['detail'] = '%s: %s' % (os.path.splitext(fn)[0], r['detail'])
    if write:
        rows = t3ledger.load()
        today = datetime.date.today().isoformat()
        for va, a in agg.items():
            rows[va] = {'va': va, 'name': a['name'] or rows.get(va, {}).get('name', ''),
                        'verdict': a['verdict'], 'calls': a['calls'], 'captured': a['captured'],
                        'contexts': a['contexts'], 'detail': a['detail'][:400], 'date': today}
        t3ledger.save(rows)
    return agg


def _dis(box, eip, n=1, orig=False):
    try:
        code = box.pe.read(eip, 16 * n) if orig else box.rd(eip, 16 * n)
    except UcError:
        return '?'
    if code is None:
        return '?'
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    return '; '.join('%08X %s %s' % (i.address, i.mnemonic, i.op_str)
                     for i in list(md.disasm(code, eip))[:n])


def explain_cli(a):
    import brbox_drive
    va = int(a.va, 16)
    cap_idx = a.capture
    if a.frame is not None:
        cap_idx = 0
    elif cap_idx is None:
        name = os.path.splitext(os.path.basename(a.script))[0] + '.json'
        d = json.load(open(os.path.join(OUT_DIR, name)))
        caps = d['results']['0x%08x' % va]['captures']
        cap_idx = next(i for i, c in enumerate(caps) if c['result'] == 'DIFF')
    box = brbox_drive.make_box(log=lambda m: None)
    drv = brbox_drive.Driver(brbox_drive.parse_script(a.script), log=lambda m: None)
    brbox_drive.attach(box, drv)
    targets = t3_targets({'0x%08x' % va})
    orc = Oracle(box, targets, per_fn=cap_idx + 1, log=print, image=a.image)
    orc.explain = (va, cap_idx)
    orc.explain_frame = a.frame
    try:
        box.boot()
        box.rally_main()
    except (Stop, GuestFault) as e:
        pass
    t = orc.t[va]
    if cap_idx >= len(t.captures):
        print('capture %d was not reached' % cap_idx)
        return 1
    c = t.captures[cap_idx]
    print('%s 0x%08X capture %d: frame %d site %08X: %s: %s' % (
        t.name, va, cap_idx, c['frame'], c['site'], c['result'], c['detail']))
    ex = orc.explained
    if not ex:
        return 0
    # show T3-side code from the IMAGE bytes
    if t.placed is not None:
        orc._swap(t, 1)
    for row in ex['final'][:24]:
        o, r = row['orig'], row['t3']
        def fmt(w, orig):
            if w is None:
                return 'not written'
            eip, wa, sz, v = w
            fv = ''
            if sz == 4:
                fv = ' (%g)' % struct.unpack('<f', struct.pack('<I', v))[0]
            elif sz == 8:
                fv = ' (%g)' % struct.unpack('<d', struct.pack('<Q', v))[0]
            return '%d@%08X=%X%s by %s' % (sz, wa, v, fv, _dis(box, eip, orig=orig))
        print('  %08X  orig: %s' % (row['addr'], fmt(o, True)))
        print('            t3:   %s' % fmt(r, False))
    for side, pl in zip(('orig', 't3'), ex.get('probe', ([], []))):
        for a_, st_, rg in pl[:80]:
            print('  probe %-4s %08X st=%s %s' % (side, a_, st_, ' '.join('%s=%08X' % kv for kv in rg.items())))
    co, cr = ex.get('calls', ([], []))
    for side, lst, cl in zip(('orig', 't3'), ex.get('watch', ([], [])), (co, cr)):
        for i, (eip, wa, sz, v) in lst[:20]:
            ci = max((j for j, c in enumerate(cl) if c[3] <= i), default=None)
            where = ('inside body call #%d (%08X->%s)' % (ci, cl[ci][0], cl[ci][1] if isinstance(cl[ci][1], str)
                                                         else '%08X' % cl[ci][1])) if ci is not None else ''
            print('  watch %-4s write#%-8d %08X <- %X  %s  %s' % (side, i, wa, v,
                                                              _dis(box, eip, orig=(side == 'orig')), where))
    k = next((i for i in range(min(len(co), len(cr))) if co[i][1] != cr[i][1]), None)
    print('body calls: %d vs %d; first differing target at #%s' % (len(co), len(cr), k))
    if os.environ.get('T3LIVE_CALLS'):
        for side, cl in (('orig', co), ('t3', cr)):
            print('  --- %s calls' % side)
            for i, c in enumerate(cl):
                tg = c[1] if isinstance(c[1], str) else '%08X' % c[1]
                parts = []
                for j in range(3):
                    b = c[4][j]
                    if os.environ.get('T3LIVE_CALLS') == 'raw':
                        parts.append('%X' % c[2][j] + ('' if b is None else ' {%s}' % b[:16].hex()))
                        if j == 2:
                            parts.append(' '.join('%X' % w for w in c[2][3:]))
                        continue
                    if b is None or not (brbox.STACK_LO <= c[2][j] < brbox.STACK_HI or c[2][j] >= 0x10000000):
                        parts.append('%X' % c[2][j])
                    else:
                        parts.append('[%s]' % ' '.join('%.9g' % x for x in struct.unpack('<9f', b)))
                print('  #%d %s %s' % (i, tg, ' | '.join(parts)))
    if os.environ.get('T3LIVE_ARGDATA'):
        for i in range(min(len(co), len(cr))):
            bo, br = co[i][4], cr[i][4]
            for j in range(3):
                if bo[j] is not None and br[j] is not None and bo[j] != br[j] and \
                        (brbox.STACK_LO <= co[i][2][j] < brbox.STACK_HI):
                    fo = struct.unpack('<9f', bo[j]); fr = struct.unpack('<9f', br[j])
                    print('  argdata call #%d ->%08X arg%d differs:' % (i, co[i][1] if isinstance(co[i][1], int) else 0, j))
                    print('     orig %s' % ' '.join('%.9g' % x for x in fo))
                    print('     t3   %s' % ' '.join('%.9g' % x for x in fr))
    stk = lambda v: 'stk' if brbox.STACK_LO <= v < brbox.STACK_HI else '%X' % v
    md2 = Cs(CS_ARCH_X86, CS_MODE_32)

    def arity(site, tgt, orig_side):
        # the caller's `add esp,N` right after the call, else the callee's `ret N`
        try:
            code = box.pe.read(site, 24) if orig_side else box.rd(site, 24)
            ins = list(md2.disasm(code, site))[:2]
            if len(ins) == 2 and ins[1].mnemonic == 'add' and ins[1].op_str.startswith('esp, '):
                return int(ins[1].op_str.split(', ')[1], 0) // 4
        except Exception:
            pass
        if isinstance(tgt, int):
            for i in md2.disasm(box.pe.read(tgt, 0x800) or b'', tgt):
                if i.mnemonic == 'ret':
                    return int(i.op_str, 0) // 4 if i.op_str else 0
        return 0
    na = 0
    for i in range(min(len(co), len(cr))):
        n = max(arity(co[i][0], co[i][1], True), arity(cr[i][0], cr[i][1], False))
        n = min(n, 3)
        ao = [stk(x) for x in co[i][2][1:1 + n]]
        ar = [stk(x) for x in cr[i][2][1:1 + n]]
        if ao != ar and na < 12:
            na += 1
            tg = co[i][1] if isinstance(co[i][1], str) else '%08X' % co[i][1]
            print('  args #%d ->%s  orig %s  t3 %s' % (i, tg, ao, ar))
    if k is not None:
        for i in range(max(0, k - 3), min(k + 6, max(len(co), len(cr)))):
            fo = ('%08X->%s' % (co[i][0], co[i][1] if isinstance(co[i][1], str) else '%08X' % co[i][1])) if i < len(co) else '-'
            fr = ('%08X->%s' % (cr[i][0], cr[i][1] if isinstance(cr[i][1], str) else '%08X' % cr[i][1])) if i < len(cr) else '-'
            print('  #%d orig %-26s t3 %s' % (i, fo, fr))
    print('write order: %d vs %d observable writes; first differing write #%s' % (
        ex['n_orig'], ex['n_t3'], ex['order_first']))
    for side, lst in (('orig', ex['order_orig']), ('t3', ex['order_t3'])):
        for eip, wa, sz, v in lst:
            print('  %-4s %08X <- %X (%d B)  %s' % (side, wa, v, sz,
                                                   _dis(box, eip, orig=(side == 'orig'))))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd')
    r = sub.add_parser('run')
    r.add_argument('scripts', nargs='+')
    r.add_argument('--only', nargs='*', help='limit to these VAs')
    r.add_argument('--per-fn', type=int, default=6, help='captures per function')
    r.add_argument('--shots')
    r.add_argument('--no-ledger', action='store_true')
    r.add_argument('--image', default=DEFAULT_IMAGE,
                   help='built T3 image whose placed spans are the T3 side '
                        '(tools/image_build_t3.py --out-dir build/brbox/image, BR_TRACE unset)')
    r.add_argument('--selftest', action='store_true',
                   help='negative control: plant an inverted branch in every T3 body; '
                        'every reached function must come out DIVERGENT')
    r.add_argument('--objects', action='store_true',
                   help='object mode for every target: the sweep objects, relocated')
    e = sub.add_parser('explain', help='localise one capture\'s divergence to instructions')
    e.add_argument('script')
    e.add_argument('va')
    e.add_argument('--capture', type=int, default=None,
                   help='capture index (default: the first DIFF in the last run)')
    e.add_argument('--frame', type=int, default=None,
                   help='explain the first call at or after this game frame')
    e.add_argument('--image', default=DEFAULT_IMAGE)
    sub.add_parser('report')
    a = ap.parse_args()
    if a.cmd == 'run':
        only = set(v.lower() for v in a.only) if a.only else None
        if a.selftest:
            Oracle.mutate = True
            a.no_ledger = True
            global OUT_DIR
            OUT_DIR = os.path.join(ROOT, 'build', 'brbox', 'live_selftest')
        for s in a.scripts:
            run_script(s, only=only, per_fn=a.per_fn, shots=a.shots,
                       image=None if a.objects else a.image)
        agg = aggregate(write=not a.no_ledger and not only)
        c = collections.Counter(v['verdict'] for v in agg.values())
        print('aggregate over %s: %s' % (OUT_DIR, ', '.join('%s %d' % kv for kv in sorted(c.items()))))
        for va, v in sorted(agg.items()):
            if v['verdict'] == 'DIVERGENT':
                print('  DIVERGENT %s %-30s %s' % (va, v['name'], v['detail'][:220]))
        return 0
    if a.cmd == 'explain':
        return explain_cli(a)
    if a.cmd == 'report':
        agg = aggregate(write=False)
        for va, v in sorted(agg.items(), key=lambda kv: kv[1]['verdict']):
            print('%-10s %s %-30s calls %-6d %s' % (v['verdict'], va, v['name'], v['calls'],
                                                     v['detail'][:160]))
        return 0
    ap.print_help()
    return 2


if __name__ == '__main__':
    sys.exit(main())
