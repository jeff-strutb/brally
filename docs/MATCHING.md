# How to match this week

Law is `CLAUDE.md`. This file is the procedure. Idioms: `tools/corpus.py find`,
not a full read of `docs/VC5-IDIOMS.md`.

## Session start

```bash
.venv/bin/python tools/refcheck.py              # must say Glide-keyed
.venv/bin/python tools/install_hooks.py
.venv/bin/python tools/t4lane.py --claim        # locks Pool B; prints Pool A as T3 candidates
git status --short src/                         # do not touch another session's files
```

Never `claim_lane.py claim N`. That picker is a 2026-08-28 snapshot and hands
out the giants. `t4lane.py --claim` locks via `claim_lane.py claim --va ...`.

Release at the end: `python3 tools/claim_lane.py release <TOKEN> [wallVA ...]`

## What to work

| pool | what | action |
|---|---|---|
| **B** | smallest screened T1 drafts, ≤400 B then 800 B | transcribe, sweep, commit T4 or park T2 after 6 probes |
| structural T2 | real missing/wrong code, start ≤400 B | first divergence, then next; class → generator |
| **A** (`reggap 0`) | same instructions, registers differ | `t3.py --qualify`; do not permute |
| giants | `0x10056260`, `0x10051600`, … (the old giants are T3) | closed unless the user names the VA |

Screen before accepting any T1/T2 row:

- `src/core/cpp/<VA>.cpp` or `report_cpp.csv` → C++ lane, skip
- odd address → split map row, skip
- orig prologue `6a ff` … `fs:[0]` → C++ EH, skip
- `fxch` chains or >2 16-bit register ops → colouring/byte-lane wall, skip
- report `opt` column is Od/O2y/O2p → `fn.py` is `/O2` only; trust the one-file sweep

Keep: plain control flow, calls to matched neighbours, integer/struct traffic,
one or two float compares.

## T1 → T4 (Pool B)

Draft: `build/ghidra_decomp/<va>.c` or `build/ghidra_work/<va>.refined.c`.
Original: `build/match/orig/<VA>.bin`. Neighbours: `config/filing.csv`.

1. Transcribe into the module that owns the neighbours (`src/core/<area>/…`).
   Matching arm only (`#ifdef BR_MATCHING_BUILD`). New `br_*.c` is fine; a new
   slice file or a new VA in an old one is refused. `WHAT IT DOES:` and
   `@implements <VA> glide <Name>` in the same edit.
2. `.venv/bin/python tools/match_sweep.py <file.c>` (~12 s). Read **every**
   row of that file in `report.csv`.
3. On diff: `fn.py <VA> --detail regnorm 30`, then always
   `sbs.py build/match/obj_<opt>/<file>.obj <Name> <VA>`.
4. Probe: `fn.py <VA> --make <tag>`, edit the variant, `fn.py <VA> --var <tag>`.
   **Six probes.** Past that: residue note, `@t4-pass` line, commit as T2, next.
5. Byte-exact: one-file sweep, then
   `git commit -m "<VA> <Name> byte-exact: <the source fact>" -- <file>`.

## Structural T2

Ground section by section against the **first** divergence. Win the
prologue/frame first (`sub esp,N`). Timebox ~40 min per region.

```bash
.venv/bin/python tools/fnmatch/fn.py <VA> --detail regnorm 30
# risky edits: --make w1 / --var w1  (do not probe the tree file)
# giants (if named): tools/probe.py /tmp/v1.c v1 --va <VA> --sym <Name> [--key 10]
```

`reggap` is the structural gap; `rawgap` is inflated by register names.
EXTRA with no MISSING counterpart = extra code; MISSING with no EXTRA = missing
code; same op, different operand source = allocation → stop, `--qualify`.

A whole-function register rotation is a symptom of one earlier source-shape
fork. Fix the earliest divergence.

N64 twin when blocked on what the source says: `build/n64/report.csv`. Useful
for commutative float operand order. Useless for colouring.

Saw the same defect twice → stop hand-solving, mint a generator.

## T3 (colouring walls)

If Gate A passes, certify. Do not spend a session permuting spellings
(permuter 0/95, refine 0/258, crank 4/536). Append `@t4-pass` when a pass ran
≥10 compiles. `--qualify` emits the tag; paste it above `@implements`.

```c
 * @t4-pass 0x........ N YYYY-MM-DD probes K bytes B insns I regions R rows G census yes
```

## The live oracle (A5)

The original DLL runs headless (`tools/brbox.py`), driven by scripts in
`tools/brbox_scripts/`. At real calls to each T3 function, `tools/t3live.py`
runs the original body and the placed T3 body from the same state and
compares: memory written, import calls, esp, callee-saved registers, x87
state, and eax/edx wherever the caller reads them. Build the image first:
`env -u BR_TRACE .venv/bin/python tools/image_build_t3.py --out-dir build/brbox/image`.

