# VC5 idioms — fresh unmatched batch (2026-08-27)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. N64 twins were not required (x86 named every construct).

BATCH: 0x100096A0 0x1003BAC0 0x1005C6D0 0x10032320 0x1006C290 0x1003C430
0x1001CA30 0x1006C4D0 0x10054390 0x10028200 0x10028620 0x100590D0

## Proven MATCH

- **Track wrap: `if (dir != 0)` first, `if (n > K)` not Ghidra `if (K < n)`,
  `dec; jns` wrap is `n = n - 1; if (n < 0) n = K`.** K is the two-constant
  ternary `(flag ? 3 : 0) + 0xb` (`neg; sbb; and 3; add 0xb`). BrStrGet is
  cdecl 1-arg (`add esp, 4`); Ghidra invented a 2-arg `BrStrGet(0xb8, prev)`
  from a leftover push. That leftover is sprintf's third arg:
  `sprintf(buf, BrStrGet(0xb8), BrStrGet(table[id]))` (`add esp, 0xc`).
  Exploded `repne scasb`/`rep movsd` → `strcat` / `strcpy` with
  `extern char s[]`. The first FUN_100387f0 after a wrap must be
  `f(DAT)` not `f(saved)` so orig `push eax; mov esi, eax` (inc path already
  had it; the dec path was the 3-byte residue). Proven 0x1003C430 (458 B,
  MATCH /O2).

- **thiscall edit-field: `if (x != 0)` first, `strlen` not exploded,
  return type `char`.** Orig `cmp [g], ebp; je next` so the `!= 0` arm
  (calls with 4) is fall-through; Ghidra nested `if (g == 0)`. Four arms
  (4/5/6/7) are NOT cross-jumped — repeat the call pair. `repne scasb;
  not ecx; dec ecx; je` is `if (strlen(s) != 0)`. Backspace store is
  `this[8 + strlen(this + 9)] = 0` (`mov [ecx+esi+8], al`) — a `char *s =
  this+9; s[strlen(s)-1] = 0` encodes `[ecx+edx-1]` (2 diffs). Vtable
  `+4` is thiscall 0-stack (`mov edx,[esi]; mov ecx,esi; call [edx+4]`).
  Returns `mov al, 1` / `xor al, al` / `or al, 0xff` — `char __fastcall`,
  not int. Proven 0x10054390 (442 B, MATCH /O2).

## Structural, not yet MATCH

- **0x10028620 (440 B, 136 diffs /O2, size +1).** `if (field != 0)` first
  restores orig `lea esi, [ecx+eax*4+4]; mov eax, [ecx+eax*4+0x26c];
  test; je else` (Ghidra `== 0` first split the lea). Callees cdecl
  (add esp 0xc / 0x18 / 0x20). Residue is the 0/w/h store burst after
  BrBmpRect4Get (orig `xor ecx; shl w; shl h; store 0, w, 0, h`; ours
  both shls then both zeros) plus a 24-byte body-size cascade. Store-
  order wall — stop.

- **0x100096A0 (469 B, 167 diffs /O2, +11).** `if (ctx->f0C != 0)` first
  (sysmsg, else app). `i = *msg; pMem = 0; switch (i) { case 3: case 5:
  case 0x104: }` is `sub eax, 3; je; sub eax, 2; je; sub eax, 0xff; jne`
  — Ghidra if/else-if is `cmp`. Strings are `extern char[]` (`push offset`).
  lstrlenA / GlobalAlloc / wsprintfA inlined in both arms (do NOT factor
  a helper). Residue: VC5 evaluates `lstrlenA(fmt)+1+lstrlenA(name)`
  name-first (`push edi; call; push fmt`); orig is fmt-first. Named temps
  for fmt-first steal ebp from param_2 (ebx vs ebp at the dispatch, 232
  diffs). Eval-order wall once the switch is in — stop.

