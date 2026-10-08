# Diagnostic forcing allowed

*Recorded 2026-10-05.*

> RULE 2026-10-05: diagnostic CDX forcing allowed to find WHICH uopt decision differs; shipped source still hand-written

the project lead chose "Allow diagnostic forcing" (2026-10-05, on BrPaintShopScreen 0x80243260 residue): pin one uopt decision at a time with CDX_FORCE / n64alloc force to learn which decision is the ROM's, then fix it by hand in source. Forced builds are never kept.

**Why:** hand-only shape guessing stalled on pure uopt colour/split residue; forcing isolates the root decision cheaply.
**How to apply:** use it to diagnose only, one decision at a time; never ship or record a forced result; the hand-transcription rule ([feedback-hand-transcription-only](hand-transcription-only.md)) still governs the source. Ask again before sweeps/permuter.
