# VC5 idioms — BRGlide.dll structural batch 2 (2026-08-27)

Proven against BRGlide.dll orig bins. Infer source from the bytes; never
permute spellings. N64 twins were not required (x86 named every construct).

## Proven MATCH

- **`if (dir != 0)` fall-through first, wrap with `>` not Ghidra `<`.**
  Orig `test dir; je else` then `inc; cmp n, K; jle; mov n, 0`. Ghidra
  prints `if (dir == 0)` first and `if (K < n)`. Spell
  `if (dir != 0) { n = n + 1; if (n > K) n = 0; } else if (rev) { n = n - 1;
  if (n < 0) n = K; } out = tab[n]; return 1;`. Hoist `dir = DAT` above a
  later zero-store when orig is `xor ecx; cmp eax, ecx; mov [g], ecx`
  (zero-register). Proven 0x1003CAE0 / 0x1003CBA0 / 0x1003CB40 (93 B,
  MATCH /O2) and 0x1003C2B0 (92 B, MATCH /O2; wrap at 9, mid-return on
  overflow). Same family as 0x1003C240.

- **`if (n <= K)` not Ghidra `if (n < K+1)`.** Orig `cmp esi, 8; jg`.
  Literal 0 as MessageBox uType (no `uType = 0` local) hoists `push 0`
  before the caption `BrStrGet`. Table is `struct { int fatal; int strid; }`
  indexed `esi*8`. Proven 0x100378C0 (85 B, MATCH /O2).

- **Four field temps, store index, then store the four, then call.**
  Ghidra stored each field as it loaded (no esi/edi, ebp frame).
  `b=p[1]; c=p[2]; d=p[3]; a=p[0]; g_idx=idx; g0=a; g1=b; g2=c; g3=d;
  f(param);` keeps all four live and pushes esi/edi. Proven 0x100382D0
  (78 B, MATCH /O2).

- **`strcpy` is `/Oi` `repne scasb`/`rep movsd`; re-deref the mutex.**
  Ghidra exploded strlen+memcpy (`long+33`) and cached `hMutex`. Spell
  `strcpy(dst, src)` with `extern char dst[]`. `push -1` is INFINITE, not
  EH (no `fs:[0]`). Proven 0x100038A0 (77 B, MATCH /O2).

- **`return f(...) != 0` without a temp is `neg; sbb; neg`.**
  `i = f(...); return i != 0;` (and `i ? 1 : 0`, `!!i`) emit
  `xor r,r; test eax; setne r; mov eax, r` (+3). The call in the return
  expression is the sbb form. `return f(...) == 0` is `neg; sbb; inc`.
  Proven 0x1006BAA0 / 0x1006B6E0 (75 B, MATCH /O2) and 0x1006BB10 (72 B,
  MATCH /O2). Callees are cdecl (`add esp, 8` / `add esp, 4`).

- **`if (x >= 0)` is `test r,r; jl`, not Ghidra `if (x > -1)`
  (`cmp r, -1; jle`).** Proven as the opening of 0x1006E130 (2 diffs
  leftover).

- **Outer `p = q` copy plus IAT of `free` in ebp.** Ghidra's one-walker
  form colored the IAT into ebx (hoisted before `push ebp`). Spell
  `p = table; do { row = p; for (30) { q = row; for (4) free; row += 10; }
  p = row; } while ((int)p < end);`. Proven 0x1005A420 (87 B, MATCH /O2).

## Structural, not yet MATCH

- **0x10001440 (88 B, 1 diff /O2, size exact).** Color-key blit of shorts:
  dest-src byte delta, key compared from the stack slot (`cmp cx,
  [esp+key]`, no bx load), pix temp (one load, store cx), shrink-wrap
  pushes after `if (h == 0) return`. `*(volatile int *)&param_4 = param_4`
  *before* the row loop is orig's `mov [home], eax` spill of height
  (without it the 4-byte store vanishes and everything after shifts).
  Residue: `mov [eax+edx], cx` vs `mov [edx+eax], cx` (SIB base/index).
  Named `char *dp` does not flip it.

- **0x1006E130 (71 B, 2 diffs /O2, size exact).** `param_1 >= 0 && < 8`,
  `n < 3`, cdecl `BrTex3dRecSet278` + cdecl fp `(*DAT_118ed1d8)`. Residue:
  `[edi+esi]` vs `[esi+edi]` on the re-deref of `param_2 + off`. Same SIB
  wall as 0x10001440.

