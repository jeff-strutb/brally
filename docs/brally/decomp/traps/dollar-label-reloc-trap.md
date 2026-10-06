# Dollar label reloc trap

*Recorded 2026-09-03.*

> A '$'-prefixed COFF symbol is a per-TU compiler label and can never be keyed globally; learning one put 3 wrong bytes in the image gate.

MSVC5 emits `$L<n>` (C++ EH handler thunk + funclets, into `.text$x`), `$T<n>`
(EH temp) and `$SG<n>` (string literal) as COFF symbols. **The number is a
per-TU counter - the same name means a different address in every object.**

Fixed 2026-09-03 (commit `a8bf973`). `config/brally/globals_learned.csv` had learned
`$L459` from 0x10029290's TU; 0x10053590 reuses the name for its own EH
handler, so `reloc_fill.resolve` answered 0x100293B4 where the original
pushes 0x100765D4 - **3 differing bytes in the image gate on a function whose
`report_cpp.csv` row reads `match … 4/4`**. `tools/brally/reloc_learn.py`'s agreement
guard is blind to this by construction: a label seen in exactly one object
"agrees" with itself and is written as corroborated.

Both `learn_from` and `resolve` now refuse any `$`-prefixed name; the site
falls to reference-fill. 121 such rows dropped from the map.

**Why:** the sweep masks relocation slots, so a per-function `match` says
nothing about whether the ADDRESS in that slot is right. Only
[image-build-gate](../oracle/image-build-gate.md) fills the slots for real - it is the only check that can
catch a mis-keyed symbol, which is the second class of thing it catches beyond
overlapping claims.

**How to apply:** when the image gate reports a handful of differing bytes in
an otherwise-matched function, dump the offsets and read the reloc's TARGET
SYMBOL NAME before touching the source - the source is probably fine and the
map is lying. Any symbol name that is not globally unique (`$`-labels,
per-file statics) must resolve to None, never to a plausible address.
See also [counting-reconciliation](counting-reconciliation.md), [resume-state](../log/resume-state.md).
