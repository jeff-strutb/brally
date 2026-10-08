# N64 trackdrawsetup

*Recorded 2026-10-06.*

> BrTrackDrawSetup 0x80234FF8 (trackdraw.c, 2996 B, largest open T3) T4 2026-10-06, 345e660a (session 12cf07), gate 687/0: function-local static count, SR-pointer web shared across loops, queue-store idiom, continue vs wrapping if

**DONE 2026-10-06, commit 345e660a, image gate 687/0.** Hand transcribed; 689 -> 0 diff lines.

Levers that generalise (IDO 5.3):
- A uopt temp (SR pointer) in an s-reg with no call in its own range = it is ONE web with a later use of the same expression across calls. Here `D_8035D1D0[n]` in the fill loop AND the cell walk (walk indexed by n, not a pointer p); the dead `move t0, zero` before the qsort jal was the walk's `n = 0` hoisted.
- Register colours follow variables (Chow): same reg in two places suggests the same variable (c/r reused as the camera cell; k as a later loop counter took a0 so a temp moved to a3).
- `slot = count - 1; for (k = slot; ...)`: a copy (`move a0, v0`) plus a separate first-test load = the loop counter initialised from another variable.
- Global with a lui per access, never an address register, reloaded after unrelated stores = function-local `static` (see docs/tgrally levers line 53). Defining it in the TU means the whole file .bss must lay out as the ROM's: define the neighbours in source order; IDO 8-aligns objects >= 8 bytes, so an unreferenced 4-byte gap is padding.
- Dead `addiu v0, v0, 0x30` after two loads = a pointer variable to a sub-array (`pos = obj[k].m[3]; f(pos[0], pos[1])`), frame word between ly and v confirmed it.
- t-ring 2 ahead before a halfword store pair = `q[0] = x & 0xffff; q[1] = x >> 16;` (BrU16QueuePop idiom, const-root invisible pops).
- as1 order of a hoisted reload vs a store: `if (x == 0) continue;` at the top instead of `if (x != 0) { body }` fixed it.
- Light: struct copy (lw/sw at pairs) = `D_8028C640[i] = D_8028A9F0;`; `lx == 0.0` (double) from cvt.d.s; eye z `0.0f` beside int zeros gives the second zero register.

Related: [n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md), [n64-ido-float-literal-spelling](../levers/n64-ido-float-literal-spelling.md), [n64-trackshadow-80234050-2026-10-06](n64-trackshadow-80234050-2026-10-06.md).

**Follow-up same session: BrSeasonDraw 0x80208CF0 T4, 34d9909f, gate 689/0.** Levers: an `(int)` cast on a static buffer passed to one function while another takes it as `char *` = two constant webs for the same address (fix the prototype); a pointer local `r = &tbl[i]` vs indexing each use changes whether uopt's temp spills to the bottom slot; reusing a dead variable for a constant arg (`y = 0x94; ...; y += 10;`) changes as1's delay-slot fill; unused `char buf[32]` sized the frame.

**Follow-up: BrWheelTyre 0x8025AC9C T4, 8ba4eea4, gate 690/0 (390 -> 0).** Levers: ugen's FP f4-f10 names at the head come from pass 1's end state, so fix every other web first and the head follows (inserting a function before it DOES move the head; the int ring did not move). Zero webs are identified by which uses share a register (one web, coloured) and which rematerialise (separate, uncoloured): spellings 0.0f / int 0 / 0. split them; `x * 0` (int) is not folded, any float-literal zero multiply is. A dead variable reused for a later temp gives a coloured f0 web; reading a value into the same variable in both arms raises its priority. Statement order (load before tq/q; h before the zero stores) fixes as1 placement.

**Follow-up: BrPaintCarView 0x80242BDC T4, 80ccea30, gate 694/0 (162 -> 0).** Levers:
- If the ROM loads a struct-field operand of a sum before the product (`add.s fX, from, prod`), the vector is a float ARRAY. cfe evaluates indexed loads first. With struct fields, uopt also folded the first `dx = (to.x - from.x)/16` into its single use, so dx lost its web: the ROM had 4 FP webs (f0, f2, f12, f14) and we had 3.
- Copies with interleaved loads and stores need a pointer variable per source. A second pointer (q) gave the ROM's v0/v1 and shifted the arg-reg colours to a1-a3.
- Else-arm constant stores in natural x, y, z order set the FP ring names. as1 still moved the 1.0 into the delay slot.
