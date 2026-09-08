# Boss Rally — matching decomp

Hard rules. Procedure is `docs/MATCHING.md`. Idioms are queried, not reread:

```bash
.venv/bin/python tools/corpus.py find --from <VA> --at <off> --len 12 --source
```

A corpus miss means the construct is not proven anywhere — source truth, not
another permutation. Newly proven mappings go on the tail of `docs/VC5-IDIOMS.md`.

**Cadence:** hand-solve one of a class, mint a generator, re-batch. Never
hand-match what a generator could sweep. **Lanes that still pay:** T1 intake
(Pool B) and structural T2. Colouring walls (`reggap 0`) are T3, not a grind.
`tools/crank.py` is an overnight lottery on Pool A only — never `--all --loop`,
never `--max-bytes` above 400. Giants are not a lottery.

The N64 tree (`n64/`) is the oracle for commutative operand order (IDO does
not canonicalise; VC5 does). It does not move register-allocation walls.
Pairing is not by shared strings (7 usable, not 195). Lookup: `build/n64/report.csv`.

## 0. Reference is BRGlide.dll. Not BRD3D.dll.

`python3 tools/refcheck.py` must say Glide-keyed. Tools honour `BR_REF` / `BR_MAP`.

## 1. Bit-exact under MSVC 5.0, same source cross-compiles as the port.

Do not reorder matching to make something run. Do keep the port buildable.

## 2. `@implements` means the bytes diff clean. Nothing else.

## 3. Read once, decide once. Query the tree for counts — never a number in prose.

Every count carries its denominator (functions vs bytes of `.text`). Never mix
strictness (byte-exact, address-verified, independently-verified).

## 4. Be concise. Yes/no first. Numbers with denominators.

## 5. Toolchain lives in the repo (`setup.sh`). Never install to the host.

## 6. A match says what it does and lives in its module.

`WHAT IT DOES:` comment directly above `@implements`, written when matched.
`sliceN_MM.c` files are address batches: never create one, never add a new VA
to an existing one. File by hand, one function or connected group; sweep both
files; keep the move only if nothing regressed. Surroundings decide codegen —
carry the whole preamble.

```bash
python3 tools/install_hooks.py    # once per clone
python3 tools/fileaudit.py        # ratchets: undescribed 0, batches 58, stranded 11
```

The pre-commit hook refuses a new `@implements` without `WHAT IT DOES:`, a new
`sliceN_MM.c`, or a new VA in an existing batch. After a refile:
`python3 tools/portcheck.py --baseline main`. The sweep compiles nothing for a
file with no `@implements`.

## 7. Commit every verified match immediately. Pathspecs. Never stage behind a revert.

## 8. No AI attribution. No `Co-Authored-By` trailers, no generator credit.

## 9. Never full-sweep for ordinary work. One file, ~12s. Full sweep is ~20 min bookkeeping.

## 10. Header edits under `include/` are serialized. Parallel work splits by `.c` only.

## 11. Giants are last. Do not open them unless the user names the VA.

- `0x10019A70` (11,223 B) — last, gated on 131 callee signatures. One C function.
- `0x1000EAF0` BrSceneDlBuild — do not reopen before the end-grind.
- `0x100250D0` BrTex3dExpand — `--key 10`, never `--key 6`.

## 12. T4 is byte-exact. T3 is certified complete, not byte-exact. Nothing between.

`tools/t3.py --qualify <VA>` decides. Gate 0: purpose comment, no unfinished
markers. Gate A: residue is allocation/scheduling, every row classified, no
lost-sync, oracle not DIFF. Gate B: two counted `@t4-pass` lines (≥10 compiles
each) at the current numbers, one of them `census yes`. Colouring walls that
pass Gate A get certified and parked — do not grind them. T3 is never counted
as matched; `t4lane.py` / `claim_lane.py` never hand one out. No session opens
a T3 function unless the user names it.

## Session start

```bash
python3 tools/refcheck.py
python3 tools/install_hooks.py
python3 tools/t4lane.py --claim          # Pool B. Never `claim_lane.py claim N`.
```

Read `docs/MEMORY.md`. Procedure: `docs/MATCHING.md`. Counts: `tools/tiers.py`,
`tools/total.py` — not README.

## Scope

| in | `.text` | |
|---|---:|---|
| `BRGlide.dll` | 480,853 | the game (primary) |
| `BRally.exe` / `BossRally.exe` / `SetVideo.exe` | ~64 KB | game code **done**; leftover is static CRT, linked not decompiled |

Out: `BRD3D.dll` (static CRT), `Boot.exe` (static MFC 4.2), `REMOVE.EXE`, 16-bit InstallShield.

Root is the decomp. `ports/` is not byte-matched. `n64/` is a second target
(IDO/MIPS); it reads `src/` and writes only `build/n64/`.
