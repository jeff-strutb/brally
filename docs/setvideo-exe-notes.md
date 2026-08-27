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
| 0x00401C10 | 91 | DlgProcOKCancel | **0** | last inner case (IDCANCEL) falls through to `return 0` |
| 0x00401C70 | 332 | DlgProc | **0** | symptoms; CheckDlgButton as if/else duplicate calls (not `==0` sete) |
| 0x00401DC0 | 251 | DlgProcRadio | **0** | method picker; CheckRadioButton spelled per arm |
| 0x00401EC0 | 59 | ComboGetItemData | **0** | `CB_GETCURSEL` then `CB_GETITEMDATA` on ctl 0x3E9 |
| 0x00401F00 | 298 | FillComboA | **0** | `[v:` sections, strncpy `strlen(s+1)-1` capped at 0x4F |
| 0x00402030 | 298 | FillComboB | **0** | same, `[c:` chipset |
| 0x00402160 | 248 | DlgProcComboA | **0** | vendor combo; merged EndDialog, `if (idx >= 0)` |
| 0x00402260 | 248 | DlgProcComboB | **0** | chipset combo; same IDOK shape, FillComboB |
| 0x004023B0 | 205 | GetIniValue | **0** | BRally for-init latch, no `_CRTIMP` |
| 0x00402CE0 | 64 | GetSectionNameByIndex | **0** | `f(idx, pini)` — FindFirst, FindNext idx times, GetObj, free |

## SetVideo-only, identified, not 0 — walls, do not grind

| SV VA | size | name | diffs | class |
|---|---:|---|---:|---|
| 0x00401150 | 217 | CHK_FGets | **192** | getc `_cnt/_ptr/_filbuf` is right; `n<=0` is `test ebp,ebp; jle` to a tail that loads `buf` *after* the four pushes. Loading `buf` before `push edi` flips `jle`/`jg` and the ret-s merge with CR+EOF. Allocator-layout wall. |
| 0x00401560 | 71 | FindFirstSection | **4** | edx vs ecx for pini. Rest byte-identical to SetSubstituteDir + `[0]=='['`. Coloring: no `name` arg, so pini lands in edx and `mov edx,[edx]` overwrites it with list. |
| 0x004015B0 | 70 | FindNextSection | **3** | ecx vs edx for pini. Indexed `list->rgsz[i][0]` (`while (i < n)`). Pointer walk (`s = rgsz+i; do`) peels the first load (`+8`, 44 diffs). Same coloring class as FindFirst. |
| 0x00402360 | 74 | FollowUse | **11** extra +16 | esi/edi (p vs use). Orig: pini ebx, p edi, use esi; `test esi,esi; jne` back to GetIniValue. `for(;;)`+break is the CFG; `do-while(use)` duplicates GetIniValue (+20 B). Decl swap / ini local stay 11/+16. Coloring. |
| 0x00402480 | 2144 | WinMain | **644** / extra −16 | **one** function. Map size 930 is truncated mid-body. Orig bytes `build/match/orig_setvideo/0x00402480.bin` (2144). Prefix through Card= search is opcode-identical except 4 stack-slot bytes. Wizard half is coloring (`inc eax` vs `lea [ebx+1]`, psec/card vs vsave slots). See below. |

WinMain is **one** function. Map-splits at non-prologue boundaries (do not match as C):

| SV VA | what |
|---|---|
| 0x004027FA | OK/Cancel `DialogBoxParamA(DlgProcOKCancel)` — also jump-table slot 0 |
| 0x00402822 | method `DialogBoxParamA(DlgProcRadio)` |
| 0x00402838 | `switch(result+1)` → table at 0x402CC0 |
| 0x00402850 | symptoms `DlgProc` template `0x6a` |
| 0x00402868 | vendor `DlgProcComboA` template `0x66` |
| 0x004028DB | chipset `DlgProcComboB` template `0x69` |
| 0x0040294F | WriteDefaultINI (starts `test eax,eax` — symptoms dialog result) |
| 0x00402AC0 | WriteVideoINI (vendor card via GetSectionNameByIndex + FollowUse) |
| 0x00402BAE / BC5 | `FreeINI(gINI); return 0` shared epilogue |
| 0x00402BCE | WriteVideoINI2 (chipset, same write shape) |
| 0x00402CC0 | jump table (`result+1`): 0→OK/Cancel `0x4027FA`, 1→cancel `0x402BA8`, 2→symptoms, 3→vendor, 4→chipset |

