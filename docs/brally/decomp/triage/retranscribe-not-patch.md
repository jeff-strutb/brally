# Retranscribe not patch

> METHOD RULE from the BrTex3dExpand post-mortem: for a function with multiple Ghidra artifacts, RETRANSCRIBE divergent regions fresh from the original asm instead of patching Ghidra's output artifact-by-artifact. Plus: timebox per function by byte value; never claim 'one lever left'.

**2026-08-29, from the project lead after weeks on 0x100250D0.**

**Why:** patching Ghidra's transcription toward the original means every
Ghidra artifact (counter-fold, phi role-swap, folded ternary, twin counters,
do-while peel, widened temps - 9 classes catalogued in docs/brally/VC5-IDIOMS.md)
must be DISCOVERED and undone separately, each costing a session or a
workflow round. A fresh transcription written from the original asm, in
period C, never introduces them, so it never pays to find them.

**How to apply:**
1. When a function shows >2 structural artifact classes, stop patching.
   Read the orig disassembly block by block and write the C fresh, using the
   VC5-IDIOMS artifact catalogue as a "never write these shapes" checklist.
   Keep any regions already instruction-exact.
2. TIMEBOX by byte value: a function is worth sessions proportional to its
   share of .text (8.5 KB = ~2 sessions). Past the box, park it with a
   dossier and work the [residue-retriage-2026-08-28](residue-retriage-2026-08-28.md) backlog, which
   produced counted matches at a fraction of the cost.
3. NEVER claim "one lever left". On 0x100250D0 that claim was made five
   times and was wrong five times. Say "unknown layers remain".
