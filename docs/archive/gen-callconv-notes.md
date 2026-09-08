# Calling-convention generator — decision logic

Standalone transform (`tools/gen_callconv.py`). Do **not** copy this file
into `ghidra_to_match.py` blindly: fold the *rules* into
`_refine_candidates` as one candidate per caller. Proven against
BRGlide.dll orig bytes.

Ghidra types every callee as cdecl `int f()`. The original calls Win32,
Glide, COM and DInput with other conventions. Wrong convention at the
**caller** emits a spurious `add esp,N` (3 bytes per site), promotes
`float` args to `double` (`fstp qword` + `and esp,-8`), or drops
`ecx=this`.

`fix_calling_convention` in `ghidra_to_match.py` already rewrites the
*defined* function's own signature (`__thiscall`→`__fastcall`, trailing
`ret imm16`→`__stdcall`). This generator rewrites **callees**.

~223 unmatched functions have at least one of: `FF 15` without an
immediate `add esp`, a Glide `E8` thunk, a vtable `call [reg+N]`, or a
`.data` funcptr. The prey table below is the measured convention-only
delta (wrapped Ghidra decomp, no hand edits).

## Byte pattern → convention + arity

Walk the original body with capstone (repo `.venv`, never the host).
Maintain `pending` = dwords pushed since the last stack baseline, plus a
parallel `arg_stack` of recovered C literals (`push 8` → `"8"`,
`xor esi,esi; push esi` → `"0"`, unknown → `None`).

Baseline ignores:

- leading `sub esp, N`, including after a hoisted load (0x100704E0 is
  `mov eax,[g]; sub esp, 0x110`)
- `push ebx/ebp/esi/edi` before the first *argument* push (prologue,
  possibly after a hoisted load — 0x100703D0 is `mov eax,[g]; push edi`)
- the same saved-reg burst after a `ret` (shrink-wrap — 0x10002F70's
  second arm starts `push esi`)
- **Exception:** a saved-reg push of a register that currently holds a
  tracked immediate is an *argument*, not prologue (0x10023B70
  `xor esi,esi; push esi` is grAlphaCombine's last stack arg). Once any
  non-prologue push is seen, later `push edi` is an arg (0x10002580
  `mov edi, 0x2710; push 5; push edi`).

`add esp, N` after a call is cdecl cleanup only when `N <= 0x40` and
`N % 4 == 0`. Larger values are frame teardown (`add esp, 0x110` /
`0x8000`), not arity. The add may be delayed across a `mov`/`test`/
`lea` that does not touch esp (CRT `sprintf` is `FF 15; mov ecx, X;
add esp, 8`). Stop the look-ahead at `push`/`call`/`ret` so a later
cdecl's cleanup is not stolen.

Capstone spells memory operands `dword ptr [eax]`. Strip the `dword ptr`
prefix before matching `[reg]` or every COM vtable load is misread as
`ecx = object`.

### 1. `FF 15 disp32` — `call [imm32]`

| next cleanup | target in IAT? | convention | arity |
|---|---|---|---|
| `add esp, N` (N≤0x40) | either | cdecl | N/4 |
| none | yes, `dll!_name@X` | **__stdcall** | X/4 from the decoration |
| none | yes, no `@X` | **__stdcall** | `pending` |
| none | no (a `.data` slot) | **__stdcall funcptr** | `pending` |

Win32/CRT names (`CloseHandle`, `sprintf`, `mciSendCommandA`, …) are
already `__declspec(dllimport)` via `windows.h` / `mmsystem.h` /
`_CRTIMP`. Do **not** redeclare them. `sprintf` is cdecl (delayed
`add esp`); CloseHandle is stdcall. The IAT encoding is `FF 15` for
both — the add-esp (or its absence) decides.

The win is the **function-pointer global**: Ghidra's
`typedef int (*funcptr)(); extern funcptr DAT;` is cdecl and emits
`call [DAT]; add esp,N`. Retype:

```
extern int (__stdcall *g_pfn575480)(int, int);
```

Name the slot via `globals_learned.csv` (`0x104b1628` → `g_pfn575480`).
Strip the wrap's `extern funcptr NAME;` or the cdecl declaration wins
(it appears first). Proven 0x10002580 (four stdcall fptrs, 186→173)
and 0x10003030 (EAR `(*g_pfn575480)(h, 0)`, 151→132).

### 2. `E8 rel32` — `call tgt`

Resolve `tgt`. If the six bytes at `tgt` are `FF 25 disp32` (`jmp [imm]`):

- `imm` in IAT, dll name contains `glide` → **Glide stdcall thunk**.
  Name = import with leading `_` and `@N` stripped (`_grClipWindow@16`
  → `grClipWindow`, arity 4). Declare
  `void __stdcall grClipWindow(int, int, int, int);`
  (or the float signature from the table below).
- Distinguisher vs CRT: CRT is `FF 15` at the call site itself, Glide is
  `E8` to a 6-byte thunk. Proven 0x1001DFB0 (117→110) and 0x10023B70
  (152→135, after padding Ghidra-dropped args).

If Ghidra's C call has fewer args than `@N` / `GLIDE_SIGS` (Ghidra
turned stdcall stack args into fake `uStack_*` locals), **rewrite the
C call** with orig-recovered immediates in C order (reverse of push
order). 0x10023B70: `grAlphaCombine();` → `grAlphaCombine(3, 8, 1, 1, 0)`
from `push esi=0; push 1; push 1; push 8; push 3`. Without this the
5-arg stdcall prototype is C2198.

