# Tyre pass mechanisms

*Recorded 2026-09-13.*

> SUPERSEDED 2026-09-24 by tyre-t4-volatile-parens-tu-2026-09-24 (function is now T4). 2026-09-13 evening: tip kick 0x10066D70 CERTIFIED @t3 (the vn dot needed explicit grouping); tyre pass 0x100651A0 transcribed to T2 (1340/1355, rows 9+13, 4 scheduler regions). Two VC5 mechanisms proven: explicit PARENTHESES change float product codegen, and a product's operand order is a SYMBOL-INDEX tie-break that the TU's header set flips (standalone TU != real TU).

**Tip kick 0x10066D70 -> @t3 (crank committed my edits as e107de2/1b58365).**
The 4 missing `fxch` were the WHEEL dot `vn`, not the chassis dot `s` (the
T2 dossier had it wrong): `nx*wx + ny*wy + nz*wz` flat is re-associated to
(t1+t3)+t2 and emitted sequentially around the `add esp`/`sub esp` pair;
`(nx*wx + ny*wy) + nz*wz` restores the preload shape AND the term order
(63 diffs, rows 4+4, 3 ledger passes / 52 probes). Residue: `count` colour,
p/w slot pair, the `s` product operand order (every base/kind/pointer/
named-field spelling byte-identical; matrix via `pM` un-folds to [ebp+k]).

**Tyre pass 0x100651A0 -> T2** (src/brally/core/driving/br_carphys.c, matching arm
beside the port with the `#define Name Name_port` rename kept ACTIVE for the
callers). Levers: ONE vector `c` scaled in place then `sn*d + c`; the force
lands in `a` (dead zero stores + mass*g in a.z); spin as ONE expression (a
named `w` in two statements flips `r * w`); `&&` contact gate with the
free-spin `else`; `&&` wrap tail with the zero `else` (a shared `goto zero`
block lands mid-function); float global for -360 (literal folds to double).
Plateau after ~130 probes + a 272-compile declaration sweep (only the
sn/d order matters, and it swaps region 1 for another wrong shape).

**Mechanisms (micro-TU harness `mx.py` in the scratch, compile + capstone):**
- VC5 honours EXPLICIT PARENTHESES in float products even when the parse
  tree is identical: `w->f1C4 * K * dt` -> (f1C4, dt, K); `(w->f1C4 * K) * dt`
  -> (f1C4, K, dt). Literals always sort last. Port sources carry parens --
  keep them when transcribing.
- The display-angle product (f1C4, K, dt) compiles f1C4-FIRST in a
  standalone TU and dt-FIRST the moment `br_carphys.h` (the repo headers)
  is included: a SYMBOL-INDEX tie-break keyed on the whole TU's symbol
  count, not on anything in the function. Bisecting function-local
  constructs (12 cuts, 30 micro variants) cannot find it; only the include
  set moves it. Corollary: a micro-TU experiment is only faithful with the
  real header set included.

**Why:** two of the four tyre regions and the tip kick's `s` region are this
class; hours went into expression spellings that a header-count tie-break
makes inert.

**How to apply:** when a 2-3 operand float product is in the wrong order and
parens/grouping are dead, stop: it is the TU symbol-index class. Record it in
the dossier and certify if the rest passes Gate A. Related:
[five-largest-2026-09-13](../log/five-largest-2026-09-13.md), [declaration-order-tiebreak](declaration-order-tiebreak.md),
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md).
