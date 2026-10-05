# Feedback do not lower t3 standard

*Recorded 2026-09-12.*

> RULE - never relax a T3 gate to hit a target number; asked whether to loosen A4, the answer was "don't lower our standards\

**RULE, 2026-09-10.** Asked directly whether Gate A4 should admit a
bounded scheduling window (rule 12 admits "allocation/scheduling" residue, yet
A4 demands identical ORDER, so it rejects what rule 12 says is certifiable),
the project lead answered: *"are you asking to lower our T3 standard? If so, don't
lower our standards."*

**Why:** the count is not the deliverable - a true MAME-standard decomp is.
A gate loosened to make today's number is a gate that no longer means anything,
and every function certified under it has to be re-audited later.

**How to apply:** when a row fails a gate, the move is to fix the SOURCE or
park the function, never to move the threshold. Concretely, on that day I:

- **declined** to raise A2's row cap (`max(4, 2.5%)`), which would have promoted
  four more rows - `0x10037FA0` and `0x10038000` sit at 6 rows against a limit
  of 4 with A3 fully clean, and they stay T2;
- **declined** a "global re-read vs hoisted CSE" class, because msetdiff masks
  relocs to `A` and so cannot prove the two sides read the SAME global  - 
  a genuinely omitted read would have paired;
- **declined** to pair `and R, 0x1f` with `movzx W, B` (0x100014A0), because
  their equivalence rests on a value range the multiset cannot see.

What IS allowed is a class for an **exact identity** with symmetric evidence:
`push K; pop R` *is* `mov R, K`; a duplicated epilogue *is* the same epilogue;
a byte-width spill whose dword reload is already an accepted singleton. Each
one carries its guard in the code comment and every existing `@t3` tag must
revalidate unchanged (`.venv/bin/python tools/t3.py`) after adding it.

**2026-09-12, project lead confirmed ("functionally equivalent is
functionally equivalent"):** the x87 STACK-DUP commutative fold (`fld st;
fmul [M]` == `fld [M]; fmul st(k)`) is on the exact-identity side of this
line - it is the memory-memory commutative fold the classifier already had,
with one operand on the fp stack. Landed in t3.py (commit 30d9dac), zero
demotions across all certified tags, promotes only 0x10067710. Do NOT cite
this rule to refuse an exact-identity class; it declines APPROXIMATIONS
(thresholds, ranges the multiset cannot see), not identities.

Distinguish this from fixing a gate that is measuring the WRONG THING - that is
not lowering the standard and is encouraged. Same day: A4's raw-byte resync
cannot anchor on a pure register transposition and reported 100% never-compared,
so it was given a positional order proof, which is *more* evidence than the 32-byte
tolerance it sits beside; Gate 0 anchored on the first line merely MENTIONING
`@implements` and was hijacked by a dossier quoting a twin's tag.

Related: [t3-certified-standard](t3-certified-standard.md), [byte-exact-non-negotiable](byte-exact-non-negotiable.md),
[goal-coverage-not-playability](goal-coverage-not-playability.md).
