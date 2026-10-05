# Fnmatch harness

*Recorded 2026-08-29.*

> tools/fnmatch/ = per-function matching sandbox: ~1.5s edit->compile->score cycle, per-tag isolation so N workers probe one function concurrently, and the register-blind multiset metric. Use it instead of the 20-min sweep.

**Built 2026-08-28 during the 0x100250D0 run; promoted out of gitignored
`build/` into `tools/fnmatch/`.** This is what made that function tractable.

    sh tools/fnmatch/vmake.sh <TAG> <src.c>           # private copy
    sh tools/fnmatch/vdiff.sh <TAG> [orig.bin]         # compile + scorecard
    sh tools/fnmatch/vdiff.sh <TAG> "" regnorm 30      # + multiset detail

- **~1.5 seconds per cycle** (cl.exe under Wine on one file). Probe
  aggressively; do not deliberate about a spelling you can just measure.
- **Per-tag isolation**: each tag owns its own `.c` and `.obj` under
  `build/match/t3d/`, so many workers can probe the SAME function
  concurrently without contending. This is the escape from project rule 10
  (parallel work splits by `.c` file only) for single-function grinding  - 
  fan out by REGION with one variant file each, then merge the disjoint
  hunks with `diff -u` + `patch`.
- Never full-sweep for this (project rule 9).

## The metric: register-blind, not raw

`mdiff2.py` has three normalisation levels: `raw` (registers kept),
`regnorm` (all GP regs -> R - THE HONEST NUMBER), `widthnorm` (widths too).

Read `regnorm`. See [register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md) for why: on
0x100250D0 raw read 1097/863 while the real structural gap was 432/198.

## Scorecard fields

`BYTES` / `INSNS` vs orig; `FIRSTDIV` (a collapse toward +0x2 means the frame
changed - `sub esp,N` - hard reject); `RAW`/`REGNORM` extra (shapes the recomp
emits that orig does not) and miss (shapes orig emits that it does not).

Gotcha: trailing `nop`s after `ret` are .obj alignment padding, not code, and
score.py counts them - read BYTES as +/-15 across variants.