- **0x10032320 (461 B, 227 diffs /O2).** stdcall COM (`push this; call
  [vt+N]`, no add esp): +0x20 arity 4, +0x30 arity 4, +0x3c arity 5,
  +0x18 arity 7, Release +8 arity 1. `if (hr >= 0)` is `cmp ebp, edi; jl`
  with edi=0 (`ge0`). HRESULT `0x8877001E` / `0x8007000E`. strcpy of two
  name fields. Ternary `((flags >> 1) & 1) ? 0x100 : 0` is `neg; sbb; and
  0x100`. Residue: orig `mov esi, [p+4]; and esi, 0xff` immediately;
  ours delays the and until after the 0x44/0x28 stores (size exact at
  461 then a positional cascade). `unsigned char flags` added a 4-byte
  slot (`sub esp, 0x14`). And-hoist wall — stop.

- **0x10028200 (441 B, 235 diffs /O2 /Oy-).** Glide `grTexTextureMemRequired`
  / `grTexMaxAddress` are `__stdcall` (E8 thunks). Bound is unsigned
  `if (n >= 0x400)` (`cmp; jae`), not Ghidra `if (0x3ff < n)`. unaff_ESI
  is the required-size result. TMU is param_1, slot index is DAT (Ghidra
  used DAT for both). `(int)(float * k)` is `__ftol`. 15 stack args; the
  float is arg 14. Extra-arg / ftol-local still forces an ebp frame.
  Stop.

- **0x1006C4D0 (450 B, 323 diffs /O2).** DirectSound: stdcall COM
  Initialize +0x28, SetCooperativeLevel +0x18, CreateSoundBuffer +0xc,
  Play +0x30, Release +8. `rep stosd` of 15 dwords is `memset(p, 0, 0x3c)`.
  `inc; cmp eax, 1; mov [g], eax; je` — do not re-store `g = 1`.
  `return hr >= 0` not Ghidra `return -1 < hr`. `FUN_10074558` is a 6-byte
  IAT thunk, stdcall 3 (no add esp). Frame 0x18 = DSBUFFERDESC (0x14) +
  size out-dword. Residue: ebx 0-register (`xor ebx; cmp eax, ebx` vs
  orig `test eax, eax`) from the desc/WAVEFORMATEX 0-stores. 0-reg wall.

- **0x1003BAC0 (469 B, 381 diffs /O2).** `if (param_2 == -1)` first
  (orig `cmp -1; jne other`). `if (DAT_10ac4100 != 0)` is `&extern !=
  NULL` (`mov eax, offset; test eax, eax`). strcpy/strcat/_itoa of
  `"TimeAttack" + itoa(idx) + ".grf"`. Frame 0x108 = `char num[4]; char
  buf[260]`. Residue: orig 4 callee-saved (ebx=param_1, ebp=buf); ours
  only esi/edi — param_1 rematerializes from the incoming home across
  `_itoa`. Named `char *buf` did not keep ebp. Live-across-call coloring
  wall.

- **0x1001CA30 (450 B, 352 diffs /O2).** `if (g == 0 && dat && n > 0)`
  wrap (early `return` shrink-wraps `push ebp`). Unsigned `xor r,r; mov
  cl, [mem]`. `if (sum >= bound)` (`cmp sum, bound; jl`). `if (*p & 1)`
  first (orig `test byte [eax], 1; je zero-arm`). `(p[0] & 1) ? 6 : 0`
  is `and dl,1; neg; sbb; and 6`. Residue: orig 0-register is ebp
  (`push ebp; … xor ebp, ebp`); ours ebx. 0-reg coloring wall.

- **0x1006C290 (459 B, 355 diffs /O2).** Frame 0x410 = `char buf[1024]`
  + 4 ints. `if (param == 0)` first. `i / 2` is signed `cdq; sub; sar`.
  strcpy/strcat of prefix + name, then cdecl FUN_1006bc10. Residue:
  `ok = 1` materialized in ebx (`mov ebx, 1; mov [slot], ebx`) vs orig
  immediate `mov [slot], 1`. 0-reg/imm-vs-reg wall.

- **0x1005C6D0 (468 B, 380 diffs /O2).** thiscall (`mov esi, ecx`).
  Callees: 0-stack thiscall BrCarInitTables / FUN_1005bcc0; 3-float
  thiscall SetPos/Vel/AngVel (`ret 0xc`, floats on the stack not edx);
  1-float SetHeading (`ret 4`); FUN_100018f0 `ret 8` (ptr + 1.0f on
  stack, dummy edx). BrAtan2 is cdecl `float(float,float)` pipelined
  into SetHeading (`fstp [esp+4]; add esp, 4`). Zero-register edi,
  ebx=2. Residue: extra `push ebp` (4 extra bytes, then a cascade).
  Dummy-edx / extra-callee-saved wall.

