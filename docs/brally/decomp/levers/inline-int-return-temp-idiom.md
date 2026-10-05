# Inline int return temp idiom

*Recorded 2026-09-24.*

> 2026-09-24: 0x100719D0 BrInputJustPressed (1246 B, largest open C row) went 928 B/reggap 477 -> BYTE-EXACT in ~10 probes by hand transcription from asm; lever = __inline int helper with `return 1; return 0;`

0x100719D0 BrInputJustPressed was a d3d-slice twin (slice3_45.c, graded via
shared.csv) diffing 928/1246 B. Hand-transcribed from the Glide asm into its
real module src/core/controls/br_inputpoll.c (after BrInputPoll) as a
`#ifdef BR_MATCHING_BUILD` arm; slice body demoted to "port-only body" (same
pattern as BrDiAcquire). Commit 4f95ae10. First straight transcription was
already 1243/1246, regnorm 6+5.

**The lever (now on the VC5-IDIOMS.md tail):** a byte result that is
materialised full-width (`mov eax,1`/`xor eax,eax`, shared cross-jumped exit)
in some arms while other writes to the same variable are byte ops (`mov al,bl`,
`or al,cl`) = a `static __inline int f` whose body is `if (c) return 1;
return 0;`. `&&`, `?:`, if/else, single-`return expr` inline, C vs C++ (/TP)
all canonicalise to `mov al,1`; `int r` fixes those arms but widens the rest.
Prologue: declaring `r = 0` before the pointer initialiser.

**Why:** the prior automated/d3d-shaped attempt looked like a 477-reggap
structural wall; it was one inlined helper.
**How to apply:** when arms of one function disagree on the width of the same
variable's writes, suspect an inlined int-returning helper with multi-return
body before anything else. See [d3d-twin-glide-transcription](d3d-twin-glide-transcription.md),
[inlined-helper-match-class](../triage/inlined-helper-match-class.md), [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md).

**Clean A7 rerun from a worktree (needed when committing into a file that
also holds a T3 function, since A7 goes stale on that file's commit):** see
[worktree-bootstrap](../toolchain/worktree-bootstrap.md) plus: tools/msvc5 and tools/msvc6 have tracked children
(link bin/include/lib individually); use LOCAL empty obj_* dirs and COPIES of
report*.csv (cpp_sweep rewrites report_cpp.csv); cpp_sweep in parallel drops
`/Gi` variants (vc50.idb collision) -> compile each report_cpp row's own opt
serially with cpp_score.compile_cpp(file, 'sweep_%08X_%d', opt); HEAD's
image_build_t3.py needed collect_dll(jobs=) from an uncommitted image_build.py
(2026-09-24). Then copy build/image/BRGlide.T3.dll to build/brbox/image/.
