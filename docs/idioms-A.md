# 0x100250D0 insn-3 wall — BROKEN 2026-08-27

BrTex3dExpand, 8480 B, `src/core/drawing/br_tex3d_expand.c`.

**The insn-3 wall is BROKEN. The earlier "coloring wall, do not grind,
mechanically impossible" verdict was WRONG.** First divergence moved from
orig+0x7 to orig+0x11: the whole prologue plus all three pushes now match
byte-for-byte. Tree file carries the fix (`for`-loop shape); `@implements`
stays OFF (full body not matched yet).

## The crack: for-loop with a raw-parameter bound

Replace the `lodEnd = param_10; if (iVar10 >= lodEnd) return; do {...}
while (iVar10 < param_10);` shape with a `for` whose bound is the raw
parameter:

```c
iVar3 = (int)param_4;
iVar22 = 0;
cbMax = param_2;
puVar21 = param_1;
for (iVar10 = param_9; iVar10 < param_10; iVar10 = iVar10 + 1) {
  ...
  puVar21 = puVar9;
}
```

Drop the `int lodEnd;` local entirely. `iVar10 < param_10` (NOT
`param_10 > iVar10`) is required: it emits `cmp eax, ecx` matching orig;
the reversed spelling emits `cmp ecx, eax`.

**Why it works (and why every prologue probe failed):** a `for`/`while`
loop bound that is a raw parameter is hoisted into a scratch register at
LOOP SETUP — VC5 emits `mov ecx, [esp+0x90]` (param_10) at orig+0x7,
before the pushes, while eax still holds param_4. A `do-while` references
the bound only at the bottom latch, so the top guard instead loads
param_9 lazily into edx (the documented wrong insn 3). Confirmed: all
four do-while variants that inline raw param_10 into the top guard STILL
diverge at +0x7 (`scratch_dw_1.obj`, w1-w4). The
[[merge-lodEnd]] refutation ("topologically impossible") was correct
about *local renaming* but a control-flow restructure changes the web
graph itself — that is the escape it missed.

**Do-not-re-run confirmed dead FOR THE PROLOGUE** (all still +0x7): the
merge-lodEnd family (iVar5/cbMax/one/iVar17 -> lodEnd, per-web split),
the entire flag/pragma grid (one cl.exe patch level; /O2 is correct),
and the do-while-with-raw-param_10 guards (w1-w4). See VC5-IDIOMS.md.

## Remaining divergence at +0x11 is BODY-DRIVEN, not prologue

orig `mov [esp+0x58], eax` (spill param_4 to a slot) vs recomp
`mov esi, [esp+0x78]` (cache param_4 in esi). esi is FREE in the recomp
only because the body does not yet claim it for `pOut`. The honest /O2
build is +1152 B / +234 insns over orig, all in three source-fixable
body families (recon 2026-08-27):
1. texel-store idiom — orig `*p++` per half-texel (`add esi,2; mov
   [esi-2],dx`, budget check between halves); recomp batches
   `mov [r]; mov [r+2]; add r,4`. ~100 insns.
2. arm order / condition sense — whole arms in a different layout
   (palette path out-of-line in orig, inline in recomp; maskOdd test
   je vs jne); ~2 kB of moved blocks.
3. temp caching — named locals cache tile fields orig reads as direct
   memory operands (`mov r,[tile+0x24]` x8 etc.); +114 loads/+70 stores.

Fix the body families -> pOut claims esi -> param_4 spills -> +0x11
converges. That is the path to full match. Attack body-first now, not
the prologue.

## Body grind attempt 2026-08-27 (7-arm workflow) — NEGATIVE, key lesson

A workflow mapped 9 store arms, wrote per-arm `*p++` rewrites, and
integrated them. Result did NOT converge and was NOT landed (tree stays
at the clean for-loop checkpoint, first-div +0x11, frame 0x68).

**Lesson: the naive `*p++` C spelling does NOT reproduce orig's store
idiom.** The agents wrote `*puVar21 = x; puVar21 = puVar21 + 1;` with the
budget check between the two half-texels — and VC5 STILL batched the
stores (`mov [r]; mov [r+2]; add r,4`). So esi never flipped to pOut,
param_4 never spilled, and the whole-function register rotation
(`cmp edi,ebx` recomp x62 vs `cmp edi,ebp` orig x61) stayed live. The
candidate's lower raw insn count (2544) was partly from DROPPING locals,
which shrank the frame to 0x60 (breaks orig 0x68) — a regression, not
convergence.

