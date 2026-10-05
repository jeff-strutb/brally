# Enemy color panels t4

*Recorded 2026-09-24.*

> 2026-09-24 0x1005EDC0 BrMakeEnemyCarColorPanels (1110 B T1) hand-transcribed to byte-exact T4 in ~20 probes; fn.py variant harness misreports index order here - probe in-tree

0x1005EDC0 (T1, 1110 B, largest free open fn at the time) → byte-exact T4 from asm, commit 7b117f99, filed in src/core/drawing/br_model.c (sibling of 0x1005F220). C, not C++: cdecl, plain ret, no EH; a /Tp compile of the same body scheduled worse. Neighbours 0x1005E7B0/0x1005F6C0 are C++-lane but this TU is not.

Levers (on the VC5-IDIOMS tail, 0b7bdb71): `unsigned short` pack temp → byte-wide operand loads; `(unsigned char)(w>>8) | ((unsigned char)w<<8)` → `mov bl,ah / mov bh,al`; named `penm` local → loop index as base register `[i+p+disp]` (inline spelling, decl order, pad count all inert).

**Why:** the first compile was already 16 bytes off; three spellings closed it.
**How to apply:** `tools/fnmatch/fn.py --var` gave different base/index order than the tree sweep for this TU, so probe in-tree with a scratch replace-and-sweep loop (~12 s each), restoring the base file afterwards. Related: [tyre-t4-volatile-parens-tu-2026-09-24](tyre-t4-volatile-parens-tu-2026-09-24.md), [declaration-order-tiebreak](declaration-order-tiebreak.md).

**Related (same session, 2026-09-24):** 0x100221D0 BrDlVtxLitDecal is DEAD CODE in retail - PROVEN: only writer of the selector 0x105CDA04=1 is 0x1001E8C0 (combine w0==0xFC317E02 && w1 in {0x5FFEF3FA,0x51FEF3FA}); sole caller 0x1001E770 = G_SETCOMBINE DL reader; the words appear nowhere in DLL/EXEs (only the cmp); census of all 38 asset files (~17.7k combine cmds) has no DECAL mode; 25/25 scripts UNCOVERED. A5 can never pass ⇒ only T4 finishes it.
