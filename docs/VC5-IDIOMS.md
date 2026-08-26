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
- **Ghidra folds default-equivalent case labels, collapsing a jump table
  to a range check.** `switch(x) { case 2: case 3: case 4: return 1;
  default: return 2; }` compiles to `cmp eax,2; jl; cmp eax,4; mov eax,1;
  jle; mov eax,2; ret` (32 B). The original also listed
  `case 11: case 12: return 2;` (same value as default — Glide texfmt
  ARGB1555/4444), which keeps the dense range 2..12 and emits the
  two-level table: `add eax,-2; cmp eax,0xa; ja; xor ecx;
  mov cl,[eax+btab]; jmp [ecx*4+ptab]; mov eax,1; ret; mov eax,2; ret`
  (39 B). A singleton extra `case 12: return 2;` is itself folded (still
  32 B); a two-case cluster or a run through the high bound is not.
  Distinguishing bytes: `83 c0 fe 83 f8 0a 77` vs `83 f8 02 7c`. Proven
  0x10024DF0 (texture bytes-per-texel). The byte table
  `[0,0,0, 2,2,2,2,2,2, 1,1]` (holes 5–10 → default slot 2; 11–12 → slot 1
  even though both code addresses are `return 2`) is how you recover that
  the high labels were a separate case-group, not a filled-in 5..12 run.
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
  **Ghidra's identity `(int)` cast hides the flip from `_CMP_RE`:** it
  prints `if (bound <= (int)idx)` for original `if (idx >= bound)` (it
  wraps the originally-first operand when it thinks the global might be
  unsigned). Bytes: `cmp idx, bound; jl` (3B D7 / 7C) vs `cmp bound, idx;
  jg` (3B FA / 7F) — two diffs, ModR/M + jcc. `_CMP_RE` is
  `[^()<>=!&|]+` so the nested parens of `(int)idx` never match and the
  existing flip refine cannot fire. Strip the cast AND flip. Proven
  0x10008F90 (114 B, MATCH /O2).
- **`long+N` with trailing 90s is COFF padding, not extra instructions.**
  MSVC 5.0 aligns .obj functions to 16 bytes; a 114-byte body is 128 with
  14 `nop`s after `ret`. `_classify_divergence` currently labels any
  recomp longer by >8 as `long+N` *before* looking at the real diffs, so
  0x10008F90 (2-byte compare-operand-order) and the 48 B fread/fwrite
  twins (string-as-int, below) both landed in the 137-wide `long` bucket.
  65 of those 137 extras were exactly 16-byte-align padding. Strip
  trailing 0x90/0xCC down to orig_size before classifying. Proven
  0x10008F90 (extra = 14× `90` after `c3`).
- **A string passed to a function is an array (or a literal), not
  `extern int`.** `f(s_Foo)` with `extern int s_Foo` emits
  `mov r, [s_Foo]; push r` (load the first dword as a value). Original
  `push offset s_Foo` is `extern char s_Foo[]` or a string literal. Proven
  0x10008E60 / 0x10008E90 (48 B twins; Ghidra typed
  `s_File_read_failure` as int). The fread temp
  (`sVar1 = fread(...); if (sVar1 != n)`) is a no-op vs
  `if (fread(...) != n)` — both compile identical.
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
  **Ghidra CSE's this into temps + one call, which is the `short` class.**
  It prints nested `if (g == 0) { … t1=A; t2=B; } else { t1=C; t2=D; }
  f(p, t1, t2);` (range-check polarity, shared call). Original is an
  else-if `!= 0` chain with the call spelled in each arm — even when two
  arms push the SAME immediates (do NOT OR the guards: `if (b4 || b0)`
  emits one push-pair, original has two). Bytes: `test; jz next_arm;
  push imm32; push imm32; jmp common` vs a register-arg call after
  mov-imm. Distinguisher: `68 xx xx xx xx 68 yy yy yy yy eb` (push/push/jmp)
  vs `8b … 50 51 e8` (load temps, push regs, call). The two Glide callees
  are `__stdcall` with `float` near/far (E8 to 6-byte thunks, no
  `add esp`). Proven 0x10023AA0 (103 B fog-table builder, MATCH /O2;
  Ghidra's temp form was 48 B, the 55-byte "short" was the three extra
  push-pairs + jmps).

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

