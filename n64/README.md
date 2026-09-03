# n64/ — the Top Gear Rally (N64, 1997) decomp

A **second byte-matched target**, not a port and not part of the PC lanes.

The repo root is the PC decomp: `src/`, `include/`, `config/` and `tools/` all
serve one image, `BRGlide.dll`, built by MSVC 5.0. This tree serves a different
image — `Top Gear Rally (USA).z64`, built by SGI IDO for MIPS — and keeps its
own tools, headers and docs so that nothing here can collide with that work.
Contrast `ports/`, which is downstream of the decomp and byte-matched against
nothing at all.

## Why the two targets share a root

Boss Game Studios wrote both games from one source lineage: Top Gear Rally
shipped in 1997, Boss Rally in 1999. The engine bodies are the same code
compiled twice for different machines. That is the whole point of this tree —
**the PC grind's deliverable is SOURCE, and source is what transfers.** A
function already recovered on the PC side can be compiled a second time with
IDO and looked for in the ROM directly, which pairs it and matches it in one
step, with no anchor and no hand work.

## Layout

| | |
|---|---|
| `tools/n64rom.py` | ROM analyser — segments, function map, data xrefs, disassembly |
| `tools/n64match.py` | compile PC source with IDO, search `.text`, score `EXACT`/`SHAPE` |
| `tools/cover.py` | coverage against the real denominator |
| `tools/pair.py` | string-anchored pairing — a cross-check, not the bridge |
| `tools/manifest.py` | file every located function into its architectural module |
| `include/` | libc + Win32 **type shims** for the cross-compile |
| `config/functions_tgr.csv` | every function in `.text`: what it is, its module, its status |
| `src/<area>/` | architectural areas, mirroring `src/core/` |
| `docs/modules.md` | the architectural map, generated, with denominators |
| `docs/menu-analysis.md` | ROM layout, compiler identification, menu system |

`config/functions_tgr.csv` and everything under `docs/modules.md` and `src/`
is **generated** by `tools/manifest.py` — do not hand-edit it. A function's
module is inherited from the PC source file it matched, so it is evidence
rather than guesswork; unlocated functions bracketed on both sides by one
module are marked `inferred` and never counted as confirmed.

`src/` is mostly empty on purpose. The shared engine is one source and it
lives in `src/core/`; copying it here would create a second thing to keep in
sync. What belongs in `src/` is **N64-only platform code** — RSP display-list
building, the audio driver, OS glue — which has no PC twin. That is most of
the 96.31% of `.text` still unidentified.

The ROMs live in `reference/tgrally/` with the other reference binaries.
Build output goes to `build/n64/` (gitignored, like `build/match/`).
The IDO toolchain is staged at `tools/ido` and `tools/ido53` — shared with the
root, since nothing else uses it.

## The shims are declarations only

`include/` is not a libc and is never linked. The portable engine bodies
include `<windows.h>` for its typedefs even where they touch no Win32 API, and
that one header blocked 182 files until it was shimmed. Widths match Win32 on
x86 (all 32-bit), which is also what O32 MIPS gives, so struct layouts carry
over unchanged. If a file needs a real implementation of something here, that
is a signal it is platform code with no N64 twin, not a signal to grow the shim.

## Running it

```
.venv/bin/python n64/tools/n64match.py --all --csv build/n64/report.csv
.venv/bin/python n64/tools/cover.py
```

The sweep compiles every `.c` under `src/` and takes a few minutes. It reads
the PC tree and writes only to `build/n64/` — it never modifies PC sources,
maps or reports.

## State (2026-09-03, first bulk run, zero hand-matching)

Denominator: `.text` is **457,392 bytes across 883 functions**.

| | fns | bytes | of `.text` |
|---|---|---|---|
| `EXACT` (byte-exact) | 22 | 964 | 0.21% |
| `SHAPE` (located, not exact) | 175 | 12,476 | 2.73% |
| **located** | **197** | **13,440** | **2.94%** |

252 of 255 PC source files cross-compile under IDO.

Two settled questions, against earlier claims in the memory note:

- **String anchoring is a seed, not the bridge.** 101 shared literals of 6+
  characters, and only **7** are referenced from code on the N64 side — the
  distinctive ones (credits text) live in pointer tables. Its real value is
  independent validation: the anchors and the opcode-multiset search agree on
  every address where both fire, and disagree nowhere.
- **IDO 5.3 vs 7.1 is not a lever.** 23 `EXACT` vs 22 over the same 1,753
  functions. Don't spend a trial on it.

## What this tree gives back to the PC decomp

43 of the located functions sit at opcode-multiset distance 0 — structurally
identical, differing only in commutative operand order and register allocation.
**IDO does not canonicalise commutative operands; VC5 does.** So where a float
sum's operand order is the last divergence on the PC side, the MIPS states the
original spelling outright: a lookup instead of a probe. This is recorded in
`docs/VC5-IDIOMS.md` under the commutative-float-addition entry, which is where
a PC session hits the wall.

The boundary matters: this buys **source truth, never codegen truth.** It
resolves what was written. It does nothing for a VC5 register-allocation wall,
and a function absent from the N64 build — PC-only code, or the Glide/D3D
submission layer — has no twin to consult.
