# Structural matching playbook (paste-ready spec)

Feed this whole file to any capable AI (Claude, Grok, …) as the task spec. It
turns "match a function" into a fixed procedure so no one re-derives the
approach or guesses at specs each session.

**What this covers:** functions with a real STRUCTURAL gap — wrong or missing
code relative to the original. **What it does not cover:** pure
register-allocation/scheduling walls (T3a). Those are measured, then PARKED as
honest residue — the project has proven (permuter 0/95, refine batch 0/258,
verdict in commit 5a4a338) that no C spelling flips them. `./grind.sh` runs the
mutation loops as a free background lottery ticket, but it is NOT a lane and
closes nothing you can plan on.

## The tier ladder (shared vocabulary)

    T1   not started — decompiler C draft exists off to the side, not yet real
         project code
    T2   in progress — real in-tree code, but logic still differs (or is
         unconfirmed)
    T3b  works, built differently — behaviorally equivalent (proven by the
         differential oracle), but compiles to different instructions
    T3a  works, near-identical — same instructions, only register choices
         differ (register-blind gap 0)
    T4   done — byte-exact

    Your job moves functions UP this ladder: T1→T2 (transcribe into the tree),
    T2→T3b (make it behave right), T3b→T3a (reshape to the original's
    instructions), T3a→T4 (usually falls out; else park).

Hard rules (from CLAUDE.md — do not violate):
1. Byte-exact against the original is the ONLY definition of done. Never
   propose dropping byte-exactness.
2. Read `docs/VC5-IDIOMS.md` before matching. Add every newly proven idiom back
   to it. Each idiom is solved once.
3. The reference binary is **BRGlide.dll** (Glide), never BRD3D.dll. Verify
   with `python3 tools/refcheck.py` if unsure.
4. Commit every byte-exact match immediately, on its own. Never batch, never
   stage behind a revert.
5. File each matched function into its real module (`src/core/<area>/…`) as you
   go.
6. Shared headers under `include/` are serialized — never edit them in parallel
   with another worker. Split parallel work by `.c` file only.
7. **The cadence that compounds:** hand-solve ONE representative of a failure
   class → if the class is homogeneous, mint a generator/transform → re-batch
   the class. Never hand-grind function #2 of a class a generator could sweep.
   (This is where automation has actually moved the count — e.g. the
   calling-convention and string-array generators — not brute-force mutation.)
8. **No AI attribution anywhere.** No `Co-Authored-By` trailers, no AI or
   developer names in commits, comments, files, or branches. Commit messages
   describe the change only.

## Before you start: two screens that save whole sessions

- **EH screen.** If the original's prologue is `push -1 … fs:[0]` (a C++/SEH
  exception frame), the function is UNREACHABLE from C — it belongs to the
  separate C++ EH workstream. Check the first bytes of
  `build/match/orig/<VA>.bin` before accepting a target; skip and note it.
- **Compile-variant check.** The sweep proves each TU under one of FOUR flag
  sets: `/O2`, `/Od`, `/O2 /Oy-`, `/O2 /Op`. Look up your function's row in
  `build/match/report.csv` (the `opt` column: O2/Od/O2y/O2p) FIRST. The fast scorer
  `fn.py` compiles `/O2` only — on an `/Od`, `/Oy-`, or `/Op` TU its diff is
  partly phantom. For those, trust the one-file sweep
  (`python3 tools/match_sweep.py <file.c>`, ~12s) as the scoreboard and use
  fn.py output only qualitatively. (`/O2 /Op` matters on float-heavy TUs:
  without it MSVC5 defers float stores and the multiset gap is huge for a
  source that is actually correct.)

## Large functions

Size is no barrier — the loop is the same, ground section by section against
the FIRST divergence (nothing after a broken region is meaningful until it is
fixed). Win the prologue/frame first; the 40-min timebox applies per region,
not per function. BUT the three giants carry history you MUST read before
touching them, or you will re-run probes already proven dead:

- **0x1000EAF0** (9,354 B, ~18 regions left): read the file header of
  `src/core/drawing/br_scenedl.c` and its entries in `docs/VC5-IDIOMS.md`
  first — 60+ failed probe variants are mapped there. `tools/divergence.py`
  is its comparator.
