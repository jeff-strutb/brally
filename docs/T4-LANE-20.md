# T4 lane: twenty more byte-exact functions (paste-ready spec)

Feed this file to a fresh session. It names the targets, the order, the tool
loop, the per-function budget and the stop conditions. It complements
`docs/STRUCTURAL-PLAYBOOK.md` (the rule book) and `docs/T1-INTAKE-LANE.md`
(the intake procedure) and does not replace `CLAUDE.md`. Every tool through
`.venv/bin/python`. Work IN SESSION, one function at a time, no agents unless
the user asks in that message.

## The goal, with its denominator

Twenty functions from `diff` (or no tag) to `match` in `build/match/report.csv`.
Starting point 2026-09-06: **T4 1,121 of the 1,501-function hand-C target**
(946 C + 175 C++ EH), 183,559 B of 452,733 B of `.text`. Target: 1,141 of
1,501. Report every count as "of 1,501" and every byte figure as "of 452,733",
and list matched, parked-T2 and still-open separately (rule 4). Never quote a
number in prose from this file; re-derive it with `tools/tiers.py`.

## Session start (in order)

    tools/refcheck.py                 # must say Glide-keyed
    tools/install_hooks.py            # idempotent; wires rule 6's hook
    tools/fileaudit.py                # note the baselines (11 stranded / 58 batches / 0 undescribed)
    tools/claimcheck.py               # 0 duplicates is the bar
    tools/t3.py                       # 0 certified today; must stay green
    git status --short src/           # another session's uncommitted files: do not touch
    tools/tiers.py                    # the denominator, before and after

## Two pools, worked in this order

The two pools have different yields and different budgets. Alternate them:
one Pool A row, then two Pool B rows, so a stall in one never idles the lane.

### Pool A -- identical instruction multiset, registers only (T2, reggap 0)

These are `tiers.py --list T2` rows whose register-blind gap is 0: the same
instructions as the original, only allocation, order or a single fact differs.
The last three passes over this class closed rows with ONE source fact each
(`docs/VC5-IDIOMS.md`, tail: declaration-order tie-break +3, guard shape +3,
sibling byte-diff screen +5). **Work them BY TRANSLATION UNIT** (cluster rule):
a lever found on one sibling usually closes the others, and a committed match
in the same TU can regress when a neighbour is re-spelled -- re-sweep the file
and read EVERY row before committing.

Primary (masked diff bytes in parentheses; smallest diff first inside a TU):

| TU | rows | note |
|---|---|---|
| `src/core/generated/0x10005330.c` | 0x10005330 BrNetBeaconTick 194 B (1) | one byte; almost certainly an immediate or a constant width |
| `src/core/drawing/br_tex3d.c` | 0x100283C0 (4), 0x10027E10 (4), 0x10027A10 (46) | header says "residue, structure exact" on the 4s |
| `src/core/geometry/br_bits.c` | 0x10018A50 (4), 0x1002F640 (7) | two leaves, 29 and 31 B |
| `src/core/geometry/br_vec.c` | 0x100345F0 BrVec3AddTo 33 B (6) | commutative FLOAT add: read the N64 twin first (`n64/`), it states the operand order |
| `src/core/gamedata/br_podwrite.c` | 0x10008BA0 (6), 0x10008C80 (9) | open/close pair; one file-handle idiom |
| `src/core/generated/0x10013E80.c` | 0x10013E80 128 B (4) | note says size-exact 30/30 insns |
| `src/core/slice2_16.c` | 0x10029480 (4), 0x10027F00 (13), 0x10027290 (20), 0x10029510 (42), 0x10029420 (45), 0x10018D50 (74) | the Gbi texture-scan family; ‼ read the file header first: one row is byte-exact only inside this TU |
| `src/core/slice2_25.c` | 0x1003C310, 0x1003C370, 0x1003C3D0 (12 each), 0x1003C1D0 (34), 0x1003C6D0 (77) | BrOptCycle family: FIVE siblings with the same residue -- run the sibling byte-diff screen (`tools/screen_slotpairs.py`, memory note) before touching one |
| `src/core/drawing/br_texlevels.c` | 0x10031030 (8) | note: REGNORM 0+0, FIRSTDIV +0x47 |
| `src/core/slice1_01.c` | 0x10003430 BrFChkFRead 140 B (8) | |
| `src/core/net/br_peerslot.c` | 0x1006B0E0 250 B (9) | |
| `src/core/slice4_50.c` | 0x10029FE0 BrMat4Perspective7 106 B (9) | x87; N64 twin for operand order |
| `src/core/startup/br_window.c` | 0x10017E30 211 B (11) | |
| `src/core/menus/br_menucb.c` | 0x10039C70 (13), 0x10039D20 (41) | both carry residue notes; read them |

