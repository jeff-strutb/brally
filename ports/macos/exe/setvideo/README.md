# setvideo — macOS port of SetVideo.exe

The first shipped Boss Rally executable to run natively on macOS. It runs the
**original 1999 code**: 29 of SetVideo.exe's 42 byte-exact functions are
compiled verbatim out of `src/exe/setvideo/`, unmodified, with
`BR_MATCHING_BUILD` defined. Only the shell around them is new.

Nothing in this directory is byte-matched, nothing carries `@implements`, and
the match tooling never sees it. Building it does not touch the matching lane.

```bash
./ports/macos/exe/setvideo/build.sh
```

Output: `build/macos/setvideo/setvideo`.

## What runs, and what was replaced

**Compiled from the matched tree (29 functions, ~5.2 KB of the original's
7,228 B `.text`)** — the whole `.vdb` / `.ini` reader and everything under it:
`ReadList`, `ReadListLine` (including `#include` nesting), `ReadINI`,
`FindFirstSection` / `FindNextSection` / `NextObj` / `GetObj` / `BindSection`,
`GetIniValue`, `FollowUse`, `GetSectionNameByIndex`, `CountSections`, and the
`CHK_*` checked-file and allocation helpers.

**Not ported (13 functions):**

| function | why |
|---|---|
| `0x00401B30 GetInstallDir` | reads `HKLM\SOFTWARE\SouthPeak Interactive\Boss Rally\Directory`. Replaced by `--dir`, defaulting to the working directory. |
| `0x00401C10` `0x00401C70` `0x00401DC0` `0x00401EC0` `0x00401F00` `0x00402030` `0x00402160` `0x00402260` | the five wizard dialog procedures and their combo fillers. There is no Win32 message loop here, and the five dialog templates still live only in the original binary's `.rsrc` (0x1b34 bytes) — they have never been extracted into this repo, so there is no UI to draw. |
| `0x00402480 WinMain` | the dialog driver. `main()` in `setvideo_port.c` replaces it and reproduces its read and write paths statement for statement. |
| `0x00403140` `0x00405940` `0x00406C85` | MSVC CRT hooks (`_setdefaultprecision`, the empty init hook, `_matherr`). No meaning outside the MSVC startup. |

## Usage

```
setvideo --list                    # every device-database section
setvideo --vendors                 # what the first drop-down showed
setvideo --chipsets                # what the second drop-down showed
setvideo --show                    # the current BossRally.ini selection
setvideo --set "Riva TNT"          # pick a card, write BossRally.ini
setvideo --set 7                   # …or by database ordinal
setvideo --symptoms --alpha-compare 1 --clear-zbuffer 0 \
         --draw-car-shadow 1 --inv-src-alpha 0
setvideo --dir <path>              # where BossRally.ini lives (default: .)
setvideo --vdb <path>              # device database (default: <dir>/BossRally.vdb)
setvideo -v                        # the original's gChkVerbose file trace
```

`--set` accepts the full bracketed section name (`[c:Riva TNT]`) or the bare
text the drop-down displayed (`Riva TNT`).

A sample device database is in `sample/BossRally.vdb`. It is **hand-written for
this demo** — the file that shipped with the game is not in this repository —
but it exercises every feature the matched reader has: `[v:` / `[c:` prefixes,
`;` comments, `Use=` section redirection and `#include`.

```
$ cp ports/macos/exe/setvideo/sample/BossRally.vdb .
$ build/macos/setvideo/setvideo --set "3Dfx Interactive"
Wrote ./BossRally.ini
  Card = [v:3Dfx Interactive]
$ cat BossRally.ini
[Video]
Card=[v:3Dfx Interactive]
Driver=Glide
GlideResolution=640x480
GlideTripleBuffer=1
GlideMipMapping=1
```

The four `Glide*` lines are not in `[v:3Dfx Interactive]` — that section is
`Use=[Generic Glide]`, and the original `FollowUse` resolved the redirection.

## The two shims

`include/windows.h` is the only thing standing between the matched sources and
clang. Two entries in it are load-bearing rather than cosmetic:

**`FILE::_flag`.** `ReadListLine` tells "this include file ended" from "the
read failed" by testing MSVC's `FILE::_flag & _IOEOF` (0x10). BSD stdio spells
the same bit `FILE::_flags & __SEOF` (0x20), so the shim rewrites the member
access. Get this wrong and every `#include` in a `.vdb` ends in `exit(1)`.

**ILP32 → LP64 allocation sizes.** The matched code sizes its own allocations
for a 32-bit target: `8` for `{pointer, int}` structs, `0xc` for `ObjList`,
`n * 4` for an array of `n` pointers. On arm64 a pointer is 8 bytes and
`ReadList` runs straight off the end of `rgsz`. The shim doubles every request
through `CHK_AllocateMemory`, which is the exact correction for all three
pointer-bearing cases (8→16, 0xc→24, n*4→n*8) and harmless slack for strings.
Removing it reproduces the overflow immediately:

```
ERROR: AddressSanitizer: heap-buffer-overflow
SUMMARY: heap-buffer-overflow 0x00401740.c:66 in ReadList
```

With the shim in place, every command above is clean under
`-fsanitize=address,undefined`.
