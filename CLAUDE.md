# Boss Rally — matching decomp

Read this before touching anything. These are hard rules, not preferences.

**Before matching any function, read `docs/VC5-IDIOMS.md`** — proven
construct→codegen mappings. Infer the source from the bytes; never permute
spellings by trial and error. Add every newly proven idiom to that file:
each idiom is solved once, and this is how per-function cost drops.

**The matching cadence (established 2026-08-25):** machine batch → hand-solve
one representative per failure class → mint a generator → re-batch. Never
hand-match what a generator could sweep. The wide batch is
`tools/ghidra_to_match.py --refine --max-diffs 200 --min-size 16` (hours,
zero tokens, crash-safe); `tools/autofile.py` files, sweep-verifies, and
commits every MATCH; `--residue` groups the failures by divergence class.
When a function is blocked on WHAT THE SOURCE SAYS (short/dense/frame
classes: unknown structure, suspected inlined helper, recomp ≪ orig,
ambiguous widths/signedness/arg order), **read its N64 twin first**: Top
Gear Rally (`reference/tgrally/`) is the same source lineage, IDO MIPS is
near-transparent, and 195 shared debug strings pair functions with
certainty — see the `tgr-n64-viability` memory note. The N64 does NOT move
register-allocation walls (scattered class) — don't burn time there.

## 0. BRGlide.dll is the reference binary. NOT BRD3D.dll.

`BRGlide.dll` (3dfx Glide) is the mature target and the reference. `BRD3D.dll`
statically links ~100 KB of CRT that has to be identified and fenced off.
Pairing one binary's bytes with the other's function map disassembles the wrong
bytes at a right-looking address, which is worse than failing outright.

**This error has been made and corrected TWICE** — `d98f480` (2026-08-15)
repointed the disassembler at Glide and said explicitly it was contrary to the
project's stated choice; the matching pipeline in `a7eb7cd` (2026-08-19)
re-made it. As of 2026-08-22 `build/match/orig/` and `build/match/report.csv`
are still D3D-keyed and need re-keying via `config/shared.csv` (2,697 rows
mapping `d3d_va` → `glide_va`).

**Verify, do not assume — tool defaults have been wrong before:**

```bash
python3 tools/refcheck.py        # fails loudly if the corpus is not Glide-keyed
```

Tools honour `BR_REF` / `BR_MAP` overrides.

## 1. The goal is a complete, MAME-standard, bit-exact decomp — by the most efficient path.

Same source, two build targets: bit-identical under MSVC 5.0 (the original
compiler), and cross-compiling to macOS/Metal as a port. The SM64 model.

**Playability is a side effect. It never drives prioritization.** Do not
reorder work to make something run.

## 2. `@implements` means the bytes diff clean against the original.

Not "passes the x87 emulator". Not "the tests are green". Not "it looks right".
If it does not reproduce the original bytes, it does not carry the tag.

## 3. No token thrashing.

Read once, decide once. No guessing, no re-deliberating settled questions, no
writing code that gets reverted. **Read the docs before forming a plan** — this
file, `README.md`, and the memory index (`docs/MEMORY.md`). A session spent on a wrong assumption
that one file read would have caught is the most expensive failure mode here.

## 4. Every number carries its denominator.

"290 matched" and "2% of the code section" are the same fact. Quoting the first
alone overstates progress by more than an order of magnitude. State what a
count is *of*: tagged functions, all known functions, or bytes of `.text`.
Never mix strictness levels — encoding-match, address-verified, and
independently-verified are three different numbers.

## 4b. Be CONCISE. Always.

Lead with the answer. No preamble, no recap of what was just done, no novel.
Numbers carry denominators; prose carries nothing else. If the user asks a
yes/no, the first word is yes or no. This is a hard rule, not a style note.

## 5. The toolchain lives in the repo.

Wine and MSVC 5.0 are staged inside the tree by `setup.sh`. **Never install to
the host.**

## 6. File each function into its named module as you match it.

The `sliceN_MM.c` files are unfiled address batches. Move each function to its
real module when you match it. Never a big-bang reorg later.

## 7. Commit every verified match immediately.

Especially parallel agents. A killed worker must leave its verified matches
behind, not nothing. Never stage work behind a revert step.

## 8. Never add `Co-Authored-By: Claude` trailers.

## 9. Never full-sweep to do ordinary work.

Making one function bit-exact needs only its own file compiled and diffed
(~12s). `report.csv` is self-maintaining — every run, single-file included,
merges its rows back. The full sweep is bookkeeping, and it takes ~20 minutes.

## 10. Header edits are serialized. Never concurrent.

Shared headers under `include/` reach dozens of files. Parallel work splits by
`.c` file only.

## 11a. 0x1000EAF0 is one wall from done. Do not re-derive it.

`src/core/drawing/br_scenedl.c` holds the second-largest function at
9,264/9,354 bytes with every structural element verified. Its remaining
~15-instruction float wall and the 60+ probe variants that FAILED are
mapped in `docs/VC5-IDIOMS.md` (the 0x1000EAF0 entries). Read the file
header and those entries before touching it; re-running mapped probes is
token waste. `tools/divergence.py` is the comparator. Fresh leads only:
compiler patch level, unprobed pragmas. The sweep compiles NOTHING when a
file has no `@implements` tag — a probe without a fresh compile proves
nothing (this trap invalidated five conclusions once already).

