# VC5 idioms — mid-size unmatched batch B (2026-08-27)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. N64 twins were not required (x86 named every construct).

BATCH: 0x100119C0 0x10069A80 0x10036B20 0x1006AB80 0x10055F40 0x1005ACE0
0x100311C0 0x10067C30 0x1006A7E0 0x1002CB49 0x1000DC00 0x100054A0

## Proven MATCH

- **Display-list emit is `int *p = DAT; DAT = DAT + 2; *p = op; p[1] = arg;`
  with a 17-int `BrRdpSetCombineLERP` prototype.** Empty `int f();` is fine
  (all args are ints). Zero-register ESI is triggered by the many `arg = 0`
  stores plus CombineLERP's right-to-left 0-pushes; `p[1] = 0` becomes
  `mov [eax+4], esi` not an imm store. `if (g != 0)` against that 0-reg is
  `cmp [g], esi; je`. Proven 0x100119C0 (837 B, MATCH /O2).

- **`(float)int_ptr[i]` is `fild`, not `fld`.** A float field loaded through
  `int *p` plus a `(float)` cast converts an integer; orig `fld dword [ebx+N]`
  is a float-typed pointer (or a struct of floats). `memcpy(dst, p, 0xA0)` is
  `rep movsd` of 0x28 dwords. Signed `x % 4 == 0` is the cdq / xor / sub /
  and-3 / xor / sub chain interleaved with the zero-stores. Comparison
  polarity is `if (p[i] >= k)` → `fld p[i]; fcomp k; test ah,1; jne`.
  Proven 0x100054A0 (691 B, MATCH /O2).

## Structural, not yet MATCH

- **0x1005ACE0 (776 B, 50 diffs /O2, size −8, frame exact 0xc0).** thiscall.
  `if (DAT_105ccb88 != 0)` first (orig `test; je` into the mode checks;
  Ghidra `== 0` first). Real `float` prototypes on BrMat4RotateAxis /
  BrMat4Mul / BrMat4TransformPoint (empty proto promotes the
  `*(float*)(p+0x73c) * k` product to double and grows the frame 4 bytes).
  Three 64-byte matrices, no extra `int tmp = p+0x220` (that slot is the
  +4). Residue is the tail-4 `fsub k` of wheel Y: orig four `fld` then
  `fxch`/`fsub`; ours `fsub`s as it goes. Named temps for that block spill
  a 4-byte slot (`sub esp, 0xc4`). x87 DAG wall — stop.

