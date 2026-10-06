# File position regalloc lever

*Recorded 2026-09-06.*

> When a leaf is byte-exact standalone but register-blind-equal in its module, sweep FILE POSITION within the TU before calling it a wall.

 2026-09-06, proved on 0x10023CB0 BrTexRgbaToArgb1555 (RGBA8888->ARGB1555
converter, br_tex3d.c): a 112 B leaf was byte-exact as a standalone TU and
byte-exact at the END of br_tex3d.c, but 43 register-blind-EQUAL diffs
anywhere earlier in that file. The whole divergence was one allocator choice
(the `pix` accumulator in ebx vs edx, which swaps the OR destinations and
forces movzx vs xor/mov for the last byte).

**Why:** VC5's register allocation for a function is sensitive to WHOLE-TU
state, not just the function's own text. Every LOCAL lever was inert here --
declaration order (six orders swept), variable width (uint pix was worse),
and OR operand order (VC5 canonicalises commutative OR). Only moving the
whole function later in the file moved it; only the very end reached 0. It is
chaotic, not monotone (headers-only in a probe gave a THIRD result), so it is
allocator state, not a preceding-symbol count.

**How to apply:** if a small function is byte-exact standalone (probe.py with
no includes) but off by a register-blind-equal margin in its module, do a
one-file sweep with the function at the TOP and at the END of the TU before
concluding it is a coloring wall. Placement within the module .c has no
correctness or filing consequence, so end-of-TU is a free win. This is
distinct from [declaration-order-tiebreak](declaration-order-tiebreak.md) (a LOCAL lever); reach for file
position only after the local levers are confirmed inert. Full write-up:
docs/brally/VC5-IDIOMS.md, "File POSITION as the last register-allocation lever".
Companion idiom same function: `*src++` (not indexed) to consume a byte quad.