- **Ghidra constant-folds locals /Od never folds.** A runtime-computed
  constant in the original bytes (`mov word [ebp-4],1; movsx; movsx; shl;
  or` for 0x10001) is a LOCAL VARIABLE Ghidra folded away. Symptoms: /Od
  frame one slot bigger than the visible variable count, and slot offsets
  that look permuted (the folded var holds an early slot). Un-fold it
  (`short v = 1; ... v | v << 16`) and both the frame and every later slot
  snap into place. Proven BrDlBorderEmit (1585 B) — this WAS the "/Od slot
  mystery"; the allocator is innocent: function-scope decls in decl order,
  then block-scoped temps in textual order, compiler temps at the bottom.

- **Ghidra shreds stack structs into locals — /O2 then dead-stores them.**
  A recomp far SMALLER than orig with dozens of `uStack_...` locals means a
  stack struct whose address only partly escapes: every non-escaping member
  store gets deleted. Rebuild the struct (offsets = 0xNNN minus Ghidra's
  local suffix), and spell field assignments in the ORIGINAL's store order —
  which for parameter copies is natural parameter order (p1,p2,p8,p9), not
  Ghidra's rearrangement. Also: `1 << fn(x)` on an int-returning fn emits
  no mask (shl masks in hardware) — Ghidra's `& 0x1f` is decoration to
  delete. Proven BrTex3dCreate (715 B). The permute-and-score loop
  (itertools over statement orders, compile each) resolves store-order
  residue in minutes — use it before declaring an allocator wall.
  **At /Od the shred permutes slots, not just dead-stores.** Symptom: first
  0x100+ bytes match, then `lea [ebp-0xc]` vs `lea [ebp-8]` on a Ghidra
  `local_10` that should sit at the end of a 24-byte buffer. Decl-order
  permutation of the shredded pieces does NOT move them (first-use wins);
  only rebuilding ONE struct of the frame size does. 0x1002DEC3 (627 B,
  MATCH /Od): Ghidra printed `int iVar1; char local_28[24]; uint local_10;
  int local_c; char local_8[4];` — original is
  `struct { int result; char buf[24]; uint flags; int idx; char tmp[4]; }`
  (0x28 bytes, the whole frame). `lea [ebp-0x24]` is `&s.buf`,
  `lea [ebp-4]` is `s.tmp`, `lea [ebp-0xc]` is `&s.flags`, loop counter
  `[ebp-8]` is `s.idx`, call result `[ebp-0x28]` is `s.result`.
- **`if (x > K) goto end_of_for_body` at /Od is `jle +2; jmp label`, not
  `jg` and not `continue`.** Nested `if (x <= K) { body }` emits `cmp K;
  jg skip`. `continue` emits `jle +2; jmp increment` (E9 backward to the
  for-inc). Original skip target is the for-body tail (the `jmp inc` at
  the bottom), a FORWARD short `eb`. Distinguisher: `83 7d xx 0b 7e 02
  eb NN` vs `7f NN` vs `e9 xx xx xx xx`. Proven 0x1002DEC3: Ghidra's
  `iVar1 < 0xc` (and `continue`) both missed; `if (result > 0xb) goto
  next;` with `next:` immediately before the for-inc matched. The earlier
  guards stay nested `if`s (`je/jne` near to that same tail).

- **char parameters push as unaligned dword windows.** With a prototype
  taking `char` params, VC5 /O2 pushes each byte argument as a full dword
  load at the byte's address (upper three bytes are neighboring memory,
  legally garbage). Ghidra renders these as overlapping CONCAT chains —
  that pattern in a call means CHAR-TYPED PARAMS, not byte arithmetic.
  Proven BrTex3dExpandInto (617 B).
