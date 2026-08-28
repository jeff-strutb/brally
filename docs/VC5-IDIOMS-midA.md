# VC5 idioms — mid-size unmatched batch A (2026-08-27)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. Work lives in `build/ghidra_work/<VA>.c`.

BATCH: 0x10054A30 0x1006C990 0x1005E7B0 0x10001510 0x1005D3C0 0x100372B0
0x10009010 0x100695C0 0x1005D060 0x10011650 0x10011300 0x10010FB0

## Proven MATCH

- **DirectPlay HRESULT pair-chain, not a balanced switch.** Orig is a
  linear chain of signed 3-way nodes, two codes per node:
  `cmp HI; jg next; je hi_ret; cmp LO; jne default; ret LO; hi_ret: ret HI`.
  Source of one node:
  `if (hr <= HI) { if (hr != HI) { if (hr != LO) goto unknown; return lo; }
  return hi; }`
  Ghidra's `if (hr < HI+1)` uses the wrong immediate (`cmp 0x80004002; jge`
  vs orig `cmp 0x80004001; jg`). Last node after the 0x88770816 pair is
  `if (hr != 0x88770820) { if (hr != 0) goto unknown; return "DP_OK"; }
  return "DPERR_LOGONDENIED";` then `unknown: wsprintfA(buf, "0x%08X", hr);
  return buf`. Strings as literals (`mov eax, offset`). `wsprintfA` is
  dllimport. `extern char DAT_10ac3070[]`. Proven 0x100372B0 (933 B,
  MATCH /O2). **Generator candidate:** Ghidra `if (x < K+1)` binary trees
  on sparse signed tables → `if (x <= K)` with the orig cmp immediate;
  inner `== HI` / `== LO` rewritten as the `!= HI { != LO goto; return lo }
  return hi` nest so the `je` lands on the clustered hi-return.

- **BR_EMIT + repeated cross-jumped call + BrMat4.** Display-list write
  pointer is `extern BrDlCmd *DAT_106e7710` with
  `{ BrDlCmd *p_ = DAT++; p_->op = C; p_->arg = A; }`. Viewport helper is
  `FUN_1001cf90(DAT_106e7710++, 0,0,0, 0x3eb,0x3e9,0,0x3eb, 0,0,0,0,
  0x3eb,0x3e9,0,0x3eb,0)` (17 cdecl args, `add esp, 0x44`). Colour word
  `DAT_1184c484 & 0xffffff | 0xdc000000`. Matrix at DAT_106e7930 is
  row-major
  `[[0,0,-1,0],[-1,0,0,0],[0,-1,0,0],[0,0,0,1]]` stored as 16
  `mov dword` (compiler interleaves the first FUN_10034af0 pushes).
  `memcpy` of 0x40 is `mov ecx, 0x10; rep movsd`. Switch on
  `DAT_100b3014` with cases **4/10 first**, then **1/7**, then default
  (orig body order). Repeat `FUN_10011300` in each arm — the second call
  cross-jumps (`push imm; jmp common`). Ghidra hoisted the second call
  through temps (the `short` class). Proven 0x10011650 (847 B MATCH /O2;
  recomp 878 includes the jump/byte tables the orig map splits off).
  **Generator candidate:** BR_EMIT macro + "repeat the call, do not
  hoist the shared lea/temps" on Ghidra's temp+one-call form.

## Structural, not yet MATCH

- **0x10054A30 (999 B, 129 diffs /O2, +2).** thiscall `ret 0x14` (5 stack
  args). `if (name == 0) return 0`. Unsigned `*(ushort *)(this+0x1a92c)
  >= 100` (`cmp word, 0x64; jb`) — Ghidra `99 < x` is the wrong
  immediate. Clamp store 99. `if (copy_full != 0) strcpy; else {
  strncpy(..., 10); strcat(dst, DAT_10ac5dd0); }` with `extern char[]`.
  Slot stride `idx * 0x438`. Re-deref the ushort every store (no idx
  local — orig has no `sub esp`). Item vtable is `__fastcall(this)` at
  `this+idx*0x438+0x2c`, +8 if `type == 3` else +4. Menu vtable +0x2c is
  `__fastcall(this, edx, 0)` — pass the incoming `_edx_unused`, not
  literal 0 (`xor edx` was +3). `(int)*(float *)(this+0x20)` is `fld
  dword; call __ftol`. `*(float *)slot = (float)*rect` is `fild; fstp`
  not Ghidra's `(int)(float)`. First 0x362 bytes match. Residue: orig
  `cmp ax, si; ja` (esi 0-register, unsigned `n > 0`) vs our
  `test; je` on `n > 0 ? n : 1` then `and eax, 0xffff; fild [esp+0x1c]`
  (arg-home). 0-reg compare encoding wall — stop.

