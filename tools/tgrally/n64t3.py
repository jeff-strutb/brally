"""T3 for Top Gear Rally: the live oracle, the whole-image run, and the gate.

    .venv/bin/python tools/tgrally/n64t3.py --live 0x8022439C [VA ...]   # A5
    .venv/bin/python tools/tgrally/n64t3.py --image [--with VA ...]      # A7
    .venv/bin/python tools/tgrally/n64t3.py --qualify 0x8022439C          # the gate

Same standard as the PC lane.  T3 means certified
complete and behaving exactly like the original, not byte-exact.

A5 -- LIVE ORACLE.  The original ROM runs headless (n64box.py) on every script
in tools/tgrally/n64box_scripts.  Each time the game calls a function under test,
the whole machine is snapshotted; the ORIGINAL body runs to its return; the
machine is put back and the CANDIDATE body (compiled from src/tgrally, linked into
the annex) runs from the identical state to the same return.  Memory (all of
it except the dead stack below the caller), the return and callee-saved
registers and every side effect the box records must agree.  The original's
result is kept and the run goes on, so every later call sees real game state.
OS calls the tested call makes (message waits, pads, DMA, video) are
recorded in the live run and replayed in both sandboxes; the candidate must
make the same ones in the same order (see RecBox).
A thread entry that never returns (THREAD_LOOPS) is compared one loop
iteration at a time instead: each arrival of the live thread at the ROM
body's outer loop head starts a recording and the next arrival ends it; the
original runs from that head to its next head, and the candidate runs from
its own entry (its prologue sets up its own registers; an OS call before its
loop head ends the run) through one iteration to its second head arrival.
All memory outside the thread's frame and every OS call must agree;
registers are not compared, since the loop carries nothing in them.
Verdicts: EQUIVALENT (compared >= 1, no divergence), DIVERGENT, UNCOVERED.
They land in config/tgrally/t3_live.csv with the source's hash.

A7 -- WHOLE IMAGE.  Every T3 body placed at once (n64image.py --t3, plus
--with for bodies being qualified), every script run for its full length, and
the log -- each display list with the data it uses, each audio buffer, the
game's RAM at every retrace -- compared with the original's.  IDENTICAL or the
first frame that differs.  Recorded in config/tgrally/whole_image.csv.

THE GATE (--qualify).  Gate 0: a WHAT IT DOES comment, an @t3 tag, no
unfinished markers.  Gate A5: EQUIVALENT for the current source.  Gate A7:
the newest whole-image run includes this body at its current source and is
IDENTICAL.  Gate B: two `@t4-pass` lines in the source from
tools/tgrally/n64permute.py (>= 10 compiles each), the later one moving nothing --
the byte grind was tried and stalled.  An unreached function never passes.
"""
import argparse
import copy
import csv
import datetime
import hashlib
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools/tgrally'))
import n64build as B  # noqa: E402
import n64link as L  # noqa: E402
import n64box as NB  # noqa: E402
import n64image as IMG  # noqa: E402
import orphanguard  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn import mips_const as M  # noqa: E402

SCRIPTS = os.path.join(ROOT, 'tools/tgrally/n64box_scripts')
LIVE = os.path.join(ROOT, 'config/tgrally/t3_live.csv')
WHOLE = os.path.join(ROOT, 'config/tgrally/whole_image.csv')
FRAMES = {'race_pause.txt': 4000, 'arcade_timeup.txt': 6000, 'loadsave_pak.txt': 3000}   # per script; default 3600 frames (a minute)


def script_frames(sc):
    """A script's length: its own `frames N` line, else FRAMES, else a minute."""
    path = sc if os.path.isabs(sc) else os.path.join(SCRIPTS, sc)
    n = NB.Script(path).frames
    return n or FRAMES.get(os.path.basename(sc), 3600)
TEST_CODE, TEST_DATA = 0x80600000, 0x80780000
CODE_LO, CODE_HI = 0x80200000, 0x8026FAB0
DEAD_STACK = 0x4000
ARG_HOME = 0x20
MAX_CALLS = 150
# Thread entries that never return: compared one loop iteration at a time
# (see Lockstep.on_head).  The iteration runs from the outer loop head to the
# next arrival there.
THREAD_LOOPS = {0x80257D3C}       # BrMusicThread


def loop_head(words, base):
    """The outer loop head of a body: the lowest target of a backward
    unconditional branch (b = beq $0,$0)."""
    heads = []
    for i, w in enumerate(words):
        if w >> 16 == 0x1000:
            off = w & 0xffff
            if off & 0x8000:
                heads.append(base + 4 * (i + 1) + ((off - 0x10000) << 2))
    return min(heads) if heads else None


def rom_frame(va):
    """The frame size the ROM body's prologue allocates (addiu sp,sp,-N)."""
    rom = open(NB.ROM_PATH, 'rb').read()
    w = struct.unpack('>I', rom[va - NB.ENTRY + 0x1000:va - NB.ENTRY + 0x1004])[0]
    return 0x10000 - (w & 0xffff) if w >> 16 == 0x27BD else 0


