# A5 a7 coverage gap

*Recorded 2026-09-24.*

> A5/A7 only prove the paths the 25 scripts execute. 0x10022600's light-refresh block (108/362 insns) never runs in any script, and the game's one light has x=y=z direction, so component swaps are invisible. Measure instruction coverage, then force real-state branches before trusting EQUIVALENT.

A5 EQUIVALENT and A7 IDENTICAL cover only the instructions the driven scripts reach.

Found 2026-09-24 on 0x10022600 BrDlVtxGen (certified T3):
- Scripts executed 253 of the 362 instructions.
- The whole light-cache refresh block never ran: the cache is always valid on entry.
- The game's only light is dir (0x54,0x54,0x54), col (ff,ff,cc). A dx/dy/dz swap passes every run. A planted swap passed the forced-refresh oracle until the direction was made asymmetric.

**Why:** the project lead asked "truly functionally identical?". The honest answer needed coverage evidence, not just the verdicts.

**How to apply** (scratch cov.py / forcelight.py pattern):
1. Get per-instruction coverage: a UC_HOOK_CODE over the function range, all 25 scripts in parallel.
2. For every block never executed, monkeypatch `brbox_drive.make_box` to add an entry hook, registered BEFORE t3live's, that sets the gating global. Run `t3live.run_script(..., only=[va], image=None)`, so both bodies run from real state that takes the branch.
3. Make degenerate data asymmetric (equal xyz, zero counts).
4. Always negative-control: plant a bug, confirm DIVERGENT, restore, re-sweep.

Result for 0x10022600: forced refresh 64/64 SAME; asymmetric light 64/64 SAME (a planted swap gave DIVERGENT); nLights=0 25/25 SAME. The only unexecuted path left is NULL matrix (top==0), which faults in both.
Related: [dlvtx-texgen-t2-2026-09-24](../functions/dlvtx-texgen-t2-2026-09-24.md), [t3-certified-standard](../rules/t3-certified-standard.md), [whole-image-oracle-2026-09-24](whole-image-oracle-2026-09-24.md).
