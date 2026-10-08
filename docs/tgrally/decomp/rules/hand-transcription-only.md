# Hand transcription only

*Recorded 2026-10-05.*

> HARD RULE 2026-10-05: hand transcription ONLY on BOTH the N64 and PC decomps, residue included; no search batches, sweeps, permuters, spelling matrices or force-sweeps unless the project lead says otherwise

HARD RULE (the project lead, 2026-10-05): from now on, unless the project lead explicitly says otherwise in chat, matching work on the N64 decomp (IDO) AND the PC decomp (MSVC 5.0) is HAND TRANSCRIPTION ONLY. This applies to the last few instructions of a function too, not just the bulk.

**Why:** the project lead had already directed hand transcription (superseded by this). On BrCarSelect 0x8020D004 ([n64-carselect-handtranscription-2026-10-05](../functions/n64-carselect-handtranscription-2026-10-05.md)) I hand-transcribed 3872 -> 14 diffs, then slid back into generated variant batches (400+ loop spellings, a 364-variant local-pairing sweep, allocator force sweeps) for the last register rotation. That burned his time and credits and broke both his direction and the machine-wide "do not guess" rule. Human decomp teams reached byte-exact by reading the compiler output and reasoning about it, and that is the standard.

**How to apply:**
- Read the original listing (N64: dv.py / ROM objdump; PC: the original bytes) and reason out what the programmer wrote: homes and stack slots give declarations and order; register lifetimes give variables (one home from two registers = two variables); saved-register count; where loop counters are set and reset; arm order; operand order. Write the source to fit.
- For residue: work out the cause from the listing and the compiler's rules (the instrumented allocator trace is fine for READING why a choice was made). Then make ONE reasoned edit and check it.
- FORBIDDEN unless the project lead says otherwise: generated variant batches, pgen/psearch/perm scripts, sweep2/n64alloc sweep, force sweeps used as search, permuter, flag sweeps, "try N spellings and keep the best" loops. Using `n64alloc force` to confirm a diagnosis is fine; using it to search is not.
- If stuck, go back to the listing and read more of it, not more variants.
