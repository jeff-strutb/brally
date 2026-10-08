# n64/: the Top Gear Rally (N64, 1997) decomp

A second byte-matched target, separate from the PC decomp. The repo root
serves `BRGlide.dll` under MSVC 5.0; this tree serves `Top Gear Rally (USA).z64`
under SGI IDO 5.3 and keeps its own sources, headers, symbols, tools and
records, so nothing here touches the PC work.

The two games share an engine but not a source: only about a fifth of the ROM
has a clear PC twin, and twins still differ by a few percent of their
instructions. PC source is a starting point for a twin, never a substitute.
See the top-level README for the measurement.

How to match IDO 5.3 output (method, compiler stages, allocator rules, the
levers that closed every function): [decomp/IDO-HANDBOOK.md](decomp/IDO-HANDBOOK.md).

## Standard

The same as the PC lane:

| | |
|---|---|
| T1 | not started: a Ghidra draft exists in `build/tgrally/n64/ghidra/` |
| T2 | in `src/tgrally`, bytes still differ |
| T3 | certified complete, not byte-exact: `n64t3.py --qualify` passes |
| T4 | byte-exact against the ROM, every relocation resolved |
| M1 | T3 + T4 bytes over the game code |
| M2 | T4 bytes over the game code |

The target is the game code. Nintendo's libultra and the zlib inflater are
fenced out (`config/tgrally/fenced_tgr.csv`, with the evidence in `config/tgrally/README.md`).

## Layout

| | |
|---|---|
| `src/tgrally/<area>/` | the decomp's C, one file per responsibility (see `src/tgrally/README.md`) |
| `src/tgrally/include/tgr/` | the decomp's headers |
| `src/tgrally/include/*.h` | libc/Win32 type shims for the old PC-source cross-compile only |
| `config/tgrally/symbols_tgr.csv` | name -> address |
| `config/tgrally/fenced_tgr.csv` | library code outside the target |
| `config/tgrally/t3_live.csv` | per-function live-oracle verdicts (A5) |
| `config/tgrally/whole_image.csv` | whole-image runs (A7) |
| `tools/tgrally/` | see below |

## Tools

| | |
|---|---|
| `n64build.py` | compile `src/tgrally` with IDO 5.3 (`-O2 -mips2 -G 0 -Wab,-r4300_mul`) and grade every function: the T4 gate |
| `n64tiers.py` | T1-T4 and M1/M2, rebuilt fresh |
| `n64image.py` | M2 gate: every T4 body placed in the ROM image, must differ by 0 bytes; `--t3` builds the M1 image |
| `n64box.py` | the original ROM run headless (Unicorn, R4000 model) with the OS and hardware modelled; scripts in `tools/tgrally/n64box_scripts/` |
| `n64t3.py` | `--live` A5 oracle, `--image` A7 whole-image run, `--qualify` the T3 gate |
| `n64permute.py` | random meaning-preserving respellings against the ROM; `--ledger` writes Gate B's `@t4-pass` lines |
| `n64ghidra.sh` | Ghidra drafts (T1) for every function |
| `n64gen.py` | turn drafts into graded candidates in `build/tgrally/n64/cand/` |
| `n64file.py` | file a candidate into its module with its name and `WHAT IT DOES:`; refuses a move that costs a neighbour |
| `n64names.py` | replace address names with real names once they are known |
| `n64index.py` | regenerate the `src/tgrally` READMEs |
| `n64rom.py`, `n64strs.py` | ROM map, disassembly, cross-references, strings per function |
| `n64match.py`, `permute.py`, `cover.py`, `pair.py`, `manifest.py` | the earlier PC-source cross-compile survey |

```bash
.venv/bin/python tools/tgrally/n64tiers.py
.venv/bin/python tools/tgrally/n64image.py
.venv/bin/python tools/tgrally/n64t3.py --qualify 0x8022439C
```

Query the tree for counts; do not trust a number written in a file.
