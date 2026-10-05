# Dlvtx texgen t2

*Recorded 2026-09-24.*

> 2026-09-24: 0x10022600 BrDlVtxGen (1212 B) hand-transcribed in src/core/drawing/br_dlvtx_texgen.c, 86 -> 50 mismatched instructions; T3 route (Gate B passed, live oracle EQUIVALENT on 5 scripts / 40 captures). Family-wide levers for the six tu_022 vertex loaders: 1-based matrix stack gives the lea, re-derived pn gives the post-test induction.

Original TU tu_022 is **/O2 /Op** (fn.py needs `FN_OPTS="/O2 /Op"`; t3.py pins it via config/t3_variant_c.csv).

Levers proven on 0x10022600 (each moved a whole class):
- **Matrix stack is 1-BASED**: `extern BrDlMtx DAT_105ccd50[]; m = DAT_105ccd50[top - 1].m;` → VC5 emits `shl r,6; lea r2,[r+0x105ccd10]` (disp = sym-0x40). Every zero-based spelling gives `add`. Same construct in 0x10021080/21C70/221D0/22BF0/23360 (told session; took them 84->78).
- **Re-derive `pn = &pSrc->n1` at the top of each pass** (no `pn += 8`): VC5 strength-reduces it to the esi induction set up AFTER the zero-trip test (hand-advanced = before).
- Loop tail: `pSrc++` before `pVc++`. Head: `pV = &base[v0]` BEFORE `n = ...` (count stays eax, p in esi, `xor ecx,ecx`).
- Lights = N64 Light records read as bytes; z/y MVP columns absolute `*(float*)0x105d17xx`, x/w extern; inline /Op casts; dotX before the s store.
- t3.py normaliser gap fixed: `lea R,[R - A]` (negative reloc addend) folds to `add R, A` (cbbe86e1).
- Peer lever (0x100221D0): `(double)pSrc->x * col` flips the x-term role there; NOT useful here (our role already right; issue slot wrong).

Residue at 50 (dead, x87 only): MVP x product issued 2nd not last; m[0]*n0 role; 3 light-setup fxch (dead under all 720 statement orders); texgen dotX spill slot 0x2c/0x30. Related: [tyre-t4-volatile-parens-tu-2026-09-24](../levers/tyre-t4-volatile-parens-tu-2026-09-24.md), [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md).
