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

- **POD writer Add (0x10008BE0, 158 B, MATCH /O2):** thiscall on a
  stream at `this+4`; file/count/dir are globals not struct fields.
  `&dir[g_count]; g_count++` is `lea 19n; inc eax; mov [count],eax;
  lea esi, [edx*4+dir]`. MakeName is thiscall / ret 8 with TWO struct
  args so edx stays the 19n scale. `off = ftell(g); pEnt->offData = off`
  (named local, not a compound assign) lets b08/b09 loads hoist into
  the `add esp,4` delay slot. Write is `__fastcall(stream, live_pvData,
  file, data, cb)`. `unsigned char` b08/b09; always Write, no `cb==0`
  guard; `_strupr` IAT; fatal-printf string has no `\n`.
- **POD writer Open/Close residue (REGNORM 0+0):** Open's `g_file = f;
  fseek(f, 0x10, 0)` emits `mov [g],eax` *before* the three fseek
  pushes; orig stores after them (6 diffs). Seven spellings (assign-as
  arg, comma origin/offset, fseek-then-store, no-local) all eager-store
  under /O1 /O2 /Os. Close's second Write has FILE* in ecx vs edx and
  `mov ecx,this` before vs after the pushes (9 diffs). Same schedule
  class as Open.

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
  **Generatored 2026-09-01:** the whole phase-leave family (33 corpus
  members starting `8b442404 8b88e82a0000 8b11 ff521c`) is stamped by
  `tools/gen_phaseleave.py` from this skeleton — 28 members matched 4/4
  first try, +1 hand-filed variant (0x1003E450, its own head slot).
  Only 0x1003FB10 (thiscall helper tail) and 0x100403B0 (second vcall
  on another object) leave the grammar.
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

- **/Od fuses `a = X; b = a * Y;` through the x87 stack (`fst [a]` keeps
  the value); BRACING the statements (`{ a = X; } { b = a * Y; }`) forces
  the original's full store + reload (`fstp [a]; fld [a]`).** volatile does
  NOT block this peephole; a `*(float *)&a` read also blocks it. Proven
  0x1002D362 BrCamFrustumBuild (466 B, MATCH /Od).
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
  SIXTH-PASS (2026-09-01): trail-quad block CLOSED on the x87 side and the
  8-byte frame delta with it (prologue now exact).  Four proven levers:
  * **A struct-typed pointer var with many uses makes the FIRST float def
    through it an integer bit-copy home** (`mov edx,[eax+ecx+0x50]; mov
    [slot],edx`, every later use reloads the slot: `fld [slot]; fmul
    [slot]; fld [slot]`), and the SECOND def a plain `fld` kept on x87
    (`fld st(1)` dup).  Swapping the def order swaps the roles.  The same
    loads written as arithmetic derefs (`*(float *)(wb + pCar + 0x50)`),
    through a 2-use pointer (substituted), or through a float* with index
    derefs all give `fld; fst [slot]` instead -- and the extra x87 value
    was the whole frame delta.
  * **Repeated expressions, not named temps, for values used twice as
    both stack and memory operands.**  `x1 = p->x - (dx/len)*K; x2 =
    (dx/len)*K + p->x` CSEs `(dx/len)*K` into a temp that is `fst`-homed
    and KEPT on the stack (x2 = `fadd [eax]` with the temp on top, x1 =
    `fsub [temp]` with px on top; px/py loaded ONCE).  A named `ox` is
    `fstp`'d and every consumer reloads it, forcing px/py to be loaded
    twice; statement order does not change that.
  * **`dx*dx + dy*dy` product order is source order** (first product is
    `fmul [slot]` on the reloaded dx, second `fmul st(3)` on the x87 dy).
  * **A global cursor read only inside a conditional arm is bound inside
    it** (`pT = DAT_1035f7d8` inside `if (param_2)`) -- outside, the load
    is hoisted above the test and the test becomes `mov eax,[ebp+0xc];
    cmp eax,ebx` instead of the original's `cmp [ebp+0xc],ebx`.
  Open in the same block: the wheel-pointer ADDRESS form.  Orig computes
  `iw*0x40 + param_4 + negCar0` into eax (param_4 first) and keeps pCar as
  the index register, `lea edi,[eax+ecx]` only after the dx reload.  A
  single-use `wb` is forward-substituted and all four terms merge into one
  register; any second source-level use of `wb` restores the 3+1 split
  (the two-part loads through the pointer var fold to [eax+ecx+disp] even
  before the lea) -- but no second use that is invisible in the original
  bytes has been found (arg, py, px, vy, z via the sum all show up or
  spill across the call).  Dead: add-order permutations, negCar0 renames /
  types / scopes / LICM, param_4 renames, two-step wb or pW defs,
  pointer-difference car base (cancelled), index-based `param_4 +
  iCar*0x2b68` (second IV), byte-offset `ring*4` (still folded).
- **‼ `divergence.py`'s region count is BLIND to immediate-operand defects.
  Screen every stalled function with `tools/msetdiff.py` before grinding it
  further.** divergence.py normalises to "mnemonic + operand SHAPE with
  imm32 wildcarded" (its own `norm`), which is right for aligning streams
  but means a wrong CONSTANT scores as a match forever. Proven 2026-09-03 on
  0x1000EAF0: the ring-wrap test is `if (head >= 500)`, not `if (499 < head)`
  — orig emits `cmp X,0x1f4; jl`, we emitted `cmp X,0x1f3; jle`, at four
  sites, and the region count had called all four MATCHING through eight
  passes of grinding. The two spellings are semantically identical, so
  nothing behavioural could have caught it either; only the bytes say which
  constant the original source wrote. Fixing it took the reloc-masked byte
  diff 3,855 -> 3,727 and the instruction count to exactly 2,328 = 2,328.
  `tools/msetdiff.py` keeps small immediates while normalising registers,
  esp displacements and relocs, then diffs the multisets — what survives is
  a real constant defect or genuinely absent code.
  **Corollary on metrics:** that fix RAISED the masked region count 19 -> 20
  (it re-opened an unrelated sink), so region count alone would have
  rejected a correct transcription. On any change touching a constant, rank
  by reloc-masked differing bytes and by instruction count, not by regions.
  The signature to watch for: a region count that will not move while the
  instruction count stays off.
- **A spelling must be measured on EVERY site that shares the allocator
  decision it targets — and on no more than those.** Proven 2026-09-03 by
  moving 0x1000EAF0 20 -> 19 masked regions on a lever its own dossier had
  recorded as REJECTED. The rejection was real but partial: byte-offset
  induction variables (`c2 << 4`, `+= 4`, ring reads and writes spelled
  `*(int *)((char *)base + off)`) had been measured on ONE of that
  function's two drain loops. One loop alone genuinely IS worse (+7 insns);
  both together gain a region and, as a side effect, close an unrelated
  `mov edi,1` sink 22 KB earlier. VC5 commits to one induction-variable
  strategy per region, so a half-converted pair leaves it straddling both
  and scores worse than either consistent form. **Every "structurally right
  but flips the global allocation" verdict in these dossiers is suspect
  until it has been re-measured across the whole sibling set.**
  The counter-case, measured the same day so this does not over-generalise:
  on 0x100250D0, converting all ten remaining `x = w*2; if (c) x = w;`
  doubling sites to the ternary form AT ONCE is clearly worse — it breaks a
  1.7 KB byte-exact prefix and costs +25 insns. The difference is what the
  sites share. Loops sharing one induction variable: convert together.
  Independently allocated arms: one at a time. Ask which single allocator
  decision the sites share before batching them; "they look alike" is not
  the test (same trap as the symptom-vs-cause residue classes).
- **A falling RAW region count can be an alignment artefact — never read it
  alone.** That bad 0x100250D0 probe dropped raw regions 45 -> 25 while
  breaking the byte-exact prefix: once the streams misalign, the resync
  merges many small regions into few large ones. Always read the
  slot-masked count AND the first-divergence address together, and treat a
  first divergence that moves EARLIER as a regression whatever else
  improved.
- **VC5 CANONICALISES COMMUTATIVE OPERAND ORDER AND STATEMENT SPLITS. Stop
  probing them.** Proven 2026-09-03 by twelve byte-identical probes across
  all three giants (0x1000EAF0, 0x100250D0, 0x1000A110). None of these
  changes a single byte:
  * swapping the operands of `&`, `|` or `+` (`(a & b)` vs `(b & a)`,
    `x << 8 | y` vs `y | x << 8`);
  * reversing a relational test's operands (`a >= b` vs `b <= a`) — this is
    NOT the same lever as the wall-1 `iVar10 < param_10` rule, which changes
    which side is the loop INDUCTION variable, not just the operand order;
  * splitting one expression into two statements, or hoisting a subterm into
    a named temp (function-scoped or block-scoped) when the temp is
    single-def/single-use in a pure-expression context — VC5 re-fuses it;
  * reordering two adjacent independent assignments (`a = p; b = q;`).
  What these spellings CANNOT reach, and what people keep mistaking them for:
  which subexpression is EVALUATED first (VC5 orders by subtree cost, so the
  simpler operand's calls and loads are emitted first), which operand lands
  in the `r/m` vs the `reg` field of a `test`/`cmp` (that follows register
  assignment), and which byte lane a value lands in (`mov dh,al` vs
  `mov dl,al`). Those are downstream of allocation. To move them you must
  change the operand-KIND ladder or the register pressure (see the
  0x1000EAF0 fourth-pass entry), not the spelling. A named temp DOES still
  work where it changes the def/use graph — a value used twice, a float
  homed to a slot, a bound reloaded from memory; those are different levers.
- **`/FAcs` is the offset-to-source-line map AND the recomp's complete frame
  map — use it before probing anything in a multi-KB function.** One compile:

      sh tools/wine.sh tools/msvc5/bin/cl.exe /nologo /O2 /W3 /I include \
        /I tools/msvc5-compat /I tools/msvc5/include /DBR_MATCHING_BUILD /c \
        /FAcs "/Fa<out>.cod" "/Fo<throwaway>.obj" <file.c>

  The listing's offsets are the same function-relative offsets
  `divergence.py` prints, so `grep '^  00<off>' <out>.cod` turns a divergence
  address into the exact source line — no more guessing which of nine
  near-identical arms a region belongs to. Just above `_<Name> PROC NEAR` the
  listing prints every local's frame offset as `_name$ = <signed offset>`,
  including compiler-generated scope suffixes (`_eyeX$2612 = -60`). That
  table is the recomp side of the slot-map work three dossiers are grinding
  on; read it instead of inferring slots from displacement histograms, which
  cannot be compared across two builds whose frame sizes differ. It also
  catches stale claims: br_drawcar.c's header asserted pack0/pack1 landed in
  "two fresh dwords (0x10/0x14)" when the equate table shows
  `_pack0$ = 8, _pack1$ = 12` — they were already in the reused ARG slots.
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
- **INTEGER adds of two fields of the SAME struct canonicalize their load
  order absolutely — there is no C spelling that flips them.** Unlike the
  x87 case above, sequencing through named temps does NOT preserve source
  order for a GP-register add. At 0x10017F80 (BrFadeDrawSprite) the original
  emits `mov esi,[edx+eax*8+0xc]; mov edi,[edx+eax*8+4]; ... add esi,edi`
  (accumulator = the +0xC field); every source form emits the pair the other
  way round, for a residue of exactly 2 bytes with RAW and REGNORM both 0+0.
  Probed and DEAD, do not re-run: (1) swapping which field goes into which
  named temp, (2) dropping the second temp (`s = pr->y1; s += pr->y0`),
  (3) the whole word as one expression with no temps at all, (4) reordering
  the `|` chain so the y-term leads. Note the ASYMMETRY that proves this is
  allocation and not source: the *other* addend pair in the same statement
  (`x1 + x0`) matches the original exactly, and it differs only in that its
  second operand is reached base-only (`mov edi,[eax]` after a `lea`) rather
  than through the scaled `[edx+eax*8+disp]` form. When both operands share
  the scaled form, VC5 picks the accumulator itself. T3a — park it.
- **In a three-term x87 row, WHICH term is subtracted decides the `fxch`
  count.** `a + b - c` and `a - c + b` are the same value but not the same
  code: they build the operand stack in different orders, and the wrong one
  costs a surplus `fxch`, i.e. one extra instruction and two bytes. Swapping
  the two ADDENDS changes nothing (VC5 canonicalises commutative adds — see
  the entries above); moving the SUBTRAHEND is the lever. Rows of the same
  formula need not agree: in 0x1006D530 BrRbQuatDerivative only the third of
  four rows takes the subtract-second form, and forcing it on the others is
  neutral or worse. Sweep the associations per row rather than assuming the
  formula is written uniformly — the 64-build sweep there took the function
  from 208 B / 74 insns to exact 206 / 73 and the residue from 26 to 21.
- **`fild qword` with a zeroed high dword is `(float)(unsigned)`; `fild dword`
  is signed.** A one-instruction read of the source's signedness, and it is
  also why the top byte of a packed colour is extracted with a bare `>> 24`
  and no `& 0xFF` — the mask is redundant only when the value is unsigned, so
  its absence in the original tells you the same thing twice. Proven
  0x1001E930.
- **A `fstp dword [slot]; fld dword [slot]` round-trip straight after a
  conversion is /Op ROUNDING, not a named source temp.** Under `/O2 /Op` MSVC5
  rounds to float on every assignment, which produces the store/reload for
  free; adding a `float t =` to try to reproduce it makes things worse (0
  diffs without the temp against 119 with it, same function, same flags).
  Diagnostic pair worth internalising: a MISSING `fstp [esp+S]` / `fld [esp+S]`
  pair with no EXTRA counterpart usually means you are compiling the wrong
  VARIANT, not writing the wrong source.
- **‼ A report.csv row's `opt` column can simply be WRONG, and it will cost
  you the function.** 0x1001E930 was recorded `O2y`; at /O2 /Oy- the best
  source form is 105 diffs and at /O2 it is 85, but at **/O2 /Op it is
  byte-exact**. The sweep picks per function from what it has tried, so a row
  whose structure you believe is right but which will not close is a reason to
  compile it under all four variants by hand before concluding anything about
  the source. This is the fifth-variant problem in a different dress.
- **‼ PROCESS: judge a change at the TU's OWN compile variant, and against
  the right parent.** Two ways a before/after measurement lies, both hit in
  one session. (1) `fn.py` compiles /O2 only; on an /Od, /Oy- or /Op TU its
  numbers are partly phantom, so a gain measured there may not exist. Look up
  the row's `opt` first and re-measure with those flags before claiming
  anything. (2) When another session is committing to the same branch,
  `HEAD~1` is NOT your commit's parent — diff against `<yourcommit>^` or you
  will compare a version against itself and read "no change" for a change
  that worked. On 0x10015630 the honest numbers at /O2 /Op were a
  register-blind gap of 77 -> 44 while the RAW BYTE DIFF WENT THE WRONG WAY
  (819 -> 821) and size went from 38 short to 7 short: rank by the
  register-blind multiset, never by size or diff count.
- **‼ PROCESS: a residue note's parity claim is only worth what it was
  measured at.** The note on 0x1006D530 asserted "instruction stream, count
  and size are exact (RAW and REGNORM multiset gap 0+0)"; rebuilding that
  note's own commit showed 208/206 bytes, 74/73 instructions and REGNORM 1+0
  — there was a surplus instruction the whole time, and the claim of parity is
  precisely what would stop the next reader from looking. Before trusting any
  "exact except for allocation" note, spend the 12 seconds to re-measure it.
  Three "unreachable"/"do not grind" notes and now one false parity claim have
  been overturned in this tree; treat every such note as a lead, not a verdict.
- **`sub reg, imm` vs `add reg, -imm` is NOT an operator choice, and it is a
  compiler tell.** MSVC 5.0 canonicalises a STRAIGHT-LINE constant subtraction
  to `add reg, -K`, always. Measured directly, not inferred: `x - 16`,
  `x -= 16`, `x -= 0x10`, the subtraction inlined into the store, a
  const-propagated `base`, an in-place bump on the parameter, and `-(16 - x)`
  all emit `add`; so do `long`, `unsigned`, `short`, and BOTH pointer forms
  (`p - 16`, `p - (char *)16`); and so do /O2, /O2 /Op, /O2 /Oy-, /O1 and /Ox.
  **VC++ 4.2 emits `sub reg, K` for exactly the same source.** Do NOT use that
  as a compiler fingerprint on its own, though — 11 of the tree's byte-exact
  MSVC5 functions contain `sub r32, imm` (0x10019480, 0x10023B10, 0x1000C9C0,
  0x1001D150, 0x1002F680, 0x1006FF00, 0x1002B997, 0x10031AC0, 0x10053EF0,
  0x10053F80, 0x1006B400), so MSVC5 clearly reaches it by another route. The
  two answer keys that were read: 0x10023B10 is a LOOP-CARRIED pointer
  decrement (`p -= 0x28` as the loop step, `cmp`/`jge` back edge), and
  0x10053EF0 is a SIXTEEN-BIT subtraction whose result stays live as `ax`
  (`movsx ax,dl; sub eax,0x20; test ax,ax`). 0x1006FF00's is a pointer
  difference feeding a divide. So when an original shows `sub reg, imm` where
  your C has `add reg, -imm`, the lever is the VALUE'S TYPE OR LIFETIME — a
  loop step, a narrower type, a difference that feeds further arithmetic —
  never the spelling of the minus sign. OPEN: 0x1006FD50 BrEntitySetIndex is
  2 bytes from exact on precisely this and fits none of the three keys.
- **Repeated constant stores are a LEADING GROUP, and the group runs
  DESCENDING.** A function that writes the same constant into several fields
  of a struct writes them all up front, not interleaved in field order. VC5
  materialises the constant in one register; interleaving the stores keeps
  that register live across the whole body, which costs a callee-saved
  register and grows the function by a `push`/`pop` pair. Grouped, the
  constant dies before the next parameter is loaded and its register is
  REUSED for that parameter — the same "dies early, register reused" shape as
  the conditional-bump entry below. Order within the group matters and is
  DESCENDING by offset: `m[8]; m[4]; m[0]` is byte-exact where `m[0]; m[4];
  m[8]` is 7 bytes off. Proven 0x1006DC30 BrMat3Skew (51 diffs and +2 bytes →
  byte-exact). SCREEN: recomp EXTRA of exactly `push R` + `pop R` with the
  original shorter is this class, not a frame problem.
- **Let VC5 build the induction variables: SUBSCRIPTS, not hand-rolled
  cursors.** In a nested loop that walks two arrays at different strides,
  writing the cursors out by hand (`col = m; v = pV;` … `col += 4; v++;`)
  gets the instruction stream exactly right and then binds the registers
  wrong — VC5 gives ecx to the cursor initialised by a register copy and edx
  to the lea-derived one, and emits the lea as `[base=output, index=delta]`.
  The original has the opposite pairing and `[base=delta, index=output]`.
  Dead levers, do not re-run: swapping the two initialisations (worse),
  swapping their declarations (no change), block-scoping both inside the
  outer loop (better, still short). What works is deleting the cursors and
  writing the plain subscripts (`pv[j] * pM->m[j][i]`): strength reduction
  then creates the two pointers in VC5's own order and the register pairing
  falls out. Proven 0x1006DA20 BrMat4TransformPoint (7 diffs → byte-exact).
  Semantics are preserved because a subscript off the parameter re-reads the
  live array every outer pass, exactly as re-initialising the cursor did —
  which matters when the output aliases the input. SUSPECTED CLASS: any
  matched-size, REGNORM-0 loop whose C uses hand-written walking pointers.
- **The integer twin of the `fchs` rule: a conditional BUMP is an
  alternative ASSIGNMENT, and that is what frees the tested register.**
  `pos = p->b; if (p->a) pos++;` pre-loads `b` and only then tests `a`, so
  both are live at once and the tested value needs a register of its own
  (`mov edx,[ecx]; mov eax,[ecx+4]; test edx,edx`). Writing it as
  `if (p->a) pos = p->b + 1; else pos = p->b;` — or the equivalent ternary,
  which is byte-identical — makes `a` die at the test, so VC5 loads it into
  the accumulator, tests it, and REUSES that register for `b`
  (`mov eax,[ecx]; test eax,eax; mov eax,[ecx+4]`), with the `je` scheduled
  after the second load because the flags survive a `mov`. Diagnostic: the
  original loads the tested field and the used field into the SAME register.
  Proven 0x1006CF80 BrBitStreamAtEnd (7 diffs → byte-exact, REGNORM was
  already 0+0 — a pure RAW 2+2 residue that WAS reachable from source, so
  do not read "REGNORM 0+0" as automatically T3a).
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

- **5-bit RGB expand is `(w >> s) & 0xF8 | (w >> t) & 7`, not the
  xor-blend transcription.** Orig emits `mov r,w; mov r,w; shr; shr;
  xor dl,cl; and dl,7; xor dl,cl` — that IS VC5's lowering of the
  and/or form. Spelling the xor-blend as source (`a ^ ((a ^ b) & 7)`
  with byte temps) compiles to a 2-register in-place shr (no esi, -6 B,
  REGNORM 0+4). The and/or form keeps `w` live for both shifts, forcing
  the two copies + `push esi` and the mid-function `pop esi` after
  channel 2's copies. Reload `w = *(unsigned *)(p+4)` per channel;
  p stays in eax (`add eax,8` return). Blue is a byte load `& 0xFE`,
  `<< 2`, plus dword `>> 3` `& 7`. Alpha is `(w & 1) ? 0xFF : 0`
  (`and cl,1; neg cl; sbb ecx,ecx; add eax,8; and ecx,0xff`) — not
  `0 - (w & 1)` (that is `neg` with no sbb). Proven 0x1001E9F0
  br_dl_fillcolour, 110 B, MATCH /O2.

- **The strength reducer folds a global array base into the walking IV only
  when every in-loop access shares ONE symbol.** A stride-12 search over
  three interleaved pinned columns (0x105B16F0/F4/F8): spelling the two
  compares off their own symbols keeps a zero-based byte IV (`xor eax,eax`;
  `cmp [eax+sym]`, 6-byte encodings). Spelling BOTH off the middle symbol
  (`(char *)&DAT_f4 + i*12 - 4` and `+ i*12`) folds base+i*12 into the IV:
  preheader `mov eax, offset f4` placed AFTER the entry test (a hand-written
  pointer init hoists ABOVE it — keep the loop indexed so the reducer owns
  the IV), compares `[eax-4]`/`[eax]`, and the found-arm's read of the third
  column recomputes from the index (`lea; [eax*4+f8]`). Writes to such
  columns are byte-offset stores with n*12 materialised once (`lea; shl`)
  and each column's own symbol as displacement — the `((T*)&g)[n].field`
  struct spelling emits scaled `[eax*4+base]` instead (+6 B). Proven
  0x10018E10 BrVtxCacheResolve + 0x10018FC0 BrVtxCacheInsert (MATCH /O2).
  **When the stride divides by a SIB scale (0x978 = 303*8), even the
  byte-offset spelling canonicalizes back to `[r*8+disp]` — a NAMED
  `int off = i * 0x978;` variable forces the one-time shl materialisation**
  (5 uses; inline arithmetic stays scaled at any use count). Proven
  0x10006150 BrNetSlotGetF030 (96 B, MATCH /O2).
- **A record loop that reads the rest of the record at negative offsets
  advanced its pointer at the TOP.** `mov ecx,[arg]; movsx [ecx]; add
  ecx,0x10; movsx [ecx-0xe]…` is source `v = *(short *)p; p += 0x10;` then
  `p - 0x0E` etc. — spelling the advance at the bottom with positive
  offsets biases the IV differently. The fild int-temp lives in a DEAD ARG
  SLOT, which requires the pointer init to sit INSIDE the count guard
  (block scope) so the arg slot dies at the right time. Proven 0x10018EF0
  BrVtxExpand (REGNORM 21→0; residue: whole-function ecx/edx rotation).

- **Per-arm duplicated switch tails come from ONE shared tail after the
  switch — VC5 tail-DUPLICATES the join; per-arm copies in source get
  cross-jump MERGED instead.** Orig: three byte-identical `store y; push;
  call emitter; add esp,4; pop esi; ret` tails, one per arm, with one arm
  storing x and falling into the default-target copy. Spelling the copies
  per arm compiles to ONE tail plus `jmp` into it (22 B short) — `return`
  vs `break`, else-if vs switch, named temps, /O1, /Og-, /Oa, /Ow, /Gy,
  VC4.2 all fail to unmerge it. The matching source is `switch { case 2:
  g_x = x - (f() >> 1); break; case 1: g_x = x - f(); break; case 0:
  g_x = x; break; } g_y = y; emit(s);` — the duplication is only reachable
  from the single-tail form. Also: a param used by every arm (`s = psz`
  homed in esi before the switch) hoists with the plain param spelling once
  the tail is shared. Proven 0x100168C0 BrTextDraw (180 B, MATCH /O2).

- **Both-memory fmul operand roles follow extern DECLARATION ORDER: the
  LATER-declared symbol takes the fld side.** Source operand order, decl
  form (scalar/array/defined-in-TU), symbol NAME, and use count are all
  irrelevant (each probed on 0x10016BE0); swapping the two extern LINES
  flips the roles. `extern float K; extern float dt;` puts dt on fld for
  `dt * K`. **TRAP: reloc-masked scoring calls a role-swapped pair
  byte-exact** — the two loads have identical shapes and the swapped
  addresses live in masked reloc slots; only tools/image_build.py catches
  it (this happened live: a "byte-exact" claim carried 3 wrong image
  bytes). After any both-memory float product lands, check the reloc
  ORDER in the .obj against the original's operand addresses. Proven
  0x10016BE0 BrWeatherStepLightning (174 B, image-clean MATCH /O2).

- **Bit-stream readers: compute the value into a block temp BEFORE the
  cursor store** (`{ v = pack; pBs->readByte = i + N; return v; }`) — the
  value-first order is what keeps the pack register live across the store.
  A big-endian s32 read seeds its Horner chain with a SIGNED char load
  (`movsx` does the sign semantics; an unsigned<<24 spelling is a
  different shape). OPEN WALL (register-byte widen): the originals widen
  later bytes in DIRTY regs (`mov dl; and edx,0xff` after register death)
  and sometimes load byte pairs high-first; VC5 zero-widens (`xor` + mov,
  low-first) from every probed spelling — |-order, +, byte temps,
  |=-accumulate, u16 temps, signed-char+mask, sum-read-before-bind. The
  register analogue of the byte-slot wall; 4-15 B residue per reader.
  Proven 0x1006CE20/0x1006CE50/0x1006CE80 (structure), 2026-09-01.

- **div/mod-by-constant pairs: `%` next to `/` emits ONE idiv; the magic-imul
  shape means the source derived the remainder itself — and a compound `-=`
  picks the neg-form.** `q = n / 100; r = n % 100;` compiles to `cdq; idiv`
  (one divide serves both). Orig magic-imul for the divide plus a mul-back for
  the remainder = source spelled the mul-back: `n -= q * 100` (compound, result
  stays in n's register) emits the NEGATED product folded into a lea-ADD
  (`neg; shl 2; sub` = −5q, `lea` ×5, `lea esi,[esi+edx*4]`), while
  `r = n - q * 100` (fresh variable) emits the positive chain + `sub`. The
  second pair (`whole -= minutes * 60`) updates in place as plain `sub ecx`.
  A named quotient local forces `mov ecx,edx; sar ecx,5` (copy first);
  shifting edx in place means no named quotient. Proven 0x10014760
  BrHudDrawTimeEntry (160 B, MATCH /O2).

- **Repeated global reads: do NOT invent a local.** Orig reloading
  `[g_cur]` at every use (even twice in three instructions for the
  f68 store + vcall) means the source read the GLOBAL each time —
  VC5 CSEs adjacent reads into ecx/eax on its own, while a `p =
  g_cur;` local claims a callee-saved register (+1 push, whole-body
  rotation). Same function: an early-`return` arm places its body
  INLINE; the original's arm-at-the-end layout is an if/else. Proven
  0x1001CE20 BrAppStateSetMode (348 B C++ TU, /O2) with both vcall
  scratch arms (EAX/EDX) from plain member calls.
- **Two-case sparse `switch` emits its compare chain ASCENDING with
  bodies in source case order; if/else-if lays the first body inline.**
  Orig `cmp eax,0x113; je far-body; cmp eax,0x501; jne end; <501 body>`
  with the 0x113 body at the END is `switch (msg) { case 0x501: ...;
  case 0x113: ...; }` — the if/else-if spelling emits `jne` and the
  0x113 body FIRST (79 diffs of pure layout). Same function: the
  five-arg Sel vcall reaches the eax-vtbl shape through a `Sel *s =
  &p->sel;` temp (direct `p->sel.s4(...)` loads the vtbl into edx
  before the lea), and a one-arg stdcall IMPORT called twice CSEs
  into edi on its own. Proven 0x10035A30 BrWmAppHook35A30 (134 B)
  and sibling 0x10036130 (103 B), both /O2 C++ TUs.
- **`errno` in a /MD matching TU is the CRT `_errno()` CALL (FF 15),
  never a variable load.** Spell it `*_errno()` with
  `_CRTIMP int *__cdecl _errno(void);` — the plain errno macro (or an
  undeclared `_errno`) compiles to a variable load or an E8 near call
  and shifts every byte after it. The wrap now auto-declares it.
  Also proven on the same pair: a drifted refine draft's whole-function
  register rotation dissolved on clean retranscription (rotation is a
  symptom), and reloc-masked STRING operands must be verified against
  the DLL data section before naming — the "rb"/"wb" twins were
  swapped relative to their VA order. Proven 0x10008DC0
  BrFileCreateChecked / 0x10008E10 BrFileOpenChecked (67 B, /O2).
- **Bounds-checked container methods (`cmp reg,[this+0x10]; jb` +
  warn printf, thiscall `ret N`) are one C++ class — write real
  members, not fastcall tricks.** The Tbl8900 family
  (0x10008930..0x10008A30, five methods, all byte-exact as C++ TUs):
  vtbl read before arg pushes, vtbl CACHED in a callee-saved reg
  across consecutive self-vcalls, and `lea ecx,[this+4]` = thiscall
  on an embedded member object. Second entry-field read via
  `[ptr+4]` after a call means the source CSEd `Ent *e = &items[i]`
  into a pointer temp (recomputing `items[i].f4` re-derives the
  index: +12 B, 32 diffs on 0x10008990). Layout: count +0x10,
  items +0x18 (76 B entries), FILE* +0x1C.
- **Vtbl load scheduled INSIDE a strcpy intrinsic = C++ member call,
  not reachable from C.** `lea edx,[dest]; mov eax,ecx; mov esi,edi;
  mov edi,edx; mov edx,[obj]; shr ecx,2; rep movsd` — the object's
  vtable read sits between the intrinsic's setup and its rep, where
  edx dies. The C fastcall spelling emits the load AFTER the copy
  (18 diffs, pure placement); a C temp read before strcpy hoists to a
  callee-saved reg (+1 push, frame class). A real C++ `obj.i1()` after
  `strcpy(obj_text, s)` reproduces the interleave exactly. Family
  marker: `short-4` (the missing `lea ecx` this-setup). Generatored as
  tools/gen_uitext.py (masked-skeleton corpus scan). Proven MATCH
  0x10038D30 + 7 clones (100 B, /O2, 2026-09-01).
- **Ghidra types byte pointers SIGNED — retype `char *param` to
  `unsigned char *` when the original widens with `xor r,r; mov r8`.**
  A byte read through `char *` compiles to `movsx`; the original's
  `xor eax,eax; mov al,[esi]` shape (default promotion at the call
  site — see the char-window entry above) requires UNSIGNED char.
  One-token fix, whole-function diff dissolves (47 diffs → 0 on a
  61 B function). Ghidra emits `char *` for every byte pointer it
  cannot prove unsigned, so this recurs across the residue (61
  unmatched work files carry retypeable `char *` decls, 2026-09-01
  screen). Automated as refine candidate `uchar:`/`ucharall` in
  `tools/ghidra_to_match.py`. Proven MATCH 0x10020CF0
  BrDlCmdTri2FlatZ / 0x10020D30 BrDlCmdTri2Flat (61 B, /O2).

## Cost model (measured, 2026-08-22 timed test)

Size is not the cost driver — code shape is. 738 B of int/call-heavy code
matched in ~10 min; a 165 B float function (BrVec3Project) is permanently
stuck at 30 diffs of scheduling residue. Int/call-heavy code sustains
5–10 min/function at any size once the file's context is loaded. The float/
x87 cluster carries the walls and is a separate workstream.

## 2026-08-30 wave merge

Consumed `build/match/idioms_new/8bed6233.md` (one scratch file). POD writer
family 0x10008BA0 / 0x10008BE0 / 0x10008C80 is already in Proven idioms
(Add MATCH /O2; Open/Close REGNORM 0+0 store-vs-push schedule wall). **0 new.**

`build/match/walls_log.csv` not present — no per-`<kind>` counts. No
parked-wall kind newly retryable (no new idiom landed).

- **Colour packs are Horner, puts live inside arms, sites recompute
  (BrCarDrawVehicle 0x1000A110, 2026-08-31).** Four levers that took the
  function from 41 divergence regions / REGNORM 75+102 to 39 / 52+43:
  (1) RGBA packs are `(((top<<8|b1)<<8|b2)<<8)|a` — `mov dh,al` is
  `(uint8_t)x<<8` into a zeroed reg, `& 0xFF` components get full-reg
  and/or — never independent `<<24|<<16|<<8` terms. (2) A branch-selected
  DL emit has the WHOLE `put()` inside each arm (three full pack copies at
  the FB envcolour; VC5 cross-jumps only arms whose generated bytes are
  identical). (3) `x ? K1 : K2` inline in the emit argument = xor/neg/sbb
  select, one per put, globals re-read per put. (4) The original
  RECOMPUTES from memory per site (icar from car+0x140 per lights store,
  `(lod*5)<<3` per DL site with one late assignment into the dead ARG
  slot, bKind 5 fresh reads); every source-level cache (dst/i/lodOff/
  bKind/hoisted flags pointer) rotated the whole allocation, and removing
  the caches snapped car=ebx / zero=ebp / load-then-cmp guard in one step
  — the strongest confirmation yet of rotation-is-a-symptom. Residue:
  byte-slot pack locals ([esp+0x31]/0x32) whose stores survive only as
  `*(volatile char *)&pack[n] = v` (plain uint8_t gets store-forwarded)
  while the orig's DWORD+`and 0xff` read-back still narrows to a byte
  load (volatile-int read forces the width but steals ebp — probed, both
  mapped); the Dir0 else-arm fld/fld/mov/fstp/mov/fstp hybrid copy; the
  0x40-vs-0x4c frame. VC4.2 cross-check on this TU is NEGATIVE: all four
  byte-exact-under-5.0 functions shrink/restructure under tools/msvc42.

- **Probes ruled out on the 0x1000A110 residue (2026-08-31, all
  byte-verified dead):** (a) VC5 does NOT fold `(w>>8)&0xFF` into a
  byte-offset dword read — it mask-algebras (`and reg,0xff00` + shifts)
  and the odd-address dword reads at [esp+0x31]/[esp+0x32] therefore ARE
  reads of packed byte-array members, not offset views of a dword local.
  (b) A `volatile const float *` read and a `(float)(double)` round-trip
  both still compile a float field copy to integer movs — the Dir0
  fld/fld/mov/fstp/mov/fstp hybrid remains unreached (also probed:
  pointer-aliased reads, signed-char masks, volatile int puns — the last
  forces dword width but blows a register). (c) The N64 twin
  (TGR ROM fn at 0x80232ed4, found by scanning lui/ori pairs for the DL
  command words — G_MTX 106/103, movemem, the BC-moveword quad) is a
  LEANER ancestor: its colour movewords write constants, so the 3-arm
  colour pack, the *4/5 dim arm, and the Dir0/Dir1 block are PC-era code
  with no MIPS oracle. It DOES re-read icar from car+0x140 per movemem —
  independent confirmation of the recompute-per-site idiom in the shared
  lineage.

- **The byte-slot idiom, CRACKED (BrGlRectFill 0x1001E380, byte-exact,
  2026-08-31).** The `mov byte [esp+S],r ... mov R,dword [esp+S]; and
  R,0xff` pattern is PLAIN SEPARATE uint8_t LOCALS — no array, no
  volatile, no source-level `& 0xFF`: the dword-read+mask IS VC5's
  widening of a byte local whose source register died before the read
  (calls in between, or scheduler kills).  When the stored register
  survives to the read, VC5 store-forwards instead (`mov dl,al`) and
  deletes the store — that is BrCarDrawVehicle's remaining pack residue:
  spelling proven right, register-death timing not yet reproduced.
  Related, proven in the same function: (a) a fresh Glide-side
  transcription beats grinding a mispaired twin (the report row scored
  the D3D body against Glide bytes — 824 diffs of pure phantom); (b)
  /O2 /Op serializes every fild through ONE scratch slot and was
  REQUIRED for this TU (under plain /O2 the filds batch with fxch);
  (c) vertex fan-out is vertex-major source with inline (float) casts —
  CSE homes each converted value in an integer register and float
  fan-out copies become integer movs; (d) an argument clamped through a
  temp (`t1 = x1; if (t1 < min) x1 = min;`) stays memory-homed, its
  register never updated.

- **`shared/prefix` twins can hide a CONVENTION fork, not missing code
  (BrCarStateEncode 0x10006510, 2026-08-31).** REGNORM 33+0 — every Glide
  instruction present, 33 extra `xor edx,edx` — because the D3D build's
  bit-writer consumes an edx dummy (fastcall-shim shape, and the matched
  D3D twin 0x100061A0 PROVES those xors are real on that side) while the
  Glide build calls a TRUE C++ thiscall writer (ecx + two callee-popped
  stack args), which VC5 C cannot spell (see the BrPhaseLeave EAX-pattern
  wall).  Int args cannot be struct-coerced (C2115), an 8-byte pair arg
  pushes from memory not reg/imm — probed.  Classification: the
  `shared/prefix` census pool (22 rows / 13.9 KB) likely contains more of
  this thiscall-fork class; triage those against the C++ workstream, not
  the coloring queue.

- **Fixed-address global array folded as a displacement ≠ pointer variable.**
  Orig `movsx ecx, byte [ecx + edx*2 + 0x100A5C78]` / `mov [ecx*4 + 0x100A5C58]`
  folds the link address as the array BASE — that symbol is DATA at a pinned
  address, not a pointer var to load first (a `const T *g` decl emits an extra
  `mov reg,[addr]`). Reproduce with `((const T *)&g_sym)[i]` under
  BR_MATCHING_BUILD (the pinned var's ADDRESS is the base). Re-derive the
  `*2`/`*4` operand from the SIB byte — the port had the scale on the wrong
  index (a real bug). Proven BrCarDrawVehicle DC-texture lookup.
- **Cross-TU stub getter = a spurious `call` the orig lacks.** The port routed a
  flag through `BrBootGlobal_ABAA0()` (a `return 0;` stub in another TU); at /O2
  it can't inline across TUs so it stays a `call`. Orig reads the real global
  directly (`cmp dword [0x100abaa0], ebp`). Use the actual global (`g_AC300`).
  Pairs with the port-safety null-check removal (orig calls the hook
  unconditionally; no `test/jz` guards `call [ptr]`) and reading a call arg AT
  the call site rather than a hoisted local. Proven BrCarDrawVehicle 0xB176.
- **CORRECTION — hoisting disjoint block vectors to distinct frame locals DOES
  grow the frame.** Two block-scope `BrVec3 tmp` in separate `{}` merged to
  `sub esp,0x40`; hoisting them to TWO NAMED function-scope vectors grew the
  frame to the orig's `sub esp,0x4c` and made the prologue byte-exact. The
  earlier "MSVC overlays disjoint lifetimes regardless" reading was wrong — it
  depends on the hoist form (two named function-scope vars, not one shared).
  Proven BrCarDrawVehicle.
- **Region count decouples from byte distance late in a function.** Removing a
  branch (a port-safety null-check) RAISED the divergence region count 36→38
  while REGNORM fell 9 (register-blind multiset is the truth). Rank residue by
  REGNORM, never region/diff count — the count re-segments on any structural
  edit. Proven BrCarDrawVehicle.

- **Phase-leave family: guarded/tailed vcall variants are ONE stampable
  grammar (2026-09-01).** 0x1003CD20 (guarded slot-6 vcall) and 0x1003FBE0
  (double-strcpy tail) both landed byte-exact FIRST COMPILE from the
  0x1003D4A0-style C++ TU skeleton — GameObj +0x2AE8 pSub, natural
  statement order. Two scheduling facts fall out for free, do not fight
  them: (1) the vcall's `push 0` hoists ABOVE a preceding member store
  (`pSub->f68 = 0; pSub->s6(0);` emits push first); (2) zero stores
  written before a strcpy get scheduled INTO the intrinsic's
  strlen/copy latency slots (`xor esi,esi` from the g_cur null-compare is
  reused as the store source). Spell the source naturally; the scheduler
  does the interleaving.
- **Reloc-masked twin stamping (tools/gen_cpptwin.py, 2026-09-01).** UI/phase
  C++ functions ship the same machine code repeatedly, differing only in
  DIR32 slots (the seven 158-B 0x1003FBE0 siblings differ in ONE byte —
  the phase-source global). After hand-proving one member, run
  `python3 tools/gen_cpptwin.py` — it compares every unmatched orig bin
  against every matched C++ TU with base-reloc + external-rel32 masking and
  stamps renamed TUs for structural twins (+6 byte-exact on first run).
  Both gates hold: the scorer masks relocs, image_build backfills slots
  from the original's bytes. Re-run it after ANY new C++ TU lands.
- **Shared `return 1` after if/else tail-duplicates with the pop FIRST.**
  Orig `pop esi; mov eax,1; ret` = ONE source `return 1;` after the
  if/else, tail-duplicated into each path AFTER that path's register
  restore. Spelling explicit `return 1;` inside each branch emits
  `mov eax,1; pop esi; ret` (mov first) — 4-diff miss. The dual is the
  branch-with-own-return shape (both sites `mov eax,1; ret`, no pop
  between): there the source really has per-branch returns. Read the
  pop/mov order at each return site to pick the spelling. Proven
  0x10040E60 StepCode.
- **Trig product chains: route every cos/sin through ONE reused float temp
  (`t = (float)cos(ang); v.x = ...(t * r + fx)`), never inline
  `cos(ang) * r` (2026-09-01, 0x100140B0 BrHudDrawDial).** Inline trig
  products emit `fld [r]`/`fld st` + `fmulp` pairs (the multiplier gets
  loaded); the reused-temp form keeps the trig value on the stack and the
  radius on the fmul side — `fmul [esp+S]` for a slot-homed radius,
  non-popping `fmul st(N)` for one living deep on the x87 stack. The temp
  also yields the original's lone `fst [slot]` rounding store at the first
  use (reusing the temp's existing home). One probe took the function
  18+13 → 1+5 regnorm. Companion facts proven in the same function:
  a coordinate converted once and re-added each vertex is a STATEMENT
  local placed just before the block (`fx = (float)x;` → fild sunk into
  first use, fstp-homed, later adds read the slot); a second coordinate
  whose fild sits inside the SECOND vertex is an int local with a CSE'd
  INLINE cast (`(float)dy`) — the statement form hoists its fild too
  early. Hoisting the next-angle assignment above the previous store to
  chase the original's pipelining FAILS (live-range extension changes the
  frame); that 2-site/4-fxch pipelining depth is scheduler-internal
  residue.
- **10.2 fixed-point rect packing must be transcribed, never simplified
  (2026-09-01, 0x10013FD0 BrGfxDrawTexRect).** The F3D-style `*4` /
  `& 0x3FFC` / `| 0x38C000` / `>> 2` / `<< 12` coordinate dance is
  value-preserving for integer coords, and the port had folded it to
  plain masks -- 25 bytes of missing code that looked like a "mixed"
  wall. VC5 emits every step, including the command byte riding through
  as `0x38C000` (`>>2 <<12` = 0xE3000000) and the redundant
  `(y<<2)>>2 & 0xFFF` (it reorders the AND first but keeps the dead
  shifts). Also proven here: `((w*4-2)&0xFFF)<<12` folds to
  `shl 14 / sub 0x2000 / and 0xFFF000` on its own -- the literal
  `w*0x4000-0x2000` spelling is not needed and changes nothing.
- **Option-cycler family (2026-09-01, 0x1003C080..0x1003C3D0): factored
  helpers must be `static __inline`, and the down-step is
  load / `--v` / store.** The original inlines both the up/down cycle body
  and the announce tail (3-arg send + intrinsic strcpy) in every cycler;
  the port's called helpers left the whole family scored "missing code at
  ~30%".  `v = *pv - 1` emits lea/test/jge -- the split
  `v = *pv; --v; *pv = v;` gives the original's dec / store / jns (flags
  survive the store).  BrSprintf-style wrappers likewise: the original
  calls sprintf through the /MD import.  TWO residue classes proven
  context-dependent here, do not spelling-probe them again: (1) the
  cycled-global reload goes to eax (A1 short form) in the original but the
  return-1 constant's precoloring pushes ours to ecx -- macro form,
  direct-global tails, named temps all identical; (2) `x -= K` emits
  sub-vs-add-neg by surrounding allocator state, not spelling (BrHudDraw's
  original wants add for -3, BrOptCycleTrack's wants sub for -0x10).
  Track adds a third: 0 and 0x1F get CSEd into ebx/edi here where the
  original rematerialises per site (xor eax,eax / mov eax,0x1f); bare-if
  gates and shifted compare constants do not flip it.

- **Ghidra's named temps are an ALLOCATION HAZARD — three proven forms
  (2026-09-01, the 2-diff scattered class).** A temp Ghidra names gets its
  own callee-saved home; the original CSE'd the expression, so registers
  pair differently and the diff is 2 modrm/SIB bytes. (1) Single-use CALL
  temp: `pv = GlobalHandle(p); GlobalUnlock(pv);` → spell NESTED
  `GlobalUnlock(GlobalHandle(p))` — fixes the hoisted-import-pointer
  edi/esi pairing (proven 0x1006C6A0). (2) SCALE temp: `i4 = i * 4;
  *(int*)(p + i4)` → repeat `p + i * 4` at each use — fixes SIB base/index
  (`8b 04 37` vs `8b 04 3e`, proven 0x1006E130). (3) The dual from the C++
  session: named temps are sometimes REQUIRED (retranscribe rule) — the
  scorer decides. All three are refine transforms now: `calltemp`,
  `scaletemp` in tools/ghidra_to_match.py.
- **`C7 05 <g> 00000000` amid `A3` zero stores = the store sits ABOVE the
  memset group in source.** After a rep-stosd/memset intrinsic eax is a
  known zero and VC5 spells later `g = 0` stores as 5-byte `A3` reuses; a
  10-byte literal-form zero right after the intrinsics means the source
  wrote it BEFORE the memsets (eax not zero there) and the scheduler SANK
  it below them. Transform `zerohoist` (k=1,2). The C7-vs-A3 split in the
  orig bytes picks the statement order. Proven 0x100703D0 (also needed
  Ghidra's bogus `double` retyped int — watch for size-luck baselines where
  a wrong type happens to give a same-length encoding).
- **3D pinned array: the row LOAD and the row POINTER use different index
  decompositions — transcribe both, don't unify.** 0x10059FE0: the first
  read is the flat byte-offset form
  `*(int *)((char *)&BASE2 + (b + a*0x1e) * 0x28)` (lea chain ends in
  `[edx*8+disp]`), the second is a ROW-POINTER
  `((int *)((char *)&BASE + a*1200 + b*40))[c]` (`shl eax,4` + 
  `lea edx,[eax+ecx*8+disp]` + `[edx+eax*4]`). Unifying them into one
  `[a*300+b*10+c]` spelling cross-CSEs the chains and misses. Ghidra's
  mixed spelling was RIGHT except for int-pointer scaling — add the
  `(char *)` cast, not a rewrite. MATCH /O2.
- **2-diff scattered class: two register-pairing walls PARKED (2026-09-01).**
  0x100283C0 (4 diffs): one arg load slides above two stores — struct-array
  and one-symbol respellings score worse; scheduler slide, multiset 0.
  0x1005A480 (7 diffs): counter/pointer esi↔edi rotation — init order, decl
  order, calltemp all no-ops. 0x10054070 (4 diffs, C++ TU in tree): delta
  scratch ecx↔edx rotation. All register-blind gap 0 — do not re-probe.
- **Early-out resource protocol bodies: `do { ... break; ... } while (0)`,
  not structured nesting (2026-09-01, 0x10036810 BrComGetAlloc).** A
  call/alloc/call/cleanup function whose failure paths all funnel into one
  shared cleanup keeps its body CONTIGUOUS only under the do-while(0)+break
  spelling; if-nesting and goto flattenings let the flow optimizer thread
  the jumps and move an arm past the cleanup (2 bytes short, wrong block
  order).  do-while(0) restored the exact 142-B size and the OOM arm's
  `jmp` in one step.  Unreached from source here: which arm goes inline --
  the original keeps the OOM arm inline jumping over the second call;
  VC5 always outlines one of the two (1+1 je/jne residue).
- **/Od TUs (2026-09-01, slice2_19 trio).** Three rules proven on
  0x1002EBD1/0x1002EC36/0x1002EB03: (1) `if (cond) return;` emits a
  jcc/jmp PAIR; the original's single `jcc`-to-epilogue comes from NESTED
  `if (!cond) { ... }` with no early return.  (2) 16-bit flag updates must
  be COMPOUND word ops end to end (`pT->flags |= orBits;
  pT->flags &= (uint16_t)clearBits;` gives `or ax, word [ebp+S]`); the
  value-cast spelling widens through eax with masks (+2 insns).  (3) /Od
  LOCAL HOMES ARE KEYED BY AN INTERNAL NAME HASH, not declaration order --
  renaming locals shuffles their [ebp-N] slots.  Probe empirically
  (compile + read the first store per slot); single-letter sets hit
  quickly (a/y/z/b matched a 5-local frame on try #11).  Also: /Od
  recomputes every expression -- transcribe repeated subexpressions
  literally, never introduce caching locals.
- **Direct 3-stack-arg thiscall (no vcall) is one insn from C
  (2026-09-01, 0x1002EB03).** `push b; push g; push r; mov ecx,this;
  call F` with callee-clean ret 0xC: the __fastcall trick must
  materialise its dummy edx (`xor edx,edx`), leaving a 2-byte insertion
  that shifts everything after.  True C++ TU territory
  (cxx-thiscall-wall); everything else in the function can still be
  brought exact around it.
- **Timer-accumulate store forwarding: t-less compound spelling forces
  the re-read (2026-09-01, 0x100414F0 BrUiTickSteps).** For
  `accum += now - last; last = now;` followed by a branch comparing
  `accum`: the t-less spelling reloads `accum` from memory in the branch
  (`mov ecx,[esi+disp]`, the original's shape) and the scheduler emits
  the `last` store FIRST.  Introducing a delta temp
  (`t = now - last; last = now; accum += t;`) makes VC5 FORWARD the
  stored value across the branch instead (`mov ecx,edx`, 2 bytes) — one
  fork, 147-diff cascade through the whole tail (register rotation +
  store-order swaps downstream).  Same family: a short local for a
  paced-counter global spills and grows an ebp frame; DIRECT GLOBAL
  REREADS CSE into ax with no spill (0x10041940, the existing
  direct-global-reread idiom applied to word globals).

- **Float copies the orig emits as integer movs while a call result rides
  st0 = spell them as dword puns (0x1006F720 BrEntSetHeading, 2026-09-01,
  byte-count exact).**  Under /O2 a plain `pDst->f = localFloat;` run of
  copies gets BATCHED onto the FPU (fld/fld/fld + fxch + fstp each) the
  moment any other float value is pending (a deferred call result, a
  nearby fld/fmul); no statement-order permutation, volatile pin, or /Op
  breaks the batch without breaking something else (/Op also spills the
  call results the orig stores direct).  `*(uint32_t *)&dst = *(uint32_t
  *)&src;` reproduces the original's mov-reg pair copies exactly and
  collapsed 241 masked diffs to 25 in one step (multiset 0+0).  Same
  function proved two siblings: a `x3 = x2 = x1 = f(h)` chained
  assignment is the orig's fst/fst/fstp triple store, and struct fields
  re-read AFTER an intervening call must be read (and fanned out) as
  dword puns too, or VC5 constant-propagates the zeros it just stored and
  the orig's reloads vanish.  Remaining 25-diff residue on 0x1006F720 is
  ONE scheduling fork: the orig defers the pending sin-result fstp past
  the first three integer stores; every probed source pops it at call
  return.  Parked.

- **Long-class harvest (2026-09-01):** (1) `call A; jmp B` at a function
  start = a TAIL-CALL WRAPPER (`{A(); B();}` — VC5 turns a trailing call
  into jmp) that the map merged with the jump target; split the map row
  (0x1005A6A0/0x1005A6B0, both MATCH after the split; corpus scanned,
  no other instance). (2) Ghidra artifacts now transforms: discarded
  float call + bare `ftol()` (`ftolfuse`), walker-rewind strcpy
  explosion (`walkerstrcpy`), dead COM-failure re-zero (`deadnull`).
  (3) `extern int s_*` vs `extern char s_*[]` still the top single fix
  (0x100356B0). (4) Ghidra `iVar = X + carry` pairs = a 64-bit
  `__int64` add (`add/adc`) — spell via an __int64 LOCAL temp
  (direct global `+=` interleaves load/add/store per half); 0x10071F00
  parked at 3 diffs (high half ecx vs edx, all spellings/flags probed).
  (5) VC5 SORTS adjacent literal global stores BY VALUE (all -1s, then
  0s); an orig that alternates -1/0 stores with a second zero register
  is NOT literal-store source — unsolved on 0x10013E80 (52 diffs,
  memset-expansion and named-local probes failed; do not repeat them).
- **/GX menu-builder trio (2026-09-01, 0x100425E0 BrUiRootEnter, 2659 B
  byte-exact).** Proven on the EH-frame `new`-ladder class: (1) a
  null-check that emits `sete al / test al` is a CHAR bool computed
  AFTER the slot store (`a18[w14] = p; char bad = (p == 0); if (bad)`);
  an int bool copy-props back to a plain jne. (2) A short temp for
  `w14 + 1` allocates to AX — the original's DX comes from spelling it
  INLINE at the store (`p->w2AB6[0] = (short)(cont->w14 + 1);`), which
  also keeps the 16-bit `inc dx`. (3) Emitted order load-w14 / inc-w2AB4
  / store-w2AB6 comes from SOURCE order w2AB6-store FIRST, w2AB4-inc
  second — the scheduler sinks the store past the RMW inc (same
  store-reorder freedom as the 0x100414F0 f1C/f3850 pair). Each `new`
  is its own EH state; states/FuncInfo/unwind actions all fall out of
  plain `p = new BrCtl;` under /GX (cpp_score verifies all four pieces).
- **`fst` (not `fstp`) of a float local does NOT mean the source was
  double** (2026-09-03, 0x1006D530 BrRbQuatDerivative, 171 -> 23 diffs).
  At `/O2` (no `/Op`) VC5 is free to store a float local to its slot and
  keep the still-live unrounded value in st, spending it once before
  reloading the slot for every later use. Reading that asymmetry as a
  mixed float/double source and typing the local `double` with per-use
  casts produces a `fmul qword` / `fld qword` chain and blows the size
  (206 -> 262 B here). **Falsify it by COUNTING**: sum the `fld`s of each
  slot plus its kept register copies and check the total against the
  number of times the value appears in the expressions. If they agree,
  the local is a plain `float`. A `fmul dword [const]` scale is a `0.5f`
  literal, never `0.5`.
- **An aggregate local blocks dead-parameter-slot packing.** Same
  function: three scalar `float hx, hy, hz;` let VC5 pack the third into
  the dead incoming-parameter slot (`sub esp, 8`, third local at
  [esp+0xc]) per the "PACKS ORDINARY LOCALS INTO DEAD PARAMETER SLOTS"
  entry above. Declaring them as ONE aggregate -- `BrVec3 h;` or
  `float h[3];`, which compile identically -- allocates the object whole
  and restores `sub esp, 0xc` with the slots in declaration order. So
  `sub esp, <exactly N locals * 4>` next to a parameter that dies at the
  first instruction is evidence the source used an aggregate.
- **Commutative x87 addend order is SOMETIMES a no-op and sometimes the
  whole match — always probe it, never assume.** In 0x1006D530 swapping
  `a*b + c*d` to `c*d + a*b` in any row was byte-identical (so is
  swapping the operands of a both-memory `fmul` in 0x1006DE70). In
  0x1006DAD0 the same kind of swap on the FIRST of three rows --
  `(y*y + z*z)` for `(z*z + y*y)` -- was the difference between
  byte-exact and 113 differing bytes: it picks which square is computed
  first, and so which value the dead parameter slot gets recycled for.
  Rule of thumb: the swap matters when the two addends compete for a
  scarce home (a stack slot, the dead parameter slot), and not when both
  live on the x87 stack anyway. It is 3-7 cheap compiles to settle;
  settle it rather than reasoning about it. (Association of a 3-term row
  is a separate, always-load-bearing question: `((t1+t2)-t3)` is faddp
  then fsubp, `t1+(t2-t3)` the reverse.)
- **`c ? K1f : K2f` is fld/fstp; `if (c) x = K1f; else x = K2f;` is two
  integer-immediate stores.** VC5 stores a float LITERAL as
  `mov dword ptr [mem], 0x3f800000` (and via the zero register for
  0.0f), but a ternary makes the value runtime-selected and costs an
  `fld` of each constant plus an `fstp`. Orig 0x1006DAD0's identity-matrix
  fill is `mov dword [esi+ebx*4], 0x3f800000` / `, edi`, so the source is
  an if/else with a literal in each arm, not `(i == j) ? 1.0f : 0.0f`.
  The if/else form also lets the strength reducer fold the field's byte
  offset into the row IV (`mov ecx, 0xc` / `add ecx, 3` / `cmp ecx, 0x15`
  for a 3x3 at struct offset 0x30) -- the ternary blocks that too.
- **Declaration order of scalar float locals picks which one is packed
  into the dead parameter slot.** With N locals and fewer frame slots
  than N, the choice of which overflows into the dead incoming-parameter
  slot follows source declaration order, and getting it wrong can make
  VC5 drop one local entirely (reading the field in place with
  `fld [reg+disp]` + `fmul st` instead of homing it). On 0x1006DAD0 the
  three edge lengths had to be declared with `x` LAST; `x` first cost the
  `y` home, `x` in the middle reversed the squares. Only three orders
  need trying, and the diff tells you immediately -- a MISSING
  `mov R,[R+I]` + `mov [esp+S],R` pair with an EXTRA `fld [R+I]` is
  exactly this.
- **`fld [1.0f]; fdiv dword ptr [mem]` is `1.0f / x`, not a double
  divide.** The operand width on the `fdiv` settles it; a
  `(float)(1.0 / (double)x)` spelling emits `fdivr qword`. The two agree
  numerically on every normal float, so no test can catch the difference
  and the bytes are the only evidence.

## SIB base/index order on `member_array[index]` — NOT source-reachable
*(proven 2026-09-03 on 0x100540D0 / 0x10054280, the font-A/font-B glyph walks)*

`mov al, [edi + eax + 9]` has two legal encodings. The original picks
`8a 44 07 09` (SIB base=edi/`this`, index=eax/the index). Our staged cl
emits `8a 44 38 09` — same instruction, same registers, same effective
address, base and index swapped. **One byte, and nothing in the source
moves it.** Twenty-one spellings across four orthogonal axes and eight
flag sets were probed; the full list is in the header of
`src/core/cpp/0x100540D0.cpp`. Everything canonicalizes: the address
expression (`sz[i]`, `*(sz+i)`, `*(i+sz)`, `i[sz]`, `((char*)this)[i+9]`,
`*((char*)this+i+9)`), the loop shape (bottom-read while, top-read
`for(;;)`+break, read-in-condition, `sz[++i]`), the indirection (inline
accessor returning `char*`, hoisted `char *self`, typed `Text540D0 *p =
this`, pointer-to-array cast), and the declarations (array extent, element
signedness, local order).

**The counter-evidence that makes this precise** — our cl DOES emit
base=pointer, so it is not a blanket emitter rule. A scan of 937
byte-exact functions found 94 scale-1 two-register memory operands, 67 of
them with base reg# > index reg#. The ones that reproduce base=pointer all
share a shape: **the base is a pointer VALUE living in a register**, e.g.
`pBs->pBuf[pBs->writeByte]` in 0x1006D000 (`mov [edi+ecx],dl`, edi=pBuf
loaded from +0x10) and `((char*)param_1)[8 + strlen(...)]` in 0x10054390
(base = the pointer parameter). When the base is instead `this` plus a
constant member offset — a member array subscript — our cl always makes
the *index* the SIB base.

So the encoding is decided by whether the address root is a materialised
pointer or a `this`+const symbol reference, and the original's root was
the former in a context where ours is the latter. Either the shipped
source reached that buffer through a pointer our reconstruction cannot
recover from 178 bytes of asm, or the compiler patch level differs here.
**Do not re-probe spellings.** If you want to move it, the only live
leads are (a) a construct that forces the text base into a register as a
genuine pointer value without changing any other byte, or (b) the
compiler-build lead already open for 0x1000EAF0.

## Inline `memset` setup order — a scheduling residue, not a fixed table
*(2026-09-03, 0x1006FCE0 and 0x100087D0; CORRECTED same day by 0x1003AB00)*

Our cl expands a constant-size zeroing `memset` as
`mov ecx,N / xor eax,eax / lea edi,dst / rep stosd`. The original emits
`lea edi,dst / mov ecx,N / xor eax,eax / rep stosd` — same instructions,
dest computed first. Nothing at the call site moves it: the destination
spelling (array name, `&a[0]`, a hoisted `char *`, a pointer local
declared at the top of the function), the destination type (`int[8]`,
`unsigned char[32]`, a nested struct with `sizeof`), and eleven flag sets
all leave the recomp order unchanged. If a function is otherwise exact
and its only residue is those three instructions, it is this — park it.

**Confirmed a second time on 0x100087D0, in the VARIABLE-length expansion**
(the shr-2 / and-3 stosd+stosb pair), which is a different expansion with
the same directional difference:

    orig    mov ecx,0x40 / lea edi,dst / sub ecx,esi / xor eax,eax
    recomp  mov ecx,0x40 / xor eax,eax / sub ecx,esi / lea edi,dst

In both of those the original materialised the destination pointer before
the fill value and ours did the reverse.

**CORRECTION — that is not a fixed rule.** 0x1003AB00 has the original
emitting `mov ecx,8 / xor eax,eax` ABOVE the register saves and the
`lea edi` after them (edi is not free until `push edi` has run) — i.e. the
count-and-value-first order, which is exactly what our cl produces there,
and that function matches on this point. So the setup order is scheduled
against what else is happening around the call, not fixed by the
expansion. Treat 0x1006FCE0 and 0x100087D0 as an unexplained scheduling
residue in that surrounding code, not as evidence of a different expansion
table, and do not cite them as compiler-build evidence.

Together with the SIB base/index entry above, that is two emitter-level
residues in one session that no source form reaches. Both are consistent
with a compiler patch level slightly different from the staged one, which
is the lead already open for 0x1000EAF0.
- **A chained assignment is `fld` + `fst` + `fstp`; two separate
  assignments from the same expression are integer copies.**
  `a = b = pSrc->m[i][j];` loads the source once and spends it twice --
  `fld [src]`, `fst [b]` (stored and kept), `fstp [a]` -- which is
  exactly what orig 0x1006DC70 BrMat4ToMat3Both does. Writing it as two
  statements (`b = pSrc->m[i][j]; a = pSrc->m[i][j];`) lets VC5 CSE the
  load into an integer register and copy with `mov`, costing three extra
  instructions. The generalisation of the `x3=x2=x1=f()` triple-store
  entry: N stores from one chain are N-1 `fst`s and a closing `fstp`, and
  the leftmost target in the chain is stored LAST. Diff signature:
  EXTRA `mov [R], R` / `mov R, [R]` against MISSING `fld [R]` /
  `fst [R]` / `fstp [R - I]`.

## Cross-jumping: our cl merges identical error tails, the original does not
*(proven 2026-09-03 on 0x10059350, the DirectInput device bring-up)*

Three `if (hr < 0) { Report(hWnd, hr, ErrLine(N)); return 0; }` blocks. The
first two compile to byte-identical instruction sequences (only the pushed
line number and the call displacement differ). The original emits three
separate copies; our cl tail-merges the first into the second —
`jge +7 / push 0xAC / jmp` landing on the second block's `call` — and the
function comes out 21 bytes short with every other byte identical.

Not reachable from the source: separate `hr` locals per step change
nothing, and neither do /Gy, /Gf, /Op, /Oy, /Ot, /Ob0, /Ox /Ob0 /Gy, or
/Og /Oi /Ot /Oy /Ob1. Rewriting the chain in nested `if (hr >= 0)` form
does suppress the merge, but at the cost of moving the success return
inline and reordering the error blocks — the original's layout is the flat
early-return one, so that is not the answer either.

**How to recognise it:** recomp shorter than orig by exactly one error
tail, first divergence at a `jge`/`jl` whose displacement is far too
small, and a stray `jmp` where the original has a full block. If a
function is otherwise byte-identical and this is the residue, park it.

This is the third emitter-level residue found in one session, and the
strongest: the other two are orderings inside an expansion, this is a
whole optimisation our cl performs and the original's did not.

## Byte stores push narrowing UP the expression — dam it with locals
*(proven 2026-09-03 on 0x1006D0B0, the bit-stream bit writer)*

When the destination of a compound expression is a single byte, VC5 pushes
the truncation back up the tree as far as it legally can (through `|`,
`&`, and `<<`, but not `>>`), and every mask it reaches gets built in 8-bit
registers — `mov bl,1 / shl bl,cl / dec bl` instead of
`mov ebx,1 / shl ebx,cl / dec ebx`. Which masks narrow is a source
decision, and it moves 30+ diffs at a time.

Three dams, each independently load-bearing on that function:

1. **One cast, outermost.** `*p = (unsigned char)((f << sh) | (*p & keep))`
   — an inner `(unsigned char)` on the field lets the narrowing reach the
   field mask.
2. **The byte pointer in its own local.** Writing `pBuf[byteIdx]` on both
   sides of the statement lets VC5 re-associate
   `(value & (mask << nbits)) >> nbits` into `(value >> nbits) & mask`,
   which changes which mask is live and narrows it.
3. **Each mask that stays 32-bit in the original gets its own
   `unsigned int` local.** An assignment to a wider local is a barrier the
   narrowing does not cross; inline, it keeps going.

Counter-pressure: locals are not free. A fourth live local in that loop
made VC5 set up an ebp frame, and the original is frameless with ebp as a
general register and a single spill slot. Add dams one at a time and watch
the prologue.

## `strlen` of a literal folds; of an extern array it scans
*(proven 2026-09-03 on 0x10055C50)*

`strcpy(buf, pKey + strlen("RallySeason"))` compiles to a `lea` — VC5
constant-folds `strlen` of a string literal outright, and the inline scan
disappears. The original expands the scan (`or ecx,-1 / xor eax,eax /
repne scasb / not ecx / dec ecx`) over the string itself, which means the
prefix was NOT a literal the compiler could see through. Spelling the
prefixes as `extern char s_Name_<va>[];` — the convention slice6_73.c
already uses — restores the scan. Cost of getting it wrong: 37 bytes and
two whole scans, i.e. a diff that looks structural and is one declaration.

## Where an address is FORMED decides whether the trailing offset folds
*(same function)*

Two branches each pick a table, index it, and hand the result to a common
`strcpy`. The original computes one `lea [tbl + idx*4 + 4]` after the arms
merge. Three source shapes, three different code shapes:

- table pointer and index as two locals, indexed at the use → VC5 hoists
  the index scaling (`mov/shl 6/add`, the `*65` for a 0x104 record) out of
  the arms; that sequence goes missing from both arms (3 insns).
- a record POINTER in the arms, `->field` at the use → the arms end with
  `lea [tbl + n*4]` and the field's `+4` becomes its own `lea [R+4]` at
  the use (2 extra, 1 missing).
- **each arm produces the FINAL `char *`** (`pRec = tbl[atoi(s)].szName`)
  → both arms end with an identical `lea [tbl + n65*4 + 4]`, VC5
  tail-merges that one instruction, and the `*65` stays duplicated in the
  arms exactly as the original has it. Byte-exact.

Generalise: when the original merges arms on a single address-forming
instruction, push the whole address expression INTO the arms and let the
tail-merge factor it. Do not factor it yourself into shared locals.

**Confirmed a second time, on a CALL, in 0x10037E60.** The original picks
one of two catalogue indices and calls one string helper:
`push 0x51 / jmp / push 0x0C / call`. Passing the choice as a ternary
argument (`f(cond ? A : B)`) materialises the index into eax and pushes
once — wrong. Writing it as an if/else with the CALL duplicated in both
arms is right: VC5 tail-merges the call and each arm keeps its own
`push imm`. Same lever, whether what merges is a `lea` or a `call`: put
the whole expression in the arms.

## A pinned zero register costs a frame dword — recognise it, don't grind it
*(observed 2026-09-03 on 0x10054E20)*

A function with two separate "clear a dozen fields to 0" blocks and a loop
between them. The original materialises 0 in ebx for the first block, lets
it die, uses ebx as the LOOP COUNTER through the loop (so the loop's zero
tests are `test ecx,ecx`), then re-zeroes it with a fresh `xor ebx,ebx`
before the second block. Our cl keeps one zero live in ebx across the
whole function, so the counter has nowhere to live, spills, and the
prologue grows from `push ecx` (one dword) to `sub esp,8`.

**Signature:** prologue is `sub esp,N+4` where the original has `push ecx`
/ `sub esp,N`; every subsequent stack displacement is off by 4; and inside
the loop the original compares with `test r,r` where the recomp has
`cmp r,ebx`. That last pair is the tell — it says which side is holding a
constant in a register.

Not reached by: a separate local for the tail block's index, scoping the
pointers into their blocks, rewriting the loop with explicit induction
pointers (worse), or /O2 /Op, /Ox, /O2 /Ot, /O2 /Gy, /O2 /Ob0. Treat a
whole-body 4-byte displacement shift with this signature as an allocator
park, not a structural miss — the diff count is large and meaningless.
- **MSVC 5.0 REJECTS `__thiscall` in C -- `error C4234: '__thiscall'
  keyword reserved for future use`** (verified 2026-09-03 on a bare
  two-line TU). **The wall is the CALL side, not the definition side,
  and the two must not be confused.**
  - DEFINING a multi-argument thiscall callee is FINE:
    `void __fastcall F(T *pThis, int _edx_unused, <stack args>)` puts
    `this` in ecx and every remaining argument on the stack, and the
    callee simply ignores edx -- nobody has to materialise it. That is
    how this tree's entity setters are spelled, and 0x1006F970
    BrEntSetMatrix (`mov ebx,ecx` / `[esp+4]` / `ret 4`) fell byte-exact
    on the first compile with it. br_match.h's warning that BR_THISCALL1
    "does NOT generalise" is about call sites; do not read it as saying
    these functions cannot be matched.
  - CALLING one is ALSO reachable -- **this entry said otherwise
    earlier on 2026-09-03 and was wrong.** Declare the callee (or the
    function POINTER, which is what a vtable send needs) `__fastcall`
    with EVERY stack argument as a one-member STRUCT: structs are never
    register-eligible, so ecx takes `this`, edx is left alone, and the
    callee cleans its own stack. Proven on 0x10030270 BrModelLoad
    (byte-exact) and on slice8_85.c's vtable sends, where the call went
    from `push <this>` + `add esp,0x10` to `mov ecx,<this>` + three
    pushes + `call` with no caller-side cleanup.
    - ALL the stack arguments must be structs, not just the second.
      MSVC keeps assigning registers PAST an ineligible argument, so
      `(pThis, struct, int, int)` hands edx to the third argument.
    - The cost: a struct argument is always materialised through a
      register, so a CONSTANT argument comes out `mov ecx,K; push ecx`
      where a plain int would be `push K` -- two bytes each.
      Initialising the struct in its declaration instead of by
      assignment does not help (probed). When a function's whole residue
      is that materialisation, the convention is right and the rest is
      the trick's own overhead.
    - Diff signature of getting the convention WRONG (plain cdecl):
      EXTRA `push R` and `add esp, I` once per send, MISSING one
      `mov ecx, <this>` per send.
- **VC5 inlines NO static helper at /O2 -- three confirmations.** Any
  `static` function the original had in line comes back as a real `call`
  with a full argument push sequence, whatever its size: an eight-dword
  struct-field mirror (BrEntMirrorQuat, in 0x1006F970), a five-argument
  vtable send (below), and the wrapper/body splits already recorded
  above. Either write the body out at the call site or make the helper a
  MACRO; there is no third option, and `__inline` is not one (see the
  macro-vs-inline entry). Diff signature: EXTRA `call I` + `push R` runs
  + `add esp, I`, MISSING the instructions the helper's body would have
  contributed.
- **A static helper that wraps a vtable send is never inlined -- make it
  a MACRO.** VC5 turned `static void Send(fn, obj, a, b, c) { if (fn)
  fn(obj, a, b, c); }` into a real `call` per send, with the object
  pushed as an extra argument and a caller-side `add esp, 0x10`, where
  the original fetches the slot (`mov eax,[ebx]; mov eax,[eax+0x14]`)
  and calls it in place. As a macro the fetch and the sends land exactly
  where the original has them (0x10038000: 110 -> 96 bytes against 92,
  REGNORM gap 12+6 -> 4+2). Two things have to go together: the macro
  AND the null guards -- a port-safety `if (fn != NULL)` or
  `pVtbl != NULL ? ... : NULL` is a branch the original does not have.
- **/Od tells a ternary from an if/else by the FRAME SIZE.** At /Od a
  ternary assigned to a named local lands in a compiler temp first and
  is then copied: `mov [ebp-8], 1 / jmp / mov [ebp-8], 0 / mov eax,
  [ebp-8] / mov [ebp-4], eax`, so ONE named int local costs `sub esp, 8`
  and carries a redundant slot-to-slot copy. An if/else writes the local
  directly and needs only four bytes. So `sub esp, <4*(locals+1)>` plus a
  copy between two adjacent slots is a ternary, every time. (At /O2 the
  same distinction shows up the other way -- see the two-constant-ternary
  entry, where the ternary is the BRANCHLESS neg/sbb form and the if/else
  branches.) Proven 0x1002DB0B BrDlOwnerFixup.
- **A table argument passed as `push <imm address>` means the symbol is
  the OBJECT, not a pointer to it.** `push 0x100AA068` is the address of
  an array; a `const void *g_Table;` variable would emit
  `mov ecx,[g_Table]; push ecx`. Retype it as an incomplete extern array
  (`extern const unsigned char g_Table[];`) and pass it bare -- and do
  NOT invent an element count the tree cannot pin. This is the
  argument-passing half of the "fixed-address global array folded as a
  displacement" entry above; the indexed half looks like
  `fld dword ptr [eax*4 + 0x106EC4F8]` with no base register, which
  likewise means the `float *aTable` parameter never existed (proven
  0x1002DE6B BrAccumAddClamp, 88 B byte-exact once the parameter was
  deleted). `tools/screen_absglobals.py` ranks the rest of this class.
- **`flags |= K` on a narrow field, never `flags = (T)(flags | K)`.**
  The compound form reads the halfword straight into a 16-bit register
  and ors the low byte (`mov cx,[eax+0x4c]; or cl,8`); writing the cast
  out longhand makes the read a widening one and adds a
  `xor edx,edx` + `mov dx,...` pair the original does not have. Proven
  0x1002DB0B.

## x87 chains: one NAMED float local per intermediate
*(proven 2026-09-03 on 0x1003A580, a float-heavy 322 B function matched to 0)*

The float class has a reputation here as unreachable scheduling. It is
not, when the chain is a straight-line sequence of conversions and
products. The rule is mechanical: **every value the original keeps on the
x87 stack or homes to a slot corresponds to a named float local in the
source.** Write one local per intermediate and the stack discipline falls
out; leave them inline and VC5 re-associates and spills.

Measured on the same function, same everything else:

    five int locals, conversions inline          205 diffs
    + name the two int-to-float conversions      155 diffs
    + name the product that is used once more      0 diffs  (byte-exact)

What each naming buys, and what to look for in the original:

- `fld st(0)` (duplicate) means a converted value is read by two later
  statements — name the conversion.
- `fst` **without** a pop into a slot means a value is read several times
  later — name it; VC5 homes it and reloads with `fld [slot]`.
- `fsubr st(1)` — an operation against a copy still on the stack — means
  both operands are named values, not one named and one inline subtree.
- Spills the original does not have, and `fld [const]; fmul st(1)` where
  the original has `fmul [const]`, are the signature of an UNNAMED
  intermediate: VC5 chose the other association.

This does not repeal the float wall for genuinely tangled DAGs (the
0x1000EAF0 / BrVec3Project class), but before calling a float function a
coloring wall, write its chain out as named locals first — it is cheap and
it moved this one from 205 to 0.
- **/Od loop-and-branch shapes, three levers proven together on
  0x1002E73A BrDlRebase (101 B, byte-exact).**
  (1) **A wrapping `if` is not an early return.** `if (p) { <rest> }`
  emits ONE inverted jump to the epilogue (`cmp [ebp+8],0; je end`);
  `if (!p) return;` emits the two-instruction `jne over / jmp end` the
  /Od-goto entry above describes. When the original has the single `je`,
  the source wrapped the body.
  (2) **The step belongs in the `for`'s THIRD clause.** `for (;; p += 2)`
  puts the increment at the TOP of the loop with a `jmp` over it on the
  first pass -- a three-instruction block the back-edge lands on -- while
  `p += 2;` as the body's last statement puts it at the bottom. The
  jump-over is the signature.
  (3) **SWITCH ON THE EXPRESSION, NOT A NAMED LOCAL.** A named switch
  value costs a SECOND frame slot: VC5 copies the local into its own
  switch temp and compares that (`mov eax,[ebp-4]; mov [ebp-8],eax; cmp
  [ebp-8],K`). Switching on the expression makes that temp the
  function's only local and restores a `push ecx` prologue. And read the
  compare ORDER before choosing switch vs if/else -- a chain that runs
  ASCENDING by case value, with two cases sharing a target, is a switch;
  an if/else chain tests in SOURCE order (see the sparse-switch entry).
- **A dispatch whose arms each name a CONSTANT index is a switch, not one
  indexed assignment.** Orig 0x10062B10 has FOUR duplicated `rep movsd`
  blocks, each with its own epilogue, a distinct source address and a
  distinct destination displacement on the same stride. Computing the
  index once (`p->profile[k] = defaults[k]`) emits index arithmetic and
  one copy; the source is a `switch` whose every arm spells its index as
  a literal, with the default arm first in code order. Byte-exact at 103
  B once written that way. Same reading applies to any run of
  near-identical blocks that differ only by a displacement: the
  duplication IS the source, and folding it into arithmetic is the
  defect.
- **`shared.csv`'s `matched_by` column is evidence, and `slot` is the
  weak grade.** A `d3d`-tagged body is scored against the GLIDE bytes
  its address maps to. When that mapping is `slot` -- the two functions
  merely occupy the same dispatch slot -- the two builds may share no
  code at all, and several such rows say "DIFFERENT CODE" outright. The
  symptom is a large permanent diff on a function that is often already
  byte-exact under its Glide name, sitting near the top of the lane
  ranking as the best available target. `@d3donly` is the label for it.
  `tools/screen_slotpairs.py` lists them; each needs eyes, because slot
  pairing is weak evidence and not proof of difference.
- **A thunk must never carry the `@implements` for the body it calls.**
  A second NAME for an address -- an adapter that forwards to the real
  implementation elsewhere in the tree -- compiles to a ~32-byte call
  that can never reproduce a body of hundreds of bytes, so tagging it
  puts one address in the measured set twice and leaves one of the pair
  permanently unmatchable. Move the tag to the body and leave the thunk
  a comment naming where the match lives (the convention slice6_74.c's
  BrVec3Len note established; 0x1006A4A0 and 0x10073C90 were both found
  this way on 2026-09-03). The same trap in reverse: a tag sitting on a
  FRAGMENT of a larger original, e.g. 0x1005B2B0, where the twelve-line
  function tagged was the first thirty bytes of a 212-byte routine.
- **The recompile's 16-byte alignment padding is not code, and counting
  it fabricated "the instruction counts are EQUAL" on two giants
  (2026-09-03).** A COFF function extent is padded to a 16-byte boundary,
  so the object ends in up to fifteen trailing `nop`s that the extracted
  original does not have. `divergence.py` counted them, and three
  dossiers then quoted an equality that never held: 0x1000EAF0 is 2,322
  vs 2,328 (six short) and 0x1000A110 is 1,828 vs 1,843 (fifteen short,
  56 bytes short), while only 0x100250D0 was genuinely at parity. The
  0x1000A110 frame census had reasoned *from* the false equality that no
  value was missing and that the frame gap was a packing curiosity.
  Both tools now strip the padding. **An instruction-count equality is a
  load-bearing claim -- it is what licenses "the residue is shape, not
  missing code" -- so never quote one that has not been padding-corrected,
  and re-check any older claim of the same shape before building on it.**
- **`msetdiff.py` needs the reloc ADDEND normalised or every reloc'd
  instruction pairs as both MISSING and EXTRA.** The linked original
  prints `push 0x106e9a38` / `[esi + 0x1035faf0]` / `[esi + <sym>+4]`;
  the object prints `push 0`, `[esi]`, `[esi + 4]`, because the field
  holds the addend and capstone renders a zero displacement as none at
  all. Untreated, this reported two "we pass 0 where the original passes
  a pointer" defects on 0x1000EAF0 that were nothing of the kind, and
  buried the real rows under 39 of noise (76 -> 37 once fixed, plus
  branch/call rel32 targets, which rotate wholesale whenever an earlier
  region changes size). **Check the reloc list before acting on any bare
  `push 0` or zero-displacement row.**
- **VC5 cross-jumps N arms when their tails are byte-identical, and an
  x87 pop's position decides it (0x1000A110 colour arms).** The original
  gives arm 1 its own copy of the colourB pack and shares one tail
  between arms 2 and 3; our build shares one tail across all three and is
  37 bytes short in that block. The only difference is where the leftover
  `fstp st(0)` lands -- inside arm 1's pack in the original, before the
  jump in ours -- which makes our three tails identical and merges them.
  Same emitter-level residue as the C++ lane's identical error tails: no
  source spelling of the arms reaches it, and giving arm 1 its own byte
  locals un-merges part of the pack but moves the first divergence 27
  bytes earlier.

## float-vs-int typing: the prologue tells you which
*(proven 2026-09-03 on 0x10038F40 -- 425 diffs to 10 on this one change)*

A value parked across a call and restored afterwards, moved in and out
with plain `mov` and set from an immediate, looks exactly like an int:

    mov edx, [esi+0x40]        ; park
    mov [esp+0x10], edx
    mov dword ptr [ebx+0x2f70], 43020000h    ; a constant
    ...
    mov eax, [esp+0x14]        ; restore
    mov [ebx+0x2f70], eax

It is not. VC5 moves FLOAT memory with GP registers and stores float
constants as immediates, so float code in this shape has no x87 in it at
all. The difference is only visible in the frame:

- **int local** -> lives in a callee-saved register across the call, so
  the prologue gains a push and the frame is 4 bytes SMALLER.
- **float local** -> must be homed to a slot, so no extra push and the
  frame is 4 bytes BIGGER.

**TELL: recomp has one more callee-saved push than the original AND a
frame 4 bytes smaller -> you typed a float local as int.** The reverse
pairing means the opposite. Check this before assuming a dword-pun
spelling is needed: here the pun was wrong and honest float typing was
right, even though every instruction involved is an integer `mov`.

## The port's ops/host-hook table is a CAUSE class, worth a screen
*(proven 2026-09-03 on 0x1001CC00 BrRallyMain -- byte-exact first compile --
and 0x1001D8A0 BrDxDetect -- 826 diffs to 19)*

Several early modules were written with a dependency-injection seam: a
`BrXxxOps` / `BrXxxHost` struct of function pointers plus a `pUser`/`pCtx`
handed to every callee, and often a null-check preamble in front of it. The
original calls its platform DIRECTLY, so every one of those becomes
`call dword ptr [ops+N]` with an extra argument pushed, and the function
cannot match no matter what else is right. On BrRallyMain that alone was the
entire 116-instruction register-blind gap.

**Screen for it, don't discover it per function:**

    awk -F, 'NR>1 && $4=="diff" {print $1}' build/match/report.csv | sort -u |
      while read f; do n=$(grep -c 'p\(Ops\|H\|Host\)->pfn' "$f"); \
      [ "$n" -gt 3 ] && echo "$n $f"; done | sort -rn

As of 2026-09-03 that names nine files; br_boot.c and br_dxver.c are done,
and br_uiboot.c (23), br_window.c (16), br_mainloop.c (6), br_strres.c (6),
br_texinit.c (5), slice4_52.c (32) and slice1_06.c (4) are open.

**The fix is mechanical, and it does NOT touch the port arm:**

1. `#ifdef BR_MATCHING_BUILD` a second definition with the ORIGINAL's
   signature; the existing one becomes the `#else`. Guard the prototype in
   the header the same way -- that is the only header edit needed.
