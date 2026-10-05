# Oracle levers

*Recorded 2026-09-19.*

> Three reusable A5-oracle levers built certifying BrSnapInterpDraw 0x100131E0 T3 (commit 75cf22cd): overlay-cap (run a recompile that would bury a neighbour), integer-operand x87 ops (fisub&c), and per-profile stub_calls to black-box heavy identically-called direct callees. All sound (spurious DIFF, never false EQUIVALENT).

**BrSnapInterpDraw 0x100131E0 (3217 B) CERTIFIED T3 on 2026-09-19 (commit
75cf22cd).** It was the largest remaining non-T3/T4 after BrObjDlBuild. A
faithful transcription; byte residue is x87 scheduling (orig keeps the blend
fraction `t` RESIDENT in an x87 register via `fmul st(1)` across every
per-component LERP + reaches car fields through hoisted base pointers `lea`
once then `[reg+imm]`; ours reloads t and uses `[reg+abs]`). crank tried 102x
-> byte-exact is a wall. Certified behaviourally instead.

** THREE REUSABLE A5-ORACLE LEVERS (all committed, all sound -- a bad world
surfaces as spurious DIFFs, NEVER a false EQUIVALENT):**

1. **Overlay cap (tools/t3b_env.neighbour_after + t3b_verify).** The oracle used
   to REFUSE ("substituted bytes bury <neighbour>") when a recompile a few bytes
   longer than the original would, overlaid at va, bury the next function's
   entry. Now it CAPS the recomp overlay at that neighbour and runs. The overlay
   only serves in-.text jump tables, which live inside the function's own span
   before the boundary; a table in the recomp's extra tail that is capped away
   can only misdispatch into a DIFF/crash. This unblocks ANY faithful-but-bloated
   transcription (recomp > orig) -- a whole class the oracle previously punted as
   UNCLASSIFIED. Re-run `t3b_verify` on old "substituted bytes bury" parks.

2. **Integer-operand x87 ops in tools/x87emu.py: `fiadd/fisub/fisubr/fimul/
   fidiv/fidivr`** (read a signed 16/32-bit int from memory, widen, apply to
   st0). Mirrors `fild`. Any function doing `(float) * intmem` / int-to-float
   arithmetic in x87 hit "unhandled fisub" before.

3. **Per-profile `stub_calls` (oracle_profiles.Profile field; t3b_verify
   `_STUB_CALLS`; x87emu `stub_targets`/`dcalls`).** Black-boxes named DIRECT
   callees (eax<-0, esp net-zero cdecl), recording the TARGET sequence (NOT args
   -- arity unknown, same trap as indirect calls) as an observable. Use for an
   orchestrator that only HANDS a seeded slot to a heavy subsystem the
   transcription is not certifying (a renderer, a logger, a timer) which would
   loop/walk-into-noise on the seeded world. Both sides call it identically; the
   function's own outputs are still compared. Here: the wall-clock timer
   0x1006E280 and the 4500-B frame driver BrFrameDrawView 0x10011FA0.

** 4th lever (commit 58e6184a): the `_CIpow` x87 pow intrinsic.** A pow-using
function emits a reloc to `__CIpow` (thunk 0x100748A0 -> MSVCRT!_CIpow); the
oracle couldn't resolve it and refused to LOAD the recomp bytes (UNCLASSIFIED).
Fix: `_CRT_HELPER_VA['CIpow']=0x100748A0` (t3b_env) + `_model_cipow` DIRECT_BUILTIN
(x87emu): base st(1), exp st(0), result base**exp, domain errors->NaN/Inf,
deterministic (both sides same model -> never a false EQUIVALENT). Unblocks the
pow-shaped control-response class.

