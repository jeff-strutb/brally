"""x87emu.py -- a scoped x86+x87 interpreter used as an EQUIVALENCE ORACLE for
transcribing the game's dense, interleaved-FPU functions.

WHY: functions like the OBB collision response (0x10067710 and its helpers) are
20-deep fxch dances; hand-tracing them is how sign/index errors slip in. This
executes the real opcode stream, so data flow is exact by construction, and it
emits golden vectors a C transcription can be pinned to.

VALIDATION: self-checks against slice3_44.c BrMat3Solve (0x1006DE70), the mode
logic of 0x10067470, and the looping BrMat4MulVec3 (0x1006D980). See the repo
scratch harnesses; every function it has been pointed at reproduces an
independently-known result.

MODEL:
  - Memory is BYTE-addressable (dict addr->byte); dword/f32 access assembles or
    splits little-endian.  A dword read as int in one place and f32 in another
    (0x117787FC in 0x10067470) therefore behaves as hardware does.
  - Integer flags ZF/SF/CF/OF are computed from cmp/sub/add/inc/dec/test, so the
    full jump family (je/jne/jl/jle/jg/jge/ja/jae/jb/jbe/js/jns) is supported.
  - Byte registers al/ah/bl/.../cl and SIB memory ([base+idx*s+disp]) are
    handled.  call/ret use a return-address stack; 0x10074560 (_ftol) is an
    intrinsic (st0 -> eax, truncated).  Other calls execute the callee's listing
    if it was load_listing'd into the same program.
  - x87 registers are doubles with f32 rounding on dword store: behavioural, not
    bit-identical, equivalence -- which is exactly this port's target.
"""
import re, struct, math


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def u32(x):
    return x & 0xFFFFFFFF


def _x87_to_i32(v):
    """x87 st(0) -> 32-bit int, truncating toward zero.  A NaN, an infinity, or a
    value outside int32 range yields the x86 'integer indefinite' 0x80000000 --
    what the hardware stores when the (masked) invalid-operation exception fires
    -- instead of raising, so a run over zeroed/garbage FP state stays modelled
    identically on both sides rather than escaping the oracle."""
    try:
        if v != v or v in (float('inf'), float('-inf')):
            return 0x80000000
        t = math.trunc(v)
        if t < -0x80000000 or t > 0x7FFFFFFF:
            return 0x80000000
        return t & 0xFFFFFFFF
    except (ValueError, OverflowError):
        return 0x80000000


def s32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x >= 0x80000000 else x


def _ieee_div(n, d):
    """x87 division with the default (masked) control word: /0 gives +-inf,
    0/0 gives NaN -- it does not trap, so the emulator must not either."""
    if d == 0.0:
        if n == 0.0 or n != n:
            return float('nan')
        return math.copysign(float('inf'), n) * math.copysign(1.0, d)
    return n / d


# 8-bit register views onto their 32-bit parent.
REG16 = {'ax': 'eax', 'bx': 'ebx', 'cx': 'ecx', 'dx': 'edx',
         'si': 'esi', 'di': 'edi', 'bp': 'ebp', 'sp': 'esp'}
REG8 = {'al': ('eax', 0), 'ah': ('eax', 8), 'bl': ('ebx', 0), 'bh': ('ebx', 8),
        'cl': ('ecx', 0), 'ch': ('ecx', 8), 'dl': ('edx', 0), 'dh': ('edx', 8)}
REG32 = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')


def _model_memmove(m):
    """cdecl memmove(dst, src, n) -> dst in eax.  Args sit at [esp..esp+8] (no
    return address has been pushed for a modeled import).  Overlap-safe via a
    read-all-then-write.  n is bounded so a garbage seed cannot hang the run;
    the bound is identical on both sides, so it never manufactures a divergence."""
    esp = m.R['esp']
    dst = m.rd_i(esp); src = m.rd_i(esp + 4); n = m.rd_i(esp + 8) & 0xFFFFFFFF
    n = min(n, 1 << 16)     # same bound as the inline rep movs the original uses
    buf = [m.rd_u8(src + k) for k in range(n)]
    for k in range(n):
        m.wr_u8(dst + k, buf[k])
    m.R['eax'] = dst


def _model_wait_single(m):
    """WaitForSingleObject(handle, ms) __stdcall -> WAIT_OBJECT_0 (0).  Under the
    oracle the mutex-guarded peer/car tables are single-threaded, so the wait
    always succeeds instantly; no compared state depends on the handle value.
    The real callee's `ret 8` clears both args, so esp += 8 (no return address
    is pushed for a modeled import, so the two arg dwords sit at [esp..esp+4])."""
    m.R['eax'] = 0
    m.R['esp'] = u32(m.R['esp'] + 8)


def _model_release_mutex(m):
    """ReleaseMutex(handle) __stdcall -> TRUE (1).  `ret 4` clears the one arg,
    so esp += 4.  Every caller in this image discards the BOOL result."""
    m.R['eax'] = 1
    m.R['esp'] = u32(m.R['esp'] + 4)


# DLL imports whose code is in a module the oracle does not map, keyed by the
# reference image's IAT slot address.  memmove is MSVCRT; the two mutex calls
# are KERNEL32, reached by the SEH-framed net dispatchers (0x1002F790 etc.).
MSVCRT_IMPORTS = {
    0x118F04FC: _model_memmove,
    0x118F044C: _model_wait_single,     # KERNEL32 WaitForSingleObject@8
    0x118F04BC: _model_release_mutex,   # KERNEL32 ReleaseMutex@4
}


