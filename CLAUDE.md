# Boss Rally — matching decomp

Read this before touching anything. These are hard rules, not preferences.

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
file, `README.md`, and the memory index. A session spent on a wrong assumption
that one file read would have caught is the most expensive failure mode here.

## 4. Every number carries its denominator.

"290 matched" and "2% of the code section" are the same fact. Quoting the first
alone overstates progress by more than an order of magnitude. State what a
count is *of*: tagged functions, all known functions, or bytes of `.text`.
Never mix strictness levels — encoding-match, address-verified, and
independently-verified are three different numbers.

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

---

## Before you start any session

1. Run `python3 tools/refcheck.py` and believe the result.
2. Read the memory index; it carries current state and open leads.
3. Do not trust a coverage number in prose — including in `README.md`.
   Query the tree.

## Layout

- `src/core/`, `src/backends/`, `include/`, `tests/`
- `config/` — function maps, globals, the `shared.csv` d3d↔glide twin table
- `build/match/` — extracted reference bytes, per-function report, objs
- `tools/` — matching pipeline, auditors, the staged MSVC toolchain
