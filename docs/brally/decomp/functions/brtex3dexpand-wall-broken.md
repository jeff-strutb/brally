# Brtex3dexpand wall broken

*Recorded 2026-09-21.*

> SUPERSEDED 2026-09-16 - 0x100250D0 BrTex3dExpand is CERTIFIED T3 (br_tex3d_expand.c); the byte-grind here (630->88 shapes etc.) is moot, A5 supersedes byte-shape. Keep only for the T4 mechanism lessons (counter-fold, phi role-swap, LICM) and tools/fnmatch/sites.py.

 2026-09-03: EVERY REGION COUNT IN THIS NOTE AND IN THE FILE HEADER
WAS MEASURED ON TWO THIRDS OF THE FUNCTION -- divergence.py lost sync at
orig+0x15b8 and stopped. Real map: 31 regions at --key 10, 52 at --key 6,
largest reliable block -50 at 0x1a4c. See [lost-sync-region-trap](../traps/lost-sync-region-trap.md).

**2026-08-28 (14 parallel workers over two runs).**
0x100250D0 BrTex3dExpand, `src/core/drawing/br_tex3d_expand.c`, 8480 B.
Commits `51853cb`, `1ec54af`, `be0009a`, `2137308` (+ `d088b47`, `14025e7`,
`78c9d3f` tooling/docs). 22 parallel workers over three runs.
`@implements` still OFF.

|                            | was   | now      |
|----------------------------|------:|---------:|
| true code bytes (orig 8480)| +1152 |  **-9**  |
| true insns (orig 2407)     |  +234 |  **+6**  |
| first divergence           | +0x11 |**+0x14** |
| register-blind gap (E+M)   |432+198| **80+65**|

(Raw scorecard reads 8480/+0 B and 2422/+15 insns; 9 of those bytes and 9 of
those instructions are trailing `nop` .obj alignment pad after `ret`.)

**SIZE IS SOLVED; SHAPE AND ALLOCATION ARE NOT.** Do not read "size matches"
as "nearly byte-exact" - measured honestly:

| measure | state |
|---|---|
| true code size | 8471 vs 8480 (−9 B) |
| instruction SHAPES correct (register-blind) | 2262 / 2407 = **94%** |
| exact instruction sequence (runs >= 4) | **~30%** |
| bytes identical | 5.7% (first divergence +0x14) |

The gap between 94% of shapes and 30% of runs is FINE-GRAINED REGISTER AND
SLOT ALLOCATION. The big role rotation IS fixed - `cmp edi,ebx` is now 0
occurrences, every budget check is `cmp edi,ebp` like orig, and esi=pOut,
ebp=cbMax, edi=count all match. What remains is per-instruction scratch
choice and which local lands in which frame slot.

Two IDX4 arms are now instruction-for-instruction identical to orig, differing
only in the global `esi = pOut` register rotation.

## SESSION 3 (2026-08-29, by hand, NO workers - project-lead directive)

Retranscribe-not-patch applied (`8073620`): I8 arm rewritten from the bytes
(arm flip + distinct group counters + bound memory-homed via assigned
param_9). Raw gap 698 -> 559, insns 2406, in-order alignment 80%.
**NEW MECHANISM: assigning to a parameter memory-homes it** - that is how
orig gets its in-loop bound reloads; distinct from slot-name chasing.
**NEXT LEVER (precise, measured): the frame-slot map is exactly ONE dword web
short in the blend group bodies** - adding one anywhere jumps first-div
+0x14 -> +0x27 with the frame intact, but the uVar19 vehicle costs +5 insns
and two webs overflow the frame. Find the right vehicle (orig homes inten
widened at [esp+0x38] AND deltas at [esp+0x2c]). Details: docs/idioms-A.md.

## SESSION 2 UPDATE (2026-08-29): two MORE Ghidra artifacts broken

Commits `fb37704`, `1aa0a32` (+ `c5e94d2` permuter integrity, sites.py in
`fb37704`). Register-blind gap now **42+46=88** (was 145 at session start,
630 originally); insns 2403 vs 2407; first-div +0x14; frame intact.

