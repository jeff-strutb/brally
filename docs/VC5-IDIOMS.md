# VC5 codegen idioms — read before matching any function

Every entry here was proven by a byte diff against BRGlide.dll, not inferred.
Add to this file whenever a new construct→codegen mapping is proven; this is
how per-function cost drops over time. Each idiom is solved ONCE.

## The method: infer the SOURCE from the bytes, never permute spellings

Trial-and-error spelling permutation is the slowest possible approach. When a
diff looks like register or width noise, ask what TYPE or STRUCTURE the
original source must have had to force those bytes.

Worked example (BrCarStatePack, 738 B, matched 2026-08-22): the original did
`mov bl,al` (byte) after a call where we emitted `mov ebx,eax` (dword). Not a
spelling problem — the original PROTOTYPE returned a char-width type.
Changing `int32_t BrFixPackS6Q7Neg/Level/U8Range(float)` to `int8_t` matched
the caller AND flipped a helper to match for free.

## Proven idioms

- **Flag-from-float:** `(f != 0.0f)` → `fld; fcomp [0.0 const]; fnstsw ax;
  test ah,0x40;` + jne/xor-vs-mov-1 branch pair. `(f != 0) ? 0x80 : 0` picks
  the constant into the mov.
- **Hoist order across calls is source left-to-right.** Subexpressions that
  must survive a call (flags into ebx/ebp, spills into stack slots) are
  hoisted in left-to-right source order. To hoist A before B, A's term must
  come first in the expression — or be a prior statement.
- **Shift distribution:** `(a*2 | b) << 1` gets the outer shift distributed
  into lea-scales. `((a << 1) | b) << 1` keeps the literal `shl`.
- **Accumulate stores:** `b[k] = x; b[k] |= y;` emits one store per statement
  with the value register-forwarded (no reloads). `t |= X; b[k] = t` leaves
  the or result in t's register; `b[k] = t | X` leaves it in al. The operand
  order of `|` itself is canonicalized — swapping does nothing.
- **Byte ops need byte types end to end:** `and bl,0x3f` / `shl al,6` come
  from uint8_t-typed locals and char-width return types, not from masks on
  ints.
- **Direct struct reads vs float locals:** both-memory products emit
  `fld [mem]; fmul [mem]`. A float local assigned from memory is copied via
  INTEGER mov to a stack slot and multiplied from the slot. A function mixing
  both (squares from locals, cross terms direct — BrRbBuildMatrix) shows the
  mix in the bytes; transcribe it, don't normalize it.
- **`(int)float` is always a `__ftol` CALL** (truncating). A bare `fistp`
  with no fldcw around it (round-to-nearest) CANNOT come from VC5 C —
  `/QIfist` does not exist in VC5 (warning D4002). Such code was inline
  `__asm` in the original and needs an asm-hybrid mechanism (none in the
  tree yet). First known case: BrDlCmdVtx 0x10021A20.
- **Statement reordering of a pure float DAG compiles to IDENTICAL bytes.**
  VC5 canonicalizes the DAG, so x87 scheduling residue (operand selection:
  `fld st5; fmul [mem]` vs `fld [mem]; fmul st5`; spill-slot count; fst vs
  fstp) is allocator-internal and NOT source-reachable. Documented wall
  class — see divergence-class triage. Do not burn time here.

