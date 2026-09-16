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
    __slots__ = ('bss', 'zero_stack', 'seeds')

    def __init__(self, bss, zero_stack=True, seeds=64):
        self.bss = bss                 # fn(seed, addr) -> byte
        self.zero_stack = zero_stack    # stack window / arg buffers seeded to 0
        self.seeds = seeds


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


PROFILES = {
    0x10019A70: Profile(_bracestep_bss),
}


def get(va):
    """Profile for a function VA, or None (default random seeding)."""
    return PROFILES.get(va)
