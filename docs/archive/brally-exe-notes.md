# BRally.exe — launcher notes (2026-08-27)

BRally.exe is the 8 KB game launcher (`orig/BRally.exe`). `.text` is 3,584
bytes at image base `0x400000`. Entry is `WinMainCRTStartup` at `0x401BF0`.
39 functions in `config/functions_brally.csv` (the map has 39 rows, not 40).

## EXE vs DLL (verified against the bytes)

| | BRally.exe | BRGlide.dll |
|---|---|---|
| image base | `0x400000` | `0x10000000` |
| CRT | **/MD** — 23 CRT imports from `MSVCRT.dll` | /MD — same |
| CRT call form | **`FF 15` IAT** (dllimport) | `FF 15` IAT |
| Win32 call form | `FF 15` IAT | `FF 15` IAT |
| `.reloc` | none | yes |
| entry | `WinMainCRTStartup` (MSVC 5.0 GUI, `__set_app_type(2)`) | `DllMain` |
| user entry | `WinMain` stdcall `ret 16` at `0x401810` | `RallyMain` export |

The guess that the EXE statically links the CRT (`E8` relative) is **false**.
It imports `MSVCRT.dll` and every CRT call in user code is `FF 15`. Matching
TUs therefore use the same `#define _CRTIMP __declspec(dllimport)` header as
the DLL. The only `E8` CRT-related calls are to *local* IAT thunks
(`FF 25 [iat]`) at the end of `.text` (`_initterm`, `_controlfp`,
`_XcptFilter`, `_except_handler3`) and to `_chkstk`.

`_setdefaultprecision` (`0x401DB0`) is `E8` to the `_controlfp` thunk, not
`FF 15` to the IAT — declare `_controlfp` as a local (non-dllimport) symbol
when matching it.

## What the launcher does

1. `GetInstallDir` — `HKLM\SOFTWARE\SouthPeak Interactive\Boss Rally\Directory`,
   fallback `"c:\\"`, append `'\\'` if missing.
2. If `BossRally.ini` is missing, `_spawnv` `SetVideo.exe`; on nonzero status,
   `MessageBoxA("User canceled SetVideo.exe")` and return 0.
3. `ReadINI` + `SetSubstituteDir("[Video]")` + `GetIniValue("Driver")`.
   `"D3D"` → `BRD3D.dll`, `"Glide"` → `BRGlide.dll`, anything else is used
   as the DLL name.
4. `LoadLibraryA` + `GetProcAddress("RallyMain")`.
5. Call `RallyMain` **cdecl** with the four WinMain args (`add esp, 0x10`),
   then `FreeLibrary`.

## Map vs reality

`0x401BF0`–`0x401D8F` is **one** CRT function (`WinMainCRTStartup`) split
into 7 map entries at non-prologue boundaries (`0x401CFE` starts mid-stream
at `mov al, [esi]`). Do not try to match those slices as C functions.

`0x401BC0` is MSVC `_chkstk` (size in EAX). Not expressible as C.

The four `FF 25 [iat]` slots are linker IAT thunks. A 0-arg dllimport tail
call (`_except_handler3`) compiles to the thunk; 2-arg wrappers do not
(they copy args and `call` / `add esp`).

## Matching results

**28 / 39** map entries byte-exact under `/O2` (COFF 16-byte `nop` padding
after `ret` ignored, same as the DLL comparator). **24 / 24** user
functions (map sizes sum **2,831 / 3,584** of `.text`, 79.0%). Winning
TUs: `build/brally_work/0x<VA>.c`. Original bytes: `build/match/orig_brally/`.

User code is `0x401000`–`0x401BBF` (FreeObjList through GetIniValue). The
remaining **11 / 39** map entries are CRT / compiler / linker glue — walls,
not targets (see below).

