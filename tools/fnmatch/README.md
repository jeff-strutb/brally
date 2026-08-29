# Per-function matching sandbox

Tooling for grinding ONE function to byte-exact, extracted from the
0x100250D0 BrTex3dExpand run (2026-08-28).  The point is a ~1.5-second
edit -> compile -> score cycle and a metric that does not lie.

    sh tools/fnmatch/vmake.sh <TAG> <src.c>   # private copy -> build/match/t3d/v_<TAG>.c
    sh tools/fnmatch/vdiff.sh <TAG> <ORIG>    # compile + scorecard
    sh tools/fnmatch/vdiff.sh <TAG> <ORIG> regnorm 30   # + multiset detail

`<ORIG>` is a build/match/orig/0x*.bin.  Each tag owns its own .c and .obj, so
N agents can probe the same function concurrently without contending.

## The metric that matters: register-blind multiset diff

`mdiff2.py` compares the ORDER-BLIND multiset of instruction shapes, with
three normalisation levels:

  raw       registers kept       -- noisy; moves with any allocation change
  regnorm   all GP regs -> R     -- THE HONEST NUMBER: the structural gap
  widthnorm regs and widths -> R -- ignores 8/16/32-bit differences too

Why this matters: on BrTex3dExpand the raw gap read 1097 extra / 863 missing,
which looks like a wall.  Register-normalised it was 432 / 198 -- meaning ~650
of the "difference" was one global register rotation, and the real structural
gap was less than half what the raw number implied.  Chasing the raw number
sends you after register allocation; chasing the regnorm number sends you
after the source defect that CAUSED the allocation to rotate.

**A whole-function register rotation is usually a SYMPTOM, not a wall.**  The
0x100250D0 dossier called it a terminal "coloring wall" twice and was wrong
both times: the rotation dissolved once the store-idiom source defect was
fixed.  Rank residue by the register-blind gap, never by raw diff count.

## Scorecard fields

    BYTES / INSNS   size and instruction count against the original
    FIRSTDIV        first differing byte offset; a sudden collapse toward +0x2
                    means the frame changed (`sub esp, N`) -- a hard reject
    RAW / REGNORM   extra = shapes the recomp emits that the original does not
                    miss  = shapes the original emits that the recomp does not

Trailing `nop`s after `ret` are .obj alignment padding, not code; score.py
counts them, so read BYTES as +/-15 when comparing across variants.