Dialog templates: OK/Cancel `gPlusD ? 0x67 : 0x6c`; radio `gPlusD ? 0x68 : 0x6b`; symptoms `0x6a`; vendor `0x66`; chipset `0x69`. Radio Back (−1) returns to OK/Cancel; other Backs return to radio. First OK/Cancel cancel is `return 0` **without** FreeINI (`je 0x402BB6`).

WinMain reconstruction (`build/setvideo_work/0x00402480.c`) is the function. Proven this pass:

- Nested `plus = strstr(lpCmdLine, "+d"); gPlusD = plus != 0;` → orig `xor ecx,ecx; test eax,eax; setne cl`. Bare `gPlusD = strstr(...) != 0` is `neg; sbb; neg`.
- `char buf[0x400]` (same as CHK_FReadOpen) → `sub esp, 0x528`. `buf[0x3f0]` is 16 short: the 0x3f0 figure was `frame - buf_offset` forgetting the 4 register pushes. line at `+0x38`, buf at `+0x138`, lpCmdLine at `+0x544`.
- Card= walk is `for (i = 0; i < gSectionCount; i++)` with inlined strcmp. That emits orig's `jmp +4` over `mov esi,[card]` (strcmp clobbers esi). `do-while` peels the body (+104 B).
- Dialog templates: `if (gPlusD) result = DialogBox(..., 0x67, ...); else result = DialogBox(..., 0x6c, ...); if (result == 0) return 0;` merges to orig's branch-only-the-push. A ternary in the arg is `neg/sbb/and`. `if (DialogBox==0) return` *inside* each arm duplicates the call.
- INI writes: `CHK_FPutS("[Video]"); CHK_FPutS("\n");` not `"[Video]\n"`. Then `Card` / `=` / value / `\n` as separate calls.

Residue (do not grind): 4 slot bytes in the Card= block (orig psec at `[esp+0x34]`, card at `+0x30`; recomp reuses those slots for vsave — live-range packing). After radio, orig `mov ebx,eax; lea eax,[ebx+1]` vs recomp `inc eax` (−4 B) then a −16 size cascade. Same coloring class as FindFirst edx/ecx. Not 0 — leave it.

## Idioms proven here

