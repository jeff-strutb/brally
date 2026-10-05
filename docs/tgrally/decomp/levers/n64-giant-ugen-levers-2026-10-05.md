# N64 giant ugen levers

*Recorded 2026-10-05.*

> BrRaceTick (22 KB giant) to T4 on 2026-10-05: ugen operand-order/need levers, the instr2 ring-injection compiler, -O1 probes, register-blind top-down workflow

BrRaceTick 0x8020082C went T3 -> T4 on 2026-10-05 (commit bb842a01, image gate 661/0) by hand transcription, top-down.

**Workflow that worked for a giant (no uopt, ugen only):**
- `build/tgrally/n64/m2tools/firstdiff.py VA draft [N] --nosp` (ignores sp offsets, so a frame-size shift doesn't hide everything) and `--blind` (also ignores register names). Fix STRUCTURE first with `--blind` all the way to the end, then chase register naming.
- `spell.py` honours `FD_MODE=--blind|--nosp`; `joinsearch.py` (line joins); `scratch/bin/swaparms.py` swaps if/else arms brace-matched.
- Most structural residue was branch POLARITY: ugen lays out the then-arm first, so `if (x == 0) {A} else {B}` vs `if (x != 0) {B} else {A}` differ. Also if-chains on one variable that the ROM does as `switch` (s7 holds the value, `beq`/`beql` ladder).
- Comparisons: ugen loads the LEFT operand first; `a < b` vs `b > a` changes load order and FP register order.

**Register residue in a giant = ugen evaluation order, not the ring:**
- ugen emits a commutative add's operands in EVALUATION order, and evaluates the operand needing more registers first (Sethi-Ullman). `&objs[s[i].obj]` puts the index first; `(s + i)->obj` (pointer arithmetic on a struct array, then a field) lowers the index's need, so `objs` loads first and lands in the ROM's register and operand order. An int array has no such form; the ROM's waterfall list turned out to be an array of one-field structs.
- Two `addiu tN, zero, 0` stores sharing one index = a `long long` field set to 0 (confirm the field from a T4 sibling TU, here ghoststep.c).
- A local that only ever spills (`sw t1, 0x74(sp)` with no reload) is a compiler CSE spill, not a declared local. Drop padding locals once the spill appears.

**Tools built (build/tgrally/ext and build/tgrally/n64/m2tools, gitignored):**
- `build/tgrally/ext/instr2/out/cc`: the traced IDO with a ugen hook. `DKWB_INJECT=proc:emit:reg,...` moves a register to the end of ugen's free list (0x10019da4; 0x10019da8 is the used/LRU list) at an emit index.
- `inject.py VA draft [spec]` grades under injection (`SHOW=a:b`, `SKIP=n`); `ringsearch.py` greedily finds the ring moves the ROM implies. If no single move fixes a statement, the cause is evaluation order, not the ring.
- `scratch/probe/run.py` (`PFLAGS=-O1`) compiles a small file the way a giant compiles (no uopt) and disassembles it. Use it to test spellings in about a second each.
- `DKWB_UGEN_TRACE=1` free-list records (ALLOC_GP_RESULT / FREE / MOVE_END with source line) show the pop order per statement.

Related: [hand-transcription-only](../../../brally/decomp/rules/hand-transcription-only.md), [n64-ido-trace-tooling](../toolchain/n64-ido-trace-tooling.md), [n64-giant-t3-method-2026-09-28](n64-giant-t3-method-2026-09-28.md).
