# VC5 idioms — t2 session (four large functions)

Proven against BRGlide.dll orig bins, 2026-08-27. None of the four
matched. Numbers are of that function's `.text` bytes.

## 0x10006510 BrCarStateEncode — 1018 B, not matched

Shape is a bitstream writer, not a packer. Orig:

- cdecl `(pBs, pSrc)`, loads pSrc first.
- `BrBitStreamWriteBits` is **thiscall, two stack args (value, nBits),
  ret 8**: `push nBits; push value; mov ecx, pBs; call` with no
  `add esp`. Header cdecl 3-arg is the wrong convention.
- nBits immediates are hoisted *before* the packer call (`push 8;
  push float; call packer; add esp, 4; … push packed; mov ecx;
  call WriteBits`).
- Width of each shift is the packer's live register, then movsx into
  WriteBits' int: `sar ax, 8` / `sar ax, 1` (S16), `sar al, 3|4|2`
  (S8/S6). `(int32_t)x >> n` is `sar eax`. Integer promotions make
  `(int16_t)x >> n` `movsx; sar eax` in this TU because the packers
  are *defined* here as int32_t — changing those returns would
  unmatch BrCarStatePack.
- Flags are `(f != 0.0f)` (fld/fcomp/test ah,0x40), not a helper
  call. Last flag has two epilogues: true path `push eax; push eax`.

**Caller-side thiscall wall.** VC5 C has no `__thiscall`. 
`__fastcall(this, struct val, int nBits)` puts nBits in edx.
`__fastcall(this, int dummy, struct val, int nBits)` keeps both
stack args but emits `xor edx,edx` at every site (~2 B × 32).
That residue is not source-reachable in C.

## 0x10032880 BrCarWheelFx — 1464 B, not matched

- **thiscall**, `mov edi, ecx`. Port's `(pCar, pEnv, pSeed)` is
  cdecl 3-arg. pEnv fields are **globals** (Glide: pRecs
  `0x106eed38`, mode `0x106ed6b0`, sel `0x100b3014`, dt
  `0x106e9d8c`, flag `0x106ed6b4`). pSeed is not an argument:
  rand at `0x100353d0` is 0-arg (`call` with no push).
- Frame `sub esp, 0x70`, ebp is the zero register.
- Wheel bases hoisted *before* the loop as a 4-pointer array
  `{car+0x994, +0x57C, +0x370, +0x788}` plus walking pointers
  for soa/pos/prev/s16/tag/pF. `for (i=0;i<4;i++)` scaled from
  `aWheelOff[i]` is a different frame (0x64 vs 0x70).
- `(int)float` is `call 0x10074560` (__ftol thunk, E8). A
  `BrFtolTrunc(float)` cdecl wrapper is an extra call.
- `and eax, 0xffff; fild dword` needs a **signed** 32-bit temp.
  `rand() & 0xFFFFu` (unsigned) is `fild qword`.

Not a wall: missing the pointer-array hoist.

## 0x1001D8A0 BrDxDetect — 924 B, not matched

Port's `BrDxHost *` vtable is not the original. Orig:

- cdecl **two** args: version then platform. Frame **0x114** =
  5 pointers + DDSURFACEDESC 0x6C + OSVERSIONINFOA 0x94.
  Handles/proc addrs are esi/ebp/ebx/eax, not extra slots.
- Win32 is `__declspec(dllimport) __stdcall` (`FF 15`). LoadLibrary
  IAT is CSE'd into ebp, GetProcAddress into edi.
- COM is **C stdcall** (this on the stack), not C++ thiscall:
  `push ppv; push iid; push this; mov ecx, [this]; call [ecx]`.
  Release: `push this; call [vtbl+8]`. SetCooperativeLevel
  `[vtbl+0x50]`, CreateSurface `[vtbl+0x18]`.
- Five NULL stores (edi=0) in order pDD, pDD2, pDDS, pDDS3, pDDS4
  with `&osvi` pushed *before* the stores.
- ebx holds platform until DirectDrawCreate succeeds, then is
  reloaded with the version pointer.

Frame 0x114 is reachable. Remaining is register coloring
(version pointer ebp vs ebx) and spilling GetProcAddress — not
SEH (`push -1` / `fs:[0]` is absent).

## 0x1003CD60 BrOpt3810 — 490 B, not matched

- cdecl 1-arg, ebx zero register, four rets.
- `if (AA2894 == 0) goto check; if (A9D000 != 0) goto psub;
  goto leave_host;` keeps leave_host at the **end**. Nested
  `if (AA2894) { if (!A9D000) { leave; return; } }` inlines it.
- GameSub slot 6 is **thiscall + one stack arg**
  (`push 0; mov ecx, this; call [vtbl+0x18]`). Phase slot
  `+0x1C` is **thiscall, no arg** — not `f00(pObj, 1)`.
- `BrSub1006A4A0` is thiscall + one pointer stack arg
  (`mov ecx, &g_aBrB4DF30[0]; push g_aBrB4FBE8; call`).
- Session-desc dispose is KERNEL32 IAT: GlobalHandle CSE'd into
  esi, GlobalUnlock/GlobalFree `FF 15`. Not the port wrappers.
- Slot walk compares the cursor as a **signed int** to the end
  address (`cmp eax, 0x10ac58f0; jl`). Pointer `<` is `jb`.
- `(dwCurrentPlayers > 1)` is `mov eax,1; cmp eax, edx; sbb;
  neg`.

Not a wall. First-divergence once the gotos landed was
`push ebx` vs `xor eax; push eax` for the slot-6 zero.

## Not walls

None of the four is `push -1` / `fs:[0]` SEH, and none is a
pure x87 DAG scheduler wall. Encode's thiscall-caller `xor edx`
is the only residue classified as unreachable from C.
