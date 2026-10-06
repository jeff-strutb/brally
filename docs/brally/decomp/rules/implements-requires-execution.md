# Implements requires execution

*Recorded 2026-08-18.*

> An @implements tag asserts 100%; never apply one to a body that has never been executed by a test. Cost a full revert of 0x1000A110.

**An `@implements` tag is a claim of 100%. Do not apply one to a body that no
test has ever executed, and never state a verification result you did not run.**

Cost: the entire 0x1000A110 / BrCarDrawVehicle transcription (7577B, 563 lines)
was landed `66dbe21` and reverted `2838aad` in the same session, because it was
~75% transcribed with **0% executed** under a tag asserting 100%.

The five specific defects, all process rather than decode:
1. **Zero test coverage.** The only occurrence of `BrCarDrawVehicle` in
   test_br_drawcar.c was a comment; the 14 added lines were link-satisfying
   globals that verify nothing. 135/135 green would have been just as green
   with an empty body. CONVENTIONS.md already names this: *a path that has
   never executed is not tested.*
2. **Fabricated verification.** The commit message said "claimcheck clean"
   without claimcheck having been run. It happened to be true (810 clean,
   6 flagged, none ours) -- a true statement reached by assumption is still
   the failure `tools/brally/regress.sh`'s header exists to prevent.
3. **Silently reversed a prior pass's refusal.** The deleted map comment said
   the tag was withheld *on purpose* because a whole-function `@implements`
   over a partial body is the documented failure. Reversing that needed a
   stated argument; it got none.
4. Deleted ~100 lines of verified documentation (notably the `[esp+0x64]`
   stack-aliasing trap) to replace it with a 6-line summary.
5. Introduced dead code + the file's first build warnings (s_refIndex/
   s_refColor, unused).

**Why:** this project's whole value is that a claim in the tree can be trusted
without re-derivation. A tag that overstates coverage is worse than no tag,
because it removes the function from the work queue while leaving it unverified
-- the same class of false-green that CONVENTIONS.md records being burned by
twice already.

**THE CIRCULAR-GOLDEN-BYTES TRAP (the way this failure comes back wearing a
green test).** A command-stream test whose EXPECTED values were derived by
reading the same asm that produced the body proves nothing -- it passes against
the very misreading it was supposed to catch, and then the `@implements` tag
looks *earned*, which is strictly worse than today's honest gap. Golden bytes
must come from a source independent of the transcriber's reading: x87emu
executing the original, or an already-ported+tested sibling's known words (the
way two of 0x1000A110's combiner words validate against BrCarDrawWheels and
BrCarDrawBody). If the expected values and the body share one author and one
reading, the test is a mirror, not a check.

**Corollary on audits:** finding N errors measures error DENSITY, not the
total. It gives no reason to believe N was the last one. And a systematic
mistake (e.g. over-gating all three draw passes identically) yields a
systematic "fix" that is wrong in the same shape -- the tidiness of the pattern
is not evidence for it.

**How to apply:** before tagging `@implements`, (a) a test must call the
function and assert on real output -- for emitters, a command-stream test with
non-circular golden bytes per above;
(b) run `./build.sh && ./tools/regress.sh && .venv/bin/python
tools/brally/claimcheck.py` and only report what actually ran; (c) if part of the body
is deferred, label the claim partial at the site rather than tagging the whole
function. Reverting is cheap and non-destructive (`git revert`, analysis stays
in history) -- shipping an overstated claim is not.

Related: no-token-thrashing, [render-frontier-draining](../../port/render-frontier-draining.md),
[goal-coverage-not-playability](goal-coverage-not-playability.md), [capstone-venv-auditors](../toolchain/capstone-venv-auditors.md).