- **0x100250D0 BrTex3dExpand** (8,480 B): size wall solved; shape residue
  only. Read its idiom entries before probing.
- **0x10019A70** (11,223 B, the largest): LAST among big targets, gated on
  131 callee signatures. One original function = one C function; never split
  it into multiple TUs to make it tractable.

Multiple workers coordinate through the claim ledger, not by guessing:

    python3 tools/fnmatch/triage.py            # refresh the lane ranking first
    python3 tools/claim_lane.py claim 5        # TOKEN + the 5 BEST unclaimed targets
    python3 tools/claim_lane.py release <TOKEN> [wallVA ...]   # park walls, free the rest

Lanes are RANKED: triage.py publishes `build/match/triage_rank.csv` and claim
hands out best-first (SHAPE targets, then mixed, then missing-code; coloring
walls last). Refresh the ranking at session start — it goes stale as matches
land. Claims go stale after 90 min, matched functions drop out automatically.
Work only functions in your lane; park true walls via release so they are
never re-handed.

## The loop (one function)

    # 0. Setup, once per session
    python3 tools/refcheck.py            # must say corpus is Glide-keyed

    # 1. Pick a target (see "Target selection" below)
    python3 tools/fnmatch/triage.py            # ranked residue, register-blind

    # 2. See the exact instruction-level gap (by VA or name)
    python3 tools/fnmatch/fn.py <VA> --detail regnorm 30

    # 3. Read the diff, classify it (see "Diff decision tree"), edit the .c
    #    that owns the function. Iterate step 2 — ~2s compile+score cycle.
    #    For risky edits use a private variant so you don't disturb the tree:
    python3 tools/fnmatch/fn.py <VA> --make w1     # copy the .c to a variant
    python3 tools/fnmatch/fn.py <VA> --var w1 --detail regnorm 30

    # 3b. When the shape gap is closed but bytes still differ, check behavior:
    python3 tools/t3b_verify.py <VA>     # EQUIVALENT / DIFF / UNCLASSIFIED
    #    DIFF on a shape-matched function = you introduced (or found) a real
    #    logic bug — fix it before anything else.

    # 4. When it reads BYTES +0 and DIFFS=0 (reloc-masked), it is byte-exact.
    #    Confirm with a one-file sweep (~12s), then commit.
    git add -p && git commit -m "<VA> <Name>: <what the source defect was>"

Never full-sweep for ordinary work; `fn.py` and the one-file sweep are enough.

## T1→T2: bringing a not-started function into the tree

Every T1 function already has a machine draft and reference bytes — nobody
reads raw asm from scratch:

- draft C: `build/ghidra_decomp/<VA>.c` (refined candidates, when the batch
  produced one: `build/ghidra_work/<VA>.refined.c`)
- original bytes: `build/match/orig/<VA>.bin`
- name/size/module hints: `config/functions_glide.csv`

Procedure: start from the draft, rewrite it into real C (fix Ghidra-isms —
see the Ghidra entries in `docs/VC5-IDIOMS.md`; >2 artifact classes in a
region means retranscribe that region fresh from the original asm, don't
patch artifact-by-artifact). Place the function in the module that owns its
neighbors (`git grep` nearby VAs), give it an `@implements <VA> glide <Name>`
comment, and run the one-file sweep — the sweep discovers new `@implements`
tags on its own; no registration step. From there it is a normal T2 target
for the loop above. `tools/autofile.py` automates exactly this for refined
candidates that already score byte-exact — check it hasn't already got your
function before hand-filing.

## Target selection — rank by register-blind gap, NEVER raw diff count

`triage.py` columns: `symbol va origB cmpl% insnD rawgap reggap struct% verdict`.

- `reggap` = instruction-shape mismatches AFTER normalising every GP register
  to one name. THIS is the real structural gap. `rawgap` is inflated by
  register naming and lies.
- `struct%` = reggap / rawgap. High = the gap is real code, not allocation.
- Take rows verdict **`SHAPE - best targets`** with **high struct% and SMALL
  reggap** — most complete, least work, what remains is genuine.
- `MISSING CODE (N% complete)` rows: the C is smaller than the original because
  the original inlined a helper the port factored out. Source-discovery work —
  consult the N64 twin (below) early.
