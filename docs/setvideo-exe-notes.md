# SetVideo.exe — notes (2026-08-27)

SetVideo.exe is the renderer/display config utility (`orig/SetVideo.exe`,
60,928 bytes). `.text` is **36,864** bytes at image base `0x400000` (CLAUDE.md
quoted 36,476 — raw section size is 0x9000). Entry `WinMainCRTStartup` at
`0x4038D0`. Map: `config/functions_setvideo.csv` (342 functions, generated
with `tools/funcmap2.py`; 61 extents cross the next entry — `WinMain` at
`0x402480` is truncated at 930 B mid-function).

## EXE vs BRally.exe / BRGlide.dll

| | SetVideo.exe | BRally.exe |
|---|---|---|
| image base | `0x400000` | `0x400000` |
| CRT | **/ML static** — **no** `MSVCRT.dll` | /MD — 23 CRT imports |
| CRT call form | **`E8` relative** to local CRT | `FF 15` IAT |
| Win32 call form | `FF 15` IAT (USER32/ADVAPI32/KERNEL32) | `FF 15` IAT |
| `.reloc` | none | none |
| `.text` | 36,864 (mostly static CRT) | 3,584 |
| entry | `WinMainCRTStartup` `0x4038D0` | `0x401BF0` |

BRally's `_CRTIMP dllimport` header is **wrong** here. Drop it: CRT calls
become `E8` and the shared helpers match. Win32 stays dllimport via
`windows.h`. KERNEL32 imports are the static CRT's heap/file/locale
backends (`HeapAlloc`, `CreateFileA`, `LCMapStringA`, …), plus
`OutputDebugStringA` / `GetProcAddress` / `LoadLibraryA`.

Each CRT call is 1 byte smaller than BRally (`E8` 5 vs `FF 15` 6). That's
why `CHK_FileExists` is 101 vs 104, `ReadList` 458 vs 467,
`ReadListLine` 416 vs 425, `FreeINI` 27 vs 28 — same C, different linkage.

## Shared helpers (byte-exact after dropping dllimport)

21 / 21 BRally user helpers that exist here scored **0** against SetVideo
with the BRally C and a static-CRT header. Plus three CRT stubs.

| SV VA | size | name | reused BR VA |
|---|---:|---|---|
| 0x00401000 | 67 | FreeObjList | 0x00401000 |
| 0x00401400 | 101 | CHK_FileExists | 0x00401050 |
| 0x00401470 | 94 | CHK_AllocateMemory | 0x004010C0 |
| 0x004014D0 | 136 | SetSubstituteDir | 0x00401130 |
| 0x00401600 | 14 | CHK_FreeMemory | 0x004011C0 |
| 0x00401650 | 34 | GetObj | 0x004011D0 |
| 0x00401680 | 31 | BindSection | 0x00401200 |
| 0x004016A0 | 46 | NextObj | 0x00401220 |
| 0x004016D0 | 78 | ReadINI | 0x00401250 |
| 0x00401720 | 27 | FreeINI | 0x004012A0 |
| 0x00401740 | 458 | ReadList | 0x004012C0 |
| 0x00401910 | 11 | ResetIncludeStack | 0x004014A0 |
| 0x00401920 | 416 | ReadListLine | 0x004014B0 |
| 0x00401AC0 | 14 | IncludeStackEmpty | 0x00401660 |
| 0x00401AD0 | 23 | PushInclude | 0x00401670 |
| 0x00401AF0 | 19 | PopInclude | 0x00401690 |
| 0x00401B10 | 6 | GetCommentChar | 0x004016B0 |
| 0x00401B20 | 10 | SetCommentChar | 0x004016C0 |
| 0x00401B30 | 212 | GetInstallDir | 0x004016D0 |
| 0x00403140 | 1 | CRT_empty | 0x00401DE0 |
| 0x00406C85 | 3 | _matherr | 0x00401DD0 |
| 0x00405940 | 19 | _setdefaultprecision | 0x00401DB0 |

## SetVideo-only user code (matched)

| SV VA | size | name | diffs | notes |
|---|---:|---|---:|---|
| 0x00401050 | 255 | CHK_FReadOpen | **0** | `{FILE*, name}` wrapper; error writes `c:\RallyError.txt` via FILE*-first 2-arg fputs |
| 0x00401230 | 215 | CHK_FWriteOpen | **0** | same wrapper, `fopen(path, mode)` |
| 0x00401310 | 89 | CHK_FPutS | **0** | |
| 0x00401370 | 144 | CHK_FClose | **0** | fclose then `free(name); free(p)` |
| 0x00401610 | 58 | CountSections | **0** | FindFirst/FindNext until index==-1 |
| 0x00401EC0 | 59 | ComboGetItemData | **0** | `CB_GETCURSEL` then `CB_GETITEMDATA` on ctl 0x3E9 |

