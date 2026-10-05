# Tu oracle value vs tuned tus

*Recorded 2026-09-15.*

> Where the TU tie-break matters AND is understood, the tree is ALREADY hand-tuned (br_collresp.c documents the exact lever); the TU oracle's value is on SCATTERED-untuned TUs and completing cumulative state, both large ops. The cheap physics-TU lever proof does NOT exist.

**2026-09-15 (physics-TU lever-proof attempt, tu_049 0x100645A0):** ran the
`tumap.py --lane` picker into the physics TU to prove the lever. Finding that
reframes the oracle's value:

- **br_collresp.c is ALREADY the lever, hand-applied.** Its header (lines
  13-17) and the SegBox comment (0x10066800, ~798-805) document the EXACT TU
  tie-break: "re-spelling any of them renumbers the TU's symbols and can flip a
  float-chain choice in another: SegBox went match->diff with no edit to its
  text when its neighbours changed (fixed by its declaration order)." The file
  is deliberately arranged (NOT in VA order: file order is 10,6,5,8,7,9,...) to
  a 6-of-9-match local optimum. Reordering it to VA order risks the 6 matches.
- **So the oracle adds little on already-co-filed, tuned TUs.** Its value is
  (a) as a SCREEN/PICKER so scattered x87 rows are never opened alone, and
  (b) enabling a FIRST co-file of TUs whose members are scattered across many
  files and never co-tuned, or completing the CUMULATIVE state a tuned file
  cannot reach on its own.
- **Cumulative-state gap:** br_collresp.c holds only orders 5-10 of tu_049.
  Its rows never see orders 0-4 (0x100645A0/0x100651A0/0x100656F0/0x10065980/
  0x10065C80 -- ~7 KB, all T2, scattered across br_cardrive/br_carphys/
  br_tritest/br_collrespsolve). The two parked rows (PointInTri 0x10066610
  diff 245, BrCrExact 0x10066950 diff 165) resisted 150+ probes INCLUDING
  declaration-order; the untested lever is prepending orders 0-4 to complete
  the cumulative symbol count while preserving the tuned relative arrangement
  (prepend, don't reorder). BIG op: ~7 KB of unmatched functions from 4 files,
  incl. BrCarPhysTyre (~1250 src lines).
- **T1 holes block the low-diff targets:** BrCarPhysSpring 0x100684F0 (order
  18, diff 53, the clean `fxch`+`fmulp` vs `fmul`+`fstp` signature) can't be
  reconstructed -- orders 15,16 (0x10068070, 0x100682C0) are T1, no source.
- **Minimal clean proof (orders 0,1,2 -> test order 2 BrTriContainsPoint,
  diff 155):** avoids the tuned block and the T1 holes, but needs BrCarPhysTyre
  (1250 lines) relocated, and order 2's residue is memory-FOLD (fadd [esp] vs
  fld+fadd), a weaker match to the symbol-index mechanism than fxch.

**2026-09-15 scattered-TU redirect (checked, none landed):** examined the
accessible scattered x87 open rows for a clean co-file win. Result: they fall
into three buckets, NONE "one co-file from matching":
- CLEAN lever signature but T1-hole-blocked: BrCarPhysSpring 0x100684F0 (fxch+
  fmulp vs fmul+fstp, 5-insn gap) -- predecessors orders 15,16 are T1.
- ALREADY hand-tuned: the br_collresp block (tu_049 orders 5-10).
- STRUCTURAL / incomplete transcription (co-filing cannot help): BrCarGhostApply
  0x10059A80 (-18 insns, 17 missing `fld`, control-flow diffs); BrMtxInvert
  0x10034B70 (diff 571); tu_055 BrMat3Solve/BrRbQuatDerivative entangled with a
  d3d twin + tuned files.
Conclusion: the lever is real (oracle gate-2 100%, tree's own docs confirm the
mechanism) but the REMAINING open scattered rows are not lever-shaped -- the
clean tie-break wins were already taken by hand, and what's left is ordinary
transcription completion. The path that pays is T1 intake / finishing
incomplete T2 rows, NOT co-filing. Highest-value single action: transcribe the
two physics T1 holes (0x10068070 581B, 0x100682C0 387B) -- moves the count AND
unblocks the one clean lever row (BrCarPhysSpring) for a later proof.

**How to apply:** the oracle is a screen + intake router, not a mechanical
wall-breaker on tuned TUs. Before a physics-TU reconstruction, decide it is
worth ~7 KB of moves for two rows that already resisted heavy probing. Prefer
pointing the oracle at SCATTERED-untuned x87 TUs (never co-filed) or at T1
intake into TU-correct homes. Related: [tu-constant-pool-oracle-2026-09-15](tu-constant-pool-oracle-2026-09-15.md),
[x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md), [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md),
[file-position-regalloc-lever](../levers/file-position-regalloc-lever.md), [collresp-cluster-2026-09-05](../functions/collresp-cluster-2026-09-05.md).
