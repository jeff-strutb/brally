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
    __slots__ = ('bss', 'zero_stack', 'seeds', 'arg', 'buf', 'exact_regions',
                 'buf_sizes', 'stub_calls')

    def __init__(self, bss, zero_stack=True, seeds=64, arg=None, buf=None,
                 exact_regions=None, buf_sizes=None, stub_calls=None):
        self.bss = bss                 # fn(seed, addr) -> byte
        self.zero_stack = zero_stack    # stack window / arg buffers seeded to 0
        self.seeds = seeds
        # {argidx: bytes}: enlarge a pointer-argument buffer past the default so
        # a large object (a ~0x2a00 car passed as `this`) fits and the buf hook
        # can seed its pointer fields to point at further scratch sub-objects.
        self.buf_sizes = buf_sizes
        # [(lo, hi), ...] ranges of INTEGER output (e.g. a display-list command
        # buffer) where a differing dword is a real divergence, never x87
        # rounding noise -- the oracle would otherwise mask a 1-integer command
        # difference as a 1-ULP float difference (t3b_verify._classify_diff).
        self.exact_regions = tuple(exact_regions or ())
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
        # {callee VA, ...}: direct calls to BLACK-BOX (skip the body, eax<-0,
        # esp net-zero as a cdecl call is), recording the target+arg window as
        # an observable event compared between the two sides.  For an
        # orchestrator that hands a whole seeded slot to a heavy subsystem the
        # transcription is not certifying (a renderer, a logger): running the
        # subsystem on the seeded world would loop or walk into noise, and both
        # sides call it identically, so its internal effects are not this
        # function's contract.  A different target/arg sequence still surfaces;
        # the blend/output this function itself computes is compared as usual,
        # so stubbing cannot manufacture a false EQUIVALENT of THIS function.
        self.stub_calls = frozenset(stub_calls or ())


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


# ---- BrObjDlBuild 0x1000CBA0 (one scene object -> display-list commands) ----
# Two paths off the top guard: the CHEAP path (3 commands + counter bumps) when
# cls==0 or a gate is clear, and the EXPANDED path (walk the object's command
# list, transform every vertex through the object matrix, clip triangles) when
# all gates are set. Both need the pointer-globals (DL write cursor, the pDL/pVtx
# expansion arenas, the object table) pointing at scratch, and the object record
# + command stream + scene block seeded. Vertex/matrix floats are seeded small so
# the x87 transform math does not overflow. The command stream is a short, well
# formed G_VTX(0x04) + G_TRI1(0xbf) + G_ENDDL(0xb8) walk. `seed & 1` picks the
# cheap vs expanded path via the g_B71538 gate; swclip (g_10396EB0) is on so the
# software clipper (FUN_1000dc00) runs.
#
# ‼ PARTIAL TEETH -- NOT A FULL T3 CERT.  This profile verifies the INTEGER
# display-list output exactly (via exact_regions: DL cursor + batch arenas +
# counters); a wrong command word, a wrong counter, or a gross transform error is
# caught.  It does NOT reliably catch a subtle vertex-transform coefficient swap
# or a clip-flag/corner-index bug: those effects flow through the sprite-matrix
# pipeline and the FUN_1000dc00 clipper subsystem, whose float output cannot be
# driven into bug-sensitive clip configurations from the input seeds (the
# transform is opaque, so the transformed coords cannot be placed astride the
# 0/1024 clip thresholds on demand).  Do NOT read EQUIVALENT here as a T3 cert
# for the geometry/clip logic -- that half wants the T4 byte-grind (see the
# br_objdl.c header) or a much deeper clipper-output profile.
_ODL_CUR = 0x10E00000            # DL write cursor (g_6E7710)
_ODL_PDL = 0x10E40000            # expanded-batch arena (g_35F7D8 / base g_2E16B0)
_ODL_PVTX = 0x10E60000           # expanded-vertex arena (g_35FAEC / base g_35FBA4)
_ODL_OBJTAB = 0x10E10000         # object table base (g_6EED38), 0x54 stride
_ODL_CMD = 0x10E50000            # the object's stored command list (rec+0x44)
_ODL_SCENE = 0x10E80000          # pScene arg
_ODL_RECTS = 0x10EA0000          # pRects arg
_ODL_VSRC = 0x10E30000           # G_VTX source vertex block (cmd word 2)
_ODL_TEX = 0x10EC0000            # texture record (pObj+0x294 -> here)

# The object matrix and the two source vertices are made non-degenerate AND
# varied per seed: 16 different geometric configurations, each with distinct,
# well-separated coefficients and mixed-sign vertices that straddle the 0 / 1024
# clip thresholds.  A degenerate or fixed world lets a wrong matrix element or a
# clip-flag bug collapse to sub-tolerance rounding on every seed; varying the
# geometry gives each subtle transform/clip bug a configuration that exposes it.
def _odl_mtx(seed, k):
    sign = -1.0 if ((seed * 3 + k) & 1) else 1.0
    mant = 1.0 + ((seed * 7 + k * 13) % 17)
    scale = 10.0 ** (((seed + k) % 4) - 1)          # 0.1 .. 100
    return sign * mant * scale


def _odl_vtx(seed, vi, comp):
    s = seed * 131 + vi * 41 + comp * 7
    sign = -1.0 if ((s >> 2) & 1) else 1.0
    return sign * float((s % 173) + 1) * (10.0 ** ((s >> 4) % 3))   # 1 .. ~1.7e4


def _f32b(x, a, base):
    import struct
    return struct.unpack('<4B', struct.pack('<f', x))[a - base]


def _f32at(x, a):
    import struct
    return struct.unpack('<4B', struct.pack('<f', x))[a & 3]


