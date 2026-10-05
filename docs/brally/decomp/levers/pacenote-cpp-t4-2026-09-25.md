# Pacenote cpp t4

*Recorded 2026-09-25.*

> 2026-09-25: 0x10011300 BrPaceNoteEmit T2->T4 by fresh hand transcription; it was C++ all along. Screen: [g + off] SIB order (global as base) that no C spelling reaches = C++ TU.

0x10011300 BrPaceNoteEmit (846 B, largest open T2 at the time) went T2 -> T4
(commit e0237954) in one session by re-transcribing from the asm instead of
patching the Ghidra draft (REGNORM 30+50 -> 5+5 on the first compile).

**The C-vs-C++ tell:** a plain cdecl free function, no EH, no `this` - looks
like C. But every list read was `[g + n*32 + disp]` with the freshly loaded
global as SIB base. The C front end always emits the offset as base under
every spelling, preamble size and decl order; the C++ front end emits the
original order (with a preamble in the tie-break window: `<stdio.h>`). Filed
as `src/core/drawing/BrPaceNoteEmit_10011300.cpp` with `extern "C"`.

**Why:** "no EH / no thiscall" does NOT prove C. Operand-role residue that
survives all C levers is worth one `/TP` compile of a micro-repro.

**How to apply:** when a C-lane T2 is down to commutative/SIB roles, compile
the same body as `/TP` before sweeping further; then preamble-size sweep and
declsweep under C++ (levers differ per front end - C pads fixed colour roles
at 16, C++ needed 108+). zsh gotcha: `${=o}` to word-split flag strings, else
cl fails silently and you read a stale .obj.
Related: [encodedelta-vc5-t4-2026-09-25](encodedelta-vc5-t4-2026-09-25.md), [declaration-order-tiebreak](declaration-order-tiebreak.md),
[cpp-lane-t3-filing-workflow](../cpp-lane/cpp-lane-t3-filing-workflow.md), [retranscribe-not-patch](../triage/retranscribe-not-patch.md).
