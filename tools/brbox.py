#!/usr/bin/env python3
"""brbox.py -- BRGlide.dll in a box: the ORIGINAL game, run headless.

WHY THIS EXISTS.  The synthetic-seed oracle (the retired t3b_verify.py A5 path)
fed each function random bytes and called whatever came out "equivalent".
BrRaceStep carried that verdict while calling BrCarSlotSetup with its two
arguments swapped -- the seeds never reached the car-setup loop, so the
certificate certified nothing there.  The only inputs that mean anything are
the ones the game really produces.  This host produces them: it loads the
byte-exact reference DLL at its image base and runs RallyMain, so every
function sees the state the real game builds for it.

WHAT IS REAL AND WHAT IS NOT.  All of BRGlide.dll's own code and data is real,
executed by Unicorn (QEMU's x86 core, 80-bit x87 via softfloat).  The only
modelled surface is the import edge -- the 190 IAT slots plus whatever the game
reaches through LoadLibrary/GetProcAddress and COM vtables.  Each import is one
of three kinds (tools/brbox_imports.py): MODEL (behaviour the game depends on:
heap, files served from the retail disc, a virtual clock, the message queue,
scripted input), STUB (pure output -- Glide, GDI, sound: recorded, answered
with success) or UNMODELLED (raises, naming the import, so nothing is ever
silently guessed).

DETERMINISM.  One thread, one virtual clock, one scripted input timeline.  The
same script gives the same run, instruction for instruction, every time.

    .venv/bin/python tools/brbox.py run --script tools/brbox_scripts/boot.txt
    .venv/bin/python tools/brbox.py run --frames 300 --shots build/brbox/shots
"""
from __future__ import print_function

import argparse
import collections
import copy
import os
import struct
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from unicorn import (Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE,  # noqa: E402
                     UC_HOOK_MEM_WRITE, UC_HOOK_MEM_UNMAPPED, UC_HOOK_INTR,
                     UC_PROT_ALL)
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX,  # noqa: E402
                               UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI,
                               UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
                               UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
                               UC_X86_REG_FS, UC_X86_REG_GDTR, UC_X86_REG_SS,
                               UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_FPSW,
                               UC_X86_REG_FPCW, UC_X86_REG_FP0)
import pe as PE  # noqa: E402

REF_DLL = os.environ.get('BR_REF_DLL', os.path.join(ROOT, 'orig', 'BRGlide.dll'))
CD_ROOT = os.path.join(ROOT, 'build', 'brbox', 'cd')

# ---------------------------------------------------------------- memory map --
# Nothing below 0x00400000 is mapped, so a NULL (or small-offset) dereference
# faults exactly as it does on Windows instead of reading zeros.
STACK_LO, STACK_HI = 0x00800000, 0x00C00000     # main thread, 4 MB
HEAP_LO, HEAP_HI = 0x02000000, 0x0E000000       # malloc/new/GlobalAlloc, 192 MB
HOST_LO, HOST_HI = 0x7E000000, 0x7F000000       # host-owned data: TEB, strings, COM objects
THUNK_LO, THUNK_HI = 0x7F000000, 0x7F100000     # one 16-byte trap per import / COM method
GDT_VA = 0x7F200000
TSTACK_LO, TSTACK_SIZE = 0x00C00000, 0x00100000   # up to seven more threads, 1 MB each
EXE_BASE = 0x00400000                           # the launcher's HINSTANCE (never mapped)

REGS = (('eax', UC_X86_REG_EAX), ('ecx', UC_X86_REG_ECX), ('edx', UC_X86_REG_EDX),
        ('ebx', UC_X86_REG_EBX), ('esp', UC_X86_REG_ESP), ('ebp', UC_X86_REG_EBP),
        ('esi', UC_X86_REG_ESI), ('edi', UC_X86_REG_EDI))


class GuestFault(Exception):
    """The game did something the box cannot continue from (a fault, an
    unmodelled import, exit()).  Carries a register dump for the report."""


class Stop(Exception):
    pass


def _gdt_entry(base, limit, access, flags):
    return struct.pack('<Q', (limit & 0xFFFF) | ((base & 0xFFFFFF) << 16) |
                       (access << 40) | (((limit >> 16) & 0xF) << 48) |
                       (flags << 52) | (((base >> 24) & 0xFF) << 56))