def _odl_bss(seed, a):
    base = a & ~3
    # pointer-valued globals -> scratch arenas
    ptrs = {0x106E7710: _ODL_CUR, 0x1035F7D8: _ODL_PDL, 0x1035FAEC: _ODL_PVTX,
            0x102E16B0: _ODL_PDL, 0x1035FBA4: _ODL_PVTX, 0x106EED38: _ODL_OBJTAB}
    if base in ptrs:
        return _b(ptrs[base], a, base)
    if base == 0x10396EB0:                        # software-clip enable -> exercise CLIPTRI
        return _b(1, a, 0x10396EB0)
    if base == 0x10B71538:                        # gate: nonzero on odd seeds -> expanded
        return _b(1 if (seed & 1) else 0, a, base)
    # object record (idx 0) at the table base: object matrix (0..0x40),
    # command-list ptr (0x44), counters (0x4e/0x50/0x52)
    if _ODL_OBJTAB <= base < _ODL_OBJTAB + 0x40:  # 4x4 object matrix (memcpy'd to OUTM)
        return _f32at(_odl_mtx(seed, (base - _ODL_OBJTAB) >> 2), a)
    if base == _ODL_OBJTAB + 0x44:                # pCmd = *(rec+0x44)
        return _b(_ODL_CMD, a, _ODL_OBJTAB + 0x44)
    if base == _ODL_OBJTAB + 0x4c:                # 0x4d flag byte (bit1=0) + 0x4e=3
        return _b(0x00030000, a, _ODL_OBJTAB + 0x4c)
    if base == _ODL_OBJTAB + 0x50:                # 0x50=2, 0x52=4 (u16 counters)
        return _b(0x00040002, a, _ODL_OBJTAB + 0x50)
    # the object's stored command list, walked on the expanded path:
    #   G_VTX(0x04) n=2 from _ODL_VSRC, then G_TRI1(0xbf) corners 0/1/0,
    #   then G_ENDDL(0xb8).  Exercises the vertex transform and the clip arm.
    #   G_VTX(0x04) n=3 from _ODL_VSRC, then G_TRI1(0xbf) with corners 0/1/2
    #   (three distinct vertices, so a clip-flag bug flips the drop decision),
    #   then G_ENDDL(0xb8).
    cmd = {_ODL_CMD + 0x00: 0x04000C00, _ODL_CMD + 0x04: _ODL_VSRC,
           _ODL_CMD + 0x08: 0xBF000000, _ODL_CMD + 0x0C: 0x00020100,
           _ODL_CMD + 0x10: 0xB8000000}
    if base in cmd:
        return _b(cmd[base], a, base)
    # G_VTX source: three 0x20-byte vertices, x/y/z at word 0/1/2, seed-varied.
    # Gate on the WORD (off < 0xc), not the exact first byte: _f32at already
    # picks the right byte via a&3, so every byte of words 0/1/2 must be served.
    # (Serving only off in {0,4,8} zeroed bytes 1-3 of each float, collapsing
    # every vertex to ~0 -- which multiplied all transform coefficients by zero
    # and left the geometry/clip half with NO teeth: a false EQUIVALENT.)
    if _ODL_VSRC <= a < _ODL_VSRC + 0x60:
        vi = (a - _ODL_VSRC) // 0x20
        off = (a - _ODL_VSRC) % 0x20
        if off < 0x0c:
            return _f32at(_odl_vtx(seed, vi, off >> 2), a)
        return 0
    # sprite direction (pObjBase = pScene): dir=(3,4,*), aux vec at +0x10
    if base == _ODL_SCENE + 0x00: return _f32at(3.0, a)
    if base == _ODL_SCENE + 0x04: return _f32at(4.0, a)
    if base == _ODL_SCENE + 0x10: return _f32at(1.0, a)   # aux length vector
    # pTex pointer at pObj+0x294 (= pScene+0x29c4) and its two scale fields
    if base == _ODL_SCENE + 0x29c4:
        return _b(_ODL_TEX, a, _ODL_SCENE + 0x29c4)
    if base == _ODL_TEX + 0x80e0 or base == _ODL_TEX + 0x80e4:
        return _f32at(1.0, a)
    return 0                                       # everything else null-safe / 0


def _odl_arg(seed, idx):
    if idx == 0:                                   # pRects
        return _ODL_RECTS
    if idx == 1:                                   # idx -> record 0
        return 0
    if idx == 2:                                   # cls: one surface-class bit
        return 1
    if idx == 3:                                   # bLit
        return (seed >> 1) & 1
    if idx == 4:                                   # pScene
        return _ODL_SCENE
    return None


# The DL command arenas and the three geometry counters are INTEGER output
# (command words / counts); a differing dword there is a real divergence, not
# x87 rounding.  The vertex arena (_ODL_PVTX) and the matrix globals are FLOAT
# output and are deliberately left tolerant.
_ODL_EXACT = ((_ODL_CUR, _ODL_CUR + 0x10000), (_ODL_PDL, _ODL_PDL + 0x20000),
              (0x106E772C, 0x106E7730), (0x106E7734, 0x106E7738),
              (0x106E86A0, 0x106E86A4))

# ---- BrCtlAiBody 0x1005D770 (per-car AI throttle/steer body, __thiscall) ----
# `this` is a 0x2b68 BrAiCar (struct in br_ctlai.c).  Enlarge its buffer so every
# field is a real slot, then POPULATE it with a realistic, per-seed-varied driving
# state so the steering/throttle rules actually compute (a null car takes every
# degenerate branch and hides bugs).  Seed:
#   - the frame rows (fwd/right/up) + varied pos/vel/aim vectors;
#   - pProfile (+0xF00) -> scratch profile with f68 bit0 CLEAR (controller on);
#   - pCtl (+0x29C0) -> scratch control record;
#   - pNode (+0xF8C) -> scratch path node whose aPt[].arc knots DECREASE
#     monotonically, so the budget walk (t -= arc[i]-arc[i+1]) crosses real
#     segments and exits naturally -- not the instant exit a single huge knot
#     forced, which masked the budget logic.
# g_0B2F00 (live-driver count) bounds the outer driver loop over g_AF0858.
_AI_NODE = 0x10E00000            # scratch path node (car+0xF8C)
_AI_PROF = 0x10E10000            # scratch profile (car+0xF00)
_AI_CTL = 0x10E20000             # scratch control record (car+0x29C0)
_AI_NPT = 8                      # knots in the path node


def _u32b(val, off, base):
    return (val >> (8 * (off - base))) & 0xFF


def _aif(seed, tag):
    """A finite, per-seed-varied float in ~[-16, 16] for car kinematic state."""
    s = (seed * 2654435761 + tag * 40503) & 0xFFFFFFFF
    return ((s % 3200) - 1600) / 100.0


def _ai_vec(seed, base_off, off, tag, scale=1.0):
    import struct
    comp = (off - base_off) >> 2                  # 0/1/2 = x/y/z
    return struct.pack('<f', _aif(seed, tag + comp) * scale)[off & 3]


