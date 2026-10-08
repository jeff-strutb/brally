# N64 carslotswap

*Recorded 2026-10-06.*

> BrCarSlotSwap 0x80229700 (rank.c) T4 2026-10-06, 7c92987a (session cdc182), gate 684/0: continue enables LFTR, one SR web across loops, goto into shared block, base+x vs &base[x]

**DONE 2026-10-06, commit 7c92987a, image gate 684/0.** Hand transcribed 431 -> 0 from the 643a5c draft (build/tgrally/n64/search/80229700/, final.c, w*.c steps, tr.py = instr7 trace helper, mk.py = splice body into TU).

**Levers (IDO 5.3), each read from the ROM then confirmed with one edit:**
- **`continue` lets uopt LFTR the loop counter.** With list[i] indexing and `continue`s, uopt replaced `i < n` with a pointer compare (sltu, i spilled to its home). Nested ifs (PC twin shape, empty `if (x64 < K) {} else if ...`) keep `slt i` and the SR pointer. Compound loop conditions (`nFree != 0 && i < n`) also block LFTR.
- **The same address expression in several loops is ONE SR/expression web** (`&list[i]` in three loops = one web, s5 throughout). Its long range lowers its priority; that is why the ROM ranked it under seen/nFree.
- **ROM moves `move s0,v0` and an address calc into each arm before a shared block** = the arms reach one block via `goto save;` (label inside the second arm's if). With the join written as fall-through after if/else the copy sat in the join. Car taken just before `goto` in arm 1 (uopt hoisted the copy into the test block).
- **Block count to break a priority tie:** save = gross/div, div = ((occ+liveblocks-2)>>2)+2; one empty `{}` branch added the block that put nFree above the SR web (50.05 vs 50.0).
- **Commutative add order in ROM matched source order only after writing the car-less arm in the same order as the car arm** (`(a - b) + (float)(...) * lap`).
- **Reusing a float local (`d`) as a swap temp** gave its register (f2) instead of a new web's f0.
- **Counter init order:** delay slot gets the last init statement; ROM order open, i, nFree, seen = `open = 1; for (i = 0, nFree = 0, seen = 0; ...)`.
- **`D_803239A0 + x` vs `&D_803239A0[x]`:** with the base already in a coloured register, `&A[x]` emits `addu d, base, off`; `A + x` emits `addu d, off, base`.
- Frame: register-only locals can be declared in ROM frame gaps to move visible homes (tA/tB) without changing codegen; web numbers follow first appearance, not declaration order.

Related: [n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md), [n64-pakmanager-wip-2026-10-06](n64-pakmanager-wip-2026-10-06.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).

**Also 2026-10-06: BrPaintClearMenu 0x80247B0C (paintmenus.c) T4, ad49b57f, gate 685/0** (325 -> 0):
- Loop counter reused as a later nested-loop counter (i = plot x) is one web with the inner loop's weight: it outranked the box-row SR pointer (s0/s1). Found from saveocc: no edit to the loops themselves could close a 54 vs 14 gap.
- Tied webs (1.5 save) colour in first-appearance order: write the offsets dx1, dy1, dx2, dy2 to get s3, s6, s7, fp.
- ROM per-arm `store; b epilogue` tails = a switch whose cases each do the store; a store after the switch is cross-jumped into one block.
- `r[0][1] + r[0][3]` in source came out r[0][3] first in the ROM's operand order (and vice versa): commutative int add order is reversed from source here.
- Reading a just-stored global (`DB68 = DB70; ...[DB68]`) keeps the value in a ugen temp instead of a coloured v0.
- Register-only locals declared above y moved the frame's visible homes to the ROM's offsets.

**Also 2026-10-06: BrObbOverlap 0x8025F4F8 (collresp.c) T4, 153467bc, gate 688/0** (516 -> 0):
- **cfe reorders commutative ops**, not uopt: for an assignment RHS it emits V(op(L,R)) = op(V(R), N(L)) (N = as written). So `a + b*c + d*e` comes out with the last term first; natural source order gave the ROM's trees, hand-"ROM-ordered" source got scrambled. Tool: build/tgrally/n64/search/8025F4F8/u/run.sh (cfe -> x.B, uopt -> x.O) + uc.py FILE LINE LINE dumps ucode per source line.
- `d = expr; c = ABS(d);` (two float vars) gives raw f0 / magnitude f2; `c = expr; c = c<0?-c:c` keeps one register.
- `in = c <= s; ok &= in;` = comparison result in a coloured v0 (tests 1-6); `ok &= c <= s` = fresh ring temps t6.. (tests 7-15).
- Opcode-only diff (ops.py in the same dir) separated structure (5) from allocation early.

**OPEN 2026-10-06: BrSkidAge 0x8023BF60 (particles.c), 3 diffs** (cdc182): compare temp `v0->ob[2]` lands in a0 (ROM v0) because the skid-point base web (v0) is live in the same uopt block. **uopt splits a basic block once its cfe-level `lod` count would exceed 21** (calibrated with straight-line probes: 2-lod stmts 10/block, 3-lod 7/block, 4-lod 5/block; counts cfe `lod`s, not uopt's, not ilod/istr). Ours: [154-157]=20 | [158-161]=19, so base's last use (half[0], line 158) shares the compare's block. ROM needs a split between 158 and the compare: +3 lods in 158-161 (or equivalent) with identical code. Diagnostic `u[0] = dt + dx + dy;` before the if (adds code) gives the ROM's v0 -> mechanism confirmed. Ruled out: h before/after 158 (copy-prop makes h[1] base-relative), h[0] in 158 (load via v1), no h (all base-relative), full vertex paths in the compare (168), scratch `a` variable (54), pointer p for pos (45). Tools: build/tgrally/n64/search/8023BF60/{probe.py, u/run.sh, u/cnt.py}. The frame has an unused `int a` above drop: possibly a dead statement there in the original.

**OPEN 2026-10-06: BrSkidStep 0x8023B418 (particles.c), 6 -> 2 diffs, draft build/tgrally/n64/search/8023B418/w2.c (tree untouched, T3 certification kept).** Lever found: **as1 schedules by .loc line** - hoisted (PRE) code inherits the last .loc in its block; when that line is later than `s = 1.5f`, as1 issues the lui first (assembling hand-edited ugen .s with `cc -c x.s` proved it). Arms 1 and 2 ending in `goto spread;` (label before `if (!hold)`) give the ROM's order. Left: f's spill temp at 0x64 (ROM 0x70): uopt spill-slot first-fit; web for car+4k (s1 SR pointer, coloured but still slotted) holds slot 0x70 and interferes with f's expression web; ROM must lack that slot holder (maybe a pointer variable in one of the "unused" u[1] frame words).

**OPEN 2026-10-06: BrFrameTintSetup 0x80218D5C (fog.c), 97 positional / 4 structural.** ROM: 0xFF (dt8 store constant, web 313) takes a1 before the tint-component load webs (131/136/141 -> ROM a2/t1/t4); ours 313 save 2.67 (nocs 3) < 131 save 3.0 -> a1 to 131. uopt hoists 0xFF+0x66 before the AA78 test in ROM (0x66+0xCC in ours; as1 fills the delay slot from the else arm). ugen spill slot 0x20 (ROM) vs 0x24; frame 0x50 = 4 more words than `int i` (u[4] reproduces the size, not the slots). Ruled out: r/g/b locals for the ramp (451). Stores use dt8 constants, arithmetic dt6 (separate webs).

**Also 2026-10-06: BrCrTriContainsPoint 0x8025B3B0 (collresp.c) T4, 06067126, gate 696/0** (41 -> 0; old permuter drafts all stuck at 41):
- **uopt walks the code twice; web numbers = first sight in that walk.** Pass 1 numbers everything; constant propagation (`n = 3` used as `% n`) then rebuilds every expression that contains n in pass 2, and those get LATE numbers (webdetail bb=-1, exprtable=-1). Phase-2 colouring goes in web-number order, so late webs lose to early ones. A literal `% 3` gives the same code but builds the scaled indices in pass 1, ahead of the vertex base loads.
- Tool: the instr7 log's `newbit site= pos=` records are the numbering walk (pos = web number); the expr words show the opcode (0x5b mpy, 0x69 rem, 0x36 ilod, 0x7b str) and operand pointers, so you can see which expressions are rebuilt.
- A dead store keeps its numbering effect after uopt deletes it: `a1 = 0.0f;` between a0's ternary and a1's put a1 ahead of the n[1] load (|n1| f12, raw f14). A store's destination var is numbered BEFORE its operands.
- Badouel GG RayPolygon: `r = 0; i = 2;` before the first edge statement; the ax/ay copies of the v0 corner were wrong (they spilled to different slots).

**Also 2026-10-06: BrPathWalk 0x802290C4 (ctlai.c) T4 7825f201:** a float constant stored in a loop arm (`frac = 1.0f`) became a coloured constant web hoisted to entry; moving the reset into the for-step (`i++, frac = 1.0f`) makes ugen build it where stored (lui in loop + mtc1), as the ROM. **BrWheelGroundProbe 0x8025E96C (tyre.c) T4 49559e6a:** (1) a sub-sum in its own float var (coloured f12) moved the CSE'd normal loads to f14/f16/f18; (2) **ugen's FP temp choice is hinted by the memory slot the value is stored to, function-wide** (ugen -S: loop mount stores decided the entry block's temps); fix by writing all three `mount[k] = mount[k] + world[k]` alike (`+=` reverses operand order). ugen temps otherwise rotate f4,f6,f8,f10,f16,f18 per function (reset per function). Use `cc -S` (scratch as1/ent.sh pattern) to read ugen temps quickly. **BrCrPlaneResolve 8025DCB8 OPEN (4):** as1 hoists `li v0,1` above the last sign test's bc1f (fill from target) in ours, not in ROM; the .s route never hoists, so it uses info only in ugen's binary output.

**Also 2026-10-06 (cdc182): BrTyreSprings 8025EDBC a398a9b9, BrTextPrintAt 8022F694 + BrTextWidth 8022F720 c098dbc4.** Levers:
- **Constrained vs unconstrained FP colouring:** a web is constrained (p1, priority order) when numintf >= the class's register count (12 FP here); unconstrained webs (p2) colour in web-number order. Extra float locals (a separate t) pushed the hoisted constants to constrained, so they grabbed f2/f12 first; merging t into d made the constants unconstrained and gave the ROM's order. If the ROM's registers fit "web-number order", look for webs to remove.
- **cfe always evaluates a converted load (cvt(lod G)) ahead of a plain var/param in a commutative op**, whatever the source order; two plain vars keep source order. `int h = G; float fh = h; y * fh` -> uopt propagates fh back to cvt(h) but keeps y first.
- **uopt canonicalises compares variable-before-constant**, but a constant reaching the compare through a variable (`esc = '%'; esc == c`) is substituted only in pass 2, so the source order (constant first) survives.
- An empty `if (s->x1b4) { }` before `prev = s->x1b4` numbered the load ahead of prev (a2/a3); a store's destination var is numbered before its operands.
- Spell a zero by which constant web it must share: int 0 / double zeros merge with each other and SIGN's `> 0`, apart from 0.0f.

**2026-10-06 late (cdc182) open drafts with real progress:** BrAnimUpdate 8021D84C q10.c (opcode 2): float constant webs are numbered in uopt's walk order, which follows the CFG not the text; `if (x) {} else goto L;` instead of `if (!x) goto L;` reordered the walk so 4096 got f18 and the frame became 0x28; `for (k = 0; k < nv; k++)` with the count in a local gives blez + bne. BrDebugPrint 80254878 w5.c (opcode 62, was 308): row/x0 locals written back at exit, printable-first, row pointers (p + w)/(p + w + w). BrVec3Cross a16.c (all registers, as1 load order left). ugen/as1 note: as1 cannot hoist a target-arm li whose register is the branch's own operand.