2. Declare the callees LOCALLY inside the matching arm rather than including
   their headers, because those headers carry the port's ops signatures too.
   Reloc-masking means the names are free; the SHAPE is what must be right.
3. Win32 imports need `__declspec(dllimport) <ret> __stdcall` to produce
   `call dword ptr [__imp__...]`; a plain extern gives a linker thunk
   (`call rel32`) instead. The convention is already used in slice1_01.c.
4. COM sends want a padded vtable struct so the member offsets land where
   the original's `call [ecx+N]` says. Only name the slots you send to; fill
   the gaps with `void *apfnXX[n]` and check the arithmetic.
5. A `__thiscall` callee with stack arguments is spelled `__fastcall` with
   EVERY stack argument in a one-member struct -- see the entry above.

Cost on the two done so far: about 40 minutes each, one of them byte-exact
on the first compile.

## Do not cache what the original re-reads
*(proven 2026-09-03 on 0x10039870, 201 diffs to 0; second sighting)*

An array element or global read twice in a row -- once per argument of two
adjacent calls, or once in a test and again in a call -- is a value the
original reads TWICE. Caching it in a local is the obvious "clean" thing to
write and it is wrong twice over: it costs a spill slot (`sub esp,N+4`
against the original's `sub esp,N`) and it rotates the loop's registers, so
the diff looks like an allocator wall when it is a source decision.

    aI = obj.GetA(kind, tbl[i].key);     /* right: two reads */
    bI = obj.GetB(kind, tbl[i].key);

    key = tbl[i].key;                    /* wrong: one read, one spill */
    aI  = obj.GetA(kind, key);
    bI  = obj.GetB(kind, key);

Same shape as 0x10038F40, where the arm's index expression appears once
inside the catalogue lookup and again for the descriptor because the
original recomputes it after the intervening call. **Read the original: if
the load is there twice, write it twice.** VC5 will not re-materialise a
cached local, and it will not spill a re-read.

## `/Od /Op` is a FIFTH compile variant, and it was missing from the sweep
*(proven 2026-09-03 on 0x1002BF50, the scissor emitter -- 242 diffs to 0)*

`/O2 /Op` was added in 2026-08 because MSVC 5.0 without `/Op` keeps an
int->float conversion in the x87 register, and with it every such conversion
rounds through a float32 stack slot:

    fild dword ptr [ebp+8]
    fstp dword ptr [ebp-0xC]      ; <- the round-to-float32 the flag adds
    fld  dword ptr [ebp-0xC]
    fmul dword ptr [scale]
    call __ftol

**The same blind spot exists one optimisation level down and nobody had
looked.** A DEBUG (`/Od`) translation unit that does float arithmetic needs
`/Od /Op`, and under plain `/Od` it reads as a large gap for a source that is
already exactly right — here four conversion groups and 16 bytes of frame,
i.e. `sub esp,0x10` against the original's `sub esp,0x20`. `tools/match_sweep.py`
now carries `Odp` in `VARIANTS`.

**Consequence worth acting on: every `/Od` TU with an int->float cast in it
was invisible to every sweep before 2026-09-03.** A full re-sweep is the way
to harvest that; the per-function `opt` column already names the ten rows
whose current best is `Od` and still `diff`.

### Two companion rules the same function pinned

- **A function's TU is fixed by ADDRESS, not by subject.** 0x1002BF50 sat in
  slice5_62.c (an `/O2` file) and could never match there. 0x1002BF4B is five
  bytes long and ends exactly at 0x1002BF50, so the function belongs to
  slice2_18.c's `/Od` unit. Before calling a function a wall, check which TU
  its NEIGHBOURING ADDRESSES are filed in and what `opt` that file won.
- **/Od slot depth: function-scope locals get the shallowest slots, inner-block
  ones go deeper, and DECLARATION ORDER DOES NOTHING.** Two `T *` locals
  declared either way both came out with the second at `ebp-4`. Putting the
  first user in function scope and the second inside its own `{ }` block is
  what lands `ebp-4` / `ebp-8` in the original's order. Compiler temps
  (an int subexpression stored so it can be `fild`ed) then fill the slots
  below in FIRST-USE order, interleaved with nothing else.
- **Recover a call's argument list from the push stream instead of guessing
  it -- and check the ORDER, because nothing else does (0x1000A110,
  2026-09-03).** cdecl pushes right-to-left, so within one call the Nth push
  is argument (nargs + 1 - N) and the last push is argument 1; a push of the
  function's zero register is a literal 0 in the source. That is enough to
  read a sixteen-token combiner call straight out of the original's bytes
  with no guessing. Two of BrCarDrawVehicle's thirty-four calls had the
  right tokens in the wrong slots, passing TK_ZERO where the original passes
  TK_TEXEL0 and 0x3F4 -- so the display list the function emitted was
  WRONG, not merely differently compiled. Fixing both closed four
  divergence regions. **Neither existing comparator can see this**:
  `divergence.py` wildcards imm32 when it normalises, and a multiset
  comparison passes a permutation. `tools/pushcensus.py` is the check; run
  it on any function that builds a display list before believing a region
  map. (Run on the other two giants it is clean, 14 of 14 groups each, so
  the technique does not just find noise.)
- **A call inside a branch belongs INSIDE EACH ARM, not after a selected
  variable (0x1000A110 model-DL hook, 2026-09-03).** Four arms selecting a
  display-list pointer into one local and calling once emits a single shared
  push pair; the original writes the call in each arm, which gives the first
  arm its own private `mov`/`push`/`push`/`jmp` and lets VC5 cross-jump only
  the three whose bytes are identical. Written that way the block is
  instruction-for-instruction and register-for-register the original. This
  is the same lever as the branch-selected DL emits above, and the way to
  FIND it is a per-call push count: the same 34 call groups on both sides
  with identical register-push totals and exactly one group short two
  pushes.

## Do not name a temp to "preserve" an observed load order
*(proven 2026-09-03 on 0x100400E0; the mirror of the re-read entry above)*

The original loads a global, stores 0 to a DIFFERENT global, then stores
the loaded value:

    mov ecx, [g_root]
    mov dword ptr [g_pending], 0
    mov [g_current], ecx

It is tempting to reproduce that with a temp (`p = g_root; g_pending = 0;
g_current = p;`) and the port body did exactly that. It is wrong: VC5
hoists the load above the unrelated store on its own, and naming the value
takes it out of eax, losing the one-byte-shorter `a1`/`a3` accumulator
encodings — 10 diffs on a 56-byte function. Write the two statements
plainly in either order and it matches.

Pair this with "do not cache what the original re-reads": both say the same
thing from opposite sides. **Load placement is VC5's decision, not the
source's.** Only name a value when the ORIGINAL keeps it in a register
across something that would otherwise clobber it.

## Screen the ORIGINAL bytes before believing the report's size column
*(three distinct bookkeeping faults found 2026-09-03, all in one pass)*

`report.csv` scores whatever C function carries the `@implements` tag against
the original at that address. Nothing checks that the pairing is sane, so a
wrong tag reads as an enormous, attractive gap. Three failure modes, each with
its own one-line screen:

**1. FALSE TWIN in `config/shared.csv`.** A `d3d` tag resolves to a Glide
address through shared.csv, and the pairing can simply be wrong. Found on
0x1001BAE0/0x1001E080: the D3D function is 26 bytes (two pointer stores and
`mov eax,1`), the Glide one is 173 bytes of 3dfx bring-up. Same renderer slot,
different code. **Screen: disassemble BOTH binaries at BOTH addresses and
compare sizes** — `BR_REF=orig/BRD3D.dll python3 tools/dumpasm.py <d3dVA>`.
Equal sizes means a real twin; wildly unequal means the pairing is false and
the tag should be `@d3donly`.

**2. THE TAG IS ON THE FORWARDER.** The tree sometimes has two C definitions
for one address: a short alias and the real body somewhere else. When the tag
sits on the alias, the report scores 32 bytes against 363 and the real
transcription is invisible to triage. Found on 0x100695D0. **Screen: any
tagged-diff row whose recomp is under a quarter of the original —
`awk -F, 'NR>1&&$4=="diff"&&$7>0&&$6>200&&$7/$6<0.25' build/match/report.csv`.**
Move the tag to the body; leave the alias untagged.

**3. …BUT SOMETIMES THE IMAGE REALLY DOES HOLD TWO COPIES.** Same screen, the
opposite conclusion: 0x1003CDA0 is 212 bytes in BOTH binaries and the "owner"
elsewhere in the tree is the same code again — two copies the linker did not
fold, not a forwarder and an owner. Writing the body out at this address, with
the DirectPlay vtable send and the KERNEL32 imports the original uses instead
of the port's struct of function pointers, was byte-exact on the first
compile. The distinction between (2) and (3) is decided by the ORIGINAL's size
at the address, never by the tree's own comments.

## The EH screen belongs in triage, not in your head
*(added to tools/fnmatch/triage.py 2026-09-03)*

`push -1 / push <handler> / mov eax,fs:[0]` is MSVC's `__try` prologue and no
C source emits it. The playbook has always said to check it by hand before
accepting a target; nobody did, and **41 tagged-diff rows / 37,677 bytes were
sitting in the ranking**, most of them reading `MISSING CODE (2% complete)`
because the port stands them in with a forwarder — which is exactly the
profile of an easy win. Twelve of them are one 201-byte family.

`triage.py` now reads the first two bytes of `build/match/orig/<VA>.bin` and
ranks any `6A FF` as `C++ EH FRAME - not reachable from C`, below the coloring
walls, so `claim_lane.py` hands them out last. They belong to the C++ EH
workstream ([[cxx-eh-frame-wall]]), not to the C lane.

## Before hunting a spelling, check whether the ORIGINAL is inconsistent
*(proven 2026-09-03 on 0x1000EAF0 wall 4; the same test applies anywhere)*

A construct that the original renders TWO different ways in two arms of the
same if/else is settled: it is a per-region allocation decision, and no source
spelling reaches it, because both arms come from the same source text.

The worked case: 0x1000EAF0 addresses two flat ring arrays by a common scaled
index. Grep the original's own disassembly for the two globals and the split
is flat — the if-arm materialises `lea edx,[ecx*4]` once and takes eight
`[edx + 0x1035faf0]` / `[edx + 0x1035f750]` sites, while the else-arm folds
all five of its sites as `[ecx*4 + abs]`, which is byte-identical to what the
recompile already emits everywhere. Eight passes of dossier had this filed as
one "`ring*4` CSE" wall to be solved by a byte-offset local, and the probe that
was measured against it converted BOTH arms — which cannot be right at any
spelling. **Run the grep before minting the probe:** one `grep` over a dump of
the original for the addressing forms of the symbol in question costs nothing
and can retire the whole lever.

The corollary is about missing instructions. When the original pins a register
this way it is a register short downstream, so it spills something the
recompile keeps live — here it homes a variable in both arms of a test and
reloads it (`mov [esp+0x20],ebx` x2, `mov edi,[esp+0x20]` x1), which are
exactly three of the six rows `msetdiff.py` reports as MISSING. **Do not open
a spill row as an independent missing-store defect until you have accounted
for the allocation that causes it**; those three rows and the addressing wall
move together or not at all.

## Two x87/byte-lane choices that are NOT source-selectable
*(measured 2026-09-03; both were live "next lever" entries before this)*

**The preload depth of a repeated x87 operand.** For a run of `d[i] = k *
sr[i]`, VC5 emits N copies of `fld k` and then a fixed three-deep
fxch/fmul/fstp pipeline. On 0x1000EAF0 the original picks 8|4 for twelve
statements and the recompile picks 5|7 — with the SAME pipeline shape, merely
offset by the depth. Ruled out as causes: the helper-call boundary (replacing
the two `__inline` row helpers with twelve flat statements emits the identical
five-deep preload), and an x87 stack leak (simulating depth over both streams
shows both enter the block at depth 0). Grouping does not address it; treat the
depth as a scheduler constant.

**Byte lane vs widened `or` when packing a colour.** `(top << 8 | b)` compiles
to `mov dh,al; mov dl,bl` when both operands are live in registers and to
`mov dh,al; or edx,<dword read + and 0xff>` when the byte local is read back
from its slot. It follows liveness only: spelling the term `(b & 0xFFu)` on an
already-`uint8_t` operand is BYTE-IDENTICAL, because VC5 folds the redundant
mask before it chooses the lane. Proven at 0x1000A110 arm 3.

## A diff row whose /Od NEIGHBOURS all match is mis-shaped, not blocked
*(proven 2026-09-03 on 0x1002D72E and 0x1002E79F, both byte-exact after)*

The compile variant is chosen PER FUNCTION, so a single `diff` sitting in a
long run of `match ... Od` rows is not a hard target — it is a function
written in the `/O2` idiom inside a `/Od` translation unit. Screen for it:

    # diff rows whose byte-adjacent MATCHED neighbours are all /Od
    python3 - <<'PY'
    import csv, collections
    rows=list(csv.DictReader(open('build/match/report.csv')))
    f=collections.defaultdict(list)
    for r in rows: f[r['file']].append((int(r['va'],16),r['status'],r['opt'],r['name']))
    for k,l in f.items():
        l.sort(key=lambda t:t[0])
        for i,t in enumerate(l):
            if t[1]!='diff': continue
            nb=[l[j][2] for j in (i-1,i+1) if 0<=j<len(l) and l[j][1]=='match']
            if nb and all(o=='Od' for o in nb) and t[2]!='Od':
                print('%-32s 0x%08X %s'%(t[3],t[0],k))
    PY

As of 2026-09-03 it names four rows, all in slice2_19.c; two fell the same
day. **Three of the remainder are PARKED as walls and should not be — the
park predates this screen.**

### The four /Od source facts these two pinned

1. **A static helper is a REAL CALL at /Od.** If the original has no call
   there, the expression is inline in the source. Its shape then tells you the
   types: `and 0xffff` before each shift is a `uint16_t` read, and an
   arithmetic `sar` for `>> 8` is the promotion to `int`.
2. **Nested tests, not early exits.** `if (p == NULL) continue;` /
   `if (x) return;` compile to a SHORT branch over a `jmp`; the original's
   near `je`/`jne` straight to the loop increment or the epilogue is what
   `if (p != NULL && x == 0) { ... }` produces. Same lever in both the loop
   and the function tail.
2b. **The tell for that lever on a RETURN: a materialised zero.** When the
   guard's early return is written as a fall-through (`if (n == 0) goto
   empty;` or `if (n == 0) return 0;` placed first), VC5 knows eax already
   holds zero — it just tested it — and returns it WITHOUT emitting `xor
   eax,eax`, which makes the function two bytes shorter than the original.
   An original that DOES spell `xor eax,eax` before its `ret` is telling you
   the zero-return is a separate TRAILING block reached by a `je`, i.e. the
   source is a wrapping `if (n != 0) { … return v; } return 0;`. Diagnostic,
   not guesswork: recomp shorter by exactly the missing `xor`, plus a `jne`
   where the original has `je`. Proven 0x10021060 BrGbiEndDList.
3. **Two uses of one pointer variable may be two variables.** The tail of
   0x1002E79F re-derives a word pointer the loop also used; sharing one C
   local cost a frame slot and shifted every displacement in the function.
   Count the DISTINCT `[ebp-N]` slots in the original first — that is the
   local count, and it is not negotiable.
4. **Inline the take-2/emit per block.** Each open-coded
   `p = cursor; cursor += 2;` gets its OWN frame slot and re-reads the global;
   one shared helper collapses them.

**/Od homes locals by an internal NAME hash, not declaration order** — already
recorded in slice2_19.c's BrCarGfxReadColour and re-confirmed here. When the
slot ORDER is wrong and the count is right, rename before restructuring.

## Naming a byte temp: when it helps and when it costs
*(proven 2026-09-03 on 0x1000A110 arm 3; read together with the "do not name a
temp to preserve an observed load order" entry, which is about a different case)*

In a shift/or pack — `(((top << 8 | b0) << 8 | b1) << 8)` — how the TOP
component is spelled decides its encoding:

    inline   `(uint32_t)(uint8_t)SOME_GLOBAL << 8`   ->  mov dh, byte ptr [mem]
    named    `uint8_t topA = SOME_GLOBAL; ... topA`  ->  mov cl, byte ptr [mem]
                                                          ...
                                                         mov dh, cl

The named form also lets the pack's byte loads issue together instead of being
interleaved with the stores between them. Where the original shows the
two-step (`mov al,[mem]` … `mov dh,al`), the source has a named `uint8_t`
local; where it shows the direct load into the lane, it does not. Applying it
to one pack site on 0x1000A110 took the reloc-masked byte diff 4,658 -> 4,539
and recovered an instruction, with the region count and the frame unchanged.

**This does NOT contradict the accumulator entry.** There, naming a temp for
an int value that VC5 wants in the eax accumulator forms COSTS bytes, because
the name pins the value into a general register and loses the short encodings.
The two cases are told apart by what the original does with the value: a byte
that reaches a lane through a register wants a name; an int that flows through
eax does not. Decide per site from the bytes, never by analogy.

**Corollary, learned the expensive way on the same function:** apply it one
site at a time. The identical spelling on a neighbouring pack looked better on
size (4 bytes closer, 2 instructions) and was a regression — the reloc-masked
byte diff rose 177 and the region's FIRST DIVERGENCE moved 27 bytes earlier,
un-merging a cross-jumped tail. Judge a cross-jump region by its
first-divergence address; size and region count both lie there.

## A helper that RETURNS A STRUCT never inlines — spell it out
*(proven 2026-09-03 on 0x100644C0, 0x100643E0 and 0x100642F0, the rigid-body
velocity trio: 137 / 138 / 122 bytes short, all three from one helper)*

MSVC 5.0 will not inline a function returning a struct by value, whatever the
optimisation level. A port that factors a shared body into
`static BrVec3 Helper(...)` therefore emits a `call` the original does not
have, and the whole family reads as `MISSING CODE (40% complete)`. **Check for
this before treating a low completeness score as source discovery: if the
callers differ only in where their input comes from, look for a struct-returning
static.** The fix is to write the body out per caller under
`BR_MATCHING_BUILD` and leave the shared helper for the port arm.

Three float facts fell out of doing it, each worth checking on any x87 target:

- **Product operand ORDER decides `fmul mem` vs `fld` + `fmulp`.** The original
  computes `fmul dword ptr [esi+0xA8]`, so the OTHER operand was already on the
  x87 stack — i.e. the source writes that one FIRST. `angVel.y * r.z` loads
  both and multiplies register-to-register; `r.z * angVel.y` is the original.
  Worth 12 instructions here.
- **`double` intermediates spill as `fstp qword ptr [esp+N]`.** If the original
  never spills a qword, the temporaries are `float`, not `double` — even where
  a comment argues the intermediate "must not round". On x87 the arithmetic is
  80-bit either way until it is stored, so the C type only decides the spill
  width.
- **`*pDst = pSrc->field` copies through a `lea` base pointer** and costs a
  callee-saved register; the original's three loads at their own displacements
  off the object are FIELD-WISE assignment. The extra register shows up as a
  surplus `push edi` in the prologue.

And one frame fact: **two uses of a vector may be ONE stack slot.** 0x100642F0
writes its sums back into the slot the transform wrote, so it has two 12-byte
locals and not three; a separate `sum` variable is a third slot and shifts
every displacement. Count the distinct slots in the original first.

## An accumulated local is not the same expression as a sum

Proven on 0x10054730 (`BrHudLayoutInit`, tail, 13 diffs in one edit).

Two locals hold clamped viewport extents; the epilogue offsets two
just-computed ints by them and stores all four fields. Written as sums,

    i1a98c = (int)f1a9ac;
    i1a990 = (int)f1a9b0;
    i1a994 = dx + i1a98c;      /* WRONG */
    i1a998 = dy + i1a990;

VC5 stores `i1a990` as soon as `_ftol` returns, which kills eax, so both
adds accumulate into the *other* operand and the store order comes out
`i1a990, i1a994, i1a998`:

    mov [esi+0x1A990],eax ; add ebp,edi ; add eax,ebx
    mov [esi+0x1A994],ebp ; mov [esi+0x1A998],eax

Written as accumulations onto the locals,

    dx += i1a98c;
    dy += i1a990;
    i1a994 = dx;
    i1a998 = dy;

the extents stay live as the destinations, so the adds land the original's
way round and the `i1a990` store SINKS past both of them:

    add edi,ebp ; add ebx,eax
    mov [esi+0x1A994],edi ; mov [esi+0x1A990],eax ; mov [esi+0x1A998],ebx

**The tell:** an `add r,r'` whose destination is a value that was live
*before* the statement (a local, a loop bound, a saved extent) rather than
the value the statement just produced. Then the source accumulated into
it. Swapping the operands of the sum does NOT reproduce this — VC5
canonicalises commutative adds, so `a + b` and `b + a` compile identically
(checked on four sites in this function). Only the assignment form moves
it.

Corollary, same function: a `lea r,[base+index]` for a two-register sum
picks its base by allocation, not by source operand order. Neither
spelling, nor a read-modify-write, flips it. That one is T3a — park it.

## `(double)` modelling is a D3D-era artefact — the Glide binary is FLOAT
*(confirmed three times: BrRbBuildMatrix's own note, 0x1006D850
BrRbIntegrateState -55 bytes to -11, and the velocity trio in slice3_42.c)*

Several float-heavy functions are written with every operand cast to `double`
and a spill map arguing the intermediates "must not round". That model was
read off **BRD3D.dll**, and it is wrong for the reference binary. On x87 the
arithmetic is 80-bit either way until it is stored, so the C type only decides
the SPILL WIDTH — and the tell is unambiguous:

**If the original never emits `fstp qword ptr [esp+N]`, the temporaries are
`float`.** A `double` model spills eight bytes at a time and cannot match.

Screen for the class — diff-bearing files with heavy `(double)` modelling:

    for f in $(awk -F, 'NR>1&&$4=="diff"{print $1}' build/match/report.csv | sort -u); do
      n=$(grep -c '(double)' "$f" 2>/dev/null); [ "$n" -ge 6 ] && echo "$n $f"; done | sort -rn

As of 2026-09-03 that names twelve files, slice2_15.c (63 casts, 7 diff rows)
and slice2_17.c (43, 8) worst.

### Two boundary conditions, both learned the hard way

- **A `(float)` cast on an already-float expression is a NO-OP** and will not
  produce the original's store-and-reload. Only a NAMED float local does, and
  only when register pressure actually forces the spill — which means the
  products have to be computed BEFORE the adds that consume them. One
  statement at a time, each product is consumed immediately and nothing
  spills at all.
- **The `fld a; fmul [b]` operand-order lever has a limit.** It works when one
  operand is a LOCAL: writing the local first is worth 12 instructions on the
  velocity trio. It does NOT work when both operands are memory — on
  0x1006D850 `pSrc->vel.x * dt` and `dt * pSrc->vel.x` compile to
  byte-identical output, so VC5 canonicalises that case and the order is not
  source-reachable. Check which side is a local before spending probes.

## `x <<= k` fuses with a preceding right shift; `x = x * (1<<k)` does not

Proven on 0x100239C0 (`BrGbiMoveWord`, 187 B, byte-exact — it was the whole
residue, two bytes in each of two arms).

    slot = off >> 5;
    slot <<= 4;                 /* WRONG: shr eax,1 ; and eax,0x7FFFFFF0 */
    slot = slot * 16;           /* RIGHT: shr eax,5 ; shl eax,4          */

VC5's peephole rewrites an unsigned `>> a` followed by `<< b` (a > b) into
one `shr a-b` plus a mask, because both are shifts on the same value. Write
the scale as a MULTIPLY and the peephole does not fire — the two operations
stay as the original emits them. The values are identical, so no test tells
them apart; only the bytes do.

**The tell:** the original has an adjacent `shr r,a` / `shl r,b` pair
(`C1 E8 aa C1 E0 bb`) where ours has `shr r,a-b` and an `and r,imm32`.
Whenever a shifted index is then scaled to an element size, try the
multiply spelling first — a scale to an element size is what the source
almost always said.

**Not the lever:** indexing a 16-byte element type (`arr[slot].b[0]`) is
much worse — it hoists the scale out of the if/else arms and rewrites the
prologue (75 -> 101 diffs). Nor do `(off >> 5) << 4` as one expression, or
`off / 32u` for the shift, change anything.

## A frame that is 4 bytes short: look for a scalar that should be an ARRAY
*(proven 2026-09-03 on 0x1000A110, which had carried a 0x48-vs-0x4c frame gap
for eight sessions)*

VC5 never enregisters an array, and it never tucks one into a dead argument
slot either — an array always gets its own slot in the LOCALS area. Two
`uint8_t` scalars, by contrast, are enregistered where possible and otherwise
packed into whatever reused argument slots are free, costing the frame
nothing. So a recompile whose frame is a few bytes SMALLER than the original's
while writing the same values is the signature of a source that spells as
scalars what the original spells as an array.

The check costs one compile: `/FAcs` prints an equate table, and
`_pack0$ = 8, _pack1$ = 12` says plainly that those two live in the argument
slots and spend no locals-area dword. Changing the declaration to
`uint8_t pack[2]` (a pure rename at every use) took `sub esp,0x48` to
`sub esp,0x4c` and made the prologue byte-exact.

**Do not judge this change by size or by fn.py's RAW/REGNORM.** Every slot
displacement in the function moves when the frame does, so both read worse for
a while; the masked region count and `msetdiff.py` are the honest scores.
The frame is worth closing first anyway — nothing downstream of it can line up
until it does.

## A named local that caches a struct field is wrong if the original re-reads it
*(proven 2026-09-03 on 0x1000A110's texture-window command words)*

Writing

    uint32_t s = p->s0, t = p->t0;
    uint32_t w0 = f(s, t), w1 = g(s, t);
    put(w0, w1);

makes VC5 keep `s`, `t` and the finished `w0` alive across the store sequence
and spill all three to slots (`mov [esp+0x38],ecx; mov ecx,[esp+0x60];
mov edx,[esp+0x38]`). The original instead re-reads both fields for the second
word (`mov edx,[eax]; mov eax,[eax+4]`), which is what the source looks like
when the field accesses are written inline at each use. Inlining them made the
block instruction-for-instruction identical and removed its 15-byte drift.

Read the original's loads before deciding: **a repeated field load in the
original is evidence of source that does not cache, not of a missed CSE.**

**And watch which number you rank by.** Removing that spill took the function
from 38 bytes short to 53 short — the three wrong instructions had been
padding a real deficit elsewhere — while the register-blind multiset went
64+75 to 52+66. Size alone would have called a correct fix a regression.

## A `static void (void)` helper with TWO call sites is not inlined either
*(proven 2026-09-03 on 0x10037B20 BrSub1003E510, 137 bytes short -> byte-exact)*

The struct-returning rule above is the hard case, but the soft one costs just
as much: MSVC 5.0 also declines to inline a plain `static void f(void)` once it
has more than one caller. Two selection sweeps factored out that way left the
function at 47 instructions against the original's 99. Spell the body out at
the call site that needs it under `BR_MATCHING_BUILD` and leave the helper for
its other caller — the same split the ops-table recipe uses.

**Rule of thumb for the whole MISSING-CODE class: before hunting for source
the port never had, grep the file for a `static` the original does not call.**

### Two indexing facts from the same function

- **A loop that probes a global keeps probing the GLOBAL, not the saved
  start.** `start = g_x; if (Probe(g_x) == 0)` compiles to one load into eax, a
  push of eax, and `mov esi,eax` for the start — the copy IS the local.
  Writing `Probe(start)` loses that copy and the whole prologue shifts.
  Happened twice in one function.
- **A doubled table index needs its OWN local.** Written inline as
  `tbl[i*2]` and `tbl[i*2+1]`, VC5 folds the ×2 into the SIB scale
  (`[eax*2 + base]`); the original materialises it once with `shl eax,1` and
  then indexes `[eax+base]` and `[eax+base+1]`. Same family as the
  `x <<= k` entry above: **when the original has an explicit shift and you
  have a scale factor, give the shifted value a name.**

## Conditional argument: put the whole CALL in each arm
*(third sighting, settled on 0x10050AC0)*

A caption whose catalogue id depends on a flag:

    if (g != 0)  push 0x66
    else         push 0x1E
    call BrStrGet          <- one call, the arms merge on it

Writing it as a shared variable (`id = g ? 0x66 : 0x1E; s34(BrStrGet(id)…)`)
does NOT give that. VC5 turns the select branchless -- `neg eax / sbb eax,eax`
and an and/add -- because the only difference between the arms is a value.
Duplicate the whole call in both arms instead and VC5 tail-merges it,
leaving each arm its own `push imm`.

This is the same lever as the `lea` in 0x10055C50 and the catalogue call in
0x10037E60, now seen a third time and on a two-way value: **whatever the
arms have in common, let the tail-merge factor it -- never factor it
yourself into a shared local.** Arm order still decides layout: the arm the
original places as the fall-through is the `then`.

## A 16-bit destination makes VC5 factor a shared shift — write the source factored

Proven on 0x100014A0 (`BrSurfSetColourKey`, 46 -> 21 diffs).

A 565 pack whose terms are written in their finished positions,

    key = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);      /* WRONG */

compiles to one `shl 8` for red and a pre-shifted green mask
(`shr 5; and 0x7E0`). The original instead emits `shl ecx,5 … shl ecx,3` —
the two terms are combined FIRST and the common `<< 3` applied to the pair.
That only happens when the source itself is factored, and only when the
destination is 16 bits wide (a `uint32_t` accumulator loses it again):

    key = (uint16_t)((((c & 0xFFFF) >> 3) << 8 | ((c >> 8) & 0xFC)) << 3
                     | ((c >> 19) & 0x1F));

Two sub-tells in the same function:

- **A mask wider than the field is information, not noise.** The red mask is
  `0xFFFF`, not `0xFF`: the green bits it drags in land at bit 16 and up and
  the store is 16 bits, so they never appear — but only the wide mask emits
  `and ecx,0xFFF8`. Do not "correct" a mask that looks too wide.
- **A byte-lane shift can cost more than it buys.** The original narrows the
  blue term to AL (`shr eax,0x10; shr al,3; and eax,0x1F`). Every char-typed
  spelling reproduces the `shr al,3` and then pays a `movzx ax,al` widening
  inside the 16-bit expression — 33 diffs against 21 for the plain
  `(c >> 19) & 0x1F`. Take the shorter residue.

## Screen the whole tree for frame-size mismatches
*(`tools/framescreen.py`, added 2026-09-03 after the array fix above)*

The array/scalar finding is a CLASS, not a one-off, so it now has a screen.
`framescreen.py` reads the `sub esp, imm` out of every tagged-diff function's
original bytes and out of its recompiled object, and ranks the disagreements:

    ours SMALLER than the original  -> a scalar that should be an array, or a
                                       local the original keeps live
    ours LARGER                     -> a local the original does not spend, or
                                       one we force to memory

26 of the 69 diff rows with a readable prologue disagree. Take these before
grinding regions in the same function: every stack displacement moves with the
frame, so nothing downstream can line up until it matches.

**The screen must use each row's own compile variant** (`report.csv`'s `opt`
column). Scoring an `/Od` or `/O2 /Oy-` row against the `/O2` object compares
two different compiles and invents a gap — 0x1002ECEB read as 76 bytes off
that way, and it is not a frame defect at all.