def _ctlai_bss(seed, a):
    import struct
    base = a & ~3
    if base == 0x100B2F00:                        # live-driver count -> 0..3
        return _b(seed % 4, a, 0x100B2F00)
    # ---- path node ----
    if base == _AI_NODE + 0x00:                    # pNext -> self (single-node ring)
        return _b(_AI_NODE, a, _AI_NODE + 0x00)
    # pSib stays NULL (default 0): the corridor scan's sibling loop
    # `while (pChild != NULL) pChild = pChild->pSib` must terminate, and the
    # main body only follows pSib when flags&1 (which is clear here).
    if base == _AI_NODE + 0x14:                    # count (u16) then flags (u16)=0
        return _b(_AI_NPT, a, _AI_NODE + 0x14)
    if _AI_NODE + 0x40 <= base < _AI_NODE + 0x40 + _AI_NPT * 0x28:
        rel = base - (_AI_NODE + 0x40)
        i, foff = rel // 0x28, rel % 0x28
        if foff == 0x24:                          # aPt[i].arc: decreasing knots
            return struct.pack('<f', float((_AI_NPT - i) * 7))[a - base]
        # A real corridor: left = centre + offset, right = centre - offset, so
        # the half-width dot(lateral, left-centre) lands in a realistic range and
        # the corridor-width thresholds (limit's 5.0f) are exercised.
        comp = (foff % 0x0C) >> 2                  # 0/1/2 within the BrVec3
        c = _aif(seed, 700 + i * 8 + comp)         # centre component ~[-16,16]
        o = _aif(seed, 900 + i * 8 + comp) * 0.45  # lateral offset ~[-7.2,7.2]
        v = c + o if foff < 0x0C else (c if foff < 0x18 else c - o)
        return struct.pack('<f', v)[a - base]
    # ---- profile ----
    if base == _AI_PROF + 0x68:                    # f68 bit0 = controller disabled -> 0
        return 0
    if base == _AI_PROF + 0x74:                    # difficulty-table row -> small
        return _b(seed % 3, a, _AI_PROF + 0x74)
    return 0                                       # everything else null-safe / cleared


def _ctlai_buf(seed, argidx, off):
    if argidx != 0:                               # only `this` (the car)
        return None
    if 0x00 <= off < 0x0c:                          # fwd = frame row 0, ~unit
        return _ai_vec(seed, 0x00, off, 100, 1.0 / 16.0)
    if 0x10 <= off < 0x1c:                          # right = frame row 1, ~unit
        return _ai_vec(seed, 0x10, off, 130, 1.0 / 16.0)
    if 0x20 <= off < 0x2c:                          # up = frame row 2, ~unit
        return _ai_vec(seed, 0x20, off, 160, 1.0 / 16.0)
    if 0x30 <= off < 0x3c:                          # pos, varied
        return _ai_vec(seed, 0x30, off, 10)
    if 0xf0c <= off < 0xf18:                        # aim, varied
        return _ai_vec(seed, 0xf0c, off, 40)
    if 0x1024 <= off < 0x1030:                      # vel, varied
        return _ai_vec(seed, 0x1024, off, 70)
    if 0xf00 <= off < 0xf04:                        # pProfile
        return _u32b(_AI_PROF, off, 0xf00)
    if 0xf8c <= off < 0xf90:                        # pNode
        return _u32b(_AI_NODE, off, 0xf8c)
    if 0xf90 <= off < 0xf94:                        # iPt (start index) -> 0
        return 0
    if 0x29c0 <= off < 0x29c4:                      # pCtl
        return _u32b(_AI_CTL, off, 0x29c0)
    return None                                    # rest of the car stays zeroed


# Integer AI outputs (bias/scan state globals + the car's counter/flag/gate
# fields + the control record's flags) are exact-compared: their small values
# (0/1/2/3) alias tiny denormal floats, so the default value-tolerant compare
# would mask a wrong integer decision as float rounding.  Float outputs (aim,
# steer, the shaped force) stay tolerant.
_AI_EXACT = (
    (0x10AC680C, 0x10AC6810),   # g_brAiScanN
    (0x10B1C888, 0x10B1C88C),   # g_brAiScanBestPt
    (0x10B1CA18, 0x10B1CA20),   # g_brAiScanFlag18, g_brAiScanDepth
    (0x10B1CBE8, 0x10B1CBEC),   # g_brAiBiasPos
    (0x10B1CF04, 0x10B1CF10),   # g_brAiBiasNeg, g_brAiScanF08, g_brAiScanF0C
    (0x00300524, 0x00300528),   # car f524 gate
    (0x00300EA0, 0x00300EB0),   # car cHoldFwd/cHoldRev/cRevRun/cFwdRun
    (0x00300F78, 0x00300F7C),   # car fF78
    (0x10E20000, 0x10E20004),   # pCtl->flags
)

# ---- BrSndCarStep 0x10061470 (one frame of one car's engine+one-shot sound) --
# `this`=pCar (BR_THISCALL1).  Main path needs the race NOT paused
# (g_105CCB5C==0, default) and the car present (pCar+0xf00 != 0).  Engine retune
# reads RPM/hertz floats (+0xf68..+0xf80); the seven one-shots each fire on a
# per-car gate byte (+0x362..+0x36d) that is then CLEARED (a direct output);
# the listener object at pCar+0x2734 supplies Doppler position.  A too-small or
# zero car takes the early-out and hides the whole body.
_SC_LSNR = 0x10E90000


def _scf(seed, tag):
    s = (seed * 2654435761 + tag * 40503) & 0xFFFFFFFF
    return ((s % 2000) - 1000) / 50.0             # ~[-20, 20]


# The engine layer's outputs are FIXED-POINT integers stored via `(__int64)`
# casts into these globals (the 32.32 hertz ratio at eef48/eef60, the packed
# stereo pair at eef54/eef6c, the cockpit-siren flag at 1184c454).  A diff in
# them is a whole-integer difference, never x87 rounding, so they must be exact
# regions -- else a clamp/scale bug that zeroes the ratio hides as FP noise
# (the masking class of [[objdl-a5-limit]]; negative-controlled below).
_SC_EXACT = ((0x118EEF48, 0x118EEF70), (0x1184C454, 0x1184C458))


