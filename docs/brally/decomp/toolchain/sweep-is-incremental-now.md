# Sweep is incremental now

*Recorded 2026-08-21.*

> match_sweep.py now writes back on every run and caches by content hash - full sweeps are no longer needed to re-baseline.

**Fixed 2026-08-20, after the project lead pushed back on repeated 20-minute sweeps.**
They were right: the full sweep was bookkeeping, and re-running it was
self-inflicted.

**THE WORK LOOP DOES NOT NEED A SWEEP.** To make one function bit-exact you need
its own file compiled and that function objdiff'd - nothing more. A single-file
sweep is ~12s measured. The whole-tree run only ever produced an accurate total.
Do not full-sweep to pick or verify a function.

**Three changes to `tools/match_sweep.py`:**
1. **Every run merges its rows into report.csv**, single-file runs included.
   Previously only an argument-less run wrote the report, so each incremental
   match made it staler until a full sweep was the only cure - that is exactly
   how the 120-vs-152 drift happened. Merge replaces the row set PER FILE, so a
   deleted/renamed/moved function drops out instead of lingering.
2. **Results cached** in `build/match/sweep_cache.json`, keyed by the file's
   own bytes plus a digest of everything under `include/` and
   `tools/msvc5-compat`. A cache hit is refused unless the variant `.obj` files
   still exist, because objdiff.py is pointed at them right afterwards.
   Measured: 11.7s cold -> 0.08s warm; full sweep 20min -> 0.076s.
   ANY header edit invalidates the WHOLE cache by design.
3. **Unknown flags are rejected.** They used to be filtered out, leaving zero
   args, which silently launched the 20-minute run - the `--help` footgun from
   [match-tooling-gotchas](../traps/match-tooling-gotchas.md) item 4. `--force`/`--no-cache` bypasses the cache.

**Verified end to end, not assumed:** merge is lossless (811 rows before and
after, content identical when sorted); a cached full sweep reproduces 152/810
exactly; and a deliberately edited file correctly MISSES the cache and
recompiles (the negative test - a cache that failed to invalidate would
silently recreate the staleness bug).

Related: [report-csv-per-file-staleness](../traps/report-csv-per-file-staleness.md), [matching-progress](../log/matching-progress.md),
[match-tooling-gotchas](../traps/match-tooling-gotchas.md).
