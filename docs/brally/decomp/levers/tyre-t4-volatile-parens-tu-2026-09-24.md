# Tyre t4 volatile parens tu

*Recorded 2026-09-24.*

> 2026-09-24: 0x100651A0 BrCarPhysTyre (1355 B, largest open fn) hand-transcribed to BYTE-EXACT T4 (f16f98cf). Three levers that broke 'walls': volatile float temp = original's store-and-reload rounding; explicit parens + statement order steer the x87 scheduler; integer index order / call-arg regs are TU state (fixed by moving the arm).

**Result:** 0x100651A0 BrCarPhysTyre T2 (9+13 multiset, "four scheduler regions", ~130 probes + 272 decl sweep dead on 09-13) -> T4 byte-exact in one session (commits 669e7ba3, 3c7dc291, f16f98cf; filing bd28b563; idioms b4b2d92b). C, not C++ (cdecl stack args, no EH frame).

**Levers, in the order they fell:**
1. Clamp then-arm: explicit temp negated in place (reuse `load`), not the ?: abs macro.
2. `c.x * (0.0f - q)` = one CSE'd fchs + reg multiplies (`c.x * -q` becomes -(c.x*q)).
3. `(f1C4 * K) * dt` for the display angle (the 09-13 "dead" verdict was context-dated).
4. Drop the `pM` local: a CSE-numbering tie-break flipped the spin's r*X preload.
5. PLACEMENT: grip-index eval order + BrMat4MulVec3 arg registers are TU state. Every index spelling byte-identical; N dummy predecessor fns (20..41) or moving the arm below the port's drive helpers fixes both. Integer regs follow VC5 round-robin temp allocation, so fixing the index fixed the call args.
6. `volatile float cs;` + `c = cs*c; c = (sn*d) + c;` - the original rounds through memory (sin popped/reloaded, cs*c stored then `fadd [c]`). Nothing else (casts, double protos, unions, aggregates, pointers, /Op, other compilers VC4.0-6) reproduces it without breaking the cross-product block.
7. load/dot block: order a.z, load, *pA, dot + `load = (a.z*nz)*3.5f` + `dot = ((vx*cx)+(vy*cy))+(vz*cz)`. Found by a 5-order x 16-paren grid; all 630 plain orders were 3 wrong families.

**Why:** the 09-13 dossier called regions 1-4 symbol-index/scheduler walls and stopped. Each fell to a lever it hadn't tried: volatile, paren x order grids, TU placement.

**How to apply:** for x87 "scheduler walls": (a) run pad-count diagnostic (N dummy `int f(int)` before the fn) - if a region flips, it's TU state, fix by placement; (b) grid explicit parens x statement order per block; (c) if the original stores a float temp and reloads every use, try `volatile` on that temp. Sibling 0x100645A0 (same TU) has the same sin pattern. Tools I used live only in the session scratch (sbs.py side-by-side, reg.py region lister, repj.py JSON edit-in-arm); worth minting into tools/brally/ if reused. Supersedes [tyre-pass-mechanisms-2026-09-13](tyre-pass-mechanisms-2026-09-13.md). Related: [x87-wall-mechanism-2026-09-13](x87-wall-mechanism-2026-09-13.md), [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md), [tu-constant-pool-oracle-2026-09-15](../corpus/tu-constant-pool-oracle-2026-09-15.md).
