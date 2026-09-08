# VC5 codegen idioms — structural residue (session notes)

Proven against BRGlide.dll this session. Merge into `docs/VC5-IDIOMS.md`.
Infer source from the bytes; never permute spellings.

## Proven this session

- **MCI_*_PARMS + string-as-`char[]` + zero-register.** Ghidra shredded
  `MCI_OPEN_PARMS` / `MCI_SET_PARMS` / `MCI_STATUS_PARMS` into overlapping
  `char[4]` locals and typed `"cdaudio"` / the sprintf format as
  `extern int` (push value vs push offset). Frame is STATUS(16)+SET(12)+OPEN(20)
  + `CHAR buf[1024]` = 0x430. Four `= 0` stores plus `mciSendCommandA(0,…)`
  plus result compares force `xor edi,edi`. Proven 0x10002980 (353 B, MATCH /O2).

- **BrMat4, not shredded vec3+w ints.** A 64-byte frame with stores at
  +0x0c/+0x1c/+0x2c/+0x3c (0,0,0,1) and +0x20/+0x24/+0x28 (0,0,1) is one
  `BrMat4`: last column plus the Z row. Ghidra emitted `char[12]` + `int`
  per row and empty `int BrVec3*();` prototypes. Real `float` prototypes
  (no default-arg double promotion) plus `BrMat4 m` matched. Proven
  0x10011D20 (383 B, MATCH /O2). N64 twin not required — the bytes named
  the 4x4.

- **`if (w > h)` fall-through vs Ghidra `if (w <= h)` first.** Aspect-ratio
  mapper (Glide `GR_ASPECT_*`): orig `cmp w,h; jle other` so the `(w > h)`
  block is first in the binary. Ghidra printed `<=` first, which also
  dropped the `push esi` prologue (frame@0xc). Exact-match arms `return 1`;
  in-between `return 0`. The `w>h` side has no `== 8` arm. Proven
  0x100275C0 (333 B, MATCH /O2).