Alternates: 0x1005A490 (75 B, 7, PARKED with a dead list -- only with a lever
absent from it), 0x10037F70 (43 B, 32), 0x100306D0 (51 B, 30), 0x10054550 (112
B, 33), 0x1001C9D0 (96 B, 57), 0x1006F720 (279 B, 25, residue note),
0x10064120 (238 B, 14), 0x10007D50 (291 B, 24).

**The levers, in the order to try them** (each is proven in
`docs/VC5-IDIOMS.md`; grep the entry before spending a probe):

1. Read the park/residue note in the file header. Anything it lists as dead
   is dead. A probe already in the dead list is a violation, not a retry.
2. `tools/fnmatch/fn.py <VA> --detail regnorm 30`, then ALWAYS the side by
   side: `build/match/sbs.py <obj> <sym> <VA>`. Read the first divergence.
3. Single-fact residue (2-8 B, regnorm 0+0): statement ORDER (fill records in
   address order), RETURN TYPE (64-bit pairs), a constant's width or sign, the
   `>= 500` vs `> 499` class -- `tools/msetdiff.py` keeps small immediates and
   shows these when `divergence.py` is blind to them.
4. Declaration order of the int locals: `tools/declsweep.py` sweeps one
   declaration through every position (x87 completion order and the imul
   destination both key on symbol index).
5. Guard shape: `&&` chain vs sequential early returns is decided by the
   RETURN VALUES; assign-then-override vs if/else only where the original
   HOMES the value on both edges.
6. Commutative float operand order: the N64 twin is the oracle
   (`n64/tools/n64match.py`, `build/n64/report.csv`); IDO does not
   canonicalise, VC5 does.
7. Families (Gbi, BrOptCycle): the sibling-asymmetry screen -- the ONE sibling
   spelled or placed differently is the lever for all of them.
8. Corpus: `tools/corpus.py find --from <VA> --at <off> --len 12 --source`.
   A miss is a result: stop permuting.

### Pool B -- smallest untouched drafts (T1 intake)

The lane that produced nine matches in one session (`docs/T1-INTAKE-LANE.md`;
memory note `t1-intake-lane-method`). Screened mechanically 2026-09-06: no
`6A FF` exception prologue, even address, not in the C++ lane, no `fxch`, at
most a stray 16-bit op. Take them smallest first. Draft path is where the
transcription starts; the module is found from `config/filing.csv`
neighbours and the port twin.

| VA | B | draft | screen note |
|---|---|---|---|
| 0x1006B080 | 83 | ghidra_work/0x1006b080.refined.c | clean |
| 0x10036220 | 224 | ghidra_work/0x10036220.refined.c | clean |
| 0x10036A30 | 227 | ghidra_work/0x10036a30.refined.c | clean |
| 0x10035AC0 | 231 | ghidra_decomp/0x10035ac0.c | clean |
| 0x1000E060 | 231 | ghidra_decomp/0x1000e060.c | clean |
| 0x10003030 | 233 | ghidra_work/0x10003030.refined.c | clean |
| 0x1006AEB0 | 234 | ghidra_work/0x1006aeb0.refined.c | clean |
| 0x1006B790 | 235 | ghidra_decomp/0x1006b790.c | clean |
| 0x10002460 | 252 | ghidra_work/0x10002460.refined.c | rep stosd/movsd struct copies; the intake spec's first pick |
| 0x100284E0 | 253 | ghidra_work/0x100284e0.refined.c | clean |
| 0x1005F580 | 259 | ghidra_decomp/0x1005f580.c | standings qsort; a port exists in br_racestep |
| 0x10023B70 | 273 | ghidra_work/0x10023b70.refined.c | clean |
| 0x10060A30 | 286 | ghidra_work/0x10060a30.refined.c | clean |
| 0x10004E00 | 287 | ghidra_work/0x10004e00.refined.c | clean |
| 0x10036300 | 289 | ghidra_work/0x10036300.refined.c | clean |
| 0x10001000 | 296 | ghidra_decomp/0x10001000.c | clean |
| 0x1005A500 | 297 | ghidra_decomp/0x1005a500.c | clean |
| 0x10053D20 | 305 | ghidra_work/0x10053d20.refined.c | clean |
| 0x10036510 | 311 | ghidra_work/0x10036510.refined.c | clean |
| 0x100704E0 | 272 | ghidra_work/0x100704e0.refined.c | DirectInput vcalls -- last |

