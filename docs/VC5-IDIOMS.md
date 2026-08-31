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
- **Two stores, then an eax-clobbering load: mention the edx value first.**
  `r.p1 = param_2; r.p2 = param_3; flag = r.f268;` hoists param_2 into edx
  and param_3 into eax; the flag load clobbers eax, so param_3 is stored
  first even though it was assigned second. Writing the stores in byte
  order (`r.p2 = param_3; r.p1 = param_2`) inverts the hoist — 4
  displacement diffs, REGNORM 0. Ghidra's `unsigned short iStack_40` for
  that flag is a mis-type: orig is `mov eax, dword [slot]` /
  `mov [slot], edi`. Proven 0x100298C0 (216 B, MATCH /O2).
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
  Dropping the local matched BrSegPtrFixup (43 B) outright. Same for
  `ftell(*pp); fseek(*pp,...)` — orig `mov r,[esi]` at every CRT call,
  IAT slots CSEd into edi/ebp; `FILE *f = *pp` folds those. Proven
  0x100032D0 BrChkFileSize (70 B, MATCH /O2).
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
  **Void function: `return` not `break`.** Empty `case X: break;` groups
  fold into default. `case X: return;` keeps a unique jump-table slot even
  when the slot address is the same `pop; ret` as default. DirectPlay
  DPSYS_* (3, 0x21, 0x31, 0x101, 0x102, 0x103 empty; 5 and 0x107 live)
  is a 0x31..0x107 two-level table. Proven 0x10009530 BrDPlaySysMsgDispatch
  (116 B body, MATCH /O2).
- **The residue ceiling on big int functions:** four of five 490–700 B
  functions landed at 4–24 divergent bytes, every one an allocator choice
  (byte-reg pick, esi/edi role, imm-vs-pooled constant). Getting to that
  ceiling took 3–10 min each; crossing it needs a type/shape insight (as
  byte-width returns were for BrCarStatePack) or does not happen.

- **Unrolled thiscall search: vtbl[1] builds a stack key, then 16
  dword compares.** Signature is `__fastcall(this, live_edx, arg)` /
  `ret 4`; pass `pArg` as the edx slot so the vtbl call is
  `push &key; push arg; mov ecx,this; call [vtbl+4]` with no
  `xor edx,edx`. The bound is unsigned (`test n; jbe` / `cmp i,n; jb`).
  A nested 16-iter loop stays a loop. Proven 0x10008850 BrKeyCacheFind
  (213 B, MATCH /O2).

- **thiscall with stack args = `__fastcall(this, int _edx_unused, args...)`.**
  Same ECX `this`, same stack layout, same `ret N`; an unused EDX slot costs
  no bytes. Generalizes BR_THISCALL1. Proven on BrBoundsFits_10058CC0 and
  four C++ scalar deleting destructors (`push esi; mov esi,ecx; call ~T;
  test byte [esp+8],1; jz; push esi; call operator delete`).
  **Call site: do not pass literal 0 for `_edx_unused`.** That emits
  `xor edx,edx` (the Ghidra `CC_fast_4(..., 0, args)` miss). Pass a value
  already live in edx — here the first stack arg, which the original loads
  into edx (`mov edx,[esp+4]`) and then `push edx`. `f(this, param_1,
  param_1, …)` keeps ecx=this, four stack args, no edx setup. Proven
  0x1003AF30 (42 B, MATCH /O2).
  **Vtbl call `push 1; mov ecx,this; mov edx,[ecx]; call [edx+0x18]`:** pass
  `pThis->pVtbl` as the edx slot so edx IS the vtable, and keep `pGame->pSub`
  inline so `push 1` precedes the ecx load. After the call, a named temp
  `p = DAT_src; DAT_dst = p;` is `a1`/`a3`; `DAT_dst = DAT_src; return 0;`
  hoists `xor eax,eax` and copies through ecx. Proven 0x1003D4F0 (30 B).
  **Slot 0 with one stack arg is `mov eax,[ecx]; push 1; call [eax]`.** C
  `__fastcall` edx-slot CSEs the vtbl into edx (`call [edx]`). Match the
  caller as C++: `pObj->f00(1)` on a `virtual` method. No named temp on
  the later pointer copy (`DAT_dst = DAT_src`) keeps ecx. Proven
  0x1003D4A0 / 0x1003D510 / 0x1003DA10. Non-EH C++ TUs (no `new`) have
  no FuncInfo on either side — that is a 4/4 vacuous sidecar, not a miss.
- **C++ `/GX` `new T` (maxState=1 op-delete):** thiscall member, `if (p == 0)
  { p = new T; … } else { cur = p; } return 1;` with the `return 1` AFTER the
  if/else. `if (p != 0)` or `return 1` inside both arms CSE's 1 into the
  f0C/f68 stores (`89` vs orig `c7`). DECLARE ctor, no dtor (unwind is
  `??3` only). Six activates matched: docs/cpp-family2-notes.md.
  Sequential `new`s in one function: maxState = count, every toState=-1,
  every unwind action 11 B `push; call ??3; pop; ret`.
- **C++ thiscall `PutByte(unsigned char)` not `unsigned`:** a char stack
  arg used in a byte `or` stays `mov bl,[esp+N]` only when the callee
  prototype takes `unsigned char`. `PutByte(unsigned)` promotes the arg
  to a dword load + `and ecx,0xff` (178 diffs on 0x10004900). Same
  family: `volatile int g_id` keeps `mov eax,[g]; and al,0xf` (plain
  `int` becomes `mov al,[g]`). Proven 0x10004900 / 0x10004AD0.
- **CSE a mask twice, don't store it:** `if ((a8 & 0x3F) <= 2)` then
  `if ((a8 & 0x3F) == 4)` emits `mov ebp,esi; and ebp,0x3f` (esi stays
  the unmasked value). `kind = a8 & 0x3F` ands esi in place then copies
  (5 diffs). Proven 0x10004AD0.
- **Live-eax retest without an early return:** 0x1003DC20 used
  `HostGo(); return 1` so a second top-level `if (g_host)` kept eax.
  When a later tail must still run, `goto` past the second `if (h)`
  from the first-time arm does the same (`test eax; je` on the
  already-inited path). Proven on 0x1003DD20's host tail (obj-hook
  eax/ecx coloring remains).
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
- **A nested `if (p) { …; if (p && x) }` deletes the second null test.**
  Sibling `if (p) { … } if (p && x)` keeps `cmp p,0; je`. Proven
  0x10009A40 BrDPlayShutdown (178 B, MATCH /O2).