**Open sub-wall for a Fable session (this is the real remaining work):**
what source shape makes VC5 emit `mov word ptr [esi], bx; add esi, 2`
(true `*p++`, pOut in esi) instead of coalescing the two half-texel
stores? Candidates NOT yet tried: (a) hoist pOut into its own pointer
variable that is the ONLY thing incremented (no parallel index like
iVar22 advancing in lockstep — the twinned `iVar22 += 2` next to
`puVar21 += 1` may be why VC5 batches); (b) split the two half-texel
writes across a real control-flow edge (the budget check) so they cannot
coalesce; (c) force param_4/iVar3 to spill directly (find the source
that denies it a callee-saved reg) and let pOut fall into esi. The
paired EXTRA/MISSING multiset shapes (see below) all collapse together
once esi flips — it is ONE lever, not nine. Metric: scratchpad/mdiff.py
(drive 2641 -> 2407). The winning candidate source and full gap list are
in the workflow output (tasks/w4gmalb6r.output).

## Confirmed transcription fix (adopt independently)

`iVar17 -> param_9` in the IDX4-16bit arm (file lines ~80-203): orig
stores that arm's width into param_9's arg slot [esp+0x9c] (0x1002517e).
-3 insns toward orig. Not yet applied to the tree.

## N64 twin: none

TGR N64 has no BrTex3dExpand ancestor (PC-side TMEM-replacement code,
no nibble-unpack signature anywhere in its 883 fns). Only second witness
is the D3D twin 0x10025AB0 (shared.csv). Do not spend more time there.

---
## HISTORICAL (pre-2026-08-27) — the failed prologue verdict

The material below is the original dossier that concluded "coloring
wall, stop." It is kept for the register map and the do-not-re-run
lists, which are still valid *for the prologue*. The verdict itself is
superseded by the for-loop crack above.

**Superseded verdict: coloring wall. Stop. Not a MATCH. Do not grind.**

## Orig vs wrap at the empty-check

Orig (Glide 0x100250D0):

```
sub  esp, 0x68
mov  eax, [esp+0x78]          ; param_4  (texel base, stays in eax)
mov  ecx, [esp+0x90]          ; param_10 (lodEnd)     <-- insn 3 target
push ebx
push ebp
push esi
mov  [esp+0x58], eax          ; spill param_4
mov  eax, [esp+0x98]          ; param_9  (lod) after 3 pushes
push edi
xor  edi, edi
cmp  eax, ecx                 ; lod vs lodEnd
mov  [esp+0x58], eax          ; store lod (different slot than the spill)
jge  ret
mov  ebp, [esp+0x80]          ; cbOut sink
mov  esi, [esp+0x7c]          ; pOut  sink
mov  ebx, [esp+0xa4]          ; aTile sink
shl  eax, 6                   ; lod still in eax
```

Latch reloads lodEnd from the arg (`mov ecx, [esp+0xa0]`; `inc eax; cmp eax, ecx; jl 0x10025106`).
ecx at insn 3 is short-lived: live only until the empty-check `cmp`.

Wrap /O2 (honest source `iVar3 = param_4; lodEnd = param_10; iVar10 = param_9; if (iVar10 >= lodEnd) return;`):

```
sub  esp, 0x68
mov  eax, [esp+0x78]          ; param_4
mov  edx, [esp+0x8c]          ; param_9   <-- insn 3, wrong arg, wrong reg
push ebx / ebp / esi
mov  [esp+0x60], eax
mov  eax, [esp+0x9c]          ; param_10 after 3 pushes
push edi
xor  edi, edi
cmp  edx, eax
```

`lodEnd = param_10` is copy-propagated. First real use is the cmp, which
loads param_9 (edx) then param_10 (eax). Cascade: lod in edx, cbOut in ebx,
siz in ecx, instead of ebp=cbOut / esi=pOut / ebx=aTile.

## Why this is coloring, not a missing statement

After `mov eax, param_4`, edx is free. Mentioning param_9 at the empty-check
(required: orig is `cmp eax, ecx; jge ret` with eax=lod) lets /O2 put lod in
edx immediately. Orig instead holds lodEnd in ecx, spills eax, then reuses
eax for lod. Same two live values (param_4, param_10), same free edx; the
allocator picks edx-for-lod vs eax-reuse-for-lod. No C-level use of param_10
forces the orig coloring without extra instructions orig does not have.

