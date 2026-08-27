# VC5 idioms — BRGlide.dll structural-residue batch (2026-08-27)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. N64 twins were not required (x86 named every construct).

## Proven MATCH

- **`strcpy`/`strcat` are `/Oi` `repne scasb`/`rep movsd`.** Ghidra explodes
  them into counted memcpy loops (`long+N` / dense). Spell `strcat(dst, src)`
  with `extern char dst[]` (push offset, never `extern int`). Proven
  0x1006FF50 (109 B, MATCH /O2): `if (!strstr(buf, s)) { strcat(buf, s);
  strcat(buf, suffix); }`.

- **COM methods with this on the stack are `__stdcall`, not Ghidra
  `typedef int (*funcptr)()`.** Distinguisher: `push this; call [vt+N]`
  (no `add esp`) vs cdecl `add esp`. Recover dropped out-params from the
  bytes (`lea r, [esp+slot]; push`). `unaff_EBX`/`unaff_EBP` means Ghidra
  lost a real stack arg. Proven 0x10036740 (127 B, MATCH /O2):
  `typedef int (__stdcall *COM3)(void *this, void *buf, unsigned *size);`
  `hr = fn(obj, 0, &size); if (hr == 0x8877001e) { p = GlobalLock(
  GlobalAlloc(0x42, size)); ... *out = p; }`. HRESULT `0x8877001E` is
  `DPERR_BUFFERTOOSMALL`; `0x8007000E` is `E_OUTOFMEMORY`.

- **Name the vtable pointer before the zero-register.** Orig `mov eax,[g];
  mov ecx,[eax]; xor esi,esi; ... call [ecx+N]`. Ghidra `pMem=0; size=0;
  pObj=g;` loads vt after the xor. `pObj = g; vt = *(int **)pObj; pMem = 0;
  size = 0; hr = (*(COM *)((char *)vt + N))(...)` hoists the vt load.
  Proven 0x10036F40 (161 B, MATCH /O2; was 13). DirectPlay stdcall COM
  at `[vt+0x48]` (4 stack args) and `[vt+0x14]` (5 stack args).

- **`if (flag != 0)` fall-through first, and hoist the flag load above a
  later store.** Orig `mov eax,[dir]; mov [g],1; test eax; je else`.
  Ghidra `g=1; if (dir==0)` stores first and puts the ==0 arm first.
  `dir = DAT; g = 1; if (dir != 0) { n = n+1; if (n > 9) ... } else ...`
  plus `if (n > 9)` not `if (9 < n)`. Proven 0x1003C240 (99 B, MATCH /O2).

- **A 12-byte message is one stack struct, store order a / magic / b.**
  Ghidra shredded it into `local_c/local_8/local_4` and /O2 reused incoming
  arg homes (`no sub esp`). `struct { int cmd; int a; int b; } msg;
  msg.a = p2; msg.cmd = 0x60000008; msg.b = p3;` restores `sub esp, 0xc`.
  Proven 0x100371F0 (108 B, MATCH /O2).

- **Cross-jumped identical calls: repeat the call, do not hoist the shared
  dest lea.** Orig duplicates `lea ebx,[esi+0x10]` in both arms then
  `jmp common; push ebx; call`. A `pRight` local hoists the lea above the
  test (short-2). Write `BrVec3Cross((BrVec3 *)(p+4), ...)` in each arm.
  Proven 0x10001BB0 (210 B, MATCH /O2). thiscall + 1 stack arg (`ret 4`).

- **`(int)float` thunk is `int f(float)` — empty `int f();` promotes to
  double (`fstp qword`, `sub esp,8`).** 0x10018990 is `fld dword [esp+4];
  jmp __ftol`. `unsigned char >= 0x40` is `cmp r, 0x40; jb` (not `> 0x3f`).
  A thiscall callee is `__fastcall(this)` (`mov ecx,esi; call`). Proven
  as the 0x1006EBC0 body (104 B, size exact; 11 diffs leftover).

## Structural, not yet MATCH

- **0x1006AAF0 (110 B, 2 diffs /O2, size exact).** HANDLE[2] + re-deref
  mutex after the worker call + integer loop bases. Residue is ebx/ebp
  swap of the two CSE'd IAT pointers (orig loads ReleaseMutex IAT first
  into ebx, WaitForMultipleObjects into ebp; ours the reverse). Naming
  function-pointer locals made it worse. Allocator IAT-color wall — stop.

- **0x1005A280 (121 B, 4 diffs /O2, size exact).** `/Oi` `x/0xff` magic
  (`0x80808081`, `shr edx,7`). Residue: orig `add dest,4` early then
  `[ecx-3]/[ecx-2]`; ours `add src,4` early; plus `mov byte [esp],dl`
  vs `mov dword [esp],edx` on the mid-quotient spill. Increment/spill
  scheduler wall — stop.

- **0x1006EBC0 (104 B, 11 diffs /O2, size exact).** Structure matches
  (thiscall, float ftol, `cmp dl,0x40; jb`, clamp to 0x3f in cl). The 11
  bytes are `fmul const` rotated with `add esp,4` / `mov [esi+0x29bc],al`
  after the first ftol. Named-temp does not reorder fmul-by-scalar
  (VC5-IDIOMS.md). x87 scheduling wall — stop.

