"""Valid-state seeding profiles for the A5 behavioural oracle (t3b_verify).

The default oracle seeds memory with random bytes.  That is right for a leaf
function whose inputs are scalars in registers/stack -- a random int is a valid
int.  It is WRONG for an orchestrator whose inputs are an object graph: a random
value in a pointer-valued global points nowhere, and the function then walks
garbage down branches the compiler proved unreachable, so the two compilations
diverge on paths that never run with real state.  BrRaceStep is the archetype.

A profile pins a VALID, consistent world for such a function: pointer-valued
globals resolve to zeroed scratch structs (null-safe), and the scalar state/flag
globals that gate control flow are varied deterministically per seed so the whole
branch space is exercised.  `bss(seed, addr)` returns the seeded byte for a BSS
address; everything not named is 0 (a null-safe pointer / cleared flag).

Adding a profile is a decision about a function's input SHAPE, not a lowering of
the standard: the oracle still runs both sides on identical inputs and compares
every output.  A profile that seeds an invalid world would surface as spurious
DIFFs, not false EQUIVALENTs.
"""


def _b(val, a, base):
    """Byte `a-base` of the little-endian dword `val`."""
    return (val >> (8 * (a - base))) & 0xFF


class Profile(object):
    __slots__ = ('bss', 'zero_stack', 'seeds', 'arg', 'buf')

    def __init__(self, bss, zero_stack=True, seeds=64, arg=None, buf=None):
        self.bss = bss                 # fn(seed, addr) -> byte
        self.zero_stack = zero_stack    # stack window / arg buffers seeded to 0
        self.seeds = seeds
        # arg(seed, idx) -> value or None: pin a scalar argument to a valid,
        # bounded value (None = default random).  An orchestrator that takes a
        # buffer+length needs the length bounded, or a random one walks a
        # 2000-byte "packet" of noise for a million steps; and a gating id must
        # be able to match the seeded owner ids for the live paths to run.
        self.arg = arg
        # buf(seed, argidx, off) -> byte or None: seed the CONTENT of a
        # pointer-argument buffer (None = default).  A random buffer for a
        # command-stream reader is not just unbounded -- an invalid command can
        # rewind the reader and never terminate -- so a packet dispatcher needs
        # a WELL-FORMED command sequence built here, not noise.
        self.buf = buf


# ---- BrRaceStep 0x10019A70 -------------------------------------------------
# State machine + gating flags exercised; the driver pointer-arrays point at
# distinct zeroed 0x2b68 scratch structs so the per-driver deref loops run on
# valid data.  Verified EQUIV-MODULO-FP across states, flag combos and drivers.
_BRS_GATES = (
    0x105CCB94, 0x105CCB88, 0x10226A44, 0x10226A48, 0x10226A4C, 0x100B3858,
    0x100B2F00, 0x100B2F04, 0x105BC8F8, 0x105CCB8C, 0x104B15E8, 0x105BC760,
    0x105CCB98, 0x10226A34,
)
_BRS_GATE_IX = {g: i for i, g in enumerate(_BRS_GATES)}
_BRS_DRV_ARR = (0x10AF3BC8, 0x10AF1208, 0x10AF1370, 0x10AF1374, 0x10AF3D70)
_BRS_SCRATCH = 0x10E00000


def _bracestep_bss(seed, a):
    base = a & ~3
    if 0x100A9360 <= a < 0x100A9364:          # g_0A9360 race state -> 0..6
        return _b(seed % 7, a, 0x100A9360)
    ix = _BRS_GATE_IX.get(base)
    if ix is not None:                        # gating flag -> small varied value
        val = (seed * 2654435761 + ix * 40503) % 4
        return _b(val, a, base)
    for k, arr in enumerate(_BRS_DRV_ARR):    # driver pointer -> zeroed scratch struct
        if arr <= a < arr + 4:
            return _b(_BRS_SCRATCH + k * 0x4000, a, arr)
    return 0                                  # else: null-safe / cleared


# ---- FUN_1002f790 0x1002F790 (remote-peer packet applier) ------------------
# Orchestrator: (pNet, pBuf, nBytes, idFrom, a5) + two 16-slot global tables
# (peer 0x117A9B88, record 0x117B3258, stride 0x96C).  The packet buffer is the
# real seeded arg buffer (varied bytes -> varied commands), but its length must
# be BOUNDED or a 2000-byte noise "packet" runs forever; and the per-slot owner
# id / state gate every live path, so seed them to small varied values with the
# owner id drawn from the same tiny set idFrom is pinned to, so ownership
# matches on some seeds.  Pointer-valued fields (the mutex handles, ring cars)
# stay 0 (null-safe; a WaitForSingleObject(0) is black-boxed identically).
_F2F_PEER = 0x117A9B88
_F2F_REC = 0x117B3258
_F2F_STRIDE = 0x96C


