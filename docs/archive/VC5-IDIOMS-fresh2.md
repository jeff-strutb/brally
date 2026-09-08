# VC5 idioms — unmatched re-attempt batch (2026-08-27)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. N64 twins were not required (x86 named every construct).

Batch: 0x1006A650 0x100018F0 0x1005A080 0x100314D0 0x1001E7A0 0x10055D40
0x100368A0 0x10020900 0x1006BDD0 0x1006B240 0x10032190 0x1005C560.

## Proven MATCH

- **Two `HANDLE[2]` arrays, byte bool, WaitForMultipleObjects IAT in ebp.**
  Ghidra's four scalar locals coalesced to an 8-byte frame (`sub esp,8`)
  vs orig `sub esp,0x10`. Spell `HANDLE h1[2], h2[2];` — the second loop
  uses both arrays at once. `char skip` not `int` (`mov bl,1` / `xor bl,bl`
  / `sete bl` / `setne bl`). A loop of `WaitForMultipleObjects` hoists
  the IAT to ebp (`mov ebp,[imp]; call ebp`). `if (wr == 0) ExitThread(0)`
  is `test eax,eax; jne; push eax` (the 0 is still in eax).
  `(p[0xb] & 0x3f) < 2 || == 3` is dword `and eax,0x3f; cmp eax,2; jl;
  cmp eax,3; je`. Later `== 3` is dword and + `cmp cl,3; sete bl` (the
  `(*p & MASK) == K` idiom: and stays 32-bit, cmp narrows). Walk
  `p += 0x25b; while ((int)p < end)`. Proven 0x1006A650 (400 B, MATCH /O2).

- **thiscall + real `float` prototypes + `== K` fall-through for
  cross-jumped Scale.** Empty `int BrVec3Scale();` promotes the float
  arg to double (`fstp qword`). `void BrVec3Scale(void *, void *, float)`
  restores `push imm32`. The gravity arm is
  `if (flag != 0) Scale(dst, this, 11.0f); else if (g == 5) {
  Scale(dst, this+0x10, -11.0f); MulAddTo(dst, this, -13.0f); }
  else Scale(dst, this, -11.0f);` — `== 5` so orig is `push -11; cmp g,5;
  jne common` (the -11 is live in both the special and the common Scale).
  `else if (g != 5)` inverts the jcc and inlines the first Scale (no
  cross-jump). `ret 8`. `len != _DAT_zero` is `fcom` (not `fcomp`) so
  ST0 survives for `fdiv`. Proven 0x100018F0 (399 B, MATCH /O2; 1 COFF nop).

- **sprintf format is `extern char s[]`; memcpy size is re-spelled after
  malloc; `x >= 0` is `test; jl`.** `extern int s_Paint` pushes a load;
  orig `push offset`. `if (p[4] >= 0)` not Ghidra `-1 < p[4]`. A size
  local live across `malloc` CSE's `h*w*4` into esi; orig reloads both
  globals and `imul; shl 2` for the `/Oi` memcpy. Spell
  `p = malloc(h*w*4); memcpy(p, src, h*w*4);` as two expressions.
  `buf[8] = 100` pokes the sprintf output (orig `mov byte [dest+8], 0x64`).
  Proven 0x1005A080 (399 B, MATCH /O2; 1 COFF nop).

- **`strcpy` is `/Oi` scasb/movsd; `n += 8` not `n + 8`; `x >= 0`.**
  Ghidra exploded `repne scasb; rep movsd` into counted loops (`long+N`).
  `strcpy(dst, DAT)` with `extern char DAT[]`. `n = lstrlenA(s); n += 8;`
  is `mov esi,eax; add esi,8` (reused for Alloc and Send); `Alloc(n+8)`
  is `lea esi,[eax+8]`. `if (hr >= 0)` is `test eax,eax; jl` not
  `if (-1 < hr)`. Zero-register ebp. `(p3 != 0) + 0x60000000` is
  `xor ecx,ecx; test; setne cl; add ecx,0x60000000`. Win32
  GlobalHandle/Unlock/Free (IAT in esi/edi/ebx), not Ghidra's
  `GlobalFree_exref` funcptr. Proven 0x100368A0 (389 B, MATCH /O2;
  11 COFF nops to 16-byte align).

## Structural, not yet MATCH