- **Re-deref, don't cache:** the original often writes `*p` repeatedly and
  lets CSE cache it; an explicit `v = *p` local changes register allocation
  (VC5 reuses the local's register where the CSE shape burns a fresh one).
  Dropping the local matched BrSegPtrFixup (43 B) outright.
- **`(uint8_t)(x >> 20) & 1` vs `x & 0x100000`:** `mov edx,ecx; shr edx,20;
  test dl,1` comes from the byte-cast shift form. `test ecx,0x100000` comes
  from the plain mask. Read which one the original used off the bytes.
- **Allocator-internal residue in integer code:** which byte temp gets cl vs
  dl, and which register holds a loaded offset (eax vs esi), cascade locally
  then resync. Three different source shapes (temp decl order, reuse target)
  compile byte-identical — same wall class as float scheduling, but rarer
  and smaller. Land the structural match and move on.

- **Dead load:** `(void)*(volatile int32_t *)&p->field;` emits exactly one
  `mov` with no store. A plain local init is elided; a volatile LOCAL spills.
- **Zero-register (ebx) trigger:** VC5 dedicates a callee-saved register to
  the constant 0 when zero feeds many small STORES through branchy code.
  Ternaries (`c ? 0x80 : 0`) that compile to setcc/materialized ints can
  suppress it — spelling the same logic as branchy if/else stores flipped
  BrPadTranslate from 625 diffs to 220.
- **Switch operands are compared SIGNED** (`jg` not `ja`) — switch on
  int32_t, not uint32_t.
- **BR_THISCALL1 (`__fastcall`, one arg) reproduces thiscall exactly** —
  confirmed live on BrPadTranslate.
- **Switch vs else-if:** a 15-case sparse switch lowers to a binary compare
  tree; the equivalent else-if chain lowers to a LINEAR ladder (worse). But
  the tree's node form can still differ (compacted `cmp;jg;je` vs the
  original's `cmp;jg;cmp;je`) — an unresolved lowering-shape wall, first
  seen on BrInputIsDown.
- **The residue ceiling on big int functions:** four of five 490–700 B
  functions landed at 4–24 divergent bytes, every one an allocator choice
  (byte-reg pick, esi/edi role, imm-vs-pooled constant). Getting to that
  ceiling took 3–10 min each; crossing it needs a type/shape insight (as
  byte-width returns were for BrCarStatePack) or does not happen.

- **thiscall with stack args = `__fastcall(this, int _edx_unused, args...)`.**
  Same ECX `this`, same stack layout, same `ret N`; an unused EDX slot costs
  no bytes. Generalizes BR_THISCALL1. Proven on BrBoundsFits_10058CC0 and
  four C++ scalar deleting destructors (`push esi; mov esi,ecx; call ~T;
  test byte [esp+8],1; jz; push esi; call operator delete`).
- **ECX copy-propagation:** after `mov esi,ecx`, calling a thiscall callee
  with an explicit `this` argument (`Dtor(param_1)`, callee declared
  `__fastcall`) emits NO `mov ecx,esi` reload — VC5 knows ECX still holds
  it. So the callee can carry its real prototype (proven BrVt55A10DeleteDtor).
- **`ret imm16` ⇒ stdcall with imm/4 word args.** For the callee: declare it
  `__stdcall` with that many params (caller then emits no `add esp,N`); for
  the function itself: pad to imm/4 params (`int __stdcall f(int,int,int)
  {return 0;}` = `33 c0 c2 0c 00`). Proven BrObj54710Dtor, BrRet0Std3.
- **Win32 imports are `__declspec(dllimport)`** — `call [__imp__X]` (FF 15),
  never `call X` (E8). windows.h provides it; hand prototypes must carry it.
  Proven BrDllMain.
- **The binary is /MD — CRT calls are FF 15 too.** Put
  `#ifdef BR_MATCHING_BUILD / #define _CRTIMP __declspec(dllimport) / #endif`
  BEFORE the first include (any header pulling a CRT decl locks _CRTIMP empty
  — the errno C2370 error means the define came too late). Applied tree-wide
  2026-08-23: +16 matches at zero losses, including the whole BrFixPack
  family that had been misfiled as register-allocation walls. When a
  CRT-calling function sits 1-5 diffs away, check the call form FIRST.
  memcpy/memset/strlen at /O2 are /Oi intrinsics and unaffected.
- **`for (;;)` vs `do {} while (1)` at /Od:** `for(;;)` loops back with a
  bare `jmp`; `do{}while(1)` emits `mov eax,1; test eax,eax; jne`. The
  original used `for(;;)` (Ghidra prints `do{}while(true)`). Proven
  0x1002DE04.
- **Comparison operand order survives:** `if (a > b)` → `cmp a,b; jle`;
  `if (b < a)` → `cmp b,a; jge`. Ghidra canonicalizes to `<`; the original
  was `>` (proven BrCdTrackNext). Unsigned globals give `jb`/`ja` (proven
  BrSecondTickLoop). Byte globals need `char` externs (`mov cl,[x]`, proven
  BrMenuLatchPending). The `--refine` pass of ghidra_to_match tries all three.
- **`while (p->next) p = p->next;`** keeps `p` in ECX and loads via
  `mov ecx,eax; mov eax,[ecx+N]`; Ghidra's two-temp form loads through EAX
  (proven BrSndListAppend).
- **x87 intrinsics:** `double f(float x){return cos(x);}` = `fld dword
  [esp+4]; fcos; ret` under /O2 (/Oi). Return type must be double (Ghidra
  `unkbyte10`), or an `__ftol` call appears.

- **Two-return vs temp-increment:** `if (f) return n + 1; return n;` loads
  flag into EAX, tests, loads n into EAX between the test and its branch
  (both paths need it); `int t = n; if (f) t++;` puts the flag in EDX
  instead. Proven BrCountedTotal.
- **Named-temp fetch order for x87:** naming ONE operand of a commutative
  fadd (`float ay = pA->y; out = (ay + pB->y) * k;`) forces it to be the
  FLD operand. Works per component; proven BrVec3Midpoint (glide order:
  x pB-first, y/z pA-first). Does NOT work for fmul-by-scalar (BrVec3Scale
  stays walled) or to force a `mov reg,reg` copy of a stored temp
  (BrCursorPairSet stays walled — chained/cast/struct temps all fold).

- **Cross-jumped identical calls:** when every branch of an if/else chain
  makes the SAME call differing only in one constant arg, VC5 merges the
  common push suffix and each branch jumps into it after pushing its own
  constant (`push 0xe; jmp common`). A single shared call through a variable
  pushes a REGISTER instead — the source must repeat the call per branch
  (macro chains are fine). Proven BrGlideResOpen, 545 B first try.

- **/Od local slot order:** plain function-scope locals get slots in DECL
  order (-4, -8, -0xC...), but a local declared INSIDE a nested block is
  allocated AFTER every function-scope local, regardless of textual position.
  When /Od slot offsets look permuted vs every decl-order permutation, a
  counter was block-scoped inside a loop body. Proven BrEntGfxRebindAll
  (424 B). Note also the pair anomaly seen while probing: with three
  same-typed locals in loops, the two outer counters swapped slots by decl
  order while the innermost always took the last slot — trust the byte
  probe, not a rule, when slots misbehave.
- **`xor cl,cl`-free unsigned byte load:** `xor ecx,ecx; mov cl,[mem]` is an
  UNSIGNED char global read widened to int (`extern unsigned char`);
  `movsx` means plain (signed) char. Compare against int constants, not
  char literals, to keep the 32-bit compare.

- **Cast placement picks the load width:** `(short)(x * -2)` loads x as
  DWORD, multiplies, stores DX; `(short)x * -2` emits MOVSX first. Same for
  sums: `(short)((a*2 + b) * 2)` keeps dword loads. And the hoist-order rule
  applies inside: orig `lea [ecx+eax*2]` with a loaded first means the
  source wrote `a*2 + b`, not `b + a*2`. Proven BrDlRectCmdEmit.
- **Global post-increment via re-read:** `p = G; G = G + 2; *p = k;` loads G
  twice (mov, mov+add+store) then stores through the saved copy. `p = G;
  *G = k; G = p + 2;` orders the opcode store BEFORE the advance — different
  bytes. Match the original's order of advance vs store.

- **/Od branchless ternary:** `x ? K : 0` (and `x ? 1 : 2`) compile to
  `mov; neg; sbb; and/add` even at /Od — the neg/sbb borrow trick is the
  TERNARY's codegen, while `-(x != 0) & K` emits cmp/setne. Proven
  BrFrameBeginDl.
- **Display-list emit is a struct post-increment.** The DL write pointer is
  `struct BrDlCmd { int op, arg; } *`; every emit is
  `{ BrDlCmd *p_ = DAT_106e7710++; p_->op = C; p_->arg = A; }` (a macro).
  The post-increment's temp lands at the BOTTOM of the /Od frame (with the
  switch-selector temp), while each block-scoped `p_` gets its own top-down
  slot — a 23-emit function burns 0x60 bytes of frame. As a call argument,
  `f(DAT_106e7710++, ...)` gives push-then-advance. Proven BrFrameBeginDl
  (1421 B) after BrDlRectCmdEmit/BrDlScreenRectEmit matched the same shape
  spelled longhand.
- **`if (a ^ b)`:** `xor reg,[mem]; test` instead of cmp — the original
  spelled inequality as XOR (proven BrFrameBeginDl head).

## Cost model (measured, 2026-08-22 timed test)

Size is not the cost driver — code shape is. 738 B of int/call-heavy code
matched in ~10 min; a 165 B float function (BrVec3Project) is permanently
stuck at 30 diffs of scheduling residue. Int/call-heavy code sustains
5–10 min/function at any size once the file's context is loaded. The float/
x87 cluster carries the walls and is a separate workstream.
