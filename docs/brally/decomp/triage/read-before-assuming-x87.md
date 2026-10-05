# Feedback read before assuming x87

*Recorded 2026-08-19.*

> READ the asm before classifying a block as 'needs x87emu'. Three blocks labelled x87-dense turned out to be trivial, wasting sessions of avoidance.

**Read the asm before classifying a block as "needs x87emu."** Do not
label a block as deferred/x87-dense without actually reading its
instructions. The label becomes a self-fulfilling avoidance: future
sessions see "x87emu required" and skip it, compounding the delay.

**Why:** Session 2026-08-19 filled the three "x87emu-required" TODO
blocks in BrCarDrawVehicle (0x1000A110). ALL THREE turned out to be
trivial once read:
- Light-dir (0xA5A1-0xA6F3): BrVec3 library calls, zero interleaved FP
- Specular (0xA6F6-0xA81B): pool allocations + argument marshalling for
  two existing library functions, minimal FP
- Post-detail (0xB925-0xBC7B): mechanical put commands with ONE
  fld/fmul/ftol triplet

The original classification was made by glancing at the block size and
seeing "x87" in the surrounding context, not by reading the actual
instructions. That single misjudgement deferred the function's claim
across two full sessions.

**How to apply:** when scoping a function, read EVERY block that might
be deferred. Count the actual fld/fmul/fxch/faddp instructions. If
there are fewer than ~15 interleaved x87 ops, it is NOT dense FP and
does not need the emulator. Label blocks as x87emu-required ONLY after
confirming the density by reading.

**Cost:** the project lead explicitly flagged the wasted time and money from
the avoidance pattern. "It wastes my time and money when you do."

Related: no-token-thrashing, depth-over-breadth,
[implements-requires-execution](../rules/implements-requires-execution.md).