- **0x100311C0 (770 B, 200 diffs /O2, +5).** `DAT_10ac080c = 0x80025c00 -
  (int)&DAT_106eff08` (Ghidra folded to 0x6f935cf8; orig `mov eax,
  0x80025c00; sub eax, offset`). `strcpy(buf, "tracks/"); strcat(buf,
  names[i])` not exploded scasb. `if (cb > 4000000)` / `if (n > 0x800)` /
  `if (hdr != 0x230)` not Ghidra's flipped `if (K < n)`. `float
  BrVec3Length` (Ghidra `double` forced ebp). Hoist `i = 0` and the count
  load above the two `= -1` stores. Residue is the instance-loop vec
  `{1,0,0}` store vs call-arg interleave. Loop-body scheduling wall.

- **0x10067C30 (762 B, 209 diffs /O2, −10).** param_2 is a **pointer**, not
  Ghidra `float` (Ghidra reused the incoming slot for dt = 0x3d088889 =
  1/30). Frame `sub esp, 0x4c` = 3 floats + 64-byte matrix, no extra
  `local_8` — Ghidra's `local_8 -= f1E8` is `mat[14]` (m[3][2]). Scale
  0.1f stores are **2,1,0**. Reciprocals are `fld ext; fdivr const` via a
  named denominator. `1/120` bits are `0x3c088889` (`0.008333333f` is
  0x3c088888, 1 ULP off; `1.0f/120` is exact). memcpy 0x44 = 0x11 dwords.
  Compare `m22 >= 0.5f` in **both** pfn arms so fnstsw sits after the
  integer `cmp [car+0xf08], FUN_1005e690`. Residue: orig issues `fcomp`
  before the memcpy; ours after. x87/integer interleave wall — stop.

- **0x10036B20 (805 B, 413 diffs /O2, −21, frame exact 0x50).** DirectPlay
  stdcall COM at `[vt+0x38]` arity 5 (`this, recs, count, buf, &size`),
  HRESULT `0x8877001E`. One 0x50-byte frame: `{unsigned size; char *id;
  struct { int guid[4]; int len; char *p; } rec[3]; }`. `memcmp(id,
  &GUID, 0x10)` is `repe cmpsb`. `if (hr >= 0)` not `if (-1 < hr)`.
  Residue: hr in edi vs esi, 21 missing bytes in the first GUID arm
  (store order of len/p vs strlen). Allocator + arm-size wall.

- **0x10055F40 (799 B, 649 diffs /O2, +27, frame 0x418 vs 0x41c).** thiscall.
  Four `memset(buf, 0, 0x104)` are `mov ecx, 0x41; xor eax,eax; lea edi;
  rep stosd` (Ghidra exploded to byte stores). `strcpy` / `strncmp` /
  `strcmp` / `atoi` / four `_itoa(n & mask, num+i, 10)` are real (the 4
  itoa of 0xff000000/00ff0000/0000ff00/000000ff into consecutive bytes
  are in the orig, not a Ghidra artifact). Residue: frame 4 bytes short
  and this-save vs esi push order. Slot-count wall.

- **0x10069A80 (828 B, 581 diffs /O2, −12).** `extern char mode[]` (push
  offset). `char` return (`setne al` after `or eax,-1` on the fail path).
  Two flag bytes `DAT_105bc8e0` / `DAT_105bc8e1`, not one dword `& 0x1f`.
  `BrReplayGetBuf2()` is **0-arg** (Ghidra invented a size arg from
  fread/adler's already-pushed nmemb). Adler32 `FUN_10001000`. Orig CSEs
  `4` into edi (`mov edi, 4; push edi; cmp eax, edi`); a `unsigned k = 4`
  still folds to `push 4`. Residue: that CSE plus 12 missing bytes.
  Imm-vs-reg CSE wall.

- **0x1006A7E0 (748 B, 552 diffs /O2, +17).** Countdown is **integer 15**
  in ebx (`mov ebx, 0xf; dec ebx`), not Ghidra's `float 2.10195e-44`.
  Index stored as `mov [edi-4], ebx` (int), value as `fild qword` of the
  unsigned payload. `if ((flags & 0x3f) >= 2 && < 5)`. Residue: count `n`
  in ebp (`xor ebp,ebp; inc ebp`) vs a stack slot; frame 0x1c vs 0x20.
  0-reg/count coloring wall.

- **0x1006AB80 (803 B, 637 diffs /O2, +18).** Bases are immediates
  `0x11849f30` / `0x117b3258` / `0x117a9bb4` (Ghidra's g_1826BD0 was
  wrong). `if ((flags & 0x3f) >= 1)` is `cmp eax, 1; jl` not `!= 0`.
  `strcpy` for the 0x52c-offset name. WaitForMultipleObjects is stdcall
  (cached IAT in ebx). `unaff_EBX` was `*local_43c` as the second handle.
  Residue: esi vs ebp 0-reg, frame 0x440 vs 0x448 (one HANDLE[2]
  overlay). 0-reg + overlay wall.

- **0x1000DC00 (702 B, 512 diffs /O2, +8, frame exact `sub esp, 8`).**
  `{int *p; int n;}` is one struct (Ghidra shredded to local_8/local_4).
  `FUN_1000e150(..., 0x44800000, ...)` is int 1024.0f bits, not a float
  (empty proto would `fstp qword`). Residue: orig `edi = DAT; xor ebx,ebx`
  (pointer in edi, 0-reg ebx); ours pointer in ebx, 0-reg ebp. 0-reg
  coloring wall.

- **0x1002CB49 (744 B, 493 diffs /Od, −160). PARKED allocator-input.**
  Orig is /Od (`push ebp; mov ebp,esp; sub esp, 0x20; add ecx,1; imul
  0x24` every use; re-deref DAT). `if (g==0 || mode==2 || mode==8)
  flag=0 else flag=1` stores **1 first** (three `je store0`).
  `(unsigned char)((flags >> 0x14) & 1)` is `shr 14; and 1; and 0xff`.
  Extra locals (0x28 vs 0x20) and the 0xff widening are the parked
  mystery. Do not sink time.

## Generator candidates

1. **Exploded `repne scasb`/`rep movsd`/`rep stosd`/`repe cmpsb` →
   `strcpy`/`strcat`/`memset`/`memcmp`.** Hits 0x100311C0, 0x10055F40,
   0x10036B20, 0x1006AB80, 0x10069A80. Same class as VC5-IDIOMS-dll.md.

2. **`if (K < n)` / `if (-1 < hr)` → `if (n > K)` / `if (hr >= 0)`.**
   Distinguisher `cmp n, K; jle` vs `cmp K, n; jge`. Hits 0x100311C0
   (size > 4e6, instances > 0x800), 0x10036B20 (`hr >= 0`), 0x1006AB80
   (`(flags & 0x3f) >= 1` = `cmp eax,1; jl`).

3. **`if (x != 0)` fall-through first.** Orig `test; je else`. Ghidra
   nests `if (x == 0)`. Hits 0x1005ACE0 (DAT_105ccb88), 0x100119C0
   (DAT_106ed6b0 vs 0-reg).

4. **`(float)*(int *)` / `(float)p[i]` on an `int *` is `fild`.** Retype
   the pointer (or the struct field) as float. Hits 0x100054A0.

5. **Ghidra-folded `0x80025c00 - &sym` must be spelled with `&sym`** so
   the sub survives. Hits 0x100311C0.

6. **Exact float immediates:** `1.0f/120` = 0x3c088889; a decimal
   `0.008333333f` is 0x3c088888 (1 ULP). Pass the bits as `int` when the
   callee is unprototyped-int, or write `1.0f/120`. Hits 0x10067C30.
