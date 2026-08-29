# 0x100250D0 BrTex3dExpand — two walls broken

`src/core/drawing/br_tex3d_expand.c`, 8480 B, 2407 insns. Second/third
largest function in `BRGlide.dll`.

## State (2026-08-28)

|                          | 2026-08-26 | 2026-08-27 | 2026-08-28 |
|--------------------------|-----------:|-----------:|-----------:|
| bytes (orig 8480)        |      +1152 |      +1152 |    **+64** |
| insns (orig 2407)        |       +234 |       +234 |    **+24** |
| first divergence         |      +0x07 |      +0x11 |  **+0x14** |
| register-blind gap (E+M) |    432+198 |    432+198 | **101+77** |

12 of those 24 instructions and 12 of those 64 bytes are trailing `nop`
padding after `ret` — .obj alignment, not code. **The real gap is ~52 bytes
and ~12 instructions**: 94% of the byte deficit closed in one session.

`@implements` stays OFF. Two IDX4 arms are now instruction-for-instruction
identical to the original, differing only in the global `esi = pOut` register
rotation.

**Both historical verdicts on this function were WRONG.** The "insn-3 coloring
wall, mechanically impossible, do not grind" verdict fell on 2026-08-27 to a
control-flow restructure. The follow-on "it is ONE lever, the `*p++` store
spelling — a Fable-shaped insight sub-wall" framing was also wrong: the lever
was never the pointer, it was the **counter**, and it was mechanical.

---

## Wall 1 (broken 2026-08-27): the prologue, insn 3

Write the lod loop as a `for` whose bound is the RAW parameter, with the
`lodEnd` local deleted:

```c
for (iVar10 = param_9; iVar10 < param_10; iVar10 = iVar10 + 1) {
```

VC5 hoists a `for`/`while` bound that is a raw parameter into a scratch
register at LOOP SETUP — `mov ecx, [esp+0x90]` at orig+0x7, before the pushes,
while eax still holds param_4. A `do-while` references the bound only at the
bottom latch, so its top guard loads param_9 lazily into edx instead. Use
`iVar10 < param_10`, not `param_10 > iVar10`: the first emits `cmp eax, ecx`
like the original, the reversed spelling emits `cmp ecx, eax`.

The old "topologically impossible" refutation was right about LOCAL RENAMING
(VC5 substitutes per-web; a single-def single-use prologue web always
collapses) but a control-flow restructure changes the web graph itself.

## Wall 2 (broken 2026-08-28): Ghidra's counter-fold — the big one

**Ghidra merges two consecutive `count += N` updates into one `count += 2N`
and rewrites the first budget test against the pre-value.** What it prints:

```c
*puVar21 = A;
if (iVar22 + 2 >= cbMax) { return; }
iVar22 = iVar22 + 4;
puVar21[1] = B;
puVar21 = puVar21 + 2;
if (iVar22 >= cbMax) { return; }
```

What the original source says:

```c
iVar22 = iVar22 + 2;
*puVar21 = A;
puVar21 = puVar21 + 1;
if (iVar22 >= cbMax) { return; }
iVar22 = iVar22 + 2;
*puVar21 = B;
puVar21 = puVar21 + 1;
if (iVar22 >= cbMax) { return; }
```

Counter bumped BEFORE each store, pOut advanced by ONE element per store, one
budget check per store on its own control edge. The two forms are semantically
identical — the counter value on the first exit path is dead, because that path
returns.

**Why it dominates.** The folded form lets VC5 coalesce the store pair into a
batch (`mov [r]; mov [r+2]; add r,4`). Batching drops pOut's register pressure,
which frees esi, which rotates the allocation across the entire function
(`cmp edi,ebx` recomp vs `cmp edi,ebp` orig, x61). Every previous session read
that rotation as the disease. It was the symptom.

Applied at 15 of 16 sites in the 2-byte form (nibble-expand, mirror-copy,
forward-copy, palette, colour-interp, 4-way unrolled) and across the 8-bit
output arms in the 1-byte form (`+= 1`, walking a real `unsigned char *`):
**+1152 → +64 bytes, +234 → +24 instructions.**

Generalised as `tools/gen_countfold.py` and wired into `_refine_candidates` as
the `countfold` candidate. Unaided it fires on 12 of the 16 sites for
−160 B / −24 insns; the 4 it skips have a `puVar9`/`puVar21` ping-pong that
needs the exit-path values proved by hand. **Bulk payoff outside this function
is currently ZERO** — the `X + N >= Y` guard shape does not occur anywhere in
the 2139-file `build/ghidra_decomp/` corpus. Keep the generator anyway: it is
free in the candidate list and it is where the knowledge lives.

## Four independent classes landed alongside it

1. **Colour channels are `unsigned char` locals**, packed through an
   `unsigned short` lvalue: `chR/chG/chB/chA`, not `(unsigned int)` int
   arithmetic. Orig homes them in byte stack slots and reads them back with a
   16-bit-destination `movzx ax, byte ptr [esp+S]` — VC5 uses the narrow load
   because the destination is a 16-bit lvalue, so only the low half is live.
   The tree's `(unsigned int)` casts forced `and edx,0xff` plus
   `xor dx,dx; mov dl,al` instead.
