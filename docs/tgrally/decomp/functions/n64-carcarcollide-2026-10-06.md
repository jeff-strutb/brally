# N64 carcarcollide

*Recorded 2026-10-06.*

> BrCarCarCollide 0x8025FDE4 (collresp.c) T4 2026-10-06, 6a4d7177 (session dfabc7), gate 706/0; levers: cfe operand reordering only at root/left spine (pending value disables it), wrapping if vs continue, block-local scalar homes

**DONE 2026-10-06, commit 6a4d7177, image gate 706/0.** 432 -> 0 by hand in one session.

Levers (each confirmed with toy probes; cfe ucode read from `cc -K` -> x.B, uopt output x.O, parsed with build/tgrally/ext/n64-decomp-workbench decomp_workbench.ucode.parse_ucode):
- **ROM `beql car,0,<latch>` with the latch's reload in the delay slot = wrapping `if (a && b) { ... }`**, not `continue` (continue duplicates the latch with an extra `b`). 432 -> 323.
- **cfe reorders commutative operands only for an expression evaluated with nothing pending** (statement root and its left spine): `d.z * pa->vel.z` alone or as the left operand of `+` comes out `vel.z * d.z` whatever the source order; as the RIGHT operand of a `+` it keeps source order. **With a value pending (`s += E`) nothing in E is reordered.** `s = 0.0f; s += E;` gives E in pure source order and uopt folds the zero away (no trace in the code).
- `imp[0] + D[i].car->stB.vel.x` and `D[i].car->stB.vel.x + imp[0]` BOTH come out swapped (pure reversal for that pair); `imp[0] + pa->vel.x` keeps order. Write the opposite of the ROM order for the global-indexed car field.
- `float d[3]` vs `BrVec3 d` changes the store/reload order of `d[k] = a - b` sequences: field form matched the ROM.
- Frame homes: every declared scalar takes a word in declaration order (first declared = highest); a block-scoped scalar goes below the block's arrays (into the low gap), so moving one scalar into the inner block shifted every array up 4 bytes to the ROM's.
- `a = b = v` stores b first.

Related: [n64-camchasestep-wip](n64-camchasestep-wip.md), [n64-spill-slot-order-lever](../levers/n64-spill-slot-order-lever.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).