def _sc_bss(seed, a):
    import struct
    base = a & ~3
    if _SC_LSNR <= a < _SC_LSNR + 0x100:          # camera/listener transform for Doppler+Pan
        return _f32at(_scf(seed, 900 + (a - _SC_LSNR)), a)
    # cockpit path (DAT_100aa044==1): iVar8 = *(&DAT_10af393c + local_4), the
    # active camera.  Unseeded it is NULL, so Doppler/Pan read the listener
    # position from address 0x30 -> 0/0 -> NaN outputs that mask every diff
    # (a false EQUIVALENT).  Point it at the scratch transform above; keep
    # DAT_10af2180 and DAT_106e86c8 at 0 so local_4==0 and local_24==0 (the
    # engine-hertz ratio is written, not skipped).
    if base == 0x10AF393C:
        return _b(_SC_LSNR, a, base)
    # g_BrAnimDt (seconds/frame) -- the GLIDE build's copy is 0x106E9D8C (the
    # slice-header 0x106C2CFC is the D3D address).  BrSndDoppler divides both
    # dot products by it; unseeded -> 0 -> inf -> NaN return, which then
    # poisons fVar10 (car+0xf74 multiply), so `(__int64)NaN` writes 0 to every
    # engine-hertz slot -- a fully masked false EQUIVALENT.
    if base == 0x106E9D8C:
        return _f32at(1.0, a)
    # BrSndPlayEx's three "audio live" gates stay 0 so the mixer early-outs.
    # Seeding them 1 drives BrSndPlayEx's real mixer path, which fires stdcall
    # voice callbacks through NULL-voice+offset slots; the emulator black-boxes
    # an unresolved icall with esp UNCHANGED (cdecl), leaking each callee's
    # stdcall arg bytes -- 4 leaks summed to a 0x20 esp drift that only bit the
    # ORIGINAL (esp-relative locals), spuriously moving its Doppler-copyback
    # target out of the compared car buffer while the recompile (ebp-relative)
    # stayed correct.  Certifying BrSndCarStep does NOT certify the mixer; the
    # one-shot GATE decisions/clears (car+0x362..) are still exercised because
    # the gate bytes fire (seeded in _sc_buf) before the early-out.
    return 0                                       # g_105CCB5C etc. default 0 (not paused)


def _sc_buf(seed, argidx, off):
    import struct
    if argidx != 0:
        return None
    if off == 0xf00 or off == 0xf01 or off == 0xf02 or off == 0xf03:
        return _b(1, off, 0xf00)                   # car present (nonzero)
    if 0x140 <= off < 0x144:
        return 0                                   # car index 0
    if off == 0x2734 or (0x2734 < off < 0x2738):   # listener pointer
        return _b(_SC_LSNR, off, 0x2734)
    if 0x362 <= off <= 0x36d:                       # one-shot gate bytes -> fire
        return (1 + (seed + off) % 3) & 0xFF
    if 0xf78 <= off < 0xf7c:                          # remote-listener flag -> 0 (local path)
        return 0
    if 0xf68 <= off < 0xf84:                          # engine RPM/hertz floats
        return struct.pack('<f', _scf(seed, 300 + (off >> 2)))[off & 3]
    if 0x30 <= off < 0x48:                            # car velocity/position (Doppler input)
        return struct.pack('<f', _scf(seed, 600 + (off >> 2)))[off & 3]
    if 0xf80 <= off < 0xf8c:                           # Doppler position param
        return struct.pack('<f', _scf(seed, 650 + (off >> 2)))[off & 3]
    if off == 0xe24 or (0xe24 < off < 0xe28):          # RPM -> bounded positive (unclamped hertz)
        return struct.pack('<f', 20.0 + (seed % 40))[off & 3]
    if 0x160 <= off < 0x164:
        return struct.pack('<f', _scf(seed, 500 + (off >> 2)))[off & 3]
    return None


# ---- BrCarPhysDriveMatch 0x100645A0 (axle-velocity constraint solver) -------
# param_1 is declared `int` but is a CAR pointer: it holds four axle-record
# pointers at +4/+8/+0xc/+0x10 and its own float/flag fields; each axle record
# has contact/state ints (+0x19c, +0x1b4) and force floats (+0x1c0..+0x1d0).
# param_3/param_4 are float velocity vectors, param_5/param_6 char output flags.
# Random-int param_1 dereferences garbage (a false EQUIVALENT -- a flipped
# physics compare went uncaught), so pin param_1 to a scratch car and seed a
# valid graph with contact ON (+0x19c/+0x1b4 nonzero) so the main solver runs.
_CD_CAR = 0x10E80000
_CD_AX = (0x10E81000, 0x10E82000, 0x10E83000, 0x10E84000)


def _cdf(seed, tag):
    s = (seed * 2654435761 + tag * 40503) & 0xFFFFFFFF
    return ((s % 2400) - 1200) / 100.0            # ~[-12, 12], bounded for x87


def _cd_arg(seed, idx):
    import struct
    if idx == 0:
        return _CD_CAR                            # param_1 = car pointer
    if idx == 1:                                  # param_2 is a DIVISOR -> nonzero
        return struct.unpack('<I', struct.pack('<f', 3.0 + (seed % 17)))[0]
    return None


def _cd_bss(seed, a):
    base = a & ~3
    for k, off in enumerate((4, 8, 0xc, 0x10)):
        if base == _CD_CAR + off:
            return _b(_CD_AX[k], a, _CD_CAR + off)
    if base == _CD_CAR + 0x2c:                    # a denominator scale -> clearly nonzero
        return _f32at(5.0 + (seed % 11), a)
    if base in (_CD_CAR + 0x84, _CD_CAR + 0x88, _CD_CAR + 0x8c, _CD_CAR + 0xbc):
        return _f32at(_cdf(seed, base & 0xffff), a)
    for k, ax in enumerate(_CD_AX):
        if ax <= base < ax + 0x1e0:
            off = base - ax
            if off == 0x19c:
                return _b(1, a, ax + 0x19c)        # contact live (nonzero)
            if off == 0x1b4:
                return _b(1 + (seed + k) % 2, a, ax + 0x1b4)   # contact flag nonzero
            # Every other axle field is a FLOAT (geometry/velocity/force); seed
            # it varied and DISTINCT per axle+offset so differences never vanish
            # (a shared geometry field at +0x78 gave 0/0 -> inf -> inf*0 -> NaN,
            # which then compares NaN==NaN and hides every bug).
            return _f32at(_cdf(seed, 700 + k * 61 + off), a)
    return 0


def _cd_buf(seed, argidx, off):
    import struct
    if argidx in (2, 3) and off < 0x0c:            # param_3/param_4 float[3]
        return struct.pack('<f', _cdf(seed, argidx * 131 + (off >> 2)))[off & 3]
    return None


# The car's flag bytes (+0x1fd, +0x209) and the axle contact-state ints (+0x1b4)
# are INTEGER output -- compared exactly, never float-masked, so a wrong flag or
# a wrong contact write is a real DIFF (else a 0-vs-N byte diff hides as denormal
# rounding).  The velocity/roll floats stay tolerant.
_CD_EXACT = ((_CD_CAR + 0x1fc, _CD_CAR + 0x200), (_CD_CAR + 0x208, _CD_CAR + 0x20c),
             ) + tuple((ax + 0x1b4, ax + 0x1b8) for ax in _CD_AX)


