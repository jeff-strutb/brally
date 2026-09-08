# VC5 idioms — batch 1 (structural residue)

Proven against BRGlide.dll this session. Merge into `docs/VC5-IDIOMS.md`.
Infer source from the bytes; never permute spellings. N64 twins were not
required for any MATCH in this batch (x86 named the constructs).

## Proven MATCH

- **`dec eax; je` switch after a call result, cases 1..N.** Ghidra's
  if/else-if on the stored global emits `cmp eax,1; jne`. Naming the
  return (`v = f(); g = v; switch (v) { case 1: ... case 2: ... case 3:
  ... default: }`) lowers to `mov [g],eax; dec eax; je; dec eax; je;
  dec eax; je`. Comparison operand order: `if (n > K)` not Ghidra
  `if (K < n)` (`cmp n,K; jle`). Proven 0x10061310 (273 B, MATCH /O2).

- **Glide `grTex*` are `__stdcall`; `(float)(unsigned)x` is `fild qword`
  with a 0 high dword.** Ghidra's empty cdecl prototypes emit `add esp`
  after each thunk (long+17). Unsigned bound is `jae` (`extern unsigned
  int` count). Lod-bias: `sub esp,8; mov [esp+4],0; mov [esp],x; fild
  qword [esp]`. Proven 0x10028420 (183 B, MATCH /O2).

- **Do not OR two arms that both load the same float constant.**
  `if (p==0) f=0; else if (z<0) f=0; else f=k*z;` keeps two `fld const`
  (orig). `if (p==0 || z<0) f=0; else f=k*z;` cross-jumps them into one
  `fld` (short-8). Same family as "don't OR identical-call guards".
  `switch (i - zero)` with a live 0-register is `mov eax,i; sub eax,ebp;
  je; dec; je; dec; je`. Frame is one `float vel[3]` (12 B), not
  shredded `char[8]+float`. Proven 0x10068600 (200 B, MATCH /O2).

- **`strcpy` is `/Oi` `repne scasb`/`rep movsd`; `fopen(s, mode)` takes
  `extern char s[]` (push offset).** Ghidra's exploded strlen+memcpy is
  the same bytes once spelled `strcpy`, but `extern int s_Foo` pushes a
  load. Loop latch `cmp ebx,0x6590; jl` is `if (i < 0x6590) continue`
  not `if (i >= 0x6590)` (`cmp 0x658f; jle`). `ret 4` = stdcall 1 unused
  arg. Proven 0x10055A40 (165 B, MATCH /O2).

## Structural, not yet MATCH

- **0x100298C0 (216 B, 4 diffs /O2).** One `int s[0xaa]` + `memcpy` of
  0x2A8 is the tile copy (`rep movsd`, `sub esp,0x2a8`). Remainder is
  param_2 vs param_3 hoist (edx from `[esp+0x2b8]` first). Field `s[6]
  = s[7]` is the BrTexShiftFromSize out[0] copy to +0x18. Stopped at
  4-byte allocator/hoist residue — sizes equal after padding.

- **0x1006AEB0 (234 B, ~157 diffs /O2).** Bitstream writer: callees are
  thiscall (`BrCountedTotal` 0-arg; `WriteU8/U24/U32` via `struct { unsigned
  v; }` so the value is not edx-eligible). `param_3 & 0x3f` is signed
  (`jg` not `ja`); `if (n <= 2) n += 0x18` not `n = base+0x22`;
  `if (n <= 0x100) { body; return 1; } return 0;` outlines the fail.
  Remaining: mask spill to `[esp+0x14]`, `push 0` vs xor+push, `mov al`
  vs dword load of the name byte. Generator: Ghidra cdecl 0-arg
  `BrBitStreamWriteU8(x)` → thiscall+struct.

- **0x10036F40 (161 B, 13 diffs /O2).** DirectPlay-style stdcall COM
  (`push this; call [vt+0x48]`), HRESULT `0x8877001E`
  (`DPERR_BUFFERTOOSMALL`). Ghidra dropped the size out-param, both
  stack args, and used cdecl. Residue is scheduling of `size=0` vs
  vtable load (orig loads vt before `xor esi`). Stop: 13 diffs,
  size +15.

- **0x10059350 (181 B, 106 diffs /O2, short-21).** Function is
  thiscall + 1 stack arg (`ret 4`; `__fastcall(this, struct {void*p;})`).
  DirectInput CreateDevice is **stdcall, this on the stack**. Ghidra
  dropped the stack arg and the `0x10072ac0` callback. Remaining: VC5
  merged the two identical `BrStrGet+err` arms (`push imm; jmp common`);
  orig did not. Stop: identical-block merge, not a type miss.

