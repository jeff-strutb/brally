# N64 ido float literal spelling

*Recorded 2026-10-06.*

> IDO keeps a float literal's source spelling as its constant identity: 0.0f, 0.f, .0f, 0.00f, 0e0f are DIFFERENT uopt constant webs; int 0 and double-literal-to-float conversions share one "0.0000e+00" web

Measured 2026-10-06 with `cc -S` on a probe (ugen `li.s` operands): `0.0f`/`0.0F` -> "0.0", `0.f` -> "0.", `.0f` -> ".0", `0.00f` -> "0.00", `0e0f` -> "0e0", `00.0f` -> "00.0"; `0`, `0u`, `0L`, `'\0'`, `(short)0`, `0.0`/`0.` assigned to float, `(float)0.0`, folded `0.0f - 0.0f` -> all "0.0000000000000000e+00". Same for any value (1.0f vs 1.f etc.).

**Why:** earlier notes ([n64-axlegrip-wip-2026-10-06](../functions/n64-axlegrip-wip-2026-10-06.md)) said "float literals merge regardless of spelling" and "only two zero webs exist"; both are wrong. Each spelling is its own uopt constant symbol, so it has its own web, priority and pieces.

**How to apply:** when a constant web's priority/uses are off by a count that literal KIND swaps cannot fix (uses conserved between two webs), a use spelled differently in the original leaves both webs. A single-use spelling has save 0 and is never coloured (rematerialised), so it is invisible in the code except through the other webs' allocation.