class Args(object):
    """Stack arguments of an import call, read at the trap (esp -> return
    address, so argument i is at esp+4+4i)."""
    __slots__ = ('box', 'esp')

    def __init__(self, box, esp):
        self.box, self.esp = box, esp

    def __getitem__(self, i):
        return self.box.rd32(self.esp + 4 + 4 * i)

    def s(self, i):
        return self.box.rd32(self.esp + 4 + 4 * i) - (1 << 32) if self[i] & 0x80000000 else self[i]

    def f64(self, i):
        """A double passed by value starting at argument slot i."""
        return struct.unpack('<d', bytes(self.box.uc.mem_read(self.esp + 4 + 4 * i, 8)))[0]


BLOCK = object()      # a model's answer: the calling thread must wait


class Thread(object):
    __slots__ = ('tid', 'handle', 'ctx', 'state', 'wait', 'pending', 'fs_sel', 'code')

    def __init__(self, tid, handle, fs_sel):
        self.tid, self.handle, self.fs_sel = tid, handle, fs_sel
        self.ctx = None
        self.state = 'ready'
        self.wait = None
        self.pending = []
        self.code = None


class HostState(object):
    """Every piece of host-side mutable state, in one deep-copyable object.

    The live T3 oracle runs a function twice from one captured state; the
    guest half of that state is restored from Unicorn, and THIS is the host
    half -- heap bookkeeping, open files, the clock, the message queue, the
    CRT's rand() seed.  Anything an import model mutates lives here, or the
    two runs would not start from identical worlds."""

    def __init__(self):
        self.heap_next = HEAP_LO
        self.heap_blocks = {}          # addr -> size (live)
        self.heap_free = []            # [(addr, size)] reusable blocks, address order
        self.host_next = HOST_LO + 0x10000
        self.ms = 0.0                  # virtual milliseconds since boot
        self.qpc_step = 0              # QueryPerformanceCounter calls (monotonic nudge)
        self.msgq = collections.deque()
        self.timers = {}               # (hwnd, id) -> [interval_ms, next_due_ms]
        self.files = {}                # FILE* guest addr -> dict(path, pos, mode, wr)
        self.findh = {}                # _findfirst handle -> [remaining names]
        self.next_handle = 0x1000
        self.rand = 1                  # MSVCRT per-thread seed (srand default 1)
        self.windows = {}              # hwnd -> dict(wndproc, cls, ...)
        self.classes = {}              # class name -> wndproc
        self.focus = 0
        self.quit = False
        self.keys = set()              # VK codes currently down (scripted)
        self.dikeys = set()            # DIK scan codes currently down
        self.mouse = [0, 0]            # pending relative mouse motion (DirectInput mickeys)
        self.mouse_btn = set()         # mouse buttons currently down
        self.frame = 0
        self.errno = 0
        self.cwd = 'C:\\BOSSRALLY\\'
        self.mutexes = {}
        self.events = {}
        self.written = {}              # virtual FS writes: canonical path -> bytearray
        self.glide = collections.Counter()
        self.loaded = {}               # lowercase dll name -> HMODULE (LoadLibrary'd)
        self.regkeys = {}              # HKEY -> key path
        self.winmsgs = {}              # RegisterWindowMessage name -> id
        self.gdi = {}                  # HBITMAP -> DIB section
        self.mmio = {}                 # HMMIO -> parse state
        self.atexit = []
        self.ear_calls = collections.Counter()
        self.messageboxes = []
        self.com = {}                  # COM object -> state

    def snapshot(self):
        return copy.deepcopy(self.__dict__)

    def restore(self, snap):
        self.__dict__.update(copy.deepcopy(snap))