- **0x10040040 (158 B, 45 diffs /O2).** First COM is thiscall 0-arg
  (`call [vt+0x1c]`) — matches. Second is thiscall 1-stack: orig
  `mov eax,[ecx]; push 1; call [eax]`; ours `push 1; xor edx,edx;
  call [ecx]` (dummy edx from `__fastcall(this, int, int)`). `strcpy`
  twice is already scasb/movsd. Stop: dummy-edx coloring.

- **0x10002460 (252 B, 126 diffs /O2, short-12).** `memset`/`memcpy`
  restore `rep stosd`/`rep movsd`. Remaining: CSE of `DAT_1021c654/658`
  out of the per-car loop (orig re-derefs every iter — they sit inside
  the same 0x118-byte global the dest pointer walks); pointer kept in
  ebp vs reloaded eax. Generator: `memset`/`memcpy` for Ghidra's
  counted 0-store / copy loops.

- **0x10005330 (194 B, ~35–41 diffs /O2).** `push -1` is
  `WaitForSingleObject(..., INFINITE)`, not SEH. `g++; if (g >= 0x1b)`
  is `inc; cmp eax,0x1b; jl` not Ghidra `if (0x1a < g)`. Missing
  `DAT_10af3bb4` as a separate char arg (Ghidra CONCAT). GetF02C
  cleanup is `add esp,4` with two pushes.

- **0x1006A330 (177 B, 41 diffs /O2).** Zero-register ebx for a
  burst of 0-stores. Orig stores then re-derefs the mutex for
  `ReleaseMutex`; ours reloads first. Allocator/reload timing. Not EH
  (`push -1` is INFINITE).

- **0x10036220 (224 B, 171 diffs /O2, was 186).** stdcall 4 args
  (`ret 0x10`). COM methods are thiscall with stack args (`ecx =
  g+0x3838; call [vt+0x10]` / `+0x28`). `test byte [param_3],1` and
  `mov byte [param_3],al` via `*(unsigned char *)&param_3` — that
  `&param` forces an ebp frame orig does not have. Not EH.

- **0x10074A30 / 0x10074960.** CRT `_DllMainCRTStartup` / `_CRT_INIT`.
  Function pointers and `BrDllMain` are `__stdcall`. `initterm`,
  dllimport `_adjust_fdiv` is two loads (`mov ecx,[IAT]; mov edx,[ecx]`).
  In-scope (lives in BRGlide.dll) but CRT shape.

## Generator candidates

1. **Ghidra empty `int f();` + call-with-args whose orig is
   `mov ecx,this; push arg; call` (no `add esp`).** Rewrite callee as
   `__fastcall(this, struct { T v; })`. Sweeps bitstream, COM thiscall
   with stack args. Distinguisher: `mov ecx,reg; push; E8` vs `push; E8;
   83 c4`.

2. **Ghidra `(**(funcptr *)(*p + N))(...)` cdecl vs orig `push this;
   call [vt+N]` (stdcall COM, this on stack) vs `mov ecx,this; call
   [vt+N]` (thiscall).** Distinguisher: ecx=object vs ecx=vtable vs
   this pushed. Already listed in VC5-IDIOMS-structural.md; this batch
   confirms DirectInput CreateDevice (`+0xc`) is the stdcall shape.

3. **Ghidra counted `for (n; n; n--) *d++ = 0 / *s++` → `memset` /
   `memcpy`.** Restores `rep stosd`/`rep movsd`. Fired on 0x10002460
   (stosd + two movsd) and 0x100298C0 (0xaa-dword tile copy).

4. **Ghidra `if (K < x)` / `if (x < 3)` vs orig `cmp x,K; jle` /
   `cmp x,2; jg`.** Operand order and `<= 2` vs `< 3`. Mechanical with
   `_CMP_RE` plus a `n < (K+1)` → `n <= K` fold when orig compares
   against K.

5. **`if (a) X; else if (b) X; else Y` must stay split** when orig has
   two copies of X (fld-0, or a call). `||` / shared tail is the
   `short-N` class. Inverse of the identical-call *merge* idiom:
   merge only when orig has `push imm; jmp common`.

## N64 twins used

None. Pairing was not needed: Glide stdcall, thiscall bitstream, COM
vtable shape, and the `dec eax` switch were all readable from the x86.
