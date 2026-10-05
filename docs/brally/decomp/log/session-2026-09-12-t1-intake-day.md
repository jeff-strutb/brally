# Session 2026 09 12 t1 intake day

*Recorded 2026-09-12.*

> 2026-09-12: the FRESH-T1 pool is real (70 rows after the de-dup filter) and pays 8 byte-exact + 3 T3 in one lane-day; the winning cadence, the new levers (while-entry shrink-wrap, in-loop LICM deadline, volatile BrVec3 trio, memset'd desc, label-inside-failure-arm, z/y/x cross reuse), and the walls hit.

** THE HEADLINE: "the ready pool is empty" was about QUALIFIED DIFF ROWS.
The untried-T1 pool is a different pool and it was 70 functions deep** (the
de-dup filter from [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md) over `tiers.py --list
T1`: drop certified, EH prologues, cpp scratches, and ledgers at BOTH twin
addresses). The project lead called this out and was right. One lane, one day:
**8 byte-exact, 3 @t3 certified, 5 parked T2 transcriptions** - and with the
parallel session the tree gained 7 @t3 (99 -> 106) and ~10 byte-exact dated
today. 20 in one lane is still not a day's work; 11 at-or-above the bar is.

**The cadence that paid (each function 15-45 min):** disassemble the orig
FIRST (capstone one-liner), read the Ghidra draft as a SEMANTIC map only
(three of them were outright wrong: a phantom 2nd arg in 0x10036300, a
missed 3rd arg in 0x10035C50, a wrong return value in 0x10027B60 - the
ARG-SLOT DEPTH MATH decides, not the draft), pick the LANE (thiscall with
stack args -> cpp; thiscall this-only -> __fastcall C), transcribe, then
divergence-driven probes. First-compile matches happened THREE times
(0x10053D20, 0x10036510, 0x100541B0-at-1-diff) - the idiom dictionary is
now good enough that faithful transcription often just lands.

**New proven levers (all on the VC5-IDIOMS tail, commit 84fdff8):**
- in-loop invariant deadline -> LICM add in the preheader (0x10004E00).
- `while` whose entry test exits through a DUPLICATED shrink-wrapped
  epilogue; a mid-loop `if (g != -1) break` before an inner wait loop IS
  the inner while's rotated entry test - writing it doubles the guard.
- `volatile BrVec3` + assignment-expression = the dead-store trio
  (0x10067F30); volatile GLOBALS made things worse (183 diffs).
- z/y/x cross-product temporaries transfer verbatim (0x10063E60 byte-exact
  in one probe off BrVec3Cross's comment).
- 12-byte record copy = three scalar copies, NOT an aggregate assign
  (aggregate assign materialises the &field base, 0x10063E60).
- DPSESSIONDESC2 zeroing is memset (an `= {0}` initializer RELOCATES the
  aggregate's slot block; memset keeps it in declaration position)  - 
  0x10035C50, 0x1006B240.
- the label-INSIDE-the-failure-arm idiom (br_sndload's) reproduced on
  0x1006B240; per-site `r = 0; goto out` DUPLICATES the tail, a shared
  `zero_out:` label keeps VC5 to one copy with an fstp on the entering
  edge (0x100656F0).
- int-typed locals for byte-table reads = xor+mov zero-extension batch
  (0x1005C560 byte-exact).
- COM under matching = BR_STDCALL on every vtable member (slice1_08.h
  edited); a caller's `p->pBuf` must be RE-READ per call site, and a
  Lock out-param can be COLORED into the dead arg slot only when it is
  the UNINITIALISED one (pLock1/pLock2 zeroed, nLock1 not - the reverse
  breaks the frame).

**Walls hit and parked with dead lists:** the v*v pop-discipline fmulp
(0x100684F0, corpus MISS), the fsubr literal-fold + fld-the-named-operand
pair (0x10067F30 - literals fold the negative multiply, named/static/extern
constants become the fld operand; ONLY /Op fixes both and breaks the rest),
the unfolded `fld u; fadd st(1)` CSE add (0x100656F0, corpus MISS), the
glyph SIB byte (0x100541B0, third instance of 0x100540D0's class), the
entry-register-roles + slot-rotation bucket (0x10058E20 cpp).

**Traps re-confirmed:** 0x10068600 was already matched in br_wheelvel.c
under its FUN_ name - GREP THE TREE before opening any "fresh" VA (rule
re-learned the hard way; resolved by refiling into br_carphys.c). The
pre-commit hook refuses a new @implements in ANY sliceN batch - file into
the module BEFORE the first commit attempt (0x1006B240 -> br_sndbuf.c; the
move held byte-for-byte). fxch-count is a good 30-second screen: >6 in a
math body = walk away (0x1000E150, 0x1006D2E0, 0x10063FA0, 0x100682C0 all
skipped on it); ZERO fxch + calls = take it.

Related: [t3-frontier-map-2026-09-10b](t3-frontier-map-2026-09-10b.md), [calling-convention-screen](../triage/calling-convention-screen.md),
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md).