# ---- BrCtlInputApply 0x1005AFF0 (apply player input to one car, __thiscall) -
# `this` is a car (>0x2b68).  A too-small buffer reads every deep field as 0, so
# the stick value, gear and steering state collapse and the deadzone / response
# curves never move.  Enlarge the buffer and seed: the raw stick through the
# pointer at car+0x29c0 (deref +0x20), varied across zero; the gear selector at
# car+0xe98 over the switch's 0..7; the body matrix (car+0x220) and aim vector
# (car+0x1e8); the steering-rate state (car+0xe20) and speed slot (car+0x1030).
# DAT_100b2e6c (speed-clamp enable) -> on; the input-latch pointer globals stay
# NULL (deref -> 0, top gate open, main path).
_CI_STICK = 0x10E70000            # scratch the car+0x29c0 pointer targets


def _cif(seed, tag, lo, hi):
    s = (seed * 2654435761 + tag * 40503) & 0xFFFFFFFF
    return lo + (s % 1000) / 1000.0 * (hi - lo)


def _ci_bss(seed, a):
    import struct
    base = a & ~3
    if base == _CI_STICK + 0x20:
        return struct.pack('<f', ((seed % 13) - 6) * 0.4)[a - base]
    if base == 0x100B2E6C:
        return _b(1, a, 0x100B2E6C)
    return 0


def _ci_buf(seed, argidx, off):
    import struct
    if argidx != 0:
        return None
    if 0x29c0 <= off < 0x29c4:
        return _b(_CI_STICK, off, 0x29c0)
    if 0xe98 <= off < 0xe9c:
        return _b(seed % 8, off, 0xe98)
    if off == 0xe81:
        return ((seed % 3) - 1) & 0xFF
    if 0x220 <= off < 0x260:
        k = (off - 0x220) >> 2
        return struct.pack('<f', (1.0 if k in (0, 5, 10, 15) else 0.0)
                           + _cif(seed, 200 + k, -0.3, 0.3))[off & 3]
    if 0x1e8 <= off < 0x1f4:
        return struct.pack('<f', _cif(seed, 300 + ((off - 0x1e8) >> 2), -4.0, 4.0))[off & 3]
    if 0xe20 <= off < 0xe24:
        return struct.pack('<f', ((seed % 7) - 3) * 0.5)[off & 3]
    if 0x1030 <= off < 0x1034:
        return struct.pack('<f', (seed % 50) * 2.0)[off & 3]
    return None


# ---- BrSnapInterpDraw 0x100131E0 (render-side snapshot interpolator) -------
# Blends the two newest of a six-slot snapshot ring into the sixth slot and
# hands that slot to the frame driver.  Inputs are an object graph in
# g_aBrSnap[6] plus the slot-index globals.  Seed: valid, distinct slot indices
# (Cur/Prev/To/From in 0..4), Interpolate on, small car/driver counts, and
# VARIED float car matrices in every slot so the per-field blend has teeth (a
# zero world would blend 0->0 and hide a wrong-field/wrong-slot LERP).  The car
# matrix-record pointers are left NULL so the pointer-retarget arms are
# deterministic.  The wall-clock timer (0x1006E280) and the frame driver
# (0x10011FA0, a 4500-byte renderer this function only hands the blend slot to)
# are STUBBED: both sides call them identically, and this function's own
# contract -- the blended slot and the return value -- is compared directly.
_SNAP_G      = 0x10396F48        # g_aBrSnap base
_SNAP_SLOT   = 0x2E0F0           # per-slot stride
_SNAP_CAROFF = 0xA08             # car[] offset within a slot
_SNAP_CARSZ  = 0x2B68            # per-car stride