- **Two tests of the same `hr >= 0` (one wrapping the body, one as the
  loop-exit) keep a shared `add esp; cmp; jge loop`.** A single
  `if (hr < 0) break; body; loop` lets /O2 duplicate `add esp` into each
  call arm and jump to the top, dropping the second compare. Proven
  0x10009880 BrDPlayPump (236 B, MATCH /O2). COM methods on that path
  are stdcall (local vtable typedef; the header's cdecl slots emit
  `add esp`).
- **Win32 thread proc is `__stdcall` (`ret 4`).** `WaitForMultipleObjects`
  IAT loaded into a callee-saved, `ExitThread` via FF 15. Header cdecl
  renamed around the include. Proven 0x10009970 BrDPlayThreadProc
  (93 B, MATCH /O2).

- **Function pointers in a table are `__stdcall`, not `typedef int (*fp)()`.**
  Orig `call [DAT]; test eax,eax` (no cleanup). Ghidra's empty `int (*fp)()`
  is cdecl and emits `call [DAT]; add esp,N` (+3 bytes per call). Four
  stdcall calls (4/1/2/2 args) were the 14 extra bytes on 0x10002580
  (`83 c4 10` right after the first `FF 15`, shifting every later je).
  Distinguisher: `FF 15 … 85 c0` vs `FF 15 … 83 c4`. Also: `if (fp(...)
  != 0)` (no temp) is `test eax,eax`; `iVar1 = fp(...); if (iVar1 != 0)`
  with a live zero-register is `cmp eax, esi`. Proven 0x10002580 (CD
  redbook init, 471 B). Remaining residue after this fix is the store
  burst: orig reloads caller-saved scratches (`xor edx,edx; mov eax,4;
  xor ecx,ecx`) for a 7-store prefix then hoists `push 0x10000020`
  after the handle load; ours uses esi for every zero and hoists the
  push immediately. Named temps, comma-join, volatile load, and a
  handle local all failed to force the edx/ecx pair — that burst is
  scheduler-internal once the stdcall shape is right.
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
- **A `fmul dword` chain is float, not double.** Orig 0x10016AA0
  (BrWeatherStepWind) is `fild; fmul [k]; fsub [1]; fmul [dt]; fadd [angle];
  fst; fcomp`. Double casts emit `fld; fld; fmulp` (REGNORM extra 12 fld +
  5 fmulp). Wrap/clamp became instruction-identical once spelled in float.
