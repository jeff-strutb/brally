# Feedback hand transcription n64 m2

*Recorded 2026-10-05.*

> HARD RULE (project lead, 2026-10-05): N64 T3->T4 work is HAND TRANSCRIPTION from the ROM listing ONLY. No permuters, force sweeps, spelling matrices, greedy searches, ring/inject searches.

**HARD RULE.** For N64 T3 -> T4, hand transcription from the ROM listing is the ONLY permitted method. Do not run n64permute, flipsearch, spell/sp matrices, joinsearch, n64alloc force/sweep/greedy, ringsearch/inject, or any batch of generated variants. User, 2026-10-05: "I LITERALLY TOLD YOU TO USE HAND TRANSCRIPTION. WE NOW HAVE A HARD RULE AGAINST ANYTHING ELSE."

**Allowed exception (project lead, 2026-10-05):** `n64alloc.py trace` used READ-ONLY to learn why a live range got its register (priority, blocks, conflicts), then the source fixed by hand. Never force/sweep/greedy.

**Why:** session burned hours on BrCarDraw (0x80230554) with variant matrices and allocator-force sweeps after the project lead had already directed hand transcription. Search pipelines plateau. BrModLoad went 340 -> 0 only once the ROM's stack homes, spills and register lifetimes were read as evidence of the original declarations ([n64-m2-session-2026-10-04](../log/n64-m2-session-2026-10-04.md)).

**How to apply:** per row, dump the ROM listing (build/n64/m2tools/dv.py) and read it top to bottom as the program: every sp-relative access (homes give declaration order and which variables exist), register lifetimes (one home from two registers = two variables), what lives in uopt registers (v0-v1/a0-a3/t0-t5/s*/f0-f2,f12-f18 = variables or CSE'd values) versus ugen temps (t6-t9, f4-f10), branch shapes, and the saved-register count. Write the C that produces that. Compile and diff to check a transcription; do not generate candidates. Draft for BrCarDraw: build/n64/search/80230554/w.c + NOTES_.txt.
