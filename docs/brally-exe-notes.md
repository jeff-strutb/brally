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

25 / 39 functions byte-exact under `/O2` (COFF 16-byte `nop` padding after
`ret` ignored, same as the DLL comparator). Winning TUs:
`build/brally_work/0x<VA>.c`. Original bytes: `build/match/orig_brally/`.

User code is 0x401000–0x401AE0 (2,336 / 3,584 of `.text`, 65%). Of those
24 functions, 21 match at 0. CRT/compiler glue is the rest.

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
| 0x00401810 | 600 | WinMain | 233 | /O2 |
| 0x00401A70 | 105 | AddSpawnArg | 56 | /Od |
| 0x00401AE0 | 223 | GetIniValue | 30 | /O2 |
| 0x00401BC0 | 47 | _chkstk | — | compiler helper |
| 0x00401BF0 | 270 | WinMainCRTStartup | — | CRT, map-split |
| 0x00401CFE | 54 | WinMainCRTStartup_cmdtail | — | map-split |
| 0x00401D34 | 30 | WinMainCRTStartup_WinMain | — | map-split |
| 0x00401D52 | 21 | WinMainCRTStartup_filter | — | map-split |
| 0x00401D67 | 13 | WinMainCRTStartup_exit | — | map-split |
| 0x00401D74 | 27 | WinMainCRTStartup_epilogue | — | map-split |
| 0x00401D8F | 15 | WinMainCRTStartup_skipspace | — | map-split |
| 0x00401DA0 | 6 | _XcptFilter_thunk | 6 | IAT thunk |
| 0x00401DA6 | 6 | _initterm_thunk | 6 | IAT thunk |
| 0x00401DB0 | 19 | _setdefaultprecision | **0** | /O2 |
| 0x00401DD0 | 3 | _matherr | **0** | /O2 |
| 0x00401DE0 | 1 | CRT_empty | **0** | /O2 |
| 0x00401DF0 | 6 | _except_handler3_thunk | **0** | /O2 (0-arg tail jmp) |
| 0x00401DF6 | 6 | _controlfp_thunk | 6 | IAT thunk |

Residue on the three user misses:

- **GetIniValue** (30 diffs, +1 byte): body matches including IAT-hoist of
  `strchr`/`printf`. Latch is `jne loop` in orig vs `je fail; jmp loop`
  (the loop-exit `return 0` merges with the BindSection-fail xor-eax
  epilogue). Same wall class as the documented do-while latch.
- **AddSpawnArg** (56 diffs): inline `strcpy` loads `gArgOff` before
  scanning `s`; orig scans `s` first then loads `gArgOff` into ebx.
  Hoist-order wall.
- **WinMain** (233 diffs, 600 vs 608): first ~0xCB bytes match (registry
  dir, `BossRally.ini` strcat, `CHK_FileExists`, `SetVideo.exe` spawn,
  MessageBox). Remaining cascade is the Driver strcmp/strcpy dest-`lea`
  placement (`lea edx, [dllname]` lives in two orig arms, one in the
  recomp tail). After that the Free/LoadLibrary/RallyMain/`ret 16` shape
  is the same, shifted.

## Idioms that paid off here

- Shared-fail epilogue wants `if (p != 0) { … } return 0`, not
  `if (p == 0) return 0` (GetObj, BindSection, UnloadRallyMain).
- Short string `strcpy` of a literal is a dword move; orig used the
  generic `rep movs` form, so `"c:\\"` / `"\\"` must be `extern char[]`.
- `n = 0` is hoisted *before* `ResetIncludeStack()` in ReadList
  (`xor ebp,ebp` then the call).
- Comment char default is `'#'` (`0x40308c`); ReadINI temporarily sets `';'`.
- List files open `"rt"`, CHK_FileExists opens `"rb"`. Verbose CHK_* uses
  `OutputDebugStringA`, not `fprintf(stderr, …)` (DLL twin).
