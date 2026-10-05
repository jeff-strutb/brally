# Scenedl 0x1000eaf0 state

*Recorded 2026-09-21.*

> SUPERSEDED 2026-09-16 - 0x1000EAF0 BrSceneDlBuild is CERTIFIED T3 (br_scenedl.c). The 'one pass short of T3 / open walls 1-6' verdict is obsolete. Keep the reverse-engineering detail on the scene DL builder; the byte-Gate-A framing is dead.

** ONE PASS SHORT OF T3 (project rule 11b/12, 2026-09-06): `tools/t3.py
--qualify` gates 0 and A PASS (insn gap 3/11.6, rows 43/58.2, 0 unpaired, no
lost-sync, oracle UNCLASSIFIED); Gate B FAILS -- the `@t4-pass` ledger in the
file header has one counted zero-movement pass (31, 85 probes) and needs two
in a row. Next touch = ONE capped pass (>= 10 fresh compiles, dead list
grepped first), append its `@t4-pass` line, and if nothing moved paste the
tag the tool emits. Then it is parked.**

**0x1000EAF0 (BrSceneDlBuild), measured 2026-09-06 (pass 31).**
14 slot-masked divergence regions / 24 raw, 9,345 of 9,354 bytes (-9),
2,325 of 2,328 instructions (three short), register-blind multiset 23
missing / 20 extra. Unchanged from pass 30; nothing landed in the tree.
 Re-measure before quoting.

** THE FILE HEADER IS THE DOSSIER, not this note.** `src/core/drawing/
br_scenedl.c` carries 31 passes with ~130 measured-dead probe variants.
Re-running one is the most expensive mistake available on this function.

**Pass 31 (2026-09-06), one session, no workers: wall 4 measured to its
floor.** The `lea edx,[ecx*4]` + `[edx + sym]` form is unique in the whole
binary. VC5 forms the `idx*4` temp only when a live dword access with the
same index goes through a register or local-array base in a dominating block
(per arm), and such an access always leaves bytes the original lacks. Every
algebraic / alias / element-pointer / 2-D / inline-helper / dead-access
spelling folds. A function-scope `ring` assigned once at the TOP of the
wheel-loop body reproduces the original's if-arm addressing AND the pDst
spill (multiset 23 -> 15 missing) but VC5 then keeps ring in ebx across the
call and evicts iWheel, and the original provably recomputes ring per arm
(no ring store at the loop top; the else-arm recomputes too). VC5 does not
rematerialise. So the source has a per-arm ring plus an unknown trigger.
Full census: docs/VC5-IDIOMS.md "When VC5 keeps a scaled index in a
register". Harness: `tools/probe.py <variant.c> <tag>` (~6 s, every measure
at once); scratch-TU rule experiments compile in ~3 s via
`build/match/t3d/exp_*.c`.

**Open walls, header numbering:** wall 1 C4-hoist notch (0xd9a, 4 sites);
wall 3 wheel-record address form (0x1be7/0x1c1c); wall 4 (0x1dc7, -15 B,
the largest -- see above); wall 5 loop-entry join (0xad4) and tail fixup
(0x23ec); wall 6 slot packer (pDst/bSolo swapped 0x20/0x24, iWheel in
fMax's slot); drain section 0x1f66-0x2278 (T3a); else-arm `dec`/`jns`
(9a, three more spellings dead 2026-09-06).

Related: [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md), [resume-state](../log/resume-state.md),
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md), [diagnose-dont-hypothesize](../triage/diagnose-dont-hypothesize.md).