## A suspect resync poisons the regions AFTER it, not just its own line
*(`divergence.py`, 2026-09-03 — this trap cost most of a session)*

`change` is a difference of two deltas, so once a resync locks onto the wrong
copy of a repeated arm, the NEXT region's delta is wrong and the two `change`
values computed from it are fiction. Both are labelled now. A session read a
"-73 byte block" out of exactly that position, wrote it into a dossier as the
function's dominant defect, and it did not exist.

**Verify any large change before grinding it.** Pick an instruction that occurs
once per unit of work and count it across the WHOLE function in both streams.
On 0x100250D0 the divide-by-255 magic constant appears 33 times in each and
the one-operand `imul` 24 times in each — every channel is present, so a
21-instruction "gap" in one window cannot be real. An equal census plus an
honest instruction total (from `fn.py`, which counts the whole function and
not the aligner's windows) means the residue is allocation, wherever the
region map points.

## The FACTORED-HELPER screen: 32 rows, 13,463 bytes short
*(the class behind 0x10037B20, 0x10014800, the velocity trio and
BrCarGfxSetColour — four separate sessions arrived at it independently)*

MSVC 5.0 declines to inline a `static` once it has more than one caller, and
will never inline one that returns a struct by value. Every port helper that
factors a shared body — or wraps a group of globals behind an accessor —
therefore emits a `call` the original does not have, and the function reads as
`MISSING CODE`. **This is the single largest source-level class left in the C
lane.** Ranked list, EH rows excluded:

    python3 - <<'PY'
    import csv,re,os
    def eh(va):
        # An EH prologue does NOT always lead with `push -1`: VC5 often emits
        # `mov eax,fs:[0]` first, so testing only byte 0 for 6A FF lets those
        # through. Look for both markers anywhere in the first 12 bytes.
        try: h = open('build/match/orig/%s.bin'%va,'rb').read(12)
        except IOError: return False
        return b'\x6a\xff' in h and b'\x64\xa1\x00\x00\x00\x00' in h
    for r in csv.DictReader(open('build/match/report.csv')):
        if r['status']!='diff' or not os.path.exists(r['file']) or eh(r['va']): continue
        o=int(r['orig_size']); c=int(r['recomp_size'] or 0)
        if o<150 or c==0 or c/o>0.85: continue
        # Do NOT hand-match in C what the C++ lane already owns: a row with a
        # src/core/cpp/<VA>.cpp is a solved-twice duplicate (see
        # tools/cpp_twin_retire.py), and its C tag is dead weight on this
        # screen. 4 rows / 1,782 B were exactly this on 2026-09-03.
        if os.path.exists('src/core/cpp/%s.cpp'%r['va']): continue
        if re.search(r'^static\s', open(r['file']).read(), re.M):
            print('%5d short  %-30s %s  %s'%(o-c,r['name'],r['va'],r['file']))
    PY

**Two screens to run before taking a row off this list**, both learned the
hard way: the EH test above (a `64 A1 ... 6A FF` prologue is C++, not C), and
the C++-duplicate test in the snippet. And check the row's TWIN: 0x1003A2B0
has no `.cpp` of its own but is instruction-for-instruction the same function
as 0x1003A140, which does — matching one in C while the other goes to C++ is
work done twice and the C tag gets retired anyway.

As of 2026-09-03, with the corrected EH test: **27 rows, 9,085 bytes short.**
Worst are BrRaceGateStep (-1962), BrTextEmitString (-1162) and
br_dl_light_setup (-587). The earlier "32 rows / 13,463 bytes" used the
byte-0-only EH test above and therefore counted C++ EH rows it claimed to
exclude — 0x10046E70 (-962) and 0x1004ABE0 (-280) are both `64 A1 ... 6A FF`
frames, unreachable from C. Two more rows have since been matched.

**The recipe is always the same and it does not touch the port arm:** spell the
helper's body out at the call site under `#ifdef BR_MATCHING_BUILD`, leave the
`static` in place for its other callers, and put the port's version in the
`#else`. Two of these landed byte-exact the same day.

### Scoping the inline-out: macros, and `#undef` after the function

Two mechanics make the recipe safe on a file where the helper has many users:

- **A macro, never a new `static`.** Replacing one factored `static` with
  another is still a call. Write the body as a `do { ... } while (0)` macro
  with its temp declared INSIDE the block, so each expansion gets its own
  (a single function-scope temp shared by every expansion is what earns a
  stack slot). Value-returning helpers become plain expression macros.
- **`#define` immediately before the function and `#undef` immediately
  after.** The helper's other callers then keep whatever shape they already
  match with, and — the trap — the macro must be defined AFTER the `static`
  definitions, or the definition line itself gets macro-expanded into
  garbage. Scoped this way the change cannot regress a sibling; verify with
  the one-file sweep, which reports every function in the TU.

- **While you are there, check the CALL FORM.** `call dword ptr [mem]` in the
  recompile against `call rel32` in the original means a port hook declared as
  a function POINTER that the original calls directly; re-declare it as an
  ordinary function in the matching arm. Count both forms in the original
  first — it is a one-line census and it is unambiguous.

Proven together on 0x100302A0 BrModelSwap: five byte-swap helpers inlined as
scoped macros and two hooks made direct took the register-blind gap from
149+285 to 60+137 and the size from 352 short to 240 short, with no sibling in
the 49-function TU disturbed.

### A 2-byte reversal is a halfword COMPOSE, not two byte stores

An in-place byte swap written the obvious way —
`t = p[0]; p[0] = p[1]; p[1] = t;` — emits two byte stores, and MSVC5 will
NOT merge them into a 16-bit store however you arrange the temps (both the
single-temp and the load-both-bytes-first forms were measured byte-identical).
The original loads the two bytes straight into the low and high halves of one
register and writes the pair once, so the source composes the halfword:

    #define BrRev2(pv) (*(uint16_t *)(void *)(pv) = (uint16_t)( \
        ((uint16_t)((unsigned char *)(pv))[0] << 8) | (uint16_t)((unsigned char *)(pv))[1] ))

