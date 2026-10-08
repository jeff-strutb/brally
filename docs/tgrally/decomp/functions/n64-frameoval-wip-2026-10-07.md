# N64 frameoval wip

*Recorded 2026-10-07.*

> BrPaintFrameOval 0x80250FCC (paintshop.c) T4 by hand 2026-10-07, 27109f38, gate 713/0; the oval twins (FillOval 0x80250B58, DashOval 0x802523CC) share the ROM shape: address-taken square pair, guarded for(;;) walk, edge variables

**DONE 2026-10-07, commit 27109f38 (session 79d5c5), image gate 713/0.** 371 -> 0 by hand. Draft history in build/tgrally/n64/search/80250FCC/s79/ (b45.c final).

Levers, each read from the ROM (and the twins' ROMs, which show the same layout and loop tail):
- Corner swap test `if (sx1 - sx0 < 0)` with no width variable: PRE recomputes the difference in the swap arm (ROM `subu t1, t8, t9`).
- A local whose ADDRESS IS TAKEN (here `p = &sq.a2`, p otherwise dead, the frame's otherwise unexplained first word) stays in memory, is re-read after calls, is never strength-reduced or PRE'd, and keeps SOURCE operand order. Arrays also stay in memory but cfe puts an array element FIRST in a product; struct fields and scalars without & get promoted and strength-reduced. `(void)&x` is folded away and does nothing; volatile reloads every read.
- IDO 5.3 uopt does strength-reduce loop-invariant * induction products (scalars), contrary to an old note; that's what to avoid here.
- Loop: product-test `if` guard, then `for (inits;;) { ...; if (ex < ey) break; }`: inits land after the guard, and on the header line (as1 ties with the hoisted address).
- PRE-inserted preheader computations are emitted in expression-number order = first appearance. Edge variables computed left, top, bottom at each step (right edge inline) give the ROM's per-run order.
- y = ry*2 after the squares keeps ry's ring register live past rx*rx (ring free order).
- Phase-two guard written `sq.a2 * y <= x * sq.b2` (x first).

Tools: build/tgrally/ext/instrS9 = instrD7 uopt + prints (stderr, not CDX_OUT). probes in s79/probe/.

Related: [n64-particlelistdraw-2026-10-07](n64-particlelistdraw-2026-10-07.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).
