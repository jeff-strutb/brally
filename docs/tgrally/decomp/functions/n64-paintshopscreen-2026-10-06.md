# N64 paintshopscreen

*Recorded 2026-10-06.*

> BrPaintShopScreen 0x80243260 T4 2026-10-06 (87b8c2ef, gate 674/0) by hand, session 6c2550; ugen dest-scratch+move lever, ugen operand-need order, as1 move coalescing; two open uopt colouring decisions

Continues [n64-paintshopscreen-2026-10-05](n64-paintshopscreen-2026-10-05.md). Drafts: build/tgrally/n64/search/80243260/best_90_6c2550.c (body) + NOTES_6c2550.txt; tree preamble = best_7_tree.c (applied in working tree, not committed).

Levers proven (2026-10-06):
- ugen evaluates a binary op's higher-need operand first (leaf Smt load need 1; Rmt var / int ldc need 0; unary = max(child,1); binary equal -> +1) but EMITS operands in ucode order. So `addu x, sra` with the sra computed first means the shift subtree was heavier.
- ugen "dest scratch": for `var = (expr) OP const` with OP a shift or mul, ugen computes the child into var's register, the root into a ring temp, then `move var, temp`; as1 folds the move, so the object shows var written directly but the t4-t9 ring advanced by one (the "uopt-invisible pop" earlier notes needed). Used: dx = (D470.w - DB94.w) >> 1 (DB94 centring) and i = (i - DB94.x) >> 2 (eyedropper).
- as1 copy-propagates register moves and store->load forwards, renaming either side; registers in the object are not always ugen's.
- ugen ring = FIFO over t4..t9; free of an already-free reg is a no-op; MOVE_END on cache hit. tools: scratch ringsim.py (validated 0 mismatches on a full trace).
- uopt address webs: `(&D_8028D470)[2].x` puts a D490 read on D470's web (kills D490's t4 peel piece); `area->` (const-propagated pointer) reads make the statement's store absolute.
- uopt splitting: piece growth only along SUCCESSORS inside the web's live set, accept iff s6 < s4 && 2*cl >= intf + s6 ([CDX] adj in build/tgrally/ext/instr6). Constant webs' live set = blocks reachable from an occurrence that reach an occurrence; split colour = whole-web bestcolor (s5 if the web spans calls).

DONE: T4 87b8c2ef. Final lever: an empty `if (r) {}` after the D490.y store splits the layout block, so &D480 small web spans 2 blocks (save 4 -> 2) and the CD8[0].y value web (3) takes v0 first. Prior state was:
LATEST: build/tgrally/n64/search/80243260/full_9_6c2550.c (FULL FILE incl. preamble; tree left at HEAD) = DIFF 9.
Eyedropper solved: `extern signed char D_8028DC80` + `if ((unsigned char)D_8028DC80 == 0)` (keeps lbu, makes the setup's `= 1` a J constant) and no empty `if (r) {}` after BrPaintGet. Forcing value(CD8[0].y web)->v0 and &D480->v1 on full_9 gives EXACT, so the ONLY open item is that priority: &D480 small web save 4 > value 3. y = CD8[0].y + ... makes value 4 (tie, value wins) but loses the ring step ugen needs before the D480.h load (ROM acts as if the D_80369CD8->x reload reg t6 stays allocated to the end of the layout block). Scratch diagnostic uopt with growth veto: build/tgrally/ext/instrNG/out/cc (CDX_NOGROW=sym:blk, also logs [CDX] shared), patched from build/tgrally/ext/instr6/uopt.c.

Older open notes:
1. Layout colours (&D480 v1, &D490 t2 in ROM): best_90 has the right ring but &D480's piece grows into the grid-loop head (adj 20 >= 20) and takes t2. r18 (y = CD8[0].y + D480.h + 2, loop-2 x via (&D470)[2].x) has all four colours right but is one ring pop short before the D480.h load.
2. Eyedropper constant 1 (ROM v1 in both arms): our L-typed constant-1 web starts at D_8028DC80 = 1 in the DB94 block and spans calls, splits for s5, pieces unprofitable. Removing that occurrence (diagnostic) gives a v0 web, still not v1.