- **Ghidra also DROPS real stores** (not just folds constants): fields the
  decompiler proves unread (fTmu2, lod, w/h copies) vanish from its output
  while the /O2 bytes still write them. When a struct rebuild still runs
  short, transcribe the store list from the original bytes, not from
  Ghidra's text.

- **Semantically-redundant mid-returns block shrink-wrap.** When a branch
  reads `f(x); return id;` in the BYTES (own pop-sequence + ret), and the
  fall-through path also returns `id`, the source had NO mid-return — the
  second epilogue is VC5's return-duplication. Writing the redundant
  `return` in C forces the callee-saved pushes into the prologue (no
  sinking past the early-out) and rotates the whole register allocation.
  Proven BrTex3dRegister: deleting one redundant `return id` moved all four
  pushes below the early-out and aligned 57 more instructions.
- **Stack slots are per-VARIABLE; register webs are identity-blind.** /O2
  gives each user variable one stack slot for its whole life (disjoint
  lifetimes inside one variable share it; separate variables never do),
  while registers go per def-use web regardless of names. When Ghidra's
  output reuses one iVar for several unrelated roles, MERGE those locals in
  the C — it is what shrinks the frame to the original's (BrTex3dRegister:
  mirT+hCur+sum one slot; wInit+loopCopy+mirrorS one home). Conversely
  declaration ORDER and pure renames never change /O2 output (proven by a
  60-variant permutation sweep — all byte-identical), so never permute
  decls hoping for a different allocation.
- **Guard-if + for-loop vs do-while+break.** `if (g) for (j = a; j <= b;
  j++) {...break;}` compiles to the entry test, a bottom `jle` backedge,
  and a `jmp` over the outlined break-block. The same loop spelled
  `do {} while` with a break gets its latch TAIL-DUPLICATED (the body's
  first compare copied after the bottom test) — a different shape. Read
  which one the bytes have off the latch.
- **Cross-jump needs branch-symmetric temps** (extends the identical-calls
  idiom): the arms merge only when their tails are identical instruction
  streams, so route per-arm values through the SAME locals and hoist the
  unchanged initializer above the branch: `w2 = w; h2 = h; if (wide)
  w2 = w/2; else h2 = h/2;` then identical call blocks in both arms.
- **Don't hand-write strength-reduced pointers.** Plain array indexing
  `tab[j].field` in a loop produces the original's pointer walk
  (`add ptr,0x40`, `[ptr-4]`) by itself; an explicit `int *p` walk risks a
  different anchor field and operand order.

- **`memset(p, 0, 12)` is the xor+reg-store triple.** Three consecutive
  zero stores through one register (`xor edx,edx; mov [ecx],edx;
  mov [ecx+4],edx; mov [ecx+8],edx`) are the /Oi small-memset expansion,
  NOT three `= 0` statements (those emit imm stores, and a hand-written
  zero variable constant-folds away). Proven BrS17BankFlip.
- **Trailing void cdecl call = tail jmp.** A void function whose last
  statement calls a void cdecl function compiles the call as a plain
  `jmp` under /O2. Proven BrRaceClockReset.
- **Wrapper/body splits hide free matches.** When the port factored a
  helper out of an address (wrapper carries the tag, body sits untagged
  in another module), the original inlined everything: either move the
  tag to the body (BrS17BankFlip, BrRenderCountersReset) or give the
  wrapper a BR_MATCHING_BUILD twin with the body inlined and globals as
  direct DAT_ externs (the six-member DirectPlay send family, BrHookTakeA/B,
  BrDlCmdFogColour, BrScratchRingDrain). Port-safety indirections (ops
  tables, sink callbacks, factored range-checks) are the same class.
- **`&extern_var != NULL` is not folded.** A `mov edx, offset DAT; test edx,edx;
  je` before a strcpy that uses that same address is a real source-level
  `if (p != 0)` / `if (&buf != 0)` on an extern object. VC5 /O2 does NOT
  treat the address of an extern as a proven non-null. Proven
  BrMenuCopyTrackName (0x1003B020, 144 B).
