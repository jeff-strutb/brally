# Trampoline goto t4 lever

*Recorded 2026-09-22.*

> MSVC5 /Od forward-goto trampoline walls crack to BYTE-EXACT T4 via an explicit trailer trampoline label

The "forward `goto` compiles to a direct short `eb`, but the original routes it
through a near `e9` + a 2-byte trampoline parked before the epilogue" residue is
NOT a branch-layout wall - it is source-reachable and cracks to byte-exact T4.

**Mechanism:** MSVC 5.0 /Od does NOT thread jump-to-jump. So don't try to stop
the compiler shortening the direct goto; instead route the goto through an
EXPLICIT trailer trampoline reached ONLY by that goto. Redirect
`goto wrap_plain` → `goto wrap_tramp`, and just before the closing brace add:

```c
    return;
wrap_tramp:
    goto wrap_plain;
```

The compiler emits `jmp near wrap_tramp` + `wrap_tramp: jmp short wrap_plain`,
exactly matching the original trampoline. The `return;` reconstructs the shared
return thunk the original jumps over. Verified byte-exact on **BrAnimUpdate
0x1002ECEB** (2026-09-21, another session's crack; I confirmed DIFFS=0 at
/Od /Op). Grade /Od-only functions with `FN_OPTS='/Od /Op' fn.py <VA>`.

The earlier dossier's huge dead-probe battery all tried to make the compiler
emit the trampoline from ordinary control flow (goto into sibling else, nested
block, loop body, flags /Z7 /Gy…) - every one inert. The lever is the OPPOSITE:
add a real extra label+goto so the jmp→jmp is in the source.

**Scope (narrow, not a class sweep):** /Od-specific - /O2 threads jmp→jmp and
reorders branches, so the near-thunk pattern never arises there. It only helps
when the forward-goto thunk is the SOLE residue. The peer scanned the diff set:
the other flagged instance 0x1002E376 is already a match, and the other
"few-bytes-short" functions have unrelated residues (e.g. BrWeatherStepWind
0x10016AA0 is x87 fxch/fst scheduling, not a trampoline). So this unlocked the
one function, not a family.

**Process lesson for me:** I mis-certified this as a T3 "colouring wall" and
even added a trampoline-classifier fold to `tools/brally/t3.py` to admit the residue  - 
lowering the gate for a wall that was actually crackable. When a residue is a
small PURELY branch-encoding delta, exhaust source levers (including adding
structure, not just removing it) before certifying T3 or touching gate tooling.
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [byte-exact-non-negotiable](../rules/byte-exact-non-negotiable.md),
[walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md), [cross-jump-wall-final-return-order](cross-jump-wall-final-return-order.md).
