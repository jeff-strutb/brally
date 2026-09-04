# setvideo — macOS port of SetVideo.exe

The Boss Rally Display Wizard, running natively on macOS, driven by the
**original 1999 code**. Thirty-eight of SetVideo.exe's 42 byte-exact
functions are compiled verbatim out of `src/exe/setvideo/`, unmodified —
`WinMain` and all five dialog procedures included. Only the platform beneath
them is new.

Nothing in this directory is byte-matched, nothing carries `@implements`, and
the match tooling never sees it. Building it does not touch the matching lane.

```bash
./ports/macos/exe/setvideo/build.sh
```

Output: `build/macos/setvideo/setvideo`. Run it with no arguments for the
wizard, or use the command-line modes below.

## What actually runs

The dialog procedures are not reimplemented. `DlgProcRadio`, `DlgProcComboA`,
`DlgProcComboB`, `DlgProc` and `DlgProcOKCancel` still receive `WM_INITDIALOG`
and `WM_COMMAND`, still stash their selection pointer with
`SetWindowLongA(hWnd, 8, …)`, still fill the drop-down with `CB_ADDSTRING` /
`CB_SETITEMDATA` / `CB_SETCURSEL`, and still finish with `EndDialog`.
`win32_dialog.m` implements the other side of those eleven calls on AppKit —
that is the entire Win32 surface this program uses.

The dialog layouts are the real ones, read out of the retail binary's `.rsrc`
by `tools/rsrc_dump.py`: same captions, same control ids, same positions. All
seven templates are `DLGTEMPLATEEX`, which VC5's resource compiler emitted and
which reorders the header and widens each control id from a word to a dword —
decoding one with the classic layout produces plausible nonsense rather than
an error, so the decoder branches on the signature.

**Not ported (4 of 42):**

| function | why |
|---|---|
| `GetInstallDir` | reads `HKLM\SOFTWARE\SouthPeak Interactive\Boss Rally\Directory`. Replaced by `--dir`, defaulting to the working directory. |
| `_setdefaultprecision`, the empty CRT init hook, `_matherr` | MSVC startup plumbing with no meaning outside the MSVC CRT. |

## Assets

`build.sh` extracts what it needs from the retail disc image, the same way
`tools/extract_assets.sh` does, and commits none of it:

- **`BossRally.vdb`** — the device database, 78 sections, pulled to
  `testdata/setvideo/`. Without a disc image the build falls back to the small
  hand-written `sample/BossRally.vdb` and says so.
- **the seven dialog templates** — read from `orig/SetVideo.exe`, or from the
  copy on the disc if `orig/` is not staged, and generated into a C table
  under `build/macos/setvideo/gen/`.

The build still succeeds with neither. It just comes out headless, and says
that too — a missing asset must never look like a passing extraction.

## Command line

Running with no arguments starts the wizard. The other modes are headless:

```
setvideo --list                    # every device-database section
setvideo --vendors                 # what the first drop-down shows
setvideo --chipsets                # what the second drop-down shows
setvideo --show                    # the current BossRally.ini selection
setvideo --set "Riva TNT"          # pick a card, write BossRally.ini
setvideo --set 7                   # …or by database ordinal
setvideo --symptoms --alpha-compare 1 --clear-zbuffer 0 \
         --draw-car-shadow 1 --inv-src-alpha 0
setvideo +d                        # the wizard's three-option variant,
                                   # which adds the symptoms page
setvideo --dir <path>              # where BossRally.ini lives (default: .)
setvideo --vdb <path>              # device database
setvideo -v                        # the original's gChkVerbose file trace
```

`--set` takes the full bracketed section name (`[c:nVidia RIVA TNT (use
Direct3D)]`) or the bare text the drop-down shows. Only an argument that is
*entirely* digits is read as an ordinal — retail card names start with digits
("3Dfx Voodoo Rush …").

## Three shims, all load-bearing

`include/windows.h` is what stands between the matched sources and clang.
None of these three is cosmetic; each was found by the port failing.

**`FILE::_flag`.** `ReadListLine` tells "this include file ended" from "the
read failed" by testing MSVC's `FILE::_flag & _IOEOF` (0x10). BSD stdio spells
the same bit `FILE::_flags & __SEOF` (0x20), so the shim rewrites the member
access. Get it wrong and every `#include` in a `.vdb` ends in `exit(1)`.

**ILP32 → LP64 allocation sizes.** The matched code sizes its allocations for
a 32-bit target: `8` for `{pointer, int}` structs, `0xc` for `ObjList`,
`n * 4` for an array of `n` pointers. On arm64 a pointer is 8 bytes and
`ReadList` runs off the end of `rgsz`. The shim doubles every request through
`CHK_AllocateMemory`, the exact correction for all three pointer-bearing cases
(8→16, 0xc→24, n\*4→n\*8) and harmless slack for strings. Removing it
reproduces the overflow immediately:

```
ERROR: AddressSanitizer: heap-buffer-overflow
SUMMARY: heap-buffer-overflow 0x00401740.c:66 in ReadList
```

**CRLF.** The retail `BossRally.vdb` is a DOS text file and MSVC opens it in
text mode, whose CRT collapses each CRLF to a bare LF inside `fgets`. BSD has
no text mode. Without the shim every value keeps a trailing `\r` and every
*blank* line arrives as the one-character string `"\r"` — `ReadList` only
drops lines of one character, so the blanks became entries, and the original's
`GetIniValue` then correctly refused them:

```
Unable to parse  in section [c:3Dfx Voodoo (use Glide)].
```

`CHK_FGets` needs no equivalent; it reads with `getc` and already folds CR and
CRLF to LF itself.

## A bug faithfully reproduced

Every entry in the card drop-down ends in a stray `]`. That is not a porting
artifact — it is the shipped game's. `FillComboA`/`FillComboB` mean to strip
the `[c:` prefix and the `]` suffix, and compute the length as
`strlen(name + 1) - 1` where `strlen(name + 3) - 1` was intended. The original
bytes say so outright: `lea edi,[ebp+1]` / `repne scasb` / `dec esi`, then
`add ebp,3` before the copy. One character too many, in 1999 and now.

## Loose end

`config/functions_setvideo.csv` names `0x00401EC0` **`ComboGetItemData`**,
which is what `DlgProcComboA` and `DlgProcComboB` call; the decompiled source
defines it as **`ComboGetCurText`**. The two names never met before, because
byte-matching compiles one function at a time and nothing had ever linked
SetVideo's sources together as a program. `setvideo_port.c` bridges them
rather than renaming a matched function.