- **The epilogue-signature audit finds these mechanically.** Compare frame
  size, ret/pop-cluster count, callee-saved push count and first-push
  offset between orig bytes and the recomp (tools/sigaudit.py): epilogue
  or push-count mismatches on small-diff rows are structural (idiom-fixable),
  not register residue. Skip functions whose orig starts `push -1` (C++ EH).
  One session of working the flagged list produced 18 new matches.

- **`and esp,-8` means an 8-byte-aligned local.** VC5 emits
  `push ebp; mov ebp,esp; and esp,-8` only when something in the frame
  wants 8-byte alignment: a `double`, an `__int64`, or Ghidra's `float10` /
  `undefined8` / `unkbyte10` (the pipeline rewrites those to `double` /
  `__int64` in `tools/ghidra_to_match.py`). That steals ebp as a frame
  pointer. A function whose original starts `sub esp, N; push ebx; push
  ebp; …` with `xor ebp,ebp` is using ebp as a general register (often
  the zero register) — it has no 8-byte local. Retype or delete the one
  that does, and the compiler goes back to `sub esp, N` and frees ebp.
  Observed on 0x10019A70: orig `83 ec 34` vs a draft that declared
  `float10 fVar22` (an x87 return, not a stack slot) and compiled
  `55 8b ec 83 e4 f8`. Instruction one cannot match until the aligned
  local is gone; nothing downstream can until instruction one does.

- **Empty `int f();` prototypes promote float args to double.** In C, a
  call to an unprototyped function converts `float` arguments to
  `double`, which emits `fstp qword [esp]` and is enough to force
  `and esp,-8` on the caller's frame. Giving the callee its real
  `float` parameters removes the aligned prologue. Proven 0x10019A70:
  after `#include "br_vec.h"` (so BrVec3Lerp/ScaleBy/… take `float`),
  `and esp,-8` disappeared and the four-register push matched; the
  frame is still 8 bytes large (`sub esp, 0x3c` vs orig `0x34`) from
  one remaining qword-arg call.

- **0x10019A70 is one C function, last among the big targets.** 11,223 B
  (11,223 / 480,853 of BRGlide.dll `.text`). Do not split it and do not
  transcribe all 11 KB in one pass. Win `sub esp, 0x34` first (0x34-byte
  frame, no 8-byte-aligned local), then grow section by section and watch
  the first divergence march forward. It makes 131 distinct calls; every
  wrong callee signature (stdcall vs cdecl, arity) corrupts the call site
  and the stack cleanup, so it wants those callees matched first. The
  port's Clock / Begin / Frame split is not a matching twin. Protocol:
  `include/br_racestep.h`.

- **Two-constant ternary `c ? K1 : K2` is neg/sbb/and (K1−K2)/add K2.**
  Ghidra decompiles it as `(c ? K1-K2 : 0) + K2` — fold the add BACK INTO the
  ternary or the expression scheduler treats the `+` as a separate node and
  reorders the whole or-chain around it. Proven 0x1000EAF0's four
  `b7000000` geometry-mode words: `(A ^ B ? 0x1000 : 0x2000) | ...` matched
  where `((A ^ B ? 0xfffff000 : 0) + 0x2000) | ...` evaluated the or-terms
  in a different order.
- **Same-width flag reads CSE into one register load.** `mov ax,[esi+0x4c];
  test eax,0x4a4; ...; test al,0x80` means the source read a USHORT field
  once and tested it twice — spell every mask test in the group off the same
  `*(uint16_t *)` read. A `uint32_t` read for one test and `uint16_t` for
  another blocks the CSE and emits a memory-direct `test word ptr` plus a
  re-read. Proven 0x1000EAF0 (post-draw flag group).
