"""c2emu.py -- run the VC5 backend (tools/msvc5/bin/C2.EXE) under Unicorn on a captured IL set.

usage: .venv/bin/python tools/c2emu.py IL_DIR OUT_OBJ     (env knobs below)

Output is byte-identical to the real backend (verified 2026-09-28 on
BrDlVtxLitDecal, two TU states; only the COFF timestamp differs), in ~0.2 s.

IL capture: build tools/c2emu_capwrap.c as C2.EXE in a copy of tools/msvc5/bin
with the real backend renamed C2REAL.EXE; every compile then drops its IL set
(<base>ex/gl/in/sy) in build/match/t3d/ilcap.  Copy those four files into
IL_DIR as ex, gl, in, sy.

Instrumentation (all env vars, all optional):
  C2TRACE=f        basic-block trace (uint32 array) of the backend's .text
  C2WATCH=lo:len   log writes to a guest range with the backend call stack
  C2CMPLOG=f       pickle every call of the operand-order comparator 0x4046CC
  C2FLIP=a:b,...   invert that comparator for the given node pairs
  C2RANK=n:r,...   force comparator order by rank
  C2KEYLOG=n,...   log operand-sort keys (0x409EA4) with children
  C2SCHED=f        pickle list-scheduler ready lists / issues (0x43CAF6)
  C2RDYLOG=f       pickle nodes entering the ready list (0x43CA68)
  C2PRIO=ins:p     override a node's scheduler priority (C2PRIOMIN=node floor)
  C2READY=ins:c    delay a node's ready cycle
  C2LAT=a:b:l      set a DAG edge latency (at 0x43BE83)
  C2LATLOG=1       log edge latency computation (0x43B974); C2LATALL=1 for all
  C2SUCC=ins,...   dump scheduler successors/heights
  C2EMITLOG=f      pickle the final emitted instruction nodes
  C2HEAPSKEW/C2MALLOCPAD  perturb the CRT heap (outputs proved insensitive)
Node addresses are deterministic for a given IL (bump allocator)."""
import os
import struct
import sys
import time as _time

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED, UC_HOOK_MEM_WRITE, UcError
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FS,
                               UC_X86_REG_GDTR, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDX)

EXE = os.environ.get('C2EMU_EXE') or os.path.join(os.path.dirname(os.path.abspath(__file__)), 'msvc5', 'bin', 'C2.EXE')
THUNK = 0x70000000
DATA = 0x71000000
HEAP = 0x20000000
STACK = 0x0f000000
STACK_SZ = 0x200000
TEB = 0x7ffd0000
GDT = 0x7ffc0000


def _gdt_entry(base, limit, access, flags):
    return struct.pack('<Q', (limit & 0xFFFF) | ((base & 0xFFFFFF) << 16) |
                       (access << 40) | (((limit >> 16) & 0xF) << 48) |
                       (flags << 52) | (((base >> 24) & 0xFF) << 56))


class Exit(Exception):
    pass