VC5 turns the `<< 8` / `|` into `mov cl,[p+1]; mov ch,[p]` with no shift at
all. **Tell: count `mov word ptr` stores in the original — one per swap site
means the compose form, and byte stores in your recompile against word stores
in the original is this and nothing else.** Worth 22 register-blind shapes on
0x100302A0 (35+36 -> 13+23), where the original has exactly seven such stores.

### The same disease at BYTE and HALFWORD granularity

The accessor sub-case below is usually spotted as a struct pointer, but the
identical mistake hides inside a single global. Three forms, all proven on
0x10058900 in one sitting (register-blind gap 5+4 -> 0+0, 71/71 instructions):

- **Two adjacent bytes read as `arr[0]` / `arr[1]` are TWO SEPARATE GLOBALS.**
  Spelled as one array VC5 merges them into a single dword load and takes the
  second byte out of `ah`; the original emits two independent
  `mov al,[g]` / `mov dl,[g+1]`. Tell: a MISSING `mov B, byte ptr [I]` pair
  against an EXTRA `mov R, dword ptr [I]` plus a `mov B,B`.
- **The high half of a dword is its own 16-bit global**, read
  `mov dx, word ptr [addr+2]` after an `xor edx,edx`. A `(x >> 16) & 0xFFFF`
  costs a `shr` and a register copy and never produces the `xor`. Tell: a
  MISSING `mov W, word ptr [I]`.
