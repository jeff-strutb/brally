# N64 decalpak transfer

*Recorded 2026-10-05.*

> BrDecalPakTransfer 0x80248F88 hand transcription state 2026-10-05 (session): decal step solved (direct field reads, row*stride, mask index local); case 7 head knot open (top colouring + LOAD/SAVE li a1 order); compiler-internals toolkit that made it tractable

BrDecalPakTransfer (src/tgrally/menus/decalpak.c, 7.4 KB, claim "80248F88 "). Must be co-filed after image.c's BrBevelPanel/BrFillRect/BrImageDrawAt (ROM paint TU; cupcosts). Draft: scratch dp/m8.full.c (= BrPaintDecalCommit + image.c + decalpak, calls renamed BrBevelPanel).

**Solved this session (all by reading the listing + compiler laws):**
- Mask loop: `row * stride` (multu operand order = source order), and the mask index through a local (`x2 = (col >> 3) + row * stride; mask[x2]`): IDO puts the base first in `mask + idx` only when the index is a plain variable (cfe uadd complexity order, workbench L52).
- Decal step: read `m->parts[m->decal[D_80369E61]].w/.h/.tex` directly, NOT through a `part` local. The local made `parts + idx*36` emit idx-first and cost one ugen ring pop (workbench L76); with direct reads the whole 505-row ring rotation vanished.
- With top's case 7 piece forced to split (`n64alloc force ... p1:w639=s`, CDX_PROC=11), the TU draft is 3 instructions from EXACT, same length (1849).

**Case 7 head SOLVED (zero-instruction statement, workbench L37/L48):** override block written `ty = 0x7C; if (ty); top = 0xDB;` on ONE line. The empty `if` emits nothing but splits uopt's block, so top's override def is not in the block adjacent to the head; top's piece loses the defs' contribution (L36 chargeB: a def whose block has a successor outside the piece) and goes memory-resident, ty stays v0. Same physical line keeps as1's lineno tie-break giving the ROM order (li t6; sw t6; li v0). Rule learned: exact under a forced split but not stock = look for a missing zero-footprint statement (probe `if (x);` before each line; the variable defined AFTER the split is the one struck).

**Open:**
2. ROM `li a1,0x73` sits before `bne op,9` (as1 took it from the else block, or uopt placed it): ugen's binasm must have had `li $5,115` before `la $4` in the SAVING call. Proven with the binasm harness (both placements reproduce ROM). No source form tried yields it (prototypes, casts, variables, ternaries all keep la-first; uopt const-props every variable).

**LOAD/SAVE residue diagnosed (2026-10-05, draft scratch dp/cf.full.c = DIFF 3):**
- as1 fills the bne stall slot with the FIRST binasm record of the target block whose dest is dead on the fallthrough (exact dataflow; proven by binasm permutations). Line numbers don't matter. It also deletes redundant li and dead instructions afterwards.
- Binasm proof: inserting a dead `move $12,$4` (record 00170062 18114000) at the start of the LOADING arm reproduces the ROM EXACTLY. So the original ugen output had a junk read of a0 at the LOADING arm start, deleted by as1 later (like the junk `lw $12,0xb0($sp)` reloads ugen emits after calls).
- NEXT: find the source construct that makes ugen emit a read of $4 at the LOADING arm start (cf.B already has `move $16,$4` at 6 sites: study what source makes those).
- Ruled out (all la-first): every spelling of 115, prototypes (K&R, variadic, implicit), ternaries/switch, variables (uopt folds every use), params/statics/volatile, string symbols, zero-footprint compares. Constant webs never include call-arg uses. ugen never reorders uopt's Rpar stores.
- Also ruled out (binasm): .livereg records, a label at LOADING start, both strings via one symbol, a dead `lw $4` in the branch block (it survives into the stall slot), sw 0x38 moved into the arms. Source: zero-footprint `if (v);` at LOADING start for every local (only `if (op);` changes code, putting the op copy in v0). Allocator check: no single web forced to a0 helps.
- Private uopt (scratch ib4) now also logs webdetail for p1cand webs (role=cand). Constant webs: per-block contrib = uses*weight - (1 if a load is needed); call-argument uses are NOT occurrences.
- macOS gotcha: pipe.sh writes uopt output to $n.O and as1 to $n.o, which are the SAME FILE (case-insensitive). Use another name for uopt output.

**Toolkit (reusable):**
- `cc` assembles `.s` with as1 `-noglobal` (no cross-block scheduling), so `.s` experiments are NOT faithful. Faithful: run cfe/uopt/ugen/as1 by hand (scratch dps/keep/pipe.sh), edit ugen's binasm (16-byte records; op 0x48=la, 0x52=li, 0x54=lw, 0x1c=.loc, 0x35=.livereg), re-run as1.
- as1 fills a branch's load-delay gap with the FIRST record of the target block (target-hoist), only in the "near" regime; ROM shows no lui-hoist anywhere.
- Instrumented ugen: build/tgrally/ext/instr2/out/ugen; `DKWB_UGEN_TRACE=1` logs FREELIST pop/free with source line; `DKWB_INJECT=proc:emit:reg` moves a reg to the ring tail (oracle for ring-phase diagnosis); `DKWB_UGEN_SCHED=1` emit provenance. Combined toolchain scratch combo/ (ib4 uopt + instr2 ugen) graded via n64alloc Grader with TGR_TRACE_CC.
- `CDX_DETAIL_WEB=<web>` prints a web's interference list. Workbench laws: build/tgrally/ext/n64-decomp-workbench/docs/compiler-laws/ido-5.3.md (L52, L58, L66, L76 used here).
