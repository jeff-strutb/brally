# N64 modrowread bfa2d4

*Recorded 2026-10-07.*

> BrModRowRead 0x80256DEC (music.c) session bfa2d4: 107 -> 3 (draft bf/BEST_n3x_diff3.c, NOT committed); interference accounting solved; arpOn late-fold and one CSE save tie left

Draft: build/tgrally/n64/search/80256DEC/bf/BEST_n3x_diff3.c (run.sh NAME, helper scratch mr.sh printed per-web numintf). Committed source still at 107.

Solved (each measured):
- Interference target: four table bases must be numintf 21 (p2 -> s0-s3), row ptr/offset 22 (p1 -> v0/v1). Committed source already had that; it contains two CODE-LESS uopt webs (type-4 cvt temps: identify with CDX_DETAIL_WEB, `type=4`, and force `p2:wN=c22` showing only the $fp save/restore).
- ROM variables: n (a3) = case-0 note (`n = D.note & 0xff;` -- the & 0xff is ugen's invisible pop, one extra int temp that the ROM's case-0 names need), case 3 `n = D.note + D_803787D0[smp - 1]->relNote;`, note block `n = D.note + D_803787D0[D.inst - 1]->relNote;` (indexed, not via a pointer var, so L52 puts relNote first); v = &D_802A4798[i] with `v->x4 = 0` BEFORE the sample load (uopt sinks v to first use; that order numbers the voice table before the sample table -> s0/s1); note block `inst = D.smp = D.inst;` (dead inst; the chain puts the value in the cfe chain-temp web, t3, as the ROM) then `smp = D.smp;` (forwarded -> andi).
- Else branch keeps the chain `note = D.note = flags;` (its cvt web is the ROM's code-less t4 web).
- Case 0 adds `n + (unsigned)(...)` (L52 operand order). Case 1 AND case 2: `porta` first, `target = 0` second (the 64-bit zero takes an even/odd register PAIR; order decides which pair). Case 15 `x2 = x0 = D.param & 0xff` (invisible pop). Final block `D_803787D0[smp - 1]->vol * D.vol`.
- Open 1: `D.arpOn = n * 0 + 1;` is a stand-in: the ROM's 1 is inline (no hoisted constant web) and the counts prove no constant candidate exists; uopt folds x*0 after candidate selection. `param != 0` inside the if folds EARLY (becomes a hoisted constant). Natural spelling unknown.
- Open 2 (the last 3 words): sample-pointer CSE (dtype A) and a code-less unsigned index temp (dtype L) tie at save 30 and the index temp has the lower web number, so it takes t0 and the pointer t2 (ROM t0). Dead stores/reads of a pointer variable do not renumber; forcing the index temp to t3 gives EXACT. Removing the temp (index via the dead inst) drops a web and cascades.
Related: [n64-musicthread-t4-2026-10-07](n64-musicthread-t4-2026-10-07.md), [n64-impulsesolve-t4-2026-10-07](n64-impulsesolve-t4-2026-10-07.md), [n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md).