- **A base address must be INSIDE the address expression** so VC5 folds it
  into the lea displacement (`lea eax,[eax*8+0x10ac5a66]`). Computing an int
  offset and adding the array afterwards costs a second lea. Tell: an EXTRA
  `lea R,[R]` with the original having one lea where you have two.

The general rule these share: **when the original reads something at a
narrower width or a different address than your C does, the source declared it
that way — do not reach for a shift or a cast to bridge the difference.**

### The accessor sub-case: a struct that is really N standalone globals

`BrScreenGet()->cx` / `BrHudGetEnv()->pszSplitPrefix` group four or five
SEPARATE absolute globals behind one struct pointer. The original loads each
absolutely (`mov eax, dword ptr [0x100A7514]`), so the port form costs a base
register AND a call. slice5_61.c, slice6_70.c and now slice5_63.c all carry the
same correction; the giveaway in the diff is **`mov R,[R+I]` where the original
has `mov R,[I]`**, several times over.

Two more facts from the same function, both cheap to check:

- **An array whose ADDRESS is pushed is an array, not a pointer.**
  `push 0x100A6B80` is `char buf[]`; a `char *` global would be
  `mov eax,[0x100A6B80]; push eax`.
- **A float argument moved with `mov`/`push` rather than `fld`/`fstp` is a
  DWORD PUN.** Declare the parameter `uint32_t` in the matching arm — the same
  four bytes reach the callee.