- **0x1001E7A0 (397 B, 168 diffs /O2, recomp 390 + 10 nops).** Cross-jump
  of `grColorCombine` per arm is right (`__stdcall`, push immediates,
  `jmp common; push 3; call`). The 7 missing bytes are orig's
  `cmp eax, 0xfcffffff; jne rest` re-test of w0 inside
  `if (w0 == X) { if (w1 == B) A; else if (w0 == X && w1 == CW) C; }`.
  Nested `else if (w0 == X && …)` CSE's the inner cmp. Separate ifs,
  inverted `if (w0 != X) goto rest`, and a `w0 = param_1` copy all
  still CSE. Cascade from those 7 bytes is the 168. Tail (DAT_105cda04
  != 0 fall-through first, swap 0x100A9A68 between 0x10021C70 and
  0x100221D0) matches. Stop — CSE of the nested re-test.

- **0x1006BDD0 (379 B, 108 diffs /O2, extra 5).** First 79 insns are
  mnemonic-identical except esi/edi of index vs byte-offset (orig
  `xor esi,esi; mov ebx,table; xor edi,edi`; ours swaps esi/edi).
  Repeat `BrSfxChanStart` per arm recovered the `push 1; push i; push
  0|0x19; jmp common`. Named temps recovered `v = new; if (old != v)
  old = v` (new loaded first). Residue: esi/edi first-region coloring
  plus `cmp [mem],ecx` vs `mov edx,[mem]; cmp edx,ecx` on the pan
  dirty-check. Same class as 0x1006AAF0's IAT ebx/ebp swap. Stop.

- **0x100314D0 (398 B, 232 diffs /O2, extra 2).** cdecl callees, cdecl
  funcptrs (`add esp` after `call [DAT]` — do NOT retype stdcall).
  `*(unsigned short *)` not `*(short *)` (xor-extend, not movsx).
  `if (*pw > max)` signed (`jle` not `jbe`) — `int max` not unsigned.
  Residue: orig `xor eax,eax; mov ax,[mem]` for the second count load
  vs ours `xor ecx,ecx; mov cx,[mem]` (eax vs ecx of a dead-across-call
  zero-extend). Allocator wall.

- **0x1005C560 (368 B, 306 diffs /O2, size exact).** thiscall
  (`mov esi,ecx; call BrEntityBindAux(this);`). `(this - 0x10af1208)
  / 0x2b68` is signed-div magic `0x5e5d422b`. Orig `mov ecx,esi; mov
  eax,magic; sub ecx,BASE; imul ecx` — the magic load sits BETWEEN
  the copy and the sub, blocking the lea peephole. Ours
  `lea ecx,[esi-BASE]`. `idx = this; idx -= BASE; idx /= N` still
  leas. unsigned char `xor r,r; mov cl,[eax+eax*2+tab]` recovered.
  Re-deref of `*(int *)(this+0xe8c)` per store recovered. Residue is
  the 2-byte lea-vs-sub then a cascade. Stop.

- **0x10020900 (380 B, 269 diffs /O2, short 12).** `push ecx` is the
  4-byte float spill; without it the three unsigned-char loads
  (`xor ecx; xor edx; mov cl,[p+5]; mov dl,[p+6]; xor eax; mov al,[p+4]`)
  collapse to one-at-a-time. `grDrawTriangle` is `__stdcall` (E8 thunk,
  no `add esp`). `(c5 & c4 & c6) != 0` early-out; `(c4 | c6 | c5) != 0`
  is the clip helper; else the lighting stores + draw. Float products
  spill via `fstp [slot]; mov r,[slot]` (int copies of the bits).

- **0x10032190 (368 B, 271 diffs /O2, extra 32).** BE assemble of the
  count at +0x224 (`xor eax,eax; mov al,[+0x225]; mov cl,[+0x226];
  mov ah,[+0x224]; mov dl,[+0x227]; shl 8; or; shl 8; or`). Jump-table
  `movsx eax, byte [p+3]; cmp eax,7; ja; jmp [eax*4+table]`. Param in
  ebx, count/zero in ebp (`push ecx; push ebx; mov ebx,arg` then late
  `push ebp; mov ebp,0`); ours swapped those two. Case 0/1/2 is two
  dword byte-pair swaps (lo into cl, hi into al). Case 4 assembles a
  second BE int and `if (count > 0)` walks `BrSwapVec3`.

