# File as you match

*Recorded 2026-08-20.*

> Move a function from its slice into the right named module when you match it - never a bulk reorg.

`sliceN_MM.c` files are **analysis batches**, not architecture - they group
functions by the address range a reverse-engineering pass happened to cover.
The named modules under `src/core/` (audio, controls, drawing, driving,
gamedata, geometry, menus, racing, scene, settings, startup) are the real
destination. As of 2026-08-19: 77 files filed, ~65 slices still unfiled.

Project lead confirmed the working rule: **file a function into its proper module at
the moment you match it. Do not do a big-bang reorg.**

**Why:** you are already touching the file, so the diff stays small and
reviewable, and it doesn't force the unresolved global-ownership question
before someone is ready to own it. README documents that slice modules are
never linked together today (each gets its own test binary), so duplicate
global definitions across slices are currently invisible - some addresses
have 10-35 users DLL-wide. A bulk move detonates that early.

**How to apply:** when a function goes byte-identical, check what it depends
on before moving it. A leaf function with no global state moves freely. One
that reads a global defined in its slice drags an ownership decision with it
 -  the README's prescribed fix is promoting shared globals into a single
owning TU with externs elsewhere, NOT renaming per-module (the aliasing is
load-bearing). If the move would require that, match first and leave the
function in place rather than blocking the match on it.

Note the end goal wants source files matching the ORIGINAL translation units
for link order; "architectural concern" is a different question and the two
can conflict. Filing incrementally keeps that reversible.

Related: [matching-progress](../log/matching-progress.md), [goal-coverage-not-playability](goal-coverage-not-playability.md),
no-token-thrashing.