## A dead verdict measured against a wrong frame is STALE
*(proven 2026-09-03 on 0x1000A110, which closed a five-times-dead region the
session after its frame was fixed)*

Every "measured, do not re-run" note that turns on REGISTER ALLOCATION is
conditional on the allocation it was measured under. Change a global input to
that allocation — the frame size above all, but also a spill removed or a
register freed — and the whole dead list has to be re-tested.

The worked case: a three-float copy where the original puts two members
through the x87 and one through an integer register. Five spellings had been
measured byte-identical and the region was written off as T3a, with the note
"treat this as T3a until the frame is solved". The frame was solved one
session later, and the sixth spelling landed instruction-for-instruction.

Two rules made that copy work, and both generalise:
- **Both floats need a named temp**, so their live ranges overlap and VC5
  keeps them on the x87 stack. One temp gets you one `fld`/`fstp` and an
  integer move for the other.
- **The integer member must be stored FIRST.** A temp whose load and store
  are adjacent is copy-propagated into an integer move; putting the integer
  store between the load and the store of its float neighbour is what keeps
  the neighbour on the stack.

Re-testing the rest of that function's dead list under the same rule found one
more verdict had shifted — from clearly-bad to ambiguous — while two others
held. **Expect a minority to flip: re-test them all, and record which held so
the next session does not repeat the sweep.**