1. **The outer puVar9/puVar21 phi is ROLE-SWAPPED** (fb37704): orig writes
   epilogues through pOut directly, NO head `puVar9 = puVar21` copy, NO latch
   `puVar21 = puVar9` commit. Deleting them freed eax and snapped the whole
   outer-loop head to orig's shape (addressing folds, no preheader jmp, aTile
   in ebx). Also `iVar10 = param_9;` hoisted ABOVE the empty guard (orig homes
   lod to a slot between cmp and jge).
2. **Blend-arm entangled combo** (1aa0a32): for-with-top-break (blocks LICM
   hoisting the base/delta pairs -> register-fresh imuls) PLUS dropping the
   widened uVar19 temp (multiplies read the byte local; VC5 CSEs the widening)
   -- each alone REGRESSES, combined they win. Also: iVar16 not param_9 as the
   byte-copy counter (param_9 made VC5 strength-reduce the walking pointer).

**GRIND PAUSED 2026-08-29 for token budget.** ~88 shapes left + the unsolved
latch-placement lever (orig puts the row-loop bound reload at the back-edge
target with an entry jmp over it; no C spelling found yet -- qc probed and
failed, read tasks/w5r7cz3w8.output). Permuter (FREE, local CPU) restarted
from the new base, 8 workers, 10h window. Resume the worker grind only when
budget allows: regenerate the worklist first
(`python3 tools/fnmatch/sites.py build/match/orig/0x100250D0.bin
build/match/t3d/v_<tag>.obj BrTex3dExpand 0x100250D0`).

## SESSION 4 (2026-09-01, by hand)