** 5th lever + BrCtlInputApply 0x1005AFF0 CERTIFIED T3 (commit 28283a33). The
x87 COMPARE-FLAG emulator bug -- and it masked a real inverted-compare
transcription bug.** THE LEVER: x87emu fcom/fcomp/fcompp only ever computed C0
(below); C3 (equal) and C2 (unordered) were NEVER set and fnstsw only wrote C0,
so every `fnstsw; test ah,0x40` (equal) read false -- the emulator thought floats
are NEVER equal. Its memory-operand decode was also wrong (`startswith('[')`
never matches capstone `dword ptr [..]`, so `fcom [m]` compared st0 with itself).
Fixed: set the full C0/C2/C3 trio from the ordered compare, fix the operand
decode, handle fucom*. **DEBUGGING LESSON (project lead was right, I was wrong twice):**
a 7/12-seed SYSTEMATIC divergence is NOT a 1-ULP precision knife-edge -- do not
hand-wave "80-bit x87" (x87 is 80-bit internally even on Win9x, but that was a
dodge). Both sides run the SAME 64-bit emulator so precision cancels; a real diff
IS a real bug. THE MASKED BUG: with C3 fixed, the oracle showed the orig's true
behaviour and caught that br_ctlinput.c line 263 was inverted -- `if (local[0] ==
local[4]) local[5]=0` must be `!=` (zero the steering target only when current-
steer sign and target sign DIFFER). The orig and our recomp branch OPPOSITE
polarity (orig `jne`, recomp `je`, because the orig has an `fxch` first), so the
always-wrong equal-flag sent them different ways -- looked like a scheduling/stack
mismatch, WASN'T. Fixed source -> EQUIVALENT 48 seeds, negative-controlled.
**Certified-sweep after the C3 fix: NO regression** -- the 5 DIFFs (BrTextEmitString
0x10015B10, BrCtlAiBody 0x1005D770, BrSub_100173F0 0x10014960, + the 2 parallel
Ext rows) all DIFF with the fix REVERTED too, so they are pre-existing (parallel-
session build state), not C3-exposed false certs; nothing flipped EQUIVALENT->DIFF.
My earlier "fst/fstp stack wall / operand-order wall / needs 80-bit" notes here
were WRONG -- deleted; it was the C3 emulator bug + one inverted `==`.

**Profile pattern (like [objdl-a5-limit-2026-09-16](objdl-a5-limit-2026-09-16.md)):** valid slot indices
for the snapshot ring, VARIED car matrices for teeth (a zero world blends 0->0
and hides a wrong-field LERP), pMat pointers NULL for deterministic retarget
arms, and INTEGER state (lock array 0x10396F10, counters) marked `exact_regions`
-- else the same float-masking false-EQUIV bug ([objdl-a5-limit-2026-09-16](objdl-a5-limit-2026-09-16.md)
bug 1) hides a 0-vs-1 lock-flag diff as denormal rounding (caught in the
negative-control: CTRL D slipped as EQUIV-MODULO-FP until the lock array was
made exact). ALWAYS negative-control every observable path
(feedback-outcomes-not-excuses, [oracle-runs-orchestrators](oracle-runs-orchestrators.md)).

** BrCarPhysDriveMatch 0x100645A0 CERTIFIED T3 (commit 3ea697a5) + NaN-MASKING
LESSON.** A faithful near-match (insn gap 1) certified EQUIV-MODULO-FP. Two
gotchas for the profile: (1) a param declared `int` but USED as a pointer
(param_1 = car ptr) is seeded as a scalar int -> derefs garbage; pin it via the
ARG hook to a scratch object graph. (2)  DEGENERATE SEEDS -> NaN OUTPUTS ->
FALSE EQUIVALENT: unseeded axle geometry made an axle-difference divisor
`[eax+0x78]-[ecx+0x78]` = 0/0 -> inf, then inf*0 -> NaN; the velocity outputs
were all NaN, and NaN==NaN compares EQUAL so EVERY bug hid. Fix = seed the whole
object with DISTINCT NONZERO floats (finite divisions) and pin divisor args
nonzero. ALWAYS check outputs are FINITE before trusting EQUIVALENT (dump the
written region; NaN/inf there = degenerate world, same class as the objdl
zero-vertex bug). Also: a multi-line C prototype breaks t3b_verify.parse_signature
(it reads one grep line, sees an unclosed paren) -> join it to one line
(codegen-neutral). Negative-control float outputs on the path that actually
produces nonzero output (here the g.ran velocity solve svB[0]->wld->car+0x84),
not a side branch that outputs 0.