## Read and update the GLOBAL; a local copy of it changes the allocation

Proven three times in one pass, all byte-exact, all in `src/core/slice2_16.c`:
0x1001FD40 `BrGbiClearGeometryMode` (8 diffs), 0x100211E0
`BrGbiSetGeometryMode` (8), 0x10021020 `BrGbiDList` (28).

A decompiled draft naturally says "load it, work on it, store it back":

    cur = g_mode;            /* WRONG */
    g_prev = cur;
    cur &= ~pCmd->w1;
    g_mode = cur;

    n = g_count; n++;        /* WRONG */
    if (n == 10) exit(1);
    n = g_count;
    g_stack[n] = p; n++; g_count = n;

The original says it in place:

    g_prev = g_mode;
    g_mode &= ~(int)pCmd->w1;

    if (g_count + 1 == 10) exit(1);
    g_stack[g_count] = p;
    g_count = g_count + 1;

Two distinct costs, both from the same cause — the local gives the loaded
value a lifetime the original never gave it:

- **The accumulator flips.** VC5 accumulates into whichever operand dies
  first. With a local copy that is the mask, so `and ecx,eax` and an extra
  `mov ecx,[esi+4]`; updating the global in place pins it to the global's
  own value, `and eax,ecx`, and folds the command word into the `or`'s
  memory operand.
- **An `inc` becomes a `lea`.** The local keeps the pre-increment value
  live across a guard, so the guard's `+1` needs its own register
  (`lea ecx,[eax+1]`) instead of destroying the load (`inc eax`). The
  original reloads the global after the call — which the call forces
  anyway, so the copy buys nothing.

**Screen:** `tools/screen_globalcache.py` lists diff rows whose matching
body assigns a global into a local and later stores a local back into that
same global. Spelling the reads and the update in place is a one-line edit
per site.

## Measure ONE change at a time, or the verdict lands on the wrong construct
*(re-proven 2026-09-03 on 0x1000EAF0; the same trap as the bundled probes
below, and it has now cost two dossiers a wrong entry)*

A probe that changes three things and scores worse tells you the BUNDLE is
worse. It does not tell you which of the three, and writing the bundle's cost
against one of them buries a construct that is actually fine.

The case: a dossier entry blamed `slot = head - 1; if (slot < 0)` for a
`dec`/`jns` against `lea`/`test`/`jge` difference, on the strength of a probe
that had also reversed a guard and sunk an assignment. Re-measured alone, the
two spellings are BYTE-IDENTICAL — VC5 canonicalises them — so the region cost
belonged entirely to the other two changes, and a correct construct had been
carrying the blame for four passes.

**When a bundled probe scores badly, re-run its members singly before writing
any of them into a dead list** — or write the entry against the bundle, named
as a bundle, so the next session knows what was actually measured.

