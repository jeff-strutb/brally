# Declaration order tiebreak

*Recorded 2026-09-04.*

> LEVER 2026-09-04/05: local DECLARATION ORDER is a VC5 codegen tie-break in TWO mechanisms - symbol INDEX (x87 completion order, 0x1000EAF0 21->18 masked) and PAIRWISE order of two named factors (the imul destination, 0x100250D0 31->28). Retracts 'declaration order is inert'.

**Declaration order of locals reaches VC5's x87 scheduler and register
allocator as a tie-break keyed on the symbol's absolute INDEX** (commit
434e739, 2026-09-04). On 0x1000EAF0 the row block finished T1 before T2 for 25
passes; declaring the four object-pointer locals in field order (pTw LAST)
flips it to the original's order at all four rows. All 24 permutations
measured: exactly two outcomes, keyed on pTw's position (1st/2nd vs 3rd/4th);
the same flip occurs with the pointers untouched when a function-scope local
leaves the list (merged or block-scoped), so it is `(index - k) mod 4`, not
pairwise order. Moving `cHead` or `nTotal` to the end of the function-scope
list ALSO moves regions (tail fixup / object-loop update) -- so the register
allocator's tie-breaks respond too, not just the x87 scheduler.

**Why:** every giant's dossier carries dozens of "byte-identical, do not
re-run" verdicts measured at ONE symbol layout. A lever that reads inert may be
one index away from its flip.

**How to apply:** when a schedule or allocation is one notch off and every
expression form is dead, sweep the declaration order (`tools/brally/probe.py` scores one
variant in ~6 s with every measure at once; `tools/brally/declsweep.py --mode all`
sweeps a declaration through every position): move each local to
the end/front of its run, then every position. Inert: extern declaration
order (24/24), unused extra locals (never indexed), renames. Slot PACKING is
still not declaration-ordered (pDst to the top of the list: byte-identical).
See docs/brally/VC5-IDIOMS.md "DECLARATION ORDER IS AN x87 SCHEDULER TIE-BREAK" and
the 26th-pass entry in src/brally/core/drawing/br_scenedl.c. Related:
scenedl-0x1000eaf0-state, [resume-state](../log/resume-state.md), [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).

** A SECOND, DIFFERENT MECHANISM (2026-09-05, commit 6836931): the `imul`
DESTINATION.** For a product of two NAMED locals, VC5 makes the LATER-DECLARED
symbol the destination register and the earlier one the memory operand
(`mov edx,later; imul edx,[earlier]`). Here it is PAIRWISE between the two
factors, not a mod-4 bucket -- moving either through every other position
changes nothing until it crosses the other. It costs real instructions where
the product must survive a one-operand `imul`: MSVC's divide-by-255 is
`imul <magic>` then `add edx,<the product again>`, so with the wrong factor as
destination the product is homed and reloaded (an extra temp dword, and a
changed frame size). On 0x100250D0 the widened intensity had to be declared
after all four channel deltas: key-10 masked regions 31 -> 28, multiset 81 ->
70 rows. **Screen: `mov R,[esp+S]; imul R,R` EXTRA against `mov R,R;
imul R,[esp+S]` MISSING, both factors locals -> swap the two declarations.**

** AND A FRAME THAT READS RIGHT CAN BE TWO ERRORS CANCELLING.** That same
function also needed one `unsigned char` local PER loop body where it had one
shared across two bodies (the original spends a byte slot per body). Split
alone = `sub esp,0x6c`, the imul fix alone = `0x64`, together = the original's
`0x68`. Read the /FAcs equate table, never `sub esp,N` alone.

**WHERE IT DOES NOT APPLY (measured, ~300 compiles, 2026-09-05): 0x1000A110.**
Every function-scope index, both comma lists split, block scopes, the pack
array at all five positions -- all byte-identical. The lever needs comparable
float PRODUCTS read through pointer locals, or two NAMED factors of an integer
product. A function whose residue is a byte-lane spill has neither.