| VA | size | name | diffs | opt |
|---|---:|---|---:|---|
| 0x00401000 | 66 | FreeObjList | **0** | /O2 |
| 0x00401050 | 104 | CHK_FileExists | **0** | /O2 |
| 0x004010C0 | 97 | CHK_AllocateMemory | **0** | /O2 |
| 0x00401130 | 136 | SetSubstituteDir | **0** | /O2 |
| 0x004011C0 | 15 | CHK_FreeMemory | **0** | /O2 |
| 0x004011D0 | 34 | GetObj | **0** | /O2 |
| 0x00401200 | 31 | BindSection | **0** | /O2 |
| 0x00401220 | 46 | NextObj | **0** | /O2 |
| 0x00401250 | 78 | ReadINI | **0** | /O2 |
| 0x004012A0 | 28 | FreeINI | **0** | /O2 |
| 0x004012C0 | 467 | ReadList | **0** | /O2 |
| 0x004014A0 | 11 | ResetIncludeStack | **0** | /O2 |
| 0x004014B0 | 425 | ReadListLine | **0** | /O2 |
| 0x00401660 | 14 | IncludeStackEmpty | **0** | /O2 |
| 0x00401670 | 23 | PushInclude | **0** | /O2 |
| 0x00401690 | 19 | PopInclude | **0** | /O2 |
| 0x004016B0 | 6 | GetCommentChar | **0** | /O2 |
| 0x004016C0 | 10 | SetCommentChar | **0** | /O2 |
| 0x004016D0 | 212 | GetInstallDir | **0** | /O2 |
| 0x004017B0 | 48 | LoadRallyMain | **0** | /O2 |
| 0x004017E0 | 33 | UnloadRallyMain | **0** | /O2 |
| 0x00401810 | 600 | WinMain | **0** | /O2 |
| 0x00401A70 | 105 | AddSpawnArg | **0** | /O2 |
| 0x00401AE0 | 223 | GetIniValue | **0** | /O2 |
| 0x00401BC0 | 47 | _chkstk | WALL | compiler helper |
| 0x00401BF0 | 270 | WinMainCRTStartup | WALL | CRT, map-split |
| 0x00401CFE | 54 | WinMainCRTStartup_cmdtail | WALL | map-split |
| 0x00401D34 | 30 | WinMainCRTStartup_WinMain | WALL | map-split |
| 0x00401D52 | 21 | WinMainCRTStartup_filter | WALL | map-split |
| 0x00401D67 | 13 | WinMainCRTStartup_exit | WALL | map-split |
| 0x00401D74 | 27 | WinMainCRTStartup_epilogue | WALL | map-split |
| 0x00401D8F | 15 | WinMainCRTStartup_skipspace | WALL | map-split |
| 0x00401DA0 | 6 | _XcptFilter_thunk | WALL | 2-arg IAT thunk |
| 0x00401DA6 | 6 | _initterm_thunk | WALL | 2-arg IAT thunk |
| 0x00401DB0 | 19 | _setdefaultprecision | **0** | /O2 |
| 0x00401DD0 | 3 | _matherr | **0** | /O2 |
| 0x00401DE0 | 1 | CRT_empty | **0** | /O2 |
| 0x00401DF0 | 6 | _except_handler3_thunk | **0** | /O2 (0-arg tail jmp) |
| 0x00401DF6 | 6 | _controlfp_thunk | WALL | 2-arg IAT thunk |

Game-code leftovers from the prior session, now matched:

- **WinMain** (`0x401810`, 600 B): `"BRD3D.dll"` / `"BRGlide.dll"` as
  `extern char[]` (`s_BRD3D` at `0x403178`, `s_BRGlide` at `0x403164`).
  A string *literal* `strcpy` of those 10/12-byte names is a dword-move
  burst; orig used generic `rep movs`, and that was the dest-lea cascade
  (233 diffs). Literals for `"D3D"` / `"Glide"` / `"BossRally.ini"` stay
  literals (strcmp/strcat, not short-strcpy).
- **AddSpawnArg** (`0x401A70`, 105 B): dest is `&gArgBuf[gArgOff]` (scan
  `s` first, then load `gArgOff` into ebx). A named `dst = gArgBuf +
  gArgOff` hoists the load before the scan. Spell `gArgOff += strlen(s)
  + 1` *before* `gArgv[gArgc] = 0` so the scasb-zero is hoisted into the
  strcpy tail (`xor eax,eax` after `mov ecx,eax`) and reused for the
  NULL store.