---

** BrSndCarStep 0x10061470 (2757 B) CERTIFIED T3 (commit 9ccc5824) + FOUR
recurring-gotcha lessons.** A5 EQUIVALENT 48 seeds. The whole grind was
distinguishing a spurious DIFF (orig copyback wrote 0x168, recomp wrote
pCar+0xf5c) from a real bug -- it was neither code nor a clean seeding gap:

1. **UNRESOLVED-ICALL ESP-LEAK (emulator limitation, not yet fixed
   generally).** `x87emu` `model_unresolved_icalls` black-boxes an unresolved
   indirect call with esp UNCHANGED (cdecl assumption). A real __stdcall game
   callback cleans its own args (`ret N`); black-boxing leaks those bytes.
   Here BrSndPlayEx's mixer fired 4 such callbacks through NULL-voice+offset
   slots -> 0x20 esp drift. It bit ONLY the ORIGINAL, which addresses locals
   esp-relative (`[esp+0x2c]`); the recompile uses ebp-relative (`[ebp-0x2c]`),
   immune. So the SAME emulator drift showed as a spurious DIFF between the two
   builds. The `mov reg,[slot];call reg` IMPORT path already cleans stdcall
   args (x87emu:453); the generic callback path does not (arg count unknown).
   RESOLUTION when a heavy subsystem's unresolved callbacks leak: gate that
   subsystem OFF (don't seed it into its real path) -- it is not what you are
   certifying. Tell: orig esp-relative locals read a "stale" pointer while the
   recompile's ebp-relative locals are correct = look for an esp imbalance, not
   a transcription bug. Same shape as the C3-flag bug (emulator artifact that
   bites only one build's idiom).

2. **NaN-masking recurrence via an unseeded FRAME-TIME divisor.** g_BrAnimDt=0
   (unseeded) -> BrSndDoppler `dot/animDt` -> inf -> NaN return -> poisons the
   caller's `fVar10 *= car+0xf74` -> `(__int64)NaN`=0 into every engine-hertz
   slot -> ALL outputs 0/NaN -> false EQUIVALENT. Seeded animDt=1.0 (NOT 1/60:
   1/60 makes seeded position deltas supersonic -> Doppler goes negative ->
   clamped to the floor -> still 0). ALWAYS dump the observables and confirm
   FINITE, NONZERO, and SEED-VARYING before trusting EQUIVALENT.

3. **GLIDE vs D3D global addresses DIFFER.** g_BrAnimDt is 0x106e9d8c in the
   GLIDE build; the slice/d3d headers say 0x106c2cfc. Seeding the header
   address did nothing. Find the address from the ACTUAL glide callee's disasm
   (`fdiv dword ptr [0x106e9d8c]`), never from a #define that may be d3d-keyed.

4. **Fixed-point `(__int64)` global outputs must be exact_regions.**
   DAT_118eef48/60 (32.32 hertz ratio), eef54/6c (packed pair), 1184c454 are
   integer outputs; `exact_regions` accepts ABSOLUTE addresses (t3b_verify
   `_classify_diff` tests `base` against them). Without them a scale bug that
   zeroes the ratio is float-masked as rounding (proven: the 110/7->100000
   negative control was EQUIV-MODULO-FP until eef48 was made exact, then DIFF).

Tag/filing gotcha: `@t3-measure` needs `rows N+M` (orig+recomp row split from
`--qualify` A2, here 35+58) in the exact order `bytes A/B insns C/D rows E+F
regions G oracle W`; a single `rows` number or swapped regions/rows makes
fileaudit flag the tag bad/stale. t3.py A2 shows `[superseded by A5
EQUIVALENT]` -- the A5 path is first-class, but Gate B's stale @t4-pass ledger
still "fails" --qualify; the @t3 tag itself is what fileaudit validates.
