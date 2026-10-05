# N64 brmenu frame layout

*Recorded 2026-10-05.*

> BrMenu 0x8020AD5C T4 2026-10-05 (af920333, gate 664/0) by hand: solved the frame from ROM slot offsets with the frame-size arithmetic; buf in an inner block; block variables split webs; pointer-typed prototypes; result set after the call

BrMenu (n64/src/menus/frontbuttons.c) T3 -> T4 on 2026-10-05, session, commit af920333 + README e26ab2a5, image gate 664/0. Pure hand transcription, 289 -> 0, about 12 reasoned edits. Follows [n64-carselect-handtranscription-2026-10-05](../functions/n64-carselect-handtranscription-2026-10-05.md) (same BrRomUnpack prototype fix gave the first 10).

**Frame arithmetic (reusable):** IDO frame = round8(args + saved + 4*homes + ~12 temp bytes); measured f(37 homes) = 0xF8, f(39) = 0x100, f(42) = 0x110 for a 0x38 arg area + 0x20 saved. Homes: function-level decls top-down in declaration order, then EVERY block-scope local in source order (each gbi macro `_g` its own slot, measured: gSPClipRatio = 4, gSPLight = 1; sibling blocks never share, checked with a 3-block repro), then cfe temps. Use the ROM's few visible slots (spills, &buf) plus this budget to prove where the block slots must sit. Here only one layout fit: `buf` declared in a block opened AFTER the lighting macros (their 7 `_g` slots above it).

**Register levers that closed it:**
- One variable used in two places merges into one web (one colour). ROM showing two different registers for "the same" variable = two variables. BrMenu needed separate block variables for the turning-icon row (v0 vs the loop's a0) and the sound-channel loop counter (a0 vs i's s3). Each new block variable also fills one of the frame's otherwise unexplained slots, which cross-checks the layout.
- Pointer vs int typing of an address constant changes its web priority: `BrRomReadSize(char *)` with `D_001BF480` (no cast) gave the ROM's s0/s1; an int literal gave the right colours but lui/ori instead of lui/addiu.
- Statement position relative to a call: `result = 3;` after `BrPadConsume(...)` puts the store after the static's store-back, as in the ROM (result is a split, memory-resident local, so uopt is free to move it).
- `a != b` compiles to `beq b, a`; compare against the promoted static (`D_80316244 != D_80316220`).
- dv.py splices only the function body into the TREE, so declaration changes outside the body (prototypes) don't show in it; grade whole-file drafts with `n64alloc trace --file`. cs.py's .rodata base can be off (lwc1 offsets), so ignore those and trust the grader.