- **0x10002C00 (67 B, 47 diffs /O2, +2).** CD track pick:
  `i = rand(); last = g; i = i * (last - 5) / 0x8000 + 3;` then
  `if (i < 3) i = 3; if (i > last) i = last;` loop while
  `last > 6 && i == cur`. Assign last AFTER rand (a function-scope last
  loaded before rand, or CSE of the global across the loop, costs `push
  edi`). `/ 0x8000` is the signed `cdq; and edx, 0x7fff; add; sar 0xf`
  (RAND_MAX+1). Residue: orig `lea edx, [ecx-5]; imul eax, edx` (rand
  stays in eax) vs ours `mov edx, eax; lea eax, [ecx-5]; imul eax, edx`
  (+2, then a positional cascade). `i *= n`, `(last-5)*i`, and the
  one-expression form all keep the extra mov. imul operand-order wall.

- **0x1002A7F0 (71 B, 42 diffs /O2 /Oy-, size exact).** BrMat4Translate:
  integer 0 / `0x3f800000` stores in orig order (nine zeros, tx, ty, three
  1.0f, tz, last 1.0f). A named `float one` forces an ebp frame. /O2
  hoists ty into edx instead of `mov edx, 0x3f800000` (the constant is
  rematerialized at the 1.0f stores). edx = ty vs edx = 1.0f allocator.

- **0x10037FA0 (92 B, 46 diffs /O2, +4).** Stack `this` (not thiscall of
  the function), `(int)this->fx` / `(int)this->fy` via `__ftol` at
  +0x3c/+0x40, thiscall callee `__fastcall(this, int edx, cmd, x, y)` at
  vt+0x14, method pointer saved to the incoming-this slot, unsigned
  `for (i = 0; i < (unsigned)g; i++)` with `jbe`/`jb` and `x += 0xc`.
  Residue: `xor edx, edx` dummy of `__fastcall` (same class as 0x10040040)
  and esi/edi swap of x vs the loop counter.

## Generator candidates

1. **Ghidra `if (x == 0) { dec-arm } else { inc-arm }` whose orig is
   `test; je dec-arm`.** Flip so the `!= 0` arm is first; `if (n > K)`
   not `if (K < n)`. Sweeps the 0x1003CAE0 wraparound family (three
   identical 93 B twins + 0x1003C2B0). Mechanical; same as 0x1003C240.

2. **Ghidra `i = f(); return i != 0;` → `return f() != 0;`.**
   Distinguisher: orig `neg; sbb; neg` (`f7 d8 1b c0 f7 d8`) vs
   `xor; test; setne; mov`. `== 0` is `neg; sbb; inc`. Fired MATCH on
   the three sound wrappers. Sweeps every `return helper(...) != 0`
   Ghidra split through a temp.

3. **Ghidra exploded `repne scasb`/`rep movsd` → `strcpy`.** Already a
   candidate in dll.md; fired MATCH on 0x100038A0 (mutex + strcpy).
   `push -1` next to WaitForSingleObject is INFINITE.

4. **Ghidra `if (n < K+1)` → `if (n <= K)`** whenever orig is `cmp n, K;
   jg`. Fired MATCH on 0x100378C0. Pair with dropping a `uType = 0` local
   so `push 0` hoists before the next call.

5. **Ghidra `if (-1 < x)` → `if (x >= 0)`.** Distinguisher: `test r, r;
   jl` vs `cmp r, -1; jle`. Opening of 0x1006E130 and similar range
   checks.

6. **Dead write-back `*(volatile int *)&param = param` before a loop
   that reuses the param's eax.** Orig `mov [home], eax` of an unmodified
   incoming arg; without it /O2 rematerializes from the slot and the
   store vanishes (4 bytes, then a positional cascade). Fired the 0x10001440
   spill; remaining 1 byte is SIB.

## Allocator / SIB / dummy-edx walls (do not grind)

| VA | diffs | size | class |
|---|---|---|---|
| 0x10001440 | 1 | exact | SIB `[eax+edx]` vs `[edx+eax]` |
| 0x1006E130 | 2 | exact | SIB `[edi+esi]` vs `[esi+edi]` |
| 0x10002C00 | 47 | +2 | imul eax,edx (rand) vs extra mov+swap |
| 0x1002A7F0 | 42 | exact | edx = ty vs edx = 0x3f800000 |
| 0x10037FA0 | 46 | +4 | dummy `xor edx,edx`; esi/edi of x vs i |

None of these is EH. 0x100038A0's `push -1` is INFINITE.

## N64 twins used

None. Wraparound menu toggles, MessageBox, mutex strcpy, DirectSound
wrappers, `free` walk, color-key blit, and BrMat4Translate (store list)
were readable from the x86. `guTranslateF` is the N64 twin of 0x1002A7F0
but would not have named the integer 0 / 0x3f800000 store order.
