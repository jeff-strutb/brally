# Callee saved zero web class

*Recorded 2026-09-10.*

> A recurring colouring wall: which callee-saved register (esi vs edi) takes the shared ZERO constant versus the first call result. Present in BrTexInit, BrSndNearestCommit and BrCtlAiBody R1/R2; 25+ probes dead, corpus MISS. Do not re-probe it per function.

The same wall shows up in at least three independent functions, and it is
worth recognising on sight instead of re-deriving:

    orig    xor esi,esi ; push esi ; call ; push esi ; mov edi,eax ; call
    ours    xor edi,edi ; push edi ; call ; push edi ; mov esi,eax ; call

Two webs -- a shared zero constant and the first call's result -- take the two
callee-saved registers the other way round. Everything else is byte-identical:
same size, same instruction count, register-blind multiset 0+0, positionally
identical.

**Sites:** 0x10029B50 BrTexInit (at 0x89), 0x10060F40 BrSndNearestCommit (at
the prologue, `push esi` then `xor esi,esi` with `push edi` DEFERRED past the
first compare), 0x1005D770 BrCtlAiBody R1/R2 (its dossier calls it "which
constant wins the callee-saved register"; the negative arm of rule 7 pins
ebx=0 and is byte-identical, the positive arm sinks the xor to its first
arithmetic use).

**PROBED DEAD, 2026-09-10, do not re-run per function** (25 compiles on
BrTexInit alone): literal vs named zero at the call sites; the zero as a
`const int`, an `unsigned`, a function-scope local, and a block-scope local;
hi/lo result temps in both orders and split from the expression; the two calls
as one expression and as two statements; declaration order of every local
including the array; the surrounding lifetimes (the TMU-count block, the
free block's store order, the tail's store order, span's type). Nothing
moves the assignment. The same axis was probed dead on BrSndNearestCommit
(declaration order, named zero) and, in its own spellings, on BrRankAssign's
four-register cyclic shift (23 compiles, 2026-09-10).

**The corpus is a MISS** on the 12-instruction run at BrTexInit 0x89 -- no run
of 3+ of these instructions is proven anywhere in the solved tree, so there is
no spelling to copy. That MISS is what makes these legitimate `census yes`
Gate B passes.

So: when a function's residue is exactly this, do NOT spend a session on it.
Run one honest counted pass, write the ledger, and certify at T3.

Related: [gate-a-distance-survey-2026-09-10](../log/gate-a-distance-survey-2026-09-10.md), [declaration-order-tiebreak](declaration-order-tiebreak.md),
[register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md), [parked-is-not-walled](../triage/parked-is-not-walled.md),
[t3-certified-standard](../rules/t3-certified-standard.md).
