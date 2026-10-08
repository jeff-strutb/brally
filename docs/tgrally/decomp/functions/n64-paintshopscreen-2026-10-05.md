# N64 paintshopscreen

*Recorded 2026-10-05.*

> BrPaintShopScreen 0x80243260 (6.5 KB, menus/paintscreen.c) T3->T4 in progress by 373356: region metric 250 -> 7 (best_7.c + best_7_tree.c); TU-defined data shares lui at; as1/ugen levers; open allocation residue

Claimed 373356. Drafts in build/tgrally/n64/search/80243260: best_7.c body + best_7_tree.c preamble (NOTES_373356.txt). Tree file has preamble edits applied (BrCarDefaultColour int, BrPaintSwatch unsigned fields, and now `BrPaintSwatch D_80369B98[16];` DEFINED in the TU); tree BODY still the old T3. Apply body+preamble together when T4; check the other paintscreen.c functions still build exact after the D_80369B98 definition.

Method: read ROM register classes; ring = t4-t9 LRF; v*/a*/t0-t3/s* = uopt webs. Tools: build/tgrally/n64/m2tools/rmet.py (blind per-region), amet.py, dv.py, ugtrace.py (ugen free list), ugs.py (ugen raw -S per line range, pre-as1), capuc.py (uopt output ucode per line range; CAPDIR=scratch/capu UCFILE='before-8-*' gives cfe->uopt input), alldec.py/inblock.py (uopt p1 decisions), `cc -Wa,-R` (as1 scheduler trace).

Levers found this round (all hand-derived):
- as1 SHARES one `lui $at` across consecutive absolute accesses only when the symbol is DEFINED in the TU (probe-proven). ROM shows shared %hi -> define the data in that file. (`racestart.c`, `loadsave.c` already define data.)
- as1 also hoists li constants out of blocks and forwards store->load (keeps either register: the live-out one).
- ugen frees a load's reg before allocating an op's result; operands freed in operand order; cache-hit = re-free (MOVE_END).
- Statement order inside loop bodies sets the peel's ring order (palette loop 2 = w, h, x, y).
- Grid loop x = `D_8028D480.x + i * 0x4a` (uopt induction value; init carries the loop line so the sll hoist schedules first).
- Layout: `D_8028D480.y = CD8[0].y;` BEFORE the D490.x line; `y = D_8028D490.y + D480.h + 2` gives the missing ring pop (as1-forwarded read-back) but flips v0/v1 (value web save 3 < &D480 4).
- cfe emits symbol field accesses as direct Smt; uopt forms lda webs from them; `area->` reads (area = &D470 const-propagated) become separate direct loads, NOT lda-web uses; a store whose RHS reads via `area->` stays an absolute store.

Late 2026-10-05 (diagnostic forcing allowed): z1.c gets all four layout colours by hand (see NOTES); remaining causes proven by force: a uopt-invisible ugen pop missing in the layout, and D490's peel piece taking t4 (kills t4 from the ring). instr6 logs f_split seeds/regions and f_addadjacents budget (accept iff s6 < cl && 2*cl >= intf + s6).

Open (best_7): layout v0/v1 (value vs &D480) and t1/t2 (D470 vs D490 piece; p1 order is whole-web save: D490 0.36 > D470 0.31); DB94 centring block ring + store form (ROM: both stores via s4, D470 loads absolute, results ring t9/t6, dx/dy v0/v1); eyedropper constant 1 = uopt v1 piece in ROM (ours web 87..264 declined); one extra instruction there.