## SetVideo-only, identified, not 0

| SV VA | size | name | diffs |
|---|---:|---|---:|
| 0x00401150 | 217 | CHK_FGets | — (inlined FILE buffer / `_filbuf`/`ungetc`) |
| 0x00401560 | 71 | FindFirstSection | 4 (edx vs ecx coloring) |
| 0x004015B0 | 70 | FindNextSection | 44 |
| 0x00401C10 | 91 | DlgProcOKCancel | 53 (`switch` vs `sub 0x110; dec`) |
| 0x00401C70 | 332 | DlgProc | — |
| 0x00401DC0 | 251 | DlgProcRadio | — |
| 0x00401F00 / 2030 | 298 | FillComboA/B | — |
| 0x00402160 / 2260 | 248 | DlgProcComboA/B | — |
| 0x00402360 | 74 | FollowUse | 11 (`Use=` alias walk; esi/edi) |
| 0x004023B0 | 205 | GetIniValue | 27 (same latch as BRally's 30) |
| 0x00402480 | 930 | WinMain | map-truncated; continues into dialog/INI write |
| 0x0040294F | 369 | WriteDefaultINI | CHK_FWriteOpen + CHK_FPutS of D3D* keys |
| 0x00402AC0 / 2BCE | 238/240 | WriteVideoINI{,2} | write `[Video]` from current/dialog |

## What SetVideo does that BRally didn't

1. **Checked stdio wrappers** — `CHK_FReadOpen` / `FWriteOpen` / `FPutS` /
   `FClose` / `FGets`. Open failure of a read dumps the sprintf'd error to
   `c:\RallyError.txt` (`"w"`) then `OutputDebugStringA` + `exit(1)`.
2. **Section cursor extras** — `FindFirstSection` / `FindNextSection` /
   `CountSections` (scan for `'['`); `FollowUse` walks `Use=` aliases.
3. **Win32 dialog UI** — `DialogBoxParamA`, `CheckDlgButton`,
   `CheckRadioButton`, `SendDlgItemMessageA` (combo 0x3E9),
   `SetWindowLongA(..., 8, lParam)` (DWL_USER, **not** GWL_USERDATA=-21).
4. **Writes `BossRally.ini`** — default D3D symptom block
   (`Driver=D3D`, `D3DInvSrcAlpha=`, `D3DClearZBuffer=`, `D3DAlphaCompare=`,
   `D3DDrawCarShadow=`, `D3DAlwaysSquareTextures=0`, …) and a `[Video]`
   dump of the selected card.
5. **Reads `BossRally.vdb`** — video-device database; missing file →
   `MessageBoxA("Error: file %s is missing.", "Boss Rally Display Wizard")`.
6. Title: `"Boss Rally Display Wizard"`. Registry dir is the same
   `HKLM\SOFTWARE\SouthPeak Interactive\Boss Rally\Directory`.

WinMain outline (from calls, not a 0-diff): GetDesktopWindow → GetInstallDir
→ `strstr` path → CHK_FileExists(`BossRally.vdb`) → MessageBox on miss →
ReadINI / CountSections → FReadOpen+FGets parse of the vdb → dialogs →
FreeINI / WriteDefaultINI / WriteVideoINI.

## Matching totals

**28 / 342** functions at diffs=0 (**2,640 / 36,864** of `.text`, **7.2%**).

Of user-region code (`0x401000`–`0x4038D0` ≈ 10,448 B before CRT startup),
the 0-diff set is 2,617 B (**25.0%** of that span). The other ~26 KB is
statically-linked MSVC 5.0 CRT (named where identified: `malloc`/`free`/
`fopen`/`sprintf`/`_chkstk`/`WinMainCRTStartup`/…).

Winning TUs: `build/setvideo_work/0x<VA>.c`. Orig bytes:
`build/match/orig_setvideo/`. 70 / 342 map rows have names.

## CRT-header rule (SetVideo)

```
/* SetVideo.exe is /ML (static CRT): CRT calls are E8, not FF 15. */
#include <windows.h>
#include <stdlib.h>
…
```

Do **not** `#define _CRTIMP __declspec(dllimport)` — that is the BRally/DLL
convention and would emit `FF 15` against this binary's `E8`s.
