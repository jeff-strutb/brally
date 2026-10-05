# Setvideo exe complete

*Recorded 2026-09-04.*

> SetVideo.exe game code is DONE as of 2026-09-03 (commits 86e5bca, 25aa852): 42/42 functions, 7,228/7,228 B of code in 0x401000-0x402D20, image_build 0 differing bytes. Everything >= 0x402D20 is fenced static CRT.

## SetVideo.exe - COMPLETE 2026-09-03

**Second fully-decompiled in-scope binary, after BRally.exe.** Game code
`0x401000` - `0x402D20`: **42 / 42 functions, 7,228 / 7,228 B** (+228 B of
inter-function alignment padding = the whole 7,456-byte span).
`tools/image_build.py` assembles `SetVideo.exe` with **0 differing bytes**.
Matched total in `.text` is 7,251 / 36,864 B (19.7%) - the extra 23 B are the
three CRT stubs `CRT_empty` / `_matherr` / `_setdefaultprecision`.

The remaining **27,888 B / 289 map rows at or above `0x402D20`** are
statically-linked MSVC 5.0 CRT: fenced by `CRT_START['setvideo']` in
`tools/progressmap.py`, reproduced by linking, **not a decomp target** (same
category as BRD3D's static CRT under rule 0). `config/fenced_exe.csv` needs no
SetVideo rows - nothing CRT sits below the boundary.

**Map defect fixed in the same pass:** `config/functions_setvideo.csv` had
WinMain truncated at 930 B with 11 further rows split out of its body at
non-prologue boundaries (`WinMain_radio`, `WriteDefaultINI`, `WinMain_jtab`,
…). Those rows made the progress map report 1,212 B of phantom unfinished game
code. Dropped; `0x00402480` now carries its real 2,144 B. 342 → 331 rows.
**Generalise: before believing an EXE map, check whether its rows split a
function at non-prologue boundaries** - `WinMainCRTStartup` at `0x4038D0` is
split into 7 the same way, and BRally has the same class.

## The last wall: WinMain 0x00402480 (2,144 B, 644 → 0)

Read as a coloring wall for a week; it was three source facts. Full write-up
in `docs/setvideo-exe-notes.md`, both new idioms in `docs/VC5-IDIOMS.md`.

1. **The radio dialog's lParam IS the loop-carried result.** `mov ebx,eax` at
   `+0x3bb` has no downstream reader, so it read as dead - but every back-edge
   lands at `+0x3a2`, *after* the `mov ebx,[gSel.method]` seed, so ebx is the
   next iteration's argument. One variable, seeded once, reassigned by each
   `DialogBoxParamA` and passed back in. A fresh `gSel.method` argument gives
   `inc eax` and 600 bytes of cascade.
2. **Compare the GLOBAL, not the snapshot you just took.** `if (vsave.method
   != 2)` lets VC5 hold the member in ebx and spill the loop variable;
   `if (gSel.method != 2)` forces all three `Sel` fields to slots
   `0x18/0x1c/0x20`. 678 → 488 and the size went exact.
3. **`if (ok) { …; return; }` + a second `return`, not an early-return
   guard.** The guard makes the exit the fall-through (`jne write`); the
   original branches away (`je <outlined stub>`) and cross-jumps seven exits
   with case 1's block as master. 488 → **0**.

## Earlier key idiom: CHK_FGets 0x00401150

Walk the `buf` **parameter** in place, no separate cursor. A `char *s = buf;`
cursor makes MSVC5 hoist the load to the loop header; walking `buf` itself
gives the preheader load + reload on the `n<=0` path that keeps `n` in ebp
(frameless). The flat `for` is the only loop form that keeps it frameless;
`do-while` / nested-`if` / early-return-guard all bring the frame back.

## Tooling notes for this lane

- Score one TU: `.venv/bin/python3` (capstone lives in the venv, not the
  system python) with `ghidra_to_match._score_source(src, name, orig,
  ['/O2 /ML'], tag)`. Orig bins: `build/match/orig_setvideo/<VA>.bin`.
- Sweep: `python3 tools/exe_sweep.py src/exe/setvideo/<VA>.c` → `report_exe.csv`.
- `/ML` static CRT: CRT calls are `E8`, never `FF 15`. Do **not** define
  `_CRTIMP` here - that is the BRally/DLL convention.

See [exe-decomp-state](exe-decomp-state.md), [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md), [image-build-gate](../oracle/image-build-gate.md),
[counting-reconciliation](../traps/counting-reconciliation.md).
