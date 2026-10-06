# BrCarDrawVehicle (0x1000A110) structural levers - proven 2026-09-01

Five source-shape → codegen idioms proven byte-closer on this function.
All verified via `fnmatch/fn.py --var` in private copies, then landed on the
live file. REGNORM multiset gap 46+56 → 35+46 across the set.

## 1. Fixed-address global array vs pointer variable
When the orig folds a global's link address as a MEMORY DISPLACEMENT
(`movsx ecx, byte [ecx + edx*2 + 0x100A5C78]`, `mov edx, [ecx*4 + 0x100A5C58]`),
that symbol is the ARRAY DATA at a pinned address, NOT a pointer variable to be
loaded first. A pointer decl emits an extra `mov reg,[addr]` load. Reproduce the
fold with `((const T *)&g_sym)[idx]` under BR_MATCHING_BUILD (the pinned pointer
var's ADDRESS is the array base). Clean-but-unverified alternative: redeclare the
global as `extern const T g_sym[];` in the header (couldn't verify in the
single-file harness: C2372 vs the pointer decl). NOTE this also exposed a real
INDEX-SCALE BUG - always re-derive which operand carries the `*2`/`*4` from the
SIB byte, don't trust the port's existing spelling.

## 2. Stub function call vs direct global read
The port routed a mode flag through `BrBootGlobal_ABAA0()` (a `return 0;` stub in
another TU). At /O2 that cannot inline across TUs, so it stays a `call`. The orig
reads the global DIRECTLY: `cmp dword [0x100abaa0], ebp`. Use the real global
(`g_AC300`) instead of the stub. General: a cross-TU stub getter in the port is a
prime source of a spurious `call` the orig doesn't have.

## 3. Port-safety null-check the original never had
`if (hook) hook(...)` where the orig calls `[ptr]` UNCONDITIONALLY. Confirm by
disassembly (no `test/jz` guarding the call), then drop the check (guard the
port's under `#else` if the port needs the safety). See memory
[port-safety-additions-block-matches](../../../../docs/brally/decomp/triage/port-safety-additions-block-matches.md) - biggest single blocker class.

## 4. Call-arg read not hoisted
Orig reads `dlBase = model+0x80` AT the call arms, not into a hoisted local.
Inline the read into the call argument; the hoisted local is an extra slot+load.

## 5. Shared Horner tail across branch arms
Two colour-pack arms that end in the same `(((top<<8|p0)<<8|p1)<<8)` merge at a
common tail in the orig (spilling p0/p1 to byte slots [esp+0x31]/[esp+0x32],
re-read with `& 0xFF`). Factor the final pack into ONE statement after a nested
`if`, routing each arm's differing `top` through a shared local. Missing-multiset
−7. (Cross-jump / branch-symmetric-temps idiom applied to byte packing.)

## FRAME LEVER (corrects an earlier failed attempt)
Orig `sub esp,0x4c` vs recomp `0x40` (3 dword gap). Hoisting the TWO disjoint
`BrVec3 tmp` block locals to DISTINCT function-scope locals DID grow the frame to
0x4c and made the prologue byte-exact - the earlier "MSVC overlays them anyway"
conclusion was wrong; it depends on how the hoist is done (two named
function-scope vectors, not one shared).

## METRIC WARNING for this function
Region count has DECOUPLED from byte distance. Removing a branch (the null-check)
RAISED region count 36→38 while REGNORM fell 9. Rank by the register-blind
multiset gap (REGNORM), not the divergence region count - per
[register-rotation-is-a-symptom](../../../../docs/brally/decomp/triage/register-rotation-is-a-symptom.md). Remaining walls are whole-function
register/frame coloring the local source levers do not reach.