## Read a register wall as a "which N of M fit" question
*(0x1000EAF0's join, 2026-09-03)*

When the original and the recompile both spill, but different things, stop
looking for a spelling and count the registers. At one join the original keeps
two loop values live across the merge and therefore has to read the loop
counter from memory at the guard (`cmp [slot],reg`); we keep the counter in a
register and reload the two values after the merge. Three values, two
registers — both builds pick two, and they pick differently.

That framing is worth reaching for early, because it says immediately that no
source spelling reaches it: the source names all three the same way in both.
What it does NOT rule out is a change that alters the pressure, which is why
these verdicts go stale when the frame moves (see the staleness entry above).

## Inlining a helper by hand: use a MACRO, and pun the float stores
*(proven 2026-09-03 on the two triangle handlers, 0x1001ECF0 and 0x1001FA30 —
337 and 627 bytes short, down to 49 and 56)*

When the factored-helper screen sends you to spell a body out, two mechanics
decide whether it lands:

- **A macro, not another `static`.** A `static` you introduce is a call again.
  Take the loop/temp variable as a macro PARAMETER rather than declaring it
  inside the macro's own `do { }`: one function-scope local shared by all the
  expansions is what gets a stack slot, and a fresh one per expansion stays in
  a register. The original here uses ONE slot at `[esp+0x10]` for all six
  products across three vertices.
- **Pun the float stores.** The original writes each product once and copies
  it to both destinations with integer movs
  (`fstp dword [esp+0x10]; mov edi,[esp+0x10]; mov [v+0x30],edi;
  mov [v+0x24],edi`). Plain float assignments make VC5 emit `fst`/`fstp`
  straight to the two fields and the temp never gets a slot at all. Spelled

      #define PUN(dst, src) (*(uint32_t*)(void*)&(dst) = \
                             *(const uint32_t*)(const void*)&(src))

  this was worth **25 register-blind shapes** on one function. Same rule for a
  plain float-to-float field copy: `mov edi,[v+0x20]` then two integer stores
  is a pun, not two `fld`/`fstp` pairs.

### What the port adds that the original never had

The same four every time, and all four are visible as EXTRA instructions:
counters (`pS->cTriIn++`), a bound check on an index the original indexes raw,
a null test before a sink call the original makes directly, and a state
pointer parameter for globals that are absolute. Strip all four in the
matching arm.

### Residue to expect afterwards, and not to grind

The original tends to keep the **scaled byte offset** in a register and
re-form `base + offset` at each access (`lea eax,[ecx + 0x105CE318]`), where a
pointer local keeps the pointer. Writing the body in INDEX form
(`pool[i].field`, no pointer locals) gets the instruction count right but
costs ~90 bytes of SIB addressing — measured, worse overall. Park it.

## Two more shapes of "MISSING CODE" that are not source discovery
*(both found 2026-09-03 while working the factored-helper screen)*

### 1. The port's loop over a static table is UNROLLED in the original
*(0x10034010 BrSpanBuildHull: 327 bytes short -> 7, regnorm 59+145 -> 12+9)*

    static const unsigned char aEdge[12][2] = { {0,1},{0,2}, ... };
    for (i = 0; i < 12; i++) AddLine(pVol, pt[aEdge[i][0]], ...);

compiles to a loop; the original has **twelve separate `call` sites** with the
operands as absolute globals. A table-driven loop in the port against a much
larger original is the tell — and it usually travels with the accessor
sub-case, because once the loop is gone the parameters go too. This function
takes NO arguments at all in the original.

Also worth reproducing there: the two inner scans are UNBOUNDED
(`inc edx; jmp` / `dec ecx; jmp`), so an uncovered column walks off both ends
of a 64-entry array. That is the original's behaviour, not a port bug to fix.

### 2. The port DELETED the original's debug tracing
*(0x1005FF00 BrRaceGateStep is -1962 bytes; 0x10067710 BrCrRespWalk is -549)*

Screen the ORIGINAL for calls to the trace sink — a printf-style function
taking a format-string pointer and varargs:

    # short diff rows whose ORIGINAL calls 0x10008D60
    python3 - <<'PY'
    import csv,os,re
    for r in csv.DictReader(open('build/match/report.csv')):
        if r['status']!='diff': continue
        o=int(r['orig_size'] or 0); c=int(r['recomp_size'] or 0)
        if o<200 or c==0 or c/o>0.85: continue
        p='build/match/orig/%s.bin'%r['va']
        if not os.path.exists(p): continue
        b=open(p,'rb').read(); va=int(r['va'],16); n=0
        for m in re.finditer(b'\xe8', b):
            i=m.start()
            if i+5<=len(b) and va+i+5+int.from_bytes(b[i+1:i+5],'little',signed=True)==0x10008D60:
                n+=1
        if n: print('%2d trace calls  -%5d  %-28s %s'%(n,o-c,r['name'],r['va']))
    PY

Two rows, 2,511 bytes between them. BrRaceGateStep alone has **12 trace calls,
10 `BrStrGet` lookups and 4 `sprintf`s**, and the port's own comments quote the
format strings while explaining that only the branch was kept ("the original
builds that predicate into a register purely to print it"). So the missing
bytes are not a helper to find — they are output that was deliberately
dropped, and putting them back is transcription, not discovery.

## The "photo" control block (menu-builder family) — SOLVED 2026-09-03

Proven byte-exact on 0x1004ABE0 (760 B, first compile). Three parts, and
each part's shape is source, not schedule:

1. **A pair of parallel arrays walked by ONE index, in two ranges.** The
   original opens each loop with two `lea`s of the *starting element* and a
   `dec`-counted trip count, which reads like a hand-written pointer walk.
   It is not: the source is an ordinary indexed `for`, and the two loops are
   contiguous (`0..14` then `15..23`) because only the stored short changes
   (0x50 -> 0x51). VC5 strength-reduces both induction variables and folds
   `&arr[15]` into the second loop's `lea`. Write the indexed form.

2. **A rect built from `__ftol` conversions, stored +0x54, +0x50, +0x58,
   +0x5C.** The y field is written FIRST. That is the source order, not a
   schedule — transcribing it as 50/54/58/5C rotates the whole block. Where
   the same float feeds two fields, the original converts it TWICE (four
   `__ftol` calls, no int temps) unless the integer is also needed by a
   later control, in which case it is a real local (0x1004AEE0 keeps `xi`
   in `ebx` across three pages).

3. `cmp r,ebx` versus `test r,r` on the new-object null-checks inside one
   function is the pinned zero register DYING, not two source shapes. In
   0x1004ABE0 `ebx` holds zero for the first two entries and is clobbered by
   the +0x34 vcall's vtable load before the third. Do not split the source.

STILL PARKED: 0x1004AEE0's photo1 tail (34 diffs). With the above applied
the ONLY divergence in 3862 bytes is a ten-instruction window where the
original computes both derived ints (`lea`, `add`) before its three stores
and sinks the `fstp` past `f2968`, while ours interleaves and sinks the
`+0x58` store instead. Identical multiset, T3a. Photos 2 and 3, and every
other byte, are exact.

## An x87 preload depth is not a constant — it is set by the block before it
*(0x1000EAF0, 2026-09-03; this RETRACTS an earlier entry of mine that called
the same number a scheduler constant)*

For a run of `d[i] = k * s[i]`, VC5 preloads N copies of `k` and then runs a
fixed three-deep fxch/fmul/fstp pipeline. When N does not match the original,
do NOT conclude the compiler is unwilling. Three checks, in this order:

1. **Look for another site in the same function.** If any site emits the
   original's depth, the compiler is willing and the difference is contextual.
   (Here a second scale block already emitted the original's 8.)
2. **Vary the statement count.** If the first batch is the same number for
   every N — measured 10 through 16, always 5 — it is not a batch-size rule,
   and something at that site is costing x87 slots.
3. **Move the block earlier.** If the depth changes when the block is hoisted
   above its predecessor, the predecessor sets it.

Here the predecessor was a row block whose four products mix operand KINDS:
two spelled as absolute literals, two through pointer locals. Spelling all
four absolutely — which is what the original does — takes the preload from 5
to the original's 8. **So two "independent walls" in that dossier were one:
the preload is downstream of the row block's operand kinds.**

Ruled out as causes of the depth, so nobody re-runs them: the inline-helper
boundary, whether source and destination are the same array (in-place still
gives 5), extern-vs-defined storage for the arrays, and removing the locals
that go dead when the terms are rewritten.

**The general lesson is the diagnosis order, not the fix.** Before writing
"the compiler will not do X", find a site in the same binary where it does.

## `jb` vs `jl` on a loop bound: the cursor is an int, not a pointer

Proven on 0x10045EF0 (selector fill loop), where it was the function's ONLY
diff. VC5 emits an UNSIGNED `jb` for a pointer/pointer comparison and a
SIGNED `jl` when the operands are ints. So a `jl` closing a table walk means
the source compared the cursor as an integer -- `(int)pe < (int)(tab + N)`,
or an int cursor stepped by the element size -- even though every use of the
cursor inside the loop is a pointer dereference. One byte, and it is
readable straight off the branch opcode: 0x72 unsigned, 0x7C signed.

## A repeated byte immediate gets pooled; write the mask at the field's real width

Proven on 0x1002F380 (`BrPadTranslate`, 690 B, byte-exact — it was the
whole residue, and the previous note there had already recorded four dead
spellings).

Two probes of the same bit in two bytes of one block:

    if (!(pBytes[1] & 0x80) && !(pBytes[7] & 0x80))     /* WRONG */
        -> mov cl,0x80 ; test byte [eax+1],cl ; test byte [eax+7],cl

    if (!(*(unsigned short *)pBytes       & 0x8000)     /* RIGHT */
        && !(*(unsigned short *)(pBytes+6) & 0x8000))
        -> test byte [eax+1],0x80 ; test byte [eax+7],0x80

Both forms test the same bits. The difference is WHEN the immediate 0x80
exists. Written as a byte mask it is one constant in the source, and VC5's
constant pooling hoists it into a register the moment it is used twice.
Written as the halfword mask the field really is, each `test` is narrowed
to `test byte …,0x80` late, per instruction, and there is no shared
constant to pool.

**The tell:** `mov r8,imm` immediately before two or more `test`/`and`
byte operations that the original writes with the immediate inline. Widen
the source mask to the natural width of the member being tested; do not
try to break the pooling with `&&` vs nested `if`, the `!` form, split
statements, or duplicated arms — all four were tried on this function and
all four pool.

## A plain `static` helper is NOT auto-inlined under /O2 — write it out

Proven on 0x10003320 (`BrChkFReadOpen`, 260 B: byte-exact the moment the
one-line helper was spelled out; it was the last two instructions).

    static FILE **ChkToPun(BrChkFile *pf) { return (FILE **)(void *)pf; }
    ...
    return ChkToPun(pf);        /* WRONG: push ebx / call / add esp,4 */
    return (FILE **)(void *)pf; /* RIGHT: mov eax,ebx                 */

/O2 implies /Ob1, which inlines only functions marked `inline` or
`__inline`. A plain `static` one-liner — the shape a decomp naturally
reaches for when the same pun, accessor or index calculation appears in
several functions — is emitted as a real call, and the original has no call
there at all.

**The tell:** recomp is a few bytes longer than orig with one extra
`push`/`call`/`add esp,N` group around a value the original just moves, and
the call target is a tiny local function. Either write the body out at the
matching call site, or mark the helper `__inline` — but check every other
caller when you do, because that changes their code too.

**Not the same thing** as the CRT intrinsics: `strcpy`, `strcat`, `strlen`
and `memcpy` ARE expanded inline under /O2 (that is /Oi, which /O2 implies),
which is why the original shows `repne scasb` / `rep movs` for those and a
real `call` for `strncpy` or `_stricmp`, which are not intrinsics.

## ‼ fn.py's DIFFS is POSITIONAL — a size shift upstream inflates or halves it
*(measured 2026-09-03 on 0x1000EAF0; this qualifies several earlier
comparisons in these dossiers, mine included)*

`fn.py`'s `DIFFS` compares byte *i* of the original against byte *i* of the
recompile, with relocation slots masked and NO alignment. So any size
difference before a stretch shifts that whole stretch and makes almost every
byte in it mismatch — and removing the shift makes almost all of them "match"
at once, whether or not the code got closer.

The case: a row-expression variant took DIFFS from 4,669 to 2,490 — a 47%
drop that looks like a breakthrough. It was two bytes. The 2-byte size change
moved a ~4,600-byte tail from delta −2 to delta 0, and ~2,200 spuriously
mismatching bytes became spuriously matching ones. The variant recovered two
bytes and one instruction, nothing more.

**Rules:**
- Compare DIFFS only between builds whose total size is the SAME. Otherwise it
  measures alignment, not correctness.
- When the size moves, rank by `msetdiff.py` rows, the instruction gap, and
  the masked region map — all three are alignment-free.
- A DIFFS swing much larger than the byte-count change is the signature of a
  shift, not of a fix. Check `divergence.py --deltas` for a long run of
  regions whose delta all moved by the same small amount.

## A redundant outer parenthesis is NOT a no-op in a float expression
*(isolated A/B on 0x1000EAF0, 2026-09-03)*

Wrapping four already-parenthesised float expressions in one more pair of
outer parentheses — changing nothing else, not even the association — moves
the x87 schedule: the scale block's preload goes 5|7 → 4|8, the recompile
grows three bytes, and the register-blind gap goes 40+46 → 41+47.

So parentheses reach VC5's scheduler, not just its parser. Two consequences:
- **Never "tidy" redundant parentheses in a matching TU.** They are load-
  bearing, and the diff will move under you.
- **They are a probe axis of their own.** When an x87 schedule is one notch
  off and no association helps, try the same association with and without an
  outer pair before calling it a wall.

Also settled on that function while measuring this: the ORIGINAL's four-term
row is LEFT-associated — its first `faddp st(2)` adds terms 1 and 2 — so a
right-associated variant is the wrong source no matter how it scores.

**Second sighting, and this one closed a function** *(0x100199A0
`BrRaceCarCtlOutro`, same day)*. A three-term sum of squares
`x*x + y*y + z*z` scheduled the THIRD square second; the same expression
written `(x*x + y*y) + z*z` — identical association, one redundant pair —
scheduled it last and took the function from 22 diffs to 2, then to
byte-exact once the two float locals were declared in the order that puts
the right one in ecx. Both sightings share a signature worth screening for:
**size and instruction multiset already exact (register-blind gap 0), the
divergence purely in x87 ordering.** On a float function in that state, try
the parenthesis axis before anything else — it is one recompile, and every
other axis in this class costs many.

## A raw-address cast and a symbol reference are NOT the same operand to VC5
*(0x1000EAF0, 2026-09-03 — this closed a wall that had stood nine passes)*

`*(float *)(0x106e9a38 + 4*k)` and `DAT_106e9a38[k]` name the same location and
assemble to the same instruction, but they reach the code generator as
different things: the cast is a **compile-time constant**, the symbol is a
**relocation**. VC5 schedules around them differently, so mixing the two
spellings inside one expression produces a schedule that neither pure form
gives.

The case: a four-term matrix row transcribed with two terms as raw-address
casts and two through pointer locals. That mix held an x87 preload at 5 for
nine passes. Written with all four as array symbols — which is what the
original's source must have had, since real source names its variables — the
following block emits the original's batching exactly.

**So treat a raw-address cast as a transcription placeholder, not a finding.**
When a decompiled expression mixes hex-address casts with named references,
make it uniform before concluding anything about the schedule. Measured on
that row: only all-symbol reproduces the original; every mixed mask lands
somewhere else, and the all-cast form lands somewhere else again.

Related, and settled at the same time: **VC5 canonicalises x87 multiply
operand order** just as it does the integer one. Swapping the factors of any
term, or of several, is byte-identical every time — so when the original
loads the other factor first, that is allocation, not source.

## Corollary: a pointer local to an array is free
`float *p = DAT_ARR; ... p[k]` and `DAT_ARR[k]` are byte-identical (measured
three ways on the same rows). The pointer local is neither the problem nor the
fix; it is the CAST that differs.

## A third shape of "MISSING CODE": the port kept only the TAIL
*(0x1001FD70 BrDlVtxRoutine, 269 bytes short -> 6; found by claimcheck.py,
not by the size screens)*

Distinct from the factored helper and from deleted tracing: here the port
transcribed the last third of a function and changed its shape while doing so.
The original **installs into dispatch slots and returns void**; the port made
it **return the value** and dropped everything before the selection — three
state blocks that XOR the new mode against the PREVIOUS one and drive a
hardware setter off each group of changed bits.

**`python3 tools/claimcheck.py` finds this class and the size screens do
not** — it flags "the original delegates, the port calls nothing", and all
three missing calls showed up there. Run it at session start; it also now
reports two names claiming one address.

Three details worth carrying:

- **A `x_new ^ x_old` guard re-reads the mode after every block**, because the
  setter it calls can change it. Write the reload out; it is three
  instructions each and they are in the original.
- **"Assign then conditionally override" is a real source shape.** The
  original stores one routine unconditionally and replaces it in the next
  breath (`mov slot,A; test bit; jne skip; mov slot,B`). An if/else does not
  produce it.
- **A value-returning port helper built on top of a void original has to be
  `#ifndef BR_MATCHING_BUILD`'d out**, or the matching arm will not compile.

### And the duplicate-claim fault this session hit twice more

0x10031B80 and 0x10059410 each had a port body and a real transcription on the
same Glide VA, because a `d3d` tag resolves through `config/shared.csv`. The
short row poisons every size-based screen — 0x10031B80 ranked SECOND on the
whole factored-helper board at "-1165 bytes short" while the function it names
is size-exact. `claimcheck.py` now flags it directly; **fragments, thunks and
port-only bodies must not carry `@implements`.**

## VC5 canonicalises commutative FLOAT addition — operand order is not source-reachable

Measured on 0x10044860, where four `fld`/`fadd` pairs are the function's only
instruction-level divergence. For `local + member` VC5 always emits
`fld <member>; fadd <local>`, and the original emits the reverse. Three
spellings produce BYTE-IDENTICAL output:

  - `fy + cont->f33C`
  - `cont->f33C + fy`
  - `ay = fy + cont->f33C;` then passing `ay`

Declaring the local first among the locals does not move it either. So a
float `a + b` whose operand order is wrong is NOT a spelling problem: the
front end normalises before scheduling, and the choice of which operand
becomes the `fld` is made from the addressing modes (it loads the disp32
member and adds the disp8 stack slot). Contrast the SUBTRACTION case, which
is not commutative and does follow the source. Do not spend probes
permuting a float sum.

## Ghidra mis-typing the parent pointer: one cause, five symptoms

When Ghidra types a builder's single pointer parameter as `float param_1`
(0x10044860), every use of it picks up an `(int)` cast and the `+0x340`
store comes out as `*(float *)`. That looks like five unrelated
unhandled lines in a scaffold. It is one cause; undo the typing when the
draft is parsed rather than teaching the patterns about the casts.
`tools/gen_menubuilder.py` does this now and its entry-point regex accepts
any scalar spelling of the parameter.
