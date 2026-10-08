# N64 pakmanager wip

*Recorded 2026-10-06.*

> BrPakManager 0x802534DC T4 by hand 2026-10-06 (fde8e379, gate 673/0): constprop renumbering, first-fit spill slots, L37 empty reads to fix a priority tie; the save formula and how to read live-block counts with instr7

Body: build/tgrally/n64/search/802534DC/draft_m4.c. Preamble edits (in the uncommitted tree copy): BrImage.w unsigned, the eight file statics deleted (function statics in the body). Earlier history: [n64-pakmanager-wip-2026-10-05](n64-pakmanager-wip-2026-10-05.md).

Levers, in order (each one reasoned from the instrumented uopt, build/tgrally/ext/instr7 = instr6 + logs: newbit, spillcand/spillreuse with expression trees, lrblocks, saveocc blk=, CDX_GRAPH, CDX_BBDUMP):
1. 190 -> 60: `y = 0x3f;` before the slot-number loop and `i * 9 + y` in all three prints. uopt numbers expressions (bit positions) in READ order; constant propagation re-hashes later, so the row expression gets the last number. That fixes loop 2's emit order (PRE inserts are emitted in bit-position order) and the spill-slot order.
2. 60 -> 43: one more 4-byte local below `name` (diagnostic `int u9;` after name). f_spilltemps hands slots first-fit in bit-position order; with the extra local the temp area starts at 0x4C and every class lands where the ROM has it. Natural owner still to choose (a block local is allocated after function locals).
3. 43 -> 42: case 5 compares written 061/062 first (L67): `D_8036A061 == D_8036A063`, `D_8036A062 != D_8036A063`, `D_8036A061 != D_8036A063`.
4. 42 -> 33: case 3 computes `bx` before `ty` (equal save 1.5, ties go to the lower web number = first read).

Open tie: web 161 (cvt D_8036A063, save 70/7 = 10.0) is coloured before web 59 (D_8028DD98, 92/12 = 7.667) and takes a0; ROM has 59 in a0, 161 in a1. Formula: save = gross / div, div = ((occ + live blocks) - 2) >> 2) + 2 (L7/L29). 59's live blocks 27 (case 1: 98-108 incl. the two blocks the `if (i != 0);` workaround adds; case 2 124-139; case 5 192-216). 161's heavy part is case 5's first branch (decrement loop, 35 of 70). Without the workaround 59 is 92/11 = 8.36, still short; the workaround itself is needed to stop uopt hoisting the last-free loop's `i = 0` into loop 1's exits (block-local loop counters avoid that but get v0, and LFTR then turns loop 1's `<` into `!=`).
Dead (tested once each): k/j counters for case-1 loops, both resets before loop 1, do-while last-free loop, `i;`, `D_8036A063 = i - i`, one static array for 060..063 (1093), `(i + 7) * 9`, `(i + 1) * 9 + 0x36`, `n = i + 1` copy, y induction variable in loop 1.

**DONE 2026-10-06: T4, fde8e379, image gate 673/0.** The tie closed with two empty reads `if (D_8036A063 != 0);` in case 5 (one at the end of the else arm, one after the if/else): they make the D_8036A063 value live through the first arm, raising its live-block count to n=34 (div 10, save 7.2 < D_8028DD98's 7.667). One read alone reached only 7.889 (n=33). Read after the case-2 move instead emitted a real branch (569). Method that worked: print save inputs per web (instr7 `lrblocks` + `saveocc blk=`), compute the n each edit needs from div = ((n-2)>>2)+2, then place one zero-footprint read where the live range grows by that much.
