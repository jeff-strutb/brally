# N64 pakmanager wip

*Recorded 2026-10-05.*

> BrPakManager 0x802534DC (cpakmenu.c) T3 in progress, : DIFF 1055 -> 193 (4 register-blind hunks); best body build/tgrally/n64/search/802534DC/draft_m2.c (needs the header edits listed); what is solved and the one open uopt-order question

BrPakManager (src/tgrally/menus/cpakmenu.c, 4420 B). Claimed `802534DC ` plus the file claim. The tree file is back at HEAD. The best body is `build/tgrally/n64/search/802534DC/draft_m2.c` (DIFF 193, frame and prologue exact, 4 register-blind hunks). It needs two header edits to compile as measured:
- delete the eight file statics D_8036A060..D_8036A270 (the draft declares them as function statics);
- make BrImage's field at 0x10 `unsigned int w`.

**Solved (each confirmed against the ROM):**
- Function statics, not file statics: the ROM uses a fresh `lui` per access.
- Frame: pads `u0`/`u1` give `by`=0x9C and `maxFiles`=0x80. 0x40-0x54 are uopt temp homes plus the gbi `_g` block local at 0x54. A volatile or declared-after-`name` model is wrong (frame 0xD8/0xE0).
- Loop 1 prints `i * 9 + 0x3f` inline. uopt strength-reduces it to a spilled IV, so no `y` local there.
- `case 8: break;`: the jump table has 9 entries.
- Case 1's first-free loop: uopt copies loop 2's `i = 0` into the break edge, which leaves a `move a2,s2` and an extra delay slot. **`if (i != 0);` between `D_8036A063 = 0;` and loop 2** stops it (L37 discarded read) and gives the ROM exactly.
- Case 3 has no x local: `(0x280 - w) >> 1` is written three times (a spilled CSE temp), with `ox`/`bx` int locals. The unsigned `BrImage.w` stops their copy-propagation and fixes the add operand order.
- Compare operand order, case 2 and case 5: write `D_8036A061 != D_8036A062/063` (L67).

**Open (all colour/order, no structure left):**
1. Loop 2's preheader: the ROM emits the name pointer init, then the `&D_8036A070[i]` temp, then the `i*9+0x3f` temp; ours emits y first. A diagnostic with loop 1 not using the expression gives the ROM order, so in the ROM loop 1's IV is not the same CSE temp, yet it shares slot 0x40. Separate-temp spellings colour loop 1's IV (s1), and uopt canonicalises `(i+7)*9`. The temp homes are in the ROM's order (y 0x40, st 0x44, V 0x4C) vs ours (V 0x50, y 0x4C, st 0x44). Same cause.
2. a0/a1: the D_8028DD98 web (56, 7.67) should beat the D_8036A063 web (158, 10.0). Forcing it fixes those rows. Same access sites as the ROM.
3. Case 3: ox s0 / bx s1 / ty s2 in the ROM; ours has ty before bx (tie at 1.5, ty founded first).
`n64alloc force 802534DC p1:w56=c3,p1:w158=c4` leaves only the ring rotation that follows item 1.

**Spill-slot data (instr6, `CDX_LOG=1 build/tgrally/ext/instr6/out/cc`, `[CDX] spilltemp web= off=`):** ours V=33 (0x50), y=45 (0x4C), 67 (0x48, unreferenced), st=80 (0x44). The ROM order is A < V < B < st < y: y is numbered last, and there is one more spilled temp (A, below V). Slots descend by web number. Here the numbers do NOT follow cfe source order (V in case 3 gets a lower number than y in loop 1). Equivalent y spellings in both loops (`(i<<3)+i+0x3f`, `i*9U+0x3f`) renumber y but keep it second. These are dead ends: `register y`, volatile y/st/x, declared-after-`name` locals, a static struct, externs, file statics, a byte array, the switch-selector ternary, and `if (ty != 0);`/`mask` locals. Next step: find uopt's web-founding traversal order (instr6 uopt.c) and why V precedes y.

Tools that worked: the probe files in scratch/probe (sA..sAB reproduce the case-1 hoist), qsim2.py ring replay, ucd.py/ucall.py ucode dumps (cap = ugen-in, capu = uopt-in), and the compiler laws in build/tgrally/ext/n64-decomp-workbench/docs/compiler-laws/ido-5.3.md (L9, L26, L32, L37, L55, L67).