def _f2f_bss(seed, a):
    base = a & ~3
    # peer[slot] owner id (+0x04) and state (+0x2C), 16 slots
    for k in range(16):
        sl = _F2F_PEER + k * _F2F_STRIDE
        if base == sl + 0x04:
            return _b((seed + k) % 3, a, sl + 0x04)          # id in {0,1,2}
        if base == sl + 0x2C:
            return _b((seed * 2654435761 + k) % 6, a, sl + 0x2C)  # state 0..5
    # record[idx] timestamp (+0x08) small so freshness compares both ways
    for k in range(256):
        rc = _F2F_REC + k * _F2F_STRIDE
        if base == rc + 0x08:
            return _b((seed + k) % 4, a, rc + 0x08)
        if base == rc + 0x2C:
            return _b((seed + k) % 3, a, rc + 0x2C)
    return 0                                                 # null-safe / cleared


# A packet is a 3-byte header then a command stream; the reader is BYTE-aligned
# but blocks (an internal refill loop) if a command reads past the declared
# length, so the packet must hold COMPLETE commands and nBytes must land exactly
# on a command boundary.  Build one well-formed cmd 0x00 (the richest table-only
# case: record ring when the 0x10 bit is set, else the flag latch; optional
# name), with the seed varying the 0x10 bit, the target slot (0..2, so it meets
# the seeded owner ids and idFrom), the sub-slot, the flags (kind + the 0x40/0x80
# latch bits), and whether a 24-byte name follows.  0x40/0x80/0x60 are excluded:
# they run the car-state codecs / rewind on the bytes that follow, which on
# non-codec garbage is a seed-world artifact, not this function's behaviour.
def _f2f_packet(seed):
    name = (seed >> 3) & 1
    kind = 0 if name else 3            # kind < 3 => a 24-byte name follows
    flags = kind | (((seed >> 1) & 3) << 6)   # bits 0x40/0x80 = finish/return latch
    b10 = 0x10 if (seed & 1) else 0
    slot = seed % 3
    b0 = (seed >> 4) % 3              # record sub-slot (for the 0x10 path)
    pkt = [0x11, 0x22, 0x33]         # 3-byte header (timestamp)
    pkt += [0x00 | b10 | slot]       # command byte (class 0x00)
    pkt += [b0, flags & 0xFF, (seed * 7 + 1) & 0xFF]  # b0, flags, nib
    pkt += [(seed * 3 + 5) & 0xFF, (seed * 5 + 9) & 0xFF, (seed + 0x40) & 0xFF]  # ca,cb,cc
    pkt += [(seed * 11 + 3) & 0xFF, seed & 0xFF, 0, 0]                            # id (S32)
    if name:
        nm = ('P%d' % (seed % 100)).encode('ascii')
        nm = nm + b'\0' * (0x18 - len(nm))
        pkt += list(nm[:0x18])
    return pkt


def _f2f_arg(seed, idx):
    if idx == 2:                       # nBytes: exactly the built packet's length
        return len(_f2f_packet(seed))
    if idx == 3:                       # idFrom == 1: matches owner id 1 on some
        return 1                       # slots; and never != 1, so the tail does
    return None                        # not recursively re-dispatch the packet


def _f2f_buf(seed, argidx, off):
    if argidx != 1:                    # only pBuf (arg index 1) is the packet
        return None
    pkt = _f2f_packet(seed)
    return pkt[off] if off < len(pkt) else 0


# ---- BrTex3dExpand 0x100250D0 (N64 texture-format expander) ----------------
# Inputs are an object graph: two pointer args get real buffers (dst=arg0,
# src=arg3), but the palette (arg4) and the tile-record base (arg10) are typed
# `int` in the prototype though used as pointers, so pin them to scratch regions
# and seed the tile-record fields here.  The record shift fields (+0x20 cols,
# +0x24 rows, and +0x60/+0x64 for the CI4 tile-1 arm) become `1 << n` loop
# counts, so they MUST be small or a run never terminates; the source-offset
# (+0x0c) and row-stride (+0x08,+0x48) fields stay 0 so the source cursor stays
# inside its seeded buffer (reads past it return 0 identically on both sides).
# The output byte budget (arg1 cbMax) is bounded so the writes stay inside the
# 0x100-byte dst buffer.  The mode/format/sub/flag/mirror/interleave selectors
# are varied per seed to exercise every expansion arm.
_TEX_RECS = 0x10E00000          # scratch tile-record base (arg10)
_TEX_PAL = 0x10E20000           # scratch palette base (arg4)


