"""n64box.py -- Top Gear Rally in a box: the ORIGINAL ROM, run headless.

The N64 lane's counterpart of tools/brbox.py.  The game's own code -- all of
it, and the pure parts of the libraries (gu matrix maths, libm, libc, zlib) --
is executed for real by Unicorn (QEMU's MIPS core).  The only modelled surface
is the operating system and the hardware behind it:

  MODEL        behaviour the game depends on: threads and the scheduler,
               message queues and events, ROM DMA, the video retrace, the
               controllers (scripted input), timers, the RSP/RDP task
               handshake.
  STUB         pure output: debug printing, audio buffers, cache operations.
  UNMODELLED   anything else that touches hardware (an access to the MMIO
               range or a trap) stops the run and names the address, so
               nothing is ever silently guessed.

DETERMINISM.  One CPU, one virtual clock (a retrace every 1/60 s of virtual
time, advanced only when every game thread is blocked), one scripted input
timeline.  The same script gives the same run, instruction for instruction.

WHAT A RUN RECORDS.  Every graphics task the game submits (the display list,
walked through its branches with the matrices, vertices and other data it
points at, hashed), every audio task, and a hash of the game's RAM at every
retrace.  Two runs agree when those streams agree -- that is the whole-image
comparison (n64/tools/n64t3.py --image) and the ground truth the per-function
oracle is checked against.

    .venv/bin/python n64/tools/n64box.py run --frames 600
    .venv/bin/python n64/tools/n64box.py run --frames 600 --script n64/tools/n64box_scripts/attract.txt
"""
import argparse
import hashlib
import os
import struct
import sys

from unicorn import (Uc, UcError, UC_ARCH_MIPS, UC_MODE_MIPS64, UC_MODE_BIG_ENDIAN,
                     UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED, UC_HOOK_INTR, UC_HOOK_MEM_READ,
                     UC_HOOK_MEM_WRITE)
from unicorn import mips_const as M

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
ROM_PATH = os.environ.get('TGR_ROM', os.path.join(ROOT, 'reference/tgrally/Top Gear Rally (USA).z64'))

RDRAM = 0x800000                 # 8 MB: the game's 4 MB plus room for placed bodies
ANNEX = 0x80500000               # where bodies that outgrow their slot are placed
MMIO_LO, MMIO_HI = 0x04000000, 0x04900000
ENTRY = 0x80200000
TICKS_PER_FRAME = 781250         # osGetCount runs at 46.875 MHz; 1/60 s
FAKE_RA = 0x80000180             # an exception-vector address the game never runs

GPR = [getattr(M, 'UC_MIPS_REG_%d' % i) for i in range(32)]
FPR = [getattr(M, 'UC_MIPS_REG_F%d' % i) for i in range(32)]
REG = dict(zero=0, at=1, v0=2, v1=3, a0=4, a1=5, a2=6, a3=7, t0=8, s0=16, sp=29, ra=31)

OS_MESG_BLOCK = 1
# libultra OSMesgQueue: mtqueue, fullqueue, validCount, first, msgCount, msg
MQ_VALID, MQ_FIRST, MQ_COUNT, MQ_MSG = 8, 12, 16, 20
# libultra event numbers the game uses
EV_SP, EV_SI, EV_AI, EV_VI, EV_PI, EV_DP, EV_FAULT = 4, 5, 6, 7, 8, 9, 12


def sx(v):
    """A 32-bit address as the 64-bit CPU sees it (sign-extended)."""
    v &= 0xffffffff
    return v | 0xFFFFFFFF00000000 if v & 0x80000000 else v


class Unmodelled(Exception):
    pass


class Thread:
    def __init__(self, tcb, tid, entry, arg, sp, pri):
        self.tcb, self.id, self.pri = tcb, tid, pri
        self.regs = {'gpr': [0] * 32, 'fpr': [0] * 32, 'hi': 0, 'lo': 0, 'fcsr': 0,
                     'pc': entry}
        self.regs['gpr'][REG['a0']] = sx(arg)
        self.regs['gpr'][REG['sp']] = sx(sp)
        self.regs['gpr'][REG['ra']] = sx(FAKE_RA)
        self.state = 'stopped'          # stopped | runnable | blocked
        self.wait_mq = None


