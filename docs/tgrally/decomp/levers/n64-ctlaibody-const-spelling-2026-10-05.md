# N64 ctlaibody const spelling

*Recorded 2026-10-05.*

> BrCtlAiBody 0x8022762C T4 2026-10-05 (69b4e284, 1064 -> 0): uopt merges FLOAT CONSTANTS BY SPELLING (cfe ucode carries the text); constant-web save/cost decides callee FP saves; IV reset after the inner loop enables SR; first-appearance web numbering breaks priority ties

BrCtlAiBody (src/tgrally/driving/ctlai.c) went 1064 -> 0 on 2026-10-05, commit 69b4e284, image gate 668/0. Hand transcription, about 15 reasoned steps, read from the ROM top-down.

**Float constants are keyed by their SPELLING.** cfe's ucode carries each float literal as text ("0.2", "0.0"; negated and folded ones as "-2.0000000298023224e-01"). uopt builds ONE constant web per spelling across the whole function. A function-wide web can cross calls or loops and change save/cost, so it grabs a callee FP register or splits where the ROM rematerialises (or the reverse). Levers, each measured:
- `t < 0` (int literal) instead of `t < 0.0f` takes a loop-weighted use out of the "0.0" web (save 86 -> 56 < 60 callee cost: it splits as in the ROM, frees f22, prologue matches).
- Writing one group differently (`.2f` vs `0.2f`, or `mag = 0.1;` as a double literal, which cfe folds to an exponent-form spelling) separates webs. That restored the ROM's per-branch CSE (one literal per branch) and fresh `lui/lwc1` loads.
- Read the spellings: capture uopt's input with `decomp-workbench capture make tools/toolchains/ido53 DIR --phase uopt`, decode `before-8-*`, and print `ldc dt12/13` strings.

**Other levers:**
- **Strength reduction of `seg->pt[i]`:** the IV reset `i = 0` must come AFTER the inner `while (seg->flags & 1) seg = seg->alt;`. uopt then steps a pointer (`addiu +0x28`, `move v1, v0` resets). Found with a 10-line probe file (`probe/run.py` with `PFLAGS=-O2`).
- **Equal-priority webs tie-break by web number = the variable's FIRST APPEARANCE in the IR** (even a dead store counts; the declaration order does not). Reusing an earlier variable (`i`) for the second sign ladder gave it the earlier number.
- **Memory-resident temps:** a dead first store kept plus `lw aN, off(sp)` args = a `float x[3]` array (struct fields get promoted, arrays don't).
- **Compare/commutative order:** `lat += mag*lat` (compound) vs `lat = lat + mag*lat`. Reading through a local pointer (`other = tbl[i].car`) flips `beql`.
- **Frame:** solve the declaration order from the ROM's stack homes. Pad with `int unusedXX[n]`.

Related: [n64-loadsave-uopt-2026-10-05](n64-loadsave-uopt-2026-10-05.md) (operand order = node vs leaf), [n64-cpakcheck-prototype-v0-2026-10-05](n64-cpakcheck-prototype-v0-2026-10-05.md), [n64-introscreen-frame-layout-2026-10-05](n64-introscreen-frame-layout-2026-10-05.md).