def _model_allmul(m):
    """__allmul: 64-bit integer multiply, __stdcall(a:i64, b:i64) -> edx:eax.
    No return address is pushed in this model, so the four arg dwords sit at
    [esp..esp+12]; the real helper's `ret 0x10` clears them, so esp += 16."""
    esp = m.R['esp']
    a = m.rd_i(esp) | (m.rd_i(esp + 4) << 32)
    b = m.rd_i(esp + 8) | (m.rd_i(esp + 12) << 32)
    p = (a * b) & 0xFFFFFFFFFFFFFFFF
    m.R['eax'] = p & 0xFFFFFFFF
    m.R['edx'] = (p >> 32) & 0xFFFFFFFF
    m.R['esp'] = u32(esp + 16)


def _model_cipow(m):
    """_CIpow: the MSVC x87 `pow` intrinsic.  Base in st(1), exponent in st(0);
    result base**exp replaces both (stack shrinks by one), nothing on the
    integer stack.  A domain error (negative base, fractional exp) or overflow
    yields NaN/Inf deterministically -- both sides compute the same, which is
    all equivalence needs."""
    y = m.st.pop(0) if m.st else 0.0        # exponent (top)
    x = m.st.pop(0) if m.st else 0.0        # base
    try:
        r = math.pow(x, y)
    except ValueError:
        r = float('nan')
    except OverflowError:
        r = float('inf')
    m.st.insert(0, r)


def _s64(x):
    return x - (1 << 64) if x & (1 << 63) else x


