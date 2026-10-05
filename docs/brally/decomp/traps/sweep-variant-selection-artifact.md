# Sweep variant selection artifact

*Recorded 2026-09-15.*

> match_sweep picks the RAW-BYTE-min variant, so a frameless original can be recorded on a dead-end /Oy- (ebp-frame) or /Op-bloated variant that is register-blind-worse and can NEVER byte-match -- this corrupts t3.py's A1/A2/A3/A4 and fakes 'missing code' signals. Check the variant before trusting any gate on a large x87 function.

**Found 2026-09-15 re-triaging the five-largest.** `tools/match_sweep.py`
ranks variants by `key = (match?, raw_byte_diffs, |size gap|)` (score +
best-selection, ~line 260/325). VARIANTS include `O2y = /O2 /Oy-` (frame
pointer KEPT, ebp frame) and `O2p = /O2 /Op`. For a FRAMELESS original
(`sub esp,N` prologue, no `push ebp`), an ebp-frame variant can have FEWER raw
byte diffs yet:
1. can NEVER byte-match (its `push ebp; mov ebp,esp` prologue vs the original's
   `sub esp,N` is unbridgeable), and
2. is register-blind-WORSE (its `[ebp-X]` locals and extra frame ops inflate
   the msetdiff), so t3.py's A1/A2/A3/A4 -- which read report.csv's recorded
   `opt` object -- are all measured on the wrong variant.

**Measured on the five-largest (register-blind rows, frame-correct O2 vs the
recorded variant):**
- 0x10032E40 cartrail: recorded O2y (ebp, 248 rows, "gap 16"); real O2 is
  frameless, 70 rows, **insn gap 3** -- NOT missing code, x87 fxch scheduling.
- 0x1000CBA0 objdl: recorded O2y (ebp, 425 rows); real O2 176 rows, gap 44.
- 0x10068900 obb: recorded O2p (bloated 2000 B, 358 rows); real O2 215 rows,
  gap 8, x87 operand-role scheduling.
- 0x10001CF0 chasestep: recorded O2 (correct); its A4 "357 B never compared" is
  a divergence.py RESYNC artifact (dense fxch), not an absent block -- whole-fn
  msetdiff only 43 rows, +6 insns.

** PRACTICE:** before trusting any gate verdict on a large x87 function, check
the original's prologue (`sub esp` = frameless) against the recorded variant's.
If the recorded variant is /Oy- (push ebp) on a frameless original, or /Op when
plain O2 is register-blind-closer, RE-MEASURE against O2 with
`tools/msetdiff.py build/match/orig/<VA>.bin build/match/obj_O2/<file>.obj
<sym>`. An "A1/A4 fail = missing code" signal on such a function is very likely
a variant artifact, not transcription work. Only 0x1000CBA0 objdl has genuine
SEMANTIC residue under O2 (shr 8 vs shr 0x10, missing and 0x1f, indexed byte
RMW); the rest are x87 scheduling walls.

**NOT YET FIXED (project lead's call -- broad, needs a full re-sweep):** make the
sweep frame-aware -- disqualify or deprioritise a variant whose prologue class
differs from the original's, OR add a register-blind tiebreak so the T3 gate
measures the register-blind-best variant. Either reshuffles report.csv
tree-wide. Related: [five-largest-2026-09-13b](../log/five-largest-2026-09-13b.md), [lost-sync-region-trap](lost-sync-region-trap.md),
[divergence-class-triage](../triage/divergence-class-triage.md), [byte-exact-non-negotiable](../rules/byte-exact-non-negotiable.md).
