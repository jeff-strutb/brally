# Matching progress

*Recorded 2026-08-20.*

> Matching decomp progress - 121/810 functions match byte-identical. Key patterns, compiler flags, and workflow documented.

**Status (2026-08-20, end of session):** **121 / 810 measurable**, verified by
a full sweep plus per-function objdiff. The sweep is the number to plan from;
a few hand-verified matches are not scoreable by the tool (statics, glide-tagged
twins) so hand counts run slightly ahead -- see [match-tooling-gotchas](../traps/match-tooling-gotchas.md).
810 is the real denominator; 812/836 were older counts.

A further 51 are UNMEASURED: 21 tagged against addresses never extracted from
the original, 30 that never appear in the object. That second bucket was read
as "folded into callers, or the tag's name is wrong" -- there is now a THIRD
cause: correctly-written functions the scorer structurally could not see. See
[match-tooling-gotchas](../traps/match-tooling-gotchas.md). Re-sweep before trusting the 30.

Run `python3 tools/match_sweep.py` for the full count -- it compiles every
tagged file at /O2 AND /Od, takes the better per function, and writes
`build/match/report.csv`. Of the 84 matches, 71 are /O2 and 12 are /Od.

**CONFIRMED TWICE (2026-08-20): the no-cascade law is real.** Laying BrDriverCar
out at its true 0x2B68 offsets -- a correct, compiler-asserted fix to a struct
whose ~281 field accesses were ALL at wrong displacements -- unlocked exactly
ONE function (BrRaceCarPre). The rest of the racing cluster stayed put:
br_race 0/1, br_racebegin 0/7, br_racestep 1/4. Necessary, not sufficient,
exactly as the BrUiCtl_ packing fix was. **When recommending a structural fix,
do NOT justify it by predicted cascade** -- justify it as removing an error
class that blocks those functions from EVER matching, and expect +1.

**LESSON (2026-08-19): systematic fixes do not cascade the way you expect.**
BrUiCtl_ had 42 of 70 fields at wrong offsets -- one 10000-byte dword array at
+0x12A (a 2-mod-4 address) that natural alignment bumps 2 bytes, dragging the
whole back half of the struct with it. `#pragma pack(1)` around THAT STRUCT
ALONE fixed 39 of the 42. Provably correct, port stayed 136/136 green -- and
it moved the match count by exactly ONE (83 -> 84). ~92 functions touch that
struct and still differ: each carries several INDEPENDENT divergences, so
removing one error class only completes a function where it was the last one.
Expect necessary-but-not-sufficient. Do not predict cascades.

Do NOT widen that pragma to a global /Zp1: it also strips BrTextBox's genuine
internal padding (0x438 -> 0x434) and aText[3] then pulls everything after it
12 bytes short. Global packing LOOKS better (42 failures -> 20) and is wrong.

**Byte-diff counts are a bad ranking metric.** Once sizes differ, one inserted
instruction near the top desynchronises everything after it, so a 95%-correct
function reads as "234 bytes wrong". It measures how EARLY the first
divergence is, not how much is wrong. `tools/objdiff.py` (needs `.venv`) gives
the real instruction-level side-by-side and diagnoses a function in seconds --
that is the working tool, and it already existed. Do not build a ranker.

**Compiler flags:**
- `/O2 /Oi` for optimized functions (majority)
- `/Od` for stubs/nops/unoptimized wrappers
- VC5 cl.exe 11.00.7022 via Wine 11.0

**Most productive fix pattern: direct global stores.**
The port routes many globals through accessor functions (BrTextGetState,
BR_PHASE_CUR, g_pExt->field). The original stores/loads at hardcoded
addresses. Declaring raw externs and writing directly matches.
Functions fixed this way: BrTextFlag358Clear, BrSub_10019280,
BrSub_10019290, BrTextSetSize. Pattern applies to ~50+ more functions.

**x87 OPERAND SELECTION IS CONTROLLABLE -- via expression SHAPE.** (2026-08-20)
VC5 canonicalises commutative fmul/fadd operands, so swapping `a * b` to
`b * a` changes NOTHING, and neither do /Op, /Oa, /Ow, /O1 or /Ox (all tested).
That led to a WRONG conclusion that operand choice was compiler-internal and
unreachable. It is reachable -- what decides which value is FETCHED (`fld`)
versus used as the fmul/fadd memory operand is the shape of the expression:

- Name the value you want fetched in a local temporary, then use it:
  `float m = pM->m[k][i]; o[i] += m * v[k];`  -- matched BrMat4MulVec3Transposed
  `float bz = pB->z; pOut->z = (bz + pA->z) * 0.5f;` -- matched BrVec3Midpoint
