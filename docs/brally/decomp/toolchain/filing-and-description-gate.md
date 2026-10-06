# Filing and description gate

*Recorded 2026-09-03.*

> tools/brally/fileaudit.py gates BOTH halves of rule 6 (module filing + WHAT IT DOES comment) with ratcheting baselines. config/brally/filing.csv is the durable per-function module record. autofile.py used to slice-dump every match by address, which caused the 570-function backlog.

## Two long-standing rules had no gate, and both drifted badly (found 2026-09-03)

Audit of 845 matched C functions found:

* **570 (67%) stranded in `sliceN_MM.c` address batches**, only 275 filed into
  responsibility folders. All 172 C++ matches sit in `src/brally/core/cpp/0xVA.cpp`,
  address-named by construction.
* **196 (23%) with no `WHAT IT DOES:` comment** - the convention
  `src/brally/core/README.md` mandates, written directly above the `@implements` tag.

**Documentation of the ADDRESS is fine** - 845/845 carry an `@implements` tag
(496 at the glide VA, 346 at a d3d VA via shared.csv, 3 via documented
COMDAT-fold notes). It is the PURPOSE that was missing.

**Cause of the filing half was mechanical, not neglect:** `tools/brally/autofile.py`
step 2 picked "the slice file whose tagged VA range brackets the address". The
automated path filed by ADDRESS, permanently, so every unattended batch grew the
backlog. Fixed: it now calls `module_destination(va)` first and only falls back
to a slice.

## The gate - `python3 tools/brally/fileaudit.py`

Fails (exit 1) on stranded / unrecorded / misplaced functions, on a NEW
`sliceN_MM.c`, and on any INCREASE in undescribed functions. Two ratcheting
baselines that may only be lowered: `BASELINE = 62` (address batches),
`DESC_BASELINE` (undescribed; 196 -> 132 on 2026-09-03).

`describes` walks up from the tag through the comment block and **steps over
preprocessor lines** - a guarded matching body puts the description above the
`#ifdef`. Without that it reported 22 false gaps.

## `config/brally/filing.csv` - decide once, never re-derive

`glide_va,name,module,basis,file`. `basis` is `filed` / `neighbour` (both
nearest already-filed functions BY ADDRESS agree - MSVC 5 emits a TU
contiguously) / `prefix` (Br<Word> maps to one module across ground truth) /
`manual` / empty = UNASSIGNED. A recorded `manual` decision is never
overwritten. `tools/brally/filing.py --todo` prints the queue; `--set 0xVA module`
records a decision.

**Automated inference cannot fill this in, and measuring that was worth doing:**
neighbour agreement covers 148/570, pure name prefix 62/570 (474 prefixes are
unseen in the filed ground truth), string references only 21/570 (this engine
barely uses strings). Combined ~a third. **The other 455 are deliberately left
empty** - a guessed module in a config file reads as authoritative, is checked
by nothing, and costs more to discover wrong later than never writing it.

## Two tool defects found and fixed

* **`tools/brally/modules.py` hardcoded `DLL = 'reference/brally/orig/BRD3D.dll'`** - a rule 0
  violation in the one tool that recovers module boundaries - **and crashed**
  (`KeyError: 'va'`; shared.csv has `d3d_va`/`glide_va`, no plain `va`). Now
  Glide-keyed off `functions_glide.csv`. Its clustering is WEAK though: 1,034
  clusters over 2,141 functions, average 2 - too fine for TU boundaries, so do
  not build filing decisions on it.
* `tools/brally/refile.py` (new) moves one function per commit, sweep-verifying both
  files. **NOT YET PROVEN END TO END** - the first real attempt (0x1006E020
  BrFontSetRenderDst -> br_font.c) was refused and correctly reverted;
  extraction works and the destination file swept clean, so the refusal came
  from the source-side sibling check. Debug that before the 183 assigned
  functions can move.

It deliberately **refuses to mint technique-named files** (`br_nop.c`,
`br_thunk.c`, `br_ret0.c`, `br_stub.c`) - `src/brally/core/README.md` names files for
a responsibility, and a 5-byte `BrNop_1002AB8F` has none. 30 such cases report
as needing a human decision.

See [image-build-gate](../oracle/image-build-gate.md) for the other deliverable gate, and
no-token-thrashing.