```bash
.venv/bin/python tools/t3live.py run tools/brbox_scripts/*.txt      # ledger -> config/t3_live.csv
.venv/bin/python tools/t3live.py run S.txt --only 0xVA --per-fn 60 --no-ledger
T3LIVE_SKIP=n .venv/bin/python tools/t3live.py explain S.txt 0xVA --frame F  # (n+1)th call at F
```

The `explain` knobs are environment variables:

- `T3LIVE_CALLS=raw`: arguments of every call the body makes.
- `T3LIVE_ARGDATA`: argument buffers that differ between the two sides.
- `T3LIVE_WATCH=addr,..`: who wrote those addresses, and inside which call.
- `T3LIVE_PROBE_O` / `T3LIVE_PROBE_T=eip,..` (optionally `T3LIVE_HEX`, `T3LIVE_PROBE_MEM=reg,off,n`): registers and x87 state at an address.
- `T3LIVE_MDIFF`: the differing ranges.

Script directives:

- `files NAME` starts from saved files (`tools/brbox_saves/`).
- `peer SCRIPT` runs a second game on a virtual network.
- `joystick ffb` attaches a force-feedback wheel.

Bytes that come only from the original reading uninitialised stack are
detected by re-running with the callee-saved registers perturbed at each
call, and masked.

Defect classes the oracle found, all fixed in source:
- **Swapped arguments or operands**, e.g. `pow` operands, and vectors passed in place.
- **Float rounding points.** VC5 forwards float scalars in registers: model x87 registers with `double` and force a store through the value's `int` image.
- **Locals assumed adjacent.** `&local` used as an array: write a real array.
- **int→float conversions** where the original copies dwords.
- **Wrong compiler variant.** The stack frame depth is behaviour: rank frame shape first.
- **Relocation rows** from the wrong object, a non-address field, or a D3D-era name.

## The whole-image run

The live oracle certifies one body at a time from a few captures. It cannot
see a path those captures never took, or a fault that exists only in the
placed image (a constant, a relocation row). The whole-image run can:

```bash
.venv/bin/python tools/brbox_diff.py S.txt                 # every frame: Glide calls + whole data area
.venv/bin/python tools/brbox_diff.py S.txt --t3calls F     # first T3 call in frame F whose writes differ
.venv/bin/python tools/brbox_diff.py S.txt --localize --frame F   # differing data + its writers
BRDIFF_CALLS=F .venv/bin/python tools/brbox_diff.py S.txt  # first differing Glide call in frame F
```

The loop for one divergence:

1. Find the first frame F whose state differs.
2. Run `--t3calls F-1`.
3. Run `t3live.py explain` on the named function at that frame.
4. Read the original's x87 code against the placed body. An annexed body sits at `0x1190xxxx` behind a `jmp` thunk.

Vertex fields the original never writes (stale stack) are reported
separately and are not failures.

Defect classes the whole-image run found, all certified by t3live before:
- **Rounding points.** The original keeps a value in a register where the transcription stores it to a float slot, or the reverse.
- **Reassociated sums**, spelled that way to chase bytes.
- **An inverted comparison.**
- **A stale cached value** in a loop.
- **`$T` constants content-located inside `.text`.**
- **Relocation rows:**
  - operand-width mismatch;
  - stale offsets, which fail the gate;
  - a D3D-era name inside the "near" window;
  - crossed commutative operands where a hand row fixed only one side.

After editing a T3 function that has lockstep rows, regenerate them: remove
its `lockstep` rows, then `tools/lockstep_rows.py <VA> --write`.

## End of session

```bash
.venv/bin/python tools/image_build.py      # IMAGE GATE PASSED, 0 differing bytes
.venv/bin/python tools/fileaudit.py        # ratchets unchanged
.venv/bin/python tools/claim_lane.py release <TOKEN>
```

After a refile: `portcheck.py --baseline main`. Report T4 bytes and T1→T2
promotions, each with its denominator. Matched and parked separately.

## Bookkeeping that bites

- `filing.py` with no args rewrites `config/filing.csv` from `report.csv` and
  **drops** in-flight VAs. Diff before committing; re-add LOST rows (CRLF).
- Header prototype vs byte-exact signature: `#define Name Name_port` before
  the include, `#undef` before the definition.
- MSVC5 `<stdio.h>` defines `getc` as a macro; `#undef getc` after the include.
- `/Od` stretch `0x1002A840`–`0x1002BF50` needs its own TU or an `/Od` neighbour.
