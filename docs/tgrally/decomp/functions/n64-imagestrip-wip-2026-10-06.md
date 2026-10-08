# N64 imagestrip wip

*Recorded 2026-10-06.*

> BrImageStrip 0x80245B00 (image.c) T4 2026-10-06 72f74234 (6ce12a), gate 685/0: w*1 line bytes splits w under the callee toll; fmt through every case orders tied webs; a gbi command written as a one-statement-per-line block changes as1 scheduling (macro = one line)

Draft: build/tgrally/n64/search/80245B00/draft_6ce12a_d6.c (function body, DIFF 6).

**Levers (each read from the ROM, one edit each):**
- 693 -> 28: case 8 tile line `((w * 1) + 7) >> 3` (libultra's width * LINE_BYTES). The w*1 CSE temp (ROM t3, a copy of w made at the switch head) takes one use off w, whose total save drops to 14 < the callee toll 14.2 (clamp(nBB/4,4,60)), so w splits (t5 after the calls) instead of taking s0. Frame 0x100 -> 0xF8, no saved register.
- dsdx/dtdy as fixed-point 1.0: `(0x400 - (w << 10)) / dw`, `((w << 10) - 0x400) / dw`, `(0x400 - (h << 10)) / dh` (ROM distributes; `(1 - w) << 10` and `* 1024` both differ).
- `dw = -dw;` before `flip = 1;`; low-res halving order x, y, dw, dh.
- Every case writes its format through `fmt` (case 4 `fmt = 4`, case 8 `fmt = 3`): uopt numbers `(fmt & 7) << 21` at its first read (case 4) before constprop folds those cases, so in case 16 it outranks the tied _g macro locals (save 3.0, ties by web number) and takes a0; the _g webs then get a1/a2/a3 as in the ROM.

**DONE 2026-10-06, commit 72f74234, gate 685/0.** Closing lever: the tile-size command as an explicit block `{ Gfx *_g = (Gfx *)(D_8028A858++);\n\n _g->words.w0 = ...;\n _g->words.w1 = ...; }` (any multi-line layout). A macro expansion is ONE source line for as1, which then sinks the dead `_g` copy below the last store; with the statements on separate lines as1 keeps the copy before the w1 store, as the ROM. Wrapping the macro call across lines does nothing (expansion keeps one line). gDPLoadTextureBlock written as the libultra macro gives the same code as the expanded statements.

**Was open (6 words):** at each case end the ROM has `or t9; move a3,v0; [b;] sw t9,4(v0)`, ours `or; sw; [b;] move`. The move is TileSize's dead _g copy. Binasm experiments (scratch is/bxe.py: move position, store bases, .loc, .livereg, w1-before-w0) never change as1's placement, so the ROM's binasm must differ in a dependency, not in order. Case 16 has no branch and still differs, so it is in-block scheduling, not delay-slot filling.
