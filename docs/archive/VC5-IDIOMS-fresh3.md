# VC5 idioms — unmatched-DLL re-attempt batch (2026-08-27)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. Work lives in `build/ghidra_work/<VA>.c`.

## Proven MATCH

- **BrMat4 last-column + Z-row, float prototypes, DL emit.** A 0x40 frame
  with stores at +0x0c/+0x1c/+0x2c/+0x3c (0,0,0,1) and +0x20/+0x24/+0x28
  (0,0,1) is one `BrMat4`. Empty `int f();` promotes the scale immediates
  to double. Real `float` prototypes + `BrMat4 m` +
  `{ BrDlCmd *p = DAT++; p->op=…; p->arg=…; }` matched. Proven 0x10011D20
  (383 B, MATCH /O2). N64 twin not required.

## Structural, not yet MATCH

- **`strcpy`/`strcat`/`_itoa`/`memset`/`_strupr` are `/Oi` `repne scasb` /
  `rep movsd` / `rep stosd`.** Ghidra exploded them (`long+N`). Spell the
  CRT calls with `extern char s[]` (push offset). 0x10055AF0: stdcall
  `short` season, `strcpy(path, prefix); _itoa(season, num, 10);
  strcat(path, num); strcat(path, ext); fopen; memset 0x104; fread;
  if ((int)n < 0x80) warn; strcpy(dest, _strupr(buf)); fclose; return 1`.
  Head matches through the first `rep movsd`. Residue: `movsx ebp` vs
  `movsx ebx` for the season (coloring; ebp is free because ebx holds the
  strcat count later). 95 diffs /O2, 352 vs 348.

- **Voice alloc: `strcpy` into `p+0xa4` + fail-label *before* success
  body.** Orig `je fail` is a short jump; success body sits *after* the
  GlobalHandle/Unlock/Free cleanup so alloc-fail and the three
  `test eax; jne fail` land on the same block. Ghidra nested success
  inside `if (p)` and the `je` became 6-byte near (success too long).
  `goto fail` after each `!= 0`, success `goto done`, fail cleanup
  `p = 0`, `done: return p` restored size-exact 352. Residue: IAT
  coloring of GlobalUnlock/Handle/Free (orig edi/esi/ebp vs ours
  esi/edi/ebp). Same wall as 0x1006AAF0. 78 diffs /O2, size exact.
  Proven 0x1006BC10.

- **64-bit tick is `__int64` *globals*, not locals.** `extern unsigned
  __int64 accum` / `freq` emit `__allmul`/`__aulldiv` without `and
  esp,-8`. A local `__int64` is the 0x14/`and esp,-8` trap. Frame is
  0x10: `t` at `[ebp-4]`, unsigned-to-float `fild qword [ebp-0xc]`
  (delta + 0 high dword), float temp at `[ebp-0x10]`. `if (useQpc != 0)`
  first (orig `cmp; je` skips the mul path). Callee is `0x10059f00`,
  not Ghidra's `thunk_FUN_10071f00`. 22 diffs /Od after this; leftover
  is fild slot packing (`[ebp-0xc]` unaligned vs `[ebp-0x10]` aligned)
  plus orig's extra `fstp`/`fld` of the float temp. Proven 0x1002E186.

- **`unsigned char` args + live dword in ebx = `mov bl, [p+N]; push
  ebx`.** BrTex3dExpand's eight hi/lo bytes are `unsigned char` params.
  Orig loads `maskOdd` into ebx, pushes it, then each earlier byte
  overwrites `bl` and re-pushes ebx (high 3 bytes are stale mask).
  `movsx` is signed `char`; `movzx` is unsigned without the live ebx.
  Byte flag tests are `test dl, 0x40` from `(unsigned char)flags & 0x40`
  after a dword load. Head + the whole char-push run match. Residue:
  lod in eax vs ecx and tile-field push interleave (`[tile+0x60]` /
  `[tile+0x64]` loaded together in orig, split in ours). 89 diffs /O2,
  368 vs 358. Proven 0x10027B60.

- **DDraw_DoInit thiscall 0-stack + `inc word` + signed bound.**
  `mov ecx, [obj]; mov edx, [ecx]; call [edx+0x20]` is
  `__fastcall(this)` (Flip/Begin). `extern short DAT_10ac5c2c;
  DAT++` is `inc word ptr` (Ghidra's `(short)x + 1` stored as dword
  was `movsx; inc; mov dword`). Loop bound is `cmp esi, 0x10ac5874;
  jl` (`<` signed), not `> 0x10ac5873`. Head matches 83 insns.
  Residue: store of `pi[-1] = result` delayed past `mov ecx, [esi];
  add esp, 0xc; test ecx` (scheduler; volatile and a `{surf,name}`
  struct did not hoist the store). 54 diffs /O2, 384 vs 379. Proven
  0x100583C0.

- **Mode-list: `extern char s[]` + thiscall-with-stack is the dummy-edx
  wall.** Frame `sub esp, 0x5c` = `CHAR buf[80]` + i + found + saved
  bpp. `wsprintfA` cdecl, formats as `char[]`. COM is thiscall + 5/3
  stack args: `mov ecx, obj+0x3838; mov edx, [vt]; call [edx+0x10]`
  with edx = *vtable*, not a dummy 0. `__fastcall(this, int dummy,
  args…)` emits `xor edx,edx` and forces a 4-reg prologue before the
  shrink-wrapped helper call. Same class as 0x10037FA0. 261 diffs /O2.
  0x10058E20.

