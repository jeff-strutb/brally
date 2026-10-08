# N64 ugen invisible pop lever

*Recorded .*

> IDO ugen levers from a peer N64 session (2026-10-06): invisible ring pop when a statement root has a constant operand; Sethi-Ullman evaluation order vs ucode emit order

Reported by a peer N64 T4 session on 2026-10-06. Not yet verified in this session.

- **Invisible pop:** when a statement's root op has a constant operand (`x = (a - b) >> 1`, `t = lbu * 4`), ugen gives the destination register to the first child, computes the root into a ring temp and emits a move. as1 then folds the move away. The final code shows the variable written directly, but the temp ring has still advanced by one.
- **Evaluation order:** ugen evaluates the operand with the higher Sethi-Ullman need first (a leaf Smt load counts 1, an Rmt or int ldc counts 0, equal binary children add 1), but emits in ucode order.

**How to apply:** when t-register names are shifted by one with identical instructions, look for one of these before suspecting the source structure. Related: [n64-pakmanager-wip-2026-10-06](../functions/n64-pakmanager-wip-2026-10-06.md).