- **0x10010FB0 (843 B, 365 diffs /O2, −11).** Guard
  `DAT_106ed6b0 == 0 || DAT_100b3014 == 2 || == 8` (esi 0-register,
  `cmp eax, esi; je`). BR_EMIT of e7000000 / ba001402 then
  `FUN_1001cf90(DAT++, 0,0,0, 0x3ec,0x3e9,0,0x3ec, 0,0,0,0, 1000,0,0,0,
  1000)` — orig interleaves the emits with the 17 pushes. Frame orig
  `sub esp, 0x18` vs ours `0x10` (two ints DCE'd). Cascade from the
  8-byte frame miss. Same BR_EMIT generator as 0x10011650. Stop.

- **0x1006C990 (994 B, 223 diffs /O2, −466).** Ghidra shredded stdcall
  Glide / cdecl fread into `puStack_*` return-address slots. Reconstruct
  from orig: `fh = FUN_10003320(p1); FUN_100034c0(&w,4,1,fh);` twice more
  into `DAT_1184c488`; `nbytes = h*w*2`; `FUN_100035e0(fh)`;
  `if (p2 != 0 && checksum != p2) exit(1);` then `FUN_100281c0`,
  `FUN_10028200` 15 cdecl args (`add esp, 0x3c`), download, `grTexCombine`
  / `grColorCombine` `__stdcall`. `(W-0x100)/2` is signed `cdq; sub; sar`.
  Clamp `if (ox < 0.0f) ox = 0`. Head through the fread calls matches
  (`push eax; call FUN_10003320; add esp,4; mov edi,eax`). Frame orig
  0x110 vs ours 0x10c (one dword). Residue is the four GrVertex (0x40
  each) store burst + two `grDrawTriangle` pairs — x87 / store-order
  wall. Stop.

- **0x10009010 (913 B, 624 diffs /O2 /Oy-, −65).** Jump-table switch:
  `add eax, 0xa0000000; cmp eax, 8; ja; jmp [eax*4+table]` =
  `switch (*msg)` cases 0x60000000..08. Orig body order is
  0,1,2,5,3,4,6,7,8 — case 0x60000005 (the 4/5/6/7 FUN_1006ba60 chain)
  sits before HOSTSTARTED. Repeat the 4/5/6/7 call per arm. HOSTSTARTED
  `return`s (no pMem check). `if (hwnd != 0) PostMessage; else
  GlobalUnlock(GlobalHandle); GlobalFree(GlobalHandle)` — Ghidra inverted
  the hwnd test. Both FUN_1002f790 and FUN_100038f0 take the five stack
  args (orig pushes then `je` to pick the callee). Residue: orig
  `sub esp, 0x508` esp-relative (ebp is sprintf IAT in one loop); ours
  ebp frame even at /O2. ebp-frame / live-across-call coloring wall.

- **0x100695C0 (877 B, ~615 diffs /Od).** `if (param_1 == 0)` first
  (orig `test; jne fopen`). `rep movsd` of 0x53 dwords = memcpy 0x14c.
  fread IAT hoisted to ebp at prologue (`mov ebp, [imp_fread]`). Fail
  path `mov al, [param_2]; test al; setne al` — `return (char)param_2
  != 0`. `ret` not `ret N`, char return (`mov al,1`). /O2 should be the
  /Oi memcpy; /Od wins on diffs because the fopen/fseek path still
  diverges (IAT coloring of fseek/ftell/fclose plus the
  `(DAT_105ccbc4^1)*0xada` pointer-array restore). Stop.

- **0x1005D060 (856 B, 572 diffs /O2, +16).** thiscall `ret 0xc` (3
  stack args). Zero-register orig ebx (`xor ebx, ebx; cmp esi, ebx`).
  `if (param_4 == 0) { param_4 = DAT_106eed48; param_3 = 0; }`.
  `if (param_2 == 0)` zeros four globals; `if (param_2 > 8 || (*flags & 1))
  return 0` (`jg` signed). FUN_10034620 is BrVec3Lerp with `0.2f`
  (`0x3e4ccccd`). Recursive thiscall reloads ecx from the saved-this
  slot. Tail of orig is `jmp` to the post-loop OR (not a trailing-call
  tail jmp — it's the `DAT_10b1cf0c = 1; goto done` outline). Residue:
  ebx vs edi 0-reg coloring. Stop.

- **0x1005E7B0 (979 B, 725 diffs /O2 /Oy-).** thiscall 0-stack
  (`mov esi, ecx; call FUN_1006fd90` with ecx still this — no reload).
  `xor edi, edi` 0-register; `cmp eax, 2/4/3; cmp eax, edi` for `== 0`.
  Both arms call FUN_1006fcb0 (thiscall + 1 stack, `push eax; mov ecx,
  esi; call` — no `xor edx`). `float FUN_100023e0(float)` /
  `FUN_10002560` (sin/cos); empty `int f()` would `fstp qword`. Named
  float temps for the four trig results force an ebp frame (`and`/`sub
  esp` vs orig `sub esp, 0x14`). Inlining them into FUN_1006f680 did
  not restore the 0x14 frame (nested x87 still wants ebp). Float-frame
  wall. Stop.

- **0x10001510 (977 B, 714 diffs /O2 /Oy-, −81).** thiscall `ret 8`.
  Orig `sub esp, 0x54; push ebx; push ebp; xor ebp, ebp; lea ebx,
  [ecx+0x28e0]` — ebp is the 0-register, ebx is `this+0x28e0`. Callees
  FUN_10034560 = BrVec3Sub, FUN_100347f0 = Length, FUN_10034310 = Dot,
  FUN_10034660 = MulAdd, FUN_10034390 = ScaleBy, FUN_100345c0 = Add,
  FUN_10034760 = Dist. `fcom [0.0]; test ah, 0x40` for `== 0.0f`.
  `setne cl; inc ecx` is `(a != b) + 1`. Named BrVec3 locals emit an
  ebp frame (`sub esp, 0x50`) vs orig 0x54 + xor ebp. Frame / 0-reg
  wall. N64 twin would name the collision helper but would not move the
  coloring. Stop.

- **0x1005D3C0 (937 B, 715 diffs /O2 /Oy-, −57).** thiscall 0-stack,
  orig `sub esp, 0x64; push ebx; push ebp; push esi; mov esi, ecx`.
  Orig hoists `(left - right)` as `fld [+0x64]; fsub [+0x8c]` before
  any vec call; Ghidra delayed the sub. Named BrVec3 locals force an
  ebp frame. Float-frame / DAG wall. Stop.

- **0x10011300 (846 B, 435 diffs /O2 /Oy-, −302).** Loop
  `while (n != 0)` over 0x20-byte records, FUN_10034920 (xform), then
  `fcomp` guards (`test ah, 0x41` / `test ah, 1`) and `(int)float` via
  `__ftol`. BR_EMIT of e7000000 / fa00ffff / b6000000 / de/df. Incomplete
  without the orig's four `__ftol` + screen-space integer web. Float /
  ftol-local wall (ebp frame). Stop.

## Generator candidates

1. **Sparse signed pair-chain** (0x100372B0 MATCH). Rewrite Ghidra
   `if (x < pivot+1)` as `if (x <= pivot)` with the orig immediate;
   nest `!= HI { != LO goto def; return lo } return hi`. Apply to any
   HRESULT / error-code string table that starts `cmp K; jg; je`.
2. **BR_EMIT + un-CSE identical calls** (0x10011650 MATCH). Display-list
   `{ p = G++; p->op; p->arg }` plus "repeat the call per arm" on
   Ghidra's temp+one-call form. Same family as 0x10010FB0 / 0x10011D20.
3. **Unsigned `>= K` not Ghidra `K-1 < x`** (0x10054A30 head). Orig
   `cmp word, 0x64; jb` is `>= 100`, not `> 99`. Same class as the
   pair-chain immediate.
