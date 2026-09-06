# T4 lane: the next twenty byte-exact functions (paste-ready, generic)

Feed this file to a fresh session. It never names a function: **the targets
come from `tools/t4lane.py`, run at session start**, so this spec is not
edited as the tree moves. It complements `docs/STRUCTURAL-PLAYBOOK.md` (the
rule book) and `docs/T1-INTAKE-LANE.md` (the intake procedure) and does not
replace `CLAUDE.md`. Every tool through `.venv/bin/python`. Work IN SESSION,
one function at a time, no agents unless the user asks in that message.

## The goal, with its denominator

Twenty functions from `diff` (or no tag) to `match` in `build/match/report.csv`.
Take the starting counts from `tools/tiers.py` at session start and quote every
count as "of the 1,501-function hand-C target" and every byte figure as "of
452,733 B of `.text`" (rule 4). List matched, parked-T2 and still-open
separately at the end. Never carry a number over from a previous session.

## Session start (in order)

    tools/refcheck.py                 # must say Glide-keyed
    tools/install_hooks.py            # idempotent; wires rule 6's hook
    tools/fileaudit.py                # note the baselines; must be green
    tools/claimcheck.py               # 0 duplicates is the bar
    tools/t3.py                       # certified rows are never targets; must be green
    git status --short src/           # another session's uncommitted files: do not touch
    tools/tiers.py                    # the denominator, before and after
    tools/t4lane.py                   # THE TARGET LIST for this session

`t4lane.py` prints two pools with the first N of each marked `<-- primary`
(`--n 20` default; `--max-bytes 400` default). Work the pools alternately:
one Pool A row, then two Pool B rows, so a stall in one never idles the lane.
Re-run it after every commit: matches drop out, the next row moves up.

## Pool A -- identical instruction multiset, registers only

Rows whose register-blind instruction multiset equals the original's: the
same instructions, only allocation, order or ONE source fact differs. The
tool groups them **by translation unit** (the cluster rule: a lever found on
one sibling closes the others, and a committed match in the same TU can
regress when a neighbour is re-spelled -- re-sweep the file and read EVERY
row before committing). A row flagged `[residue note: read it first]` is a
target, but its note is the first thing you read.

Rows the tool lists as **HELD** are Pool A's second tier, and they are most
of it: a lane parked them, or their file carries a dead list. The tool prints
when and why. They are worked ONLY with a lever the park predates -- the
declaration-order, guard-shape and sibling screens (levers 4, 5, 7 below)
date from 2026-09-03 to 05 and closed eleven parked rows in three passes,
so a park older than that has never met them. A probe that is in the row's
dead list is a violation. Same cap as the live rows; the ledger line at the
end either way.

The levers, in the order to try them (each is proven in `docs/VC5-IDIOMS.md`;
grep the entry before spending a probe):

1. The file header's note for the row. Anything it lists as dead is dead; a
   probe already in a dead list is a violation, not a retry.
2. `tools/fnmatch/fn.py <VA> --detail regnorm 30`, then ALWAYS the side by
   side: `build/match/sbs.py <obj> <sym> <VA>`. Read the first divergence.
3. Single-fact residue (2-8 B, regnorm 0+0): statement ORDER (fill records in
   address order), RETURN TYPE (64-bit pairs), a constant's width or sign, the
   `>= N` vs `> N-1` class. `tools/msetdiff.py` keeps small immediates and
   shows these where `divergence.py` is blind.
4. Declaration order of the int locals: `tools/declsweep.py` sweeps one
   declaration through every position (x87 completion order and the imul
   destination both key on symbol index).
5. Guard shape: `&&` chain vs sequential early returns is decided by the
   RETURN VALUES; assign-then-override vs if/else only where the original
   HOMES the value on both edges.
6. Commutative float operand order: the N64 twin is the oracle
   (`n64/tools/n64match.py`, `build/n64/report.csv`); IDO does not
   canonicalise, VC5 does.
