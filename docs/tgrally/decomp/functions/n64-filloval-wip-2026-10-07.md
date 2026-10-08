# N64 filloval wip

*Recorded 2026-10-07.*

> BrPaintFillOval 0x80250B58 (paintshop.c) WIP d98055 2026-10-07: 250 -> 182, parked; only residue = first walk's step d (ROM colours its split piece a0); uopt split/penalty mechanics read from instrSP

**Parked 2026-10-07 (d98055), claim released. Best draft build/tgrally/n64/search/80250B58/s98/a5.c, DIFF 182 (source still at 250, not committed).**

a5 = BrPaintFrameOval's matched skeleton (r[2] radii, address-taken sq pair via p, b2b/a2b, ey/ex, d declared last at 0x44) with this function's spans (cx, xe, xs, i loops), each walk an explicit guard around `ex = ..., ey = 0, d = 0; do { } while (ey <= ex);`. The guard/do-while (not FrameOval's for(;;)+break) drops one block from err's range: err nocs 9, 20.22 > the span-end temp's 20.0, so err gets s5 and the temp s6 as in the ROM. Every register now matches the ROM except d.

Open: the ROM keeps the FIRST walk's d in a0 (move a0,zero in the preheader, sw a0,0x44 at the even head, lw a0,0x44 at the tail entry), memory in walk 2. Ours: d (save 9.11) splits; the piece {11,12,13,14,18,19,23,24,25} carries f22 store penalties at 12,13,14,18,19 (-10 each) -> save -6.33 -> dropped, d stays in memory. ROM piece must be {11,12,13,23,24,25} (penalties 12,13 only; v0/v1 live in 13 force a0).

uopt facts read this session (instrSP uopt.c):
- p1 candidate scan: web-number order, strictly greater save wins (ties -> lowest web).
- f_addadjacents: when the byte at 0x1001eb10 is 0 (our runs), growth accepts every reachable non-call block with cl != 0; the 2*cl >= intf+newshared test only applies when it is set.
- unit f22 = some successor outside the range (or a bvectin set hit); penalty -weight applies only if the range flag +34 is set (range contains a def or f20 unit, f_updatelivran).
- "[CDX] adj sym= blk= s6= s4= cl= intf=" lines in the log show each growth step.

Dead (tested): scalar ry/rx (coloured, 255), scalar b2/a2 (232), t/b edge variables (propagated), `(x & 1) == 0` and inverted if/else (same BFS), d = 0 before the guards (uopt sinks it), top-tested for after the guard (199), cx + (x >> 1) commuted (canonicalised).

Related: [n64-frameoval-wip-2026-10-07](n64-frameoval-wip-2026-10-07.md), [n64-dashoval-t4-2026-10-07](n64-dashoval-t4-2026-10-07.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).

**Re-triage 239431 (2026-10-07 evening): the "growth accepts every cl != 0 block" premise above is WRONG.** Our uopt enforces `s6 < cl_before && 2*cl >= intf + s6` on split growth (0x1001eb10 set). Drafts build/tgrally/n64/search/80250B58/w/ (a0.c = s98/a5.c, DIFF 182; dg.sh FULL.c [ENV=..] prints d's decisions/growth; gd.py VA FULL.c ENV=.. uses private uopt build/tgrally/ext/instrF5 with CDX_GROWDENY/CDX_SAVESET/CDX_PRIO/CDX_UNCONS/CDX_SHLOG).
- **CDX_GROWDENY=95:14,95:19 on a5 = 2 differing words** (only an as1 order of `addiu s3,s3,-0x24a8` in the walk-1 preheader). So the ROM's d piece is {11,12,13,18,23,24,25}: growth rejected into the two inner-loop preheaders 14 and 19, accepted into 18.
- a5 numbers: 13 shares 6 new webs (i 113, xs 111, xe 107, v1 x>>1 106, xe copy 104, v0 sx0+sx1 102); then 14: cl 11, intf 19, s6 1 (22 >= 20 pass); 18: intf 20, s6 0; 19: intf 20, s6 1. The ROM pattern needs intf EXACTLY 22 after {11,12,13,23} (+3 live interferers): 14 and 19 then fail (23 > 22), 18 passes (22).
- The four webs live there that ours drops at the first scan (save < 0): promoted r[1] (42, -6.5), r[0] (50, -9.17), sq.b2 (57, -11), sq.a2 (64, -3.75). They are exactly the ROM's other memory-resident walk-1 values. Scalar rx/ry (s1.c) gives intf 22 and the ROM's d piece exactly, but CSEs ry*ry across the walk (new temps, ry coloured v1 in setup, frame +8): DIFF 255.
- Dead: FrameOval-style t/b row locals (f1, identical IR), FrameOval for(;;)+break walks (g1, 184), centres named once (c1, 187), d processing order (CDX_PRIO moves intf the wrong way).
- **Keep-pending diagnostic (the project lead allowed forcing-only sweeps 2026-10-07):** instrF5 CDX_KEEP=webs CDX_KEEPUNTIL=95 keeps negative-save webs undropped until d is decided. Any 3 (or all 4) of {42 r[1], 50 r[0], 57 sq.b2, 64 sq.a2} pending → **2 words** (same as the growth deny); any 2 → 102. So the ROM's IR has ≥3 of those values as live, positive-save webs when d splits, and still never colours them.
- Constraint from the ROM: walk-1 recomputes `r[1]*r[1]` each iteration from 0x88, so r[1] is killed by the inner-loop calls (aggregate/aliased); a scalar ry is hoisted/CSE'd instead (s1). Removing the dead `p = &sq.a2` (h1) makes sq.a2/sq.b2 positive pending webs (+2: intf 21 at the 14 step, one short) but grows the frame 0x10 and colours their pieces (DIFF 232). Open: the third pending web and a form where sq stays uncoloured.

**eebbc5 (2026-10-07 late):** first-scan charges of r[1]/r[0]/sq.b2/sq.a2 (w/a0, instrF5 saveocc) = f22 exit penalties at walk-1 blocks 12/13 (f34 set because each web contains its setup defs) plus nl entries at 18/23; walk-1 uses give +10..+20. Address-taken scalar radii (s1 + dead `q = &ry; q = &rx;`, e1..e3) = 237..258: &D_8028DB58 then hoists to the function top.
