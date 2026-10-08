# N64 rumbleupdate wip

*Recorded 2026-10-07.*

> BrRumbleUpdate 0x80260490 (rumble.c) WIP 66d9b9 2026-10-07 - 170 -> 4 by hand. The middle loop uses its own counter j; the frame counter goes through a local k; the counter's address register is left

Drafts are in build/tgrally/n64/search/80260490/s66d (run.sh NAME). The best draft is e1.c (DIFF 4). It is not committed, so the source is still the T3 at 170.

Levers, each measured:
- **The middle (pulse) loop uses its own counter j** (b3: 170 -> 11). uopt builds an expression web per lexical expression and variable, so `&D_8031A3F8[i]` was ONE web across all three loops. Loop 1 crosses calls, so that web needed s1. With j, the pulse loop's pak address is its own web in a0, computed once before the on/off branch. That frees a saved register, so &D_802A4BEC goes into fp and the constant 2 into s7, as in the ROM.
- **The frame counter goes through a fresh local** (`int k`): `k = D_802A4C08 + 1; D_802A4C08 = k = k & 0x3f; if (k == 0 && ...)` (e1/e2/f1/f2 all 4). The test must use k, because a reload of the global adds a third reference.
- **Left (4 words):** ours keeps &D_802A4C08 in v1 (an rlda web, save 1.0) for the lw/sw. The ROM accesses the global directly (lui v0/lw, lui at/sw). These made no change: volatile, file static, `k = D; k++`, assignment inside the condition, and `% 64` (worse). Diagnostic split force on that web (p1:w122=s) leaves the rlda in ucode (t8), so the ROM has no lda candidate at all for the counter. uopt f_ldainreg is in-reg unless context == 4.
Related: [n64-collgridcell-t4-2026-10-07](n64-collgridcell-t4-2026-10-07.md) (distinct locals = distinct webs).