def _div_helper(m, signed, want_rem):
    """Shared body for __alldiv/__allrem/__aulldiv/__aullrem: two i64 args at
    [esp..esp+12], quotient-or-remainder in edx:eax, esp += 16 (`ret 0x10`)."""
    esp = m.R['esp']
    a = m.rd_i(esp) | (m.rd_i(esp + 4) << 32)
    b = m.rd_i(esp + 8) | (m.rd_i(esp + 12) << 32)
    if signed:
        a, b = _s64(a), _s64(b)
    if b == 0:
        q = r = 0                         # a real divide would fault; both sides alike
    else:
        q = abs(a) // abs(b)
        if signed and (a < 0) != (b < 0):
            q = -q
        r = a - q * b if signed else a - (a // b) * b
    v = (r if want_rem else q) & 0xFFFFFFFFFFFFFFFF
    m.R['eax'] = v & 0xFFFFFFFF
    m.R['edx'] = (v >> 32) & 0xFFFFFFFF
    m.R['esp'] = u32(esp + 16)


# Compiler helpers reached by DIRECT call into .text that the interpreter models
# rather than execute (64-bit ops the byte interpreter does not implement).
# Keyed by their function address in the reference image.
DIRECT_BUILTINS = {
    0x10074680: _model_allmul,
    0x100748B0: lambda m: _div_helper(m, signed=True, want_rem=False),    # __alldiv
    0x10074610: lambda m: _div_helper(m, signed=False, want_rem=False),   # __aulldiv
    0x100748A0: _model_cipow,                                             # _CIpow (x87 pow)
}


class Machine:
    def __init__(self, mem, regs, listing, idx=None, imports=None, code_provider=None):
        self.mem = mem                      # dict: byte-address -> 0..255
        self.R = dict(regs)
        self.st = []                        # x87 stack, st[0] is TOP
        self.ZF = self.SF = self.CF = self.OF = 0
        self.C0 = 0                         # last fcom below/unordered
        self.seg_fs = {}                    # fs:[disp] SEH-chain slots (frame state)
        self.trace_calls = None             # opt-in list: resolved call targets, for diagnosis
        self.stub_targets = frozenset()     # direct-call VAs to black-box (profile-driven)
        self.dcalls = []                    # (target, args) of black-boxed direct calls, an observable
        self.callstack = []
        self.FTOL = 0x10074560
        self.prog = listing
        # Indirect calls `call dword ptr [slot]` reach two kinds of target: a
        # function pointer stored in a game global (points back into the mapped
        # .text -- executed normally once read), or a true DLL import whose code
        # is in a module this oracle never maps (MSVCRT).  The latter are modeled
        # here, keyed by their IAT slot address, so both sides run the identical
        # operation.  BRGlide's only such import reached from these functions is
        # memmove @ 0x118F04FC; extend this map as more surface.
        self.imports = imports if imports is not None else dict(MSVCRT_IMPORTS)
        # When set, an indirect call to a pointer that is not mapped code is
        # recorded (see run()) instead of raising -- lets orchestrator functions
        # that dispatch through init-only function-pointer tables be compared.
        self.model_unresolved_icalls = False
        self.icalls = []
        # Optional callback addr -> [(addr, mn, ops), ...].  The shared .text
        # index is a LINEAR sweep, so a function entry that sits after embedded
        # data (a jump table, alignment) can be missing.  When a call lands on
        # such an address, disassemble it on demand and splice it in.
        self.code_provider = code_provider
        # `idx` may be supplied prebuilt (the oracle shares one 130k-entry .text
        # index across every run rather than rebuild it per Machine).
        self.idx = idx if idx is not None else {a: i for i, (a, _, _) in enumerate(listing)}

    def _load_code(self, target):
        if self.code_provider is None:
            return False
        insns = self.code_provider(target)
        if not insns:
            return False
        own = self.idx.own if hasattr(self.idx, 'own') else self.idx
        for ins in insns:
            if ins[0] not in self.idx:
                own[ins[0]] = len(self.prog)
                self.prog.append(ins)
        return target in self.idx
        # `idx` may be supplied prebuilt.  The equivalence oracle maps the
        # whole original .text (about 130k instructions) so that calls execute
        # the real callee; rebuilding that index per Machine -- twice a seed,
        # sixty-four seeds a function -- is what made it unusably slow.
        self.idx = idx if idx is not None else {a: i for i, (a, _, _) in enumerate(listing)}

    # ---- byte-addressable memory --------------------------------------
    def rd_u8(self, a):
        return self.mem.get(a, 0) & 0xFF

    def wr_u8(self, a, v):
        self.mem[a] = v & 0xFF

    def rd_i(self, a):
        return (self.mem.get(a, 0) | self.mem.get(a + 1, 0) << 8 |
                self.mem.get(a + 2, 0) << 16 | self.mem.get(a + 3, 0) << 24)

    def wr_i(self, a, v):
        v = u32(v)
        for k in range(4):
            self.mem[a + k] = (v >> (8 * k)) & 0xFF

    def rd_f(self, a):
        return struct.unpack('<f', struct.pack('<I', self.rd_i(a)))[0]

    def wr_f(self, a, v):
        self.wr_i(a, struct.unpack('<I', struct.pack('<f', f32(v)))[0])

    def rd_f8(self, a):
        """Read an 8-byte DOUBLE.  x87 `fld/fcom/fadd/... qword ptr` operands are
        double precision; reading them as f32 (the low dword) is silently wrong --
        e.g. 0.5 becomes 0.0, which made the OBB box-classify reject everything."""
        return struct.unpack('<d', bytes(self.rd_u8(a + k) for k in range(8)))[0]

    def wr_f8(self, a, v):
        b = struct.pack('<d', v)
        for k in range(8):
            self.wr_u8(a + k, b[k])

    # ---- operand helpers ----------------------------------------------
    def mem_addr(self, s):
        s = s.strip()
        m = re.fullmatch(r'\[(0x[0-9a-fA-F]+)\]', s)
        if m:
            return int(m.group(1), 16)
        # [base + idx*scale + disp] with optional pieces
        inner = s[1:-1]
        addr = 0
        # scale term
        m = re.search(r'(\w+)\s*\*\s*(\d+)', inner)
        if m:
            addr += self.R[m.group(1)] * int(m.group(2))
            inner = inner[:m.start()] + inner[m.end():]
        for tok in re.finditer(r'([+\-]?)\s*(\w+|0x[0-9a-fA-F]+|\d+)', inner):
            sign, t = tok.group(1), tok.group(2)
            if t in self.R:
                v = self.R[t]
            elif re.fullmatch(r'0x[0-9a-fA-F]+|\d+', t):
                v = int(t, 0)
            else:
                continue
            addr += -v if sign == '-' else v
        return addr & 0xFFFFFFFF

    def rd_reg(self, name):
        if name in self.R:
            return self.R[name]
        if name in REG8:
            parent, sh = REG8[name]
            return (self.R[parent] >> sh) & 0xFF
        if name in REG16:
            return self.R[REG16[name]] & 0xFFFF
        raise KeyError(name)

    def wr_reg(self, name, v):
        if name in self.R:
            self.R[name] = u32(v)
        elif name in REG8:
            parent, sh = REG8[name]
            mask = 0xFF << sh
            self.R[parent] = (self.R[parent] & ~mask) | ((v & 0xFF) << sh)
        elif name in REG16:
            parent = REG16[name]
            self.R[parent] = (self.R[parent] & 0xFFFF0000) | (v & 0xFFFF)
        else:
            raise KeyError(name)

    def _seg_off(self, x):
        """`fs:[disp]` / `gs:[disp]` -> the disp, or None if not segment-relative.

        The only segment access these functions make is the C++/SEH prologue's
        `fs:[0]` -- the thread's exception-registration head.  It is thread/frame
        state the oracle never compares (both runs touch it identically), so a
        small per-machine dict models it soundly without a real TIB."""
        m = re.match(r'(?:fs|gs):\s*(\[.*\])$', x)
        return self.mem_addr(m.group(1)) if m else None

    def _rd(self, x, byte=False):
        """Read an operand that may be a register OR a memory reference."""
        x = x.strip()
        so = self._seg_off(x)
        if so is not None:
            return self.seg_fs.get(so, 0) & (0xFF if byte else 0xFFFFFFFF)
        if x.startswith('['):
            a = self.mem_addr(x)
            return self.rd_u8(a) if byte else self.rd_i(a)
        return self.rd_reg(x)

    def _wr(self, x, v, byte=False):
        """Write an operand that may be a register OR a memory reference."""
        x = x.strip()
        so = self._seg_off(x)
        if so is not None:
            self.seg_fs[so] = v & (0xFF if byte else 0xFFFFFFFF)
            return
        if x.startswith('['):
            a = self.mem_addr(x)
            (self.wr_u8 if byte else self.wr_i)(a, v)
        else:
            self.wr_reg(x, v)

    def _val(self, x, byte=False):
        x = x.strip()
        if x in self.R or x in REG8 or x in REG16:
            return self.rd_reg(x)
        if re.fullmatch(r'-?0x[0-9a-fA-F]+|-?\d+', x):
            return int(x, 0) & 0xFFFFFFFF
        so = self._seg_off(x)
        if so is not None:
            return self.seg_fs.get(so, 0) & (0xFF if byte else 0xFFFFFFFF)
        if x.startswith('['):
            return self.rd_u8(self.mem_addr(x)) if byte else self.rd_i(self.mem_addr(x))
        raise ValueError('val? %r' % x)

    # ---- flags --------------------------------------------------------
    def _flags_sub(self, a, b, width=32):
        mask = (1 << width) - 1
        a &= mask; b &= mask
        r = (a - b) & mask
        self.ZF = 1 if r == 0 else 0
        sign = 1 << (width - 1)
        self.SF = 1 if r & sign else 0
        self.CF = 1 if a < b else 0
        self.OF = 1 if ((a ^ b) & (a ^ r) & sign) else 0
        return r

    def _flags_add(self, a, b, width=32):
        mask = (1 << width) - 1
        a &= mask; b &= mask
        r = (a + b) & mask
        self.ZF = 1 if r == 0 else 0
        sign = 1 << (width - 1)
        self.SF = 1 if r & sign else 0
        self.CF = 1 if (a + b) > mask else 0
        self.OF = 1 if (~(a ^ b) & (a ^ r) & sign) else 0
        return r

    def _flags_logic(self, r, width=32):
        mask = (1 << width) - 1
        r &= mask
        self.ZF = 1 if r == 0 else 0
        self.SF = 1 if r & (1 << (width - 1)) else 0
        self.CF = 0; self.OF = 0

    def _cond(self, mn):
        Z, S, O, C = self.ZF, self.SF, self.OF, self.CF
        return {
            'je': Z, 'jz': Z, 'jne': not Z, 'jnz': not Z,
            'js': S, 'jns': not S,
            'jl': S != O, 'jnge': S != O, 'jge': S == O, 'jnl': S == O,
            'jle': Z or (S != O), 'jng': Z or (S != O),
            'jg': (not Z) and (S == O), 'jnle': (not Z) and (S == O),
            'jb': C, 'jc': C, 'jnae': C, 'jae': not C, 'jnb': not C, 'jnc': not C,
            'jbe': C or Z, 'jna': C or Z, 'ja': (not C) and (not Z), 'jnbe': (not C) and (not Z),
        }[mn]

    # ---- run loop -----------------------------------------------------
    def run(self, start, maxsteps=5000000):
        pc = self.idx[start]
        steps = 0
        while 0 <= pc < len(self.prog):
            steps += 1
            if steps > maxsteps:
                raise RuntimeError('runaway @%08X' % self.prog[pc][0])
            addr, mn, ops = self.prog[pc]
            self.pc_addr = addr
            if mn == 'call':
                t = ops.strip()
                slot = None
                if re.fullmatch(r'(?:0x)?[0-9a-fA-F]+', t):
                    target = int(t, 16)
                else:
                    mem = re.sub(r'\b(?:dword|qword|word|byte) ptr ', '', t).strip()
                    if mem in self.R or mem in REG8 or mem in REG16:
                        # call <reg>: the register HOLDS the callee address (a
                        # cached import pointer or a game function pointer).  Not
                        # a memory dereference -- take the value directly.  Mark
                        # slot non-None (the register value) so that if the value
                        # is neither an import nor mapped code -- a garbage/uninit
                        # function pointer in the seeded world -- it BLACK-BOXES
                        # via model_unresolved_icalls below instead of raising
                        # "call to unmapped" (which would leave the function
                        # UNCLASSIFIED, the pre-fix behaviour was to black-box).
                        target = self.rd_reg(mem)
                        slot = target
                    else:
                        # indirect: call dword ptr [slot] -- slot holds either a
                        # game function pointer (into mapped .text) or a DLL import.
                        slot = self.mem_addr(mem)
                        if slot in self.imports:
                            if self.trace_calls is not None:
                                self.trace_calls.append(('imp', slot))
                            self.imports[slot](self)     # modeled import, esp net-zero
                            pc += 1; continue
                        target = self.rd_i(slot)         # deref the function pointer
                # A cached import called through a register: the IAT slots are
                # seeded to their own address, so `mov reg,[slot]; call reg` lands
                # here with target == the slot.  Model it -- crucially cleaning the
                # stdcall args, which a bare unresolved-icall would leak (that leak
                # drifted esp and corrupted a stack object 8 bytes downstream).
                if target in self.imports:
                    if self.trace_calls is not None:
                        self.trace_calls.append(('imp', target))
                    self.imports[target](self)
                    pc += 1; continue
                if self.trace_calls is not None:
                    self.trace_calls.append((hex(target), 'i' if slot is not None else 'd',
                                             hex(self.R['eax'])))
                if target == self.FTOL:
                    # _ftol is cdecl-with-no-args: on hardware the call pushes a
                    # return address and the callee's ret pops it, net esp change
                    # zero -- which is what leaving esp alone here models.
                    v = self.st.pop(0)
                    self.R['eax'] = _x87_to_i32(v)
                    pc += 1; continue
                if target in DIRECT_BUILTINS:
                    DIRECT_BUILTINS[target](self)
                    pc += 1; continue
                if target in self.stub_targets:
                    # Black-box a heavy subsystem this function only hands a
                    # seeded slot to (profile stub_calls).  Record target + arg
                    # window as an observable, then skip the body: eax<-0, esp
                    # unchanged (cdecl -- the caller cleans; the skipped ret
                    # would have popped only the return address).
                    esp = self.R['esp']
                    self.dcalls.append((target,
                                        tuple(self.rd_i(esp + 4 * k) for k in range(4))))
                    self.R['eax'] = 0
                    pc += 1; continue
                if target not in self.idx:
                    self._load_code(target)   # on-demand: entry missed by linear sweep
                if target in self.idx:
                    # Model the real return-address push: decrement esp and store
                    # the return VA.  WITHOUT THIS a nested cdecl callee reads its
                    # stack arguments one slot too high ([esp+4] lands on arg2,
                    # not arg1), silently corrupting every function that both
                    # takes stack args AND is reached through a call -- e.g. the
                    # matrix helpers under the collision-response solver.  The
                    # matching pop is in 'ret'.
                    self.R['esp'] = u32(self.R['esp'] - 4)
                    self.wr_i(self.R['esp'], self.prog[pc + 1][0])
                    self.callstack.append(pc + 1); pc = self.idx[target]; continue
                if slot is not None and self.model_unresolved_icalls:
                    # An indirect call whose pointer is not mapped code: a game
                    # callback whose slot holds no valid target in a seeded image
                    # (the real value is installed by init we do not run).  Record
                    # it as an observable event -- WHICH slot, and the argument
                    # window on the stack -- then black-box it identically on both
                    # sides (eax<-0, esp unchanged).  Calling a different slot or
                    # pushing different args shows as an event-sequence mismatch;
                    # the callee's own effects are skipped on both sides alike.
                    esp = self.R['esp']
                    args = tuple(self.rd_i(esp + 4 * k) for k in range(8))
                    self.icalls.append((slot, args))
                    self.R['eax'] = 0
                    pc += 1; continue
                raise ValueError('call to unmapped %08X' % target)
            if mn == 'ret':
                if self.callstack:
                    # Pop the return address, plus any callee-cleaned argument
                    # bytes named by `ret <imm>` (stdcall/thiscall: e.g. a ctor
                    # `ret 8`).  Ignoring the immediate left esp low by that many
                    # bytes, mis-aligning every stack local the caller touched
                    # afterwards -- a packet object read 8 bytes off its ctor.
                    imm = int(ops.strip(), 0) if ops.strip() else 0
                    self.R['esp'] = u32(self.R['esp'] + 4 + imm)
                    pc = self.callstack.pop(); continue
                return
            nxt = self.step(addr, mn, ops)
            if nxt is None:
                pc += 1
            else:
                if nxt not in self.idx:
                    self._load_code(nxt)          # entry missed by the linear sweep
                if nxt not in self.idx:
                    # jump target is not mapped code -- an indirect jmp through an
                    # import slot (tail-call into a module we don't map) or garbage.
                    # Model it as a return: the tail-called callee's ret would go
                    # to THIS function's caller.  Identical on both sides.
                    if self.callstack:
                        self.R['esp'] = u32(self.R['esp'] + 4)
                        pc = self.callstack.pop(); continue
                    return
                pc = self.idx[nxt]

    def step(self, addr, mn, ops):
        st = self.R
        qword = 'qword ptr' in ops           # x87 double-precision memory operand
        ops = re.sub(r'\b(?:dword|qword|word|byte) ptr ', '', ops)
        byte = mn in ('movzx', 'movsx') or ' al' in (',' + ops) or False
        o = [x.strip() for x in ops.split(',')] if ops else []
        # x87 memory operands are f32 (dword) or f64 (qword); route accordingly.
        rdf = self.rd_f8 if qword else self.rd_f
        wrf = self.wr_f8 if qword else self.wr_f

        def is8(x):
            return x in REG8 or (x.startswith('[') and False)  # byte flagged via mnemonic size
        # ---- integer ----
        if mn == 'mov':
            d, s = o
            bsize = (d in REG8) or (s in REG8)
            v = self._val(s, byte=bsize)
            self._wr(d, v, byte=bsize)
        elif mn in ('movzx',):
            d, s = o
            self.wr_reg(d, self._val(s, byte=True) & 0xFF)
        elif mn in ('movsx',):
            d, s = o
            v = self._val(s, byte=True) & 0xFF
            self.wr_reg(d, v - 0x100 if v & 0x80 else v)
        elif mn == 'lea':
            d, s = o
            self.wr_reg(d, self.mem_addr(s))
        elif mn == 'add':
            d, s = o
            bs = (d in REG8) or (s in REG8)
            r = self._flags_add(self._rd(d, bs), self._val(s, byte=bs), width=8 if bs else 32)
            self._wr(d, r, bs)
        elif mn == 'sub':
            d, s = o
            bs = (d in REG8) or (s in REG8)
            r = self._flags_sub(self._rd(d, bs), self._val(s, byte=bs), width=8 if bs else 32)
            self._wr(d, r, bs)
        elif mn == 'inc':
            bs = o[0] in REG8
            cf = self.CF
            self._wr(o[0], self._flags_add(self._rd(o[0], bs), 1, width=8 if bs else 32), bs)
            self.CF = cf
        elif mn == 'dec':
            bs = o[0] in REG8
            cf = self.CF
            self._wr(o[0], self._flags_sub(self._rd(o[0], bs), 1, width=8 if bs else 32), bs)
            self.CF = cf
        elif mn == 'cmp':
            bsize = (o[0] in REG8) or (o[1] in REG8)
            self._flags_sub(self._val(o[0], byte=bsize), self._val(o[1], byte=bsize),
                            width=8 if bsize else 32)
        elif mn == 'test':
            bsize = (o[0] in REG8)
            self._flags_logic(self._val(o[0], byte=bsize) & self._val(o[1], byte=bsize),
                              width=8 if bsize else 32)
        elif mn == 'neg':
            v = self.rd_reg(o[0])
            self.wr_reg(o[0], self._flags_sub(0, v))
        elif mn == 'sbb':
            d, s = o
            a = self.rd_reg(d); b = self._val(s); c = self.CF
            total = b + c
            r = (a - total) & 0xFFFFFFFF
            self.ZF = 1 if r == 0 else 0
            self.SF = 1 if r & 0x80000000 else 0
            self.CF = 1 if a < total else 0
            self.OF = 1 if ((a ^ b) & (a ^ r) & 0x80000000) else 0
            self.wr_reg(d, r)
        elif mn == 'adc':
            d, s = o
            a = self.rd_reg(d); b = self._val(s); c = self.CF
            r = (a + b + c) & 0xFFFFFFFF
            self.ZF = 1 if r == 0 else 0
            self.SF = 1 if r & 0x80000000 else 0
            self.CF = 1 if (a + b + c) > 0xFFFFFFFF else 0
            self.OF = 1 if (~(a ^ b) & (a ^ r) & 0x80000000) else 0
            self.wr_reg(d, r)
        elif mn == 'cdq':
            self.R['edx'] = 0xFFFFFFFF if (self.R['eax'] & 0x80000000) else 0
        elif mn in ('idiv', 'div'):
            divisor = self._val(o[0])
            dividend = (self.R['edx'] << 32) | self.R['eax']
            if mn == 'idiv':
                if dividend & (1 << 63):
                    dividend -= (1 << 64)
                dv = divisor - 0x100000000 if divisor & 0x80000000 else divisor
                q = int(dividend / dv) if dv != 0 else 0   # truncate toward 0
                rem = dividend - q * dv
            else:
                q = dividend // divisor if divisor else 0
                rem = dividend % divisor if divisor else 0
            self.R['eax'] = q & 0xFFFFFFFF
            self.R['edx'] = rem & 0xFFFFFFFF
        elif mn == 'imul':
            if len(o) == 3:
                a = self._val(o[1]); b = self._val(o[2]); dst = o[0]
                sa = a - 0x100000000 if a & 0x80000000 else a
                sb = b - 0x100000000 if b & 0x80000000 else b
                self.wr_reg(dst, (sa * sb) & 0xFFFFFFFF)
            elif len(o) == 2:
                a = self.rd_reg(o[0]); b = self._val(o[1]); dst = o[0]
                sa = a - 0x100000000 if a & 0x80000000 else a
                sb = b - 0x100000000 if b & 0x80000000 else b
                self.wr_reg(dst, (sa * sb) & 0xFFFFFFFF)
            else:                                   # one-operand: edx:eax = eax * r/m (signed)
                a = self.R['eax']; b = self._val(o[0])
                sa = a - 0x100000000 if a & 0x80000000 else a
                sb = b - 0x100000000 if b & 0x80000000 else b
                p = (sa * sb) & 0xFFFFFFFFFFFFFFFF
                self.R['eax'] = p & 0xFFFFFFFF; self.R['edx'] = (p >> 32) & 0xFFFFFFFF
        elif mn == 'mul':                           # one-operand: edx:eax = eax * r/m (unsigned)
            b = self._val(o[0])
            p = (self.R['eax'] * b) & 0xFFFFFFFFFFFFFFFF
            self.R['eax'] = p & 0xFFFFFFFF; self.R['edx'] = (p >> 32) & 0xFFFFFFFF
        elif mn == 'not':
            bs = o[0] in REG8
            self._wr(o[0], (~self._rd(o[0], bs)) & (0xFF if bs else 0xFFFFFFFF), bs)
        elif mn in ('shl', 'sal', 'shr', 'sar'):
            d, s = o
            cnt = self._val(s) & 0x1F
            v = self.rd_reg(d)
            if mn in ('shl', 'sal'):
                r = (v << cnt) & 0xFFFFFFFF
            elif mn == 'shr':
                r = v >> cnt
            else:  # sar
                sv = v - 0x100000000 if v & 0x80000000 else v
                r = (sv >> cnt) & 0xFFFFFFFF
            self.wr_reg(d, r); self._flags_logic(r)
        elif mn == 'xor':
            d, s = o
            bs = (d in REG8) or (s in REG8)
            if d == s:
                self._wr(d, 0, bs); self._flags_logic(0, width=8 if bs else 32)
            else:
                r = self._rd(d, bs) ^ self._val(s, byte=bs)
                self._wr(d, r, bs); self._flags_logic(r, width=8 if bs else 32)
        elif mn == 'or':
            d, s = o
            bs = (d in REG8) or (s in REG8)
            r = self._rd(d, bs) | self._val(s, byte=bs)
            self._wr(d, r, bs); self._flags_logic(r, width=8 if bs else 32)
        elif mn == 'and':
            d, s = o
            bs = (d in REG8) or (s in REG8)
            r = self._rd(d, bs) & self._val(s, byte=bs)
            self._wr(d, r, bs); self._flags_logic(r, width=8 if bs else 32)
        elif mn == 'push':
            st['esp'] = u32(st['esp'] - 4); self.wr_i(st['esp'], self._val(o[0]))
        elif mn == 'pop':
            self.wr_reg(o[0], self.rd_i(st['esp'])); st['esp'] = u32(st['esp'] + 4)
        elif mn == 'jmp':
            t = o[0]
            if t.startswith('['):
                return self.rd_i(self.mem_addr(t))     # indirect: jump table / import tail-call
            if t in self.R or t in REG16 or t in REG8:
                return self.rd_reg(t)                  # computed (register) jump
            return int(t, 16)
        elif mn.startswith('j'):
            return int(o[0], 16) if self._cond(mn) else None
        elif mn == 'nop':
            pass
        elif mn.startswith('set'):
            # setCC dest8 -- store the flag condition (1/0) into a byte operand
            self._wr(o[0], 1 if self._cond('j' + mn[3:]) else 0, byte=True)
        # ---- x87 ----
        elif mn == 'fld':
            if o[0].startswith('st'):
                self.st.insert(0, self.st[int(re.search(r'\d', o[0]).group())])
            else:
                self.st.insert(0, rdf(self.mem_addr(o[0])))
        elif mn == 'fild':
            self.st.insert(0, float(s32(self.rd_i(self.mem_addr(o[0])))))
        elif mn in ('fiadd', 'fisub', 'fisubr', 'fimul', 'fidiv', 'fidivr'):
            # x87 op with an INTEGER memory operand: read the int, widen to
            # float, apply to st(0).  `word ptr` is a signed 16-bit source;
            # otherwise a signed 32-bit dword (as `fild` reads).
            addr = self.mem_addr(o[0])
            if 'word ptr' in o[0] and 'dword ptr' not in o[0]:
                raw = self.mem.get(addr, 0) | self.mem.get(addr + 1, 0) << 8
                other = float(raw - 0x10000 if raw & 0x8000 else raw)
            else:
                other = float(s32(self.rd_i(addr)))
            a = self.st[0]
            if   mn == 'fimul':  self.st[0] = a * other
            elif mn == 'fiadd':  self.st[0] = a + other
            elif mn == 'fisub':  self.st[0] = a - other
            elif mn == 'fisubr': self.st[0] = other - a
            elif mn == 'fidiv':  self.st[0] = _ieee_div(a, other)
            elif mn == 'fidivr': self.st[0] = _ieee_div(other, a)
        elif mn == 'fst':
            wrf(self.mem_addr(o[0]), self.st[0])
        elif mn == 'fstp':
            if o[0].startswith('st'):
                i = int(re.search(r'\d', o[0]).group()); self.st[i] = self.st[0]; self.st.pop(0)
            else:
                wrf(self.mem_addr(o[0]), self.st[0]); self.st.pop(0)
        elif mn in ('fmul', 'fadd', 'fsub', 'fsubr', 'fdiv', 'fdivr'):
            if o and o[0].startswith('st'):
                other = self.st[int(re.search(r'\d', o[0]).group())]
            else:
                other = rdf(self.mem_addr(o[0]))
            a = self.st[0]
            if   mn == 'fmul':  self.st[0] = a * other
            elif mn == 'fadd':  self.st[0] = a + other
            elif mn == 'fsub':  self.st[0] = a - other
            elif mn == 'fsubr': self.st[0] = other - a
            elif mn == 'fdiv':  self.st[0] = _ieee_div(a, other)
            elif mn == 'fdivr': self.st[0] = _ieee_div(other, a)
        elif mn == 'fchs':
            self.st[0] = -self.st[0]
        elif mn == 'fabs':
            self.st[0] = abs(self.st[0])
        elif mn == 'fsqrt':
            self.st[0] = math.sqrt(self.st[0]) if self.st[0] >= 0.0 else float('nan')
        elif mn in ('fsin', 'fcos'):
            v = self.st[0]
            if v != v or v in (float('inf'), float('-inf')):
                self.st[0] = float('nan')
            else:
                self.st[0] = math.sin(v) if mn == 'fsin' else math.cos(v)
        elif mn == 'fsincos':                     # st0 -> sin; push cos: st0=cos, st1=sin
            v = self.st[0]
            bad = (v != v or v in (float('inf'), float('-inf')))
            self.st[0] = float('nan') if bad else math.sin(v)
            self.st.insert(0, float('nan') if bad else math.cos(v))
        elif mn == 'fptan':                       # st0 -> tan; push 1.0
            v = self.st[0]
            self.st[0] = float('nan') if (v != v or v in (float('inf'), float('-inf'))) else math.tan(v)
            self.st.insert(0, 1.0)
        elif mn == 'fpatan':                      # st1 = atan2(st1, st0); pop
            a = self.st[0]; b = self.st[1] if len(self.st) > 1 else 0.0
            try:
                r = math.atan2(b, a)
            except (ValueError, OverflowError):
                r = float('nan')
            self.st.pop(0)
            self.st[0] = r
        elif mn == 'fxch':
            i = int(re.search(r'\d', o[0]).group()) if o else 1
            self.st[0], self.st[i] = self.st[i], self.st[0]
        elif mn in ('faddp', 'fsubp', 'fmulp', 'fsubrp', 'fdivp', 'fdivrp'):
            i = int(re.search(r'\d', o[0]).group())
            a, b = self.st[i], self.st[0]
            if   mn == 'faddp':  self.st[i] = a + b
            elif mn == 'fsubp':  self.st[i] = a - b
            elif mn == 'fmulp':  self.st[i] = a * b
            elif mn == 'fsubrp': self.st[i] = b - a
            elif mn == 'fdivp':  self.st[i] = _ieee_div(a, b)
            elif mn == 'fdivrp': self.st[i] = _ieee_div(b, a)
            self.st.pop(0)
        elif mn in ('fcom', 'fcomp', 'fcompp'):
            other = (rdf(self.mem_addr(o[0])) if (o and o[0].startswith('['))
                     else self.st[int(re.search(r'\d', o[0]).group())] if o else self.st[1])
            a = self.st[0]
            self.C0 = 1 if (math.isnan(a) or math.isnan(other) or a < other) else 0
            if mn == 'fcomp':
                self.st.pop(0)
            elif mn == 'fcompp':
                self.st.pop(0); self.st.pop(0)
        elif mn == 'fnstsw':
            st['eax'] = (st['eax'] & 0xFFFF00FF) | ((self.C0 & 1) << 8)
        elif mn == 'fldz':
            self.st.insert(0, 0.0)
        elif mn == 'fld1':
            self.st.insert(0, 1.0)
        elif ('stosd' in mn or 'stosb' in mn or 'stosw' in mn
              or 'movsd' in mn or 'movsb' in mn or 'scasb' in mn):
            # x86 string ops (DF assumed 0 -- these functions never set it).
            # ecx is bounded so a garbage-seeded count cannot run away; the bound
            # is identical on both sides, so it never manufactures a divergence.
            rep = mn.startswith('rep')
            CAP = 1 << 16
            n = min(self.R['ecx'], CAP) if rep else 1
            if 'stosd' in mn:
                for _ in range(n):
                    self.wr_i(self.R['edi'], self.R['eax']); self.R['edi'] = u32(self.R['edi'] + 4)
                if rep: self.R['ecx'] = u32(self.R['ecx'] - n)
            elif 'stosb' in mn:
                al = self.R['eax'] & 0xFF
                for _ in range(n):
                    self.wr_u8(self.R['edi'], al); self.R['edi'] = u32(self.R['edi'] + 1)
                if rep: self.R['ecx'] = u32(self.R['ecx'] - n)
            elif 'stosw' in mn:
                ax = self.R['eax'] & 0xFFFF
                for _ in range(n):
                    self.wr_u8(self.R['edi'], ax & 0xFF)
                    self.wr_u8(u32(self.R['edi'] + 1), (ax >> 8) & 0xFF)
                    self.R['edi'] = u32(self.R['edi'] + 2)
                if rep: self.R['ecx'] = u32(self.R['ecx'] - n)
            elif 'movsd' in mn:
                for _ in range(n):
                    self.wr_i(self.R['edi'], self.rd_i(self.R['esi']))
                    self.R['esi'] = u32(self.R['esi'] + 4); self.R['edi'] = u32(self.R['edi'] + 4)
                if rep: self.R['ecx'] = u32(self.R['ecx'] - n)
            elif 'movsb' in mn:
                for _ in range(n):
                    self.wr_u8(self.R['edi'], self.rd_u8(self.R['esi']))
                    self.R['esi'] = u32(self.R['esi'] + 1); self.R['edi'] = u32(self.R['edi'] + 1)
                if rep: self.R['ecx'] = u32(self.R['ecx'] - n)
            elif 'scasb' in mn:
                al = self.R['eax'] & 0xFF; cnt = 0
                while (self.R['ecx'] if rep else 1) and cnt < CAP:
                    b = self.rd_u8(self.R['edi']); self.R['edi'] = u32(self.R['edi'] + 1)
                    if rep: self.R['ecx'] = u32(self.R['ecx'] - 1)
                    cnt += 1
                    self.ZF = 1 if al == b else 0
                    if 'repne' in mn and al == b: break
                    if ('repe' in mn or mn == 'rep scasb') and al != b: break
                    if not rep: break
        else:
            raise ValueError('unhandled %s %s @%08X' % (mn, ops, addr))
        return None


def load_listing(path):
    out = []
    for ln in open(path):
        m = re.match(r'^([0-9A-F]{8})\s+[0-9a-f]+\s+(\w+)\s*([^;]*?)\s*(?:;.*)?$', ln)
        if not m:
            continue
        out.append((int(m.group(1), 16), m.group(2), m.group(3).strip()))
    return out


def load_many(*paths):
    prog = []
    for p in paths:
        prog += load_listing(p)
    return prog
