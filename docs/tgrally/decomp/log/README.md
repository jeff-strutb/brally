# Top Gear Rally: Dated progress log

- [n64-m1-complete-2026-10-04](n64-m1-complete-2026-10-04.md): N64 M1 reached 2026-10-04 (567/567 T3+T4) via BrPaintShopScreen; IDO s-reg allocation levers learned on it (unsigned index temp blocks SR, shift vs multiply, SR-step reload tells the source, empty-if LR extension, frame-size dead-stack A5 trap)
- [n64-m2-session-2026-10-04](n64-m2-session-2026-10-04.md): N64 M2 session (2026-10-04 night): 9 rows T3->T4 (M2 487/567); levers: ugen two-pass FP free list = operand order, address-taken struct keeps operand order, pointer vs re-index liveness, lane chain written out, block-scoped index, in-place param updates (dataflow), retranscribe Ghidra bodies from ROM; parked-row verdicts in build/n64/m2tools/verdicts.txt
- [n64-session-2026-09-29](n64-session-2026-09-29.md): N64 session 2026-09-29 (VA split with peer) -- what landed, what's parked, how coordination worked
- [n64-session-2026-10-03](n64-session-2026-10-03.md): N64 session (2026-10-03, range < 0x80230000 + 4 handed-off T1s) -- levers, tooling fixes, oracle artifact, what is pending
- [n64-session-2026-10-04-m2](n64-session-2026-10-04-m2.md): N64 M2 T3->T4 levers measured 2026-10-04 (statement-order search, int-0 vs 0.0f constant, copy form, pointer-local address webs, line join); parked rows with evidence
- [n64-t1-intake-2026-10-03](n64-t1-intake-2026-10-03.md): N64 session (2026-10-03) -- T1 intake >= 0x80230000 split with peer; what landed, the levers that paid, grader extension, coordination protocol
