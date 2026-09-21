# Memory index

Query the tree for coverage (`tools/tiers.py`, `tools/total.py`). Do not trust
a number in prose. Procedure: `docs/MATCHING.md`. Idioms: `tools/corpus.py`.

## Open leads

### Port build is broken (drift, 2026-09-21)

`./build.sh` stops with one compile error: `src/core/controls/br_inputpoll.c:203`
declares `__declspec(dllimport) short __stdcall GetAsyncKeyState(int)` with no
port guard, and clang rejects `__declspec`. Guard the Win32 declaration behind
`BR_MATCHING_BUILD` (or `_WIN32`). This is fresh drift from a recently-matched
function — the old `slice2_12.c BrFixPackS16Q15Neg` blocker is resolved. Last
fully-green suite was 136/136 on 2026-08-27; the port has drifted since as
signatures were rematched. Not a matching reorder — keep the port buildable.

### Colouring tail → T3, not a grind

~5 T2 rows have `reggap 0` (same instructions, registers differ). Proven
unreachable from source (permuter 0/95, refine 0/258, crank 4/536). Qualify
with `tools/t3.py --qualify`; park until the end-grind.

### Giants — all three certified T3 (do not reopen)

`0x10019A70` BrRaceStep, `0x1000EAF0` BrSceneDlBuild, `0x100250D0`
BrTex3dExpand are all certified T3. The largest still-open bodies are now
`0x10056260` (8,349 B, `br_uiimg.c`) and `0x10051600` (4,109 B, C++ lane) —
named-only, per CLAUDE.md rule 11.