class Box:
    def __init__(self, rom_path=ROM_PATH, script=None, image=None, quiet=True, extra=()):
        self.rom = open(rom_path, 'rb').read()
        # The VR4300 is an R4000-family 64-bit CPU: the game's own assembly
        # uses 64-bit loads and stores, and libultra's long-long helpers use
        # 64-bit arithmetic, so the 64-bit core with an R4000 model it is.
        self.uc = Uc(UC_ARCH_MIPS, UC_MODE_MIPS64 | UC_MODE_BIG_ENDIAN)
        self.uc.ctl_set_cpu_model(M.UC_CPU_MIPS64_R4000)
        self.uc.mem_map(0, RDRAM)
        self.uc.mem_map(MMIO_LO, MMIO_HI - MMIO_LO)
        # IPL3 copies the first megabyte after the header to the entry point
        body = image if image is not None else self.rom[0x1000:0x101000]
        self.uc.mem_write(ENTRY & 0x1FFFFFFF, body)
        for va, blob in extra:                  # bodies placed in the annex
            self.uc.mem_write(va & 0x1FFFFFFF, blob)

        self.w32(0x80000300, 1)                 # osTvType: NTSC
        self.w32(0x80000318, 0x400000)          # osMemSize: 4 MB, no Expansion Pak
        self.quiet = quiet
        self.threads = {}
        self.cur = None
        self.events = {}                        # event -> (mq, msg)
        self.vi_event = None                    # (mq, msg, retraceCount)
        self.count = 0                          # virtual osGetCount
        self.frame = 0
        self.frames_wanted = 0
        self.pending = []                       # [(when, mq, msg)] deliveries
        self.ai_q, self.ai_rate = [], 0
        self.sp_busy = None
        self.framebuffer = 0
        self.log = []                           # (frame, kind, digest)
        self.prints = []
        self.pad = Script(script)
        self.stop_reason = None
        self.fault = None
        self.hle = {}
        self.install_hle()
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self.on_unmapped)
        self.uc.hook_add(UC_HOOK_INTR, self.on_intr)
        self.uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, self.on_mmio,
                         begin=sx(0xA4000000), end=sx(0xA48FFFFF))
        self.uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, self.on_mmio,
                         begin=MMIO_LO, end=MMIO_HI - 1)

    # ------------------------------------------------------------ memory
    @staticmethod
    def phys(va):
        return va & 0x1FFFFFFF

    def r32(self, va):
        return struct.unpack('>I', self.uc.mem_read(self.phys(va), 4))[0]

    def w32(self, va, v):
        self.uc.mem_write(self.phys(va), struct.pack('>I', v & 0xffffffff))

    def read(self, va, n):
        return bytes(self.uc.mem_read(self.phys(va), n))

    def write(self, va, b):
        self.uc.mem_write(self.phys(va), bytes(b))

    def reg(self, name):
        return self.uc.reg_read(GPR[REG[name]]) & 0xffffffff

    def setreg(self, name, v):
        self.uc.reg_write(GPR[REG[name]], sx(v))

    def arg(self, i):
        if i < 4:
            return self.uc.reg_read(GPR[4 + i]) & 0xffffffff
        return self.r32(self.reg('sp') + 4 * i)

    # ------------------------------------------------------------- faults
    def on_unmapped(self, uc, access, addr, size, value, data):
        pc = uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff
        self.fault = 'UNMAPPED access %08X (size %d) at pc %08X' % (addr & 0xffffffff, size, pc)
        uc.emu_stop()
        return False

    def on_intr(self, uc, intno, data):
        pc = uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff
        self.fault = 'TRAP %d at pc %08X' % (intno, pc)
        uc.emu_stop()

    def on_mmio(self, uc, access, addr, size, value, data):
        pc = uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff
        self.fault = 'UNMODELLED hardware access %08X at pc %08X (library entry %s)' % (
            addr & 0xffffffff, pc, self.last_lib and '%08X from %08X' % self.last_lib)
        uc.emu_stop()

    # ------------------------------------------------------------ threads
    def save(self, t):
        uc = self.uc
        t.regs['gpr'] = [uc.reg_read(r) for r in GPR]
        t.regs['fpr'] = [uc.reg_read(r) for r in FPR]
        t.regs['hi'] = uc.reg_read(M.UC_MIPS_REG_HI)
        t.regs['lo'] = uc.reg_read(M.UC_MIPS_REG_LO)
        t.regs['fcsr'] = uc.reg_read(M.UC_MIPS_REG_FCSR)

    def load(self, t):
        uc = self.uc
        for r, v in zip(GPR[1:], t.regs['gpr'][1:]):
            uc.reg_write(r, v)
        for r, v in zip(FPR, t.regs['fpr']):
            uc.reg_write(r, v)
        uc.reg_write(M.UC_MIPS_REG_HI, t.regs['hi'])
        uc.reg_write(M.UC_MIPS_REG_LO, t.regs['lo'])
        uc.reg_write(M.UC_MIPS_REG_FCSR, t.regs['fcsr'])
        uc.reg_write(M.UC_MIPS_REG_PC, sx(t.regs['pc']))

    def pick(self):
        run = [t for t in self.threads.values() if t.state == 'runnable']
        if not run:
            return None
        return max(run, key=lambda t: (t.pri, -t.id))

    def reschedule(self, resume_pc):
        """Switch to the best runnable thread.  The current thread resumes at
        resume_pc.  When only the idle thread (priority 0) can run, virtual
        time moves to the next event instead."""
        if self.cur is not None:
            self.save(self.cur)
            self.cur.regs['pc'] = resume_pc
        while True:
            nxt = self.pick()
            if nxt is not None and nxt.pri > 0:
                break
            if not self.advance_time():
                self.stop_reason = self.stop_reason or 'deadlock: no runnable thread'
                self.uc.emu_stop()
                return
            if self.stop_reason:
                self.uc.emu_stop()
                return
        self.cur = nxt
        self.load(nxt)

    def ret(self, v0=None, v1=None):
        if v0 is not None:
            self.setreg('v0', v0)
        if v1 is not None:
            self.setreg('v1', v1)
        self.uc.reg_write(M.UC_MIPS_REG_PC, sx(self.reg('ra')))

    # -------------------------------------------------------- mesg queues
    def mq_send(self, mq, msg, jam=False):
        valid, first, count = self.r32(mq + MQ_VALID), self.r32(mq + MQ_FIRST), self.r32(mq + MQ_COUNT)
        if valid >= count:
            return False
        buf = self.r32(mq + MQ_MSG)
        if jam:
            first = (first + count - 1) % count
            self.w32(buf + 4 * first, msg)
            self.w32(mq + MQ_FIRST, first)
        else:
            self.w32(buf + 4 * ((first + valid) % count), msg)
        self.w32(mq + MQ_VALID, valid + 1)
        for t in self.threads.values():
            if t.state == 'blocked' and t.wait_mq == mq:
                t.state, t.wait_mq = 'runnable', None
        return True

    def mq_recv(self, mq, out):
        valid = self.r32(mq + MQ_VALID)
        if valid == 0:
            return False
        first, count, buf = self.r32(mq + MQ_FIRST), self.r32(mq + MQ_COUNT), self.r32(mq + MQ_MSG)
        if out:
            self.w32(out, self.r32(buf + 4 * first))
        self.w32(mq + MQ_FIRST, (first + 1) % count)
        self.w32(mq + MQ_VALID, valid - 1)
        for t in self.threads.values():                 # a sender waiting for room
            if t.state == 'blocked' and t.wait_mq == ('full', mq):
                t.state, t.wait_mq = 'runnable', None
        return True

    def post_event(self, ev):
        if ev in self.events:
            mq, msg = self.events[ev]
            self.mq_send(mq, msg)

    # --------------------------------------------------------------- time
    def advance_time(self):
        """Nothing can run: deliver the next thing the hardware would do.
        -> False when there is nothing left that could ever wake a thread."""
        next_vi = (self.count // TICKS_PER_FRAME + 1) * TICKS_PER_FRAME
        self.pending.sort(key=lambda p: p[0])
        if self.pending and self.pending[0][0] <= next_vi:
            when, kind, a, b = self.pending.pop(0)
            self.count = max(self.count, when)
            if kind == 'event':
                self.post_event(a)
            elif kind == 'mq':
                self.mq_send(a, b)
            elif kind == 'timer':
                t, interval, mq = a
                self.mq_send(mq, b)
                if interval:
                    self.pending.append((self.count + interval, 'timer', a, b))
            return True
        # the next vertical retrace
        self.count = next_vi
        self.frame += 1
        self.log.append((self.frame, 'ram', self.ram_digest()))
        if self.frames_wanted and self.frame >= self.frames_wanted:
            self.stop_reason = 'frames'
            return True
        if self.vi_event:
            mq, msg, n = self.vi_event
            if self.frame % max(1, n) == 0:
                self.mq_send(mq, msg)
        self.post_event(EV_VI)
        return True

    # Thread stacks: top (the sp osCreateThread gets) -> bottom.  Below a
    # thread's current sp is dead space; a body that keeps its temporaries in
    # registers instead of the stack leaves different garbage there without
    # behaving differently, so the digest does not look at it.  Bottoms are
    # from the idle thread's stack-fill loops where it has them (main, SP and
    # DP event threads), otherwise the end of the thread's own OSThread.
    STACKS = {0x80316CD0: 0x803168D0,       # idle (boot stack top below it)
              0x80318CD0: 0x80316CD0,       # main game thread, filled 8 KB
              0x803196D0: 0x80318ED0,       # SP event thread, filled 2 KB
              0x80319ED0: 0x803196D0,       # DP event thread, filled 2 KB
              0x8031B1B0: 0x8031ADB0,       # fault thread
              0x80379568: 0x80379168}       # audio thread

    def ram_digest(self):
        lo, hi = 0x8026FAB0, 0x802AC400 + 0xD67B0
        ram = bytearray(self.read(lo, hi - lo))
        cur_sp = {}
        for t in self.threads.values():
            sp = (self.reg('sp') if t is self.cur else t.regs['gpr'][REG['sp']]) & 0xffffffff
            cur_sp[t] = sp
        for t, sp in cur_sp.items():
            for top, bottom in self.STACKS.items():
                if bottom <= sp <= top:
                    a, b = max(bottom, lo) - lo, min(sp, hi) - lo
                    if a < b:
                        ram[a:b] = bytes(b - a)
        return hashlib.sha1(bytes(ram)).hexdigest()[:16]

    # ---------------------------------------------------------------- HLE
    def install_hle(self):
        H = self.hle
        H[0x80265D90] = ('osInitialize', lambda: self.ret())
        H[0x80265670] = ('osCreateThread', self.os_create_thread)
        H[0x802657C0] = ('osStartThread', self.os_start_thread)
        H[0x802668F0] = ('osSetThreadPri', self.os_set_thread_pri)
        H[0x80265410] = ('osCreateMesgQueue', self.os_create_mq)
        H[0x802654B0] = ('osSendMesg', self.os_send)
        H[0x802642E0] = ('osRecvMesg', self.os_recv)
        H[0x80265440] = ('osSetEventMesg', self.os_set_event)
        H[0x80265600] = ('osViSetEvent', self.os_vi_set_event)
        H[0x80266400] = ('osCreateViManager', lambda: self.ret())
        H[0x80266760] = ('osCreatePiManager', lambda: self.ret())
        H[0x80264A60] = ('osViSetMode', lambda: self.ret())
        H[0x80260AB0] = ('osViBlack', lambda: self.ret())
        H[0x80264AD0] = ('osViSwapBuffer', self.os_vi_swap)
        H[0x80264B20] = ('osViGetCurrentFramebuffer', lambda: self.ret(self.framebuffer))
        H[0x80264650] = ('osPiStartDma', self.os_pi_dma)
        H[0x802663A0] = ('osPiReadIo', self.os_pi_read_io)
        H[0x802662E0] = ('osInvalDCache', lambda: self.ret())
        H[0x802649C0] = ('osWritebackDCacheAll', lambda: self.ret())
        H[0x802649F0] = ('osGetCount', lambda: self.ret(self.count & 0xffffffff))
        H[0x802607DC] = ('osSyncPrintf', self.os_print)
        H[0x80266390] = ('__osGetCurrFaultedThread', lambda: self.ret(0))
        H[0x80265910] = ('osContInit', self.os_cont_init)
        H[0x80261940] = ('osPfsIsPlug', self.os_pfs_is_plug)
        H[0x80262370] = ('osMotorInit', lambda: self.ret(1))      # PFS_ERR_NOPACK
        H[0x80261F20] = ('osMotorStop', lambda: self.ret(0))
        H[0x80262088] = ('osMotorStart', lambda: self.ret(0))
        # No Controller Pak is plugged in: every pak call reports PFS_ERR_NOPACK
        for va in (0x80261CB0, 0x80265CD0, 0x80262540, 0x80262660, 0x802628C0, 0x80262A80,
                   0x802635DC, 0x802639E0, 0x80263B30, 0x802677E0, 0x80267930, 0x80267C20):
            H[va] = ('osPfs* (no pak)', lambda: self.ret(1))
        H[0x80268390] = ('osSetTimer', self.os_set_timer)
        # libultra's float <-> long long helpers.  The VR4300 runs cvt.l/trunc.l
        # with FR=0; QEMU only allows them with FR=1, so these are modelled
        # exactly (C semantics: truncate toward zero).
        H[0x802669D0] = ('__d_to_ll', lambda: self.to_ll(self.fdouble(12), True))
        H[0x802669EC] = ('__f_to_ll', lambda: self.to_ll(self.fsingle(12), True))
        H[0x80266A08] = ('__d_to_ull', lambda: self.to_ll(self.fdouble(12), False))
        H[0x80266AA8] = ('__f_to_ull', lambda: self.to_ll(self.fsingle(12), False))
        H[0x80266B44] = ('__ll_to_d', lambda: self.from_ll(True, True))
        H[0x80266B5C] = ('__ll_to_f', lambda: self.from_ll(True, False))
        H[0x80266B74] = ('__ull_to_d', lambda: self.from_ll(False, True))
        H[0x80266BA8] = ('__ull_to_f', lambda: self.from_ll(False, False))
        H[0x80264760] = ('osContStartReadData', self.os_cont_start_read)
        H[0x80264824] = ('osContGetReadData', self.os_cont_get_read)
        H[0x80264C7C] = ('osSpTaskLoad', self.os_sp_task)
        H[0x80264E0C] = ('osSpTaskStartGo', lambda: self.ret())
        # The audio interface plays queued buffers at the set rate, 4 bytes a
        # sample; the game's streaming driver paces itself on what is left.
        H[0x80268520] = ('osAiGetStatus', lambda: self.ret(self.ai_status()))
        H[0x80268530] = ('osAiGetLength', lambda: self.ret(self.ai_length()))
        H[0x80268230] = ('osAiSetFrequency', self.os_ai_set_frequency)
        H[0x80268470] = ('osAiSetNextBuffer', self.os_ai_set_next)
        for va in H:
            self.uc.hook_add(UC_HOOK_CODE, self.on_hle, begin=sx(va), end=sx(va))
        self.last_lib = None
        import csv
        for r in csv.DictReader(open(os.path.join(N64, 'config/fenced_tgr.csv'))):
            v = int(r['va'], 16)
            if v not in H:
                self.uc.hook_add(UC_HOOK_CODE, self.on_lib, begin=sx(v), end=sx(v))
        # a thread returning from its entry function has nowhere to go
        self.uc.hook_add(UC_HOOK_CODE, self.on_thread_exit, begin=sx(FAKE_RA), end=sx(FAKE_RA))

    def on_hle(self, uc, addr, size, data):
        name, fn = self.hle[addr & 0xffffffff]
        try:
            fn()
        except Unmodelled as e:
            self.fault = str(e)
            uc.emu_stop()

    def on_lib(self, uc, addr, size, data):
        if self.last_lib is None or not (0x802607AC <= self.reg('ra') < 0x80270000):
            self.last_lib = (addr & 0xffffffff, self.reg('ra'))

    def on_thread_exit(self, uc, addr, size, data):
        self.fault = 'thread %s returned from its entry function' % (self.cur and self.cur.id)
        uc.emu_stop()

    def os_create_thread(self):
        tcb, tid, entry, arg, sp, pri = [self.arg(i) for i in range(6)]
        self.threads[tcb] = Thread(tcb, tid, entry, arg, sp, pri)
        self.ret()

    def os_start_thread(self):
        t = self.threads[self.arg(0)]
        t.state = 'runnable'
        if self.cur is None:                # from boot: this thread takes over
            self.cur = None
            self.reschedule_from_boot()
            return
        if t.pri > self.cur.pri:
            self.cur.state = 'runnable'
            self.reschedule(self.reg('ra'))
        else:
            self.ret()

    def reschedule_from_boot(self):
        nxt = self.pick()
        self.cur = nxt
        self.load(nxt)

    def os_set_thread_pri(self):
        tcb, pri = self.arg(0), self.arg(1)
        t = self.cur if tcb == 0 else self.threads[tcb]
        t.pri = pri
        best = self.pick()
        if best is not None and best is not self.cur and best.pri > self.cur.pri:
            self.cur.state = 'runnable'
            self.reschedule(self.reg('ra'))
        elif self.cur.pri == 0:
            # the idle thread lowering itself: from here on it only spins
            self.reschedule(self.reg('ra'))
        else:
            self.ret()

    def os_create_mq(self):
        mq, buf, count = self.arg(0), self.arg(1), self.arg(2)
        for off, v in ((0, 0x802A6230), (4, 0x802A6230), (MQ_VALID, 0), (MQ_FIRST, 0),
                       (MQ_COUNT, count), (MQ_MSG, buf)):
            self.w32(mq + off, v)
        self.ret()

    def os_send(self):
        mq, msg, flag = self.arg(0), self.arg(1), self.arg(2)
        if self.mq_send(mq, msg):
            woke = self.pick()
            if woke is not None and woke is not self.cur and woke.pri > self.cur.pri:
                self.setreg('v0', 0)
                self.cur.state = 'runnable'
                self.reschedule(self.reg('ra'))
                return
            self.ret(0)
            return
        if flag != OS_MESG_BLOCK:
            self.ret(0xffffffff)
            return
        self.cur.state, self.cur.wait_mq = 'blocked', ('full', mq)
        self.reschedule(self.uc.reg_read(M.UC_MIPS_REG_PC))

    def os_recv(self):
        mq, out, flag = self.arg(0), self.arg(1), self.arg(2)
        if self.mq_recv(mq, out):
            self.ret(0)
            return
        if flag != OS_MESG_BLOCK:
            self.ret(0xffffffff)
            return
        self.cur.state, self.cur.wait_mq = 'blocked', mq
        self.reschedule(self.uc.reg_read(M.UC_MIPS_REG_PC))     # retry the call on wake

    def os_set_event(self):
        self.events[self.arg(0)] = (self.arg(1), self.arg(2))
        self.ret()

    def os_vi_set_event(self):
        self.vi_event = (self.arg(0), self.arg(1), self.arg(2))
        self.ret()

    def os_vi_swap(self):
        self.framebuffer = self.arg(0)
        self.log.append((self.frame, 'swap', '%08X' % self.framebuffer))
        self.ret()

    def os_pi_dma(self):
        # osPiStartDma(mb, pri, direction, devAddr, vAddr, nbytes, mq)
        mb, pri, direction, dev, vaddr, n, mq = [self.arg(i) for i in range(7)]
        if direction != 0:
            raise Unmodelled('osPiStartDma write to the cartridge')
        src = dev & 0x0FFFFFFF
        self.write(vaddr, self.rom[src:src + n].ljust(n, b'\0'))
        if mb:
            self.w32(mb + 4, mq)
        self.pending.append((self.count, 'mq', mq, mb))
        self.ret(0)

    def os_pi_read_io(self):
        dev, out = self.arg(0), self.arg(1)
        off = dev & 0x0FFFFFFF
        v = struct.unpack('>I', self.rom[off:off + 4])[0] if off + 4 <= len(self.rom) else 0
        self.w32(out, v)
        self.ret(0)

    def os_print(self):
        self.prints.append((self.frame, self.arg(0)))
        self.ret()

    AI_CLOCK = 48681812                  # NTSC video clock the DAC divides

    def ai_update(self):
        rate = getattr(self, 'ai_rate', 0)
        q = getattr(self, 'ai_q', [])
        while q and rate:
            start, n = q[0]
            played = (self.count - start) * rate * 4 // 46875000
            if played < n:
                break
            q.pop(0)
            if q:
                q[0] = (start + n * 46875000 // (rate * 4), q[0][1])
        self.ai_q = q

    def ai_length(self):
        self.ai_update()
        if not self.ai_q or not getattr(self, 'ai_rate', 0):
            return 0
        start, n = self.ai_q[0]
        played = (self.count - start) * self.ai_rate * 4 // 46875000
        return max(0, n - played) & ~7

    def ai_status(self):
        self.ai_update()
        return 0x80000000 if len(self.ai_q) >= 2 else 0

    def os_ai_set_frequency(self):
        f = self.arg(0)
        dac = int(self.AI_CLOCK / f + .5)
        self.ai_rate = self.AI_CLOCK // dac
        self.ret(self.ai_rate)

    def os_ai_set_next(self):
        self.ai_update()
        if len(getattr(self, 'ai_q', [])) >= 2:
            self.ret(0xffffffff)
            return
        n = self.arg(1)
        start = self.count
        if self.ai_q:
            s0, n0 = self.ai_q[-1]
            start = max(start, s0 + n0 * 46875000 // (max(1, self.ai_rate) * 4))
        self.ai_q.append((start, n))
        self.log.append((self.frame, 'ai', hashlib.sha1(self.read(self.arg(0), n)).hexdigest()[:16]))
        self.ret(0)

    def os_set_timer(self):
        # osSetTimer(t, OSTime countdown, OSTime interval, mq, msg): the u64s
        # travel in a2:a3 and at sp+0x10; the rest follow on the stack.
        t = self.arg(0)
        countdown = (self.arg(2) << 32) | self.arg(3)
        interval = (self.r32(self.reg('sp') + 0x10) << 32) | self.r32(self.reg('sp') + 0x14)
        mq, msg = self.r32(self.reg('sp') + 0x18), self.r32(self.reg('sp') + 0x1C)
        self.pending.append((self.count + max(1, countdown), 'timer', (t, interval, mq), msg))
        self.ret(0)

    # FR=0: a double lives in an even/odd pair, low word in the even register
    def fsingle(self, r):
        return struct.unpack('>f', struct.pack('>I', self.uc.reg_read(FPR[r]) & 0xffffffff))[0]

    def fdouble(self, r):
        lo = self.uc.reg_read(FPR[r]) & 0xffffffff
        hi = self.uc.reg_read(FPR[r + 1]) & 0xffffffff
        return struct.unpack('>d', struct.pack('>II', hi, lo))[0]

    def to_ll(self, x, signed):
        if x != x:
            v = 0
        else:
            v = int(x)                                  # toward zero
        v &= 0xffffffffffffffff
        self.ret(v >> 32, v & 0xffffffff)

    def from_ll(self, signed, double):
        v = (self.arg(0) << 32) | self.arg(1)
        if signed and v & (1 << 63):
            v -= 1 << 64
        if double:
            hi, lo = struct.unpack('>II', struct.pack('>d', float(v)))
            self.uc.reg_write(FPR[0], lo)
            self.uc.reg_write(FPR[1], hi)
        else:
            self.uc.reg_write(FPR[0], struct.unpack('>I', struct.pack('>f', float(v)))[0])
        self.ret()

    def os_cont_init(self):
        mq, pattern, status = self.arg(0), self.arg(1), self.arg(2)
        self.write(pattern, b'\x01')                   # one controller, port 1
        # OSContStatus[4]: type (u16), status (u8), errno (u8)
        self.write(status, b'\x05\x00\x00\x00' + b'\x00\x00\x00\x08' * 3)
        self.ret(0)

    def os_pfs_is_plug(self):
        self.write(self.arg(1), b'\x00')               # no Controller Pak
        self.ret(0)

    def os_cont_start_read(self):
        self.pending.append((self.count, 'event', EV_SI, None))
        self.ret(0)

    def os_cont_get_read(self):
        out = self.arg(0)
        # OSContPad[4]: button (u16), stick_x (s8), stick_y (s8), errno (u8), pad
        b, x, y = self.pad.at(self.frame)
        self.write(out, struct.pack('>HbbBx', b, x, y, 0) + b'\x00\x00\x00\x00\x08\x00' * 3)
        self.ret()

    def os_sp_task(self):
        task = self.arg(0)
        # OSTask: type, flags, ucode_boot, ..., data_ptr @0x30, data_size @0x34
        ttype = self.r32(task)
        dptr, dsize = self.r32(task + 0x30), self.r32(task + 0x34)
        if ttype == 1:
            self.log.append((self.frame, 'gfx', self.dl_digest(dptr)))
            self.pending.append((self.count + 1, 'event', EV_SP, None))
            self.pending.append((self.count + 2, 'event', EV_DP, None))
        else:
            self.log.append((self.frame, 'task%d' % ttype,
                             hashlib.sha1(self.read(dptr, min(dsize, 0x20000))).hexdigest()[:16]))
            self.pending.append((self.count + 1, 'event', EV_SP, None))
        self.ret()

    # ------------------------------------------------------ display lists
    def dl_digest(self, dl):
        """Hash a display list, following branches, with the data it uses."""
        h = hashlib.sha1()
        seg = [0] * 16
        stack = [dl]
        seen = 0

        def addr(a):
            return (seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) | 0x80000000
        while stack and seen < 200000:
            pc = stack.pop()
            while seen < 200000:
                seen += 1
                w0, w1 = self.r32(pc), self.r32(pc + 4)
                h.update(struct.pack('>II', w0, w1))
                op = w0 >> 24
                pc += 8
                if op == 0xDB and (w0 >> 16) & 0xFF == 0x06:        # G_MOVEWORD segment
                    seg[((w0 & 0xFFFF) >> 2) & 0xF] = w1 & 0x1FFFFFFF
                elif op == 0x01:                                      # G_MTX
                    h.update(self.read(addr(w1), 64))
                elif op == 0x04:                                      # G_VTX (F3DEX)
                    n = ((w0 >> 10) & 0x3F) or 1
                    h.update(self.read(addr(w1), 16 * n))
                elif op == 0x03:                                      # G_MOVEMEM
                    h.update(self.read(addr(w1), 16))
                elif op == 0x06:                                      # G_DL
                    if (w0 >> 16) & 0xFF == 1:
                        pc = addr(w1)
                    else:
                        stack.append(pc)
                        pc = addr(w1)
                elif op == 0xB8:                                      # G_ENDDL
                    break
        return h.hexdigest()[:16]

    # ---------------------------------------------------------------- run
    def boot(self):
        self.uc.reg_write(M.UC_MIPS_REG_PC, sx(ENTRY))
        self.uc.reg_write(GPR[REG['sp']], sx(0x803168D0))

    def run(self, frames, max_insns=0):
        """Run until `frames` retraces.  The CPU is only ever stopped from
        inside a model (a known-safe point).  A stop at an arbitrary
        instruction -- Unicorn's own timeout -- can land in a branch delay
        slot, and resuming from there corrupts the branch, so a watchdog
        thread ends a stuck run for good instead of pausing it."""
        import threading
        import time
        self.frames_wanted = frames
        pc = self.uc.reg_read(M.UC_MIPS_REG_PC)
        if pc == 0:
            self.boot()
        state = dict(frame=self.frame, t=time.time(), done=False)

        def watchdog():
            while not state['done']:
                time.sleep(1)
                if self.frame != state['frame']:
                    state['frame'], state['t'] = self.frame, time.time()
                elif time.time() - state['t'] > 600 and not state['done']:
                    self.fault = ('no retrace for 600 s of wall time: spinning at pc %08X '
                                  '(library entry %s)' % (
                                      self.uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff,
                                      self.last_lib and '%08X from %08X' % self.last_lib))
                    self.uc.emu_stop()
                    return
        if not max_insns:
            threading.Thread(target=watchdog, daemon=True).start()
        try:
            while not self.stop_reason and not self.fault:
                try:
                    self.uc.emu_start(self.uc.reg_read(M.UC_MIPS_REG_PC), sx(0xFFFFFFFC),
                                      count=max_insns)
                except UcError as e:
                    if not self.fault and not self.stop_reason:
                        self.fault = 'unicorn: %s at pc %08X' % (
                            e, self.uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff)
                if max_insns:
                    break
        finally:
            state['done'] = True
        return self.fault or self.stop_reason


class Script:
    """Scripted pad input: lines `frame buttons [stick_x stick_y]`, buttons
    as names joined by + (A B Z START L R CU CD CL CR DU DD DL DR) or `-`."""
    BITS = dict(A=0x8000, B=0x4000, Z=0x2000, START=0x1000, DU=0x0800, DD=0x0400,
                DL=0x0200, DR=0x0100, L=0x0020, R=0x0010, CU=0x0008, CD=0x0004,
                CL=0x0002, CR=0x0001)

    def __init__(self, path):
        self.events = []
        if path:
            for line in open(path):
                line = line.split('#')[0].split()
                if not line:
                    continue
                f = int(line[0])
                b = 0
                if line[1] != '-':
                    for k in line[1].split('+'):
                        b |= self.BITS[k]
                x = int(line[2]) if len(line) > 2 else 0
                y = int(line[3]) if len(line) > 3 else 0
                self.events.append((f, b, x, y))
        self.events.sort()

    def at(self, frame):
        cur = (0, 0, 0)
        for f, b, x, y in self.events:
            if f > frame:
                break
            cur = (b, x, y)
        return cur


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['run'])
    ap.add_argument('--frames', type=int, default=300)
    ap.add_argument('--script')
    ap.add_argument('--log')
    a = ap.parse_args()
    box = Box(script=a.script)
    r = box.run(a.frames)
    print('stopped: %s after %d frames, %d gfx tasks, %d prints'
          % (r, box.frame, sum(1 for x in box.log if x[1] == 'gfx'), len(box.prints)))
    if a.log:
        with open(a.log, 'w') as f:
            for row in box.log:
                f.write('%d %s %s\n' % row)


if __name__ == '__main__':
    main()