- **An uncancelled `x + (y - x - K)` means the subtrahend was a VARIABLE.**
  `mov eax,0xffffd620; sub eax,ecx` hoisted to a slot, then
  `iw*0x40 + param + [slot] + cursor` summed at the use, is
  `int negBase = -(param + 0x29e0);` assigned to a local before the loop —
  spelled inline, VC5 algebraically cancels the ±param and the hoist
  vanishes. The negated-base local blocks the cancellation and LICM spills
  it. Proven 0x1000EAF0 (wheel-record addressing).
- **A `lea reg,[base+index]` materialized after `[base+index+disp]` loads**
  means the source read fields through a two-part sum before binding the
  combined pointer: `dx = *(float *)(wb + (int)pCar + 0x50);` then
  `pW = (float *)(wb + (int)pCar);` — folding both into pW first makes the
  compiler pre-merge into one register. Proven 0x1000EAF0.
- **A redundant-looking re-test of an unchanged variable is two SEPARATE
  ifs.** `if (x) {A} ... if (!x) {B}` re-loads and re-tests x when the
  distance is long (VC5 does not value-number across it); writing
  `if (x) { A; goto tail; } B; tail:` folds the second test into a `jmp`.
  Match the original's pair of tests with a pair of ifs. Proven 0x1000EAF0
  (param_2 trail-section exit).