- **sincos of a global: integer-bit copy plus fld of the global.**
  `int ia = *(int*)&angle; z=0; *(int*)&a = ia; x=(float)cos(angle)*s*dt;
  y=(float)sin(a)*s*dt` emits orig's `mov eax,[angle]; mov [z],0; fld
  [angle]; mov [esp],eax; fld [esp]; fxch; fcos; fxch; fsin`. A named
  `float a = angle` CSEs both loads back to the global. Proven 0x10016AA0.
- **Dual fst-home of live fcos/fsin through one slot** (`fxch; fst [esp];
  fxch; fst [esp]; fxch` before the interleaved scale-out) is /O2 skipping
  the double-to-float of the first (cos) result. `/Op` on the function also
  rounds the fild path (`fstp; fld`) which orig does not. A volatile store
  of cos lands the first fst but keeps the slot live (epilogue restore).
  Same 3-insn hole in a solo TU. 0x10016AA0 firstdiv +0xf7, REGNORM 0+3.

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
  Proven BrTex3dExpandInto (617 B). A prototyped `unsigned char` that
  CSEs with a prior dword load of a nearby field is `mov al,[esi+N];
  push eax` (high bits leftover — Ghidra CONCAT31). The same expression
  to an unprototyped `int f()` default-promotes: `xor eax,eax; mov al;
  push eax` (+2 insns per site). Distinguisher: orig `8a 46 04 51` vs
  `33 c0 8a 46 04`. Proven 0x10027710 (319 B /O2).
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
  different anchor field and operand order.  A named `BrTexTile40 *pTile
  = &tab[i]` for the *base* tile (not the walk) emits `shl eax,6; add
  eax, &tab` (full pointer, extra stack slot, frame +4) where orig is
  `mov eax,edx; shl eax,6` then `[eax+tab+field]` (offset only).  Proven
  BrTex3dRegister: the extra slot is 0x2b4 vs orig 0x2b0.

- **`h = 1` then `(g > h)` then `w = h << s; h = h << t` is one web.**
  Orig `mov ebp,1; cmp edi,ebp; setg; mov ebx,ebp; shl ebx,cl; shl
  ebp,cl`.  Spelling `fTmu2 = (g > 1)` with a literal emits `cmp ecx,1`
  plus a later `mov edx,1` that steals edx from the base CSE.  Merging
  1/h makes the compare a register-register `cmp` and copies 1 into the
  w-shift register.  Does not by itself flip orig edx vs ours esi for
  base — that coloring is the next bullet.  Proven BrTex3dRegister.

- **Caller-saved vs callee-saved for a no-call-crossing CSE is first-
  region coloring, not a live-across-call choice.**  On BrTex3dRegister
  the DAT_106b7ab0 CSE dies in the LOD walk (orig `lea ecx,[edx+1]; mov
  edx,ecx` kills edx; ours `inc esi` kills esi) and the fmt-call uses
  leftover `eax = base<<6`, not base.  Orig still loads it `mov edx,
  [DAT_106b7ab0]` *before* the callee-saved pushes; ours `push esi; mov
  esi,[DAT]`.  Truncating *before* the fmt-call with stores kept live
  puts base in eax (caller-saved); including the fmt-call (or any later
  DAT_106b7ab0 use) flips it to esi even when those later uses are
  `*(volatile int *)&DAT` and r.iLevel is store-only.  First-region
  pressure is 7 values / 7 regs (base, max, 1/h, w, sMask, offset,
  shift); orig colors base=edx, max=esi, 1=ebp, w=ebx, sMask=edi;
  ours is that rotation with base in esi.  Ruled out as levers (all
  byte-probed): decl rename (identity-blind), late r.iLevel store,
  clamp-via-global vs clamp-via-field, hand-written tile pointer,
  `__asm nop` (forces ebp frame pointer, prologue dies).  Volatile
  post-call loads recover leftover-eax fmt-call shape (1177 diffs of
  1755) but not the edx load.  Residue-ceiling wall on the 1,755 B
  function: the coloring is determined, not a coin-flip, and no
  remaining C-level lever moves edx vs esi.  Proven BrTex3dRegister
  2026-08-26.

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
- **A state-pointer argument that the original never loads is absolute
  globals.** Orig `xor eax,eax; mov ecx,1; mov [DAT],eax` / `mov [DAT],ecx`
  (plus `c7 05` for a 2 and a pointer imm) is ten stores to named externs
  and reads nothing off the stack. The port's `pState->field = K` emits
  `[ecx+off]` and misses the whole body. Spell the matching twin as
  `DAT_x = 0/1/2; DAT_ptr = DAT_obj;` in source order. Proven 0x10037660
  (66 B, MATCH /O2). Same family: `int v = K; word_g = (int16_t)v;
  dword_g = v;` is `mov eax,K; mov word [DAT],ax; mov [DAT2],eax; ret`
  (0x100376B0 / 0x100376E0, 17 B bodies after the 16-byte link-stage
  `jmp +0x0b` / 11-nop preamble in config/preambles.csv).
- **Stride-0x978 global struct array is `slots[i].field`, not `imul`.**
  `slots[i]` of a 0x978-byte struct compiles to `lea ecx,[eax+eax*4];
  lea ecx,[ecx+ecx*4]; lea eax,[eax+ecx*4]; lea esi,[eax+eax*2];
  shl esi,3` then `[esi+base]`. An `int` array indexed by `i*0x25e`
  emits `imul`. A pointer local (`p = &slots[i]`) folds the base into
  esi and encodes `[esi]`/`[esi+off]`, not `[esi+base]`. Three field
  accesses (`hMutex`, `f02C`, `hMutex` again) keep the ReleaseMutex
  handle as a reload. WaitForSingleObject must be the dllimport stdcall
  (`push -1; FF 15`) — a cdecl wrapper is `E8` + `add esp` and was the
  entire 4-byte miss. Proven BrNetSlotSetF02C 0x10004DC0 (60 B).
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
  FIFTH-PASS (2026-08-30): the in-place-fchs lever has no remaining
  abs/negate site here (one `fchs`, already `fld fMax; fld fMin; fchs;
  fxch; fcompp`).  The same dest-once class IS the trail distance
  check: `if (d1=a, d2=b, d1<K && d2<K)` keeps both sqlens live for
  orig's `fst` homes + dual fcomp (REGNORM extra 66→60, miss 64→56;
  that DAG is instruction-identical).  `&&` of the raw products
  short-circuits to fstp-st + recompute; statement-then-if extra-stores
  the temps and splits a region.  Still 18 slot-masked regions.  Walls
  1-3 (D-product one-notch, scale 5|7 vs 8|4, frame 0xd4 vs 0xdc) plus
  trail ox/oy `fst` homes (named px/py integer-home instead of x87
  preload) resisted: global-symbol helpers, nested-scope k copies,
  volatile dest barrier, counted loops, comma D-preload, /Op, volatile
  frame pad, nTotal-hoist, pPos-shared probe arg -- none dropped a
  region.  Extra live floats grow the frame to 0xdc but inflate the DAG.
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
- **Unrolled Vec3 bswap: inner pair high is p[2], not p[1].** Loop of 3
  dword swaps is 43 B vs orig 75 B fully unrolled. Per dword:
  `t=p[3]; p[3]=p[0]; p[0]=t; t=p[2]; p[2]=p[1]; p[1]=t;` (and +4/+8).
  Temp-holds-LOW or inner `t=p[1]` rotates the cl/dl schedule. Proven
  0x10018AF0 BrSwapVec3 (75 B, MATCH /O2).
- **Accessor thunks vs a global are a CALL vs `mov r,[DAT]`.** Port
  helpers `int g(void){return DAT;}` emit a call; orig loads the global
  at the call site. Spell `extern int DAT; f(..., DAT, ...)` under
  BR_MATCHING_BUILD. Proven 0x1001CDD0 BrAppStateLoading (67 B, MATCH /O2).
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
- **`/Oi` string ops are CRT calls, not exploded scans.** Orig
  `or ecx,-1; f2 ae; not ecx; shr ecx,2; f3 a5` is strcpy/strcat;
  `or ecx,-1; f2 ae; not ecx; dec ecx; je` is `if (strlen(s) != 0)`
  (Ghidra's signed `i = -1; … if (i != -2)`); `xor eax,eax; mov ecx,N;
  f3 ab` is `memset(p, 0, N*4)`; `mov ecx,N; f3 a5` with no preceding
  scasb is memcpy of a known size. `extern int s_*` loads the first
  dword (`mov r,[s]; push r`); orig `push offset s` is `extern char
  s_*[]`. Folded as one combined candidate (`stringops` in
  `_refine_candidates`). Do not convert a stride-loop inner copy
  (0x100013F0) or a comparison-only scasb (0x10040A90). Proven
  0x10038490 / 0x10038550 / 0x100387C0 (strlen), 0x10023900 /
  0x10033C90 (memcpy), 0x100418C0 (memset), 0x10055AF0 (strcpy/strcat +
  memset 0x104 + char[]).
- **Byte return is `char`, not `int`.** Orig `mov al,1; pop*; ret`
  (`b0 01 5b c3`) vs wrap `int`'s `b8 01 00 00 00 c3`. Also `xor al,al`
  / `or al,0xff` before the pops (`return (char)0xff`). Orig-gated in
  `refine_function` (ungated `int`→`char` would rewrite every `return 1`).
  Skip fnstsw helpers whose AL is a status nibble (0x10006A10). Proven
  0x10054390 (tree) and 0x10069930.

- **`1 << ((char)*(int *)p - 1U & 0x1f)` is decoration.** Orig is
  `mov ecx, dword [p]; dec ecx; shl r, cl` (and `mov ecx, dword [p];
  shl r, cl` for the un-decremented maskT). The `(char)`/`(byte)` cast
  emits `mov cl, byte [p]` plus `and ecx, 0x1f`. Delete both. Proven
  0x100250D0 (BrTex3dExpand) at every tile maskS/maskT shift.
- **Ghidra inverts `if ((flags & 2) && lod == 1)` to the CI4-first
  `if ((flags & 2) == 0 || lod != 1)`.** Orig is `test byte [flags], 2;
  je CI4; cmp lod, 1; jne CI4` with IDX4 as the fall-through. Swap the
  arms. Proven 0x100250D0 at 0x10025148.
- **LOD walk is empty-check then `do {…; lod++;} while (lod < end)`,
  not test-at-top `for(;;) { if (lod >= end) return; }`.** Orig
  `cmp lod, end; jge ret` then shrink-wrapped `ebp=cbOut; esi=pOut;
  ebx=aTile`, body, latch `inc lod; cmp lod, end; jl body` (jl target
  is the aTile reload, not the first compare). Putting `pOut = param_1`
  before the empty-check prevents the sink. Proven 0x100250D0.
- **Byte live across a call is `mov [esp+slot], dl; mov ebx, [esp+slot];
  and ebx, 0xff`.** Two nibble uses of one unsigned char with
  FUN_100271f0 between them (CI4 palette). Ghidra's one `bVar11` for
  every arm shares a slot; orig has a slot per call-crossing loop.
  CONCAT22 at those call sites is `mov dx, word [pal]; push edx`
  (leftover high bits) — the source passes the ushort, not a
  reconstructed dword. Proven 0x100250D0.

- **Nested call, not address-temp + inner call.** Orig
  `push &DAT; push 0; call GetModuleHandleA; push eax; call f` is
  `f(GetModuleHandleA(0), &DAT)` (args right-to-left, address already
  on the stack before the inner call). Ghidra CSEs `&DAT` into a temp
  (`ppi = &DAT; h = GetModuleHandleA(0); f(h, ppi)`), so the address
  sits in a register and the first push is the inner call — first
  divergence at that push. Same family: `msg(BrStrGet(id), hr)` is
  `push hr; push id; call BrStrGet; add esp,4; push eax; call msg;
  add esp,8`. Ghidra's `t = BrStrGet(id, hr); msg(t)` gives BrStrGet
  a phantom second arg (orig is cdecl 1) and drops hr from msg.
  Proven 0x10035400 (DirectPlay init, 307 B MATCH /O2).

- **Flag stores belong in both arms, `!= 0` fall-through first.** Orig
  `cmp [g], edi; je z; mov [a],2; mov [b], ebx; jmp join; z: mov [a],ebx;
  mov [b], edi`. Ghidra's `if (g == 0) a=1; else a=2; b = (uint)(g != 0)`
  re-tests `g` and emits a second compare. Spell the stores inside the
  arms; `if (g != 0)` so the nonzero arm is the fall-through. The
  refine pass's `DAT:float` on a `cmp dword, edi` flag is poison.
  Proven 0x10035400.

- **Load the flag, then store the sibling.** Orig `mov eax, [flag];
  mov [other], imm; cmp eax, edi`. Ghidra stores `other` first then
  reloads `flag`. A named temp (`t = flag; other = imm; if (t == 0)`)
  restores the load-first order. Proven 0x10035400 (DAT_10ac4090 /
  DAT_10ac5d2c).

- **Map-split join is not a function — `long+N` is the tail.** The
  analyzer cut 0x10035400 at the if/else join `mov [DAT_10ac4090], ebx`
  (0x10035533, 174 B, no prologue). Ghidra correctly decompiled one
  C function; wrap's recomp is 496 B vs the 307 B prefix. score()
  trims to orig length, so the full source MATCHes 0x10035400 and the
  extra bytes ARE 0x10035533. 0x10035533 cannot be a standalone C
  function. nestcall-addr prey in the decomp corpus is exactly these
  two files (same dump). Do not mint; the remaining `long` residue is
  mixed (prologue-0, type-refine, other), not this shape. Proven
  0x10035400 2026-08-27.

- **I4 blend channels as unsigned char locals restore `sub esp, 0x68`.**
  Orig stores each 5-bit result (`(inten * d + lo) >> 3` and `>> 7` for
  A) as `mov byte [esp+0x13], dl` (and +0x9c / +0x7c) because the value
  is live across the next `imul 0x80808081` signed-div-by-255. Intensity
  is `mov [slot], al; mov ecx, [slot]; and ecx, 0xff`. Ghidra's one
  giant pack expression keeps those bytes in registers and the frame is
  `sub esp, 0x64` (one dword short). Naming `unsigned char bI4inten,
  chA, chR, chG, chB` and assigning them before the 1555 pack restores
  `83 ec 68` and instruction two (`mov eax, [esp+0x78]` = param_4).
  Proven 0x100250D0.
- **Ghidra SSA-reuses the tile pointer as IDX4 width.** Orig
  `lea edx, [lod*64+aTile]; mov [esp+0x30], edx` and every pitch-add
  is `mov eax, [esp+0x30]; mov ecx, [eax+8]`. Ghidra's
  `iVar5 = 1 << (*(int *)(param_11+0x60)-1)` overwrites the pointer, so
  pitch has to spell `param_11+0x48`. Keep iVar5 as `&tile[lod]` for
  the whole LOD body; IDX4 width is a separate temp (iVar17). Proven
  0x100250D0 IDX4 arm.
- **Budget checks are `iVar22 >= cbMax`, not `cbMax <= iVar22`.** Orig
  `add edi, 2; cmp edi, ebp; jge ret` (count vs cbOut in ebp). Ghidra
  prints `if (param_2 <= iVar22)` which emits `cmp cbMax, edi`. Flip
  every guard (`iVar22 + 2 >= cbMax` for the mid-texel test). Proven
  0x100250D0.
- **Expand hi/lo are `unsigned char`.** Matched caller 0x10024E60
  (`FUN_100250d0(..., char,char,char,char,char,char,char,char, int)`).
  Orig I4 loads `mov r, [esp+hi]; and r, 0xff`. maskOdd stays `int`.
  Proven 0x100250D0 / 0x10024E60.
- **A for/while bound that is a raw parameter is hoisted into a scratch
  reg at LOOP SETUP — before the pushes.** THE fix for the 0x100250D0
  insn-3 wall (broken 2026-08-27). Orig loads `mov ecx, [esp+0x90]`
  (param_10) at orig+0x7 while eax still holds param_4. A `do-while`
  references the bound only at the bottom latch, so the top guard loads
  param_9 lazily into edx instead (the wrong insn 3). Writing the loop
  as `for (lod = param_9; lod < param_10; lod++)` — bound is the RAW
  parameter, no `lodEnd` local — makes VC5 hoist param_10 into ecx at
  setup, reproducing orig+0x7 through +0x10 exactly. Use `lod < param_10`
  (emits `cmp eax, ecx`), NOT `param_10 > lod` (emits `cmp ecx, eax`).
  The lesson generalizes: an early load of a loop bound in scratch that
  a do-while defers is a control-flow-shape fix, not a spill trick — no
  prologue permutation reaches it (the whole merge/DCE/pragma do-not-
  re-run family stays dead; see docs/idioms-A.md). Next divergence
  (+0x11, param_4 spill vs esi-cache) is body-driven register pressure.
  Proven 0x100250D0.
- **Ghidra FOLDS two consecutive `count += N` into one `count += 2N` and
  retests the first guard against the pre-value.** THE fix for the
  0x100250D0 body (broken 2026-08-28; +1152 -> +512 B, +234 -> +81 insns).
  Ghidra prints `*p = A; if (c + 2 >= b) EXIT; c = c + 4; p[1] = B;
  p = p + 2; if (c >= b) EXIT;` for a source that reads
  `c += 2; *p = A; p += 1; if (c >= b) EXIT; c += 2; *p = B; p += 1;
  if (c >= b) EXIT;` — counter bumped BEFORE each store, pointer advanced
  by ONE element per store, one budget check per store on its own control
  edge. Semantically identical (the counter on the first exit path is
  dead — that path returns) but the folded form lets VC5 coalesce the
  pair into a batch (`mov [r]; mov [r+2]; add r,4`), which drops the
  output pointer's register pressure, frees esi, and ROTATES THE
  ALLOCATION ACROSS THE WHOLE FUNCTION. Generator:
  `tools/gen_countfold.py`, wired as the `countfold` refine candidate
  (12 of 16 sites unaided). Proven 0x100250D0 x15.
  **The lesson that generalizes past this function: a whole-function
  register rotation is usually a SYMPTOM of a structural source defect in
  the body, not a terminal coloring wall.** 0x100250D0 was written off as
  a coloring wall TWICE on the strength of a raw diff count; the rotation
  dissolved once the source defect was fixed. Rank residue by the
  REGISTER-BLIND multiset gap (`tools/fnmatch/`, `regnorm` mode), never by
  raw diff count — raw read 1097/863 where the honest structural gap was
  432/198.
- **A 16-bit lvalue narrows the whole feeding expression.** `*pOut = ((((chA
  << 5 | chR) << 5 | chG) << 5) | chB)` with `unsigned char` channel locals
  and an `unsigned short *pOut` makes VC5 load each channel with a
  16-BIT-destination `movzx ax, byte ptr [esp+S]` — only the low half is
  live, because the store is `mov word ptr [esi-2], dx`. Spelling the
  operands `(unsigned int)` instead forces `and edx,0xff` plus the
  in-register widening idiom `xor dx,dx; mov dl,al`. `(unsigned short)` and
  `(unsigned char)` operand casts are byte-identical — it is the LVALUE
  width that decides. Proven 0x100250D0.
- **A plain `unsigned char` local with 2+ uses is homed in a byte slot and
  read back as `mov r32, [slot]; and r32, 0xff`** — not `movzx r32, byte
  ptr [slot]` — and VC5 CSEs that widening across every use. So a distinct
  byte local per loop body vs one shared local is byte-identical. Proven
  0x100250D0.
- **Narrow a callee prototype when orig pushes without zero-extension.**
  `FUN_100271f0(unsigned int)` vs `(unsigned short)`: orig passes
  `mov dx, word ptr [ecx+eax*2]; push edx` with no widening, so the wider
  prototype costs 4 bytes of zero-extension at every call site (32 B over
  8 calls). Return width is byte-neutral. Proven 0x100250D0.
- **Ghidra canonicalises `if (ctr >= bound) break;` to
  `if (bound <= ctr) break;`.** Byte-neutral at /O2 but it flips the
  memory-operand side (`cmp reg,[esp+S]` vs `cmp [esp+S],reg`), so it moves
  the register-blind gap — worth restoring when reading residue. By
  contrast `0 < X` vs `X > 0` and integer `imul` operand order are both
  fully byte-identical (54 and 6 sites measured). Proven 0x100250D0.
- **Ghidra's `if ((mask & flag) == 0) { A } else { B }` inverts the source's
  arm order whenever B is the inline fall-through block in the bytes.**
  Read the jcc sense at the test to decide which arm the source wrote
  first. Not universal — of six mask sites in 0x100250D0 three wanted the
  flip and three were already right, so MEASURE each one. Proven 0x100250D0.
- **x87 argmin: Ghidra writes `if (b <= a)` (then-arm out of line) for orig
  `test ah,1; je then` whose fall-through is `a < b`.** Source wrote
  `if (t0 < t1) { x-vs-z } else { y-vs-z }` with a shared `goto` z-wins;
  Ghidra inverted to `if (t1 <= t0)` and duplicated z. Restoring the `<`
  fall-through plus the shared z recovered orig's 3 `je`s, the leftover
  `fcom [Zero]; fld st(0); fchs` cluster, and the deferred-fnstsw sign
  (`sgn = -1; if (!(c[0] < Zero)) sgn = 1` after the dead-axis stores).
  Refer to `local_c[0]` not a named copy so raw cx stays under |cx| on
  the x87 stack. `pA->y` at a goto-join hoists `lea r, [pA+4]` and steals
  esi; `float *param_2; param_2[1]` keeps `[eax+4]`. Sequenced
  `t = p[-3]; *pc = (t + p[3] + *p) * K` preserves orig addend order
  (plain `p[-3]+p[3]` canonicalizes). Proven 0x10067470.
- **In-place x87 `fchs` is a ternary (or if/else that assigns both arms),
  not a reassignment.** `t = x; if (t < Z) t = -t` emits
  `fstp st(0); fld; fchs` — the dest already exists so the negate is a
  NEW value. `t = (x < Z) ? -x : x` makes the pre-branch load the dest,
  so the taken path is just `je; fchs`. When the RAW x must stay live
  under the abs (later leftover `fcomp [Z]` for a sign pick), the first
  of a pair is `fcom [Z]; fld st(0); je; fchs` and the second is
  `fcomp [Z]; fld [mem]; je; fchs`. This is the 0x1000EAF0-class lever
  too: a named temp that is reassigned is homed; a dest assigned once
  stays in st(0). Proven 0x10067470 (REGNORM extra 16→4).
  Corollary (dual compare, same class): `if (a < K && b < K)`
  short-circuits, so VC5 `fstp st`s the second pair and recomputes it
  in the taken arm. `if (d1 = a, d2 = b, d1 < K && d2 < K)` evaluates
  both assignments before either compare and emits orig's `fst` homes
  plus interleaved fmul + dual fcomp. Statement-then-if
  (`d1=a; d2=b; if (d1<K && d2<K)`) extra-stores the temps. Proven
  0x1000EAF0 trail distance (REGNORM extra 66→60, miss 64→56).
- **x87 spill-slot HOMES follow computation order, and the scheduler's
  drain order keys off them.** Under /Op, paired temps (w2/h2 in
  0x100215C0) round-trip through [esp+4]/[esp+0xc] in the order computed;
  swapping which is computed first re-homes them and flips the whole
  downstream sub/div drain sequence. Computing h2 BEFORE w2 took the
  function from 547 to 129 masked diff bytes at exact length. When an x87
  block matches in shape but drains slots mirrored, permute the SETUP
  ORDER of the spilled temps, not the consuming statements. Proven
  0x100215C0.
- **`(t = r) * mem` copies a live x87 value: `fld st; fmul [mem]`.**
  A settled local `r * pV->x` loads the component (`fld [mem]; fmul st`)
  and leftover r is `fstp st`-discarded.  Copying r into a FRESH named
  temp as part of the product keeps the scalar on the fld side for every
  term.  First product is Div's `(r = 1.0f/s) * mem`; each later product
  is `(rN = r) * mem`.  In-place `*=` / named-r temps / `s = 1.0f/s`
  all canonicalize to the load-components form.  Proven 0x100343F0
  (BrVec3DivBy, 45 B MATCH /O2).  Same shape as orig ScaleBy's 3x
  `fld [s]` — the copies are `fld st` when r is a just-computed st
  value rather than a memory parameter.
- **After a call, `p * k` (k named) homes k (`fst` + `fld [slot]`);
  later products still need `(t = k) * mem`.**  All-copy-assign skips
  the home; `p *= k` loads every component.  Proven 0x1006D4B0
  (BrVec3Normalise, 113 B) and 0x1006D410 (BrVec4Normalise, 148 B).
- **That scale-out is TU-sensitive.**  The same source in br_vec.c
  (with Cross/Dot/Scale) compiles `pV->x * k` as three `fld st` copies.
  Own TU — the adjacent 0x1006D410/0x1006D4B0 pair — matches.  Do not
  merge Normalise into the 0x100343xx vector cluster.
- **Mat4 transform-point is two counted column-walks, not unrolled
  products.** Orig 0x1006DA20 (100 B) is `mov ebp,3` / `mov esi,3`,
  `sub edi,eax` (pM-pOut), inner `fld [v]; fmul [m]; add m,0x10; add v,4;
  dec esi; fadd [eax]; fstp [eax]`, then translation from row 3 with pops
  in the x87 delay slots. Unrolled products were +50 B. Named `vv=*v`
  makes v the fld operand. Exact size, REGNORM 0+0; remaining 7 B is the
  inner ecx/edx pointer swap (orig edx=v ecx=m). Solo TU same. Do not
  grind. Proven 0x1006DA20.
- **A 4-term sum must be left-to-right with no extra parens**
  (`y*y + z*z + w*w + x*x`) so the last load hoists into the previous
  add (`fld x; fxch; faddp st(3); fmul [x]`).  `(y*y + z*z) + w*w + x*x`
  adds w² before loading x (+2 B).  Proven 0x1006D410.
- **12-float max-abs is a pointer walk with a stack zero, not an index
  loop.** Orig 0x1002A957 is `push ebp; sub esp,0x18`, six slots
  (hi/lo/zero/cursor/end/v), `jae` vs `pv+0x30`, integer-copied v,
  `lo=-lo` as `fchs; fstp; fld`, ternary flds.  Closest is that source
  under /Od (58 diffs, +6 B): /Od emits p++ before fld, `fst` not fstp,
  and a return temp.  /O2 /Oy- keeps p/end in registers (112 B).  No C
  spelling is both /Od-shaped and fnstsw-latency-scheduled.

- **A divider-pipeline interleave means PRE-DIVIDED TEMPS in the source.**
  Orig starting two fdivs ahead of a second constant-divide pair
  (fdiv/fdiv ... fdiv/fdiv interleaved with /Op round-trips) is not
  reachable by statement reordering of combined chains — the source
  divided the inputs into named temps first (`f = (float)(unsigned)v / K;`),
  split around the other setup, and the chains consume the temps. Made
  0x100215C0's +0x55..+0xd3 block instruction-identical. Proven 0x100215C0.
- **C++ argument-list scheduling: VC5 pushes args in place, right-to-left,
  ONLY when no argument carries a side effect.** A later constant arg is
  pushed BEFORE a call inside an earlier arg (`push 8; push field; call
  quantiser; ...; push result; call`). ANY side effect in an argument --
  an assignment, or an `__inline` helper (whose expansion introduces the
  inliner's temp) -- makes VC5 pre-evaluate that argument before beginning
  the pushes, moving the constant push after the call. Proven by controlled
  probes on 0x10006510 (build/match/sched.cpp, 2026-08-29).
- **Narrow (16-bit) shift of a call result requires an assignment to a short
  lvalue.** `short s; s = Q(x) >> 8;` emits `sar ax,8`; the same expression
  inline in an argument emits `movsx; sar r32` -- and a `short`-typed
  PARAMETER gets `sar ax` but skips the `movsx` (pushes eax raw). The
  combination sar-narrow + movsx + push-early exists in the original
  (0x10006510, ~30 sites) but no C/C++ spelling found yet produces all
  three. Same probes.
- **A `__thiscall` callee with stack args means the CALLER'S TU was C++ --
  match it as C++, do not fake it from C.** The fastcall dummy-edx idiom
  costs one `xor edx,edx` per call site (the entire +66 B gap on
  0x10006510); a declared-not-defined class method costs zero. The
  src/core/cpp/ convention already supports this. Broke the documented
  "cxx-thiscall-wall" 2026-08-29.
- **`/O2 /Op` is a REAL build variant — a float-heavy TU compiled with precise
  FP can never match under plain /O2.** Without `/Op`, MSVC 5.0 keeps an
  int->float conversion in the x87 register; with it, every conversion is
  followed by the round-to-float idiom `fstp dword [tmp]; fld dword [tmp]`,
  and `/ 2.0f` stops being strength-reduced to a multiply. Added to
  match_sweep VARIANTS as `('O2p', '/O2 /Op')` 2026-08-28. Found on
  0x100215C0: large multiset gap under /O2, ONE surplus `fxch` under /O2 /Op.
- **A live float that orig `fsubr [esp]`s needs its own slot, not x87.**
  Named `float h = (float)g;` under `/Op` keeps h in st (`fsubp st(1)`).
  Address-taken (`float *p = &h; *p = (float)g; ... = *p - x`) emits
  `push ecx` and orig's `fsubr [esp]`. Combined with movsx-from-int16*
  payload and `/Op` fstp/fld on each i16->float, 0x10023920 (BrGbiCall10024260)
  is 156 B MATCH /O2 /Op. Direct globals, not BrScreenGet/BrRdpGetRegs.
- **A MACRO and an `__inline` function are NOT interchangeable — the macro
  changes evaluation order.** A function evaluates all its arguments before
  the body, so VC5 hoists an argument's global load ahead of a preceding
  guard test and can cross-jump two call sites into one. A macro evaluates
  each argument at its point of use. Proven 0x10009C10 (BrCarDrawWheels): the
  two-word append stores word 0 before loading the global for word 1, which
  only the macro form reproduces. Safe to convert only when every argument at
  every site is a pure load or a constant.
- **`v != 0.0f` is ONE compare** (`fcomp; fnstsw; test ah,0x40; jne`);
  `(v < 0.0f) || (v > 0.0f)` is two. Proven 0x10006510.
- **Relocation slots must be masked when scoring.** They are zero in an
  unlinked .obj and patched at link time, so they ALWAYS differ from the
  original. `match_sweep.score` masks them; a per-function harness that does
  not will report a finished match as hundreds of differing bytes (this hid a
  completed 0x10009C10 on 2026-08-28).
- **MSVC 5.0 PACKS ORDINARY LOCALS INTO DEAD PARAMETER SLOTS.** Orig writes
  both bytes and dwords into param_9's and param_1's incoming arg slots
  ([esp+0x9c], [esp+0x7c]) at 0x100250D0. Therefore **Ghidra's `param_N`
  scratch names are slot coincidences, not evidence about the source, and
  renaming a local to chase orig's slot number is NOT a lever** — every
  such rename measured worse (+16 B IDX4 arm, +32 B CI8 arm, +16 B I8 arm).
  This retires a whole speculative class. Proven 0x100250D0.

- **0x10002580 store-burst is a coloring wall, not a frame layout.**
  Residue stamp `frame@0xe/68` is a classifier artifact: bytes 0..0xd
  match (`mov eax,[g]; push ebx; push esi; xor esi,esi; cmp eax,esi;
  push edi; 0f 84`); offset 0xe is the near-je displacement, off by 1
  because the success-path body is a different length. Orig has no
  `sub esp`. After the stdcall-fptr fix (4/1/2/2), the first real
  divergence is 0x5d: orig `xor edx,edx; mov eax,4; xor ecx,ecx` then
  a 7-store prefix (edx/eax/ecx) and `push 0x10000020` after the handle
  reload; wrap `mov eax,4; push 0x10000020` immediately and uses esi
  for every zero. Named temps (`z`/`four`/`z2`), a mid-burst handle
  local, comma-join, and restoring `iVar1` all compile byte-identical
  to the wrap (168 diffs). Retyping `_DAT_1021c784` / `DAT_1021c788`
  to float drops 168→68 but is wrong (`orig_widths` are int; orig
  `mov [x], esi`). Do not permute further. `stackshred` is a no-op
  on this VA (no locals).

- **Ghidra-shredded stack struct → one struct of the frame size
  (`stackshred`).** Ghidra prints `char local_10[4]; int local_c; int
  local_8;` for a 16-byte stack object whose 4th dword is unread, and
  /O2 emits `sub esp, 0xc`. Orig `sub esp, 0x10`. Fold into one
  struct, pad holes (and up to orig's `sub esp` when given), keep
  Ghidra's field names as `_fr.local_N`. Same family as BrTex3dCreate
  / 0x1002DEC3 and BrTex3dExpand `sub esp, 0x68`. Tight: 2..8
  `local_N`, frame ≤ 0x40, at least one address-taken, no C++ EH.
  Only-if-better (0x10027710 CLOSE(2) would become 159). Proven MATCH
  0x100027E0 (MCI_STATUS, 80 B /O2) and 0x10002870 (MCI_PLAY 12-byte
  parms, 109 B /O2, was short-29). Frame residue 52→51; 0x10002580
  is not prey.

- **Shared-tail goto, not early `return 0` (`misscode`).** DX5
  WaveOpenFile success is `goto TEMPCLEANUP` joining cleanup's
  `*phmmio = hmmio; return nError`. Ghidra prints `*out = h; return 0`
  (or `return err` inside `if (err == 0)`), so /O2 peepholes
  `xor eax,eax` and the cleanup store goes through ecx — 7 diffs on
  top of the esi/edi colouring wall (35 diffs) while the outer
  `if (h==0) err; else BODY` is still there. Orig duplicates
  `mov [eax], edi; mov eax, esi; pops; add esp, 0x24; ret`
  (`8bc65f5e5d5b83c424c3`) at both rets. Rewrite the success wrap to
  `if (err != 0) goto CLEAN; goto TEMPCLEANUP;` and label the last
  `*out = h; return err`. Distinguisher: two `ret`s share an 8-byte
  tail, or `sub esp, 0x24` + `cmp ,0x10; jb`. Also: Ghidra shredded
  PCMWAVEFORMAT into 4 ints so `mmioRead(..., 0x10)` is only known to
  write one dword (short-25, frame 0x18 vs orig 0x24); `0xf < cksize`
  is `cmp ,0xf; jbe`, orig `cmp ,0x10; jb`; after `mmioClose(h,0)` orig
  `xor r,r` is `h = 0` before `*out = h`. Folded orig-gated
  only-if-better (`misscode` in `refine_function`, one candidate in
  `_refine_candidates`). Proven MATCH 0x1006FFC0 (425 B /O2).

- **Post-increment, not `i + 1`.** Orig `mov al,[eax+ecx]; inc ecx; mov [pos],ecx`
  is `return p[i++]`. Writing `i = pos; pos = i + 1; return p[i]` emits
  `lea ecx,[ecx+1]` (+2 bytes). Proven 0x1006CE00 (BrBitStreamReadU8, 23 B
  MATCH /O2).

- **`operator delete` is a local E8, CRT `free` is FF 15.** Adjacent fields
  can still split: fclose the FILE via IAT, `BrOperatorDelete` the buffer
  (0x1007DE40). Header already said this; spelling `free()` was the whole
  1-shape miss. Proven 0x10008B50 (BrKeyCacheReset, 80 B MATCH /O2).

- **Ghidra drops a `base + k*8 + disp32` scale.** Orig
  `mov eax,[ptr]; mov ecx,[idx]; mov eax,[eax+ecx*8+0x1de48]` is an 8-byte
  record array at offset 0x1DE48 of the pointed-to object, not
  `ptr[idx]` (scale 4, no disp — 4 bytes short). Matching twin:
  `*(void **)((char *)ptr + 0x1DE48 + idx * 8)`. Proven 0x100366C0
  (BrSub1003D030, 55 B MATCH /O2).

- **Global table walk takes no args.** Orig `mov edx,[count]; mov ecx,offset
  recs; test edx,edx; jle ret` then `cmp dword [ecx],0; ... add ecx,stride;
  dec edx; jne`. Parameters are a port convenience (`mov r,[esp+N]`). Also:
  `if (n > 0) { do ... while (--n); } return c;` shares one `ret` via `jle`;
  `if (n <= 0) return 0;` duplicates the epilogue (`jg` + extra `ret`).
  Proven 0x100057E0 (BrEntityCountActive, 33 B MATCH /O2).

- **Port-only NULL guards are extra early-outs.** Orig starts
  `mov esi,[esp+8]; cmp word [esi+0x12],18` with no `test esi,esi`.
  `if (!p) return NULL;` emits an extra test/jne/xor/pops/ret. Keep the
  guard under `#ifndef BR_MATCHING_BUILD`. Proven 0x10001240
  (BrSurfFromBitmap, 75 B MATCH /O2).