- **0x10031960 (217 B, 44 diffs /O2).** Word bswap at +0x14/+0x16 is
  `xor eax,eax; mov al,[15]; mov ah,[14]; mov word [14],ax` — the
  BrRcaFixupRecord form, not a byte-pair swap. Later dword pairs match
  the high-temp idiom. Residue: first-pair AL vs CL (byte-reg coloring)
  and the record loop (orig re-reads `mov ax,[esi+0x14]` every iter with
  ebx as the walker, shrink-wraps `push ebx` to the loop; ours CSE's n
  and walks esi). Block-scoping the walker forced `push ebx` into the
  prologue and blew the first 0xAF matching bytes.

- **0x1006A330 (177 B, 41 diffs /O2).** Zero-register ebx, `push -1` is
  INFINITE not EH. Orig stores then re-loads the mutex for ReleaseMutex;
  ours hoists the reload before the stores (different symbols, no alias).
  Volatile / one-struct-array not enough. Reload-timing wall — stop.

- **0x1006D0B0 (164 B, 95 diffs /O2).** BrBitStreamWriteBits: thiscall
  `ret 8`, leftover in the incoming nBits slot, `shl bl,cl` byte shift,
  `if (n==0) return; do {} while (remaining)`. Merge space and take into
  one variable (orig edi is both). Remaining is ebp-vs-ebx first-region
  coloring (orig `push ebp; mov ebp,[esp+0xc]`). Stop.

- **0x10039580 (136 B, 125 diffs /O2).** Jump-table `cmp eax,3; ja;
  jmp [eax*4]`. Each arm is `xor eax,eax; mov ecx, TABLE; cmp [ecx],key;
  je found; add ecx,0x24; inc eax; cmp ecx,END; jl header; xor eax,eax;
  ret`. Integer table bases give `mov ecx,imm`. VC5 peels the loop to
  `jge default; cmp [ecx],key; jne` (unsigned/inverted) from every
  while/do/for/goto spelling. Same family as the compacted-switch wall.

- **0x1005A630 (105 B, 46 diffs /O2).** Nested scan `if (*p==0) { c=p[1];
  if (c==p[2]) return 1; }`, y=0 hoisted before height, `stride = w*4`.
  Residue: orig `mov eax,edi; cmp byte [eax],0` vs ours `lea eax,[edi+2]`
  (CSE of p+2) and `jl` vs `jge/jmp` outer latch.

- **0x10059F70 (102 B, 75 diffs /O2, short-6).** BGR→BGRA + Y-flip.
  Load height first, src = base + (h-1)*stride. Orig walks src with three
  `inc ecx`; ours `add ecx,3` plus indexed loads. Dest is four `inc eax`
  stores. Pointer-walk vs index lowering.

- **0x10023CB0 (112 B, 80 diffs /O2).** RGB→ARGB1555:
  `pix = a>>7; pix = (pix<<5)|(r>>3); pix = (pix<<5)|(g>>3);
  pix = (pix<<5)|(b>>3)` with src `inc`s and dest `add esi,2` early.
  A one-expression pack is algebraically combined (`shr cl,1; and bl,0x83;
  or cl,bl`). Statement chain still folds. Bitfield-algebra wall.

## Generator candidates

1. **Ghidra exploded `repne scasb`/`rep movsd` → `strcpy`/`strcat`.**
   Distinguisher: orig `or ecx,-1; repne scasb; not ecx; shr ecx,2;
   rep movsd`. `extern char s[]` not `extern int`. Sweeps the CRT
   string cluster. Fired MATCH on 0x1006FF50.

2. **Ghidra `typedef int (*funcptr)()` cdecl vs orig stdcall COM
   (`push this; call [vt+N]`, no `add esp`).** `unaff_*` = dropped
   stack arg. Rewrite as `typedef int (__stdcall *T)(void *this, ...)`.
   Fired MATCH on 0x10036740 and 0x10036F40.

3. **`int f();` called with a float → `int f(float)`.** Distinguisher:
   `sub esp,8; fstp qword` vs `push ecx; fstp dword`. The 0x10018990
   thunk is the latter. Same empty-prototype / double-promotion idiom
   as 0x10019A70.

4. **Ghidra byte-pair swap of a 16-bit field vs orig `xor r,r; mov al;
   mov ah; mov word`.** That's a BE halfword, not two byte swaps.
   Distinguisher: `33 c0 8a 46 15 8a 66 14 66 89 46 14`. Same as
   BrRcaFixupRecord. Would sweep track-fixup 16-bit fields.

5. **Identical switch arms that orig did not merge, with `cmp p,end; jl`
   (signed) back to `cmp [p],key`.** Ghidra/VC5 peel to `jge; cmp [p];
   jne`. A refine that emits the compact jl-to-header form would sweep
   the 0x10039580 family. Mechanical once the table bases are integer
   constants.

## Allocator / x87 / lowering walls (do not grind)

| VA | diffs | size | class |
|---|---|---|---|
| 0x1006AAF0 | 2 | exact | IAT ebx/ebp swap |
| 0x1005A280 | 4 | exact | dest vs src increment; byte vs dword spill |
| 0x1006EBC0 | 11 | exact | fmul vs add-esp after ftol |
| 0x1006A330 | 41 | | mutex reload hoist (no alias) |
| 0x1006D0B0 | 95 | | remaining in ebx vs ebp |
| 0x10039580 | 125 | long+32 | loop peel; jl vs jge |
| 0x10023CB0 | 80 | exact | bitfield algebra |

None of these is EH (`push -1` here is INFINITE).

## N64 twins used

None. COM/Win32, strcat, thiscall bitstream, and the vec orthonormalizer
were readable from the x86. Top Gear Rally would pair 0x10031960's
record fixup if the loop shrink-wrap needs a second opinion.
