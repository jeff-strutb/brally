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


PROFILES = {
    0x10019A70: Profile(_bracestep_bss),
    0x1002F790: Profile(_f2f_bss, zero_stack=False, seeds=48,
                        arg=_f2f_arg, buf=_f2f_buf),
}


def get(va):
    """Profile for a function VA, or None (default random seeding)."""
    return PROFILES.get(va)