- **Slot sharing crosses variables ONLY via block scope.** /O2 packs a
  block-scoped local into the dead slot of ANY earlier variable (bSolo's
  slot reused for a trail-loop float), refining the per-VARIABLE rule:
  function-scoped locals never share with each other, but block-scoped ones
  overlay dead function-scoped slots. When the original's frame is smaller
  than the visible variable count suggests — or a Ghidra local changes type
  mid-function — the second role was a block-scoped variable. Declaring the
  right locals block-scoped (fMax/fMin/scale inside the transform arm, the
  drain loop's own counters) is how the frame converges. Proven 0x1000EAF0.
- **The fld-side of both-memory fmuls depends on the global's DECLARATION
  FORM, not its spelling at the use.** Three forms of the same global
  compile differently: `extern float g[16]` (reloc value 0),
  `float g[16] = {1.0f}` (defined in-TU — reloc plus a real .data offset),
  and `(*(float *)0x106e9a38)` (absolute, no reloc). On 0x1000EAF0 the
  defined-in-TU form reproduced the original's scale block (slot preloaded
  8x on the fld side, globals on the fmul side) where extern form matched
  too; the absolute form reproduced the row transforms (globals on the fld
  side) but broke the scale block. No single form fixed both blocks, and
  within a form, operand order, sum association, term order (all 24
  permutations scored identical), array-vs-scalar and cast spellings change
  NOTHING — the per-expression schedule is canonicalized. So the residual
  wall is narrower than "float DAG scheduling": it is the operand-RANKING
  interaction between declaration form and reuse count, and a function can
  be left with ONE block mirrored while the rest matches. Downstream: the
  mirrored block shifts spill lifetimes, so a slot-layout cascade behind it
  is the same single blocker.
  SECOND-PASS REFINEMENT (0x1000EAF0 rows, hand-simulated from the bytes):
  the MIXED spelling that reproduces both operand-role patterns is
  absolute-address derefs for the row transforms plus an extern-symbol
  spelling for the scale products, in ONE TU — no single declaration does
  both. The original's row schedule is tree ((f+c)+e)+d with the fadds
  DEFERRED two products behind and the next row's loads interleaved; a
  sequenced barrier (`float t = f_term;` then the rest as one expression)
  reproduces the first six instructions exactly but forces the first add
  early, and no probed form defers it: 24 term orders x left/right/balanced
  trees, comma-joined rows, local += chains (spills), global += chains
  (emits fst per step), two-temp forms (spill), /G3-/G6/GB, /Op, /Oa, /Ow,
  /Ox, /Os, /O1 all fail differently. ~15 instructions of fxch/ordering
  residue in one block is the function's floor pending a genuinely new
  insight (different compiler patch level? an unprobed pragma?).
  FOURTH-PASS (2026-08-25): the wall above is BROKEN — its floor was an
  artifact of the absolute-deref spelling.  What actually governs the two
  float blocks (all proven by A/B compiles on 0x1000EAF0):
  * **Store deferral is an ALIAS question.** VC5 moves a global LOAD above
    a pending global STORE only when both sides are named symbols (symbol
    A vs symbol B = provably distinct).  An absolute `*(float *)0x...` on
    either side kills the reorder, which is why the absolute spelling
    could never reproduce the original's software-pipelined rows (next
    row's fld-side loads hoist above this row's fstp).  Spell the store
    target as an extern symbol (`DAT_106e78f0[k]`), never as an absolute
    deref, when the original defers stores.
  * **The fld/fmul side of `a*b` follows an operand-KIND ladder,** not
    spelling permutations, displacement values, escape, declared size, or
    use counts (all probed): absolute-const deref > deref of a plain
    pointer copy of the TU's "main" array symbol > deref of a
    pointer-to-pointer-arithmetic result / pObj[literal] > deref of a
    plain copy of any OTHER symbol.  The higher kind takes the fld side.
    A product's roles are set by its own pair; swapping the source
    operand order NEVER changes them (mul operands canonicalize).
  * **Product ORDER in a sum canonicalizes only between comparable
    products.**  Two products whose operand-kind pairs are the same shape
    get sorted (by displacement); pairs of DIFFERENT shapes keep source
    order.  The original's per-row product order (V12*tw first, then V0,
    V8, V4) is reachable only by giving the four products distinct pair
    shapes — the landed spelling is absolute*pPos / symPtr*arithPtr /
    absolute*pPos / symPtr*arithPtr with `pPos = pObj + 0xc` etc.
  * **Load hoisting is binary by the same kind ladder:** absolute and
    main-symbol-pointer loads hoist aggressively (2 products ahead);
    other-symbol-pointer loads do not hoist at all.  No probed kind gives
    the intermediate one-notch hoist the original's fourth product shows
    (orig `fld V4; fxch3; faddp2` vs ours `fxch2; faddp1`) — that is the
    current 2-insns-per-row floor of the row block.
  * **The scale block's 8+4 batch split** (8 slot-preloads, drain, 4
    more) is the inliner's region boundary: spell it as `static __inline`
    per-row helpers (BrRowScale8/BrRowScale4).  One fld still leaks
    across the batch boundary (scheduler refills greedily after the
    first fstp frees a slot).
  * **`uint16_t idx` vs `int idx` decides the zero-extend idiom**: the
    original's `mov di, [mem]; and edi, 0xffff` (load into live reg +
    mask) comes from a uint16_t variable; `int` gives `xor edi, edi;
    mov di, [mem]`.  Retyping also flipped `mov eax,edi; shl eax,3` to
    the original's `lea eax,[edi*8]` for the `idx*0x54` scaling.
  * **A 16-bit flag test whose mask global is read 32-bit** (`mov eax,
    [mask]; mov dx, [flags]; and edx, eax; test dx, dx`) means the mask
    global was declared int-sized and the result truncated:
    `(uint16_t)(*(uint16_t *)p & mask_int) == 0`.
- **A probe is only evidence if the compile actually ran.** match_sweep
  compiles NOTHING when the file has no `@implements` tag — it returns
  before the compiler is invoked and every diff silently reuses the stale
  .obj. Five spelling probes on 0x1000EAF0 were judged this way and all
  five conclusions were wrong. Keep the tag on while iterating (a
  tagged-but-diffing row is normal working state); the tell is `0/0 tagged
  functions` in the sweep output.
- **`float a = g[0], b = g[4]; x = a*m1 + b*m2 ...` ICEs VC5** (fatal
  C1001) when the products feed a 4-term sum inside a deep if-else — the
  named-temp lever cannot even be TESTED on fmul row shapes, and the
  original source cannot have been spelled that way.