## Lever that hits insn 3 — then dies

Two volatile temps, source order param_4 then param_10:

```
volatile int va, vb;
va = (int)param_4;
vb = param_10;
iVar3 = va;
lodEnd = vb;
```

emits the target bytes at +0x7 (`8b 8c 24 90 00 00 00`) with eax still
holding param_4. That is the non-DCE scratch use the brief asked for.

It is not a match:

- Two extra volatile stores (lodEnd is register-only in orig until the cmp;
  orig never writes param_10's slot in the prologue).
- If param_9 is in the empty-check (it must be), the next insn is
  `mov edx, [esp+0x8c]` vs orig `push ebx`. lod is in edx, not eax.
- Omitting param_9 from the check (diagnostic `if (lodEnd >= 0)`) matches
  through `push ebx`, then diverges on the va store vs `push ebp`. Wrong
  semantics.

## 3+ successive first-divergence fixes that do not move lod out of edx

With two-volatile (insn 3 matching, first-div at +0xe = p9-in-edx vs push ebx),
all of these still emit `mov edx, [esp+0x8c]` at +0xe:

1. `iVar5 = param_11` before the check
2. `cbMax = param_2; puVar21 = param_1` before the check
3. `if (param_9 >= lodEnd)` without an `iVar10` copy
4. `&iVar3` address-taken / `*p = *p`
5. `iVar10 = param_9 + iVar3; iVar10 -= iVar3`
6. `iVar10 = iVar3; iVar10 = param_9`
7. late `volatile int v9; v9 = param_9`

That is the stop condition: register rename, no source lever that moves it
while keeping `if (lod >= lodEnd)`.

## Do not re-run (DCE or wrong order / extra insns)

Already listed in VC5-IDIOMS.md, confirmed /O2-identical to wrap:

- `volatile int lodEnd` (param_10 loads *before* param_4, or ebp if the
  latch uses lodEnd)
- `iVar10 = (lodEnd = param_10, param_9)`
- `param_10*0` / `param_10|0` / `param_10^0` / `param_10+0`
- `param_4+(param_10-param_10)` and `param_10 + iVar3 - iVar3` (one expr)
- `*(volatile int *)&param_10`
- `lodEnd = lodEnd` / `lodEnd = (lodEnd | 0)`
- `iVar3 += (lodEnd = param_10, 0)`
- `one = param_10` as the cmp RHS
- `if (lodEnd <= iVar10)` (compare-flip)
- `if (param_10 <= param_9)` as the first statement after param_4
- `register int` copy of param_10
- `if (iVar3) lodEnd = param_10; else lodEnd = param_10;`
- `if (iVar3 == lodEnd) {}` empty
- `one = iVar3` in the prologue (dead; `one = 1` is set before any read)
- `param_10 = lodEnd` writeback / `param_10 = (param_10 | 0)`
- `iVar3 += param_10; iVar3 -= param_10`
- `(void)param_10`
- `volatile int *p = &param_10; lodEnd = *p`

Non-DCE but wrong (do not chase):

- `volatile int scratch; scratch = param_10` after param_4: still hoists
  param_10 before param_4 (no dependence on iVar3)
- `scratch = param_10 + iVar3; lodEnd = scratch - iVar3`: loads param_4
  then param_10 but **swaps** eax/ecx (`mov ecx, param_4; mov eax, param_10`)
  plus extra add/store
- `static int s_keep; s_keep = param_10`: store delayed to after the cmp
  (copy-prop into eax=lodEnd); does not force the early ecx load
- `one = 1` plus volatile param_10: param_10 still before param_4
- `__asm` / `__asm nop`: forces ebp frame; prologue dies (BrTex3dRegister)

## Honest source to keep

```
iVar3 = (int)param_4;
lodEnd = param_10;
iVar10 = param_9;
iVar22 = 0;
if (iVar10 >= lodEnd) {
  return;
}
cbMax = param_2;
puVar21 = param_1;
```

Empty-check then `do { …; lod++; } while (lod < param_10)` with pOut/cbOut
sunk after the check. Latch must say `param_10`, not `lodEnd` (long-lived
lodEnd becomes ebp). That shape is already in `br_tex3d_expand.c`.
`@implements` stays off.

Fresh lead only: a first-region value that occupies edx for a reason orig
also has at +0xe, *or* a type/shape insight that makes lod and param_4 share
eax without a volatile store. Do not permute registers.
