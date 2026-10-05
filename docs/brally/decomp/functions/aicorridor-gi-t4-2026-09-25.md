# Aicorridor gi t4

*Recorded 2026-09-25.*

> 2026-09-25: 0x1005D060 BrAiScanCorridor T1 -> T4 as a C++ /Gi TU (src/core/driving/BrAiCorridor_1005D060.cpp). /Gi join-reload order depends on source+obj PATH LENGTH and idb chain; verify via the serial O2-Gi chain, never the root vc50.idb.

0x1005D060 (856 B, recursive thiscall) went T1 -> T4, commit b02fa277.

**Source levers:** inline `operator=` on the Vec type (reg+disp component
copy), `&centre[depth]` in one pointer, pointer-walk clearance loop, single
-return if/else tail with `++mid`, depth store AFTER the three memcpys (before
them = whole-block eax/ecx/edx rotation).

**The /Gi trap:** the last 4-16 bytes were the order of four reloads at a
join. Under /Gi that order follows the compiler's heap layout, which is set by
the source path length, the /Fo object path length and the idb contents.
Names, decl order and preamble size were all inert at the real path. Fixed by
choosing the file name length (12 chars before `_1005D060`), measured through
the faithful chain: "O2 Gi" rows in (file, va) order, one fresh private idb,
sweep-named objs (`<base>_sweep_<VA>_1.obj`), predecessors into a same-length
private obj dir. The ledger row was written with cpp_sweep.score_one +
merge_report using that chain (scratch script gisweep_row2.py pattern).

**Why:** the shared root vc50.idb gives C1073 for every /Gi compile (parallel
/Gi runs corrupt it; it was broken before this session). A /Gi row's match is
only reproducible through the serial chain, like 0x10054730
([gi-serial-idb-a7-recipe](../oracle/gi-serial-idb-a7-recipe.md)).
**How to apply:** renaming this file, or adding an "O2 Gi" TU that sorts
before src/core/driving/, can flip it -- re-run the chain. BrRaceStep's /Gi
recompile reads 6,765 diffs vs 6,752 in the ledger with or without this TU
(pre-existing drift, not caused here).
Related: [encodedelta-vc5-t4-2026-09-25](../levers/encodedelta-vc5-t4-2026-09-25.md) (the non-/Gi heap tie-break).