def _snap_f(slot, ci, off):
    s = slot * 61 + ci * 29 + (off >> 2) * 7
    sign = -1.0 if (s & 1) else 1.0
    return sign * float((s % 23) + 1) * (10.0 ** ((s // 23) % 3))   # 1 .. ~2.2e3


def _snap_bss(seed, a):
    base = a & ~3
    idx = {0x104AB4EC: 0, 0x104AB500: 1, 0x10396F44: 2, 0x104AB4FC: 3,
           0x100A5EAC: 1, 0x100B2F00: 2, 0x100B2F04: 2, 0x104AB4F4: 0}
    if base in idx:
        return _b(idx[base], a, base)
    if base == 0x10396F24:                    # g_brSnapOrigin (float t at last reset)
        return _f32at(0.0, a)
    if _SNAP_G <= a < _SNAP_G + 6 * _SNAP_SLOT:
        off = a - _SNAP_G
        slot = off // _SNAP_SLOT
        os_ = off % _SNAP_SLOT
        if os_ >= _SNAP_CAROFF:
            oc = os_ - _SNAP_CAROFF
            if oc < 16 * _SNAP_CARSZ:
                ci = oc // _SNAP_CARSZ
                ocar = oc % _SNAP_CARSZ
                if 0x2734 <= ocar < 0x273c:   # pMatA / pMatB -> NULL
                    return 0
                if ocar < 0x2734 or ocar >= 0x273c:   # a float field
                    return _f32at(_snap_f(slot, ci, ocar), a)
        return 0
    return 0


def _snap_arg(seed, idx):
    if idx == 0:                              # force
        return (seed >> 1) & 1
    return None


# INTEGER state outputs -- compared exactly, never float-masked (a lock flag or
# a counter differing by 1 reads as a denormal-float rounding diff otherwise and
# a wrong-write bug hides).  g_brSnapOrigin (0x10396F24) is the float t and is
# deliberately NOT here; the blended car matrices are float and stay tolerant.
_SNAP_EXACT = (
    (0x10396F10, 0x10396F24),   # g_aBrSnapLocked[5]
    (0x10396F44, 0x10396F48),   # g_brSnapTo
    (0x104AB4F0, 0x104AB4F8),   # g_brSnapFrames, g_brSnapT0
    (0x104AB4FC, 0x104AB500),   # g_brSnapFrom
)


# ---- BrKeyTableFind 0x10030FD0 (backward key-table scan) --------------------
# The count at 0x10AC0808 gates the scan loop: a random dword there walks the
# table for up to 2^31 iterations (runaway).  Pin it to 0..3; the bias at
# 0x10AC080C varies per seed; unnamed BSS (the table entries' keys) reads 0, so
# pinning even seeds' key argument to -bias makes the biased key hit entry key
# 0 (both stores + return 1), while odd seeds take the miss path.
def _ktf_bss(seed, a):
    if 0x10AC0808 <= a < 0x10AC080C:
        return _b(seed % 4, a, 0x10AC0808)            # count 0..3
    if 0x10AC080C <= a < 0x10AC0810:
        return _b((seed * 0x101) & 0xFFFF, a, 0x10AC080C)
    return 0


def _ktf_arg(seed, idx):
    if idx == 0 and (seed & 1) == 0:
        return (-((seed * 0x101) & 0xFFFF)) & 0xFFFFFFFF   # hit key 0
    return None


# ---- FUN_1006ec30 0x1006EC30 (vertical raycast into collision grid) ---------
# Orchestrator: casts a vertical ray through the collision-plane grid.  The
# loop count comes from DAT_11778800[cell]; with random BSS that overflows the
# step limit.  BrCollGridCellAcquire is stubbed so cell=0; plane count, plane
# fields, and the three pointer globals (tri-index, chain-data, chain-index)
# are seeded to a small, valid world.  Vec math callees are leaf functions the
# oracle can execute.
_CR_SCRATCH_TRI  = 0x10E20000   # g_pBrCollTriIdx target (face records)
_CR_SCRATCH_CD   = 0x10E21000   # DAT_106eed64 target (chain data, NUL-terminated)
_CR_SCRATCH_CI   = 0x10E22000   # DAT_106eed68 target (chain index)
_CR_SCRATCH_VERT = 0x10E23000   # scratch BrVec3 for pV0/pV1/pV2

_CR_PLANE_BASE   = 0x11773698   # DAT_11773698[0][0]
_CR_PLANE_SIZE   = 32           # sizeof(BrCollPlane)
_CR_MAX_PLANES   = 3


def _collray_bss(seed, a):
    # DAT_11778800[0] at 0x11778800: plane count, 1..3 varied by seed
    if 0x11778800 <= a < 0x11778802:
        count = (seed % _CR_MAX_PLANES) + 1
        return _b(count, a, 0x11778800)
    # DAT_11778800[1..3]: zero (unused since cell=0 from stubbed acquire)
    if 0x11778802 <= a < 0x11778808:
        return 0

    # Collision planes at 0x11773698: seed up to _CR_MAX_PLANES entries
    if _CR_PLANE_BASE <= a < _CR_PLANE_BASE + _CR_MAX_PLANES * _CR_PLANE_SIZE:
        off = a - _CR_PLANE_BASE
        pidx = off // _CR_PLANE_SIZE        # which plane
        foff = off % _CR_PLANE_SIZE         # offset within plane
        base = _CR_PLANE_BASE + pidx * _CR_PLANE_SIZE
        if 0x00 <= foff < 0x04:             # nx = 0.0
            return 0
        if 0x04 <= foff < 0x08:             # ny = 0.0
            return 0
        if 0x08 <= foff < 0x0C:             # nz = 1.0 (positive, passes the guard)
            return _b(0x3F800000, a, base + 0x08)
        if 0x0C <= foff < 0x10:             # d = small float varied by seed+pidx
            return _f32b(((seed * 101 + pidx * 37) % 200 - 100) / 10.0,
                         a, base + 0x0C)
        if 0x10 <= foff < 0x14:             # pV0 -> scratch vert
            return _b(_CR_SCRATCH_VERT + pidx * 36, a, base + 0x10)
        if 0x14 <= foff < 0x18:             # pV1 -> scratch vert + 12
            return _b(_CR_SCRATCH_VERT + pidx * 36 + 12, a, base + 0x14)
        if 0x18 <= foff < 0x1C:             # pV2 -> scratch vert + 24
            return _b(_CR_SCRATCH_VERT + pidx * 36 + 24, a, base + 0x18)
        if 0x1C <= foff < 0x1E:             # tri = pidx (small index)
            return _b(pidx, a, base + 0x1C)
        return 0                            # flags, pad

    # g_pBrCollTriIdx pointer at 0x106EECE4
    if 0x106EECE4 <= a < 0x106EECE8:
        return _b(_CR_SCRATCH_TRI, a, 0x106EECE4)
    # DAT_106eed64 (chain data pointer) at 0x106EED64
    if 0x106EED64 <= a < 0x106EED68:
        return _b(_CR_SCRATCH_CD, a, 0x106EED64)
    # DAT_106eed68 (chain index pointer) at 0x106EED68
    if 0x106EED68 <= a < 0x106EED6C:
        return _b(_CR_SCRATCH_CI, a, 0x106EED68)

    # Tri-index table at _CR_SCRATCH_TRI: 4 uint16 per face [v0, v1, v2, faceID]
    # Each face's [3] (the faceID) is a small non-zero value.
    if _CR_SCRATCH_TRI <= a < _CR_SCRATCH_TRI + 64:
        off = a - _CR_SCRATCH_TRI
        face = off // 8      # 4 uint16 = 8 bytes per face
        slot = (off % 8) // 2
        if slot == 3:         # faceID
            return _b(face + 1, a, _CR_SCRATCH_TRI + face * 8 + 6)
        return 0

    # Chain data at _CR_SCRATCH_CD: three NUL-terminated lists
    # list 0: [1, 0]  list 1: [2, 0]  list 2: [3, 0]
    if _CR_SCRATCH_CD <= a < _CR_SCRATCH_CD + 32:
        off = a - _CR_SCRATCH_CD
        idx = off // 2
        if idx < 6:  # 3 lists of 2 entries each
            listno = idx // 2
            slot = idx % 2
            if slot == 0:
                return _b(listno + 1, a, _CR_SCRATCH_CD + idx * 2)
            else:
                return 0  # NUL terminator
        return 0

    # Chain index at _CR_SCRATCH_CI: maps tri -> offset in chain data
    # tri 0 -> 0, tri 1 -> 2, tri 2 -> 4
    if _CR_SCRATCH_CI <= a < _CR_SCRATCH_CI + 16:
        off = a - _CR_SCRATCH_CI
        idx = off // 2
        if idx < _CR_MAX_PLANES:
            return _b(idx * 2, a, _CR_SCRATCH_CI + idx * 2)
        return 0

    # Scratch vertices at _CR_SCRATCH_VERT: varied small floats
    if _CR_SCRATCH_VERT <= a < _CR_SCRATCH_VERT + _CR_MAX_PLANES * 36:
        base = a & ~3
        off = (base - _CR_SCRATCH_VERT) // 4
        return _f32b(((seed * 7 + off * 13) % 200 - 100) / 10.0, a, base)

    # Time globals: small non-zero to give a valid disc
    if 0x106EED14 <= a < 0x106EED18:   # DAT_106eed14 time now
        return _f32b((seed % 100 + 1) / 60.0, a, 0x106EED14)
    if 0x106EED10 <= a < 0x106EED14:   # DAT_106eed10 time prev
        return 0

    return 0


# ---- BrEnvEmit 0x10017110 (per-frame track-surface display-list emitter) ----
# A void, globals-driven display-list builder.  The default random world runs
# away in the per-segment projection loop (its trip count DAT_104add38 is a raw
# .data dword) and in the early surface-flag scan (count DAT_106e8a18).  The
# profile bounds both counts, points the DL write cursor (g_6E7710) and the
# surface table (g_6EED38) at scratch, seeds the view matrix g_6E78F0 to identity
# and the clip thresholds so segments actually project into range (giving the
# tile-pack + DL-write teeth), and black-boxes the seven sub-calls both sides
# invoke identically (matrix build, the visibility rasteriser, the F3D helpers).
# The integer DL command words the function itself writes are the contract and
# are verified exactly.
_ENV_CUR = 0x10E00000        # DL write cursor (g_6E7710)
_ENV_OBJTAB = 0x10E10000     # surface table base (g_6EED38), 0x54 stride


def _env_bss(seed, a):
    base = a & ~3
    # pointer-valued globals -> scratch arenas
    if base == 0x106E7710:
        return _b(_ENV_CUR, a, 0x106E7710)
    if base == 0x106EED38:
        return _b(_ENV_OBJTAB, a, 0x106EED38)
    # bounded loop counts
    if base == 0x106E8A18:                       # early surface-flag scan count
        return _b(2, a, 0x106E8A18)
    if base == 0x104ADD38:                       # per-segment projection count
        return _b(3, a, 0x104ADD38)
    # control flags: both non-zero so the function does its full work; the
    # weather flag varies the dc/dd combiner arm and the colour constant.
    if base == 0x106ED6B0:
        return _b(1, a, 0x106ED6B0)
    if base == 0x106ED6B4:
        return _b(seed & 1, a, 0x106ED6B4)
    if base == 0x106EC798:                       # section index -> 0 (min offsets)
        return 0
    if base == 0x106EA3F4 or base == 0x106E8204: # viewW sign xor -> no flip
        return 0
    if base == 0x100A7514 or base == 0x100A7518: # view half-dims
        return _b(64, a, base)
    # view/projection matrix g_6E78F0 (4x4, row-major): small non-zero, varied
    # coefficients so every segment gets a non-zero projective w (avoiding the
    # 1/w = inf that pushes the projected coords out of every screen bound and
    # skips the tile emit) and lands in range, giving the transform + tile-pack
    # path real teeth.  The translation row (m[3][*]) that the body zeroes is
    # left to the body; m[3][3] is a solid positive so w never collapses to 0.
    if 0x106E78F0 <= base < 0x106E7930:
        idx = (base - 0x106E78F0) >> 2
        if idx == 15:                             # m[3][3]
            return _f32at(1.0, a)
        return _f32at(0.05 + 0.01 * idx, a)
    # projection thresholds / scales
    if base == 0x10077364:                       # near-plane w -> very negative (accept any w)
        return _f32at(-1.0e30, a)
    if base == 0x10077314:                       # perspective numerator
        return _f32at(1.0, a)
    if base == 0x10077368:                       # screen min -> very negative
        return _f32at(-1.0e30, a)
    if base == 0x10077304:                       # screen max -> very positive
        return _f32at(1.0e30, a)
    if base == 0x10077348:                       # first-loop bias
        return 0
    if base in (0x10077344, 0x1007734C, 0x10077350, 0x10077354):
        return _f32at(0.1, a)
    # per-segment source vertices (psVar16 walks &DAT_104add54, reading the
    # shorts at [-2]/[-1]/[0] each step): give each of the three segments
    # distinct, seed-varied coordinates so a wrong vertex index or a transform
    # coefficient swap changes at least one segment's projected tile command,
    # instead of every segment collapsing to the same point.
    if 0x104ADD50 <= a < 0x104ADD68:
        h = a & ~1
        val = (((h - 0x104ADD50) >> 1) * 13 + 7 + (seed & 7)) & 0x7fff
        return (val >> (8 * (a - h))) & 0xFF
    return 0                                      # everything else null-safe / 0


# The DL command words this function writes are INTEGER output; a differing dword
# there is a real divergence, not x87 rounding.  The view matrix and float
# scratch are left tolerant.
_ENV_EXACT = ((_ENV_CUR, _ENV_CUR + 0x400),)
_ENV_STUBS = (0x10008D60, 0x1001CF90, 0x10034B70, 0x10034AF0,
              0x100349C0, 0x100344D0, 0x100597F0)


# ---- BrCarStep 0x1006F170 (per-car frame step) -----------------------------
# thiscall(pCar): casts the wheel collision ray + applies input (live path,
# pCar->f7c == 0) or replays the recorded transform (f7c != 0), then writes the
# four wheels' suspension/slip fields, runs the net/steer sub-steps, recomputes
# the scalar speed and advances the chase camera.  The four wheel-record
# pointers (pCar+0x168/16c/170/174), the two flag/aux record pointers (+0x29c0,
# +0x29c4) and the entity arg buffer are seeded to distinct scratch sub-objects;
# the body slip (+0xe68) and spin (+0xe6c) and the wheel-mode byte (+0xd9) are
# varied so the four-wheel distribution and the sign/threshold arms all fire.
# Every sub-call both sides invoke identically (collision ray, input apply, the
# a7a0 sub-step, wheel-steer, net-send, the four BrEntSet* setters, the three
# BrVec3 helpers, the bit latch, the chase-cam) is black-boxed; this function's
# own contract -- the wheel fields it writes, pCar->1020/1030/e24, and the call
# sequence -- is compared directly.  The wheel fields are integer/float output;
# a wrong distribution surfaces as a differing dword.
_CS_WHEEL = 0x10E00000     # four wheel records, 0x400 apart
_CS_FLAGS = 0x10E10000     # pCar->29c0 -> flags record
_CS_AUX   = 0x10E11000     # pCar->29c4 -> record holding the +0xd9 mode byte


def _cs_bss(seed, a):
    import struct
    base = a & ~3
    if base == 0x105ccb88:                       # net-replay gate off -> steer runs
        return 0
    if base == 0x100a9360:                       # race state 0..5
        return _b(seed % 6, a, 0x100a9360)
    if base == 0x10226a44 or base == 0x10226a48: # net gates off
        return 0
    if base == 0x100aa044:                       # chase-cam table length
        return _b(2, a, 0x100aa044)
    # the chase-cam id table g_6E86C8[n], stride 0x58 (0x16 ints): entry 0's id
    # matches car->140 (seeded 0) so the chase arm is reached on some seeds.
    if base == 0x106e86c8:
        return _b(seed & 1, a, 0x106e86c8)
    # scale / threshold constants: small, non-degenerate
    if base == 0x106e9d8c:
        return _f32at(1.0, a)
    if base == 0x10077c38:                       # slip threshold -> 0
        return 0
    if base in (0x10077c60, 0x10077c64):
        return _f32at(0.5, a)
    if base in (0x10077c68, 0x10077c6c, 0x10077c70):
        return _f32at(0.75, a)
    if base == 0x10077c74:                       # spin clamp -> large so both arms reachable
        return _f32at(100.0, a)
    if base == 0x10077c78:                       # speed scale
        return _f32at(0.1, a)
    # flag record (pCar->29c0): bit 0x10 = replay cross arm, bit 0x20000 = slip
    # sign flip; the +0x44 gate.  Vary by seed so those arms get teeth.
    if base == _CS_FLAGS:
        return _b((0x10 if ((seed >> 1) & 1) else 0) | (0x20000 if ((seed >> 2) & 1) else 0),
                  a, _CS_FLAGS)
    if base == _CS_FLAGS + 0x44:
        return _b(seed & 1, a, _CS_FLAGS + 0x44)
    # aux record (pCar->29c4): the wheel-mode byte at +0xd9 -> 0/1/2 selector
    if a == _CS_AUX + 0xd9:
        return ((seed >> 1) % 3) & 0xFF
    return 0


def _cs_arg(seed, idx):
    if idx == 0:
        return None                              # pCar buffer (buf hook fills it)
    return None


def _cs_buf(seed, argidx, off):
    import struct
    if argidx != 0:
        return None
    # path selector: mostly the live path, replay on one seed in three (so
    # replay seeds overlap the odd-seed flag bit 0x10 that arms the cross step)
    if 0xf7c <= off < 0xf80:
        return _b(1 if (seed & 1) else 0, off, 0xf7c)
    # four wheel-record pointers -> distinct scratch sub-objects
    if 0x168 <= off < 0x178:
        k = (off - 0x168) >> 2
        return _b(_CS_WHEEL + k * 0x400, off, 0x168 + k * 4)
    if 0x29c0 <= off < 0x29c4:
        return _b(_CS_FLAGS, off, 0x29c0)
    if 0x29c4 <= off < 0x29c8:
        return _b(_CS_AUX, off, 0x29c4)
    # body slip / spin: straddle the (zero) threshold so every arm runs
    if 0xe68 <= off < 0xe6c:
        return struct.pack('<f', ((seed % 5) - 2) * 3.0)[off & 3]
    if 0xe6c <= off < 0xe70:
        return struct.pack('<f', ((seed % 7) - 3) * 2.0)[off & 3]
    if 0xe20 <= off < 0xe24:
        return struct.pack('<f', (seed % 9) * 0.5)[off & 3]
    # replay transform source
    if 0x2720 <= off < 0x2730:
        return struct.pack('<f', _cif(seed, 400 + ((off - 0x2720) >> 2), -2.0, 2.0))[off & 3]
    # position + orientation scratch vectors
    if 0x10 <= off < 0x40:
        return struct.pack('<f', _cif(seed, 500 + ((off - 0x10) >> 2), -1.0, 1.0))[off & 3]
    # velocity for the speed magnitude
    if 0x1e8 <= off < 0x1f4:
        return struct.pack('<f', _cif(seed, 600 + ((off - 0x1e8) >> 2), -4.0, 4.0))[off & 3]
    # entry id (chase-cam / net gates); small so it can match the table
    if 0x140 <= off < 0x144:
        return _b(seed & 1, off, 0x140)
    # active flag: non-zero so the speed magnitude + chase arm run
    if 0x730 <= off < 0x734:
        return _b(0x5b + (seed % 4), off, 0x730)
    # a control-mode function pointer compared against BrCtlHuman (0x1005d050)
    if 0xf08 <= off < 0xf0c:
        return _b(0x1005d050 if (seed & 1) else 0, off, 0xf08)
    return None


# the flag record (pCar->29c0): bit 0x10 (replay cross arm), bit 0x20000 (slip
# sign flip) and the +0x44 gate, varied by seed.
def _cs_ptrfill(seed):
    return None


_CS_EXACT = ((_CS_WHEEL, _CS_WHEEL + 0x1000),)
_CS_STUBS = (0x1006EC30, 0x1005AFF0, 0x1005A7A0, 0x1005ACE0, 0x10059A50,
             0x1006F680, 0x1006FA10, 0x1006FC10, 0x1006FA90,
             0x10034360, 0x100345F0, 0x100342B0, 0x1002F640, 0x10001CF0)


PROFILES = {
    0x1006F170: Profile(_cs_bss, zero_stack=True, seeds=64, arg=_cs_arg,
                        buf=_cs_buf, buf_sizes={0: 0x2b68},
                        exact_regions=_CS_EXACT, stub_calls=_CS_STUBS),
    0x10017110: Profile(_env_bss, zero_stack=True, seeds=48,
                        exact_regions=_ENV_EXACT, stub_calls=_ENV_STUBS),
    0x10061470: Profile(_sc_bss, zero_stack=False, seeds=48, buf=_sc_buf,
                        buf_sizes={0: 0x2b68}, exact_regions=_SC_EXACT),
    0x100645A0: Profile(_cd_bss, zero_stack=False, seeds=48, arg=_cd_arg, buf=_cd_buf,
                        exact_regions=_CD_EXACT),
    0x1005AFF0: Profile(_ci_bss, zero_stack=False, seeds=48, buf=_ci_buf,
                        buf_sizes={0: 0x2b68}),
    0x10030FD0: Profile(_ktf_bss, arg=_ktf_arg),
    0x100131E0: Profile(_snap_bss, zero_stack=False, seeds=24, arg=_snap_arg,
                        stub_calls=(0x1006E280, 0x10011FA0), exact_regions=_SNAP_EXACT),
    0x1005D770: Profile(_ctlai_bss, seeds=160, arg=None, buf=_ctlai_buf,
                        buf_sizes={0: 0x2b68}, exact_regions=_AI_EXACT),
    0x1000CBA0: Profile(_odl_bss, zero_stack=False, seeds=16, arg=_odl_arg,
                        exact_regions=_ODL_EXACT),
    0x1000EAF0: Profile(_scenedl_bss, zero_stack=False, seeds=32),
    0x100250D0: Profile(_tex_bss, zero_stack=False, seeds=64, arg=_tex_arg),
    0x10019A70: Profile(_bracestep_bss),
    0x1006EC30: Profile(_collray_bss, zero_stack=True, seeds=48,
                        stub_calls=(0x100686D0,)),
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
