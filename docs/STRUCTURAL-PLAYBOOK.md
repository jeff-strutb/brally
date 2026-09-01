# Structural matching playbook (paste-ready spec)

Feed this whole file to any capable AI (Claude, Grok, …) as the task spec. It
turns "match a function" into a fixed procedure so no one re-derives the
approach or guesses at specs each session. Coloring/scheduling walls are NOT
covered here — those go to `./grind.sh` (compute, zero tokens). This file is
only for functions with a real STRUCTURAL gap (wrong or missing code).

---

## Your job

Take BRGlide.dll functions from "compiles but diffs" to "byte-identical against
the original," by reading the diff, inferring the source defect, and fixing the
C. You infer the source from the bytes. You never permute spellings by trial
and error — that is the fleet's job, not yours.

Hard rules (from CLAUDE.md — do not violate):
1. Byte-exact against the original is the ONLY definition of done. Not "close,"
   not "passes tests." Never propose dropping byte-exactness.
2. Read `docs/VC5-IDIOMS.md` before matching. Add every newly proven idiom back
   to it. Each idiom is solved once.
3. The reference binary is **BRGlide.dll** (Glide), never BRD3D.dll. Matching is
   Glide-VA-keyed. Verify with `python3 tools/refcheck.py` if unsure.
4. Commit every byte-exact match immediately, on its own. Never batch, never
   stage behind a revert.
5. File each matched function into its real module (`src/core/<area>/…`) as you
   go, not in a big reorg later.
6. Shared headers under `include/` are serialized — never edit them in parallel
   with another worker. Split parallel work by `.c` file only.

## The loop (one function)

    # 0. Setup, once per session
    python3 tools/refcheck.py            # must say corpus is Glide-keyed

    # 1. Pick a target (see "Target selection" below)
    python3 tools/fnmatch/triage.py            # ranked residue, register-blind

    # 2. See the exact instruction-level gap (by VA or name)
    python3 tools/fnmatch/fn.py <VA> --detail regnorm 30

    # 3. Read the diff, classify it (see "Diff decision tree"), edit the .c that
    #    owns the function. Iterate step 2 — it is a ~2s compile+score cycle.
    #    For risky edits use a private variant so you don't disturb the tree:
    python3 tools/fnmatch/fn.py <VA> --make w1     # copy the .c to a variant
    python3 tools/fnmatch/fn.py <VA> --var w1 --detail regnorm 30

    # 4. When it reads BYTES +0 and DIFFS=0 (reloc-masked), it is byte-exact.
    #    Confirm with a one-file sweep (~12s), then commit.
    git add -p && git commit -m "<VA> <Name>: <what the source defect was>"

Never full-sweep for ordinary work; `fn.py` and the one-file sweep are enough.

## Target selection — rank by register-blind gap, NEVER raw diff count

`triage.py` columns: `symbol va origB cmpl% insnD rawgap reggap struct% verdict`.

- `reggap` = instruction-shape mismatches AFTER normalising every GP register to
  one name. THIS is the real structural gap. `rawgap` is inflated by register
  naming and lies.
- `struct%` = reggap / rawgap. High = the gap is real code, not allocation.
- Take rows verdict **`SHAPE - best targets`** with **high struct% and SMALL
  reggap** — most complete, least work, and what remains is genuine.
- `MISSING CODE (N% complete)` rows: the C is smaller than the original because
  the original inlined a helper the port factored out. Different workstream
  (source discovery). Lower priority unless N% is already high.
- `mixed` and low struct%: partly or wholly coloring. Hand it to `./grind.sh`.

## Diff decision tree — what `fn.py --detail regnorm` is telling you

Read `EXTRA` (shapes recomp emits that orig does not) and `MISSING` (orig has,
recomp lacks). Then:

- **FIRSTDIV collapses toward +0x2, BYTES far off** → the stack frame differs
  (`sub esp,N` / prologue). Hard structural — fix the locals/frame first;
  nothing after the prologue will line up until it matches.
- **MISSING has whole instructions with no EXTRA counterpart** (recomp is
  shorter) → missing code: an inlined helper, an extra store, a reloaded value
  the original kept in memory. Find it in the original asm and add it.
- **EXTRA/MISSING are the same op with a different control shape** (extra
  `cmp`/`jmp`/`test`, a `jne` where orig has `jg`) → wrong branch structure.
  Match the original's comparison tree: nested `<=`/`!=` vs `switch` vs flat
  else-if ladder all compile differently. See BrInputIsDown in slice3_45.c.
- **EXTRA/MISSING differ only in operand SOURCE** — e.g.
  `fld [R]`+`fmul [esp+S]` vs `fld [esp+S]`+`fmul [R]`, or a register swapped
  for a stack slot, with identical op counts → this is SCHEDULING / ALLOCATION,
  **not structural**, even though `struct%` scored it high (stack vs pointer
  operands normalise to different tokens). Source cannot reach it reliably.
  **STOP. Hand it to `./grind.sh`.** Do not burn tokens permuting spellings.
- **A whole-function register rotation** (every eax↔esi etc.) → a SYMPTOM of one
  earlier source-shape fork, not a wall. Find the earliest divergence and fix
  the construct there; the rotation dissolves. Never chase it register by
  register.

Proven idioms (widths, signedness, thiscall, store order, byte-slot locals,
Horner tails, etc.) live in `docs/VC5-IDIOMS.md` — check there before inventing
a fix, and record any new one you prove.

## When blocked on WHAT THE SOURCE SAYS

If the gap is "the original clearly does something the decompiled C doesn't
express" and you cannot infer the construct (dense/short/ambiguous), read the
N64 twin: Top Gear Rally under `reference/tgrally/` is the same source lineage,
IDO MIPS is near-transparent, and 195 shared debug strings pair functions with
certainty. It is a SOURCE ORACLE for structure — it does NOT help with register
allocation. See the `tgr-n64-viability` memory note.

## Stop conditions (do not thrash)

- Byte-exact → commit, file into module, pick the next.
- Turns out to be scheduling/coloring (see tree) → `./grind.sh`, pick the next.
- Genuinely stuck on structure after a real attempt → leave an HONEST residue
  note in the function's comment (what diverges and what you ruled out), and
  move on. A near-miss is not a match; only the byte-exact count is progress.
- Timebox ~40 min per function; breadth over depth-hole. Never claim "one lever
  left" — that has been wrong every time it was said.

## Reporting back

State the count with its denominator (e.g. "743 → 744 byte-exact of 1,529
hand-C targets"), what the source defect was, and which module you filed it in.
Never mix strictness levels (encoding-match vs address-verified). If you proved
a reusable idiom, say that you added it to `docs/VC5-IDIOMS.md`.
