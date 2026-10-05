# N64 textemitstring

*Recorded 2026-10-05.*

> BrTextEmitString 0x8022E4E0 T4 by hand (ccb1c0a3), 549 -> 0. Levers: uopt unit-cut via a read-once local; spill-temp stack slots follow WEB NUMBER (first occurrence in cfe ucode), not p1 decision order; cfe emits ?: ternaries before the rest of the statement

BrTextEmitString (drawing/textstate.c) landed T4 2026-10-05 (ccb1c0a3; image gate 671/0). Drafts + notes: build/tgrally/n64/search/8022E4E0 (tv5.c = shipped body).

Levers (all apply to any IDO 5.3 N64 function):
- **Spill-temp slots = web-number order.** f_spilltemps (uopt) hands out Mtemp offsets descending in ascending web number; the first-numbered web gets the highest sp slot. It is NOT p1 decision/save order (that theory was wrong for hours). Web number = first occurrence of the expression in cfe's output ucode. instr6 logs it: `[CDX] spilltemp ... web= off=` (CDX_LOG=1, TGR_TRACE_CC=build/tgrally/ext/instr6/out/cc).
- **cfe evaluates every `?:` in a statement BEFORE the rest of the statement's arithmetic** (results into temps, then `lod tmp ... swp sub`). So in gSPScisTextureRectangle the dtdy inside the clamp ternary is numbered before the plain t term.
- Fix shape: compute the term into a local at the exact point in source order its number must fall (here `u1 = (cell - 1) << 5;` between the w1 line and the RDPHALF_1 gImmp1). Placement mattered by one statement: before w0 = 11, after w0 = 3, after w1 = EXACT.
- Unit cut: `n = w * size / cell;` read once moved uopt's 20-load block cut one statement earlier (549 -> 56), freeing a3 for &D_8028A858.
- as1 line tie-break and `if (c == '%' && p[1] != 0) { n = p[1];` as in [n64-paintshopscreen-2026-10-05](n64-paintshopscreen-2026-10-05.md) / summary notes.

Diagnostic method that cracked it: capuc on ugen input showed Mmt offsets; grep 0x1001c4b4 (temp counter) callers in instr6/uopt.c -> f_spilltemps; one log line there.
