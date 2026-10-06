# Oracle diff revert profile first

*Recorded 2026-09-21.*

> A false A5 DIFF is usually a half-reverted profile edit - fully revert oracle_profiles.py + the .c before trusting any t3b_verify verdict.

Chasing a false A5 `DIFF` cost an entire session on 0x10028BB0 BrTex3dRegister
(2026-09-21). The committed profile actually gave **EQUIVALENT**; every "DIFF at
seed 9, memory at 0x1186C988" came from my own un-reverted intermediate edits to
`tools/brally/oracle_profiles.py` (a p2-seeding change, stub_calls experiments) still on
disk when I re-ran `t3b_verify`. A partially-reverted world reruns as a
DIFFERENT world on each side and manufactures a spurious DIFF.

**Why:** t3b_verify reads the LIVE `oracle_profiles.py` (and the built .obj from
the last `match_sweep`). Any dirty edit - even one you think you reverted  - 
changes the seeded world.

**How to apply:** before trusting ANY t3b_verify / t3.py --qualify verdict:
`git status --short tools/brally/oracle_profiles.py src/brally/.../<file>.c` must be clean (or
show only edits you intend). If a verdict surprises you, `git checkout --` the
profile and the .c, re-run `match_sweep`, and re-verify BEFORE instrumenting.
A surprising DIFF is a reason to re-check the tree, not to start a deep bug hunt.

Corollary caught the same day: the once-seen seed-9 split was a real *oracle*
thunk-vs-direct import modelling gap (original reaches a Glide/CRT import via
`call thunk; jmp [IAT]`, the recomp via a direct `call [IAT]`), which a parallel
session fixed in x87emu. When only ONE seed of a multi-LOD/edge config diffs at a
transient global, suspect an import-modelling gap or a dirty profile before the
transcription. Reinforces [objdl-a5-limit-2026-09-16](objdl-a5-limit-2026-09-16.md), feedback-outcomes-not-excuses
(the project lead was right: "dead end" was an excuse - the cause was findable).
