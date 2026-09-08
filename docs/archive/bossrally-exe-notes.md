# BossRally.exe — intro-shim notes (2026-08-27)

BossRally.exe is the ~40 KB intro stub (`orig/BossRally.exe`). `.text` is
23,552 bytes at image base `0x400000`. Entry is `WinMainCRTStartup` at
`0x401BC0`. 215 functions in `config/functions_bossrally.csv`.

## EXE vs BRally.exe (verified against the bytes)

| | BossRally.exe | BRally.exe |
|---|---|---|
| image base | `0x400000` | `0x400000` |
| CRT | **/MT — static libcmt, no `MSVCRT.dll`** | **/MD** — 23 CRT imports from `MSVCRT.dll` |
| CRT call form | **`E8` relative** to local libcmt | `FF 15` IAT |
| Win32/OLE call form | `FF 15` IAT | `FF 15` IAT |
| imports | KERNEL32, USER32, GDI32, **ole32** | KERNEL32, USER32, ADVAPI32, MSVCRT |
| `.reloc` | none | none |
| entry | `WinMainCRTStartup` (MSVC 5.0 GUI) | same |
| user entry | `WinMain` stdcall `ret 16` at `0x401340` | `WinMain` at `0x401810` |

The brief's "/MD like BRally" guess is **false** for this EXE. The import
table has **no MSVCRT.dll**. `.rdata` carries the static-CRT banner
("Microsoft Visual C++ Runtime Library", R60xx strings). HeapCreate /
GetEnvironmentStrings / GetACP are imported from KERNEL32 the way libcmt
does. Matching TUs therefore do **not** `#define _CRTIMP
__declspec(dllimport)` — CRT callees are local `E8`. Win32/OLE stay
dllimport (`FF 15`) via `windows.h`.

The only `FF 25` slots are 0-arg IAT thunks (`CoUninitialize` at
`0x4010F0`, `RtlUnwind` at `0x406A20`). `_setdefaultprecision`
(`0x401FF0`) is `E8` to the local `_controlfp` (`0x403F10`), same shape
as BRally's thunked version.

## What this EXE does that BRally / SetVideo don't

BRally.exe is the renderer chooser (registry → `BossRally.ini` →
LoadLibrary `BRGlide.dll`/`BRD3D.dll` → `RallyMain`). SetVideo.exe
writes that INI. **BossRally.exe never touches the INI, the registry, or
the game DLLs.**

It is a DirectShow CPlay-style player plus a process launch:

1. `CoInitialize(NULL)`, copy `"Player"` into the window class name.
2. `RegisterClassA` (`WndProc` at `0x401030`, icon/menu resource 0x80,
   `LTGRAY_BRUSH`) and `CreateWindowExA` of a hidden owner
   (`WS_OVERLAPPED|CAPTION|SYSMENU|THICKFRAME|MINIMIZEBOX` = `0x00CE0000`,
   size 0×65, title `"Player - Untitled"`). No `ShowWindow`.
3. `CoCreateInstance(CLSID_FilterGraph)` + `IMediaEvent::GetEventHandle`.
4. `GetFullPathNameA("brally.avi")`, `MultiByteToWideChar`,
   `IGraphBuilder::RenderFile`, wait cursor around the render.
5. `IVideoWindow::put_MessageDrain(hwnd)`, `HideCursor(-1)`,
   `put_FullScreenMode(-1)`.
6. `IMediaControl::Run`. Message loop (`WaitMessage` /
   `MsgWaitForMultipleObjects` on the graph event). Escape /
   `ID_APP_EXIT` (`0xE141`) / `EC_COMPLETE` stop the graph.
7. `CoUninitialize`, then `_spawnve`-family
   `SpawnWait("brally.exe", "brally.exe", NULL)` — **always**, even if
   AVI open failed (only `InitCOM` failure returns before the spawn).