- **DirectPlay HRESULT `0x88770082` + 0x78 frame.** `if (this == 0)
  return 0x88770082` (DPERR_INVALIDOBJECT). `sub esp, 0x78` = 0x28 +
  0x50 desc (`dwSize = 0x50`, flags `c ? 0x140 : 0x40` =
  `neg; sbb; and 0x100; add 0x40`). COM is stdcall (this on the stack,
  `call [vt+0x9c]` / `[vt+0x18]`, no `add esp`). Ghidra's cdecl
  `funcptr` plus shredded locals collapsed the frame to 0x18. Head
  matches through the NULL check. 197 diffs /O2, 368 vs 383. 0x10035C50.

- **Bind loop: 0x2b8 frame = 4 ints + `int rec[0xaa]`, `memcpy` is
  `rep movsd`.** Ghidra's `int auStack_2a8[18]` was 72 B; orig copies
  0xaa dwords from `DAT_106b7aa0 + idx*0x2b4 + 4`, then writes rec
  fields at +0x48/+0x4c. Zero-register `xor edx,edx` before `sub esp,
  0x2b8`; unsigned `cmp si, dx; jbe`. Best score still `/O2 /Oy-`
  (ebp frame vs orig `sub esp` only). 298 diffs. 0x100299A0.

## Not a function

- **0x10073994 (357 B) is data.** Head `eb 00 00 00 0c eb 00 80 …`
  repeating 16-byte records, first byte 0xEB..0xFF, pointer
  `0x100786d8`. survey.csv: `dead`. Capstone reads ADD [EAX], AL
  because Ghidra cut .rdata as .text. Not SEH (`push -1` / `fs:[0]`
  absent). Unreachable from C.

## Generator candidates

1. **Ghidra exploded `repne scasb`/`rep movsd`/`rep stosd` →
   `strcpy`/`strcat`/`memset`.** Distinguisher: orig `or ecx,-1; xor
   eax,eax; repne scasb; not ecx; shr ecx,2; rep movsd`. `extern char
   s[]` not `extern int`. Fired MATCH-shape on 0x10055AF0 (95 leftover
   coloring) and 0x1006BC10 (78 leftover IAT). Sweeps the CRT string
   cluster. Already a candidate in dll.md / dll2.md.

2. **Success body after fail cleanup → `goto fail`.** Distinguisher:
   orig short `je fail` (2 B) to a block *before* the success tail;
   Ghidra's nested `if (ok) { success; return p; }` makes fail far and
   the `je` is `0f 84` (6 B). Rewrite so fail is the forward label and
   success falls into a shared `return p`. Fired size-exact on
   0x1006BC10.

3. **`extern unsigned __int64` for add/adc pairs; no local `__int64`.**
   Distinguisher: orig `add ecx,eax; adc eax,edx` on two consecutive
   globals, frame `sub esp, 0x10` with ebp, no `and esp,-8`. Ghidra
   `__int64` locals and `__allmul` as `double` returns. `(float)(unsigned)
   delta` is `mov [slot],delta; mov [slot+4],0; fild qword`. Fired the
   0x1002E186 body (22 leftover).

4. **`unsigned char` params after a live dword in the same register →
   `mov bl; push ebx`.** Distinguisher: `8a 9e xx; 53` not `0f be` /
   `0f b6`. Pair with `(unsigned char)flags & K` for `test dl, K`.
   Fired the 0x10027B60 expand site.

5. **`extern short g; g++` → `inc word ptr [g]`.** Distinguisher:
   `66 ff 05` vs `0f bf; 41; 89`. Fired 0x100583C0.

6. **Ghidra `if (p > end-1) return` → `while (p < end)` (`cmp p, end;
   jl`).** Distinguisher: `3b; 7c` vs `cmp; 7f`. Same family as
   `if (n <= K)` not `if (n < K+1)`.

## Allocator / thiscall-edx / data walls (do not grind)

| VA | diffs | size | class |
|---|---|---|---|
| 0x10011D20 | 0 | exact | MATCH |
| 0x1002E186 | 22 | 346 vs 344 | fild slot / float-temp |
| 0x100583C0 | 54 | 384 vs 379 | store-vs-reload scheduler |
| 0x1006BC10 | 78 | exact | IAT edi/esi Unlock vs Handle |
| 0x10027B60 | 89 | 368 vs 358 | lod eax vs ecx; tile-field push order |
| 0x10055AF0 | 95 | 352 vs 348 | season ebp vs ebx |
| 0x10035C50 | 197 | 368 vs 383 | stdcall COM arg/store residue |
| 0x10058E20 | 261 | 368 vs 353 | thiscall+stack dummy-edx |
| 0x100299A0 | 298 | 384 vs 361 | ebp frame / zero-register |
| 0x10073994 | — | 357 data | not C |

None of these is SEH.

## N64 twins used

None. Clock, strcpy, DDraw, DirectPlay, and the tex expander are PC.
The 0x10011D20 BrMat4 was readable from the x86 stores.
