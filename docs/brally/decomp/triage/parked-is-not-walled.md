# Parked is not walled

*Recorded 2026-09-04.*

> A parked function with regnorm 0+0 and 2-6 diff bytes is usually SOURCE STATEMENT ORDER, not a register-colouring wall. Two such notes were overturned on 2026-09-03.

**A "coloring wall - real" verdict on a small function is a hypothesis, not a
finding.** `triage.py` prints that verdict from register-blind gap alone: it
means "the instruction multiset matches", which is equally consistent with a
genuine allocation wall AND with the source writing the same statements in a
different ORDER.

Overturned 2026-09-03: `0x1002A7A0 BrMat4Scale` had carried a residue note
calling its 2 diff bytes a scheduling wall. It was not. Matrix and record
builders assign their fields in **address order, row by row** - the zero
stores that fall between two interesting values are what force VC5 to reuse
a freed register instead of hoisting both parameters. Re-spelling in address
order made it byte-exact. Its untagged sibling `0x1002A7F0 BrMat4Translate`
matched on the first compile with the same shape.

Also overturned the same day: `0x10071F00`, at regnorm 0+0 / raw 3+3, looked
like a pure `ecx`-vs-`edx` colouring residue. The cause was the **return
type** - `edx:eax` is the only pair a 64-bit return can use, so a register
difference on a 64-bit value is a signature question, never colouring.

**Why:** these two classes are register-blind-INVISIBLE by construction, so
the one metric the triage ranks by cannot distinguish them from a real wall.
The parked pool is therefore systematically contaminated with source-order
work, and a park timestamp shared by a dozen rows means one lane ran out of
time, not that a dozen walls were found.

**How to apply:** before accepting a park on any function whose instruction
count is already exact and whose residue is under ~6 bytes, ask (a) is this
filling a fixed-layout record? - then re-spell the assignments in address
order; (b) is a 64-bit value involved? - then check the return type. Only
after both come back no is it worth writing the wall note. See
[register-rotation-is-a-symptom](register-rotation-is-a-symptom.md) and [divergence-class-triage](divergence-class-triage.md).