- **PCMWAVEFORMAT must be a real stack struct.** Ghidra shredded it into
  four ints; `mmioRead(..., (HPSTR)&local_24, 0x10)` is only known to write
  the first dword, so /O2 kept a 0x18 frame vs orig 0x24 (16+20 with
  `MMCKINFO`). `if (cksize >= sizeof(PCMWAVEFORMAT))` is `cmp ,0x10; jb`
  (Ghidra's `0xf < cksize` is `cmp ,0xf; jbe`). Success `return mmioAscend`
  result, not a fresh 0; cleanup is `hmmio = 0; *phmmio = hmmio`. After
  those fixes the remaining 35 diffs are esi/edi coloring of hmmio vs
  wError (allocator wall — stop). Proven 0x1006FFC0 (425 B, 35 diffs /O2,
  size exact). Microsoft DX5 `wave.c` is the same function CSE'd: orig
  unified the PCM / extra-bytes alloc arms (one `GlobalAlloc(0, 0x12+cb)`).

- **COM 0-arg methods are thiscall (`__fastcall(this)`).**
  `mov ecx, [obj]; mov edx, [ecx]; call [edx+N]` — ecx is this, no stack
  args. Ghidra's `(**(funcptr *)(*p + N))()` is cdecl, no this.
  DirectInput `Poll`/`GetDeviceState` are the other shape: stdcall with
  this *on the stack* (`push this; call [vtable+N]`). Distinguish by
  whether ecx is the object or the vtable. Proven 0x100583C0 (thiscall
  Flip/Begin, 71 diffs leftover) and 0x100704E0 (stdcall Poll+GetState).

- **DIJOYSTATE2 + missing out-param.** Frame 0x110 = `sizeof(DIJOYSTATE2)`.
  Ghidra shredded it and dropped the `int *out` at `[esp+0x114]`.
  `rgbButtons` at +0x30; loop bound 0x80 is also the pressed mask
  (`test [esp+eax+0x30], cl` with `ecx = 0x80`). `0x8007001E` is
  `DIERR_INPUTLOST`. Proven 0x100704E0 (272 B; 152 diffs leftover —
  CSE of 0x80 into ecx still open).

- **/Od PackBits: one 0x20 struct in address order + local memcpy thunk.**
  Orig is `push ebp; mov ebp,esp; sub esp,0x20`. Ghidra inlined `memcpy` of
  the per-row length at /O2 (short-201); orig CALLs the 6-byte `jmp [memcpy]`
  thunk at 0x100746B4 (declare a cdecl `FUN_100746b4`, not dllimport memcpy).
  Count is `movsx`'d, `off++`, *then* `if (count < 0)` literals else
  `n = count+3; fill = movsx; off++; run`. Unused `param_2 = param_2`
  (`mov eax,[ebp+0xc]; mov [ebp+0xc],eax`). /Od slots are the 0x20-byte
  struct in address order: `{end, n, row, len, src, dest, k3, sbyte}` at
  ebp-0x20 .. ebp-4 (same "rebuild one struct of the frame size" idiom as
  0x1002DEC3). Decl-order permutation of loose ints does NOT move them.
  Proven 0x1002E5B9 (313 B, MATCH /Od).

- **32 KB `char buf[0x8000]` is `mov eax,0x8000; call __chkstk`.** Ghidra
  dropped the alloca (`in_stack_00007fdc`) and CSE'd stdcall Glide args
  into temps + a 0-arg `grAlphaCombine()`. Real `__stdcall` prototypes
  with the push order (3,8,1,1,0) restore the missing 33 bytes. The
  alloca buffer *is* the display-list (`DAT_106e7710 = buf`).
  Candidate: 0x10023B70.

- **unsigned-to-float at /Od is `fild qword` with a 0 high dword.**
  `(float)(unsigned)delta` stores delta at `[ebp-0xc]`, 0 at `[ebp-8]`,
  `fild qword [ebp-0xc]`, `fstp dword` temp, then `fld; fdiv`. That is
  why 0x1002E186's frame is 0x10, not an `__int64` local. Do not declare
  `__int64` just to get the fild.

## Generator candidates

1. **`extern int s_Foo` → `extern char s_Foo[]`** whenever the bytes
   `push offset s` (68 xx) not `mov r, [s]; push r`. Already an idiom;
   a refine transform that fires on `extern int s_` + a use as a
   pointer would sweep 0x100583C0, 0x100356B0, 0x10060A30, 0x100023F0,
   0x10036A30 and the 0x100089xx cluster. Mechanical.

2. **Ghidra 0-arg COM `(**(funcptr *)(*p + N))()` → thiscall.** If orig
   is `mov ecx, [obj]; call [vtable+N]` with no stack args, rewrite to
   `(*(T __fastcall *)(void *))(*p + N)(p)`. If orig `push this; call
   [vtable+N]`, use `__stdcall` with this as arg1. Distinguisher: ecx =
   object vs ecx = vtable.

3. **`if (a <= b) { X } else { Y }` whose orig is `cmp a,b; jle Y_label`
   with Y first.** Flip the if so the fall-through block matches the
   bytes. Same family as `_CMP_RE`; apply when the first-divergence is
   inside the opening compare and the two blocks are large.

4. **Shredded Win32 structs whose `lea`/`mmioRead` size exceeds the
   recovered local.** PCMWAVEFORMAT (16), MMCKINFO (20), MCI_*_PARMS,
   DIJOYSTATE2 (0x110), GUID (16). If `mmioRead`/`GetDeviceState` size
   is N and the pointed-to local is smaller, rebuild the struct.
   `sizeof` in the comparison recovers the type (`cmp ,0x10; jb` =
   `cksize < sizeof(PCMWAVEFORMAT)`).

5. **Large `char buf[0xN]` ↔ `mov eax, N; call __chkstk`.** When orig
   starts `B8 N 00 00 00 E8` to a  `__chkstk`-shaped helper, Ghidra's
   `in_stack_*` / missing frame is that array. Restore `char buf[N]`.
   0x10023B70 grew from short-33 / 240 B recomp to 279 B (orig 273) once
   the 0x8000 DL buffer and stdcall Glide arity were restored; remaining
   145 diffs are store-vs-push interleave around `grAlphaBlendFunction`.

6. **/Od frame-sized struct (extends 0x1002DEC3).** When /Od slot
   offsets permute under every decl-order of loose ints, pack them into
   one struct of the `sub esp, N` size with fields in address order
   (lowest address = first field = ebp-N). Proven 0x1002E5B9 MATCH.

## N64 twins used

None this session. 0x10002980 / 0x1006FFC0 / 0x100704E0 / 0x100583C0 are
Win32 (MCI / mmio / DirectInput / DDraw). 0x10011D20 / 0x100275C0 were
readable from the x86 (mat4 stores, aspect ladder). 0x1002E5B9 PackBits
would pair with a TGR texture unpacker if needed; the /Od bytes were
enough.

## Allocator walls (stop, do not grind)

- 0x1006FFC0 after the PCMWAVEFORMAT rebuild: 35 diffs, size exact,
  esi/edi swap of `hmmio` vs `wError`. Same class as BrTex3dRegister.