Screened OUT (do not start them in this lane): C++-lane rows 0x10054070,
0x1006FCE0, 0x100087D0, 0x10038CA0, 0x1006D0B0, 0x10059350, 0x10054280,
0x1003AA10; 16-bit-lane rows 0x10023CB0, 0x10019040, 0x100541B0, 0x100684F0,
0x1006F840, 0x10001A80; x87-juggling rows 0x10024680, 0x1000E150, 0x1006D2E0,
0x10067F30, 0x1000C9E0, 0x10063E60; odd-address (split map row) 0x10035533.

**The intake loop for one function** (the six-probe budget; detail in
`docs/T1-INTAKE-LANE.md`):

1. `tools/dumpasm.py <VA>` beside the draft. Reject on sight if the screen
   missed something (an `fxch` chain, byte-lane packing).
2. Find the module (`grep -i "^0x<prefix>" config/filing.csv`; the port twin
   in `src/`/`include/` names fields). A new `br_*.c` module file is fine; a
   new `sliceN_MM.c`, or a new tag in an old one, is refused by the hook.
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
  it is compiled. At the cap: append `@t4-pass <VA> <n> <date> probes <k>
  bytes <b> insns <i> regions <r> rows <m> census <yes|no>` to the file
  header (end-of-pass numbers from `tools/probe.py` or `fn.py`), commit the
  note, next row.
- Pool B: **six probes per row.** Same ending.
- A row that reaches Gate A (`tools/t3.py --qualify <VA>`) is NOT finished:
  it owes two more zero-movement passes before it can be T3, and those are
  someone else's later passes. Log the line and move on.
- A whole TU sweep after every change to a file with more than one tagged
  row; read every row (the committed-match-regressed trap, memory note
  `collresp-cluster-2026-09-05`).
- Forty minutes with no match landed anywhere: switch pools.

## Commit discipline

- `git commit -m "<VA> <Name>: byte-exact (<what the last fact was>)" --
  <file.c> [config/filing.csv]` -- always with `-- <paths>`; never a bare
  commit, never staged behind a revert (rules 7, 8; memory
  `parallel-agent-durability`).
- `tools/filing.py` DROPS rows whose VA is absent from report.csv: check for
  LOST rows before committing `config/filing.csv`.
- Every proven fact goes into `docs/VC5-IDIOMS.md` in the same commit as the
  match. A lever that closed one row is what closes the next five.
- After every five matches: `tools/corpus.py build`, and `tools/cpp_twin_retire.py`
  if any row was a d3d twin.
- Never `--no-verify`. Never a full sweep for ordinary work.

## Expected yield, stated so it can be judged

Pool B has returned nine of eleven starts on a smaller-first pass; expect
eleven to thirteen of the twenty. Pool A rows fall to one fact each when the
fact is found, and about a third of them will not fall inside the cap; expect
seven to nine of the twenty-one primaries. Together that is the twenty, with
the alternates as the reserve. If a session ends short, the report says how
many of 1,501, which rows landed, which parked with their ledger lines, and
what the next session starts with.

## Report format (end of session)

    matched:   N of 20 (T4 now X of 1,501; Y B of 452,733)
      <VA> <Name> <B> -- <the fact that closed it> (idiom entry: <title>)
    parked T2: M (each with its @t4-pass line and residue note)
      <VA> <Name> <B> -- <numbers>, dead: <count>
    untouched: the rest, in the order to start next time