- **GBI tex-scan OtherMode H/0E take pCmd only; fields are globals.**
  Orig `mov eax,[esp+4]; mov eax,[eax+4]` then `mov [0x10697a44],imm`
  (and H's `push ecx; call 0E; add esp,4` — one arg). The port's
  `pSt->f5553DC` is `[R+disp]` plus a wasted first-arg load. Matching
  twin: 1-arg `const BrGfxWords *pCmd`, store `DAT_10697a44` /
  `DAT_106b7ab0`. Hide the 2-arg header proto in the .c (no header
  edit). Proven 0x100297C0 (47 B) and 0x10029780 (55 B) MATCH /O2.

- **Uninitialised `push ecx` slot live across a call is `volatile int`.**
  Orig saves a global to `[esp]` only on the `-1` arm, restores after the
  call; `int saved = g;` plus `if (g==0) return;` drops the slot (callee-
  saved or nothing) and duplicates `ret`. Matching: `volatile int saved;`
  (no init), `if (g != 0) { ... }` so one `je` to the shared pop/ret.
  Proven 0x10019840 (BrS17DrawGated, 71 B MATCH /O2).

- **Segment-base setter is two globals plus a 0-arg helper, not a
  pointer write.** Orig 0x10018A10 is `call BrRcaResetCounts; mov
  [g_brSegN64Base],arg1; mov [g_brSegHostBase],arg2` (25 B). The port's
  `pMap->n64Base = n64Base` is `[R],R` through the first arg. Keep the
  3-arg header proto: matching treats arg1 as n64Base and arg2 as
  hostBase, third unused. Proven 0x10018A10 (BrSegSetBases, MATCH /O2).

- **LoadImageA is nested GetModuleHandleA; failed hbm is the live zero.**
  Orig `push flags; ...; push 0; call GetModuleHandleA; push eax; call
  LoadImageA`. On fail, `push 0x2010; push cy; push cx; push eax; push
  name; push eax; call LoadImageA` — eax is still 0, used for both
  IMAGE_BITMAP and hInst. `LoadImageA(NULL, ..., 0, ...)` emits extra
  `push 0`. Proven 0x10001290 (136 B) and 0x1005A210 (101 B, file-only
  LoadImageA(NULL, path, 0, 0, 0, 0x2010)) MATCH /O2.

- **Fill-rect handlers are 1-arg; 10.2/s12 extract is shl/sar.** Orig
  `mov esi,[esp+0xC]` (after push ebx/esi) then `mov edx,[DAT_100a7518]`.
  `(w >> 2) & 0x3FF` is `shr` — orig is `((int)(w << 20) >> 22) & 0x3FF`
  and `((int)(w << 8) >> 22) & 0x3FF` (0xF6) or `>> 20` with no mask
  (0xE1). Then cdecl `FUN_1001e380(ulx, H-lry-1, lrx+1, H-uly)`. A
  2-arg wrapper into a shared helper does not match. Proven 0x1001E320
  (96 B) and 0x1001E720 (73 B) MATCH /O2.

## Cost model (measured, 2026-08-22 timed test)

Size is not the cost driver — code shape is. 738 B of int/call-heavy code
matched in ~10 min; a 165 B float function (BrVec3Project) is permanently
stuck at 30 diffs of scheduling residue. Int/call-heavy code sustains
5–10 min/function at any size once the file's context is loaded. The float/
x87 cluster carries the walls and is a separate workstream.