7. Families (several siblings in one TU with the same residue): the
   sibling-asymmetry screen -- the ONE sibling spelled or placed differently
   is the lever for all of them. `tools/screen_slotpairs.py` for slot pairs.
8. Corpus: `tools/corpus.py find --from <VA> --at <off> --len 12 --source`.
   A miss is a result: stop permuting.

## Pool B -- smallest untouched drafts (T1 intake)

The lane that produced nine matches in one session. The tool screens every
untouched function mechanically (exception-frame prologue, odd address, C++
lane, x87 juggling, 16-bit lanes) and prints survivors smallest first with
the draft to start from, and the rejects with the reason. Rejects are not
this lane's; do not start them here.

The intake loop for one function (the six-probe budget; detail in
`docs/T1-INTAKE-LANE.md`):

1. `tools/dumpasm.py <VA>` beside the draft. Reject on sight if the screen
   missed something (byte-lane packing, a big-endian Horner chain).
2. Find the module (`grep -i "^0x<prefix>" config/filing.csv`; the port twin
   in `src/` and `include/` names the fields). A new `br_*.c` module file is
   fine; a new `sliceN_MM.c`, or a new tag in an old one, is refused by the
   hook.
3. Transcribe into the matching arm (`#ifdef BR_MATCHING_BUILD`), callees and
   globals declared locally with the names in `config/globals_learned.csv`.
   Write `WHAT IT DOES:` and `@implements <VA> glide <Name>` in the same edit.
4. `tools/match_sweep.py <file.c>` (~12 s). Read every row of that file.
5. If diff: `fn.py --detail regnorm 30`, then `sbs.py`. Four to six probes,
   each from a byte-level reading, never a permutation. Idioms first.
6. Exact: commit on the spot. Not exact after six probes: it is a T2 row
   now -- write the residue note in the file header with the numbers and the
   dead probes, append its `@t4-pass` line, commit, move on.

## Budgets and stop conditions (hard)

- Pool A: **20 minutes or 12 fresh compiles per row**, whichever first.
  Every probe grepped against the file's dead list and the idiom file before
  it is compiled. At the cap: append
  `@t4-pass <VA> <n> <date> probes <k> bytes <b> insns <i> regions <r> rows <m> census <yes|no>`
  to the file header (end-of-pass numbers from `tools/probe.py` or `fn.py`),
  commit the note, next row.
- Pool B: **six probes per row.** Same ending.
- A row that passes `tools/t3.py --qualify <VA>` Gate A is NOT finished: it
  owes two more zero-movement passes before T3, and those are later passes.
  Log the line and move on.
- A whole-TU sweep after every change to a file with more than one tagged
  row; read every row.
- Forty minutes with no match landed anywhere: switch pools.
- Twenty matched, or both pools' primaries exhausted: stop and report.

## Commit discipline

- `git commit -m "<VA> <Name>: byte-exact (<the last fact>)" -- <file.c>
  [config/filing.csv]` -- always with `-- <paths>`; never a bare commit,
  never staged behind a revert (rules 7, 8).
- `tools/filing.py` DROPS rows whose VA is absent from report.csv: check for
  LOST rows before committing `config/filing.csv`.
- Every proven fact goes into `docs/VC5-IDIOMS.md` in the same commit as the
  match. A lever that closed one row is what closes the next five.
- After every five matches: `tools/corpus.py build`, and
  `tools/cpp_twin_retire.py` if any row was a d3d twin.
- Never `--no-verify`. Never a full sweep for ordinary work.

## Report format (end of session)

    matched:   N of 20   (T4 now X of 1,501; Y B of 452,733 -- from tiers.py)
      <VA> <Name> <B> -- <the fact that closed it> (idiom entry: <title>)
    parked T2: M   (each with its @t4-pass line and residue note)
      <VA> <Name> <B> -- <numbers>, dead: <count>
    next:      the first rows tools/t4lane.py prints now
