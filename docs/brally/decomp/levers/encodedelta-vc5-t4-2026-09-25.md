# Encodedelta vc5 t4

*Recorded 2026-09-25.*

> 2026-09-25: 0x10006BA0 BrCarStateEncodeDelta BYTE-EXACT under plain VC5 /O2 (C++ lane) - refutes the 'bitstream family is a VC4.2 object' verdict. Levers: unsigned writer param, ref-before-cur decl, preamble-size heap tie-break.

0x10006BA0 went T2 -> T4 (commit e05c06ce) by a fresh hand transcription under
**VC5 /O2 /GX /MD** - the [vc42-is-the-real-compiler](../toolchain/vc42-is-the-real-compiler.md) claim that the net
bitstream TU (tu_006) needs VC4.2 is WRONG; three earlier sessions stopped at a
"compiler wall" that was a missing type.

**Levers (all in the file header and docs/brally/VC5-IDIOMS.md tail):**
1. Writer `WriteBits(unsigned int value, unsigned int nBits)` (ReadBits returns
   unsigned; the writer's own transcription already had unsigned). Converting
   `short>>8` to UNSIGNED narrows to `sar ax,8; movsx` as a pure expression,
   so `push 8` goes first. int param widens; short param drops movsx; casts
   fold; short stores push late.
2. `uint32_t ref, cur;` with ref declared first (xor operand copy order).
3. `|` operand roles are a VC5 heap-layout tie-break: flipped by how much the
   TU declares before the function (dummy-prototype windows 200-248/328-376;
   stdio+string+math headers land in it). Source order, /Gi, TU bodies,
   variable roles, macro vs inline: all inert or worse.

**How to apply:** sibling 0x10006510 BrCarStateEncode (T3, 151 diffs) has the
exact same "push-early vs narrow" residue - lever 1 should crack it, but it is
T3, so only if the project lead names it. Before calling any commutative-order residue
a wall, sweep preamble size (N dummy prototypes) first.
Related: [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md), [carstate-decode-cpp-2026-09-12](../cpp-lane/carstate-decode-cpp-2026-09-12.md).
