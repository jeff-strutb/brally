# Divergence class triage

*Recorded 2026-08-27.*

> Rank match candidates by KIND of divergence, never by byte-diff count - structural is reachable, register allocation is not.

**The single highest-leverage rule learned so far.** Same work, same tools, same
session: one round produced 1 match out of 10 attempts, the next produced 10
out of 10. The only thing that changed was how candidates were chosen.

**Do NOT rank by byte-diff count or by "closest to matching."** The functions
with the fewest differing bytes are near BECAUSE every reachable error class has
already been removed from them. What is left is the compiler's own register and
scheduling choices, which source cannot steer. Sorting by closeness sorts for
the hardest cases.

**Rank by divergence kind instead**, read off `objdiff.py`:

| Signal | Class | Verdict |
|---|---|---|
| `orig_size != recomp_size` | structural - wrong shape | TAKE IT |
| Original loads `[ecx+N]`, ours `[esp+4]` | thiscall | TAKE IT - [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md) |
| Original `mov [abs]`, ours `[reg+off]` | port threads a state pointer | TAKE IT |
| Ours calls a helper, original has the body | port factored what the original inlines | TAKE IT |
| Sizes equal, only which register differs | register allocation | DOCUMENT AND MOVE ON |
| Field displacement wrong | struct layout | TAKE IT, but it is a whole-struct job |

**Register allocation is a hard wall.** Hit ~19 times across three sessions. It
resists operand order, named temporaries, and every /O flag tested (/Op, /Oa,
/Ow, /O1, /Ox). When the ONLY remaining difference is `eax` vs `ecx` or `edi`
vs `esi`, stop: two attempts maximum, write the finding in the source so nobody
re-derives it, move to the next function. Grinding here is the main way rounds
get wasted.

**One exception, worth trying once before giving up:** if the divergence is a
REDUNDANT RE-LOAD rather than an outright different register, naming the value
in a local fixes it - the original loads once and reuses. Matched
BrGbiSet4C1694, BrMat4MulVec3Transposed, BrVec3Midpoint, BrVec3Div this way.
Caution: it has a blast radius - once it changed register allocation in two
NEIGHBOURING functions in the same file and cost two matches net. Always
re-sweep the whole file after using it, never just the target.

**Register-allocation walls are a BATCH awaiting a PERMUTER, not a permanent
loss (2026-08-26).** These functions ARE eventually in-scope (goal = 100%
bit-exact), but hand source-permutation is the WRONG tool and is proven
exhausted on them (33 minutes went to confirming BrTex3dRegister 0x10028BB0 is
first-region graph-coloring of a 7-value web - no C lever moves it; see
[ghidra-pipeline](../toolchain/ghidra-pipeline.md)). The RIGHT tool is the SM64-style decomp permuter:
randomized source mutation that compiles-and-scores thousands of
coloring-equivalent spellings looking for one that colors like the original.
So: when a function is CONFIRMED an allocator/scheduler wall, tag it, record
the specific coloring evidence, and BATCH it - do not hand-grind.

**PERMUTER BUILT + TESTED 2026-08-26 - basic version does NOT crack these
(negative result).** `tools/permute.py` (SM64-style: mutate liveness/temp-
count, compile MSVC5, score, anneal) was run on all 19 coloring-wall near-
misses. ZERO cracked; the closest 5 held at 2-10 diffs after 150-220 unique
compiles each. The mutation set (intro/inline temp, split/merge locals,
reassoc, stmt reorder, hoist/sink, width, cmp-flip, loop shape, block scope,
if-invert) does not reach these register webs. Next iteration needs RICHER
mutations: identity ops (+0/*1), if(a&&b) guard nesting, address-taken locals.
Do NOT rebuild the permuter from scratch - extend its mutation library.
The one "match" (0x10039E10) was a static-vs-extern storage-class artifact on
g_menu, not a coloring win. Tool committed as the foundation.

**EXTENDED PERMUTER - DEFINITIVE VERDICT 2026-08-27.** permute.py grown to
1719 lines with the coloring mutations (identity +0/*1, addr_taken, guard_nest,
first_live, recompute, store_swap, self_assign, live_merge, split_add). Result:
1 crack of 16 near-misses (0x1003CC00 BrUiOptHook, 12→0 via split_add twice  - 
output is mangled identity-op noise, not worth hand-porting for 93B). **The BIG
allocator walls are CONFIRMED NOT permuter-reachable:** BrTex3dRegister 1219→1152
(nibbled 47/1755, edx-vs-esi first-region coloring did NOT flip), BrRcaFixup
1169→1144, BrCarWheelFx 1147→1217 - a 7-value first-region graph is not
reachable by C-level live-set noise in ~120 compiles. The 2-diff/4-diff walls
(BrFixUnpackS6Q7Neg etc.) held at 555-839 compiles = leftover instruction-choice
/ x87 spelling, NOT coloring - also not permuter-crackable. **CONCLUSION: source
permutation cannot crack the first-region coloring class. These big functions
(BrTex3dRegister, BrRcaFixup, BrCarWheelFx) need link-time/hand-asm or accept as
unmatchable-in-C - STOP investing search effort.** Mutations worth folding into
_refine_candidates: binop_ident (x*t→x*(t+0), cracked 0x10007D50 24→16),
reassoc:swap:+ (0x100316D0 24→8). split_add already weighted; cmp_flip already in.
Matching is all-or-nothing per function, so a walled function contributes 0
matched bytes whether we grind it or not - there is no partial credit to chase.

** AMENDED 2026-08-28 - read [register-rotation-is-a-symptom](register-rotation-is-a-symptom.md) BEFORE
applying the conclusion above.** The "coloring wall, stop investing" verdict
was reached TWICE on 0x100250D0 and was WRONG both times: the rotation was
downstream of a structural source defect (Ghidra's counter-fold), and fixing
it took the function from +1152 to +512 bytes. The verdict above still holds
for a function whose REGISTER-BLIND multiset gap is already near zero - that
is what "coloring" actually means. It does NOT hold on the strength of a raw
diff count, which is inflated by exactly the rotation you are trying to
classify. Re-triage any function retired as "coloring" without a register-
blind measurement (BrTex3dRegister, BrRcaFixup, BrCarWheelFx were all retired
that way). Measure with [fnmatch-harness](../toolchain/fnmatch-harness.md) `regnorm` mode.

Related: [matching-progress](../log/matching-progress.md), no-token-thrashing, [ghidra-pipeline](../toolchain/ghidra-pipeline.md),
[register-rotation-is-a-symptom](register-rotation-is-a-symptom.md), [fnmatch-harness](../toolchain/fnmatch-harness.md).