- **Last switch case falls through.** `case IDCANCEL: EndDialog(h, 0);` with **no** `return 0` — the outer `return 0` is the fall-through. An explicit `return 0` in the last case outlines it with `je` + a 5-byte xor-ret (53 diffs on DlgProcOKCancel). Outer `switch(msg)` **does** emit `sub eax, 0x110; je; dec; jne`. Inner `switch(wParam & 0xffff)` emits `and eax, 0xffff; dec; je IDOK; dec; jne default`.
- **CheckDlgButton 0/1 is if/else duplicate calls**, not `CheckDlgButton(h, id, g==0)` (that is `sete`). Cross-jump merges the call; orig is `test; jne push0; push 1; jmp; push 0`.
- **CheckRadioButton per arm**, not `id = …; CheckRadioButton(h, first, last, id)` (that is `neg/sbb` on the default `gPlusD` ternary). `switch(lParam) { case 2: Check(…, 0x3EC); return 1; case 3: Check(…, 0x3ED); return 1; }` then `if (gPlusD==0) Check(…, 0x3EC); else Check(…, 0x3EB);`.
- **GetIniValue latch is a `for`**, same as BRally. Drop `_CRTIMP`.
- **FillCombo strncpy**, not atoi (map misnamed 0x4036A0). `n = strlen(name+1)-1; if (n >= 0x4f) n = 0x4f; strncpy(buf, name+3, n); buf[n]=0;` then CB_ADDSTRING / SETITEMDATA(i) / SETCURSEL if `Sel.index==i`. Item data is the FindFirst/Next ordinal, not `psec->index`.
- **GetSectionNameByIndex(int idx, INI *pini)** — pini is the **second** arg. `if (idx > 0) do { p = FindNextSection(p); idx--; } while (idx);`.
- **Combo IDOK keeps `ok` live by merging EndDialog.** Two EndDialog sites (`if (idx < 0) { EndDialog(..., ok); return; } ok=1; EndDialog(..., ok)`) const-fold `ok` to `push 0`/`push 1`, drop ebp, extra −8 (IAT in ebx, 3 callee-saved). `if (idx < 0) saved; else { ok=1; index=idx; } EndDialog(ok)` keeps `ok` in edi / IAT in ebp / 4-reg prologue but **inverts** `jl`/`jge` (29 diffs). Winning form: `ok=0; idx=ComboGetItemData(h); if (idx >= 0) { ok=1; sel->index=idx; } else sel->index=sel->saved; EndDialog(h, ok);` — success is the true branch so orig `jl fail` is fall-through success, and the store-between-pushes on the fail path is scheduler output (no comma needed).
- **FindNextSection is indexed, not a pointer walk.** `while (i < n) { if (list->rgsz[i][0]=='[') …; i++; }`. `s = rgsz+i; do { *s; s++; }` peels (`mov edi,[edx+ecx*4]; lea edx,[edx+ecx*4]`).
- **WinMain `switch (result + 1)`**, not `switch (result)`. Orig `mov ebx,eax; lea eax,[ebx+1]; cmp eax,4; ja epilogue; jmp [eax*4+0x402CC0]`. Recomp `inc eax` is the same switch with result in eax not ebx (coloring, −4 then a size cascade). DialogBoxParamA IAT cached in edi across the wizard. `or ebx,0xffffffff` is both the scasb count and the `Sel.index = Sel.saved = -1` value.
- **`gPlusD = plus != 0` needs a temp.** `plus = strstr(...); gPlusD = plus != 0` is `xor ecx,ecx; test eax,eax; setne cl`. The call in the assignment (`gPlusD = strstr(...) != 0`) is `neg; sbb; neg`. Same idiom as dll2 `return f() != 0`. Declare `plus` in a nested block so it does not steal a function-wide register.
- **`char buf[0x400]`**, not `0x3f0`. Frame is `sub esp, N` *then* 4 pushes; locals are accessed from post-push esp. `N = 0x528` with line at `+0x38` and buf at `+0x138` means buf is 0x400, ending at post-push `+0x538` = `0x10 + 0x528`. Computing `0x528 - 0x138 = 0x3f0` drops the push shift.
- **Card= search is `for (i = 0; i < n; i++)`.** Inlined strcmp clobbers esi (the card pointer); orig reloads it at the continue point with `jmp +4` over the first-iter load. `do { ... i++; } while (i < n)` peels the body (+104 B).
- **DialogBoxParamA template if/else, compare after.** `if (gPlusD) result = DialogBox(..., 0x67, ...); else result = DialogBox(..., 0x6c, ...); if (result == 0) return 0;` merges to orig `test; push proc; push hwnd; je; push 0x67; jmp; push 0x6c; call; test; je`. Ternary in the arg is sbb; `return 0` inside each arm duplicates the call. Same for radio `0x68` / `0x6b`.

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

WinMain outline (structurally identified, not 0-diff): GetDesktopWindow →
GetInstallDir → strcpy/strcat `BossRally.ini` → `strstr(lpCmdLine, "+d")` →
CHK_FileExists(`BossRally.vdb`) → MessageBox on miss → ReadINI / CountSections
→ FReadOpen+FGets parse of existing INI (four `D3D*` keys) → Card= vs VDB
section names → wizard dialogs → WriteDefaultINI / WriteVideoINI /
WriteVideoINI2 → FreeINI.

WriteDefaultINI (`wt`): `CHK_FPutS("[Video]"); CHK_FPutS("\n");` then `Card` / `=` / `[Set via symptoms (use Direct3D)]` / `\n`, then
`Driver=D3D`, `D3DAlphaCompare=%d`, `D3DAlwaysSquareTextures=0`,
`D3DClearZBuffer=%d`, `D3DDrawCarShadow=%d`, `D3DWaitCanFlip=0`,
`D3DWaitFlipDone=0`, `D3DInvSrcAlpha=%d`. WriteVideoINI dumps `[Video]` +
`Card=<section name>` then FollowUse/BindSection/NextObj lines of the VDB
section.

