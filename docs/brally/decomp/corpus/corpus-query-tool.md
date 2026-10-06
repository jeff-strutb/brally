# Corpus query tool

> tools/brally/corpus.py - query the ~1,036 byte-exact functions for the C that produced a given original instruction pattern. Use INSTEAD of inventing a spelling.

`tools/brally/corpus.py`, built 2026-09-03 (7b6673f, docs in 18e3f32 / ee44690).

**The premise.** Inventing spellings is measured dead here (permuter 0/95,
refine batch 0/258). But every byte-exact function is a PROOF that "this C
produced these bytes" - spills, slot layout and scheduling included - and
that corpus was write-only: counted, never queried. VC5's allocator is
deterministic, so wherever the original spills a value and we forward it,
the difference is IN THE SOURCE; somewhere in the solved tree, someone has
probably already written the construct that produces it.

**Use it.**

    .venv/bin/python tools/brally/corpus.py build                     # after ANY batch of matches
    .venv/bin/python tools/brally/corpus.py find --from <VA> --at <off> --len 12 --source
    .venv/bin/python tools/brally/corpus.py find --pattern 'mov R, dword ptr [esp+S]; and R, 0xff'
    .venv/bin/python tools/brally/corpus.py show --va <VA> --at <off> --len 20

`--at` takes an offset straight from `divergence.py`. `--source`/`show`
resolve a hit to real C through the compiler's own `/FAcs` listing - the
compiler's statement-to-offset mapping, not an inference. Valid because a
corpus member is byte-exact, so the original's offsets are ours.

** A MISS IS A RESULT.** "No corpus member contains this run" means the
construct is not proven anywhere in 1,036 solved functions: there is no
spelling to copy, and the site needs SOURCE truth (N64 twin, retranscribe)
rather than another permutation. On a partial hit the tool prints the
boundary between what the corpus explains and what it does not - the
instructions past that boundary ARE the open question.

** It indexes ORIGINAL bytes only**, across all three lanes (C, C++, EXE)  - 
never recompiled ones, which would poison the index with wrong spellings.
Normalisation is `msetdiff.norm`, so patterns copied out of msetdiff or
divergence.py output mean the same thing. Small immediates are KEPT
(`and R,0xff` and `and R,0xf` are different questions).

**It compounds** - every new match anywhere makes it better at the stuck
functions. This is the concrete reason T1 intake is not a detour from the
giants: it grows the oracle that may crack them.

**First finding (unblocked a ~10-session wall's diagnosis).** 0x1000A110's
byte-lane defect - orig homes both byte locals and reads them back widened
(`mov R,[esp+S]; and R,0xff; or R,R`), we forward one from a register as
`mov dl,al` - **that run exists NOWHERE in the corpus**, nor does
`mov byte [esp+S],B; mov R,[esp+S]; and R,0xff`. Closest proven relative:
byte-exact `BrGlRectFill` (0x1001E380, `src/brally/core/drawing/br_dlglide.c`),
which emits the two-instruction widening FOUR times. Its source says the
cause: **four `uint8_t` locals assigned on BOTH ARMS of an if/else and read
after the join** get memory homes and come back widened at every later use.
Same axis as the `x=a; if(c) x=b;` vs true-if/else entry proved on
0x1000EAF0 the same day, arriving from the other direction - and never
connected to this wall before, because nobody could see the two sites
together. NOT yet applied to 0x1000A110; that is the next move there.

**OUTCOME OF ACTING ON IT (session 17, d637767) - NO MATCH.** The probe the
finding pointed at (arm 1's private `packA[2]` replaced by the function-scope
`pack[2]`, so the array is defined on all three edges - the minimal statement
of the 0x1001E380 shape, and the one combination sessions 7/10 did NOT cover)
is **BYTE-IDENTICAL**: 34 regions, 1,835 insns, 7,546 B, multiset 13/5, all
unchanged.  IDIOM banked: **array IDENTITY is inert when live ranges do not
overlap** - VC5 gives a private and a shared array the same slots and code.
What is left: our pack bytes are read ONCE per colour where 0x1001E380's are
read FOUR times each. If USE COUNT (not edge count) is the discriminator, no
faithful spelling can add uses and the wall is real. Test that before
spending another session on the pack.

 **`show` IS ONLY AS GOOD AS THE SOURCE YOU OPEN.** It resolved 0x1001E380
+0xc7 correctly and gave plausible-looking NONSENSE for +0x345 and for
0x1002E79F (which is odd-addressed - suspect under the rule-2 screen anyway).
The PROC-scoping bug behind part of that is fixed (4222102), but the habit is
the safeguard: READ THE SOURCE THE TOOL POINTS AT before quoting it.

See [resume-state](../log/resume-state.md), no-token-thrashing, [vc5-idiom-dictionary](vc5-idiom-dictionary.md).