- **0x10055D40 (391 B, 318 diffs /O2, extra 57).** thiscall `ret 4`.
  Frame 0x228 = 3 ints + `char header[0x104]` + `_finddata_t` (name at
  +0x14). Ours 0x22c. `rep stosd` of 0x41 dwords zeros the header.
  fopen mode is `extern char DAT_100ac9c8[]` (push offset). Virtual
  `add = *(fn *)(*this + 0x18); add(this, 0, name, header)` is thiscall
  (`mov ecx, this; call eax`) not stdcall-COM. fseek IAT hoisted to ebx.
  Capped at 100. `_findfirst` / `_findnext` / `_findclose` cdecl.

- **0x1006B240 (370 B, 336 diffs /O2, short 18).** Frame 0x34 with a
  burst of `xor eax,eax` stores (lock out-params + desc). Ours 0x24 —
  Ghidra shredded the lock pointers into `stack0xffffffbc`. COM is
  stdcall (`push this; call [vt+N]`, no `add esp`): CreateSoundBuffer
  +0xc arity 4, Lock +0x2c arity 8, Unlock +0x4c arity 5, Play +0x3c
  arity 2, Stop +0x40 arity 2, Release +8 arity 1, GetCaps +0xc arity 2.
  `memcpy` of the locked pointer. `param_1[1] & 4` selects looping.

None of these is EH (`push -1` is INFINITE).

## Generator candidates

1. **Ghidra `extern int fmt` / mode string → `extern char s[]`.**
   Distinguisher: orig `68 xx xx xx xx` (push offset) vs `8b … 50`
   (load dword, push). Already the string-as-char-array idiom; this
   batch added a sprintf site (0x1005A080) and an fopen mode
   (0x10055D40, still unmatched on frame). Sweeps CRT file/format
   callers.

2. **Ghidra `-1 < x` / `x > -1` → `x >= 0`.** Distinguisher: orig
   `85 c0 7c` (`test; jl`) vs `83 f8 ff 7e` (`cmp -1; jle`). Fired
   MATCH on 0x1005A080 (`p[4] >= 0`) and 0x100368A0 (`hr >= 0`).
   Same as the 0x1006E130 note in VC5-IDIOMS-dll2.md.

3. **Ghidra exploded `repne scasb`/`rep movsd` → `strcpy`.** Already
   a generator in VC5-IDIOMS-dll.md. Fired MATCH on 0x100368A0.

4. **`Alloc(n + K)` reused as send-size → `n += K` then reuse.**
   Distinguisher: orig `8b f0 83 c6 08` (`mov esi,eax; add esi,8`) vs
   `8d 70 08` (`lea esi,[eax+8]`). Fired MATCH on 0x100368A0.

5. **memcpy size local live across malloc → re-spell `h*w*4` at the
   memcpy site.** Distinguisher: orig reloads both globals and
   `imul; shl 2` after malloc; CSE'd local is `mov ecx,esi`. Fired
   MATCH on 0x1005A080. Sweeps other malloc+memcpy pairs.

6. **Cross-jump `== K` fall-through, not Ghidra `!= K` first.**
   Distinguisher: orig `cmp r,5; jne common` vs `cmp r,5; je special`.
   Fired MATCH on 0x100018F0's Scale arm. Same polarity rule as
   `if (dir != 0)` fall-through first.

7. **Two `HANDLE[2]` when orig `sub esp,0x10` and two
   `WaitForMultipleObjects` are live together.** Distinguisher: orig
   `83 ec 10` vs recomp `83 ec 08`. Fired MATCH on 0x1006A650.
   Sibling of the 0x1006AAF0 HANDLE[2] note.

8. **Repeat the Glide/helper call per arm (cross-jump), never temps +
   one call.** Already documented. 0x1001E7A0's combiner chain is the
   next sweep target once the nested w0 re-test has a lever; the
   per-arm `grColorCombine` shape is already right.

## N64 twins used

None. Win32 mutex/file/COM, Glide combiner, and vec3 Scale were
readable from the x86. Top Gear Rally would pair 0x10032190's BE
record walk if the ebx/ebp param-vs-count coloring needs a second
opinion on the switch body.