def rom_words(va):
    size = B.function_map()[va]
    rom = open(NB.ROM_PATH, 'rb').read()
    off = va - NB.ENTRY + 0x1000
    return list(struct.unpack('>%dI' % (size // 4), rom[off:off + size]))


def scripts():
    return sorted(f for f in os.listdir(SCRIPTS) if f.endswith('.txt'))


def source_of(va):
    """-> (path, name, source sha) of the @implements tag for va."""
    for f in B.all_sources():
        src = open(f).read()
        for v, n, _ in B.tags_in(src):
            if v == va:
                return f, n, hashlib.sha1(function_text(src, n).encode()).hexdigest()[:12]
    return None, None, None


def function_text(src, name):
    # a K&R definition has its parameter declarations between ) and {
    m = re.search(r'^[^\n;{}]*\b%s\s*\([^;{]*\)(?:\s*[^;{}()]+;)*\s*\{' % re.escape(name), src, re.M)
    if not m:
        return ''
    depth, i = 0, m.end() - 1
    while i < len(src):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[m.start():i + 1]
        i += 1
    return src[m.start():]


def return_kind(va):
    if CAND_DIR:
        f = os.path.join(CAND_DIR, '0x%08X.c' % va)
        name = re.search(r'@implements 0x[0-9A-F]{8} tgr (\w+)', open(f).read()).group(1)
    else:
        f, name, sha = source_of(va)
    text = function_text(open(f).read(), name)
    head = text.split('(')[0]
    if re.search(r'\bvoid\s+\w+\s*$', head) and '*' not in head:
        return 'void'
    if re.search(r'\bdouble\b', head) and '*' not in head:
        return 'double'
    if re.search(r'\bfloat\b', head) and '*' not in head:
        return 'float'
    if re.search(r'\blong\s+long\b', head) and '*' not in head:
        return 'llong'
    return 'int'


CAND_DIR = None                 # --cand: test build/tgrally/n64/cand drafts, not src/tgrally


def link_candidate(va, code_va, data_va):
    if CAND_DIR:
        f = os.path.join(CAND_DIR, '0x%08X.c' % va)
        src = open(f).read()
        name = re.search(r'@implements 0x[0-9A-F]{8} tgr (\w+)', src).group(1)
        obj, err = B.compile_c(f)
        if obj is None:
            raise L.LinkError('compile error')
        code, data = L.link_function(obj, name, code_va, data_va, {name: va}, B.load_symbols())
        return code, data, hashlib.sha1(src.encode()).hexdigest()[:12]
    f, name, sha = source_of(va)
    if f is None:
        raise L.LinkError('no source tags %08X' % va)
    obj, err = B.compile_c(f)
    if obj is None:
        raise L.LinkError('compile error: %s' % err.strip().split('\n')[0])
    fnvas = {n: v for v, n, _ in B.tags_in(open(f).read())}
    code, data = L.link_function(obj, name, code_va, data_va, fnvas, B.load_symbols(),
                                 static_va=B.static_bases(obj, fnvas))
    return code, data, sha


# ------------------------------------------------------- record / replay
# A call under test may make OS calls that block (a message wait), switch
# threads or talk to hardware (pads, DMA, video).  The sandboxes cannot model
# those, so the live run RECORDS each one the call makes -- its arguments,
# what came back (v0/v1/f0/f1), every byte of RAM that changed from the call
# until the thread resumed (other threads ran meanwhile), and what landed in
# out-parameters in the caller's own frame -- and both sandboxes REPLAY that
# recording in order.  The candidate must make the same OS calls, in the same
# order, with the same arguments; a pointer into its own stack frame is only
# required to be in its stack frame (frame layout is not behaviour), and an
# out-parameter there is written at the candidate's pointer.
NATIVE_OS = ('osSyncPrintf', 'osInvalDCache', 'osWritebackDCacheAll')
NARGS = {'osRecvMesg': 3, 'osSendMesg': 3, 'osContStartReadData': 1, 'osContGetReadData': 1,
         'osPiStartDma': 7, 'osViSwapBuffer': 1, 'osSpTaskLoad': 1, 'osSpTaskStartGo': 1,
         'osViBlack': 1, 'osAiSetNextBuffer': 2, 'osAiSetFrequency': 1, 'osAiGetStatus': 0,
         'osAiGetLength': 0, 'osViGetCurrentFramebuffer': 0, 'osSetEventMesg': 3,
         'osCreateMesgQueue': 3, 'osContInit': 3, 'osPfsIsPlug': 2, 'osPiReadIo': 2,
         'osMotorStop': 1, 'osMotorStart': 1, 'osMotorInit': 3, 'osViSetMode': 1,
         'osStartThread': 1, 'osSetThreadPri': 2, 'osCreateThread': 6, 'osViSetEvent': 3,
         'osSetTimer': 8, 'osPfsInitPak': 3, 'osPfsInit': 3, 'osPfsRepairId': 1,
         'osPfsFindFile': 6, 'osPfsChecker': 1, 'osPfsReadWriteFile': 6, 'osPfsFreeBlocks': 2,
         'osPfsAllocateFile': 7, 'osPfsNumFiles': 3, 'osPfsFileState': 3, 'osPfsDeleteFile': 5}
# out-parameters: arg index -> bytes written there
OUTS = {'osRecvMesg': {1: 4}, 'osContGetReadData': {0: 24}, 'osContInit': {1: 1, 2: 16},
        'osPfsIsPlug': {1: 1}, 'osPiReadIo': {1: 4}, 'osPfsFreeBlocks': {1: 4},
        'osPfsFindFile': {5: 4}, 'osPfsAllocateFile': {6: 4}, 'osPfsNumFiles': {1: 4, 2: 4},
        'osPfsFileState': {2: 32}}
FRAME_WINDOW = 0x4000           # a call's own frames: this far below its entry sp


def call_args(reg, r32, sp, name):
    n = NARGS.get(name, 4)
    return tuple(reg(i) if i < 4 else r32(sp + 4 * i) for i in range(n))


def in_frame(a, sp):
    return sp - FRAME_WINDOW <= a < sp


def same_args(rec, made, cand_sp):
    """rec = (value, in_frame) per argument, from the live run."""
    if len(rec) != len(made):
        return False
    for (v, fr), m in zip(rec, made):
        if fr:
            if not in_frame(m, cand_sp):
                return False
        elif v != m:
            return False
    return True


def fmt_args(args):
    return '(%s)' % ', '.join(('frame' if isinstance(a, tuple) and a[1] else
                               '%X' % (a[0] if isinstance(a, tuple) else a)) for a in args)


def ram_patch(before, after, skip_lo, skip_hi):
    """Runs of bytes that differ, as (physical offset, bytes), outside the
    skipped physical range."""
    out = []
    for off in range(0, len(before), 0x1000):
        x, y = before[off:off + 0x1000], after[off:off + 0x1000]
        if x == y:
            continue
        i = 0
        while i < len(x):
            if x[i] == y[i] or skip_lo <= off + i < skip_hi:
                i += 1
                continue
            j = i
            while j < len(x) and x[j] != y[j] and not (skip_lo <= off + j < skip_hi):
                j += 1
            out.append((off + i, bytes(y[i:j])))
            i = j
    return out


class RecBox(NB.Box):
    """The live box, recording the OS calls made by calls under test."""

    def __init__(self, *a, **k):
        self.recs = []                  # active recordings
        self.pend = {}                  # thread -> the OS call in progress
        super().__init__(*a, **k)

    def snap(self):
        return bytes(self.uc.mem_read(0, NB.RDRAM))

    def on_hle(self, uc, addr, size, data):
        name, fn = self.hle[addr & 0xffffffff]
        t = self.cur
        mine = [r for r in self.recs if r['thread'] is t]
        if mine and name not in NATIVE_OS and name != 'osGetCount' \
                and not (name.startswith('__') and '_to_' in name):
            sp = self.reg('sp')
            args = call_args(self.arg, self.r32, sp, name)
            p = self.pend.get(t)
            if p is None or p['addr'] != addr or p['args'] != args:
                self.pend[t] = dict(name=name, addr=addr, args=args, ra=self.reg('ra'), sp=sp,
                                    ram=self.snap(), recs=mine)
        super().on_hle(uc, addr, size, data)
        self.finish()

    def load(self, t):
        super().load(t)
        self.finish()

    def finish(self):
        t = self.cur
        p = self.pend.get(t)
        if p is None:
            return
        uc = self.uc
        if uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff != p['ra'] or self.reg('sp') != p['sp']:
            return
        del self.pend[t]
        after = self.snap()
        for r in p['recs']:
            if r not in self.recs:
                continue
            lo = (r['sp'] - FRAME_WINDOW) & 0x1FFFFFFF
            hi = r['sp'] & 0x1FFFFFFF
            ev = dict(name=p['name'],
                      args=tuple((v, in_frame(v, r['sp'])) for v in p['args']),
                      patch=ram_patch(p['ram'], after, lo, hi), out=[],
                      v0=uc.reg_read(NB.GPR[2]), v1=uc.reg_read(NB.GPR[3]),
                      f0=uc.reg_read(NB.FPR[0]), f1=uc.reg_read(NB.FPR[1]), count=self.count)
            ok = True
            outs = OUTS.get(p['name'], {})
            for off, blob in ram_patch(p['ram'], after, 0, 1 << 30):
                if not (lo <= off < hi):
                    continue
                # a change inside the call's frames must be a known out-parameter
                hit = [i for i, n in outs.items() if p['args'][i] and
                       (p['args'][i] & 0x1FFFFFFF) <= off < (p['args'][i] & 0x1FFFFFFF) + n]
                if not hit:
                    ok = False
            for i, n in outs.items():
                a = p['args'][i]
                if a and in_frame(a, r['sp']):
                    ev['out'].append((i, after[a & 0x1FFFFFFF:(a & 0x1FFFFFFF) + n]))
            r['events'].append(ev if ok else None)


# ---------------------------------------------------------------- sandbox
SENTINEL = 0x80000400          # return address the sandboxes stop at
BUDGET = int(os.environ.get("N64T3_BUDGET", 200000000))   # instructions one call may take
DEAD_LO = 0x80000400


class Sandbox:
    """A CPU that runs one call from a copy of the live machine.

    The live run is only ever READ: at each call under test its memory and
    registers are copied out to a worker process, where the original body
    runs in one sandbox and the candidate in another, each to a sentinel
    return address, and the two end states are compared.  (Driving one
    Unicorn from inside another's hook crashes; a separate process cannot.)
    Library code runs for real in the sandbox; the few OS services a game
    function may touch without blocking are modelled; anything else makes the
    call uncomparable (counted apart)."""

    def __init__(self, hle_names):
        from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS64, UC_MODE_BIG_ENDIAN
        self.uc = Uc(UC_ARCH_MIPS, UC_MODE_MIPS64 | UC_MODE_BIG_ENDIAN)
        self.uc.ctl_set_cpu_model(M.UC_CPU_MIPS64_R4000)
        self.uc.mem_map(0, NB.RDRAM)
        self.stop = None
        self.count = 0
        for va, name in hle_names.items():
            self.uc.hook_add(UC_HOOK_CODE, self.on_os, begin=NB.sx(va), end=NB.sx(va),
                             user_data=name)

    def reg(self, i):
        return self.uc.reg_read(NB.GPR[i]) & 0xffffffff

    def ret(self, v0=None, v1=None):
        if v0 is not None:
            self.uc.reg_write(NB.GPR[2], NB.sx(v0))
        if v1 is not None:
            self.uc.reg_write(NB.GPR[3], NB.sx(v1))
        self.uc.reg_write(M.UC_MIPS_REG_PC, NB.sx(self.reg(31)))

    def on_os(self, uc, addr, size, name):
        if getattr(self, 'heads', 0) < getattr(self, 'os_from', 0):
            self.stop = 'calls %s before its loop head' % name
            uc.emu_stop()
            return
        if name in NATIVE_OS:
            self.ret()
        elif name == 'osGetCount':
            self.ret(self.count & 0xffffffff)
        elif name.startswith('__') and ('_to_' in name):
            self.convert(name)
        else:
            self.replay(name)

    def replay(self, name):
        """An OS call the live run recorded: check it is the same call, then
        give it the same outcome (see Recorder)."""
        uc = self.uc
        if self.ev >= len(self.events):
            self.stop = 'calls %s (not in the recording)' % name
            uc.emu_stop()
            return
        e = self.events[self.ev]
        if e is None:
            self.stop = 'calls %s (recording not replayable)' % name
            uc.emu_stop()
            return
        sp = self.reg(29)
        args = call_args(lambda i: self.reg(4 + i),
                         lambda a: struct.unpack('>I', bytes(uc.mem_read(a & 0x1FFFFFFF, 4)))[0],
                         sp, e['name'])
        if e['name'] != name or not same_args(e['args'], args, self.entry_sp):
            self.stop = 'OS call %d differs: recorded %s%s, made %s%s' % (
                self.ev, e['name'], fmt_args(e['args']), name, fmt_args(args))
            uc.emu_stop()
            return
        self.ev += 1
        for off, blob in e['patch']:
            uc.mem_write(off, blob)
        for i, blob in e['out']:                     # out-parameters in the caller's frame
            uc.mem_write(args[i] & 0x1FFFFFFF, blob)
        uc.reg_write(NB.GPR[2], e['v0'])
        uc.reg_write(NB.GPR[3], e['v1'])
        uc.reg_write(NB.FPR[0], e['f0'])
        uc.reg_write(NB.FPR[1], e['f1'])
        self.count = e['count']
        uc.reg_write(M.UC_MIPS_REG_PC, NB.sx(self.reg(31)))

    def convert(self, name):
        import struct
        def f32(r):
            return struct.unpack('>f', struct.pack('>I', self.uc.reg_read(NB.FPR[r]) & 0xffffffff))[0]
        def f64(r):
            lo = self.uc.reg_read(NB.FPR[r]) & 0xffffffff
            hi = self.uc.reg_read(NB.FPR[r + 1]) & 0xffffffff
            return struct.unpack('>d', struct.pack('>II', hi, lo))[0]
        if name.endswith('_to_ll') or name.endswith('_to_ull'):
            x = f64(12) if name.startswith('__d') else f32(12)
            v = (int(x) if x == x else 0) & 0xffffffffffffffff
            self.ret(v >> 32, v & 0xffffffff)
            return
        v = (self.reg(4) << 32) | self.reg(5)
        if name.startswith('__ll') and v & (1 << 63):
            v -= 1 << 64
        if name.endswith('_d'):
            hi, lo = struct.unpack('>II', struct.pack('>d', float(v)))
            self.uc.reg_write(NB.FPR[0], lo)
            self.uc.reg_write(NB.FPR[1], hi)
        else:
            self.uc.reg_write(NB.FPR[0], struct.unpack('>I', struct.pack('>f', float(v)))[0])
        self.ret()

    def on_end(self, uc, addr, size, data):
        self.stop = 'returned'
        uc.emu_stop()

    def run(self, ram, regs, pc, count, events=(), stop_pc=None, os_from=0):
        """stop_pc: a loop run -- stop at the second arrival there (the start
        counts as the first for a run that begins at it); os_from: an OS call
        before that many arrivals ends the run (a thread's pre-loop code)."""
        uc = self.uc
        self.events, self.ev = events, 0
        self.heads, self.os_from = 0, os_from
        hook = None
        if stop_pc is not None:
            def at_head(uc_, addr, size, data):
                self.heads += 1
                if self.heads == 2:
                    self.stop = 'looped'
                    uc_.emu_stop()
            hook = uc.hook_add(UC_HOOK_CODE, at_head, begin=NB.sx(stop_pc), end=NB.sx(stop_pc))
        try:
            return self._run(ram, regs, pc, count)
        finally:
            if hook is not None:
                uc.hook_del(hook)

    def _run(self, ram, regs, pc, count):
        uc = self.uc
        self.entry_sp = regs['gpr'][29] & 0xffffffff
        uc.mem_write(0, ram)
        for r, v in zip(NB.GPR[1:], regs['gpr'][1:]):
            uc.reg_write(r, v)
        for r, v in zip(NB.FPR, regs['fpr']):
            uc.reg_write(r, v)
        uc.reg_write(M.UC_MIPS_REG_HI, regs['hi'])
        uc.reg_write(M.UC_MIPS_REG_LO, regs['lo'])
        uc.reg_write(M.UC_MIPS_REG_FCSR, regs['fcsr'])
        uc.reg_write(NB.GPR[31], NB.sx(SENTINEL))
        self.count = count
        self.stop = None
        try:
            uc.emu_start(NB.sx(pc), NB.sx(SENTINEL), count=BUDGET)
        except Exception as e:
            return 'faulted (%s at %08X)' % (e, uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff)
        if self.stop:
            return self.stop
        if uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff == SENTINEL:
            return 'returned'
        return 'did not return within the budget'

    def state(self):
        uc = self.uc
        return (bytes(uc.mem_read(0, NB.RDRAM)),
                dict(gpr=[uc.reg_read(r) for r in NB.GPR], fpr=[uc.reg_read(r) for r in NB.FPR]))


def regs_of(uc):
    return dict(gpr=[uc.reg_read(r) for r in NB.GPR], fpr=[uc.reg_read(r) for r in NB.FPR],
                hi=uc.reg_read(M.UC_MIPS_REG_HI), lo=uc.reg_read(M.UC_MIPS_REG_LO),
                fcsr=uc.reg_read(M.UC_MIPS_REG_FCSR))


def compare(a, b, sp, kind='int', regs=True):
    """-> None if the two end states agree, else a description."""
    ram_a, regs_a = a
    ram_b, regs_b = b
    if ram_a != ram_b:
        # dead stack below the caller's sp, plus the caller's argument home
        # area at sp..sp+0x20, which the O32 convention gives the callee as
        # scratch (IDO spills incoming arguments there)
        dlo, dhi = (sp - DEAD_STACK) & 0x1FFFFFFF, (sp + ARG_HOME) & 0x1FFFFFFF
        # compare page by page, skipping the dead stack below the caller
        if os.environ.get('N64T3_VERBOSE'):
            runs = [(o, b) for o, b in ram_patch(ram_a, ram_b, dlo, dhi) if o >= (DEAD_LO & 0x1FFFFFFF)]
            if runs:
                return '%d runs: %s' % (len(runs), ', '.join(
                    '%08X+%d(%s->%s)' % (0x80000000 | o, len(b), ram_a[o:o + min(len(b), 8)].hex(),
                                         b[:8].hex()) for o, b in runs[:12]))
        for off in range(0, len(ram_a), 0x1000):
            x, y = ram_a[off:off + 0x1000], ram_b[off:off + 0x1000]
            if x == y:
                continue
            for i in range(len(x)):
                pa = off + i
                if x[i] != y[i] and not (dlo <= pa < dhi) and pa >= (DEAD_LO & 0x1FFFFFFF):
                    return 'memory %08X: original %02X, candidate %02X' % (0x80000000 | pa, x[i], y[i])
    if not regs:
        return None
    ret_gpr = {'int': (2,), 'llong': (2, 3)}.get(kind, ())
    ret_fpr = {'float': (0,), 'double': (0, 1)}.get(kind, ())
    for r in ret_gpr + (16, 17, 18, 19, 20, 21, 22, 23, 29, 30):
        if regs_a['gpr'][r] & 0xffffffff != regs_b['gpr'][r] & 0xffffffff:
            return 'register $%d: original %08X, candidate %08X' % (
                r, regs_a['gpr'][r] & 0xffffffff, regs_b['gpr'][r] & 0xffffffff)
    for r in ret_fpr + (20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31):
        if regs_a['fpr'][r] & 0xffffffff != regs_b['fpr'][r] & 0xffffffff:
            return 'register $f%d: original %08X, candidate %08X' % (
                r, regs_a['fpr'][r] & 0xffffffff, regs_b['fpr'][r] & 0xffffffff)
    return None


def returns_of(va):
    """Every `jr $ra` in the original body at va."""
    size = B.function_map()[va]
    rom = open(NB.ROM_PATH, 'rb').read()
    off = va - NB.ENTRY + 0x1000
    return [va + i for i in range(0, size, 4)
            if rom[off + i:off + i + 4] == b'\x03\xe0\x00\x08']


def _sandbox_worker(conn, hle_names):
    a, b = Sandbox(hle_names), Sandbox(hle_names)
    while True:
        job = conn.recv()
        if job is None:
            return
        if job[0] == 'loop':
            _, va, code_va, head_a, head_b, top, regs, count, ram, events = job
            ra_ = a.run(ram, regs, head_a, count, events, stop_pc=head_a)
            if ra_ == 'looped' and a.ev != len(events):
                ra_ = 'replayed %d of %d recorded OS calls' % (a.ev, len(events))
            if ra_ != 'looped':
                conn.send((va, 'blocked', ra_))
                continue
            sa = a.state()
            rb = dict(regs, gpr=list(regs['gpr']))
            rb['gpr'][29] = NB.sx(top)
            rb_ = b.run(ram, rb, code_va, count, events, stop_pc=head_b, os_from=1)
            if rb_ == 'looped' and b.ev != len(events):
                rb_ = 'made %d of the %d OS calls' % (b.ev, len(events))
            if rb_ != 'looped':
                conn.send((va, 'divergent', 'candidate %s' % rb_))
                continue
            why = compare(sa, b.state(), top, 'void', regs=False)
            conn.send((va, 'divergent' if why else 'equal', why))
            continue
        va, code_va, kind, sp, regs, count, ram, events = job
        ra_ = a.run(ram, regs, va, count, events)
        if ra_ == 'returned' and a.ev != len(events):
            ra_ = 'replayed %d of %d recorded OS calls' % (a.ev, len(events))
        if ra_ != 'returned':
            conn.send((va, 'blocked', ra_))
            continue
        sa = a.state()
        rb_ = b.run(ram, regs, code_va, count, events)
        if rb_ == 'returned' and b.ev != len(events):
            rb_ = 'made %d of the %d OS calls' % (b.ev, len(events))
        if rb_ != 'returned':
            conn.send((va, 'divergent', 'candidate %s' % rb_))
            continue
        why = compare(sa, b.state(), sp, kind)
        conn.send((va, 'divergent' if why else 'equal', why))


class Lockstep:
    """Compares candidates with the original at every real call of a live run."""

    def __init__(self, box, targets, cap=None):
        import multiprocessing as mp
        self.box = box
        self.cap = cap or MAX_CALLS
        self.t = {}
        self.hle_names = {va: name for va, (name, fn) in box.hle.items()}
        self.ctx = mp.get_context('spawn')
        self.spawn()
        self.inflight = []
        code = TEST_CODE
        for va, (cblob, dblocks, sha) in targets.items():
            self.t[va] = dict(code=code, sha=sha, compared=0, divergent=0, blocked=0,
                              first=None, calls=0, kind=return_kind(va), sent=0)
            box.uc.mem_write(code & 0x1FFFFFFF, cblob)
            for dva, blob in dblocks:
                box.uc.mem_write(dva & 0x1FFFFFFF, blob)
            if va in THREAD_LOOPS:
                t = self.t[va]
                t['head'] = loop_head(rom_words(va), va)
                t['chead'] = loop_head(list(struct.unpack('>%dI' % (len(cblob) // 4), cblob)), code)
                t['fsize'] = rom_frame(va)
                box.uc.hook_add(UC_HOOK_CODE, self.on_head, begin=NB.sx(t['head']),
                                end=NB.sx(t['head']), user_data=va)
            else:
                box.uc.hook_add(UC_HOOK_CODE, self.on_entry, begin=NB.sx(va), end=NB.sx(va),
                                user_data=va)
                # the call is sent when it returns, with the OS calls it made
                for pc in returns_of(va):
                    box.uc.hook_add(UC_HOOK_CODE, self.on_return, begin=NB.sx(pc), end=NB.sx(pc),
                                    user_data=va)
            code += (len(cblob) + 15) & ~15

    def spawn(self):
        self.conn, child = self.ctx.Pipe()
        self.proc = self.ctx.Process(target=_sandbox_worker, args=(child, self.hle_names),
                                     daemon=True)
        self.proc.start()

    def on_entry(self, uc, addr, size, va):
        t = self.t[va]
        t['calls'] += 1
        if t['sent'] >= self.cap and t['calls'] % 97:
            return
        if t['sent'] >= self.cap * 4:
            return
        box = self.box
        if any(r['va'] == va and r['thread'] is box.cur for r in box.recs):
            return                          # recursion: the outer call covers it
        t['sent'] += 1
        sp = uc.reg_read(NB.GPR[29]) & 0xffffffff
        job = [va, t['code'], t['kind'], sp, regs_of(uc), box.count,
               bytes(uc.mem_read(0, NB.RDRAM))]
        box.recs.append(dict(va=va, thread=box.cur, sp=sp,
                             ra=uc.reg_read(NB.GPR[31]) & 0xffffffff, events=[], job=job,
                             frame=box.frame))

    def on_head(self, uc, addr, size, va):
        """A thread loop arrives at its head: the iteration being recorded
        (if any) ends and is sent; the next one starts."""
        box, t = self.box, self.t[va]
        for r in box.recs:
            if r['va'] == va and r['thread'] is box.cur:
                box.recs.remove(r)
                self.send(tuple(r['job']) + (r['events'],), r['frame'])
                break
        t['calls'] += 1
        if t['sent'] >= self.cap and t['calls'] % 97:
            return
        if t['sent'] >= self.cap * 4:
            return
        t['sent'] += 1
        sp = uc.reg_read(NB.GPR[29]) & 0xffffffff
        top = sp + t['fsize']
        job = ['loop', va, t['code'], t['head'], t['chead'], top, regs_of(uc), box.count,
               bytes(uc.mem_read(0, NB.RDRAM))]
        box.recs.append(dict(va=va, thread=box.cur, sp=top, ra=None, events=[], job=job,
                             frame=box.frame))

    def on_return(self, uc, addr, size, va):
        box = self.box
        ra = uc.reg_read(NB.GPR[31]) & 0xffffffff
        for r in box.recs:
            if r['va'] == va and r['thread'] is box.cur and r['ra'] == ra:
                box.recs.remove(r)
                self.send(tuple(r['job']) + (r['events'],), r['frame'])
                return

    def send(self, job, frame):
        try:
            self.conn.send(job)
            self.inflight.append((job[1] if job[0] == 'loop' else job[0], frame))
        except (BrokenPipeError, EOFError, OSError):
            self.crashed()
            return
        while len(self.inflight) > 4:
            self.collect(block=True)
        self.collect(block=False)

    def collect(self, block):
        while self.inflight and (block or self.conn.poll()):
            try:
                va, what, why = self.conn.recv()
            except (EOFError, OSError):
                self.crashed()
                return
            _, frame = self.inflight.pop(0)
            t = self.t[va]
            if what == 'blocked':
                t['blocked'] += 1
            else:
                t['compared'] += 1
                if what == 'divergent':
                    t['divergent'] += 1
                    t['first'] = t['first'] or 'frame %d: %s' % (frame, why)
            block = False

    def crashed(self):
        # the sandbox process died on the oldest job in flight: that call's
        # candidate (or original) took Unicorn down -- never a pass
        if self.inflight:
            va, frame = self.inflight.pop(0)
            t = self.t[va]
            t['compared'] += 1
            t['divergent'] += 1
            t['first'] = t['first'] or 'frame %d: the sandbox crashed on this call' % frame
        self.inflight = []
        self.spawn()

    def close(self):
        self.collect(block=True)
        try:
            self.conn.send(None)
        except Exception:
            pass


_calls = None


def NB_calls():
    global _calls
    if _calls is None:
        import n64rom
        _calls = n64rom.calls
    return _calls


def verdict(t):
    if t['divergent']:
        return 'DIVERGENT'
    if t['compared'] == 0:
        return 'UNCOVERED'
    return 'EQUIVALENT'


CAP = None


def _live_worker(job):
    global CAND_DIR, CAP
    sc, vas, cand, cap = job
    CAND_DIR, CAP = cand, cap
    targets, code_va, data_va = {}, TEST_CODE, TEST_DATA
    for va in vas:
        try:
            code, data, sha = link_candidate(va, code_va, data_va)
        except L.LinkError:
            continue
        targets[va] = (code, data, sha)
        code_va += (len(code) + 15) & ~15
        data_va += sum((len(b) + 15) & ~15 for _, b in data) + 16
    vas = list(targets)
    box = RecBox(script=os.path.join(SCRIPTS, sc))
    ls = Lockstep(box, targets, cap=CAP)
    r = box.run(script_frames(sc))
    ls.close()
    return sc, r, {va: dict((k, ls.t[va][k]) for k in ('compared', 'divergent', 'blocked', 'first', 'sha'))
                   for va in vas}


def run_live(vas):
    from concurrent.futures import ProcessPoolExecutor
    agg = {va: dict(compared=0, divergent=0, blocked=0, first=None, sha=None, scripts=[])
           for va in vas}
    with ProcessPoolExecutor(min(14, len(scripts())), initializer=orphanguard.watch_parent) as ex:
        for sc, r, res in ex.map(_live_worker, [(sc, vas, CAND_DIR, CAP) for sc in scripts()]):
            if r != 'frames':
                print('  %s: run ended early: %s' % (sc, r))
            for va in vas:
                if va not in res:
                    continue
                t, g = res[va], agg[va]
                for k in ('compared', 'divergent', 'blocked'):
                    g[k] += t[k]
                g['first'] = g['first'] or (t['first'] and '%s %s' % (sc, t['first']))
                g['sha'] = t['sha']
                g['scripts'].append('%s:%d' % (sc[:-4], t['compared']))
    out = LIVE if not CAND_DIR else os.path.join(B.OUT, 'cand_live.csv')
    rows = {}
    if os.path.exists(out):
        rows = {r['va']: r for r in csv.DictReader(open(out))}
    today = datetime.date.today().isoformat()
    for va, g in agg.items():
        v = verdict(g)
        rows['%08X' % va] = dict(va='%08X' % va, verdict=v, compared=g['compared'],
                                 divergent=g['divergent'], blocked=g['blocked'],
                                 src=g['sha'], date=today, scripts=' '.join(g['scripts']),
                                 first=g['first'] or '')
        print('%08X %-10s compared %d, divergent %d, blocked %d  %s'
              % (va, v, g['compared'], g['divergent'], g['blocked'], g['first'] or ''))
    with open(out, 'w', newline='') as f:
        w = csv.DictWriter(f, ['va', 'verdict', 'compared', 'divergent', 'blocked', 'src',
                               'date', 'scripts', 'first'], lineterminator='\n')
        w.writeheader()
        w.writerows(sorted(rows.values(), key=lambda r: r['va']))


def _image_worker(job):
    sc, img, extra = job
    a = NB.Box(script=os.path.join(SCRIPTS, sc))
    ra = a.run(script_frames(sc))
    b = NB.Box(script=os.path.join(SCRIPTS, sc), image=img, extra=extra)
    rb = b.run(script_frames(sc))
    first = None
    for x, y in zip(a.log, b.log):
        if x != y:
            first = 'frame %d %s' % (x[0], x[1])
            break
    if first is None and (len(a.log) != len(b.log) or ra != rb):
        first = 'run ended differently (%s / %s)' % (ra, rb)
    return sc, first, a.frame, ra


def t3_tagged():
    out = set()
    for f in B.all_sources():
        for m in re.finditer(r'@t3 (0x[0-9A-Fa-f]{8})', open(f).read()):
            out.add(int(m.group(1), 16))
    return out


def run_image(with_vas=()):
    from concurrent.futures import ProcessPoolExecutor
    # every T3 body: an @t3 tag in the source and an EQUIVALENT row (an
    # EQUIVALENT draft that has not been certified is not part of the image)
    cert = (IMG.t3_certified() & t3_tagged()) | set(with_vas)
    img, extra, rep = _build_only(cert)
    results = []
    with ProcessPoolExecutor(min(14, len(scripts())), initializer=orphanguard.watch_parent) as ex:
        for sc, first, frames, ra in ex.map(_image_worker, [(sc, img, extra) for sc in scripts()]):
            results.append((sc, first, frames))
            print('%s: %s over %d frames%s' % (sc, 'IDENTICAL' if first is None else 'DIFFERS at ' + first,
                                               frames, '' if ra == 'frames' else ' (original: %s)' % ra))
    verdict_ = 'IDENTICAL' if all(r[1] is None for r in results) else 'DIFFERENT'
    shas = {'%08X' % va: source_of(va)[2] for va in sorted(cert)}
    rows = list(csv.DictReader(open(WHOLE))) if os.path.exists(WHOLE) else []
    rows.append(dict(date=datetime.datetime.now().isoformat(timespec='seconds'),
                     verdict=verdict_, bodies=' '.join('%s:%s' % kv for kv in sorted(shas.items())),
                     scripts=' '.join('%s:%d' % (r[0][:-4], r[2]) for r in results),
                     first='; '.join('%s %s' % (r[0], r[1]) for r in results if r[1])))
    with open(WHOLE, 'w', newline='') as f:
        w = csv.DictWriter(f, ['date', 'verdict', 'bodies', 'scripts', 'first'], lineterminator='\n')
        w.writeheader()
        w.writerows(rows)
    print('A7:', verdict_, '(%d T3 bodies placed)' % len(cert))
    return verdict_


def build_with(vas):
    """The M1 image with exactly the given non-exact bodies placed."""
    return IMG.build(t3=True, all_tagged=False, only=None) if not vas else _build_only(vas)


def _build_only(vas):
    img, extra, rep = IMG.build(t3=False)
    img = bytearray(img)
    fmap = B.function_map()
    code_va, data_va = IMG.ANNEX_CODE, IMG.ANNEX_DATA
    for va in sorted(vas):
        code, data, sha = link_candidate(va, va, data_va)
        off = va - B.BASE
        if len(code) > fmap[va]:
            code, data, sha = link_candidate(va, code_va, data_va)
            extra.append((code_va, code))
            img[off:off + 8] = (0x08000000 | ((code_va >> 2) & 0x03FFFFFF)).to_bytes(4, 'big') + bytes(4)
            code_va += (len(code) + 15) & ~15
        else:
            img[off:off + fmap[va]] = code.ljust(fmap[va], b'\0')
        for dva, blob in data:
            extra.append((dva, blob))
            data_va = max(data_va, (dva + len(blob) + 15) & ~15)
    return bytes(img), extra, rep


PASS = re.compile(r'@t4-pass\s+(0x[0-9A-Fa-f]{8})\s+\S+\s+(\S+)\s+compiles\s+(\d+)\s+best\s+(\d+)\s+moved\s+(\d+)')


def qualify(va):
    f, name, sha = source_of(va)
    ok = True

    def gate(n, passed, why):
        nonlocal ok
        ok &= passed
        print('  Gate %-3s %s  %s' % (n, 'pass' if passed else 'FAIL', why))
    print('%08X %s  (%s)' % (va, name, f and os.path.relpath(f, ROOT)))
    if f is None:
        print('  no source')
        return False
    src = open(f).read()
    body = function_text(src, name)
    idx = src.find('@implements 0x%08X' % va)
    head = src[max(0, idx - 1500):idx]
    gate('0', 'WHAT IT DOES:' in head and ('@t3 0x%08X' % va) in src
         and not re.search(r'\b(TODO|FIXME|XXX)\b', body),
         'description, @t3 tag, no unfinished markers')
    live = {r['va']: r for r in csv.DictReader(open(LIVE))} if os.path.exists(LIVE) else {}
    r = live.get('%08X' % va)
    gate('A5', bool(r) and r['verdict'] == 'EQUIVALENT' and r['src'] == sha,
         r and '%s over %s calls (%s)%s' % (r['verdict'], r['compared'], r['scripts'],
                                            '' if r['src'] == sha else ', STALE: source changed')
         or 'no live-oracle row')
    whole = list(csv.DictReader(open(WHOLE))) if os.path.exists(WHOLE) else []
    last = next((w for w in reversed(whole) if ('%08X:' % va) in w['bodies']), None)
    cur = last and ('%08X:%s' % (va, sha)) in last['bodies']
    gate('A7', bool(last) and last['verdict'] == 'IDENTICAL' and bool(cur),
         last and '%s on %s%s' % (last['verdict'], last['date'], '' if cur else ', STALE') or
         'never placed in a whole-image run')
    passes = [m for m in PASS.findall(src) if int(m[0], 16) == va]
    good = [m for m in passes if int(m[2]) >= 10]
    gate('B', len(good) >= 2 and good[-1][4] == '0',
         '%d counted @t4-pass lines' % len(good))
    print('  =>', 'QUALIFIES for T3' if ok else 'not T3')
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--live', nargs='+')
    ap.add_argument('--image', action='store_true')
    ap.add_argument('--with', dest='with_', nargs='*', default=[])
    ap.add_argument('--qualify', nargs='+')
    ap.add_argument('--cand', action='store_true',
                    help='--live on build/tgrally/n64/cand drafts (a survey; writes build/tgrally/n64/cand_live.csv)')
    ap.add_argument('--cap', type=int,
                    help='calls compared in full per script before sampling (default %d)' % MAX_CALLS)
    a = ap.parse_args()
    global CAND_DIR
    global CAP
    if a.cap:
        CAP = a.cap
    if a.cand:
        CAND_DIR = os.path.join(B.OUT, 'cand')
        CAP = 12                        # a survey: a dozen calls per script
    if a.live:
        run_live([int(v, 16) for v in a.live])
    if a.image:
        run_image([int(v, 16) for v in a.with_])
    if a.qualify:
        for v in a.qualify:
            qualify(int(v, 16))


if __name__ == '__main__':
    main()
