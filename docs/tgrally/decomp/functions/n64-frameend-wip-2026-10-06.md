# N64 frameend wip

*Recorded 2026-10-06.*

> BrFrameEnd 0x8021AA08 (frameloop.c) T4 2026-10-06, a9c051a1 (session 5a96e9), gate 707/0: PC twin's source shape (shared stats var + recomputed count) gave an invisible v0 web; uopt block split; levers

**DONE 2026-10-06, commit a9c051a1, image gate 707/0.** 209 -> 0 by hand. Drafts in build/tgrally/n64/search/8021AA08/s5a/ (f1.c final; run.sh grades a body + dumps early-block p1 decisions).

**Main lesson: read the PC twin (src/brally) FIRST.** The last residue (&D_8028AB7C v0 vs ROM a0) cost hours of mechanism hunting. The PC BrFrameEnd 0x1002CEE9 (br_framebegin.c) showed the original shape: one `len` variable for all three max stats, and the frame count *recomputed* for D_8028AB70. On IDO that makes uopt CSE the count into a v1 temp from the first count to the AB70 store. The stats variable's first chain (a copy of that temp) is coloured v0 and emits nothing, so the address web takes a0. Find PC twins by string: `grep -rn "<ROM string>" src/brally`.

Facts measured on the way (IDO 5.3):
- uopt closes a block at a statement start once it has ~20 local (Mmt) lods (only local lods count). An empty `do { } while (0);` gives the same split. The gRaw `_g` locals are coloured but invisible (copies of the post-increment cfe temp).
- Copies of a *variable* are always propagated before web founding (`m = n`, `m = n = E`, guarded empty reads did not stop it here). A copy of a uopt/cfe *temp* survives as a coloured web with no code.
- ugen computes an expression's operand into the register of the FIRST variable stored from it (`n = m = E` puts the intermediate in m).
- An address web for a global exists only when the stored value is the directly computed variable (a propagated copy as the store operand makes the access direct).
- localcolor marks v0 only for used call results (CDX_UF trace on a TU holding just the function).
- Other levers in this function: data_ptr `D[i] + 0x40` (array first), task store order from the ring, statements sharing one line for as1 ties (osGetCount pair, mul/count pair).

Related: [n64-camchasestep-wip](n64-camchasestep-wip.md), [n64-spill-slot-order-lever](../levers/n64-spill-slot-order-lever.md).