class C2(object):
    def __init__(self, il_dir, out_obj, flags):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.il_dir, self.out_obj, self.flags = il_dir, out_obj, flags
        self.heap = HEAP + int(os.environ.get('C2HEAPSKEW', '0'), 0)
        self.malloc_pad = int(os.environ.get('C2MALLOCPAD', '0'), 0)
        self.data = DATA
        self.files = {}          # FILE* -> dict
        self.fds = {}            # fd -> FILE*
        self.next_fd = 3
        self.stdout = []
        self.exit_code = None
        self.hooks_extra = []
        self._load()

    # ---- memory helpers
    def rd32(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def wr32(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def rdstr(self, a, n=None):
        out = bytearray()
        while True:
            c = self.uc.mem_read(a + len(out), 64)
            z = bytes(c).find(b'\0')
            if z >= 0:
                out += c[:z]
                break
            out += c
        return bytes(out)

    def alloc(self, n, align=8):
        a = (self.heap + align - 1) & ~(align - 1)
        self.heap = a + max(n, 1)
        return a

    def dalloc(self, n):
        a = (self.data + 15) & ~15
        self.data = a + n
        return a

    def cstr(self, s):
        if isinstance(s, str):
            s = s.encode('latin-1')
        a = self.alloc(len(s) + 1)
        self.uc.mem_write(a, s + b'\0')
        return a

    # ---- loading
    def _load(self):
        b = open(EXE, 'rb').read()
        pe = struct.unpack_from('<I', b, 0x3c)[0]
        nsec = struct.unpack_from('<H', b, pe + 6)[0]
        osz = struct.unpack_from('<H', b, pe + 20)[0]
        opt = pe + 24
        self.base = struct.unpack_from('<I', b, opt + 28)[0]
        self.ep = self.base + struct.unpack_from('<I', b, opt + 16)[0]
        size = struct.unpack_from('<I', b, opt + 56)[0]
        uc = self.uc
        uc.mem_map(self.base, (size + 0xfff) & ~0xfff)
        uc.mem_write(self.base, b[:0x1000])
        secs = [struct.unpack_from('<8sIIII', b, opt + osz + 40 * k) for k in range(nsec)]
        for n, vs, va, rs, ro in secs:
            uc.mem_write(self.base + va, b[ro:ro + min(rs, max(vs, rs))])
        uc.mem_map(THUNK, 0x100000)
        uc.mem_map(DATA, 0x100000)
        uc.mem_map(HEAP, 0x10000000)
        uc.mem_map(STACK, STACK_SZ)
        uc.mem_map(TEB, 0x10000)
        uc.mem_map(GDT, 0x1000)
        # FS -> TEB
        gdt = (b'\0' * 8 + _gdt_entry(0, 0xfffff, 0x9b, 0xc) + _gdt_entry(0, 0xfffff, 0x93, 0xc)
               + _gdt_entry(TEB, 0xfff, 0x93, 0x4))
        uc.mem_write(GDT, gdt)
        uc.reg_write(UC_X86_REG_GDTR, (0, GDT, len(gdt) - 1, 0x0))
        from unicorn.x86_const import UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES
        uc.reg_write(UC_X86_REG_SS, 2 << 3)
        uc.reg_write(UC_X86_REG_DS, 2 << 3)
        uc.reg_write(UC_X86_REG_ES, 2 << 3)
        uc.reg_write(UC_X86_REG_FS, 3 << 3)
        self.wr32(TEB, 0xffffffff)
        self.wr32(TEB + 0x18, TEB)
        self.wr32(TEB + 4, STACK + STACK_SZ)
        self.wr32(TEB + 8, STACK)
        # imports
        self.thunks = {}
        self.tnext = THUNK
        imp = struct.unpack_from('<II', b, opt + 96 + 8)[0]

        def r2o(rva):
            for n, vs, va, rs, ro in secs:
                if va <= rva < va + max(vs, rs):
                    return rva - va + ro
        o = r2o(imp)
        while True:
            ilt, ts, fc, name, iat = struct.unpack_from('<IIIII', b, o)
            o += 20
            if not name:
                break
            dll = b[r2o(name):b.index(b'\0', r2o(name))].decode().lower()
            p = r2o(ilt or iat)
            k = 0
            while True:
                e = struct.unpack_from('<I', b, p)[0]
                p += 4
                if not e:
                    break
                q = r2o(e) + 2
                fn = b[q:b.index(b'\0', q)].decode()
                self.wr32(self.base + iat + 4 * k, self._bind(dll, fn))
                k += 1
        uc.hook_add(UC_HOOK_CODE, self._on_thunk, begin=THUNK, end=THUNK + 0xfffff)
        uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._on_unmapped)

    def _bind(self, dll, fn):
        if fn == '_adjust_fdiv':
            a = self.dalloc(4)
            self.wr32(a, 0)
            return a
        a = self.tnext
        self.tnext += 16
        impl = getattr(self, 'f_' + fn, None)
        stdcall = dll in ('kernel32.dll', 'user32.dll')
        nargs = {'GetModuleHandleA': 1, 'SetConsoleCtrlHandler': 2, 'LoadStringA': 4}.get(fn, 0)
        if fn == '_initterm':
            # guest loop: call every non-null pointer in [begin, end)
            code = bytes.fromhex('5657' '8b74240c' '8b7c2410' '39fe' '730d' '8b06' '85c0' '7402' 'ffd0'
                                 '83c604' 'ebef' '5f5e' 'c3')
            self.uc.mem_write(a, code)
            self.tnext += 32
            return a
        self.uc.mem_write(a, b'\xc2' + struct.pack('<H', 4 * nargs) if stdcall and nargs else b'\xc3')
        self.thunks[a] = (dll, fn, impl)
        return a

    def _on_unmapped(self, uc, access, addr, size, value, ud):
        eip = uc.reg_read(UC_X86_REG_EIP)
        print('UNMAPPED access %d at %08x size %d eip %08x' % (access, addr, size, eip), file=sys.stderr)
        return False

    def _on_thunk(self, uc, addr, size, ud):
        t = self.thunks.get(addr)
        if t is None:
            return
        dll, fn, impl = t
        esp = uc.reg_read(UC_X86_REG_ESP)
        args = [self.rd32(esp + 4 + 4 * i) for i in range(12)]
        if impl is None:
            print('UNMODELLED %s!%s args %s' % (dll, fn, ' '.join('%x' % x for x in args[:4])), file=sys.stderr)
            uc.emu_stop()
            self.exit_code = 'unmodelled ' + fn
            return
        try:
            r = impl(args, esp)
        except Exit:
            uc.emu_stop()
            return
        if r is not None:
            uc.reg_write(UC_X86_REG_EAX, r & 0xffffffff)

    # ---- run
    def run(self):
        uc = self.uc
        if os.environ.get('C2EMITLOG'):
            from unicorn.x86_const import UC_X86_REG_EBX
            self.emitlog = []
            def _e(uc_, addr, size, ud):
                n = uc_.reg_read(UC_X86_REG_EBX)
                self.emitlog.append((n, bytes(uc_.mem_read(n, 0x40))))
            uc.hook_add(UC_HOOK_CODE, _e, begin=0x43f88b, end=0x43f88b)
        if os.environ.get('C2PTRW'):
            want = set(int(x, 16) for x in os.environ['C2PTRW'].split(','))
            self.ptrw = []
            def _pw(uc_, access, addr, size, value, ud):
                if size == 4 and (value & 0xffffffff) in want:
                    esp = uc_.reg_read(UC_X86_REG_ESP)
                    st = [self.rd32(esp + 4 * k) for k in range(48)]
                    rets = ['%x' % v for v in st if self.base + 0x1000 <= v < self.base + 0x8c000]
                    self.ptrw.append('%08x <- %08x eip %08x  %s' % (addr, value, uc_.reg_read(UC_X86_REG_EIP), ' '.join(rets[:10])))
            uc.hook_add(UC_HOOK_MEM_WRITE, _pw, begin=HEAP, end=HEAP + 0x0fffffff)
        if os.environ.get('C2FLIP'):
            flips = set()
            for pr in os.environ['C2FLIP'].split(','):
                if pr:
                    x_, y_ = [int(v, 16) for v in pr.split(':')]
                    flips.add((x_, y_)); flips.add((y_, x_))
            def _f(uc_, addr, size, ud):
                a_ = uc_.reg_read(UC_X86_REG_ECX); b_ = uc_.reg_read(UC_X86_REG_EDX)
                if (a_, b_) in flips:
                    ka = self.rd32(a_ + 0xc); kb = self.rd32(b_ + 0xc)
                    r = 0 if ka == kb else (1 if ka < kb else 0xffffffff)
                    r = {1: 0xffffffff, 0xffffffff: 1, 0: 0}[r]
                    uc_.reg_write(UC_X86_REG_EAX, r)
                    esp_ = uc_.reg_read(UC_X86_REG_ESP)
                    uc_.reg_write(UC_X86_REG_EIP, self.rd32(esp_))
                    uc_.reg_write(UC_X86_REG_ESP, esp_ + 4)
            uc.hook_add(UC_HOOK_CODE, _f, begin=0x4046cc, end=0x4046cc)
        if os.environ.get('C2PRIO'):
            pri = {}
            for pr in os.environ['C2PRIO'].split(','):
                x_, y_ = pr.split(':'); pri[int(x_, 16)] = int(y_, 16)
            def _pp(uc_, addr, size, ud):
                n_ = uc_.reg_read(UC_X86_REG_ECX)
                ins = self.rd32(n_ + 0x1c)
                lo_ = int(os.environ.get('C2PRIOMIN', '0'), 16)
                if ins in pri and n_ >= lo_:
                    self.wr32(n_ + 0x2c, pri[ins])
            uc.hook_add(UC_HOOK_CODE, _pp, begin=0x43ca68, end=0x43ca68)
        if os.environ.get('C2SCHED'):
            from unicorn.x86_const import UC_X86_REG_ESI
            self.sched = []
            def _pick(uc_, addr, size, ud):
                cyc = uc_.reg_read(UC_X86_REG_ECX)
                lst = []
                n_ = self.rd32(0x49b36c)
                while n_ and len(lst) < 60:
                    lst.append((self.rd32(n_ + 0x1c), self.rd32(n_ + 0x2c), self.rd32(n_ + 0x30),
                                struct.unpack('<H', bytes(uc_.mem_read(n_ + 0x36, 2)))[0]))
                    n_ = self.rd32(n_ + 0x10)
                self.sched.append(('R', cyc, lst))
            uc.hook_add(UC_HOOK_CODE, _pick, begin=0x43caf6, end=0x43caf6)
            def _iss(uc_, addr, size, ud):
                s_ = uc_.reg_read(UC_X86_REG_ESI)
                self.sched.append(('I', self.rd32(0x49b330), self.rd32(s_ + 0x1c)))
            uc.hook_add(UC_HOOK_CODE, _iss, begin=0x43c9e0, end=0x43c9e0)
        if os.environ.get('C2SUCC'):
            want_ = set(int(x, 16) for x in os.environ['C2SUCC'].split(','))
            self.succ = []
            def _sc(uc_, addr, size, ud):
                n_ = uc_.reg_read(UC_X86_REG_ECX)
                ins = self.rd32(n_ + 0x1c)
                if ins in want_ and n_ >= 0x20060000:
                    out_ = []
                    e_ = self.rd32(n_ + 0xc)
                    while e_ and len(out_) < 20:
                        sn = self.rd32(e_ + 0xc)
                        lat = struct.unpack('<H', bytes(uc_.mem_read(e_ + 0x14, 2)))[0]
                        out_.append((self.rd32(sn + 0x1c), struct.unpack('<H', bytes(uc_.mem_read(sn + 0x34, 2)))[0], lat, '%08x' % e_))
                        e_ = self.rd32(e_)
                    self.succ.append((ins, self.rd32(n_ + 0x2c), out_))
            uc.hook_add(UC_HOOK_CODE, _sc, begin=0x43ca68, end=0x43ca68)
        if os.environ.get('C2W'):
            def _wt(uc_, addr, size, ud):
                pw = self.rd32(0x49c050)
                sys.stderr.write('WEIGHTS %08x %s\n' % (pw, [struct.unpack('<i', struct.pack('<I', self.rd32(pw + 4 * k)))[0] for k in range(8)]))
            uc.hook_add(UC_HOOK_CODE, _wt, begin=0x43be83, end=0x43be83)
        if os.environ.get('C2READY'):
            rdy = {}
            for pr in os.environ['C2READY'].split(','):
                x_, y_ = pr.split(':'); rdy[int(x_, 16)] = int(y_, 16)
            def _ry(uc_, addr, size, ud):
                n_ = uc_.reg_read(UC_X86_REG_ECX)
                ins = self.rd32(n_ + 0x1c)
                if ins in rdy and n_ >= int(os.environ.get('C2PRIOMIN', '0'), 16) and self.rd32(n_ + 0x30) < rdy[ins]:
                    self.wr32(n_ + 0x30, rdy[ins])
            uc.hook_add(UC_HOOK_CODE, _ry, begin=0x43ca68, end=0x43ca68)
        if os.environ.get('C2LAT'):
            lats = {}
            for pr in os.environ['C2LAT'].split(','):
                a_, b_, l_ = pr.split(':'); lats[(int(a_, 16), int(b_, 16))] = int(l_)
            def _lt(uc_, addr, size, ud):
                blk = uc_.reg_read(UC_X86_REG_ECX)
                first = self.rd32(blk); last = self.rd32(blk + 4)
                n_ = first; guard = 0
                while n_ and guard < 5000:
                    guard += 1
                    ins = self.rd32(n_ + 0x1c)
                    e_ = self.rd32(n_ + 0xc)
                    while e_:
                        sn = self.rd32(e_ + 0xc)
                        key = (ins, self.rd32(sn + 0x1c))
                        if key in lats and n_ >= int(os.environ.get('C2PRIOMIN', '0'), 16):
                            uc_.mem_write(e_ + 0x14, struct.pack('<H', lats[key]))
                        e_ = self.rd32(e_)
                    if n_ == last:
                        break
                    n_ = self.rd32(n_)
            uc.hook_add(UC_HOOK_CODE, _lt, begin=0x43be83, end=0x43be83)
        if os.environ.get('C2LATLOG'):
            self.latlog = []
            def _ll(uc_, addr, size, ud):
                e_ = uc_.reg_read(UC_X86_REG_ECX)
                esp_ = uc_.reg_read(UC_X86_REG_ESP)
                p2 = uc_.reg_read(UC_X86_REG_EDX); p3 = self.rd32(esp_ + 4)
                pn = self.rd32(e_ + 8); sn = self.rd32(e_ + 0xc)
                pi = self.rd32(pn + 0x1c); si = self.rd32(sn + 0x1c)
                self.latlog.append((pi, si, p2, p3, self.rd32(pi + 4), self.rd32(si + 4), bytes(uc_.mem_read(pn + 0x38, 1))[0]))
            uc.hook_add(UC_HOOK_CODE, _ll, begin=0x43b974, end=0x43b974)
        if os.environ.get('C2KEYLOG'):
            wantk = set(int(x, 16) for x in os.environ['C2KEYLOG'].split(','))
            self.keylog = []
            def _kl(uc_, addr, size, ud):
                # at 0x409ea4: mov [param_1+0xc], eax ; find param_1 via esi/edi/ebx candidates
                from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBX, UC_X86_REG_EBP
                for rr in (UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX):
                    n_ = uc_.reg_read(rr)
                    if n_ in wantk:
                        ch = []
                        c_ = self.rd32(n_ + 0x18)
                        while c_ and len(ch) < 6:
                            ch.append((c_, bytes(uc_.mem_read(c_ + 8, 1))[0], self.rd32(c_ + 0xc), self.rd32(c_ + 0x14), self.rd32(c_ + 0x20)))
                            c_ = self.rd32(c_)
                        self.keylog.append((n_, uc_.reg_read(UC_X86_REG_EAX), ch))
            uc.hook_add(UC_HOOK_CODE, _kl, begin=0x409ea4, end=0x409ea4)
        if os.environ.get('C2RDYLOG'):
            self.rdylog = []
            def _r(uc_, addr, size, ud):
                n_ = uc_.reg_read(UC_X86_REG_ECX)
                d_ = bytes(uc_.mem_read(n_, 0x40))
                ins = self.rd32(n_ + 0x1c)
                self.rdylog.append((n_, ins, d_))
            uc.hook_add(UC_HOOK_CODE, _r, begin=0x43ca68, end=0x43ca68)
        if os.environ.get('C2RANK'):
            rank = {}
            for pr in os.environ['C2RANK'].split(','):
                x_, y_ = pr.split(':'); rank[int(x_, 16)] = int(y_)
            def _rk(uc_, addr, size, ud):
                a_ = uc_.reg_read(UC_X86_REG_ECX); b_ = uc_.reg_read(UC_X86_REG_EDX)
                if a_ in rank and b_ in rank:
                    r = 0xffffffff if rank[a_] > rank[b_] else (1 if rank[a_] < rank[b_] else 0)
                    uc_.reg_write(UC_X86_REG_EAX, r)
                    esp_ = uc_.reg_read(UC_X86_REG_ESP)
                    uc_.reg_write(UC_X86_REG_EIP, self.rd32(esp_))
                    uc_.reg_write(UC_X86_REG_ESP, esp_ + 4)
            uc.hook_add(UC_HOOK_CODE, _rk, begin=0x4046cc, end=0x4046cc)
        if os.environ.get('C2CMPLOG'):
            self.cmplog = []
            def _c(uc_, addr, size, ud):
                a_ = uc_.reg_read(UC_X86_REG_ECX); b_ = uc_.reg_read(UC_X86_REG_EDX)
                self.cmplog.append((a_, b_, bytes(uc_.mem_read(a_, 0x28)), bytes(uc_.mem_read(b_, 0x28))))
            uc.hook_add(UC_HOOK_CODE, _c, begin=0x4046cc, end=0x4046cc)
        if os.environ.get('C2WATCH'):
            lo, ln = [int(x, 16) for x in os.environ['C2WATCH'].split(':')]
            def _w(uc_, access, addr, size, value, ud):
                eip = uc_.reg_read(UC_X86_REG_EIP)
                esp = uc_.reg_read(UC_X86_REG_ESP)
                st = [self.rd32(esp + 4 * k) for k in range(64)]
                rets = ['%x' % v for v in st if self.base + 0x1000 <= v < self.base + 0x8c000]
                sys.stderr.write('WRITE %08x size %d val %x eip %08x stack %s\n' % (addr, size, value, eip, ' '.join(rets[:16])))
            uc.hook_add(UC_HOOK_MEM_WRITE, _w, begin=lo, end=lo + ln - 1)
        if os.environ.get('C2TRACE'):
            import array
            self.trace = array.array('I')
            tr = self.trace
            def _blk(uc_, addr, size, ud):
                tr.append(addr)
            from unicorn import UC_HOOK_BLOCK
            uc.hook_add(UC_HOOK_BLOCK, _blk, begin=self.base, end=self.base + 0x100000)
        sp = STACK + STACK_SZ - 0x100
        uc.reg_write(UC_X86_REG_ESP, sp)
        self.wr32(sp, THUNK + 0xff000)       # return from entry -> stop
        uc.mem_write(THUNK + 0xff000, b'\xf4')
        try:
            uc.emu_start(self.ep, THUNK + 0xff000)
        except UcError as e:
            eip = uc.reg_read(UC_X86_REG_EIP)
            print('UcError %s at eip %08x' % (e, eip), file=sys.stderr)
            raise
        return self.exit_code

    # ---- CRT models
    def f___set_app_type(self, a, esp): return 0
    def f___setusermatherr(self, a, esp): return 0
    def f__controlfp(self, a, esp): return 0x9001f

    def f___p__fmode(self, a, esp):
        if not hasattr(self, '_fmode'):
            self._fmode = self.dalloc(4); self.wr32(self._fmode, 0)
        return self._fmode

    def f___p__commode(self, a, esp):
        if not hasattr(self, '_commode'):
            self._commode = self.dalloc(4); self.wr32(self._commode, 0)
        return self._commode

    def f___getmainargs(self, a, esp):
        argv = self.dalloc(8)
        self.wr32(argv, self.cstr('c2.exe'))
        self.wr32(argv + 4, 0)
        env = self.dalloc(8)
        self.wr32(env, 0)
        self._envp = env
        self.wr32(a[0], 1)
        self.wr32(a[1], argv)
        self.wr32(a[2], env)
        return 0

    def f___p___initenv(self, a, esp):
        p = self.dalloc(4)
        self.wr32(p, self._envp)
        return p

    def f__XcptFilter(self, a, esp): return 0

    def f_exit(self, a, esp):
        self.exit_code = a[0]
        raise Exit()
    f__exit = f_exit

    def f__onexit(self, a, esp): return a[0]
    def f___dllonexit(self, a, esp): return a[0]
    def f_freopen(self, a, esp): return a[2]

    def f___p__iob(self, a, esp):
        if not hasattr(self, '_iob'):
            self._iob = self.dalloc(32 * 20)
            for i in range(3):
                self.wr32(self._iob + 32 * i + 16, i)
                self.files[self._iob + 32 * i] = {'fh': None, 'std': i}
        return self._iob

    def f_strpbrk(self, a, esp):
        s = self.rdstr(a[0]); cs = self.rdstr(a[1])
        for i, c in enumerate(s):
            if c in cs:
                return a[0] + i
        return 0

    def f_strchr(self, a, esp):
        s = self.rdstr(a[0]); c = a[1] & 0xff
        if c == 0:
            return a[0] + len(s)
        i = s.find(bytes([c]))
        return a[0] + i if i >= 0 else 0

    def f_strncpy(self, a, esp):
        s = self.rdstr(a[1])[:a[2]]
        self.uc.mem_write(a[0], s + b'\0' * (a[2] - len(s)))
        return a[0]

    def f_strncmp(self, a, esp):
        n = a[2]
        x = self.rdstr(a[0])[:n]; y = self.rdstr(a[1])[:n]
        return 0 if x == y else (1 if x > y else 0xffffffff)

    def f_memmove(self, a, esp):
        if a[2]:
            self.uc.mem_write(a[0], bytes(self.uc.mem_read(a[1], a[2])))
        return a[0]

    def f_atoi(self, a, esp):
        s = self.rdstr(a[0]).strip()
        m = __import__('re').match(rb'[+-]?\d+', s)
        return int(m.group(0)) if m else 0

    def f_strtol(self, a, esp):
        s = self.rdstr(a[0])
        base = a[2]
        import re
        st = len(s) - len(s.lstrip())
        t = s[st:]
        m = re.match(rb'[+-]?(0[xX][0-9a-fA-F]+|\d+)', t) if base in (0, 16) else re.match(rb'[+-]?\d+', t)
        if not m:
            if a[1]:
                self.wr32(a[1], a[0])
            return 0
        txt = m.group(0).decode()
        v = int(txt, 0 if base == 0 else base)
        if a[1]:
            self.wr32(a[1], a[0] + st + len(m.group(0)))
        return v

    def f__strdup(self, a, esp):
        return self.cstr(self.rdstr(a[0]))

    def f_getenv(self, a, esp):
        n = self.rdstr(a[0]).decode()
        if n == 'MSC_CMD_FLAGS':
            if not hasattr(self, '_flags_s'):
                self._flags_s = self.cstr(self.flags)
            return self._flags_s
        return 0

    def f_time(self, a, esp):
        if a[0]:
            self.wr32(a[0], 0x60000000)
        return 0x60000000

    def f_malloc(self, a, esp):
        n = a[0]
        p = self.alloc(n + 8 + self.malloc_pad, 16)
        self.wr32(p, n)
        return p + 8

    def f_free(self, a, esp): return 0

    def f__fullpath(self, a, esp):
        s = self.rdstr(a[1])
        buf = a[0] or self.alloc(260)
        self.uc.mem_write(buf, s + b'\0')
        return buf

    def f___unDName(self, a, esp): return 0

    def f_LoadStringA(self, a, esp):
        if a[3]:
            self.uc.mem_write(a[2], b'\0')
        return 0

    def f_GetModuleHandleA(self, a, esp): return self.base
    def f_SetConsoleCtrlHandler(self, a, esp): return 1

    # ctype
    def _ctype_table(self):
        if hasattr(self, '_ct'):
            return self._ct
        t = self.dalloc(2 * 257)
        vals = [0]
        for c in range(256):
            v = 0
            ch = chr(c)
            if c < 128:
                if ch.isupper(): v |= 0x101
                if ch.islower(): v |= 0x102
                if ch.isdigit(): v |= 0x4
                if ch in ' \t\n\r\v\f': v |= 0x8
                if 33 <= c <= 126 and not ch.isalnum(): v |= 0x10
                if c < 32 or c == 127: v |= 0x20
                if c == 32: v |= 0x40
                if ch in '0123456789abcdefABCDEF': v |= 0x80
            vals.append(v)
        self.uc.mem_write(t, struct.pack('<257H', *vals))
        self._ct = t + 2
        return self._ct

    def f___p__pctype(self, a, esp):
        p = self.dalloc(4)
        self.wr32(p, self._ctype_table())
        return p

    def f__isctype(self, a, esp):
        c = a[0] & 0xffffffff
        if c >= 256:
            return 0
        v = struct.unpack('<H', bytes(self.uc.mem_read(self._ctype_table() + 2 * c, 2)))[0]
        return v & a[1]

    def f___p___mb_cur_max(self, a, esp):
        p = self.dalloc(4)
        self.wr32(p, 1)
        return p

    def f__errno(self, a, esp):
        if not hasattr(self, '_errno_p'):
            self._errno_p = self.dalloc(4); self.wr32(self._errno_p, 0)
        return self._errno_p

    def f__strerror(self, a, esp):
        return self.cstr('error')

    # formatting
    def _format(self, fmt, argp):
        import re
        out = bytearray()
        i = 0
        while i < len(fmt):
            c = fmt[i:i + 1]
            if c != b'%':
                out += c
                i += 1
                continue
            m = re.match(rb'%([-+ #0]*)(\*|\d+)?(?:\.(\*|\d+))?([hlL]?)([diouxXcsSpfeEgG%n])', fmt[i:])
            if not m:
                out += c
                i += 1
                continue
            flags, width, prec, lm, conv = m.groups()
            i += len(m.group(0))
            if width == b'*':
                width = str(self.rd32(argp)).encode(); argp += 4
            if prec == b'*':
                prec = str(self.rd32(argp)).encode(); argp += 4
            spec = '%' + flags.decode() + (width.decode() if width else '') + ('.' + prec.decode() if prec is not None else '')
            cv = conv.decode()
            if cv == '%':
                out += b'%'
                continue
            if cv in 'fFeEgG':
                v = struct.unpack('<d', bytes(self.uc.mem_read(argp, 8)))[0]; argp += 8
                out += (spec + cv) % v
                continue
            v = self.rd32(argp); argp += 4
            if cv in 'di':
                v = v - (1 << 32) if v & 0x80000000 else v
                out += (spec + 'd').encode() % v
            elif cv in 'ouxX':
                out += (spec + cv).encode() % v
            elif cv == 'c':
                out += bytes([v & 0xff])
            elif cv in 'sS':
                s = self.rdstr(v) if v else b'(null)'
                if prec is not None:
                    s = s[:int(prec)]
                out += (spec.replace('.' + prec.decode(), '') if prec is not None else spec).encode().replace(b'%', b'%') % () if False else (s.rjust(int(width)) if width and b'-' not in flags else (s.ljust(int(width)) if width else s))
            elif cv == 'p':
                out += b'%08X' % v
            elif cv == 'n':
                self.wr32(v, len(out))
        return bytes(out)

    def f_sprintf(self, a, esp):
        s = self._format(self.rdstr(a[1]), esp + 12)
        self.uc.mem_write(a[0], s + b'\0')
        return len(s)

    def f_printf(self, a, esp):
        s = self._format(self.rdstr(a[0]), esp + 8)
        self.stdout.append(s)
        return len(s)

    # files
    def _hostpath(self, p):
        p = p.decode('latin-1').replace('\\', '/')
        if p[:2].lower() == 'z:':
            p = p[2:]
        return p

    def f_fopen(self, a, esp):
        path = self._hostpath(self.rdstr(a[0]))
        mode = self.rdstr(a[1]).decode()
        try:
            fh = open(path, ('r+b' if '+' in mode and 'r' in mode else
                             'w+b' if '+' in mode and 'w' in mode else
                             'rb' if 'r' in mode else 'ab' if 'a' in mode else 'wb'))
        except OSError:
            return 0
        f = self.dalloc(32)
        fd = self.next_fd
        self.next_fd += 1
        buf = self.alloc(4096)
        self.uc.mem_write(f, struct.pack('<IIIIIIII', buf, 0, buf, 1, fd, 0, 4096, 0))
        self.files[f] = {'fh': fh, 'path': path, 'buf': buf}
        self.fds[fd] = f
        return f

    def _cnt(self, f):
        return self.rd32(f + 4)

    def _drop_buffer(self, f):
        """Return the host position the guest has actually consumed up to, and empty the buffer."""
        d = self.files[f]
        cnt = self.rd32(f + 4)
        cnt = cnt - (1 << 32) if cnt & 0x80000000 else cnt
        pos = d['fh'].tell() - max(cnt, 0)
        self.wr32(f + 4, 0)
        self.wr32(f, d['buf'])
        return pos

    def f__filbuf(self, a, esp):
        f = a[0]
        d = self.files.get(f)
        if not d or not d['fh']:
            return 0xffffffff
        data = d['fh'].read(4096)
        if not data:
            self.wr32(f + 4, 0)
            return 0xffffffff
        self.uc.mem_write(d['buf'], data)
        self.wr32(f, d['buf'] + 1)
        self.wr32(f + 4, len(data) - 1)
        return data[0]

    def f_fread(self, a, esp):
        buf, sz, n, f = a[0], a[1], a[2], a[3]
        d = self.files[f]
        want = sz * n
        got = bytearray()
        cnt = self.rd32(f + 4)
        cnt = cnt - (1 << 32) if cnt & 0x80000000 else cnt
        if cnt > 0:
            ptr = self.rd32(f)
            k = min(cnt, want)
            got += self.uc.mem_read(ptr, k)
            self.wr32(f, ptr + k)
            self.wr32(f + 4, cnt - k)
        if len(got) < want:
            got += d['fh'].read(want - len(got))
        if got:
            self.uc.mem_write(buf, bytes(got))
        return len(got) // sz if sz else 0

    def f_fgets(self, a, esp):
        buf, n, f = a[0], a[1], a[2]
        d = self.files[f]
        pos = self._drop_buffer(f)
        d['fh'].seek(pos)
        line = d['fh'].readline(n - 1)
        if not line:
            return 0
        self.uc.mem_write(buf, line + b'\0')
        return buf

    def f_fseek(self, a, esp):
        f, off, whence = a[0], a[1], a[2]
        off = off - (1 << 32) if off & 0x80000000 else off
        d = self.files[f]
        if d.get('rd', True):
            pos = self._drop_buffer(f)
            if whence == 1:
                off += pos
                whence = 0
        d['fh'].seek(off, whence)
        return 0

    def f_ftell(self, a, esp):
        f = a[0]
        d = self.files[f]
        cnt = self.rd32(f + 4)
        cnt = cnt - (1 << 32) if cnt & 0x80000000 else cnt
        return d['fh'].tell() - max(cnt, 0)

    def f_fwrite(self, a, esp):
        buf, sz, n, f = a[0], a[1], a[2], a[3]
        d = self.files.get(f)
        data = bytes(self.uc.mem_read(buf, sz * n)) if sz * n else b''
        if d is None or d.get('std') is not None:
            self.stdout.append(data)
            return n
        d['fh'].write(data)
        if os.environ.get('C2FINDW'):
            pat = bytes.fromhex(os.environ['C2FINDW'])
            k = data.find(pat)
            while k >= 0:
                sys.stderr.write('fwrite pattern at guest %08x (+%d) of buffer %08x len %d\n' % (buf + k, k, buf, len(data)))
                k = data.find(pat, k + 1)
        return n

    def f_fputc(self, a, esp):
        c, f = a[0] & 0xff, a[1]
        d = self.files.get(f)
        if d is None or d.get('std') is not None:
            self.stdout.append(bytes([c]))
        else:
            d['fh'].write(bytes([c]))
        return c

    def f_fflush(self, a, esp):
        d = self.files.get(a[0])
        if d and d.get('fh'):
            d['fh'].flush()
        return 0

    def f_fclose(self, a, esp):
        d = self.files.pop(a[0], None)
        if d and d.get('fh'):
            d['fh'].close()
        return 0

    def f__fcloseall(self, a, esp):
        for f in list(self.files):
            if self.files[f].get('fh'):
                self.files[f]['fh'].close()
                del self.files[f]
        return 0

    def f__write(self, a, esp):
        fd, buf, n = a[0], a[1], a[2]
        data = bytes(self.uc.mem_read(buf, n)) if n else b''
        f = self.fds.get(fd)
        if f is None or f not in self.files:
            self.stdout.append(data)
            return n
        self.files[f]['fh'].write(data)
        return n

    def f__chsize(self, a, esp):
        f = self.fds.get(a[0])
        if f in self.files:
            self.files[f]['fh'].truncate(a[1])
        return 0

    def f_remove(self, a, esp):
        try:
            os.remove(self._hostpath(self.rdstr(a[0])))
        except OSError:
            return 0xffffffff
        return 0

    # mspdb50: report failure everywhere
    def _pdbfail(self, a, esp): return 0
    f_PDBOpen = f_PDBOpenStream = f_PDBCommit = f_PDBClose = _pdbfail
    f_StreamAppend = f_StreamReplace = f_StreamQueryCb = f_StreamRead = f_StreamRelease = _pdbfail
    f_ILModGetILVer = f_ILModGetIL = f_ILModRelease = f_ILStoreRelease = f_ILStoreGetILMod = f_ILStoreOpen = _pdbfail
    f_SigForPbCb = _pdbfail


def main():
    il, out = sys.argv[1], sys.argv[2]
    ild = il.rstrip('/') + '/'
    for s in ('ex', 'gl', 'in', 'sy'):
        src = ild + s
        dst = ild + 'ca' + s
        if not os.path.exists(dst) or open(dst, 'rb').read() != open(src, 'rb').read():
            open(dst, 'wb').write(open(src, 'rb').read())
    flags = ('-il %sca -f src/core/drawing/_cap.c -G5 -dos -ML -Gs4096 -Gy -W 3 -Fo%s' %
             (os.path.abspath(ild).replace('/', '\\') + '\\', os.path.abspath(out).replace('/', '\\')))
    c = C2(il, out, flags)
    t0 = _time.time()
    r = c.run()
    if os.environ.get('C2KEYLOG'):
        seen_ = set()
        for n_, k_, ch in c.keylog:
            if n_ in seen_: continue
            seen_.add(n_)
            sys.stderr.write('KEY %08x = %08x  children %s\n' % (n_, k_, ' | '.join('%08x t%d k%08x s%08x o%x' % x for x in ch)))
    if os.environ.get('C2LATLOG'):
        for r_ in c.latlog:
            if os.environ.get('C2LATALL') or r_[0] in (0x20012408, 0x200121f8, 0x20012300, 0x20012618, 0x20014184):
                sys.stderr.write('LAT pred %08x(op %x base %d) succ %08x(op %x) p2 %08x p3 %08x\n' % (r_[0], r_[4], r_[6], r_[1], r_[5], r_[2], r_[3]))
    if os.environ.get('C2SUCC'):
        for s_ in c.succ[:40]:
            sys.stderr.write('SUCC %08x prio %x -> %s\n' % (s_[0], s_[1], ' ; '.join('%08x h%d lat%d edge %s' % (a, b, l, h) for a, b, l, h in s_[2])))
    if os.environ.get('C2SCHED'):
        import pickle
        pickle.dump(c.sched, open(os.environ['C2SCHED'], 'wb'))
    if os.environ.get('C2RDYLOG'):
        import pickle
        pickle.dump(c.rdylog, open(os.environ['C2RDYLOG'], 'wb'))
    if os.environ.get('C2CMPLOG'):
        import pickle
        pickle.dump(c.cmplog, open(os.environ['C2CMPLOG'], 'wb'))
    if os.environ.get('C2EMITLOG'):
        import pickle
        pickle.dump(c.emitlog, open(os.environ['C2EMITLOG'], 'wb'))
    if os.environ.get('C2PTRW'):
        open(os.environ['C2PTRWOUT'], 'w').write('\n'.join(c.ptrw))
    if os.environ.get('C2TRACE'):
        c.trace.tofile(open(os.environ['C2TRACE'], 'wb'))
    sys.stderr.write('exit %r in %.1fs\n' % (r, _time.time() - t0))
    sys.stderr.write(b''.join(c.stdout).decode('latin-1')[-2000:])


if __name__ == '__main__':
    main()
