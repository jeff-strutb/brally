# Feedback verify blockers through twins

*Recorded 2026-08-19.*

> Blocker lists go stale. Always check shared.csv d3d twins when verifying if a callee is ported -- glide-address-only checks miss d3d-tagged implementations.

**Always resolve callees through shared.csv before declaring them unported.**
A grep for `@implements 0x<glide_addr>` misses functions tagged with their d3d
twin address. Four of nine frontier functions had wrong blockers because the
scanner only checked glide addresses.

**Why:** Session 2026-08-19 re-scanned the frontier and found that
0x100119C0, 0x10014E00, 0x1006EC30, 0x1002C50E, and 0x1002CB49 were all
reported as "still blocked" by an earlier scan. When the check was corrected
to look up d3d twins via shared.csv, three of those turned out to be genuinely
leaf-complete. The stale blocker list had been copied forward across sessions
without re-verification.

**How to apply:** when checking if address X is ported:
1. `grep @implements X` in port/src/ (direct glide tag)
2. Look up X in shared.csv column 2 (glide) to find the d3d twin in column 1
3. `grep @implements <d3d_twin>` in port/src/
4. Check config/brally/ported.csv for both addresses
5. If none found, it's genuinely unported

Never trust a blocker list from a previous session without re-running this
check. Callees get ported under d3d addresses by other work streams.

Related: [readme-status-was-stale](../traps/readme-status-was-stale.md), [render-frontier-draining](../../port/render-frontier-draining.md).
