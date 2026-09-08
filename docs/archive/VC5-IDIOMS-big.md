# VC5 idioms — large UNTRANSCRIBED functions (2–4 KB)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. N64 twins were not required (x86 named every construct).
PC-only surfaces (BossRally.ini, DirectInput) have no N64 twin.

## Proven MATCH

- **`strcpy`/`strcat` of path + `.ini`, then CHK_* + else-if `strncmp`.**
  Ghidra explodes `/Oi` strcpy/strcat into counted memcpy loops (frame
  `0x108` vs orig `0x100`, `long+383`). Spell `strcpy(dst, base);
  strcat(dst, name)` with `extern char dst[]` / `extern char base[]`.
  Cmdline half: `strstr` + `atoi(p + strlen(key))` — `strlen` of a
  **string literal folds** (`add p, 12`, function `short-215`). Keys must
  be `extern char s_NetworkPlay[]` so `/Oi` emits `repne scasb`.
  `ReadJoystick=` is `switch (n) { case 1: case 2: case 3: default: }`
  (`dec eax; je` chain). Dir keys: `strcpy(dst, line+N);
  dst[strlen(dst)-1] = 0` (no `len>0` guard; `not ecx; mov [ecx+dst-2], al`).
  `D3DDrawCarShadow=` is `(atoi(line+17) == 0)` (`neg; sbb; inc`).
  Proven 0x10007F40 (2071 B, MATCH /O2).

## Structural, not yet MATCH

- **IDirectInputDevice COM is stdcall GetDeviceState/Acquire/Poll.**
  Ghidra `typedef int (*funcptr)()` is cdecl (`add esp` after `call [vt]`).
  Orig: `push buf; push cb; push this; call [edx+0x24]` (no cleanup).
  `vt+0x24` = GetDeviceState, `vt+0x1c` = Acquire(this), `vt+0x64` = Poll(this).
  Keyboard `cb=0x100` (`shl idx, 8`), joystick DIJOYSTATE2 `cb=0x110`
  (`idx*0x110`), mouse `cb=0x10`. HRESULT `0x8007001E` (DIERR_INPUTLOST)
  then Acquire and retry. Two zero registers (`xor ebx,ebx; xor ebp,ebp`).
  Ghidra `* 0x40` on a `char` DAT is a 64-byte stride; orig `shl eax, 8`
  is 256 — type the buffer as `char` and write `idx * 0x100`.
  `unaff_ESI/EBP/EBX` after GetDeviceState(16) are DIMOUSESTATE lX/lY/lZ
  (Ghidra dropped the out-buffer). Frame `sub esp, 0x110` = 16-byte mouse
  + 256-byte sprintf (`"fps: %.2f"`). 0x100706D0 (4145 B): prologue
  matches through `xor ebx/ebp` and `sub esp, 0x110`; first_div `0x34`
  (`mov [esp+0x10], ebp` hoist of a later `local = 0` vs orig
  `mov edx, [ecx]` vtable load). 2105 diffs, short-1009. Next: stop the
  0-store hoist (block-scope the analog locals).

- **Human controller thiscall, zero in ebp.** 0x1005C8B0 (1951 B): orig
  `mov eax, [flag]; sub esp, 0x30; push ebx; push ebp; xor ebp,ebp;
  push esi; cmp eax, ebp; push edi; mov esi, ecx`. After real
  `float` BrVec3 prototypes (empty `int f();` promotes float args to
  double) the push/xor/thiscall shape matches; first_div `0x7` is the
  `sub esp` immediate (`0x30` vs `0x24`). 1485 diffs, short-47. Missing
  12-byte slot is a live vec3 Ghidra shredded (`auStack_c[12]`) that /O2
  packed away. Hex float immediates (`0xc1700000`) passed to a `float`
  param are int-to-float; they are `fld` constants (`-15.0f`, `27.0f`).

- **Lap save/restore thiscall.** 0x1005F6C0 (2104 B): orig `sub esp, 0xd8;
  mov eax, [track]; push ebx; push ebp; mov ebp, ecx`. Unprototyped
  `BrEntSetPos`/`BrEntSetVel`/`BrPodNop` with float args emit `fstp qword`
  and `and esp,-8`. Real protos (`void BrEntSetPos(void *, int, float);
  void BrEntSetVel(float, float, float); void BrPodNop(void *, int, int,
  int, int)`) kill the aligned prologue. Restoring-lap `%d` args must be
  int bits, not floats. Remaining: `sub esp, 0xe4` vs `0xd8` (first_div
  `0x2`), this in edx vs ebp, 1606 diffs, long+507. Extra 12-byte frame
  is leftover shredded locals (`local_98[152]` + two vec3s).

- **Per-frame car sound, thiscall.** 0x10061470 (2757 B): orig
  `sub esp, 0x2c; push ebx; push ebp; push esi; mov esi, ecx; mov ecx,
  [player]; push edi`. Ghidra `double` x87 temps add 8 bytes (`0x34` then
  `0x30` after retype). first_div `0x2`, 2169 diffs, long+63. `ftol()`
  with no arg is a dropped `(int)float`. Unprototyped `BrVec3Length()`
  (Ghidra dropped the arg) vs `float BrVec3Length(void *)` is a compile
  fork — keep empty proto until the arg is recovered from the bytes.

- **Scene object DL emit.** 0x1000CBA0 (3971 B): orig `sub esp, 0x68`.
  Mixing `BrDlCmd *` (`++` = +8) with `uint *` (`+ 2` = +8) is required;
  `*DAT_106e7710 = imm` on a struct is C2115. Frame `0x60` vs `0x68`
  (first_div `0x2`), 2935 diffs, size within 5 bytes (3976 vs 3971).
  Ghidra `--(expr)` is unary minus. Wall: 8-byte frame + emit-order.

- **Scene sort / clip helper.** 0x1000E320 (1992 B): orig `mov edx,
  [hi]; sub esp, 0x14; push ebx; mov ebx, [lo]; ... xor edi, edi;
  cmp edx, ebx`. Frame matches (`0x14`). first_div `0xa` is bound-register
  coloring (orig ebx = `DAT_10ac2c54` loaded between `push ebx` and the
  other pushes; ours edi after all four). Zero in edi vs ebp. 1355 diffs,
  long+123. Allocator coloring of (hi, lo, 0) — residue-ceiling class.

- **Render-frontier projector.** 0x10017110 (2039 B): orig `mov eax,
  [flag]; sub esp, 0x2c; test eax, eax; push ebx; mov ebx, [flag2]`.
  first_div `0x0` after an ebp-frame variant; /O2 is `sub esp, 0x24`
  (8 short) then short-263. Cross-jumped DL emits CSE'd by Ghidra
  (same class as 0x10023AA0). 1186–1201 diffs.

## N64 twins used

None. 0x10007F40 is PC `.ini` + cmdline (no N64 file). 0x100706D0 is
DirectInput. The scene-DL trio (0x1000CBA0 / 0x1000E320 / 0x10017110)
is the N64 gfx display-list lineage but the x86 bytes already named
the constructs (RDP commands, `BrRdpSetCombineLERP`).