No MCIWnd, no WINMM, no quartz.dll import — quartz is loaded in-proc via
`CoCreateInstance`. Resource stringtable is the CPlay set ("Sorry, the
Quartz core components failed to initalise.", "This file refuses to
play/pause/stop"). GUIDs at `0x407000`:

| VA | GUID |
|---|---|
| `0x407000` | `IID_IMediaEvent` |
| `0x407010` | `CLSID_FilterGraph` |
| `0x407020` | `IID_IGraphBuilder` |
| `0x407030` | `IID_IVideoWindow` |
| `0x407040` | `IID_IMediaControl` |
| `0x407050` | `IID_IMediaPosition` |

Media state at `0x40AC78`: 0 none, 1 stopped (file loaded), 2 paused, 3
playing.

## Map vs reality

`0x401BC0`–`0x401D46` is **one** CRT function (`WinMainCRTStartup`) split
into 7 map entries at non-prologue boundaries, same class as BRally's
startup split. Do not match those slices as C functions.

`0x401A50` is MSVC `strncpy` (0x7efefeff nul-scan). CRT, not user.
`0x401D90` (491 B) is `_spawnve` (searches `\`, `/`, `:`, prepends `.\`).
`0x4039B5` (542 B) is the `CreateProcessA` worker. All walls.

Several later map rows start mid-instruction (`0x2e` CS-prefix nops,
`0x4d`, `0x406082` size 1) — map artifacts, not C functions.

## BRally twins

No file/INI helper is byte-identical. Three CRT scraps are:

| BossRally | size | BRally twin |
|---|---:|---|
| `0x401B70` | 1 | `0x401DE0` CRT_empty |
| `0x401D2B` | 27 | `0x401D74` WinMainCRTStartup_epilogue |
| `0x403075` | 3 | `0x401DD0` `_matherr` |

## Matching results

35 / 215 functions byte-exact under `/O2` (COFF 16-byte `nop` padding
after `ret` ignored). Winning TUs: `build/bossrally_work/0x<VA>.c`.
Original bytes: `build/match/orig_bossrally/`.

35 / 215 functions, 2,482 / 23,552 of `.text` (10.54%).

User region is `0x401000`–`0x401BBF` (3,008 / 23,552 of `.text`, 12.8%).
28 matched functions live there, 2,431 / 3,008 (80.8%). Every CPlay-derived
user function in that range is a MATCH. The three unmatched map rows
inside it are CRT (`strncpy` `0x401A50`, `_fpmath` `0x401B50`,
`_initp_misc_cfltcvt` `0x401B80`). The rest of `.text` is static CRT.

Source lineage is the DirectX 7 CPlay sample (`cplay.c` / `media.c`,
1998-08-17): same names (`DoMainLoop`, `OnGraphNotify`, `OnMediaStop`,
`ChangeStateTo` → `SetMediaState`, `CanPlay` → `CanRun`, `CanStop` →
`IsPlayingOrPaused`). BossRally strips the toolbar/About/file-dialog,
hardcodes `brally.avi`, always `_spawnve`s `brally.exe`, and adds
fullscreen + Escape-to-quit.

| VA | size | name | diffs | opt |
|---|---:|---|---:|---|
| 0x00401000 | 45 | HandleCommand | **0** | /O2 |
| 0x00401030 | 124 | WndProc | **0** | /O2 |
| 0x004010B0 | 54 | InitCOM | **0** | /O2 |
| 0x004010F0 | 6 | CoUninitialize_thunk | **0** | /O2 |
| 0x00401100 | 137 | RegisterWindowClass | **0** | /O2 |
| 0x00401190 | 155 | CreatePlayerWindow | **0** | /O2 |
| 0x00401230 | 261 | DoMainLoop | **0** | /O2 |
| 0x00401340 | 199 | WinMain | **0** | /O2 |
| 0x00401410 | 24 | CanRun | **0** | /O2 |
| 0x00401430 | 24 | IsPlayingOrPaused | **0** | /O2 |
| 0x00401450 | 15 | IsStopped | **0** | /O2 |
| 0x00401460 | 14 | HasGraph | **0** | /O2 |
| 0x00401470 | 10 | SetMediaState | **0** | /O2 |
| 0x00401480 | 36 | InitMedia | **0** | /O2 |
| 0x004014B0 | 129 | CreateGraph | **0** | /O2 |
| 0x00401540 | 75 | SetVideoDrain | **0** | /O2 |
| 0x00401590 | 163 | SetFullScreen | **0** | /O2 |
| 0x00401640 | 46 | DeleteContents | **0** | /O2 |
| 0x00401670 | 133 | OpenMediaFile | **0** | /O2 |
| 0x00401700 | 161 | SetPlayerTitle | **0** | /O2 |
| 0x004017B0 | 104 | OpenClip | **0** | /O2 |
| 0x00401820 | 74 | OnMediaPlay | **0** | /O2 |
| 0x00401870 | 126 | OnMediaStop | **0** | /O2 |
| 0x004018F0 | 149 | OnMediaPauseStop | **0** | /O2 |
| 0x00401990 | 6 | GetGraphEvent | **0** | /O2 |
| 0x004019A0 | 139 | OnGraphNotify | **0** | /O2 |
| 0x00401A30 | 21 | SpawnWait | **0** | /O2 |
| 0x00401A50 | 254 | strncpy | — | CRT |
| 0x00401B50 | 23 | _fpmath | — | CRT |
| 0x00401B70 | 1 | CRT_empty | **0** | /O2 |
| 0x00401B80 | 56 | _initp_misc_cfltcvt | — | CRT |
| 0x00401BC0 | 247 | WinMainCRTStartup | — | CRT, map-split |
| 0x00401FF0 | 19 | _setdefaultprecision | **0** | /O2 |
| 0x0040305D | 6 | CRT_ret_0x411 | **0** | /O2 |
| 0x00403063 | 6 | CRT_ret_0x804 | **0** | /O2 |
| 0x00403069 | 6 | CRT_ret_0x412 | **0** | /O2 |
| 0x0040306F | 6 | CRT_ret_0x404 | **0** | /O2 |
| 0x00403075 | 3 | _matherr | **0** | /O2 |
| 0x0040607D | 5 | CRT_ret_10 | **0** | /O2 |

No coloring walls on the four named misses — all four MATCH `/O2`.

## Idioms that paid off here

- Shared-fail epilogue: `if (p != 0) { if (f(p) != 0) { … } }` not
  early `return 0` (OpenClip). Same class as BRally GetObj.
- `if (wParam != ID_APP_EXIT) return DefWindowProcA(…); PostQuit; return
  0` — `je` over the default path, not `jne` to quit (HandleCommand).
- Short `"Player"` / `" - Untitled"` / `" - "` strcpy of a **literal**
  is the dword/rep-movs form. `extern char[]` of the same bytes does
  not (InitCOM, CreatePlayerWindow, SetPlayerTitle).
- `switch` on `WM_DESTROY` / `WM_CHAR` / `WM_SYSKEYDOWN` (0x104, **not**
  `WM_QUERYNEWPALETTE` 0x30F) / `WM_COMMAND` reproduces the
  `cmp 0x102; ja; je; sub 2` tree (WndProc).
- COM duals keep IDispatch (IMediaControl/Event/Position/VideoWindow);
  `IGraphBuilder::RenderFile` is index 13 (`+0x34`), no IDispatch.
- `put_CurrentPosition(0.0)` is `push 0; push 0` (two dwords), no
  `and esp,-8`.
- 0-arg dllimport tail = `jmp [IAT]` (`CoUninitialize_thunk`).
- `OpenFile` collides with `windows.h`; the user function is `OpenClip`.
- Spawn wrapper is 3-arg `(cmd, argv, env=NULL)`, **not**
  `_spawnv(mode, cmd, argv)`. Callee is local `_spawnve` (`E8`, /MT).
- **DoMainLoop is CPlay's `while (TRUE)` with `GetGraphEvent` each
  lap**, not a one-shot then `WaitMessage`. Shape:
  `if ((ahObjects[0] = GetGraphEvent()) == NULL) WaitMessage(); else {
  Result = MsgWaitForMultipleObjects(cObjects, ahObjects, FALSE,
  INFINITE, QS_ALLINPUT); if (Result != WAIT_OBJECT_0 + cObjects) {
  if (Result == WAIT_OBJECT_0) { OnGraphNotify(); … } continue; } }
  while (PeekMessage(…, PM_REMOVE)) { … }`. Inner `while (PeekMessage)`
  is the shared peek/dispatch; fail of the inner while returns to
  GetGraphEvent, not to WaitMessage. IAT hoist (ebx=TranslateMessage,
  ebp=MsgWait, esi=PostMessageA, edi=PeekMessageA) falls out of this
  loop — WaitMessage / DispatchMessageA stay `FF 15`. BossRally extras
  inside the Peek loop: `WM_CHAR` Escape → pause-stop +
  `PostMessageA(hwnd, WM_QUIT, 0, 0)`; skip Translate/Dispatch on
  `WM_SYSKEYDOWN`; after `OnGraphNotify`, `IsStopped` → same PostQuit.
- **OnMediaStop is CPlay `OnMediaAbortStop` + `FROM_START`, no
  MessageBox.** `Stop`, Release control, QI `IMediaPosition`, then two
  *separate* `if (SUCCEEDED(hr))` — first does `put_CurrentPosition(0)`
  + Release, second does `ChangeStateTo(Stopped)`. The second test is
  dead after the first `jl`, but VC5 keeps hr in esi across the COM
  calls only in this form. `if (hr < 0) return` after the QI **DCEs**
  the retest and drops esi (the old 99-diff / 9-byte miss).
- **OnGraphNotify is CPlay nested `SUCCEEDED`, not early-return
  Release.** `if (SUCCEEDED(QI)) { if (SUCCEEDED(GetEvent)) { if
  EC_COMPLETE OnMediaPauseStop; else if USERABORT||ERRORABORT
  OnMediaStop; else if EC_FULLSCREEN_LOST SetFullScreen; } Release; }`.
  The 2-diff `mov ecx,[eax]; call [ecx+8]` vs `edx` on the shared
  Release was the early-return spelling, not a coloring wall. EC_COMPLETE
  is 1, USERABORT 2, ERRORABORT 3, FULLSCREEN_LOST `0x12`.
- **WinMain: do not write `nReturn = hPrevInstance`.** CPlay leaves
  `UINT nReturn` uninitialized on the `Register && Create && Init`
  fail path; VC5 materializes `mov esi, [esp+0xc]` (reload hPrev) at
  the join. Spelling `nReturn = (UINT)hPrev` hoists hPrev into esi
  *before* `RegisterWindowClass`, replacing orig `mov eax,[esp+8]; mov
  ecx,[esp+4]; push esi; push eax; push ecx` (57-diff cascade). InitCOM
  fail stays shrink-wrapped `ret 16`. Drain/fullscreen failure is
  `goto fail` so those `je`s skip DoMainLoop and hit the hPrev reload.
  Success still does the extra CPlay `DeleteContents` before the
  always-on DeleteContents + CoUninit + SpawnWait.
