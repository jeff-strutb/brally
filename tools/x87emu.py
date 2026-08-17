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
REG8 = {'al': ('eax', 0), 'ah': ('eax', 8), 'bl': ('ebx', 0), 'bh': ('ebx', 8),
        'cl': ('ecx', 0), 'ch': ('ecx', 8), 'dl': ('edx', 0), 'dh': ('edx', 8)}
REG32 = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')


class Machine:
    def __init__(self, mem, regs, listing):
        self.mem = mem                      # dict: byte-address -> 0..255
        self.R = dict(regs)
        self.st = []                        # x87 stack, st[0] is TOP
        self.ZF = self.SF = self.CF = self.OF = 0
        self.C0 = 0                         # last fcom below/unordered
        self.callstack = []
        self.FTOL = 0x10074560
        self.prog = listing
        self.idx = {a: i for i, (a, _, _) in enumerate(listing)}

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
        raise KeyError(name)

    def wr_reg(self, name, v):
        if name in self.R:
            self.R[name] = u32(v)
        elif name in REG8:
            parent, sh = REG8[name]
            mask = 0xFF << sh
            self.R[parent] = (self.R[parent] & ~mask) | ((v & 0xFF) << sh)
        else:
            raise KeyError(name)

    def _val(self, x, byte=False):
        x = x.strip()
        if x in self.R or x in REG8:
            return self.rd_reg(x)
        if re.fullmatch(r'-?0x[0-9a-fA-F]+|-?\d+', x):
            return int(x, 0) & 0xFFFFFFFF
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
            if mn == 'call':
                target = int(ops.strip(), 16)
                if target == self.FTOL:
                    # _ftol is cdecl-with-no-args: on hardware the call pushes a
                    # return address and the callee's ret pops it, net esp change
                    # zero -- which is what leaving esp alone here models.
                    v = self.st.pop(0)
                    self.R['eax'] = int(math.trunc(v)) & 0xFFFFFFFF
                    pc += 1; continue
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
                raise ValueError('call to unmapped %08X' % target)
            if mn == 'ret':
                if self.callstack:
                    self.R['esp'] = u32(self.R['esp'] + 4)   # pop the return addr
                    pc = self.callstack.pop(); continue
                return
            nxt = self.step(addr, mn, ops)
            pc = self.idx[nxt] if nxt is not None else pc + 1

    def step(self, addr, mn, ops):
        st = self.R
        ops = re.sub(r'\b(?:dword|qword|word|byte) ptr ', '', ops)
        byte = mn in ('movzx', 'movsx') or ' al' in (',' + ops) or False
        o = [x.strip() for x in ops.split(',')] if ops else []

        def is8(x):
            return x in REG8 or (x.startswith('[') and False)  # byte flagged via mnemonic size
        # ---- integer ----
        if mn == 'mov':
            d, s = o
            bsize = (d in REG8) or (s in REG8)
            v = self._val(s, byte=bsize)
            if d.startswith('['):
                (self.wr_u8 if bsize else self.wr_i)(self.mem_addr(d), v)
            else:
                self.wr_reg(d, v)
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
            r = self._flags_add(self.rd_reg(d), self._val(s))
            self.wr_reg(d, r)
        elif mn == 'sub':
            d, s = o
            r = self._flags_sub(self.rd_reg(d), self._val(s))
            self.wr_reg(d, r)
        elif mn == 'inc':
            cf = self.CF; self.wr_reg(o[0], self._flags_add(self.rd_reg(o[0]), 1)); self.CF = cf
        elif mn == 'dec':
            cf = self.CF; self.wr_reg(o[0], self._flags_sub(self.rd_reg(o[0]), 1)); self.CF = cf
        elif mn == 'cmp':
            bsize = (o[0] in REG8) or (o[1] in REG8)
            self._flags_sub(self._val(o[0], byte=bsize), self._val(o[1], byte=bsize),
                            width=8 if bsize else 32)
        elif mn == 'test':
            bsize = (o[0] in REG8)
            self._flags_logic(self._val(o[0], byte=bsize) & self._val(o[1], byte=bsize),
                              width=8 if bsize else 32)
        elif mn == 'xor':
            d, s = o
            if d == s:
                self.wr_reg(d, 0); self._flags_logic(0)
            else:
                r = self.rd_reg(d) ^ self._val(s); self.wr_reg(d, r); self._flags_logic(r)
        elif mn == 'or':
            d, s = o
            r = self.rd_reg(d) | self._val(s); self.wr_reg(d, r); self._flags_logic(r)
        elif mn == 'and':
            d, s = o
            r = self.rd_reg(d) & self._val(s); self.wr_reg(d, r); self._flags_logic(r)
        elif mn == 'push':
            st['esp'] = u32(st['esp'] - 4); self.wr_i(st['esp'], self._val(o[0]))
        elif mn == 'pop':
            self.wr_reg(o[0], self.rd_i(st['esp'])); st['esp'] = u32(st['esp'] + 4)
        elif mn == 'jmp':
            return int(o[0], 16)
        elif mn.startswith('j'):
            return int(o[0], 16) if self._cond(mn) else None
        elif mn == 'nop':
            pass
        # ---- x87 ----
        elif mn == 'fld':
            if o[0].startswith('st'):
                self.st.insert(0, self.st[int(re.search(r'\d', o[0]).group())])
            else:
                self.st.insert(0, self.rd_f(self.mem_addr(o[0])))
        elif mn == 'fild':
            self.st.insert(0, float(s32(self.rd_i(self.mem_addr(o[0])))))
        elif mn == 'fst':
            self.wr_f(self.mem_addr(o[0]), self.st[0])
        elif mn == 'fstp':
            if o[0].startswith('st'):
                i = int(re.search(r'\d', o[0]).group()); self.st[i] = self.st[0]; self.st.pop(0)
            else:
                self.wr_f(self.mem_addr(o[0]), self.st[0]); self.st.pop(0)
        elif mn in ('fmul', 'fadd', 'fsub', 'fsubr', 'fdiv', 'fdivr'):
            if o and o[0].startswith('st'):
                other = self.st[int(re.search(r'\d', o[0]).group())]
            else:
                other = self.rd_f(self.mem_addr(o[0]))
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
            other = (self.rd_f(self.mem_addr(o[0])) if (o and o[0].startswith('['))
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
