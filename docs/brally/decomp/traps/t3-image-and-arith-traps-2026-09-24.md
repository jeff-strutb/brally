# T3 image and arith traps

*Recorded 2026-09-24.*

> 2026-09-24: four traps that made 'certified' T3s wrong. A generator picks float associations that change rounding; the image placed a different compile than A5 graded; reloc_pair overrode a DAT_ symbol by 4 bytes; A7 rows with 0 frames were counted IDENTICAL. All fixed; fix and how to check each.

Found and fixed while certifying these three T3s (all now pass every gate):
- 0x10022600 BrDlVtxGen
- 0x10021C70 BrDlVtxLit (new file br_dlvtx_lit.c)
- 0x10017110 BrEnvEmit

1. **Byte-score generators change arithmetic.** VC5 reorders float adds by operand KIND, not source order. A hill-climb picked `((x+z)+y)` and `(m1dy+m2dz)+m0dx`: 1-ulp divergences, not "scheduling".
   - Check with the scratch symbolic x87 evaluator (x87sym/x87cmp/x87diff pattern). It rebuilds each binary's float-store expression trees with rounding points.
   - Require IDENTICAL trees before T3.
   - `(double)pSrc->x` restores the original's order in the tu_022 vertex loaders.
2. **The image must place the compile that is graded.** image_build_t3 used the report's raw-diff winner (plain /O2) and ignored config/brally/t3_variant_c.csv (/O2 /Op). Fixed dd0f8f58.
3. **reloc_pair audit must never override a `DAT_<8hex>` symbol.** Pairing "learned" DAT_104add54 = 0x104add50, so every particle read was 4 bytes low (BrEnvEmit snow). Fixed f8177e97. Hand-coined names keep the override.
4. **A7 rows must have real frame counts.** A worktree without build/brally/win32/brbox/cd plays 0 frames. Bootstrap = symlink build/brally/win32/brbox/cd, copy build/brally/win32/brbox/saves, check boot plays 300 frames.
   - A peer's fix b71d9162 makes 0 frames fail.
   - Also regenerate ALL C++ sweep objects in a fresh worktree; stale objects block the image. Compile the /Gi rows serially.

**Diagnosis recipe:** brbox_diff --localize for the first bad dword, then trace writes/args at that frame in both runs (orig DLL vs T3 image) with unicorn hooks. Compare the placed body's instructions with the graded object. That localised all three bugs in minutes.

Coordination: ONE A7 run at a time, owned by one session. Everyone else waits for its frame counts.
Related: [a5-a7-coverage-gap](../oracle/a5-a7-coverage-gap.md), [dlvtx-texgen-t2-2026-09-24](../functions/dlvtx-texgen-t2-2026-09-24.md), feedback-parallelize-long-runs.
