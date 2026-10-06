# Boss Rally decompilation: project rules

These are the standing rules for the matching decompilation. Older notes and
commit messages cite them by number ("rule 6", "rule 11b"); the numbers are
kept. The working procedure is [MATCHING.md](MATCHING.md); proven compiler
idioms are in [VC5-IDIOMS.md](VC5-IDIOMS.md) and are queried, not reread:

```bash
.venv/bin/python tools/brally/corpus.py find --from <VA> --at <off> --len 12 --source
```

A corpus miss means the construct is not proven anywhere: go to source truth,
not another permutation. Newly proven mappings go on the tail of VC5-IDIOMS.md.

**Cadence.** Hand-solve one of a class, mint a generator, re-batch. Never
hand-match what a generator could sweep. The lanes that still pay are T1
intake (Pool B) and structural T2. Colouring walls (`reggap 0`) are T3, not a
grind. `tools/brally/crank.py` is an overnight lottery on Pool A only: never
`--all --loop`, never `--max-bytes` above 400. Giants are not a lottery.

The N64 decompilation (Top Gear Rally) is the oracle for commutative operand
order (IDO does not canonicalise; VC5 does). It does not move
register-allocation walls. Pairing is not by shared strings (7 usable, not
195).

## 0. The reference is BRGlide.dll, not BRD3D.dll

`tools/brally/refcheck.py` must say Glide-keyed. Tools honour `BR_REF` / `BR_MAP`.

## 1. Bit-exact under MSVC 5.0; the ports adapt, the decomp does not

Do not reorder matching to make something run. The decomp sources hold only
what MSVC compiles: no `BR_MATCHING_BUILD` / `_MSC_VER` conditionals (the hook
refuses one). The ports carry their own differences: ports/brally is a retyped
64-bit fork (ports/brally/tools/sync.py reports what to carry over), and
ports/brally-wasm compiles the MSVC arm as it is. A refactor of the decomp is
checked with `tools/brally/ppgate.py` (MSVC tokens unchanged).
(Until 2026-10-05 a legacy macOS harness also compiled the decomp through
per-file port specs; it was retired with the address-batch slices it ran on.)

## 2. `@implements` means the bytes diff clean. Nothing else.

## 3. Read once, decide once. Query the tree for counts; never a number in prose.

Every count carries its denominator (functions vs bytes of `.text`). Never mix
strictness (byte-exact, address-verified, independently verified).

## 4. Be concise. Yes or no first. Numbers with denominators.

## 5. The toolchain lives in the repository (`setup.sh`). Never install to the host.

## 6. A match says what it does and lives in its module

A `WHAT IT DOES:` comment sits directly above `@implements`, written when the
function is matched. `sliceN_MM.c` files are address batches: never create
one, never add a new VA to an existing one. File by hand, one function or
connected group; sweep both files; keep the move only if nothing regressed.
Surroundings decide codegen: carry the whole preamble.

```bash
python3 tools/common/install_hooks.py    # once per clone
python3 tools/brally/fileaudit.py        # ratchets: undescribed 0, batches 0, stranded 0
```

The pre-commit hook refuses a new `@implements` without `WHAT IT DOES:`, a new
`sliceN_MM.c`, or a new VA in an existing batch. After a refile, sweep both
files and keep the move only if nothing regressed. The sweep compiles nothing
for a file with no `@implements`.

## 7. Commit every verified match immediately. Use pathspecs. Never stage behind a revert.

## 8. No attribution

No credit trailers, no generator or tool credit, no names. Commit messages
describe what changed and why. `tools/common/provcheck.py` enforces this in the
pre-commit and commit-msg hooks.

## 9. Never full-sweep for ordinary work

One file takes about 12 s. A full sweep is about 20 minutes of bookkeeping.

## 10. Header edits are serialized. Parallel work splits by `.c` only.

## 11. Giants are last

Do not open them unless their VA is named for the work. The three historical
giants are certified T3: `0x10019A70` BrRaceStep, `0x1000EAF0`
BrSceneDlBuild, `0x100250D0` BrTex3dExpand. The largest bodies still open
when this rule was written were `0x10056260` (8,349 B, `br_uiimg.c`) and
`0x10051600` (4,109 B, C++ lane), under the same rule.

After milestone M1 nothing is closed: giants, "do not reopen" and parked rows
are all open to M2 work (see
[post-m1-nothing-closed](decomp/rules/post-m1-nothing-closed.md)).

## 12. T4 is byte-exact. T3 is certified complete, not byte-exact. Nothing between.

EXCLUDED sits beside the tiers, not between them: game code the retail game
provably never runs (`config/brally/excluded.csv`, with the proof in the function's
source header). It is outside the target and every work list, and
`tools/brally/tiers.py` labels it with the tier its transcription reached, for
example `EXCLUDED (T2)`.

`tools/brally/t3.py --qualify <VA>` decides. Gate 0: purpose comment, no unfinished
markers. Gate A: the residue is allocation or scheduling, every row
classified, no lost sync, and A5 (the live oracle, `tools/brally/t3live.py`, the
original game run headless by `tools/brally/brbox.py`) says EQUIVALENT in
`config/brally/t3_live.csv`. UNCOVERED, UNVERIFIED and DIVERGENT fail; an unreached
function is never passed. A7 (the whole-image run, `tools/brally/brbox_diff.py --all`:
every T3 body placed, every script, every frame against the original) must be
IDENTICAL in `config/brally/whole_image.csv` and no older than the function's
source. A5 alone certified about 25 real bugs that A7 found; nothing
supersedes A7. Gate B: two counted `@t4-pass` lines (at least 10 compiles
each) at the current numbers, one of them `census yes`. Colouring walls that
pass Gate A are certified and parked, not ground. T3 is never counted as
matched; `t4lane.py` / `claim_lane.py` never hand one out.

## Session start

```bash
python3 tools/brally/refcheck.py
python3 tools/common/install_hooks.py
python3 tools/brally/t4lane.py --claim          # Pool B. Never `claim_lane.py claim N`.
```

Counts come from `tools/brally/tiers.py` and `tools/brally/total.py`, not the README.

## Scope

| in | `.text` | |
|---|---:|---|
| `BRGlide.dll` | 480,853 | the game (primary) |
| `BRally.exe` / `BossRally.exe` / `SetVideo.exe` | ~64 KB | game code done; the rest is static CRT, linked, not decompiled |

Out of scope: `BRD3D.dll` (static CRT), `Boot.exe` (static MFC 4.2),
`REMOVE.EXE`, the 16-bit InstallShield.

The Boss Rally decomp is the master. Ports are not byte-matched. The Top Gear
Rally N64 decompilation is a second, separate target under IDO 5.3.

## Where the knowledge is

- [decomp/](decomp/): what was learned matching the DLL, by kind:
  [rules](decomp/rules/), [levers](decomp/levers/), [oracle](decomp/oracle/),
  [traps](decomp/traps/), [triage](decomp/triage/), [cpp-lane](decomp/cpp-lane/),
  [corpus](decomp/corpus/), [toolchain](decomp/toolchain/),
  [functions](decomp/functions/), and the dated [log](decomp/log/).
- [notes/](notes/): the earlier long-form notes (conventions, decomp notes,
  idiom batches, per-family C++ notes, per-executable notes).
- [port/](port/) and [remaster/](remaster/): the platform ports and the
  Remastered presentation.
