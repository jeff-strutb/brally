# EXE integration — tree-resident matches (2026-08-27)

The three in-scope EXEs (BRally.exe, SetVideo.exe, BossRally.exe) were
matched as `build/{brally,setvideo,bossrally}_work/<VA>.c`. Those TUs
are now tree-resident under `src/exe/<exe>/` and counted from
`build/match/report_exe.csv`. This is **additive**: it does not touch
the DLL sweep, `build/match/report.csv`, `include/`, or any existing
`src/` module.

## Layout

```
src/exe/brally/0x00401000.c     # one function per VA
src/exe/setvideo/0x00401000.c
src/exe/bossrally/0x00401000.c
tools/exe_sweep.py              # EXE-only scorer
build/match/report_exe.csv      # EXE-only report
build/match/orig_brally/        # extracted .text (unchanged)
build/match/orig_setvideo/
build/match/orig_bossrally/
```

Wall attempts stay in the work dirs. `tools/exe_sweep.py --ingest-work`
copies a work `.c` into `src/exe/` only when `ghidra_to_match._score_source`
returns 0 against `build/match/orig_<exe>/<VA>.bin`. Score ≠ 0 is excluded.

## CRT flags (load-bearing)

The three binaries do not share a CRT. `exe_sweep.py` passes the matching
cl flag; mixing them emits the other binary's call form.

| binary | flag | CRT | call form |
|---|---|---|---|
| BRally.exe | `/MD` | `MSVCRT.dll` | `FF 15` IAT |
| SetVideo.exe | `/ML` | static libc | `E8` relative |
| BossRally.exe | `/MT` | static libcmt | `E8` relative |

Opt shapes are the DLL trio (`/O2`, `/Od`, `/O2 /Oy-`) plus the CRT flag.
Every current match is `/O2` + CRT.

BRally TUs also `#define _CRTIMP __declspec(dllimport)` before the CRT
headers (same as BRGlide.dll). SetVideo and BossRally must **not** — that
define is what turns `E8` into `FF 15`.

## `@implements` tag

```
/* @implements 0x00401000 brally.exe FreeObjList */
```

The `.exe` in the middle token is load-bearing. `match_sweep.sources()`
walks **all** of `src/` for the substring `@implements`.
`match_diff.parse_implements` requires `0xVA word word`; `brally.exe`
fails that parse (the `.` stops `\w+`), so a DLL full-sweep cannot score
these TUs against BRGlide orig bins. Do not write
`@implements 0x00401000 brally FreeObjList` — that *would* parse, look up
`build/match/orig/0x00401000.bin`, and pollute `report.csv` with `no_orig`.

`tools/match_sweep.py` is not modified. A file under `src/exe/` with an
`@implements` substring still appears in `sources()`; `sweep_file` then
returns `[]` immediately. Harmless.

## Counting

`tools/total.py` `score_exe()` reads `report_exe.csv` (and will run
`exe_sweep.py` once if the report is missing), the same way `score_cpp()`
reads `report_cpp.csv`. It does **not** walk `build/<exe>_work`.

```
python3 tools/exe_sweep.py              # src/exe → report_exe.csv
python3 tools/exe_sweep.py --summary
python3 tools/total.py                  # EXE line from report_exe.csv
```

`exe_matches.csv` remains a `total.py` manifest (exe, va, bytes) for the
progress map. It is derived from `report_exe.csv`, not a second scoring
pass.

## Totals (byte-exact)

28 + 37 + 35 = **100** functions, 2,860 + 4,675 + 2,482 = **10,017** bytes.

| EXE | fns | bytes | of `.text` |
|---|---:|---:|---|
| BRally.exe | 28 | 2,860 | 2,860 / 3,584 |
| SetVideo.exe | 37 | 4,675 | 4,675 / 36,864 |
| BossRally.exe | 35 | 2,482 | 2,482 / 23,552 |
| **in-scope EXE** | **100** | **10,017** | 10,017 / 64,000 (3,584+36,864+23,552) |

The grand total line (`TOTAL byte-exact`) is DLL C (`report.csv`) + C++ EH
(`report_cpp.csv`) + this EXE row. These 100 functions are 10,017 of the
**692 functions · 62,136 bytes** grand total.

User-region leftovers are walls, not missing copies: BRally 11 CRT/linker
slices; SetVideo WinMain + four coloring walls + static CRT; BossRally
static CRT past `0x401BBF`. See `docs/brally-exe-notes.md`,
`docs/setvideo-exe-notes.md`, `docs/bossrally-exe-notes.md`.

## Adding a new EXE match

1. Land a 0-diff TU in `build/<exe>_work/<VA>.c` (same header convention
   as the existing work files).
2. `python3 tools/exe_sweep.py --ingest-work` copies it to
   `src/exe/<exe>/<VA>.c` with the `@implements` tag and refreshes
   `report_exe.csv`. Or copy by hand, add the tag, and run `exe_sweep.py`
   on that file.
3. Do not grind the walls listed in the per-EXE notes.
