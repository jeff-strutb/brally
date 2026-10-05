# Cpp lane t3 filing workflow

*Recorded 2026-09-16.*

> 2026-09-15: the exact end-to-end steps + gotchas to file a C++-lane function and get t3.py --qualify to emit an @t3 tag (behavioural cert), proven on BrRaceStep 0x10019A70.

**End-to-end to certify a reconstructed C++-lane function as T3 (proven on
BrRaceStep 0x10019A70, bracestep-wall, [oracle-runs-orchestrators](../oracle/oracle-runs-orchestrators.md)).**

1. **File** at `src/core/cpp/0x<VA>.cpp`. Header order, top-of-file:
   `/* WHAT IT DOES: ... */` then (later) `/* @t3 ... */` then
   `/* @implements 0x<VA> glide <Name>` + ` * @cpp_symbol _<Name>`. In the C++
   lane @implements is the SWEEP ANCHOR and coexists with @t3 + not-byte-exact
   (rule 2's "@implements = diff clean" is the C-lane reading; the cpp lane uses
   it to mark which VA the file implements). self-contained externs are fine
   (opaque `struct Obj`/`Driver`, ~200 extern decls) -- cpp_score compiles it
   standalone with /GX.
2. **Sweep** to get a report row: `tools/cpp_sweep.py src/core/cpp/0x<VA>.cpp`
   -> writes build/match/report_cpp.csv. It picks the min-diff VARIANT; for a
   FRAMELESS original (`sub esp,N`) that is often O2y (ebp-frame), a sweep-variant
   artifact that INFLATES rows -- fine, because A5 supersedes byte-shape, but note
   it in the tag ([sweep-variant-selection-artifact](../traps/sweep-variant-selection-artifact.md)).
3. **Qualify**: `tools/t3.py --qualify 0x<VA>`.
   - Gate 0 (WHAT IT DOES) passes if the comment is present.
   - Gate A: A5 must run + return EQUIVALENT/EQUIV-MODULO-FP (see
     [oracle-runs-orchestrators](../oracle/oracle-runs-orchestrators.md) for making it run) -> supersedes A1-A4.
   - Gate B (@t4-pass ledger): needs >=2 lines with `probes >=10`, one
     `census yes`, and the last two AT THE CURRENT measured numbers
     (bytes/insns/regions/rows). Write them honestly from the real T4 grind.
4. **Paste** the emitted `@t3 ...` block above the @implements line; fill the
   residue placeholder.
5. **Update counts/README/SVG**: `tools/progressbar.py` (regens README block +
   docs/progress-map.svg from tiers.py). Commit with pathspecs, NO attribution.

** GOTCHAS that cost time:**
- **Stale sweep objs pollute the oracle.** `_obj_index` scans obj_O2/O2y/O2p/Od
  (+obj_cpp now) and setdefault-picks the first symbol match. A leftover
  stale experiment objects (a prior experiment) in those dirs shadowed BrRaceStep with an
  oversized copy -> "substituted bytes bury <neighbour>". Delete stray objs:
  delete them. Any stale obj with your symbol
  does this.
- **The /Od sweep variant is huge** and buries neighbours; the frame-correct
  ~size variant is the one to run. Removing the stale obj let setdefault pick a
  good one.
- **parse_signature** needs `git grep --untracked` (untracked new file) and must
  strip `extern "C"` -- both fixed in t3b_verify.
- **tiers.py undercounted C++-lane T3**: it only counted report_cpp `match`
  (T4) rows; a T3-certified pure-cpp diff row was in no tier and reappeared as
  T1. Fixed to add cpp diff rows to the diff list. If T3 count != certified
  count, this class is why.
- **fileaudit.py** must stay clean (undescribed 0, wrong-module 0, stranded 0)
  or the pre-commit hook refuses.
