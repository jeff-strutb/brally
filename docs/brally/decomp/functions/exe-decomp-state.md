# Exe decomp state

*Recorded 2026-08-27.*

> The in-scope EXE workstream: 105 functions match across BRally/SetVideo/BossRally in src/brally/exe/ + report_exe.csv (exe_sweep.py). BRally.exe AND SetVideo.exe are both game-code complete; BossRally.exe is the one left.

## EXE decompilation - opened AND integrated 2026-08-27 (parallel sessions)

The three in-scope EXEs (~64KB .text, ~12% of the target) were untouched until
this session. **NOW INTEGRATED (update 2026-08-27 later same day): 100 EXE
functions match, filed under `src/brally/exe/{brally,setvideo,bossrally}/` (one
`0x<VA>.c` per fn, all `#ifdef BR_MATCHING_BUILD`-guarded so the port build
ignores them), swept by `tools/brally/exe_sweep.py` into
`build/brally/win32/match/report_exe.csv`, and folded into the combined count by
`total.py`.** The earlier "NOT integrated / deferred" note is superseded. Per-
function bins are in `build/brally/win32/match/orig_<exe>/`; scratch C remains in
`build/<exe>_work/`. See [counting-reconciliation](../traps/counting-reconciliation.md).

### Per-EXE state
- **BRally.exe** (launcher, 3,584B .text): **GAME CODE COMPLETE - 24/24 user
  functions match, 2,831B (79% of .text). FIRST fully-decompiled binary.** The
  other 11/39 map entries are CRT/compiler/linker walls (fence, don't match):
  _chkstk, WinMainCRTStartup (one fn, 7 map slices), 3 two-arg IAT thunks
  (FF 25), _except_handler3, _setdefaultprecision, _matherr, CRT_empty.
  **/MD** - CRT via import table (FF 15), same `_CRTIMP` dllimport header as
  BRGlide.dll. Base 0x400000; WinMain stdcall ret 16; RallyMain cdecl 4 args.
  GetIniValue uses `for(line=NextObj;line;line=NextObj)` (do-while merges
  the loop-exit with BindSection's xor-fail → je fail;jmp loop) - SAME latch
  matches SetVideo's GetIniValue 0x4023B0. Map names in
  `config/brally/functions_brally.csv`; notes `docs/brally-exe-notes.md`.
- **SetVideo.exe** (video config util, 36,864B .text): **COMPLETE 2026-09-03
  - 42/42 game functions, 7,228/7,228 B in `0x401000` - `0x402D20`, image gate
  0 differing bytes. SECOND fully-decompiled binary.** See
  [setvideo-exe-complete](setvideo-exe-complete.md). The other 27,888 B (289 map rows at/above
  `0x402D20`) is static CRT, fenced by `CRT_START`, not a target.
  **/ML - statically-linked CRT (NO MSVCRT.dll import), CRT calls are E8
  (direct), DROP the `_CRTIMP` dllimport.** The 21 BRally file/INI helpers
  reused byte-identical. Writes BossRally.ini, reads BossRally.vdb, Win32
  dialogs. Map `config/brally/functions_setvideo.csv` (70/342 named);
  `docs/setvideo-exe-notes.md`.
- **BossRally.exe** (intro shim, 23,552B .text): 31/215 matched (7.46%); user
  region 0x401000-0x401BBF is 3,008B, 24 matches = 56.7% (rest static CRT).
  **/MT - static multithreaded CRT (libcmt, E8), NOT /MD; Win32/OLE stay
  FF 15; no _CRTIMP.** DirectShow: plays brally.avi via CoCreateInstance(
  CLSID_FilterGraph)/RenderFile/IVideoWindow, then _spawnve("brally.exe"). No
  INI/registry/game-DLL; shares almost nothing with BRally (3 tiny CRT scraps
  only). 4 user misses: OnGraphNotify (2d, ecx/edx Release = coloring wall),
  WinMain (57d, esi hoist of hPrev), OnMediaStop (99d, DCE of dead HRESULT),
  DoMainLoop (184d, message-loop IAT hoist). `config/brally/functions_bossrally.csv`,
  `build/bossrally_work/`, `docs/bossrally-exe-notes.md`.

### THREE CRT linkages, one per EXE (proven 2026-08-27)
BRally=**/MD** (dynamic, FF 15 via MSVCRT.dll), SetVideo=**/ML** (static
single-thread, E8 via libc), BossRally=**/MT** (static multithread, E8 via
libcmt). ALWAYS read the imports/bytes per binary; the BRGlide /MD convention
does NOT generalize. Win32/OLE calls stay FF 15 dllimport in all of them.

### Key idiom (non-obvious, per-binary)
**CRT linkage VARIES by EXE and dictates the call form.** BRally=/MD (dynamic
CRT, FF 15 dllimport). SetVideo=/ML (static CRT, E8 direct). Read the imports/
bytes per binary before matching - do NOT assume the BRGlide /MD convention.
Static-CRT `.text` (SetVideo's majority) is a wall class like BRD3D's linked
CRT - match the project lead-region, fence the CRT.

### Resume
Verify a match: `g._score_source(open('build/<exe>_work/<VA>.c').read, fn,
open('build/brally/win32/match/orig_<exe>/<VA>.bin','rb').read, [...])`. Integration
(deferred): create `src/brally/` EXE modules, fold in every `*_work` match, extend
the sweep to the EXE bins. See [matching-progress](../log/matching-progress.md), [glide-is-the-reference](../rules/glide-is-the-reference.md).