- **0x100590D0 (419 B, 319 diffs /O2).** `ret 0x14` = stdcall 5 args.
  `switch (msg)` restores orig `cmp eax, 0x102; ja; je; cmp eax, 6`
  (Ghidra `if (msg < 0x103)` is `cmp 0x103; jae`). Cases 6, 0x102,
  0x111, 0x112, 0x211/0x231, 0x212/0x232, 0x400. Residue: orig
  `push edi; cmp; mov edi, 1` (live 1-register for every `return 1`);
  `int r = 1; return r` folds back to `mov eax, 1`. 1-register fold
  wall.

None of the 12 start `push -1` / `fs:[0]` — not C++ EH.

## Generator candidates

1. **Ghidra exploded `repne scasb`/`rep movsd` → `strcpy`/`strcat`/
   `strlen`.** Distinguisher: orig `or ecx,-1; repne scasb; not ecx;
   shr ecx,2; rep movsd` (or `dec ecx; je` for strlen). `extern char
   s[]` not `extern int`. Fired MATCH on 0x1003C430 (strcat/strcpy)
   and 0x10054390 (strlen). Sweeps the string cluster; already a
   candidate in VC5-IDIOMS-dll.md.

2. **Ghidra `if (x == 0) { rest } else { arm_K }` whose orig is
   `test/cmp; je next; arm_K`.** Flip so `!= 0` is first. Fired MATCH
   on 0x10054390 (four 4/5/6/7 arms) and 0x1003C430 (dir wrap). Same
   family as 0x1003C240 / 0x1003CAE0. Also flipped 0x10028620 from
   331 → 136 (lea encoding snapped).

3. **`mov al, 1` / `xor al, al` / `or al, 0xff` at the ret → return
   type `char` (or `unsigned char`), not int.** Distinguisher: `b0 01
   c3` vs `b8 01 00 00 00 c3`. Fired MATCH on 0x10054390. Sweeps
   small WndProc-adjacent helpers.

4. **Ghidra `BrStrGet(a, b)` 2-arg from a leftover push → cdecl 1-arg
   plus a later varargs consumer.** Distinguisher: `E8; 83 c4 04`
   twice, then `push; push dest; call sprintf; 83 c4 0c`. The first
   return is sprintf's extra arg. Fired MATCH on 0x1003C430.

5. **`sub eax, K; je; sub eax, K2; je` after a load → `switch` on
   cases K, K+K2, …** Ghidra if/else-if is `cmp; jne`. Fired the
   0x100096A0 3/5/0x104 tree (167 leftover is eval order, not the
   switch). Same as the `dec eax; je` idiom in VC5-IDIOMS-batch1.md.

## Final diffs (this session)

| VA | orig B | diffs | opt | class |
|---|---|---|---|---|
| 0x1003C430 | 458 | **0** | /O2 | MATCH |
| 0x10054390 | 442 | **0** | /O2 | MATCH |
| 0x10028620 | 440 | 136 | /O2 | store-order after matching lea |
| 0x100096A0 | 469 | 167 | /O2 | lstrlen eval order |
| 0x10032320 | 461 | 227 | /O2 | and-esi hoist |
| 0x10028200 | 441 | 235 | /O2 /Oy- | extra float arg / ftol frame |
| 0x100590D0 | 419 | 319 | /O2 | 1-register fold |
| 0x1006C4D0 | 450 | 323 | /O2 | 0-reg ebx |
| 0x1001CA30 | 450 | 352 | /O2 | 0-reg ebp vs ebx |
| 0x1006C290 | 459 | 355 | /O2 | imm 1 vs ebx=1 |
| 0x1005C6D0 | 468 | 380 | /O2 | extra ebp |
| 0x1003BAC0 | 469 | 381 | /O2 | ebx/ebp live-across-_itoa |