## 11. 0x10019A70 is last among the big targets.

11,223 bytes — 11,223 / 480,853 of BRGlide.dll `.text` (2.33%). One original
function, so one C function; the port's Clock/Begin/Frame split is not a
matching twin. Win `sub esp, 0x34` first (no 8-byte-aligned local; ebp is a
general register, `xor ebp,ebp`). Grow section by section against the first
divergence. Gated on 131 callee signatures. Not "never" — prologue-first,
and last. See `docs/VC5-IDIOMS.md` (`and esp,-8`) and `include/br_racestep.h`.

---

## Before you start any session

1. Run `python3 tools/refcheck.py` and believe the result.
2. Read the memory index (`docs/MEMORY.md`); it carries current state and open leads.
3. Do not trust a coverage number in prose — including in `README.md`.
   Query the tree.

## Scope — which shipped binaries are being decompiled

Established 2026-08-22 by reading each binary's imports, exports and strings,
not by inference from filenames. Until then nothing recorded this, and the
matching pipeline had silently covered exactly one file. Silence is not a
decision; if scope changes, change it *here*.

**IN SCOPE**

| binary | .text | what it is |
|---|---|---|
| `BRGlide.dll` | 480,853 | **The game.** Imports 38 entry points from `glide2x.dll`, plus DirectInput, DirectPlay (multiplayer) and WINMM audio. Exports `RallyMain`. This is the primary target. |
| `BRally.exe` | 3,584 | **The launcher.** Reads the registry, loads `BRGlide.dll` or `BRD3D.dll`, calls `RallyMain`. The code that chooses the renderer. |
| `SetVideo.exe` | 36,476 | Renderer/display config utility; writes settings into `BossRally.ini`. Shares the game's file-check and list-parsing helpers, so part of it matches for free once the DLL's config layer lands. |
| `BossRally.exe` | 23,552 | Plays `brally.avi`, then launches `brally.exe`. An intro shim — borderline, but shipped game code. Lowest priority. |

In-scope EXE code is ~64 KB against 481 KB of game DLL: about 12% of the
target. As of 2026-08-22 **none of the EXEs has been started.**
`config/functions_boot.csv`, `functions_bossrally.csv` and `functions_brally.csv`
exist but their name columns are empty — maps were generated, never worked.

**OUT OF SCOPE**

| binary | why |
|---|---|
| `BRD3D.dll` | Direct3D twin of the game. Reference/cross-check only — see rule 0. Its `.text` is ~100 KB larger than Glide's *because it statically links the CRT*: it imports no `MSVCRT.dll` at all, while `BRGlide.dll` imports 57 CRT functions dynamically. Matching D3D means matching Microsoft's CRT as a side effect. This is the evidence for rule 0, not just a preference. |
| `Boot.exe` | CD autorun / installer front-end. Runs `setup.exe`, DirectX Setup, DXMEDIA, `setvideo.exe`. Imports COMCTL32, SHELL32, WINSPOOL, comdlg32 — a Windows dialog app. Its 96 KB is the largest EXE and is **not game code**. **Re-confirmed 2026-09-03, and the reason is stronger than "not game code": it statically links MFC 4.2** (`AfxWnd42s`, `AFX_MODULE_STATE`, `Microsoft Visual C++ Runtime Library` in `.rsrc`; it imports neither `MFC42.dll` nor `MSVCRT.dll`). Decompiling it means decompiling Microsoft's MFC — the same trap as `BRD3D.dll`'s static CRT under rule 0, and MFC is far bigger. Its own logic is small: 1,453 functions in `.text`, and the code that actually launches anything (registry read, `setup.exe`/`brally.exe`/`setvideo.exe`/DirectX spawn) is **3 functions / 745 B** in the first 3 KB. Nothing here is shared with the game. |
| `REMOVE.EXE` | Uninstaller stub; spawns `IASINST.EXE`. |
| `SETUP.EXE`, `_ISDEL.EXE`, `_SETUP.DLL` | 16-bit NE binaries, InstallShield. Not PE32, not game code. |

## Layout

**The repo root IS the decomp** (the master, byte-matched against the 1999
binaries). `ports/` is the only thing outside it — derived platform layers that
are NOT byte-matched. Don't wrap the decomp in a `decomp/` subfolder: the
toolchain (32 tools compute ROOT as the parent of `tools/`, plus the staged
Wine/MSVC and venv) assumes the decomp is at root, which is also the decomp-
project convention (SM64). See `ports/README.md`.

- `src/core/` — portable game logic (byte-matched)
- `src/backends/{glide,d3d,win32}` — original **Win9x** platform backends
  (byte-matched; currently mostly filed into `src/core` modules)
- `src/exe/` — the three Win9x executables (launcher / config / intro),
  byte-matched
- `include/`, `tests/`
- `config/` — function maps, globals, `shared.csv` (d3d↔glide twins),
  `binaries.csv` (per-binary compiler + CRT model + base + entry — the build
  spec), `fenced.csv` / `fenced_exe.csv` (linker/CRT reproduced-by-linking,
  not a decomp target)
- `build/match/` — extracted reference bytes, per-function report, objs
- `tools/` — matching pipeline, auditors, the staged MSVC toolchain
- `ports/macos/` — the macOS/Metal port: NEW platform code, not byte-matched,
  no `@implements`, invisible to the match tooling. Built by `build.sh`.