- Low struct% (`coloring wall - real`): T3a territory. Confirm with the T3b
  oracle if reachable, record the residue note, PARK, move on. Do not grind.
- **Class awareness — symptom vs cause.** The `--residue` groups (short /
  long / frame / scattered) are SYMPTOM classes: members share how the diff
  looks, not what caused it, and transforms minted from one member have
  historically swept ~0 siblings (re-confirmed 2026-09-01: full re-batch 0/258).
  Real families are CAUSE groups found by screening ORIGINAL BYTES for a shared
  construct — e.g. the C++ vcall/twin lode (~25 byte-exact in one session,
  `tools/gen_cpptwin.py` stamps siblings after each new TU), thiscall twins,
  /Od twins, globals-form siblings. Mint a transform only after you have SEEN
  the same cause twice (rule 7); otherwise just bank the hand-solve.

## Diff decision tree — what `fn.py --detail regnorm` is telling you

Read `EXTRA` (shapes recomp emits that orig does not) and `MISSING` (orig has,
recomp lacks). Then:

- **FIRSTDIV collapses toward +0x2, BYTES far off** → the stack frame differs
  (`sub esp,N` / prologue). Fix the locals/frame first; nothing after the
  prologue lines up until it matches.
- **MISSING has whole instructions with no EXTRA counterpart** (recomp
  shorter) → missing code: an inlined helper, an extra store, a reloaded
  value. Find it in the original asm and add it.
- **EXTRA/MISSING are the same op with a different control shape** (extra
  `cmp`/`jmp`, a `jne` where orig has `jg`) → wrong branch structure. Match
  the original's comparison tree: nested `<=`/`!=` vs `switch` vs flat
  else-if all compile differently.
- **EXTRA/MISSING differ only in operand SOURCE** — e.g. `fld [R]`+`fmul
  [esp+S]` vs the reverse, identical op counts → SCHEDULING/ALLOCATION, not
  structural, even when `struct%` scored it high (stack vs pointer operands
  normalise to different tokens). **STOP. This is T3a. Record it, park it.**
  Do not burn tokens permuting spellings — proven unreachable from source.
- **A whole-function register rotation** (every eax↔esi etc.) → a SYMPTOM of
  one earlier source-shape fork, not a wall. Find the earliest divergence and
  fix the construct there; the rotation dissolves.

Proven idioms (widths, signedness, thiscall, store order, byte-slot locals,
etc.) live in `docs/VC5-IDIOMS.md` — check there before inventing a fix, and
record any new one you prove.

## When blocked on WHAT THE SOURCE SAYS

If the original clearly does something the C doesn't express and you cannot
infer the construct, read the N64 twin: Top Gear Rally under
`reference/tgrally/` is the same source lineage, IDO MIPS is near-transparent,
and 195 shared debug strings pair functions with certainty. It is a SOURCE
ORACLE for structure; useless for register allocation.

## Stop conditions (do not thrash)

- Byte-exact → commit, file into module, pick the next.
- Turns out to be T3a (scheduling/coloring) → verify with the oracle if
  reachable, leave an honest residue note, park, pick the next.
- Genuinely stuck on structure after a real attempt → leave an HONEST residue
  note (what diverges, what you ruled out) and move on. A near-miss is not a
  match; only the byte-exact count is progress.
- Timebox ~40 min per function; breadth over depth-holes. Never claim "one
  lever left" — that claim has been wrong every time it was made here.
- Saw the same defect twice in one class → stop hand-solving, mint the
  transform (rule 7).

## The final gate

Per-function matches are necessary, not sufficient: `python3
tools/image_build.py` assembles the real DLL from every matched claim and
diffs the image (currently 0 differing bytes). Run it at the end of a session
that landed matches — an image regression means a claim is wrong even though
its function-level diff is clean.

## Reporting back

State the count with its denominator (e.g. "739 → 740 byte-exact of 1,529
hand-C targets"), what the source defect was, which module you filed it in,
and any idiom added to `docs/VC5-IDIOMS.md` or transform proposed. Never mix
strictness levels (byte-exact vs behaviorally-equivalent vs shape-matched are
three different claims — say which one you have).
