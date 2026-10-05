# Sibling asymmetry screen

*Recorded 2026-09-04.*

> The cheapest lever on a long-stalled function: find the ONE sibling spelled differently from its parallel twins, or assigned in a different PLACE. Broke two four-plus-session walls in one session.

When a big function has been ground for many passes and every probe comes
back dead, **stop probing the construct and screen for ASYMMETRY instead.**
Two walls that had each stood four or more sessions fell to this in one
session (2026-09-03), and both were invisible to every probe aimed at the
arithmetic.

Two shapes it takes:

**1. One sibling SPELLED differently.** Parallel terms that should compile
alike, where one does not. The case: a four-term float row where terms 1, 2
and 4 read their object factor as `ptr[0]` off a dedicated pointer local and
term 3 alone read `ptr[2]` - the same pointer as term 1, at a non-zero index.
**VC5 ranks `ptr[0]` above `ptr[k!=0]`** when deciding which of a float
multiply's two memory operands gets the `fld`, so term 3 alone came out
object-first. A fourth pointer local, making all four symmetric, closed it.
 ADD a pointer local, never remove one - the opposite probe (delete them all,
spell every term off the base) is much worse, because VC5 then CSEs harder.

**2. One value assigned in a different PLACE.** Not how it is written, where.
The case: a byte pack that compiled to a one-instruction lane move
(`mov dl,cl`) where the original spends four (home the byte, reload as a
dword, `and 0xff`, `or`). No spelling of the pack reached it - explicit
widening, a named temp, a private array, operand swaps, reordering the
assignments among themselves were all measured dead. Moving the two
assignments ABOVE the preceding statement, so their live ranges span it, did:
the values then need slots and come back widened. **Read the original's
SCHEDULE, not its arithmetic** - where a load and its home sit inside the
previous statement's instructions, that assignment is above that statement in
the source.

 **THE HONEST HIT RATE, measured the same day.** The screen found two walls
in its first pass and five dead ends in its second. **Shape #1 fires often
and is usually INERT** - VC5 canonicalises most commutative and ordering
choices (integer multiply operand order, including a variable against an
inline subexpression; the order of two assignments within one pack). When it
is inert, APPLY the normalisation anyway so the screen does not stop on the
same false lead next session. **Shape #2 is the one that pays** - and its
blocker is the frame: hoisting a value needs somewhere to put it, and in a
function whose prologue is already byte-exact a new aggregate local costs a
locals dword and breaks it. Check the first-divergence address on any
shape-#2 probe that adds storage.

**How to run the screen:** grep the region for sibling expressions and
compare their spellings token for token; then compare the ORDER of the
original's loads against the source's assignment order. Both are minutes, and
both are things a probe loop cannot find because they are not variations of
the construct being probed.

 **Expect the region count to go UP on a win like this.** Both fixes raised
the masked region count while bytes, instructions, the raw gap and the
register-blind gap all improved - a closed wall re-opens small scheduling
sinks downstream. Rank by the register-blind multiset; read the region map
for WHERE, never for HOW MUCH. See [register-rotation-is-a-symptom](register-rotation-is-a-symptom.md),
[divergence-class-triage](divergence-class-triage.md), [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md), [resume-state](../log/resume-state.md).