def _tex_bss(seed, a):
    base = a & ~3
    for k in range(8):                          # tile records, 0x40 stride
        rc = _TEX_RECS + k * 0x40
        if base == rc + 0x20:                   # cols shift -> 1<<(n-1) or 1<<n
            return _b(2, a, rc + 0x20)
        if base == rc + 0x24:                   # rows shift
            return _b(2, a, rc + 0x24)
    if base == _TEX_RECS + 0x60:                # CI4 tile-1 cols shift
        return _b(2, a, _TEX_RECS + 0x60)
    if base == _TEX_RECS + 0x64:                # CI4 tile-1 rows shift
        return _b(2, a, _TEX_RECS + 0x64)
    if _TEX_PAL <= a < _TEX_PAL + 0x200:        # palette: distinct entry per index
        return ((a - _TEX_PAL) * 7 + 0x13) & 0xFF   # so a wrong CLUT index diverges
    return 0                                     # source offsets / strides -> 0


def _tex_arg(seed, idx):
    mode = seed % 3                              # param_3: 0 expand / 1 alt / 2 raw
    if idx == 1:                                 # param_2 cbMax: bound to the dst buffer
        return 0x80
    if idx == 2:                                 # param_3 mode
        return mode
    if idx == 4:                                 # param_5 palette base (scratch)
        return _TEX_PAL
    if idx == 5:                                 # param_6 format
        if mode == 2:
            return 0                             # mode 2 only acts on fmt 0
        return (2, 3, 4)[(seed // 3) % 3]
    if idx == 6:                                 # param_7 mirror H
        return (seed // 4) & 1
    if idx == 7:                                 # param_8 mirror V
        return (seed // 8) & 1
    if idx == 8:                                 # param_9 tile0
        return 0
    if idx == 9:                                 # param_10 tile1 (two tiles -> idx 1 exists)
        return 2
    if idx == 10:                               # param_11 tile-record base (scratch)
        return _TEX_RECS
    if idx == 11:                               # param_12 flags (bit 2 = CI4 tile-1 arm)
        return 2 if ((seed // 2) & 1) else 0
    if idx == 12:                               # param_13 sub-format (blend vs direct)
        return (seed // 9) & 1
    if idx == 21:                               # param_22 row-interleave mask
        return 1 if (seed & 1) else 0
    return None                                  # blend endpoints (13..20): random low byte is fine


# ---- BrSceneDlBuild 0x1000EAF0 (per-frame scene display-list builder) -------
# Orchestrator: loops over the live scene objects/drivers (count in g_0B2F04)
# building each one's matrix + DL, then the trail quads.  g_0B2F04 must be small
# or the object loop runs for millions of iterations on a garbage count; the
# per-object pointer arrays stay null-safe (0) so the derefs land on zeroed
# scratch, and any gating flag reads 0.  Built incrementally against the runaway
# blockers.
def _scenedl_bss(seed, a):
    base = a & ~3
    if base == 0x100B2F04:                       # live object / driver count
        return _b(seed % 3 + 1, a, 0x100B2F04)   # 1..3
    return 0                                      # null-safe pointers / cleared flags


PROFILES = {
    0x1000EAF0: Profile(_scenedl_bss, zero_stack=False, seeds=32),
    0x100250D0: Profile(_tex_bss, zero_stack=False, seeds=64, arg=_tex_arg),
    0x10019A70: Profile(_bracestep_bss),
    0x1002F790: Profile(_f2f_bss, zero_stack=False, seeds=48,
                        arg=_f2f_arg, buf=_f2f_buf),
    # 0x100038F0 is the base (non-remote) dispatcher; identical table/packet
    # layout, so the same seeding applies.  Its arg 3 is nMode (not idFrom); the
    # arg hook pins it to 1, the mode the main command arms all run under.
    0x100038F0: Profile(_f2f_bss, zero_stack=False, seeds=48,
                        arg=_f2f_arg, buf=_f2f_buf),
}


def get(va):
    """Profile for a function VA, or None (default random seeding)."""
    return PROFILES.get(va)
