# SPEC C — SetVideo.exe user-region idioms

Proven against `build/match/orig_setvideo/<VA>.bin` with `exe_sweep.py`
(`/O2 /ML`). Fence Microsoft CRT (`0x402D20`–end except the three tiny
stubs already tagged). Do not touch the DLL pipeline.

## Method

Infer the source from the bytes. A 3–4 byte ecx/edx split that is
otherwise identical is a live-range coloring problem, not a spelling
problem.

## Proven this pass

- **FollowUse is `do { use = GetIniValue(p, "Use"); if (use != 0) { free; p = SetSubstituteDir(pini, use); } } while (use != 0)`.**
  Orig back-edge is `test esi,esi; jne` onto the single GetIniValue site
  (`p` in edi, `use` in esi, `pini` in ebx). `for (;;)` + `if (!use)
  break` is a `jmp` back-edge and duplicates GetIniValue (+16, 11 diffs).
  `while (use) { free; SetSubstituteDir; use = GetIniValue; }` also
  duplicates. Proven 0x00402360 (74 B).

- **Init the loop index while the pointer is still live.** VC5 prefers
  ecx for a loop counter and the first stack arg. If `i` is born *after*
  `pini` dies (`list = pini->list; for (i = 0; ...)`), they do not
  interfere: pini lands in ecx, list is copied to edx, ecx is reused for
  i — `mov ecx,[esp+14]; …; mov edx,[ecx]; xor ecx,ecx` (4 diffs on
  FindFirst, 3 on FindNext). If `i` is assigned *before* the list load,
  they interfere: i takes ecx, pini/list stay in edx —
  `mov edx,[esp+14]; …; mov edx,[edx]; xor ecx,ecx` (orig). Instruction
  scheduling still places the `xor ecx,ecx` *after* the list load.
  FindFirst: `if (pini) { i = 0; list = pini->list; for (; i < list->n; i++) }`.
  FindNext: `i = p->index + 1; list = p->pini->list;` then `while (i < n)`.
  Proven 0x00401560 (71 B), 0x004015B0 (70 B).

## Remaining user-region walls (do not grind)

- **0x00401150 CHK_FGets (217 B, ~126 diffs once the loop is a do-while).**
  `getc(p->fp)` macro is right (re-deref `p->fp` for `_cnt`/`_ptr`/
  `_filbuf`). Walk `buf` (no `s = buf` before the loop) so both pushes
  happen before the loads and `p` is loaded before `buf`. `while (i < n)`
  with `i == 0` duplicates the `n <= 0` test and loads `buf` before
  `push edi`. Remaining wall: loop-exit `return buf` merges with the
  nle0/CR+EOF `mov eax,esi; ret`, so the loop test inverts to
  `jge shared; jmp loop` instead of orig `jl loop` fall-through; EOF
  then sits immediately after the loop (`je +0x14`) instead of far
  (`je +0x73`). Orig keeps a separate fall-through epilogue after the
  loop and shares only nle0 with CR+EOF (`s = buf` then `return s`).

- **0x00402480 WinMain (2144 B, 644 diffs, extra −16).** One function;
  map-splits at 0x402822 / 294F / 2AC0 / 2BCE / 2CC0 are not C
  (`test eax,eax` / jump table). Prefix through Card= is opcode-identical
  except 4 stack-slot bytes at +0x266. Wizard half is coloring
  (`inc eax` vs `lea [ebx+1]`).

CRT in the user-region span (`0x402D20`–`0x4038D0` = 2,992 B): fence.
Already-matched stubs: `CRT_empty` 0x403140, `_setdefaultprecision`
0x405940, `_matherr` 0x406C85.

## CRT header

SetVideo.exe is /ML (static CRT): CRT calls are E8, not FF 15. Do not
`#define _CRTIMP __declspec(dllimport)`.
