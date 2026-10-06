# Slot census screen

*Recorded 2026-09-05.*

> tools/brally/slotcensus.py - list every write and read of each original stack slot. Two payoffs: it catches "one variable emitting two different values", and a WRITE/READ COUNT ASYMMETRY locates a shared control-flow join (this broke 0x1000A110's 12-session byte-lane wall)

`tools/brally/slotcensus.py` (added 2026-09-03) prints, per `[esp+N]` slot of an
original function, every write and read plus the call that produced the
written value. Optionally side-by-side with a recompile's obj.

**Why it exists:** on **0x1000A110 (BrCarDrawVehicle)** two display-list
appends were both spelled `specMem`. The original reads two *different* slots
there - `[esp+0x30]` (the 1st `0x100625A0` allocation, also used by the light
calls) feeds the 0xBC3F pair, `[esp+0x28]` (the 2nd) only the 0xAE34 pair. So
the second MOVEMEM pair emits `pLights`; spelling it `specMem` put the **wrong
pointer in the display list** - a behaviour bug, not a codegen one. Fixing it
closed 2 masked regions (24 → 22) and took the register-blind multiset
19+10 → 17+14.

Nothing else sees this class: `divergence.py` sees `mov [eax+4],R` in both
streams, `msetdiff.py` compares shapes and both sites share one, the push
census sees no pushes.

**How to apply:** run it on any function that allocates or caches more than
one pointer, and on any long-stalled giant. Look first at slots the original
writes ONCE and reads several times - then confirm the source uses one
variable at every one of those reads.

##  SECOND PAYOFF (2026-09-05): a WRITE/READ COUNT ASYMMETRY LOCATES A JOIN

**This broke 0x1000A110's byte-lane wall after ~12 sessions and ~300
recorded-dead compiles** (commit fd34818). Census the two byte slots and the
counts do not balance: each is **written FOUR times, read THREE**. One arm's
writes have no read of their own - because that arm `jmp`s straight INTO a
read the *other* arm falls through to. **A slot written on N paths but read on
fewer is a SHARED JOIN, and the census is the cheapest way to find one.**

Once the join is located the question stops being "how is this expression
spelled" and becomes **WHICH VALUES MUST BE LIVE ACROSS THE EDGE.** There the
original had folded the top byte into a dword partial at the end of EACH ARM
(`xor edx,edx; mov dh,<top>` before the join label), so only the two data
bytes crossed, in memory. We carried the top across in a register instead,
which cost a register and pushed one byte back into a lane move. Assigning the
partial on both edges and starting the shared statement from it made the join
block instruction-for-instruction the original: multiset 13+5 → 9+5,
instructions 8 short → 4, bytes 31 → 16, frame intact.

 **It is a JOIN lever, not an expression lever** - the identical partial at
two straight-line sites in the same function is byte-identical, separately and
together. Full idiom in `docs/brally/VC5-IDIOMS.md`. See also
[diagnose-dont-hypothesize](diagnose-dont-hypothesize.md): the census found this in one pass on the same
day a six-worker sweep of *hypothesised* levers returned zero.

 The payoff is the VALUE, never the NAME. Renaming is inert: three
byte-identical measurements the same day (a reused parameter as a loop counter
vs a fresh local; a Ghidra-recycled temp split into its own variable at all
five sites; a local copy or `x += c` used to try to break a CSE). VC5 splits
live ranges into webs itself and value-numbers `x + c`, `t = x; t += c` and
`x += c` identically. See [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).