If `tgt` is a local function, open `build/match/orig/0x<tgt>.bin`:

| callee tail | `add esp` after call? | convention | arity |
|---|---|---|---|
| `C2 xx 00` (`ret imm16`) | no | **__stdcall** | imm/4 |
| `C3` (`ret`) | yes, immediately or delayed | cdecl | N/4 |
| `C3` | no, `mov ecx, <obj>` in the last 8 insns | **thiscall 0-stack** | 0; pending pushes belong to a *later* call |
| `C3` | no, ecx was a small immediate (`mov ecx, 0x80` for `rep stosd`) | cdecl 0 | do **not** treat as thiscall |
| `C3` | no, ecx was a data VA (`mov ecx, 0x11849f30`) | **thiscall 0-stack** | 0 (the imm *is* this) |
| `C3` | no, pending>0, no ecx=obj | cdecl 0 | pending kept (delayed `add esp` after a `mov`) |

`int f();` (unspecified args) already compiles as cdecl with the
call-site arity, so a delayed `add esp` on a cdecl FUN_ is a no-op for
this transform. Do not rewrite those as stdcall: stdcall N>0 is
`ret 4N`, never `ret`.

`E8` + `C3` is never stdcall N>0. 0-arg cdecl and 0-arg stdcall are
identical at the caller — leave cdecl.

### 3. `FF /2` mem — `call [reg+N]` / `call [reg]` (vtable)

Any `FF /2` that is not `FF 15` (IAT) and not `FF D0–D7` (call reg).
Displacement 0 is a slot (`call [eax]`). Look back ≤12 insns, stop at
the previous `call`.

**stdcall COM** (IDirectSound, IDirectInput, IDirectPlay):

```
mov eax, [obj]
mov ecx, [eax]          ; ecx = vtable   (or mov edx, [eax])
push argN
…
push eax                ; this on the stack
call [ecx+N]            ; or call [edx+N]
; NO add esp
```

Rule: the call's base register (or ecx) was loaded from `[objreg]`
*without a displacement*, and `objreg` was pushed. Model as
`typedef int (__stdcall *T)(int /*this*/, …args);`
`(*(T *)(*(int *)(obj) + N))(obj, args…)`.
Arity = `pending` (includes this). Proven 0x1006C6A0 (Stop +0x48,
Release +8) 78→2 diffs; 0x100703D0 (CreateDevice +0xc arity 4,
SetDataFormat +0x2c arity 2, SetCoop +0x34 arity 3, Acquire +0x1c
arity 1).

Ghidra's wrap form is `(**(funcptr *)(*OBJ + DISP))(ARGS)` — two
adjacent stars. Also:

- `(**(funcptr *)(**(int **)(INNER) + DISP))(ARGS)` (this through a
  field — 0x1003FDA0 +0x1c)
- `(**(funcptr *)*OBJ)(ARGS)` — vtable +0, Ghidra drops `+ 0`
  (0x1003FDA0 `(**(funcptr *)*g_brPhaseAA2904)(1)`)
- `(**(funcptr *)(*OBJ))(ARGS)` — vtable +0 with inner parens

If Ghidra dropped `this`, inject `obj` as C arg 0. If it dropped
further stack args, pad from orig-recovered immediates.

**thiscall** (C++ object, DDraw Flip/Begin):

```
mov ecx, obj            ; ecx = this  (NOT a [reg] vtable load)
mov edx, [ecx]          ; vtable
call [edx+N]
; NO add esp, this was NOT pushed
```