2. **`FUN_100271f0` takes `unsigned short`.** A wider prototype costs 32 bytes
   of zero-extension across the 8 call sites. Orig passes
   `mov dx, word ptr [ecx+eax*2]; push edx` with no widening. (The return
   width is byte-neutral.)
3. **Group-loop bound tests are `if (ctr >= bound) break;`** — Ghidra
   canonicalises this to `if (bound <= ctr)`. Byte-neutral, but it moves the
   register-blind gap by 25.
4. **Mask arms are written odd-path-first**: `if ((row & mask) != 0)
   { ODD } else { PLAIN }`. Read the jcc sense at the test to decide which arm
   the source wrote first — orig lays the fall-through arm inline. NOT a
   blanket rule: six of the nine mask sites want the flip, three are already
   correct as Ghidra printed them. Measure each one.

Plus an explicit `if (param_9 >= param_10) return;` guard before the
pOut/cbOut/aTile sinks (orig loads those only after the `jge ret`). This is
what moved the first divergence from +0x11 to +0x14.

---

## MEASURED NEGATIVES — do not re-run

- **`iVar17 -> param_9` and every other rename chasing orig's stack-slot
  numbers.** MSVC 5.0 PACKS ORDINARY LOCALS INTO DEAD PARAMETER SLOTS, so
  Ghidra's `param_N` scratch names are slot coincidences, not evidence about
  the source. Orig really does write the IDX4 arm's width into param_9's arg
  slot at 0x1002517e — and the rename still costs +16 B / +4 insns
  post-transform (+32 B for the CI8 arm, +16 B for the I8 arm). **This
  supersedes the "confirmed transcription fix, −3 insns" claim in the older
  dossier and in memory; that measurement was taken pre-transform and does not
  survive.**
- Flipping `0 < X` to `X > 0`: byte-identical, 54 sites.
- Swapping integer `imul` operand order: byte-identical.
- `bVar11 = *pbVar12++;` folding the load pointer's bump: worse.
- Naive `*p++` on the load pointer in the param_8 copy-back loops: worse.
- A distinct `unsigned char` local per I4 loop body instead of a shared one:
  byte-identical (VC5 CSEs the widening).
- `do{}while` → `for` on the two `local_24`/`iVar13` group loops of arm 1:
  worse. (Four other such conversions measured better and are in the tree.)
- The whole flag/pragma grid, and the do-while-with-raw-param_10 prologue
  guards (w1–w4). One cl.exe patch level; `/O2` is correct.

## The metric, and the lesson about it

Use the **register-blind** multiset diff, not the raw one:

    sh tools/fnmatch/vdiff.sh <TAG>              # scorecard
    sh tools/fnmatch/vdiff.sh <TAG> "" regnorm 30

Raw read 1097 extra / 863 missing on the pre-transform file — a wall. Register
normalised it read 432 / 198: roughly 650 of that "difference" was ONE global
register rotation, and the real structural gap was under half the raw number.

**A whole-function register rotation is usually a SYMPTOM of a structural
source defect elsewhere in the body, not a terminal wall.** This function was
declared a coloring wall twice on the strength of the raw number. Rank residue
by the register-blind gap.

## Also landed

- **8-bit-output arms**: the counter-fold in its 1-byte form, walking a real
  byte pointer (the tree spelled the destination as a cast of the halfword
  pointer with `puVar21 += 1` covering two byte stores), plus pre-biased read
  pointers in the mirror blocks matching orig's `lea eax,[esi-1]` / `dec eax`.
- **IA8 arm**: the `puVar9`/`puVar21` ping-pong was Ghidra's rendering of the
  folded double-step and collapses to one output pointer; the low nibble is a
  named local; the nibble merge reads ONE widened value instead of
  re-deriving from the source byte three times.
- **The 4-texel group loops are `for`, not `do{}while` + top `break`.** The
  do-while form makes VC5 peel the break test into a guard AND tail-duplicate
  it at the latch — two instructions a site.
- **Twin-counter collapses**: Ghidra prints two names for one variable
  (`iVar16 = param_9 + 1; ...; param_9 = iVar16;`). Provably equal at every
  read including the break edges.
- Six of the nine mask arms want the odd-path-first flip; three do NOT.
  Measure each one — it is not a blanket rule.

## Remaining gap (~52 real bytes / ~12 insns), register-blind

| | |
|---|---|
| +30   | `mov [esp+S], R` / `mov R, [esp+S]` — extra stack accesses |
| −17   | `add R, R` / `lea R, [R+R]` — orig doubles in-register; the recomp reloads a slot and adds. 10 of orig's 18 sites reproduce; 8 do not. |
| −6    | 8-bit `or B, B` — the I4-blend arms still widen their nibble merge (orig does it entirely in byte registers through byte slots) |
| −4/+4 | `mov B, [R]` vs `mov B, [R+R]` — one indexed-vs-walking load site left |
| −3    | `imul R, R` — orig folds nothing from memory into `imul` |
| −4/+3 | `mov word [R−I], W` vs `mov word [R], W` — store scheduling, downstream |

## N64 twin: none

TGR N64 has no BrTex3dExpand ancestor (PC-side TMEM-replacement code, no
nibble-unpack signature in its 883 functions). The only second witness is the
D3D twin 0x10025AB0 in `shared.csv`. Do not spend more time there.
