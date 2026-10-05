# Vc5 idiom dictionary

*Recorded 2026-09-03.*

> Byte-level VC5 codegen idioms proven during the 5-function timed test: byte-width return types, hoist order, shl distribution, and the analytical (not trial-and-error) method that pays.

**Measured 2026-08-22 during the timed 5-function test. Each entry was proven
by a byte diff, not inferred.**

## The method that pays: infer the SOURCE, don't permute spellings

`mov bl,al` (byte) where you emit `mov ebx,eax` (dword) after a call is not a
spelling problem - it means the original PROTOTYPE returned a char-width
type.  Changing `int32_t BrFixPackS6Q7Neg/Level/U8Range(float)` to `int8_t`
matched BrCarStatePack (738 B, the project's largest match) AND flipped a
helper to match for free.  When a byte diff looks like register/width noise,
ask what TYPE the original source must have used.

## Proven idiom facts

- **Flag-from-float**: `(f != 0.0f)` → fld; fcomp [0.0 const]; fnstsw;
  test ah,0x40; jne/xor-or-mov-1.  `? 0x80 : 0` picks the constant.
- **Hoist order across calls is source LEFT-TO-RIGHT**: subexpressions that
  must survive a call (flags into ebx/ebp, overflow into a stack slot) are
  hoisted in left-to-right source order.  To get flag A hoisted before flag B,
  A's term must appear first - or be a prior statement.
- **Shift distribution**: `(t9C*2 | t88) << 1` keeps a literal `shl` only if
  the inner scaled term is spelled as a SHIFT: `((t9C << 1) | t88) << 1`.
  With `*2` VC5 distributes the outer shift into lea-scales.
- **Accumulate stores**: `b[k] = x; b[k] |= y; ...` emits one store per
  statement with the value forwarded in a register (no reloads).  `t |= X;
  b[k] = t` puts the or result in t's register; `b[k] = t | X` puts it in
  al.  Operand order of `|` itself is canonicalized - flipping does nothing.
- **Direct struct reads vs locals**: a product with both operands read
  directly from a struct emits fld [mem]; fmul [mem].  A float local
  assigned from memory is copied via INTEGER mov to a stack slot and
  multiplied from the slot.  A function mixing both (squares from locals,
  cross-products direct) is visible in the bytes - transcribe the mix.
- **`(int)float` in VC5 is always a __ftol CALL** (truncation).  A bare
  `fistp` with no fldcw (round-to-nearest) CANNOT come from VC5 C - /QIfist
  does not exist (warning D4002).  Such code was inline __asm in the
  original: needs an asm-hybrid mechanism (none in tree yet).  First case:
  BrDlCmdVtx 0x10021A20.
- **Float-heavy DAG scheduling is the wall class, size is not.**  738 B of
  int/call-heavy code fell in ~10 min; a 165 B float function
  (BrVec3Project) is stuck on operand-selection (fld st5/fmul mem vs fld
  mem/fmul st) and a 405 B one (BrRbBuildMatrix, 400/405 bytes) on a
  spill-slot count (7 vs 6, fstp vs fst).  Statement reordering compiles to
  IDENTICAL bytes - VC5 canonicalizes the DAG, so these are internal
  allocator decisions, not source-reachable.  Related:
  [inlined-helper-match-class](../triage/inlined-helper-match-class.md), [divergence-class-triage](../triage/divergence-class-triage.md).

## 2026-09-03: the canonicalisation boundary, measured on the three giants

Twelve probes across 0x1000EAF0, 0x100250D0 and 0x1000A110 were **all
byte-identical** to the form already in the tree. VC5 canonicalises, so
never spend a probe on: commutative operand order (`&`, `|`, `+`),
relational operand reversal (`a>=b` vs `b<=a`), splitting one expression
into two statements, hoisting a single-def/single-use subterm into a named
temp in a pure-expression context, or reordering two adjacent independent
assignments. What those spellings CANNOT reach: which subexpression is
evaluated first (VC5 orders by subtree cost), which operand takes the r/m
vs reg field of a test/cmp, and which byte lane a value lands in
(`mov dh,al` vs `mov dl,al`) - all downstream of allocation. Named temps
still work where they change the def/use graph (a value used twice, a float
homed to a slot, a bound reloaded from memory).

**Tooling: `/FAcs` before probing anything in a multi-KB function.** One
compile emits a listing whose offsets are exactly the ones `divergence.py`
prints, so a region address greps straight to its source line; and just
above `_<Name> PROC NEAR` it prints every local's frame offset as
`_name$ = <offset>` - the recomp side of the slot-map work three dossiers
are grinding on. It immediately falsified br_drawcar.c's standing claim
that pack0/pack1 sat in fresh dwords (`_pack0$ = 8` - already in the reused
arg slots). Recipe in docs/VC5-IDIOMS.md. Do not infer slots from
displacement histograms across two builds with different frame sizes.

## 2026-09-03b: measure a spelling on ALL siblings that share one allocator
## decision - and know which siblings those are

0x1000EAF0 went 20 -> 19 masked regions on a lever its own dossier had
recorded as REJECTED. The rejection was real but partial: byte-offset
induction variables (`c2 << 4`, `+= 4`, ring reads/writes through `char *`
casts) had been measured on ONE of the two drain loops. One loop alone
genuinely is worse; both together gain a region and pull an unrelated
`mov edi,1` sink into place. VC5 picks one IV strategy per region, so a
half-converted pair leaves it straddling both and scores worse than either
consistent form. **Re-test any "structurally right but flips the allocation"
verdict this way before trusting it.**

The limit, measured the same day so it does not over-generalise: on
0x100250D0 converting ALL TEN remaining doubling sites to the ternary form
at once is WORSE, not better - it breaks a 1.7 KB byte-exact prefix. The
difference is what the siblings share. Drain loops sharing one induction
variable strategy = convert together. Independently allocated arms = one at
a time. Ask which allocator decision the sites share before batching them.

**Also: a falling RAW region count can be an alignment artefact.** That bad
tex3d probe dropped raw regions 45 -> 25 while breaking the prefix. Always
read masked regions AND the first-divergence address together.

Negatives banked the same session (do not re-run): a 3-float vector copy on
0x1000A110 is canonicalised into 3 integer loads + 3 stores by every
spelling - whole-struct assignment, dword pun on one member, three statement
orders, named float temps. The original's mixed x87/integer interleave is an
allocator state, not a source construct.

##  2026-09-03c: the progress metric was hiding a wrong CONSTANT

`divergence.py` normalises to "mnemonic + operand SHAPE with imm32
wildcarded", so its region count - the number every giant's dossier is
graded on - **cannot see an immediate-operand defect.** 0x1000EAF0's
ring-wrap test was `if (499 < head)` where the original wrote
`if (head >= 500)`: four sites emitting `cmp X,0x1f3; jle` against orig's
`cmp X,0x1f4; jl`. Semantically identical, so no behavioural oracle could
catch it either. The region count had scored all four as MATCHING through
eight passes of grinding. Fixing it: reloc-masked byte diff 3,855 -> 3,727,
instruction count to exactly 2,328 = 2,328.

Built `tools/msetdiff.py` for this - register-blind instruction-multiset
diff that normalises registers (32/16/8), esp displacements and relocs but
KEEPS small immediates. **Run it on any function that has stalled.** The
signature it catches: a region count that will not move while the
instruction count stays off. It also turned 0x1000A110's opaque 30 regions
into a concrete worklist (orig has 4 more `and R,0xff`, 4 more `or R,R`,
2 more `shl R,8`, 3 more byte-slot stores; ours 4 more dword-slot stores  - 
i.e. 3-4 values are BYTE locals in the original and dword locals here).

**Corollary, and it matters:** the fix RAISED the masked region count
19 -> 20 while making the function 128 bytes closer. Region count alone
would have rejected a correct transcription. On anything touching a
constant, rank by reloc-masked differing bytes and instruction count.

## Timed-test result (for pace calibration)

F1 60B match 2.5min; F2 165B stuck-at-30-diffs 7.5min; F3 405B stuck-at-
allocator 9min; F4 584B wall-identified 3min; F5 738B MATCH ~10min.
2/5 matched on a deliberately hard-skewed sample; both misses are documented
compiler-internal walls, not lack of effort.

## 2026-09-03: three EMITTER-level residues in one session (C++ lane)

Full entries with dead-probe lists are in `docs/VC5-IDIOMS.md`; the short
form, because these are recognition patterns you want before you start
probing:

- **SIB base/index on `member_array[index]`** - orig `8a 44 07 09`
  (base=`this`), ours `8a 44 38 09` (base=index). Same address, one byte.
  21 spellings across four axes + 8 flag sets: nothing. Our cl DOES emit
  base=pointer when the base is a materialised pointer VALUE
  (0x1006D000, 0x10054390), never for `this`+const. Cost: 0x100540D0 and
  0x10054280 both sit at exactly 1 diff.
- ~~**Inline `memset` setup order**~~ - **RETRACTED 2026-09-03 same day.**
  0x1006FCE0 and 0x100087D0 do differ, but 0x1003AB00 has the ORIGINAL
  emitting count-and-value before the dest lea - our order - and matches.
  So the setup order is scheduled against the surrounding code, not fixed
  by the expansion. Those two are an unexplained scheduling residue; do
  NOT cite them as compiler-build evidence.
- **Cross-jumping** - our cl tail-merges two byte-identical error blocks
  (`jge / push imm / jmp` into the sibling's `call`); the original keeps
  separate copies (0x10059350). This one is a whole optimisation ours
  performs and the original's did not.

A fourth candidate, weaker: 0x1003AB00's clamp, where the original emits
`sub / test / jge` and ours fuses to `sub / jns`. A scan of 943 matched
functions found our cl emitting sub-then-test exactly once, and there on a
POINTER - so the fused form looks like our rule for integers.

**Read this as a lead, not a verdict:** 1,045 functions match with the
staged cl, so it is not blanket-wrong, and one of the four original
data points (memset ordering) was RETRACTED within the session once a
counter-example turned up. Only the SIB and cross-jumping entries still
stand as source-unreachable. Anyone chasing the compiler patch-level lead
should start from those two - they are small, isolated, single-instruction
discriminators a candidate cl either reproduces or does not - and should
expect more of these to dissolve under a counter-example, as this one did.

## 2026-09-03: byte stores push narrowing UP (source-reachable, big)

Opposite lesson, same session. A byte destination makes VC5 truncate as
far up the expression as it legally can (through `|`, `&`, `<<`; not
`>>`), building every mask it reaches in 8-bit registers. Where it stops
IS a source decision worth 30+ diffs each: one cast outermost (never
inside), the byte pointer in its own local (or VC5 re-associates
`(v & (m << k)) >> k` into `(v >> k) & m`), and each mask that must stay
32-bit in its own `unsigned int` local. Counter-pressure: a fourth live
local tipped the function into an ebp frame. 0x1006D0B0: 153 -> 40 diffs.


## 2026-09-03 (third pass): the float wall is NOT a wall for straight chains

**x87 chains match when every intermediate is a NAMED float local.**
0x1003A580 (322 B, five __ftol conversions) went 205 -> 155 -> 0 diffs as
the names were added: name the int-to-float conversions, then name the
one product that is read again. Each name is what makes VC5 treat the
value as one definition with several uses, which is what produces the
original's `fld st(0)` (duplicate), `fst` without a pop (home a
thrice-read value) and `fsubr st(1)` (operate against the copy still on
the stack). Unnamed, VC5 re-associates and spills.
Signature of a missing name: `fld [const]; fmul st(1)` where the original
has `fmul [const]`, plus spill slots the original does not use.
Full entry in docs/VC5-IDIOMS.md. This does not repeal the tangled-DAG
wall (0x1000EAF0, BrVec3Project) but it is cheap and it should be tried
BEFORE calling any float function a coloring wall.

## 2026-09-03: arm order decides block PLACEMENT, and it is source-reachable

Repeatedly worth diffs this session. Rules observed:
- The arm you want placed LAST should be the `else`. Testing
  `if (flag == 0) { common } else { odd-one-out }` puts the odd arm after
  everything, which is what 0x100393C0's original does (26 diffs).
- For a sentinel/format pair, sentinel as the THEN arm and format as the
  ELSE gives [sentinel][jmp end][format][end] -- the original's layout
  (0x1003A580, 0x1003A420 both byte-exact this way).
- BUT once the sentinel arm is reached from TWO paths (a mode guard plus
  the value test), VC5 outlines the format block past the common tail and
  the inner `je` goes near. Four arm orderings measured on 0x1003A140,
  none reproduces the original's interleaving (parked at 237).
- How deep VC5 tail-merges sibling arms is allocator-driven, not
  source-driven: 0x100393C0's original merges 2 of 3 arms; source shapes
  that merge 0, 1 or 3 give 352 / 336 / 320 bytes against the original's
  330.


## 2026-09-03 (session 8): LOAD PLACEMENT IS VC5'S, NOT THE SOURCE'S

Two entries that are the same rule from opposite sides, both proven this
session and both worth 200+ diffs on their own function:

- **Do not CACHE what the original re-reads.** 0x10039870 reads the same
  table field twice, once per adjacent call; caching it in a local costs a
  spill slot (`sub esp,N+4`) and rotates the loop registers. 201 -> 0.
  Same shape in 0x10038F40, where an index expression appears twice
  because the original recomputes it after an intervening call.
- **Do not NAME a temp to preserve an observed load order.** 0x100400E0
  loads a global, stores 0 to a different global, then stores the loaded
  value. The temp that "preserves" that is wrong -- VC5 hoists the load by
  itself, and naming it moves the value out of eax and loses the
  one-byte-shorter `a1`/`a3` accumulator encodings. 10 -> 0. The port body
  had added exactly that temp.

**Rule:** only name a value when the ORIGINAL keeps it in a register across
something that would otherwise clobber it. Otherwise write the plain
member/global access as many times as the original reads it.

Hard direction, still open: 0x10041180 parks at 243 because the original
RELOADS a field the source just stored to, and there VC5's store-to-load
forwarding does the caching, so re-writing the access does not undo it.


## 2026-09-03 (lane d29628ed) - three entries added to docs/VC5-IDIOMS.md

1. **The commutative-float canonicalisation covers a WHOLE FLAT
   SUM-OF-PRODUCTS, not one add.** Eleven spellings of a 4x4 projection
   compile byte-identically: term order in the `+` chain, operand order
   inside each `*`, `(...)*r` vs `r*(...)`, a named numerator temp, the
   divisor inlined into the reciprocal, declaration order, and an array
   pointer local vs direct member subscripts. Only two things move the code:
   GROUPING (`a + (b + c)` is a different tree - a legitimate probe axis) and
   dropping the scalar locals, which lets VC5 re-CSE the loads. Never probe
   flat-sum order again.

2. **THE BOUNDARY on naming temps.** The "do not name a temp" rules above are
   about a value read once, or re-read after an intervening call. The
   OPPOSITE holds for a value BOTH arms of a branch need: compute it once in
   a named local ABOVE the branch. VC5 does not sink duplicated common code
   back out. Worth 33 bytes and the whole match on the sprite blit
   dispatcher (0x10001320) - inline in each arm 239 B / reggap 18+8, hoisted
   206 B / 85 insns / reggap 0.

3. **The absolute-store accumulator encoding is a byte-count diagnostic.**
   `mov [imm32],eax` is 5 bytes, from any other register 6; same for the load.
   A function a handful of bytes long at reggap 0 with several global
   accesses is an eax rotation, one byte per access - count them before
   probing anything. On 0x10059410 the "missing `xor eax,eax`" was the
   EFFECT of that rotation, not a source difference; the stores are already
   literal zeroes.

See also [unswept-tu-bookkeeping-class](../traps/unswept-tu-bookkeeping-class.md) - the two byte-exact matches of
this lane were bookkeeping, not idioms.