`ecx` holds the *object* (`[global]`, `ebx`, `[eax+disp]`, data-VA
imm). Small immediates (`mov ecx, 0x80`) are stosd counts, not this.
Model as `typedef int (__fastcall *T)(void *this);` for 0 stack args
(BR_THISCALL1). For K>0 stack args:
`typedef int (__fastcall *T)(void *this, int _edx_unused, args…);`
and the C call is `f(this, 0, arg1, …)` so edx is the dummy and the
real stack args sit at `[esp+…]`. Proven 0x100583C0 (+0x20 / +0x14,
0 stack) and 0x1003FDA0 (+0x1c 0-stack, +0 1-stack).

Ghidra's `(**(funcptr *)(*obj + N))()` often *drops this*. The rewrite
must inject `obj` as the first C argument.

Default when neither shape fires and `pending==0`: thiscall 0-stack.
Default when `pending>0` and this-push not proven: stdcall-COM, and
FLAG.

### 4. `FF D0–D7` — `call reg`

Almost always a Win32 import hoisted into esi/edi/ebx
(`mov esi, [__imp_GlobalHandle]; call esi`). Already stdcall via
windows.h. No rewrite.

## Glide float parameters

The `@N` decoration gives arity, not types. Empty `int grFog…();`
promotes `float` args to `double` (and can force `and esp,-8`). Known
non-int signatures (from glide2x and 0x1001DFB0):

| name | signature |
|---|---|
| `guFogGenerateLinear` | `(float *out, float near, float far)` |
| `grTexLodBiasValue` | `(int tmu, float bias)` |
| `grFogTable` | `(float *table)` |
| everything else used here | all `int` |

Anything not in the table is `int × (N/4)`. Flag new `gr*`/`gu*` with
float literals at the C call site as a manual add. Recovered integer
`0` for a `float` slot is emitted as `0.0f`.

## What this transform does NOT do

- Un-CSE Ghidra's "temps then one call" (cross-jump identical-calls).
  0x1001DFB0 still has that after stdcall decls (117→110 — remaining is
  the TMU0 combine spelled once through uVars). A second generator.
- Delayed `add esp` across a `mov` on a cdecl FUN_ with 3 args. Harmless:
  `int f();` already accepts the extra args.
- thiscall FUN_ with 0 stack args: declaring
  `int __fastcall FUN_1006d180(void *this);` does not insert `this` at
  the C call `FUN_1006d180()`, and Ghidra often attached a later cdecl's
  pushes (`FUN_1006d180(1,1)` — 0x1006B0E0), which then C2198 against a
  1-arg prototype. Flag and leave the cdecl decl. A later pass has to
  thread the object expression (`mov ecx, ebx` / `mov ecx, 0x11849f30`)
  to each site and peel the stolen args back onto the following call.
- Allocator residue after a clean convention (0x1006C6A0: 2 diffs,
  esi/edi of `GlobalUnlock` vs `GlobalHandle`). Stop.
- Ghidra-shredded frames (0x100704E0 DIJOYSTATE2 as `abStack_*`,
  0x10023B70 `_chkstk` 0x8000 as `in_stack_00007fdc`). Convention
  rewrite is still applied; the other residue dominates the diff count.

## Folding into `_refine_candidates`

One candidate, not a search:

```
def _callconv_rewrite(src, orig_bytes, va):
    import gen_callconv as cc
    calls = cc.analyze_orig(int(va, 16))
    new, _ = cc.transform(src, calls)
    if new != src:
        yield ('callconv', new)
```

Seed from the wrapped TU (`wrap_for_compile` output). Do not write
`ghidra_work` from this module; the refine loop already does that.

CLI:

```
python3 tools/gen_callconv.py --va 0x1006C6A0
python3 tools/gen_callconv.py --va 0x1006C6A0 --from-decomp
python3 tools/gen_callconv.py --validate
python3 tools/gen_callconv.py --va 0x1006C6A0 --from-decomp --no-score
```

`--from-decomp` isolates the convention-only delta (wrap of
`build/ghidra_decomp`, never a hand-edited `ghidra_work`). `--validate`
runs the prey list that way.

## Measured convention-only delta

Scored with `ghidra_to_match._score_source` against `build/match/orig`,
opts `/O2`, `/Od`, `/O2 /Oy-`. From-decomp wrap. 2026-08-26.

