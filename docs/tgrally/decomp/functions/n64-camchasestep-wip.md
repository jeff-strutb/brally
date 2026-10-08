# N64 camchasestep wip

*Recorded 2026-10-07.*

> BrCamChaseStep 0x80221170 (carcam.c) T4 2026-10-06, 9760f803 (session 5c4f1d), gate 705/0; levers: cfe indexed-operand order, uopt 24-local-lod block limit after a call, CSE temps claim spill slots first

**DONE 2026-10-06, commit 9760f803, image gate 705/0.** 131 -> 0 by hand (d7b110 had the earlier draft).

Levers proven here (IDO 5.3), each checked with toy probes, not searches:
- **cfe orders commutative operands by complexity**: an indexed load (`car->mtx0[2][0]`, two ixa) goes ahead of a field (`pv->xb8`) whatever the source order. ROM `xb8 * m20` with xb8 first = the matrix row is read as a field: `((BrVec3 *)car->mtx0[2])->x`. With all rows as BrVec3 casts, `B + (pos + x * off)` comes out exactly as the ROM (root, product and inner sum).
- **uopt block limit**: a block that opens after a call (cup) closes once it would exceed **24 loads of locals/params (Mmt/Pmt lods)**; globals (Smt) and the call result do not count. Measured with a private instrumented uopt (scratch instr8: f_appendgraph prints the ucode record index; uopt reads the file twice, so subtract one pass). One extra `pv = car->model` statement pushed the z row out of the tail block; reading `car->model` inline kept it in. All FP values in one block = block-granular interference = the ROM's spilled matrix temps.
- **Spill slots go first-fit in web order, and only expression temps (type 4) take them.** A local assigned from an expression of other *variables* is copy-propagated into a temp; one assigned from a memory load stays a variable. `&cams[1]` reached the ROM's 4th slot only once the lift's two products were written inline (CSE temps), with the 0.5/0.15 factor in its own local and `/ 31.415928f` inline (a `k = t / PI` local broke the CSE of `camSpin * k`).
- ROM frame words with no visible use = unused locals; slots one word high/low across the board = one local word below `dist`.
- The f8/f10 swaps in the locked-frame arm were operand order, not ring names (the old note was wrong).

Related: [n64-spill-slot-order-lever](../levers/n64-spill-slot-order-lever.md), [n64-trackdrawsetup-2026-10-06](n64-trackdrawsetup-2026-10-06.md).