- **GetIniValue** (`0x401AE0`, 223 B):
  `for (line = NextObj(pini); line != 0; line = NextObj(pini))`.
  A `do { …; line = NextObj(); } while (line)` merges loop-exit
  `return 0` with the BindSection-fail xor-eax epilogue and emits
  `je fail; jmp loop` instead of orig `jne loop` (30 diffs, +1 byte).
  Same latch on SetVideo's GetIniValue (`0x4023B0`, 27 diffs) — use the
  for-init form there, do not retry do-while / `return line`.

## CRT / EH walls (11 / 39) — not targets

Checked against the bytes. None of these is hand-written game code.

| VA | bytes | what it is | why a wall |
|---|---:|---|---|
| `0x401BC0` | 47 | MSVC `_chkstk` | Probe EAX in pages of 0x1000, `test [ecx],eax`, then `mov esp,ecx` and ret-to-caller. Size in EAX. Compiler helper; no C spelling. |
| `0x401BF0`–`0x401D9E` | ~430 | `WinMainCRTStartup` | MSVC 5.0 GUI CRT (`__set_app_type(2)`, `__p__fmode` / `__p__commode`, `__getmainargs`, `_initterm`, cmdtail quote/space skip, `WinMain`, `_XcptFilter` SEH). **One** function, map-split into 7 entries at non-prologue boundaries (`0x401CFE` is `mov al,[esi]` mid-stream). Do not match the slices as C. |
| `0x401DA0` | 6 | `_XcptFilter` thunk | `ff 25 68 c1 40 00` — `jmp [IAT]`. Linker thunk. |
| `0x401DA6` | 6 | `_initterm` thunk | `ff 25 74 c1 40 00` — same. |
| `0x401DF6` | 6 | `_controlfp` thunk | `ff 25 8c c1 40 00` — same. |

A 0-arg dllimport tail call *does* compile to `jmp [iat]` — that is why
`_except_handler3` at `0x401DF0` matches (`void f(void) { _except_handler3(); }`).
A 2-arg C wrapper copies args and `call` / `add esp,8`; it cannot become a
6-byte `FF 25`. Those three thunks are linker output, not source.

Matched CRT glue (not walls, tiny and C-reachable): `_setdefaultprecision`
(E8 to the local `_controlfp` thunk), user `_matherr` stub (`xor eax,eax;
ret`), empty user-init (`ret`), `_except_handler3` thunk.

## Idioms that paid off here

- Shared-fail epilogue wants `if (p != 0) { … } return 0`, not
  `if (p == 0) return 0` (GetObj, BindSection, UnloadRallyMain).
- Short string `strcpy` of a literal is a dword move; orig used the
  generic `rep movs` form, so `"c:\\"` / `"\\"` / `"BRD3D.dll"` /
  `"BRGlide.dll"` must be `extern char[]`. `strcat` of a longer literal
  (`"BossRally.ini"`) already uses `rep movs` — leave it a literal.
- `n = 0` is hoisted *before* `ResetIncludeStack()` in ReadList
  (`xor ebp,ebp` then the call).
- Comment char default is `'#'` (`0x40308c`); ReadINI temporarily sets `';'`.
- List files open `"rt"`, CHK_FileExists opens `"rb"`. Verbose CHK_* uses
  `OutputDebugStringA`, not `fprintf(stderr, …)` (DLL twin).
- **GetIniValue latch is a `for`**, not a `do-while`. First `NextObj` is
  the for-init (its 0-result shares the BindSection xor-fail); the
  increment `NextObj` is the latch (`test eax,eax; jne body`) and
  fallthrough is the no-xor `return 0` (eax already 0). `do-while` plus
  `return line` still folds to the xor epilogue and emits `je fail; jmp
  loop`.
- **AddSpawnArg dest is `&gArgBuf[gArgOff]`**, not a named `dst = gArgBuf
  + gArgOff` (that loads `gArgOff` before scanning `s`). Statement order
  after the copy: `gArgv[gArgc] = dest; gArgc++; gArgOff += strlen(s)+1;
  gArgv[gArgc] = 0` — strlen *before* the NULL store, so `xor eax,eax`
  lives in the strcpy tail and is reused. Putting the NULL store before
  strlen steals eax for `gArgc` (`inc eax; mov [eax*4-4], dest`) and
  materializes 0 as an immediate (`C7`).