| VA | before | after | delta | what the transform did | remaining |
|---|---|---|---|---|---|
| 0x1006C6A0 | 78 | 2 | **-76** | stdcall COM Stop/Release (this already in C) | esi/edi Unlock vs Handle wall |
| 0x10003030 | 151 | 132 | **-19** | `g_pfn575480` stdcall arity 2 | nop-split + mci arm |
| 0x10023B70 | 152 | 135 | **-17** | Glide stdcall + pad `grAlphaCombine(3,8,1,1,0)` / blend `(4,0,4,0)` | `_chkstk` 0x8000 frame, fake uStack locals |
| 0x10002580 | 186 | 173 | **-13** | four stdcall fptrs (4/1/2/2) | store-burst (edx/ecx vs esi) |
| 0x1001DFB0 | 117 | 110 | **-7** | 15 Glide stdcall decls | CSE of TMU0 `grTexCombine` |
| 0x1003FDA0 | 115 | 114 | **-1** | thiscall vtable +0x1c and +0 (inject this) | inlined strcpy |
| 0x1006B0E0 | 151 | 151 | 0 | FLAG thiscall FUN_1006d180/190; decl left cdecl | missing `mov ecx,this` + shared add-esp |
| 0x100583C0 | 177 | 177 | 0 | thiscall DDraw +0x20/+0x14 (inject this) | sprintf/loop residue dominates |
| 0x100704E0 | 185 | 185 | 0 | stdcall COM Poll +0x64 / GetState +0x24 | Ghidra shredded DIJOYSTATE2 frame |
| 0x100703D0 | 86 | 87 | **+1** | stdcall COM CreateDevice/SetDataFormat/SetCoop/Acquire | stosd zeroing; convention is a small island |
| 0x10002F70 | 99 | 101 | **+2** | same `g_pfn575480` stdcall as 0x10003030 | nop-split (two arms); stdcall is correct |

None of these 11 reached 0 diffs. Closest is 0x1006C6A0 at 2/176
(allocator wall — stop). 0x1001DFB0 is a known MATCH in `ghidra_work`
after a hand un-CSE of TexCombine; the convention generator alone
leaves the CSE.

+1 / +2 on 0x100703D0 / 0x10002F70: the convention rewrite is still the
right bytes for those sites; Ghidra's other residue (stosd counts,
nop-split control flow) dominates the count and can move ±2 when a
3-byte `add esp` disappears and later encodings reshuffle. Do not
revert the stdcall decls.

## Ambiguous cases (bytes alone cannot decide)

1. **Shared `add esp, N` covering several calls.** 0x1006B0E0:
   `push 1; push 1; mov ecx, this; call CountedTotal; push eax;
   mov ecx, this; call StateGet; push eax; push ebp; call F; add esp, 0x14`.
   CountedTotal ends `ret` so it is not stdcall. Whether the two `push 1`
   belong to CountedTotal (cdecl 2-arg thiscall-hybrid) or sit on the
   stack for F cannot be told from CountedTotal's tail. The generator
   treats CountedTotal as thiscall-0 and leaves pending for F, which is
   the right *bytes* reading, but Ghidra's C is `FUN_1006d180(1,1)` —
   those args must be peeled off by a later pass. FLAG.
2. **thiscall vs stdcall COM when both `pending>0` and ecx is live.**
   Use the vtable-shape rule (call-base loaded from `[objreg]` and
   objreg pushed → stdcall COM; ecx = object and object not pushed →
   thiscall). If neither fires, default stdcall-COM and FLAG.
3. **Glide arity `@N` vs Ghidra CSE dropping args.** Recovered orig
   immediates fill the holes when every pending push is a known
   constant. Unknown register pushes pad with `0` — convention matches,
   values may not. FLAG if C site count ≠ orig site count (CSE).
4. **0-arg cdecl vs 0-arg stdcall.** Identical at the caller (`call` /
   `ret` with no `add esp`). Do not rewrite.
5. **Linear constant tracker vs CFG.** `mov eax, 1` on a fail-path
   `ret` is still in `reg_imm` when the live path later `push eax`
   (0x100704E0 Poll). Do not trust recovered `c_args` over the C
   object expression for a this-pointer. The rewrite injects `obj`
   from the Ghidra expression, not from `c_args`.
6. **thiscall FUN_ object expression.** `mov ecx, ebx` cannot be
   named in C from bytes alone (ebx's C expression is a local).
   `mov ecx, 0x11849f30` *could* be injected as `(void *)0x11849f30`
   but mixed sites (some ebx, some imm, some with stolen args) make a
   uniform rewrite unsafe. FLAG; leave cdecl.
7. **`call [reg+N]` with SIB** (`call [eax+ecx*1+N]`) — not seen in
   the prey. Unclassified; FLAG.