- Consume a freshly-computed value INLINE to keep it live on the x87 stack:
  `pOut->x = (r = 1.0f / s) * pV->x;` gives the original's `fld st(0); fmul mem`
  for the first term while later terms take the settled `fld mem; fmul st(1)`
  form. Hoisting it to `float r = 1.0f/s;` loses the first term. -- BrVec3Div

Also: the original often does NOT use a consistent order across x/y/z. Read
each term's fetch order separately rather than assuming uniformity.

**Other patterns that produced matches:**
- **CRT float call -> intrinsic double form.** The `f`-suffixed math functions
  (`sqrtf`, `sinf`, ...) are real library calls in VC5 and are NOT on the
  intrinsic list; only the double forms are. Where the original shows the bare
  x87 op inline (`fsqrt`, `fsin`) with no call, write `(float)sqrt(x)` under a
  scoped `#pragma intrinsic(sqrt)` / `#pragma function(sqrt)` pair. Matched
  BrSqrtF; expect it to recur on every float-suffixed math call.
- float suffix on constants (`0.5f` not `0.5`) - d8 fmul vs dc
- `uint32_t` loop counters instead of `int` - jb vs jl
- Calling 3-arg functions directly instead of 2-arg wrappers
- Plain casts instead of range-checked wrappers (BrFtolArg)
- Buffer size corrections (BR50_DPMSG_SIZE 0x3F8 -> 0x400)

**WHAT ACTUALLY WORKS -- structural patterns, in yield order (2026-08-20).**
All of these fully resolve. See [divergence-class-triage](../triage/divergence-class-triage.md) for how to spot them.
1. **Port threads a state pointer where the original uses fixed globals.** Drop
   the parameter, declare raw externs at the documented addresses, use directly.
   Struct field comments carry the addresses. Yielded ~9 matches in one round
   (BrGbiTexScan family, BrRcaResetCounts, BrGbiSet* setters).
2. **Port factored out a helper the original inlines.** Write the body out in
   each caller. Yielded 6 (BrUiHook85 mode/kind hooks).
3. **thiscall** -- see [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md).
4. **Plain casts instead of float wrappers.** `BrFtolTrunc(x)` pushes an integer
   arg; `(int32_t)x` leaves it on the x87 stack and calls MSVC's own __ftol,
   which is what the original does.
5. **Unchecked jump table.** A C `switch` always emits a cmp/ja guard; tail
   dispatch through a function-pointer table gives the bare indirect jump
   (BrAppFrame).
6. **Naming a value that is loaded once** -- but read the blast-radius warning
   in [divergence-class-triage](../triage/divergence-class-triage.md) first.

**Known blockers for remaining functions:**
- ~~Register load order (~35 same-size) - compiler quirk, can't control~~
  **PARTLY WRONG**: x87 operand selection IS reachable via expression shape
  (see above). Which *register* a pointer parameter lands in still resists
  (BrVec3Dot), but re-test that class before calling anything immovable.
- ~~`__thiscall` (~22 functions) - needs C++ compilation~~ **WRONG, see
  [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md).** Single-argument thiscall is reachable from
  plain C with `__fastcall`, and the two-argument case has a trick too.
- Accessor vs raw global (~50+) - systematic, each needs extern + test fixup
- Size mismatches - C structure differs, per-function asm-guided work
- **STRUCT OFFSETS GATE WHOLE FAMILIES.** BrDriverCar was laid out at true
  0x2B68 offsets (done, compiler-asserted). **BrMenuItem is the next one and is
  the highest-value open item**: the caption family stores at +4 where the
  original stores at +0x1E20C. Four functions are already size-exact and
  blocked on nothing else. Same shape of job as BrDriverCar was.

**Test fixup pattern for direct globals:**
When a function changes from accessor to direct global, tests that link
that module need: (1) storage for the global in the test, (2) any test
that previously checked the struct field now checks the raw global.
Watch for cascade: test_br_imgblit links slice6_78, test_slice5_61
links slice5_61, etc.

**How to resume:**
```
cd /Users/jeffreywilbur/projects/strutb/brally
sh build.sh && sh tools/regress.sh   # verify port (136/136)
# Full matching recompile + count:
sh build_match.sh  # or the manual loop below
```

**Parallelizable:** yes, each function/file is independent. Multiple
sessions can work different files. Avoid editing the same file.

Related: [matching-decomp-pivot](../rules/matching-decomp-pivot.md), [msvc-version](../toolchain/msvc-version.md)
