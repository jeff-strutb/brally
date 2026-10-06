# Repo open leads

*Recorded 2026-09-24.*

> The former repo docs/the notes index (open leads as of 2026-09-24), moved out of the repo 2026-09-29

Verbatim copy of docs/the notes index, deleted from the repo on 2026-09-29. Dated; re-verify every lead against the tree. Related: project-rules.

## Index

Query the tree for coverage (`tools/brally/tiers.py`, `tools/brally/total.py`). Do not trust
a number in prose. Procedure: `docs/brally/MATCHING.md`. Idioms: `tools/brally/corpus.py`.

## Open leads

### T3 verification: A5 + A7 (2026-09-24)

The T3 image (`BRGlide.T3.dll`) behaves identically to the original on all 25
brbox scripts (`tools/brally/brbox_diff.py --all`, `config/brally/whole_image.csv`) and runs
the retail game in the Win98/86Box VM. The whole-image run found ~25 bugs the
per-function oracle had certified; `t3.py --qualify` now requires both (A5 and
A7). Still open: 6 T3 functions no script reaches (UNCOVERED in
`config/brally/t3_live.csv`), and the port's `#else` arms of the fixed functions may
carry the same semantic bugs. The six T3 bodies rewritten on 2026-09-24
(BrRaceStep, BrCtlInputApply, BrCarPhysDriveMatch, BrCrImpulseSolve,
BrCarCarCollide, BrGhostPlaybackStep) need fresh `@t4-pass` lines for Gate B;
BrGhostPlaybackStep (regnorm 6+8) and BrCarCarCollide (same size) are close to T4.

### Port build is broken (drift, 2026-09-21)

`./build.sh` stops with one compile error: `src/brally/core/controls/br_inputpoll.c:203`
declares `__declspec(dllimport) short __stdcall GetAsyncKeyState(int)` with no
port guard, and clang rejects `__declspec`. Guard the Win32 declaration behind
`BR_MATCHING_BUILD` (or `_WIN32`). This is fresh drift from a recently-matched
function - the old `slice2_12.c BrFixPackS16Q15Neg` blocker is resolved. Last
fully-green suite was 136/136 on 2026-08-27; the port has drifted since as
signatures were rematched. Not a matching reorder - keep the port buildable.

### Colouring tail → T3, not a grind

~5 T2 rows have `reggap 0` (same instructions, registers differ). Proven
unreachable from source (permuter 0/95, refine 0/258, crank 4/536). Qualify
with `tools/brally/t3.py --qualify`; park until the end-grind.

### Giants - all three certified T3 (do not reopen)

`0x10019A70` BrRaceStep, `0x1000EAF0` BrSceneDlBuild, `0x100250D0`
BrTex3dExpand are all certified T3. The largest still-open bodies are now
`0x10056260` (8,349 B, `br_uiimg.c`) and `0x10051600` (4,109 B, C++ lane)  - 
named-only, per project rule 11.
