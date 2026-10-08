# N64 sndcarstep wip

*Recorded 2026-10-06.*

> BrSndCarStep 0x8022BCB4 (audio/sndcar.c) T4 by hand 2026-10-06, afd14835, gate 676/0 (session 6c2550). Levers: constant webs merge by value+dtype (unsigned extern splits them), web numbers = first appearance (spill-slot order), PRE needs a non-critical pred (structured ifs vs goto), ugen .noalias for array-indexed stores

DONE: T4 afd14835, image gate 676 placed / 0 bytes. Exact file: build/tgrally/n64/search/8022BCB4/exact_6c2550.c. DIFF 837 -> 0.

Levers proven (hand, forcing only to confirm):
- A constant web spans EVERY occurrence of that value with the same dtype in the function. The knock's `3` was one long web with the `D_8028C800 == 3` snow tests, so its priority sat below k (ra), ra was forbidden and the piece failed growth (2*cl 18 < intf 19). `extern unsigned int D_8028C800` in the TU split the compares off, and the piece grew in a1.
- Web NUMBERS follow each expression's first appearance. Numbers decide priority ties AND the spill-temp slot order (f_spilltemps walks webs ascending). `k = car->slot * 2` gave the arms' slot*2 index web 11 (spill -76 ahead of ch). `k = car->slot << 1` renumbered it, and ch's spill moved 0x28 -> 0x2c.
- `if (cond) goto level;` leaves a separate empty join block after the rolling clamp. PRE inserts the xf38 load there and the goto edges reuse v1. Structured `if (!(cond)) { ... }` merges the joins, so there's no non-critical pred for PRE, and the ROM's load at level comes on every entry (beql delay loads).
- A reload `m = car->sndImpact;` at the end of each case arm (not after the switch) gives the ROM's lbu in each arm's b delay slot, with the default edge skipping it.
- ugen emits `.noalias $reg,$sp` only when the store tree's base is a known global (D_802A4920[k + 1].x), not a pointer variable (ch[1].x). Without it as1 can't lift stack loads above the stores. Check `cc -S` output for .noalias when only the post-call schedule differs.
- Also: the end compare `D_8028AB0C == 1` (not `1 ==`).

Diagnostics added to build/tgrally/ext/instrNG/uopt.c: CDX_GROW=sym:blk (force accept), [CDX] seedintf/seedprune (a piece's interferers at its seed block, -1 webs pruned). Scratch grader with log: scratch dgl.py. Related: [n64-huddialdraw-2026-10-06](n64-huddialdraw-2026-10-06.md), [n64-paintshopscreen-2026-10-06](n64-paintshopscreen-2026-10-06.md).