class Box(object):
    """The machine: Unicorn + the reference image + the import edge."""

    def __init__(self, dll=REF_DLL, log=None, trace_imports=False):
        import brbox_imports
        self.log = log or (lambda *a: None)
        self.trace_imports = trace_imports
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.hs = HostState()
        self.pe = PE.load(dll)
        self.thunks = {}               # trap VA -> (name, pop_bytes, fn)
        self.thunk_names = {}          # name -> trap VA (dedupe)
        self.thunk_nargs = {}          # trap VA -> argument count for the call log
        self.thunk_next = THUNK_LO + 0x100
        self.pending = []              # callback frames: see _drive
        self.stops = []                # stop-trap requests (nested top-level calls)
        self.icall_log = None          # when a list: (name, args, ret) per import call
        self.import_counts = collections.Counter()
        self.recent = collections.deque(maxlen=64)
        self.on_frame = None           # callback(box) each BrAppFrame entry
        self.stop_reason = None
        self._map_image()
        self._map_host()
        self._init_threads()
        self.imports = brbox_imports
        brbox_imports.install(self)
        self.uc.hook_add(UC_HOOK_CODE, self._on_trap, begin=THUNK_LO, end=THUNK_HI - 1)
        self.uc.hook_add(UC_HOOK_INTR, self._on_intr)

    # ------------------------------------------------------------ mapping --
    def _map_image(self):
        p = self.pe
        lo = p.image_base
        hi = max(p.image_base + s.vaddr + max(s.vsize, s.raw_size) for s in p.sections)
        hi = (hi + 0xFFFF) & ~0xFFFF
        self.img_lo, self.img_hi = lo, hi
        self.uc.mem_map(lo, hi - lo, UC_PROT_ALL)
        self.uc.mem_write(lo, p.data[:0x1000])                    # headers (GetModuleHandle readers)
        for s in p.sections:
            raw = p.data[s.raw_ptr:s.raw_ptr + min(s.raw_size, max(s.vsize, s.raw_size))]
            self.uc.mem_write(lo + s.vaddr, raw)
        t = p.section_by_name('.text')
        self.text_lo, self.text_hi = lo + t.vaddr, lo + t.vaddr + t.vsize

    def _map_host(self):
        uc = self.uc
        uc.mem_map(STACK_LO, STACK_HI - STACK_LO, UC_PROT_ALL)
        uc.mem_map(HEAP_LO, HEAP_HI - HEAP_LO, UC_PROT_ALL)
        uc.mem_map(HOST_LO, HOST_HI - HOST_LO, UC_PROT_ALL)
        uc.mem_map(THUNK_LO, THUNK_HI - THUNK_LO, UC_PROT_ALL)
        uc.mem_write(THUNK_LO, b'\xF4' * (THUNK_HI - THUNK_LO))     # hlt: a trap nobody claimed
        uc.mem_map(GDT_VA, 0x1000, UC_PROT_ALL)
        # A TEB for fs:[0] (the SEH chain head) and fs:[0x18] (self pointer).
        self.teb = HOST_LO
        uc.mem_write(self.teb, struct.pack('<III', 0xFFFFFFFF, STACK_HI, STACK_LO))
        uc.mem_write(self.teb + 0x18, struct.pack('<I', self.teb))
        # Once a GDT is loaded every segment register is checked against it,
        # so all of them get flat descriptors: SS a ring-0 data segment (the
        # CPL Unicorn runs at), DS/ES ring-3 data, FS the TEB.
        gdt = (b'\0' * 8 + _gdt_entry(self.teb, 0xFFF, 0xF3, 0x4) +
               _gdt_entry(0, 0xFFFFF, 0x92, 0xC) + _gdt_entry(0, 0xFFFFF, 0xF2, 0xC))
        uc.mem_write(GDT_VA, gdt)
        uc.reg_write(UC_X86_REG_GDTR, (0, GDT_VA, len(gdt) - 1, 0))
        uc.reg_write(UC_X86_REG_FS, (1 << 3) | 3)
        uc.reg_write(UC_X86_REG_SS, (2 << 3) | 0)
        uc.reg_write(UC_X86_REG_DS, (3 << 3) | 3)
        uc.reg_write(UC_X86_REG_ES, (3 << 3) | 3)
        uc.reg_write(UC_X86_REG_ESP, STACK_HI - 0x100)
        uc.reg_write(UC_X86_REG_FPCW, 0x027F)       # Win32's default: 53-bit, all masked
        # the two reserved traps: return-to-host and callback continuation
        self.STOP = THUNK_LO
        self.CONT = THUNK_LO + 0x10
        self.TEXIT = THUNK_LO + 0x20
        uc.mem_map(TSTACK_LO, TSTACK_SIZE * 7, UC_PROT_ALL)

    # ------------------------------------------------------- memory access --
    def rd32(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    # Every host-side write goes through _w.  While the live oracle runs a
    # sub-run, `wlog` is a list and each write first records the bytes it
    # overwrites -- the guest's own writes are logged by a Unicorn hook, and
    # an import model's writes (fread, sprintf, GetDeviceState...) would
    # otherwise escape both the undo and the comparison.
    wlog = None

    def _w(self, a, b):
        if self.wlog is not None and b:
            self.wlog.append((a, bytes(self.uc.mem_read(a, len(b)))))
        self.uc.mem_write(a, b)

    def wr32(self, a, v):
        self._w(a, struct.pack('<I', v & 0xFFFFFFFF))

    def rd16(self, a):
        return struct.unpack('<H', bytes(self.uc.mem_read(a, 2)))[0]

    def wr16(self, a, v):
        self._w(a, struct.pack('<H', v & 0xFFFF))

    def rd8(self, a):
        return self.uc.mem_read(a, 1)[0]

    def wr8(self, a, v):
        self._w(a, bytes([v & 0xFF]))

    def rdf32(self, a):
        return struct.unpack('<f', bytes(self.uc.mem_read(a, 4)))[0]

    def rd(self, a, n):
        return bytes(self.uc.mem_read(a, n))

    def wr(self, a, b):
        self._w(a, bytes(b))

    def cstr(self, a, limit=4096):
        if not a:
            return None
        out = bytearray()
        while len(out) < limit:
            chunk = bytes(self.uc.mem_read(a + len(out), 64))
            z = chunk.find(b'\0')
            if z >= 0:
                out += chunk[:z]
                break
            out += chunk
        return out.decode('latin1')

    def wcstr(self, a, s, cap=None):
        b = s.encode('latin1') if isinstance(s, str) else bytes(s)
        if cap is not None:
            b = b[:max(cap - 1, 0)]
        self._w(a, b + b'\0')
        return len(b)

    def reg(self, name):
        return self.uc.reg_read(dict(REGS)[name])

    def regs(self):
        d = {n: self.uc.reg_read(r) for n, r in REGS}
        d['eip'] = self.uc.reg_read(UC_X86_REG_EIP)
        d['efl'] = self.uc.reg_read(UC_X86_REG_EFLAGS)
        return d

    # x87: ST(i) is physical register (TOP+i)&7, TOP in FPSW bits 11..13.
    def st(self, i):
        top = (self.uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7
        mant, exp = self.uc.reg_read(UC_X86_REG_FP0 + ((top + i) & 7))
        return _f80_to_float(mant, exp)

    def st_raw(self, i):
        top = (self.uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7
        return self.uc.reg_read(UC_X86_REG_FP0 + ((top + i) & 7))

    def fpu_top(self):
        return (self.uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7

    # ------------------------------------------------------ host allocation --
    def host_alloc(self, n, align=16):
        """Host-owned memory (COM objects, returned strings).  Bump-allocated;
        part of HostState so a replayed capture hands out the same addresses."""
        a = (self.hs.host_next + align - 1) & ~(align - 1)
        self.hs.host_next = a + n
        if self.hs.host_next > HOST_HI:
            raise GuestFault('host arena exhausted')
        return a

    def host_str(self, s):
        b = s.encode('latin1') + b'\0'
        a = self.host_alloc(len(b), 4)
        self.uc.mem_write(a, b)
        return a

    # ------------------------------------------------------------- thunks --
    def trap(self, name, pop, fn, nargs=None):
        """A 16-byte trap in the thunk page; executing it runs `fn`.
        `pop` is the stdcall argument bytes the callee removes; `nargs` the
        argument count recorded for the call log (defaults to pop/4)."""
        if name in self.thunk_names:
            return self.thunk_names[name]
        va = self.thunk_next
        if nargs is not None:
            self.thunk_nargs[va] = nargs
        self.thunk_next += 16
        if self.thunk_next >= THUNK_HI:
            raise GuestFault('thunk page exhausted')
        self.thunks[va] = (name, pop, fn)
        self.thunk_names[name] = va
        return va

    sub_stop = None          # (return address, entry esp) while a sub-run executes
    sub_done = False

    def _on_trap(self, uc, addr, size, _ud):
        try:
            if self.sub_stop is not None and addr == self.sub_stop[0] and \
                    uc.reg_read(UC_X86_REG_ESP) > self.sub_stop[1]:
                self.sub_done = True
                uc.emu_stop()
                return
            if addr == self.STOP:
                self.stop_reason = self.stop_reason or 'return'
                uc.emu_stop()
                return
            if addr == self.CONT:
                self._resume_callback()
                return
            if addr == self.TEXIT:
                self.cur.state = 'done'
                self.cur.code = uc.reg_read(UC_X86_REG_EAX)
                self._switch(block=True)
                return
            ent = self.thunks.get(addr)
            if ent is None:
                raise GuestFault('execution reached unclaimed trap %08X' % addr)
            name, pop, fn = ent
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = self.rd32(esp)
            self.import_counts[name] += 1
            if self.trace_imports:
                self.log('imp %s ret=%08X a=%s' % (name, ret, ' '.join(
                    '%08X' % self.rd32(esp + 4 + 4 * k) for k in range(max(pop // 4, 3)))))
            self.recent.append((name, ret))
            frame = {'name': name, 'ret': ret, 'esp': esp, 'pop': pop}
            if self.icall_log is not None:
                n = self.thunk_nargs.get(addr, pop // 4)
                frame['args'] = tuple(self.rd32(esp + 4 + 4 * k) for k in range(n))
            res = fn(self, Args(self, esp))
            if res is BLOCK:
                # the calling thread waits: it re-executes this trap when it
                # is next scheduled (retry semantics), so save it right here
                self._switch(block=True)
                return
            if hasattr(res, 'send'):
                frame['gen'] = res
                self._drive(frame, None)
            else:
                self._finish(frame, res)
            if self.want_yield:
                self.want_yield = False
                self._switch(block=False)
        except (GuestFault, Stop) as e:
            self.fault = e
            uc.emu_stop()
        except Exception as e:      # a bug in a model: stop with the evidence
            import traceback
            self.fault = GuestFault('model %s raised %s: %s\n%s' % (
                self.thunks.get(addr, ('?',))[0], type(e).__name__, e, traceback.format_exc()))
            uc.emu_stop()

    def _finish(self, frame, res):
        """Complete an import call: set the return value, pop the return
        address and any stdcall arguments, and resume at the caller."""
        uc = self.uc
        if isinstance(res, tuple):
            uc.reg_write(UC_X86_REG_EAX, res[0] & 0xFFFFFFFF)
            uc.reg_write(UC_X86_REG_EDX, res[1] & 0xFFFFFFFF)
        elif res is not None:
            uc.reg_write(UC_X86_REG_EAX, res & 0xFFFFFFFF)
        if self.icall_log is not None:
            self.icall_log.append((frame['name'], frame.get('args'), None if isinstance(res, tuple) else res))
        if frame.get('jump') is not None:          # a model that tail-jumps into guest code
            uc.reg_write(UC_X86_REG_ESP, frame['esp'])
            uc.reg_write(UC_X86_REG_EIP, frame['jump'])
            return
        uc.reg_write(UC_X86_REG_ESP, (frame['esp'] + 4 + frame['pop']) & 0xFFFFFFFF)
        uc.reg_write(UC_X86_REG_EIP, frame['ret'])

    def _drive(self, frame, sendval):
        """Step a model written as a generator.  It yields ('call', va, args)
        to run guest code (a WndProc, a qsort comparator, a CRT initialiser)
        and receives that call's eax; its `return` value completes the
        import.  The guest call is built on the stack just below the import's
        return address, with CONT as ITS return address, so no nested
        emu_start is ever needed and the callback may itself call imports."""
        gen = frame['gen']
        try:
            req = gen.send(sendval)
        except StopIteration as e:
            if frame.get('active'):
                self.pending.pop()
                frame['active'] = False
            self._finish(frame, e.value)
            return
        if req[0] == 'call':
            _, target, args = req
            sp = frame['esp']
            for v in reversed(args):
                sp -= 4
                self.wr32(sp, v)
            sp -= 4
            self.wr32(sp, self.CONT)
            self.uc.reg_write(UC_X86_REG_ESP, sp)
            self.uc.reg_write(UC_X86_REG_EIP, target)
            if not frame.get('active'):
                self.pending.append(frame)
                frame['active'] = True
            return
        if req[0] == 'jump':
            # complete the import by transferring into guest code with the
            # caller's frame intact (a forwarding thunk)
            if frame.get('active'):
                self.pending.pop()
                frame['active'] = False
            frame['jump'] = req[1]
            self._finish(frame, None)
            return
        raise GuestFault('model yielded %r' % (req,))

    def _resume_callback(self):
        if not self.pending:
            raise GuestFault('callback returned with no pending model')
        frame = self.pending[-1]
        eax = self.uc.reg_read(UC_X86_REG_EAX)
        # cdecl callbacks leave their arguments behind: the model's view of
        # the stack is the import's own frame, whatever the callee did
        self.uc.reg_write(UC_X86_REG_ESP, frame['esp'])
        self._drive(frame, eax)

    def _on_intr(self, uc, intno, _ud):
        self.fault = GuestFault('CPU exception %d at %08X' % (intno, uc.reg_read(UC_X86_REG_EIP)))
        uc.emu_stop()

    # ------------------------------------------------------------ running --
    def call(self, target, args, budget_s=None):
        """Run guest `target(args...)` to completion from the host (cdecl or
        stdcall alike: esp is restored afterwards).  Returns eax."""
        uc = self.uc
        esp0 = uc.reg_read(UC_X86_REG_ESP)
        sp = esp0
        for v in reversed(args):
            sp -= 4
            self.wr32(sp, v)
        sp -= 4
        self.wr32(sp, self.STOP)
        uc.reg_write(UC_X86_REG_ESP, sp)
        self.run_from(target, budget_s=budget_s)
        eax = uc.reg_read(UC_X86_REG_EAX)
        uc.reg_write(UC_X86_REG_ESP, esp0)
        return eax

    def run_from(self, eip, budget_s=None):
        """Execute from `eip` until the STOP trap, a fault or a Stop."""
        uc = self.uc
        self.fault = None
        self.stop_reason = None
        t0 = time.time()
        while True:
            try:
                uc.emu_start(eip, 0xFFFFFFFF)
            except UcError as e:
                raise GuestFault(self.describe_fault('%s' % e))
            if self.fault is not None:
                f, self.fault = self.fault, None
                if isinstance(f, Stop):
                    raise f
                raise GuestFault(self.describe_fault(str(f)))
            if self.stop_reason is not None:
                return
            if self.switch_req:
                self._do_switch()
            # a hook asked for an intermission (see brbox_drive / t3live):
            eip = uc.reg_read(UC_X86_REG_EIP)
            if self.intermission is not None:
                cb, self.intermission = self.intermission, None
                eip = cb(self) or uc.reg_read(UC_X86_REG_EIP)
            if budget_s is not None and time.time() - t0 > budget_s:
                raise Stop('budget')

    intermission = None

    def describe_fault(self, why):
        r = self.regs()
        lines = ['FAULT: %s' % why,
                 '  eip=%08X eax=%08X ecx=%08X edx=%08X ebx=%08X' % (
                     r['eip'], r['eax'], r['ecx'], r['edx'], r['ebx']),
                 '  esp=%08X ebp=%08X esi=%08X edi=%08X' % (
                     r['esp'], r['ebp'], r['esi'], r['edi'])]
        lines.append('  stack: ' + ' '.join('%08X' % self._safe32(r['esp'] + 4 * k) for k in range(12)))
        lines.append('  ebp chain: ' + ' '.join('%08X' % a for a in self.backtrace()))
        lines.append('  recent imports: ' + ', '.join(n.split('!')[-1] for n, _ in list(self.recent)[-12:]))
        return '\n'.join(lines)

    def _safe32(self, a):
        try:
            return self.rd32(a)
        except UcError:
            return 0xDEADDEAD

    def backtrace(self, depth=16):
        out = []
        ebp = self.uc.reg_read(UC_X86_REG_EBP)
        for _ in range(depth):
            if not (STACK_LO <= ebp < STACK_HI):
                break
            ret = self._safe32(ebp + 4)
            out.append(ret)
            nxt = self._safe32(ebp)
            if nxt <= ebp:
                break
            ebp = nxt
        return out

    # ----------------------------------------------------------- threads --
    # Cooperative and deterministic.  A thread runs until it blocks (a Wait*
    # on an unsignalled object) or reaches a yield point (Sleep, an idle
    # message pump); then the next ready thread in creation order runs.  The
    # only thread the single-player game creates is DirectPlay's, which
    # blocks at once on two events nobody sets.  During a live-oracle
    # sub-run no switch may happen (brbox_imports raises instead), so an A/B
    # comparison is always one thread's straight-line execution.
    want_yield = False
    subrun = False

    def _init_threads(self):
        self.threads = [Thread(1, 0, fs_sel=(1 << 3) | 3)]
        self.cur = self.threads[0]

    def thread_create(self, start, param):
        k = len(self.threads)
        if k >= 8:
            raise GuestFault('too many threads')
        lo = TSTACK_LO + (k - 1) * TSTACK_SIZE
        hi = lo + TSTACK_SIZE
        teb = self.host_alloc(0x1000, 0x1000)
        self.wr(teb, struct.pack('<III', 0xFFFFFFFF, hi, lo))
        self.wr32(teb + 0x18, teb)
        idx = 4 + k
        self.wr(GDT_VA + 8 * idx, _gdt_entry(teb, 0xFFF, 0xF3, 0x4))
        gdt_len = 8 * (idx + 1)
        self.uc.reg_write(UC_X86_REG_GDTR, (0, GDT_VA, max(gdt_len, 8 * 16) - 1, 0))
        sp = hi - 16
        self.wr32(sp + 4, param)
        self.wr32(sp, self.TEXIT)
        # an initial context: this thread's registers, cloned then re-pointed
        cur = self.uc.context_save()
        saved = {r: self.uc.reg_read(r) for r in (UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FS)}
        self.uc.reg_write(UC_X86_REG_FS, (idx << 3) | 3)
        self.uc.reg_write(UC_X86_REG_ESP, sp)
        self.uc.reg_write(UC_X86_REG_EIP, start)
        ctx = self.uc.context_save()
        self.uc.context_restore(cur)
        for r, v in saved.items():
            self.uc.reg_write(r, v)
        t = Thread(0x100 + k, 0, fs_sel=(idx << 3) | 3)
        t.ctx = ctx
        t.handle = self.imports.new_handle(self)
        self.threads.append(t)
        return t

    def _switch(self, block):
        """Request a thread switch.  Unicorn's context_save inside a hook
        does not see an EIP written by that hook, so the switch itself runs
        between emu_start calls (run_from -> _do_switch)."""
        if self.subrun:
            raise GuestFault('thread switch inside a live-oracle sub-run')
        if block:
            self.cur.state = 'blocked'
        self.switch_req = True
        self.uc.emu_stop()

    switch_req = False

    def others_runnable(self):
        return any(t is not self.cur and (t.state == 'ready' or (
            t.state == 'blocked' and self.imports.wait_ready(self, t))) for t in self.threads)

    def _do_switch(self):
        self.switch_req = False
        cur = self.cur
        cur.ctx = self.uc.context_save()
        cur.pending = self.pending
        n = len(self.threads)
        i0 = self.threads.index(cur)
        for _attempt in range(2):
            for k in range(1, n + 1):
                t = self.threads[(i0 + k) % n]
                if t.state == 'ready' or (t.state == 'blocked' and self.imports.wait_ready(self, t)):
                    t.state = 'ready'
                    self.cur = t
                    self.pending = t.pending
                    self.uc.context_restore(t.ctx)
                    return
            # nobody runnable now: jump the clock to the earliest wait deadline
            dl = [t.wait[2] for t in self.threads
                  if t.state == 'blocked' and t.wait is not None and t.wait[2] is not None]
            if not dl:
                break
            self.hs.ms = max(self.hs.ms, min(dl))
        raise Stop('deadlock: every thread is blocked')

    # --------------------------------------------------------- the clock --
    # Virtual time only moves when the game looks at it or waits for it: each
    # time query costs TICK_MS, Sleep(n) costs n, and a WaitMessage with an
    # empty queue jumps straight to the next scheduled event.  Deterministic,
    # and a busy-wait on timeGetTime terminates in a bounded number of polls.
    TICK_MS = 0.25

    def tick(self):
        self.hs.ms += self.TICK_MS

    def advance(self, ms):
        self.hs.ms += ms
        self.pump()

    def pump(self):
        """Deliver whatever is due at the current virtual time: WM_TIMERs
        and the input script's timed events."""
        import brbox_imports as I
        hs = self.hs
        for (hwnd, tid), t in sorted(hs.timers.items()):
            if t[1] <= hs.ms:
                t[1] = hs.ms + t[0]
                if not any(m[1] == I.WM_TIMER and m[0] == hwnd and m[2] == tid for m in hs.msgq):
                    I.post(self, hwnd, I.WM_TIMER, tid, 0)
        if self.driver is not None:
            self.driver.pump(self)

    def idle_peek(self):
        if self.driver is not None:
            self.driver.idle(self)

    def next_event_ms(self):
        cands = [t[1] for t in self.hs.timers.values()]
        if self.driver is not None:
            d = self.driver.next_event_ms(self)
            if d is not None:
                cands.append(d)
        return min(cands) if cands else None

    def wait_for_event(self):
        if self.hs.msgq:
            return
        t = self.next_event_ms()
        if t is None:
            raise Stop('WaitMessage with nothing ever due: the game is idle forever')
        self.hs.ms = max(self.hs.ms, t)
        self.pump()

    driver = None

    # ------------------------------------------------------ string tables --
    _strtabs = None

    def strings_for(self, hinst):
        if self._strtabs is None:
            self._strtabs = {}
        name = None
        if hinst == self.pe.image_base:
            name = REF_DLL
        elif hinst == EXE_BASE:
            name = os.path.join(CD_ROOT, 'BRally.exe')
        else:
            for k, v in self.hs.loaded.items():
                if v == hinst:
                    name = os.path.join(CD_ROOT, k)
        if name is None:
            return {}
        if name not in self._strtabs:
            self._strtabs[name] = _pe_strings(name)
        return self._strtabs[name]

    def exe_has_bitmaps(self):
        exe = PE.load(os.path.join(CD_ROOT, 'BRally.exe'))
        rs = exe.section_by_name('.rsrc')
        if rs is None:
            return False
        d, base = exe.data, rs.raw_ptr
        nn, ni = struct.unpack_from('<HH', d, base + 12)
        return any(struct.unpack_from('<I', d, base + 16 + 8 * k)[0] == 2 for k in range(nn + ni))

    # ------------------------------------------------------------ boot ----
    def boot(self):
        """DllMain(PROCESS_ATTACH), exactly as the loader runs it."""
        entry = self.pe.image_base + self.pe.entry_rva
        r = self.call(entry, [self.pe.image_base, 1, 0])
        if r == 0:
            raise GuestFault('DllMain refused PROCESS_ATTACH')

    def rally_main(self, cmdline='', budget_s=None):
        rm = next(va for va, n in self.pe.exports.items() if n == 'RallyMain')
        cl = self.host_str(cmdline)
        return self.call(rm, [EXE_BASE, 0, cl, 1], budget_s=budget_s)


def _pe_strings(path):
    """{id: text} from a PE's RT_STRING resources (case-insensitive path)."""
    d = os.path.dirname(path)
    real = next((os.path.join(d, f) for f in os.listdir(d)
                 if f.lower() == os.path.basename(path).lower()), None)
    if real is None:
        return {}
    p = PE.load(real)
    rs = p.section_by_name('.rsrc')
    if rs is None:
        return {}
    data = p.data
    base = rs.raw_ptr

    def entries(off):
        nn, ni = struct.unpack_from('<HH', data, base + off + 12)
        for k in range(nn + ni):
            nid, tgt = struct.unpack_from('<II', data, base + off + 16 + 8 * k)
            yield nid, tgt

    out = {}
    for tid, tgt in entries(0):
        if tid != 6 or not (tgt & 0x80000000):
            continue
        for bid, t2 in entries(tgt & 0x7FFFFFFF):
            for _lang, t3 in entries(t2 & 0x7FFFFFFF):
                drva, dsz = struct.unpack_from('<II', data, base + t3)
                o = p.rva_to_off(drva)
                pos = 0
                for i in range(16):
                    n = struct.unpack_from('<H', data, o + pos)[0]
                    pos += 2
                    if n:
                        out[(bid - 1) * 16 + i] = data[o + pos:o + pos + 2 * n].decode(
                            'utf-16-le').encode('latin1', 'replace').decode('latin1')
                    pos += 2 * n
    return out


def _f80_to_float(mant, exp):
    sign = -1.0 if exp & 0x8000 else 1.0
    e = exp & 0x7FFF
    if e == 0 and mant == 0:
        return 0.0 * sign
    if e == 0x7FFF:
        return float('nan') if mant & 0x7FFFFFFFFFFFFFFF else sign * float('inf')
    try:
        return sign * (mant / float(1 << 63)) * (2.0 ** (e - 16383))
    except OverflowError:
        return sign * float('inf')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd')
    r = sub.add_parser('run', help='boot the game headless and drive it')
    r.add_argument('--script', help='input timeline (tools/brbox_drive.py format)')
    r.add_argument('--frames', type=int, default=0, help='stop after N game frames')
    r.add_argument('--seconds', type=float, default=0, help='wall-clock budget')
    r.add_argument('--shots', help='dump framebuffer PNGs here at script SHOT lines')
    r.add_argument('--trace-imports', action='store_true')
    r.add_argument('--coverage', help='write reached-state coverage JSON here')
    a = ap.parse_args()
    if a.cmd != 'run':
        ap.print_help()
        return 2
    import brbox_drive
    return brbox_drive.run_cli(a)


if __name__ == '__main__':
    sys.exit(main())