- **`(*p & MASK) < K` narrows only the cmp.** A plain int compare after an
  `&` mask emits the dword load + 32-bit and, then just the `cmp` narrowed
  to the byte register (`mov ecx,[esi]; and ecx,0x3f; cmp cl,5; jge`).
  A `(char)` cast on the expression narrows the whole chain to a byte load
  (`mov cl,[esi]; and cl,0x3f`); an int local flips the web to eax/al.
  Proven BrNetPeerMsgCancel 0x1006A3F0.
- **Returning the assignment forces the quotient copy.** `return g = x / K;`
  emits `mov eax,edx; shr eax,N; mov [g],eax` after the magic multiply —
  the copy into EAX exists because the value is returned.  A void function
  shifts EDX in place.  Proven BrReplayCountFromBytes 0x10063DB0.
- **`x ? 0 : -1` vs `(x != 0) - 1`:** the ternary compiles to the branchless
  `neg; sbb; neg; dec` chain (extends the /Od-ternary idiom to /O2 returns);
  the arithmetic spelling emits `xor; test; setne; dec`.  Proven BrCrtAtExit
  0x100745E0.
- **Byte-pair swap: one temp, temp holds HIGH.** `t = p[hi]; p[hi] = p[lo];
  p[lo] = t;` per pair, with the SAME temp variable reused across both pairs
  of a dword swap, reproduces `mov al,[hi]; ...` with transients in AL and
  the direct-move RHS in cl/dl.  Ghidra prints the temp-holds-mid rotation
  and BrSwap4's temp-holds-low form; both rotate the byte registers (8 diffs).
  Proven BrTrackFixupSegList 0x10031910 + BrTrackSwapRec28 0x10031A40.
- **Loop-latch field test wants a VALUE temp.** `iNext = p[7]; p += 4;
  } while (iNext)` loads `[esi+0x1c]`, advances, tests.  A POINTER temp
  (`pNext = p + 7`) additionally materialises a dead `lea eax,[esi+0x1c]`.
  Proven BrHudTextListDraw 0x10013140.
- **thiscall with a short stack arg = struct-short via __fastcall.**
  `typedef struct { unsigned short v; } Arg;` as the stack parameter of a
  `__fastcall(this, Arg)` twin reproduces `mov ax,[esp+0xc]` (16-bit arg
  load) and `ret 4`.  Proven BrBitStreamWriteU16 0x1006CFC0.
- **Glide-API calls are E8 to local thunks, CRT via MSVCRT stays FF 15.**
  Declare glide2x entry points as plain `__stdcall` externs (grTex...) and
  the per-DLL CRT glue's __dllonexit as a plain extern; `_onexit`/`exit`/
  stdio keep `_CRTIMP` dllimport.  Proven BrCrtOnExit 0x100745B0,
  BrTex3dDownloadAt 0x100283C0.
- **Ten originals carry a fused 16-byte link preamble** (`e9 0b 00 00 00` +
  11×`90`, jmp over nops to the body at +0x10) inside their map entry —
  link-stage output, unreachable from C, same category as relocs/thunks.
  They are enumerated in `config/preambles.csv`; the comparator
  (`match_sweep.load_orig`) verifies the recorded preamble VERBATIM and
  matches the compiler body at +0x10, so every original byte stays
  accounted for.  Never try to spell the preamble in source.  Proven
  0x1001E220 (FPS screen-W/2 setter): the untouched Ghidra body scored 0
  the moment the preamble was stripped.  The family: 0x10017F30,
  0x1001E1E0/200/220/250/280/2B0, 0x100376B0/E0, 0x1005A480.

## Cost model (measured, 2026-08-22 timed test)

Size is not the cost driver — code shape is. 738 B of int/call-heavy code
matched in ~10 min; a 165 B float function (BrVec3Project) is permanently
stuck at 30 diffs of scheduling residue. Int/call-heavy code sustains
5–10 min/function at any size once the file's context is loaded. The float/
x87 cluster carries the walls and is a separate workstream.
