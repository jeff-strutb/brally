# IDO 5.3 matching handbook

What the Top Gear Rally decomp (572 functions, all byte-exact against
`Top Gear Rally (USA).z64`) taught about writing C that SGI IDO 5.3 `-O2`
turns into a given N64 listing. It is a distillation; each rule links to the
dated note that measured it. Everything here was measured on IDO 5.3 with
`-O2 -mips2 -G 0 -Wab,-r4300_mul`; it should carry to any IDO 5.x N64 game.

Contents:

1. [Method](#1-method)
2. [Which stage decided it](#2-which-stage-decided-it)
3. [Frame and declarations](#3-frame-and-declarations)
4. [cfe: the front end](#4-cfe-the-front-end)
5. [uopt: the global optimiser and allocator](#5-uopt-the-global-optimiser-and-allocator)
6. [ugen: code generation](#6-ugen-code-generation)
7. [as1: scheduling](#7-as1-scheduling)
8. [Zero-footprint statements](#8-zero-footprint-statements)
9. [Data, symbols and linking](#9-data-symbols-and-linking)
10. [Giants: functions over -Olimit](#10-giants-functions-over--olimit)
11. [Tools](#11-tools)

---

## 1. Method

**Hand transcription from the listing.** Every late function closed by
reading the ROM's code as the program it is and writing the C that produces
it, then making one reasoned edit per residue. Generated variant batches,
permuters, flag sweeps and force sweeps plateau and were banned for the M2
push ([rules/hand-transcription-only](rules/hand-transcription-only.md),
[rules/hand-transcription-n64-m2](rules/hand-transcription-n64-m2.md)).
Forcing one allocator decision to learn which one the ROM made is allowed as a
diagnostic; a forced build is never kept
([rules/diagnostic-forcing-allowed](rules/diagnostic-forcing-allowed.md)).

Order of work for one function:

1. **Frame first.** Read every `sp`-relative access: the frame size, the
   saved registers, the homes of locals, the spill slots. Solve the
   declaration list from them (section 3).
2. **Structure next, register-blind.** Diff ignoring register names and stack
   offsets (`firstdiff.py --blind`, `--nosp`) until the opcode stream matches:
   branch polarity, block order, switch vs if-chain, loop shapes, call order.
3. **Allocation last.** Map every ROM register to a role: uopt-coloured
   variables and CSE values (v0, v1, a0-a3, t0-t5, s0-s8, f0, f2, f12-f18)
   versus ugen ring temporaries (t6-t9, f4-f10). A value the ROM keeps in a
   uopt register where ours has a ring temp is a missing named local; one
   home written from two registers is two variables.
4. **Residue: find the deciding stage** (section 2), read why the compiler
   chose what it chose (the instrumented allocator logs are fine for reading),
   then change the source once.

**PC twins** (the same game's MSVC build) give the source shape: locals,
statement order, helper calls. Port the twin, then drop MSVC-only crutches
([log/n64-t1-intake-2026-10-03](log/n64-t1-intake-2026-10-03.md)). A twin
compiled without optimisation shows the literal source shape; read it before
inventing variables ([functions/n64-animupdate-wip-2026-10-07](functions/n64-animupdate-wip-2026-10-07.md)).

**Ghidra bodies are drafts.** Several "walls" were Ghidra structure carried
into the source. Retranscribing the body from the asm closed them in a few
builds ([log/n64-m2-session-2026-10-04](log/n64-m2-session-2026-10-04.md)).

## 2. Which stage decided it

| Symptom (instructions otherwise identical) | Stage | Section |
|---|---|---|
| Frame size or home offsets differ | declarations, cfe temps, uopt spill slots | 3, 5.10 |
| Operand order of `+`, `*`, compares | cfe tree order, uopt leaf vs node | 4 |
| A value in a different uopt register (v0/v1/aN/tN/sN, f0-f18) | uopt colouring | 5 |
| A variable reloaded from its home in ROM, kept in a register in ours (or the reverse) | uopt split/save | 5.6-5.8 |
| Extra callee-saved register in the prologue | uopt callee cost vs save | 5.5 |
| t6-t9 (or f4-f10) names shifted by one | ugen ring | 6 |
| Same instructions, two adjacent ones swapped | as1 | 7 |
| An instruction moved across a branch | as1 cross-block hoist, uopt `.alias` | 7 |
| Extra or missing CSE / reload | uopt CSE, aliasing, statics | 5.9, 9 |

## 3. Frame and declarations

- Every declared local gets a home, top-down in declaration order from
  `sp + frame - 4`, used or not; block-scope locals sit below function-level
  ones; each gbi macro `_g` pointer takes its own slot; uopt spill temps go
  below the locals ([log/n64-m2-session-2026-10-04](log/n64-m2-session-2026-10-04.md),
  [levers/n64-introscreen-frame-layout-2026-10-05](levers/n64-introscreen-frame-layout-2026-10-05.md),
  [levers/n64-brmenu-frame-layout-2026-10-05](levers/n64-brmenu-frame-layout-2026-10-05.md)).
  Unused arrays always keep their slots; pad with `int unusedN[k]`.
- An unused word at the top of the frame plus a spill just below it is one
  short-lived named scalar and a CSE temp
  ([levers/n64-ido-levers-2026-09-27](levers/n64-ido-levers-2026-09-27.md)).
- A spill at a low offset (the temp area) is a uopt temp or a block-scoped
  local, not a function-level variable; a ROM pointer in a low slot is a
  strength-reduced temp ([levers/n64-spill-slot-order-lever](levers/n64-spill-slot-order-lever.md)).
- One frame word too many with every home matching: a cfe temp from a call
  nested in another call's arguments, or a ternary inside a comparison. Hoist
  it into a declared local ([levers/n64-paintclick-locals-lineno-2026-10-05](levers/n64-paintclick-locals-lineno-2026-10-05.md)).
- Odd filler counts above a float array re-align it.
- A frame larger than the ROM's changes where callees read stale stack, which
  the live oracle catches as DIVERGENT ([log/n64-m1-complete-2026-10-04](log/n64-m1-complete-2026-10-04.md)).
- Temp homes are allocated to split webs even when the code never touches
  them; an "unused" word in the temp area is normal.

## 4. cfe: the front end

- **Operand order.** For an assignment RHS cfe emits op(V(R), N(L)): the last
  term of `a + b*c + d*e` comes out first. Write natural source order; hand
  "ROM-ordered" source gets scrambled
  ([functions/n64-carslotswap-2026-10-06](functions/n64-carslotswap-2026-10-06.md)).
  Reordering applies only on the root/left spine; a pending value disables it
  ([functions/n64-carcarcollide-2026-10-06](functions/n64-carcarcollide-2026-10-06.md)).
  `x += e`, `x = x + e`, `x = e + x` all differ; `+=` loads x last.
- **Compares.** ugen loads the left operand first; `a < b` vs `b > a` changes
  load and FP register order. In the ucode uopt hands ugen, a plain variable
  leaf comes first and a node second, so ROM `bne $const, $reg` means the
  compared value is a node; `(mode | 0) == 2` keeps the node at no cost
  ([levers/n64-loadsave-uopt-2026-10-05](levers/n64-loadsave-uopt-2026-10-05.md)).
- **Array indexing evaluates first** (ixa); `&objs[s[i].obj]` puts the index
  first, `(s + i)->obj` lowers its need
  ([levers/n64-giant-ugen-levers-2026-10-05](levers/n64-giant-ugen-levers-2026-10-05.md),
  [functions/n64-carddriveinput-wip-2026-10-06](functions/n64-carddriveinput-wip-2026-10-06.md)).
  `A + x` vs `&A[x]` swaps the addu operands.
- **Hidden temps:** `?:` is emitted before the rest of the statement; a call
  inside another call's args or an expression with an array index
  (`call() + a[i]`) gets an 8-byte frame temp; cfe reuses one temp slot for
  switch selectors and post-increments, so those share a uopt web
  ([levers/n64-cpakcheck-prototype-v0-2026-10-05](levers/n64-cpakcheck-prototype-v0-2026-10-05.md),
  [functions/n64-carplayerctl-wip-2026-10-06](functions/n64-carplayerctl-wip-2026-10-06.md)).
- **Literals are keyed by spelling.** `0.0f`, `0.f`, `.0f`, `0.00f` are
  different constants; int `0` and double literals converted to float share
  one exponent-form constant; `1000.f` and `1000.0f` never merge. Each
  spelling is its own uopt web with its own priority
  ([levers/n64-ido-float-literal-spelling](levers/n64-ido-float-literal-spelling.md),
  [levers/n64-ctlaibody-const-spelling-2026-10-05](levers/n64-ctlaibody-const-spelling-2026-10-05.md)).
  `x * 2.0f` becomes `add.s`; ROM `mul.s` by 2 means integer `x * 2`;
  `/ 256.0` becomes a multiply, the ROM's `div.d` came from `/ (double)256`.
- **Prototypes decide registers.** A callee declared `int` that is really
  `void` keeps v0 live in its block and forbids v0 to every web there; a call
  with one argument too many claims a3. Check every prototype first when the
  residue is v0/v1 or a-register swaps
  ([levers/n64-cpakcheck-prototype-v0-2026-10-05](levers/n64-cpakcheck-prototype-v0-2026-10-05.md),
  [functions/n64-carselect-handtranscription-2026-10-05](functions/n64-carselect-handtranscription-2026-10-05.md),
  [functions/n64-racegatestep-2026-10-06](functions/n64-racegatestep-2026-10-06.md)).
- Types change CSE: two uses that differ in type are two expressions; a cast
  on one use kills a CSE the ROM has. Unprototyped calls avoid spurious casts.

## 5. uopt: the global optimiser and allocator

Most M2 residue lived here. The instrumented uopt
([toolchain/n64-ido-trace-tooling](toolchain/n64-ido-trace-tooling.md)) prints
every candidate, decision, split and growth step; read it before editing.

### 5.1 Candidates and webs

- A variable is a register candidate unless an address-of (LDA) location
  overlaps it; taking `&x` also makes calls kill CSEs involving x
  ([functions/n64-aiscancorridor-wip-2026-10-07](functions/n64-aiscancorridor-wip-2026-10-07.md)).
- **A variable is one live range.** Reusing it for an unrelated value drags
  that value onto its colour; disjoint if/else arms that need different
  registers declare their own block-scoped locals
  ([log/n64-m2-session-2026-10-04](log/n64-m2-session-2026-10-04.md),
  [levers/n64-spill-slot-order-lever](levers/n64-spill-slot-order-lever.md)).
- Never-address-taken local arrays and struct fields are promoted to
  registers; local arrays whose address is taken stay in memory.
- Address webs: every array element or struct member address gets one; a
  scalar global only when it is loaded or chain-assigned. They are keyed by
  symbol and offset.
- Constant webs (constinreg): a constant stored by a statement is always a
  candidate; add/sub operands only if they do not fit an immediate; and/or/xor
  only if negative; mpy/div by a power of two never; compare operands never;
  zero never ([functions/n64-musicthread-t4-2026-10-07](functions/n64-musicthread-t4-2026-10-07.md)).
  Uncoloured constant webs emit nothing but still interfere
  ([functions/n64-modrowread-t4-2026-10-07](functions/n64-modrowread-t4-2026-10-07.md)).

### 5.2 Web numbers

- Web numbers follow the first appearance in the code (a dead store counts;
  declaration order does not). uopt walks the code twice: constant
  propagation rebuilds every expression containing a propagated variable,
  and those get late numbers ([functions/n64-carslotswap-2026-10-06](functions/n64-carslotswap-2026-10-06.md),
  [functions/n64-pakmanager-wip-2026-10-06](functions/n64-pakmanager-wip-2026-10-06.md)).
- Ties (equal priority) go to the lowest web number. Naming a value early
  (`cx = (x0 + x1) >> 1;` once, propagated away) reorders colouring at no
  code cost ([functions/n64-dashoval-t4-2026-10-07](functions/n64-dashoval-t4-2026-10-07.md),
  [functions/n64-weatherdraw-8023A784-2026-10-05](functions/n64-weatherdraw-8023A784-2026-10-05.md)).

### 5.3 Priority

- priority = save / div, div = ((occurrences + live blocks - 2) >> 2) + 2.
  Live-range length is priority: moving an init next to its loop raises it
  ([functions/n64-pakmanager-wip-2026-10-06](functions/n64-pakmanager-wip-2026-10-06.md),
  [log/n64-m2-session-10a087-2026-10-04](log/n64-m2-session-10a087-2026-10-04.md)).
- Per-block save: (uses + defs) x weight, minus weight if the value must be
  loaded on entry and the block is not a movable loop head; loop blocks weigh
  10.
- Webs with save <= 0 are dropped at the first scan and stop counting as
  interference ([functions/n64-filloval-wip-2026-10-07](functions/n64-filloval-wip-2026-10-07.md)).

### 5.4 Phases and colours

- Colours: 1 v0, 2 v1, 3-6 a0-a3, 7 t0, 8-12 t1-t5, 13 ra, 14-22 s0-s8; FP
  24 f0, 25 f2, 26-29 f12-f18. The lowest free colour wins among equal costs.
- A web with 22 or more interferers is constrained and coloured in phase 1
  by priority; the rest are coloured in phase 2, first-fit in web-number
  order ([functions/n64-modrowread-t4-2026-10-07](functions/n64-modrowread-t4-2026-10-07.md)).
- Any uopt colour in t1-t5 removes that register from ugen's ring for the
  whole procedure, so the ROM's ring usage proves which t-registers uopt
  never used.

### 5.5 Costs

- **Callee-saved cost = 0.25 x basic blocks** (clamped 4..60), plus call
  costs. Calls end blocks; labels merge; an empty `if (c) {}` adds two blocks
  ([functions/n64-musicthread-t4-2026-10-07](functions/n64-musicthread-t4-2026-10-07.md)).
  This bounds the ROM's block count: a web the ROM keeps in an s-register (or
  splits) pins the count to a narrow window, so it limits how many empty tests
  the original can have had.
- Caller-saved cost charges a unit whose block's first predecessor, or whose
  own block, is a register-clobbering call inside the piece.

### 5.6 Splitting

When a web's total save does not beat its best cost it is split:

- The seed is the first unit with an entry definition, else the first unit
  with uses in depth-first order.
- Growth adds successors of expanded blocks. A block is not expanded if it
  ends with a call that clobbers registers, ends with an indirect jump, or
  the variable is not live out of it.
- A successor joins only if `new_shared < colours_left_before` and
  `2 x colours_left >= interference + new_shared`
  ([functions/n64-aiscancorridor-wip-2026-10-07](functions/n64-aiscancorridor-wip-2026-10-07.md)).
- An unprofitable piece is dropped and the rest re-split while its save
  stays positive.
- So whether a variable is kept in a register in one region depends on how
  many live, positive-save webs share that region.

### 5.7 Loops

- `continue` lets uopt replace the counter test (LFTR); nested ifs and
  compound conditions block it. The same address expression in several loops
  is one strength-reduction web ([functions/n64-carslotswap-2026-10-06](functions/n64-carslotswap-2026-10-06.md)).
- `r << 2` keeps r as the counter, `r * 4` is strength-reduced; an
  `unsigned` index sum stops reduction; a reload of a value in a loop step
  means the source multiplied, not accumulated
  ([log/n64-m1-complete-2026-10-04](log/n64-m1-complete-2026-10-04.md)).
- The loop latch compares with `bne` only against a compiler temp limit; a
  user variable limit stays `slt`/`bnez`.
- A function-static loop counter is not LFTR'd; a pointer co-induction in
  the `for` header stops unrolling; small constant loops need their own
  counter to unroll ([levers/n64-ido-levers-2026-09-27](levers/n64-ido-levers-2026-09-27.md)).

### 5.8 Blocks

- uopt splits a basic block once its cfe-level `lod` count would pass 21
  ([functions/n64-carslotswap-2026-10-06](functions/n64-carslotswap-2026-10-06.md));
  a 24-local-load limit applies after a call
  ([functions/n64-camchasestep-wip](functions/n64-camchasestep-wip.md)).
- `do { } while (0)` and empty tests add blocks; block boundaries end live
  ranges, which is how a value can give up a register before the next one
  takes it ([functions/n64-aiscancorridor-wip-2026-10-07](functions/n64-aiscancorridor-wip-2026-10-07.md)).

### 5.9 CSE, PRE and aliasing

- Pointer stores alias globals; indexed-global stores do not. Indexing the
  table (`T[n].f` at every use) instead of an element pointer changes which
  values are CSE'd or reloaded ([log/n64-m2-session-10a087-2026-10-04](log/n64-m2-session-10a087-2026-10-04.md)).
- PRE needs a non-critical predecessor: structured ifs vs a `goto` into a
  join decide whether a load is hoisted ([functions/n64-racegatestep-2026-10-06](functions/n64-racegatestep-2026-10-06.md)).
- `default: break;` gives PRE an edge block.

### 5.10 Spill slots

- Spill slots are handed out first-fit in expression-number order, each
  avoiding slots of earlier candidates live in the same blocks. To move a
  slot, change where its expression is first met
  ([levers/n64-spill-slot-order-lever](levers/n64-spill-slot-order-lever.md),
  [functions/n64-dashcircle-t4-2026-10-07](functions/n64-dashcircle-t4-2026-10-07.md)).

### 5.11 The `.alias` pseudo

- uopt emits `.noalias $r,$sp` when an address register's range opens and
  `.alias $r,$sp` where it closes, at the start of the first block where the
  web is not live. Inside its noalias range as1 may move sp loads above
  stores through that register. A block boundary that closes the range early
  moves `.alias` up and blocks the move; an alias pseudo between a branch and
  the next label also forms an extra as1 block
  ([functions/n64-planeresolve-t4-2026-10-07](functions/n64-planeresolve-t4-2026-10-07.md)).

## 6. ugen: code generation

- **The temp ring**: t6 t7 t8 t9 t0..t5 minus uopt-coloured registers, FIFO
  on free. One extra temp anywhere shifts every later name
  ([log/n64-m2-session-10a087-2026-10-04](log/n64-m2-session-10a087-2026-10-04.md)).
- **Invisible pops**: a statement root with a constant operand computes into
  a ring temp then moves (as1 folds the move); `if (a - b)` / `if (a ^ b)`
  pops one where `!=` does not; a redundant `& 0xffff` pops one
  ([levers/n64-ugen-invisible-pop-lever](levers/n64-ugen-invisible-pop-lever.md),
  [functions/n64-collgridcell-t4-2026-10-07](functions/n64-collgridcell-t4-2026-10-07.md)).
- **Evaluation order**: the operand with the higher Sethi-Ullman need first,
  emitted in ucode order.
- **Two passes**: ugen runs each function twice; pass 2 starts its FP free
  list in the order pass 1 freed it, so FP names depend on operand order
  anywhere in the function ([log/n64-m2-session-2026-10-04](log/n64-m2-session-2026-10-04.md)).
- **Value cache**: ugen reuses a loaded value within a block. It is flushed at
  an empty-test boundary and by a store through an address register, so a
  later use reloads into a new temp ([functions/n64-aiscancorridor-wip-2026-10-07](functions/n64-aiscancorridor-wip-2026-10-07.md)).
- **Layout**: the then-arm is laid out first (block order = source order);
  polarity of `if` is visible.

## 7. as1: scheduling

- Ties break on the **source line**, lowest first: statements on one physical
  line can issue in reverse; a gbi macro is one line, the same block written
  one word per line schedules differently
  ([levers/n64-paintclick-locals-lineno-2026-10-05](levers/n64-paintclick-locals-lineno-2026-10-05.md),
  [functions/n64-imagestrip-wip-2026-10-06](functions/n64-imagestrip-wip-2026-10-06.md)).
  Read decisions with `cc -Wa,-R`.
- Hoisted code inherits the last `.loc` of its block.
- **Cross-block hoist (xbb)**: one instruction moves from a dominated block
  into a dominating block with idle cycles if their level difference is
  under 6 and the total cycles drop
  ([functions/n64-planeresolve-t4-2026-10-07](functions/n64-planeresolve-t4-2026-10-07.md)).
- **Dead code is deleted after scheduling.** Dead loads still shape the
  schedule around them before they vanish.
- `lui $at` is shared between stores only for symbols defined in the TU.
- Forever loops get `.align 5` before the epilogue; the function's offset in
  the object must equal the ROM's mod 32, so keep files in ROM function order
  ([functions/n64-musicthread-t4-2026-10-07](functions/n64-musicthread-t4-2026-10-07.md)).

## 8. Zero-footprint statements

Some ROM allocations need IR that emits no code. These constructs keep values
live, add blocks or renumber webs without adding instructions:

| Construct | Effect | Cost |
|---|---|---|
| Dead store `t = expr;` before first use | numbers expr's web earlier | none |
| Named value propagated away (`cx = ...;`) | renumbers tied webs | none |
| `(v | 0)`, `(v ^ 0)` | keeps a compare operand a node | none |
| Empty test `if (x);` / `if (x) {}` | keeps x live to that point; splits the block | +2 blocks, flushes ugen's cache, can move `.alias` |
| `x = load; if (x != x);` | one dead FP product, one ring temp | as above |

Precedents in the tree: BrPakManager's `if (D_8036A063 != 0);` raised one
web's live-block count to break a priority tie; BrAnimUpdate's
`j = a->nKeys; if (j) {}`; BrCrImpulseSolve's NaN check; BrAiScanCorridor's
compiled-out debug test that keeps five values live through one block so
uopt stops growing `depth`'s register into it.

Placement rules (BrAiScanCorridor,
[functions/n64-aiscancorridor-wip-2026-10-07](functions/n64-aiscancorridor-wip-2026-10-07.md)):
nothing between a compare and a store that reuses its loaded value (the cache
flush adds a reload); not inside a block where an address register's
noalias range must stay open; define the probe values where they must be
live and put the test in a block where its boundary harms nothing. Write these
as plausible compiled-out debug lines on real values, never on dummy data.

## 9. Data, symbols and linking

- Globals reached through one base register with small offsets are one
  struct symbol; separate lui/addiu per access are separate symbols.
- A global touched by one function only is a function-local `static`
  (separate %hi/%lo per access, never CSE'd); file statics are addressed off
  the section base ([levers/n64-loadsave-uopt-2026-10-05](levers/n64-loadsave-uopt-2026-10-05.md)).
- Pooled .rodata floats declared `extern float` are mutable to IDO (pointer
  stores kill them); write the literal.
- Two ROM loads of one address through different symbols (a member of an
  adjacent declaration) stop address CSE.
- String literals: file literal order = source order; the linker pairs
  HI16/LO16 for string pointers ([oracle/n64-a7-literal-mapping-2026-10-04](oracle/n64-a7-literal-mapping-2026-10-04.md)).
- Library code (libultra, zlib 1.0.4) matched from its own sources
  ([n64-zlib-lode-2026-09-29](n64-zlib-lode-2026-09-29.md)).

## 10. Giants: functions over -Olimit

The ROM's largest functions exceeded `-Olimit` and were compiled without uopt:
no strength reduction, no cross-statement CSE, locals in the frame, `register`
locals in s0, s1... in declaration order. Compile at the default `-O2`, never
a raised `-Olimit`; compile without `-w` to see the warning. Residue there is
ugen evaluation order and branch polarity, worked top-down register-blind
([levers/n64-giant-t3-method-2026-09-28](levers/n64-giant-t3-method-2026-09-28.md),
[levers/n64-giant-ugen-levers-2026-10-05](levers/n64-giant-ugen-levers-2026-10-05.md)).

## 11. Tools

| Tool | Use |
|---|---|
| `tools/tgrally/n64build.py FILE` | grade every function in a file (T4 gate) |
| `tools/tgrally/n64image.py` | whole-image gate: 0 bytes may differ |
| `tools/tgrally/n64t3.py` | live (A5) and whole-image (A7) oracles, T3 gate |
| `tools/tgrally/n64alloc.py` | instrumented uopt: trace, one-decision force (diagnostic) |
| `tools/tgrally/build_ido_trace.sh` | builds the instrumented compiler |
| `cc -K` | keeps cfe (`.B`) and uopt (`.O`) ucode and ugen assembly (`.s`) |
| `cc -Wa,-R` | as1 scheduling trace |
| n64-decomp-workbench capture | decode cfe/uopt ucode per source line |

See [toolchain/n64-ido-trace-tooling](toolchain/n64-ido-trace-tooling.md) for the
log fields and [toolchain/n64-ido-corpus](toolchain/n64-ido-corpus.md) for the
ROM-confirmed corpus of other IDO games.

Every per-function note is indexed in [functions/README.md](functions/README.md);
the session logs in [log/README.md](log/README.md).