Masked divergence regions (`tools/divergence.py --mask-slots`, /O2 obj)
49 -> 32; 8464/8480 B; insns 2415/2407. Commits d6b63e2, 354c0e5, 9b3f109,
86a8392. Levers (all in docs/idioms-A.md session-4 + file header):
IDX4 width = reused param_9 (dead arg slot 0x9c); the doubling ternary had
REGRESSED to if-form at all 18 sites (IDX4 pair alone closes the IDX4 tail;
CI4 + next-arm pairs land only after the CI4 loops are in for-init form; any
further pair flips the allocation); group loops are `for (ctr = 0; ctr <
width;)` with the zero as the for-INIT in EACH arm (VC5 hoists the inits,
guard keeps `cmp ebx,eax`) -- closed the CI4 arm. Open: +0x2b sink order,
CI8 row head (int/pointer local still folds, param_9 flips), blend arms
(byte temps' dead-param-slot packing). Report row's O2y variant is STALE:
the function is plain /O2.

## The crack: Ghidra's COUNTER-fold, not the pointer

Ghidra merges two consecutive `count += N` into one `count += 2N` and retests
the first guard against the pre-value. It prints
`*p = A; if (c + 2 >= b) EXIT; c = c + 4; p[1] = B; p = p + 2; if (c >= b) EXIT;`
for a source that reads
`c += 2; *p = A; p += 1; if (c >= b) EXIT; c += 2; *p = B; p += 1; if (c >= b) EXIT;`
 -  counter bumped BEFORE each store, pointer advanced by ONE element per store,
one budget check per store on its own control edge. Semantically identical
(the counter on the first exit path is dead; that path returns).

The folded form lets VC5 batch the pair (`mov [r]; mov [r+2]; add r,4`), which
drops pOut's register pressure, frees esi, and rotates the allocation across
the whole function. Applied at 15 of 16 sites.

##  The prior session's framing was WRONG - do not restore it

The old note said the remaining work was "ONE lever: make VC5 emit true `*p++`
stores... an insight sub-wall, not a grind." **The lever was never
the pointer, it was the counter, and it was mechanical.** Likewise the
"coloring wall, do not grind" verdict fell twice. See
[register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md) - that is the transferable lesson.

## Also landed (four independent classes)

1. Colour channels are `unsigned char` locals packed through an
   `unsigned short` LVALUE (the lvalue width narrows the whole expression;
   `(unsigned int)` casts cost `and 0xff` + `xor dx,dx; mov dl,al`).
2. `FUN_100271f0` takes `unsigned short` - a wider prototype costs 32 B of
   zero-extension over 8 call sites.
3. Group-loop bounds are `if (ctr >= bound) break;`, not Ghidra's flipped form.
4. Three of six mask arms are written odd-path-first; the other three were
   measured correct as printed. MEASURE each one.
Plus `if (param_9 >= param_10) return;` before the pOut/cbOut/aTile sinks
(this moved first-divergence +0x11 -> +0x14).

##  CORRECTION to the old record

The "confirmed transcription fix `iVar17 -> param_9`, -3 insns" is **FALSE**
post-transform: it costs +16 B / +4 insns. MSVC 5.0 packs ordinary locals into
dead parameter slots, so Ghidra's `param_N` scratch names are slot
coincidences. Every slot-chasing rename measured worse. Retires the class.

## Also landed after the first commit

- **8-bit-output arms**: the counter-fold in its 1-byte form, walking a real
  byte pointer (`+= 1`, not `+= 2`), plus pre-biased read pointers in the
  mirror blocks (orig `lea eax,[esi-1]` / `dec eax`). -320 B.
- **IA8 arm**: puVar9/puVar21 ping-pong collapsed to one pointer; low nibble a
  named local; nibble merge from ONE widened value. -112 B.
- **4-texel group loops are `for`, not `do{}while`+break** - the do-while form
  makes VC5 peel the break test into a guard AND tail-duplicate it at the
  latch, 2 insns a site.
- Several twin-counter collapses (two Ghidra names for one variable).
- Six of nine mask arms want the odd-path-first flip; three do NOT. Measure
  each one - it is not a blanket rule.

## Also landed in the endgame pass

- **The copy-back preamble is a TERNARY, not an if/else.**
  `iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;` - Ghidra prints
  `iVar5 = iVar17 * 2; if (param_7 == 0) { iVar5 = iVar17; }`, which makes VC5
  hold the width in a stack slot and reload-and-add; the ternary keeps it in a
  register and emits orig's `lea eax,[ebx+ebx]` self-add. 9 sites, -64 B.
  Orig's doubling count went 10/18 -> 18/18.
- `cVar1` was not a local: orig re-reads `*(int *)(iVar5 + 0x20)` at each use.
- The four channel-delta computations sit NEXT TO their uses, not hoisted.

## Remaining: 145 register-blind shapes (6% of 2407)

| | |
|---|---|
| ~25 | extra stack accesses (`mov [esp+S],R` / `mov R,[esp+S]`) |
| -6  | 8-bit `or B,B` - I4-blend arms still widen the nibble merge; orig does it entirely in byte registers through byte slots ([esp+0x13] -> [esp+0x48]) |
| -3  | `imul R,R` - orig folds nothing from memory into imul |
| ±4  | one indexed-vs-walking load site; store scheduling (`[R-I]` vs `[R]`) |

##  THE NEXT CONCRETE LEVER (frame layout, and it is measurable)

Frame slot inventory: orig uses 27 local slots, recomp 26, total esp
references 646 vs 660 - nearly identical. The ONLY structural difference:

    orig-only slots : 0x12, 0x13   (two BYTE locals, packed in one dword)
    recomp-only slot: 0x10         (one dword)

So orig has two `unsigned char` locals where the tree still has one int. That
is the I4-blend 8-bit nibble merge (orig does it entirely in byte registers
through byte slots: `mov al,[esp+0x13] ... or dl,al ... mov [esp+0x48],dl`;
orig has 15 8-bit `or B,B`, recomp 9). Fixing it should also shift the slot
assignment toward orig's, which perturbs displacement bytes everywhere - this
is the highest-leverage remaining item, not a 6-instruction detail.

Untried lever, measured once and shelved: drop the widened `uVar19` temp so the
four channel multiplies read the `unsigned char` intensity local directly
(`bI4inten * iVar16`) and let VC5 CSE the widening. Structurally exact and
better on bytes/insns in an earlier pass, but it grew the frame 0x68 -> 0x78
then. Re-test - the arms have changed a lot since, and the frame budget has
9 bytes of slack now.

Already measured byte-identical here (do NOT re-run): a distinct byte local per
loop body; `(unsigned char)` cast present vs absent; `uVar19 = bI4inten` vs
`uVar19 = (unsigned int)bI4inten`.

Full dossier incl. all measured negatives: `docs/idioms-A.md`.
Idioms: `docs/VC5-IDIOMS.md`. Harness: [fnmatch-harness](../toolchain/fnmatch-harness.md).
Resume: `continue BrTex3dExpand; read docs/idioms-A.md, score with
sh tools/fnmatch/vdiff.sh <tag>`