## Matching totals

**37 / 342** functions at diffs=0 (**4,675 / 36,864** of `.text`, **12.7%**).
No new 0-diff this pass — remaining user functions are walls.

Of user-region code (`0x401000`–`0x4038D0` = 10,448 B before CRT startup),
the 0-diff set is **4,653 / 10,448 (44.5%** of that span) including
`CRT_empty` (1 B at `0x403140`). Game user (`0x401000`–`0x402D20` = 7,456 B)
is **4,652 / 7,456 (62.4%)**. Remaining game bytes: WinMain 2,144 + four
walls 432 (CHK_FGets 217 / FindFirst 71 / FindNext 70 / FollowUse 74) +
228 B alignment. Walls: table above (CHK_FGets / FindFirst / FindNext /
FollowUse / WinMain).

The other **~26 KB** is statically-linked MSVC 5.0 CRT starting at `0x402D20`
(~292 / 342 map rows). Fence, don't match — walls, not targets. Early CRT in
the user-region span (`0x402D20`–`0x4038D0` = 2,992 B): `free` / `exit` /
`fclose` / `fopen` / `sprintf` / `_filbuf` / `ungetc` / `fputs` / `malloc` /
`printf` / `_chkstk` / `strchr` / `strncmp` / `fgets` / `strtol` (map-named
`atoi` at `0x4036A0`) / `atoi` wrapper (`0x403840`, 14 B, `mov eax,[esp+4];
push eax; call 0x4037A0`) / `strstr`. `WinMainCRTStartup` at `0x4038D0` is
one CRT function map-split into 7 entries at non-prologue boundaries (same
class as BRally). From `0x403A70` through `_stricmp` at `0x409DF0` is heap,
stdio, locale, and math. Three tiny CRT stubs already match (`CRT_empty`,
`_matherr`, `_setdefaultprecision`); the rest is Microsoft's, not game code.

Winning TUs: `build/setvideo_work/0x<VA>.c`. Orig bytes:
`build/match/orig_setvideo/`. 79 / 342 map rows have names.

## CRT-header rule (SetVideo)

```
/* SetVideo.exe is /ML (static CRT): CRT calls are E8, not FF 15. */
#include <windows.h>
#include <stdlib.h>
…
```

Do **not** `#define _CRTIMP __declspec(dllimport)` — that is the BRally/DLL
convention and would emit `FF 15` against this binary's `E8`s.

Globals used by the dialog/INI layer (VA in `.data` / bss):

| VA | name |
|---|---|
| 0x40B29C | gD3DAlphaCompare (inverted checkbox 0x3EE) |
| 0x40B2A0 | gD3DDrawCarShadow (inverted checkbox 0x3EF) |
| 0x416BC0 | gD3DInvSrcAlpha (checkbox 0x3F0) |
| 0x416BBC | gD3DClearZBuffer (checkbox 0x3F1) |
| 0x416BC4 | gPlusD (`strstr(lpCmdLine, "+d")`) |
| 0x416BB0 | gINI (ReadINI of BossRally.vdb) |
| 0x40E998 | gSectionCount (CountSections) |
| 0x4169A0 | Sel.saved |
| 0x4169A4 | Sel.index (combo item data / section ordinal) |
| 0x4169A8 | Sel.method (0 / 2 vendor / 3 chipset) |
| 0x40E090 | gInstallDir |
| 0x40E198 | gIniPath (install dir + `BossRally.ini`) |
| 0x40E598 | gLineBuf |

Dialog resource IDs (MAKEINTRESOURCE):

| ID | dialog |
|---|---|
| 0x66 | vendor combo (DlgProcComboA) |
| 0x67 / 0x6c | OK/Cancel (`gPlusD` ? 0x67 : 0x6c) |
| 0x68 / 0x6b | method radio (`gPlusD` ? 0x68 : 0x6b) |
| 0x69 | chipset combo (DlgProcComboB) |
| 0x6a | symptoms (DlgProc) |
