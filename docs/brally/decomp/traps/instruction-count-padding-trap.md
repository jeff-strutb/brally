# Instruction count padding trap

*Recorded 2026-09-03.*

> Recompile instruction counts included 16-byte alignment padding, fabricating EQUAL counts on 0x1000EAF0 and 0x1000A110; fixed 2026-09-03.

Until 2026-09-03 `tools/brally/divergence.py` counted the COFF function extent's
16-byte alignment padding (up to 15 trailing `nop`s the extracted original
does not have) as recompiled code. Three dossiers then recorded an
instruction-count EQUALITY that never held:

- 0x1000EAF0 - 2,322 vs 2,328: **six short** (eighth pass said "EQUAL").
- 0x1000A110 - 1,828 vs 1,843: **fifteen short**, 56 bytes short. Its frame
  census reasoned *from* the equality that "no value is missing" and that
  the 0x48-vs-0x4c frame gap was a packing curiosity. Retracted; region 1
  is open again.
- 0x100250D0 - 2,408 vs 2,407: genuinely at parity, residue really is shape.

**Why:** an instruction-count equality is load-bearing - it is the claim
that licenses "the residue is shape, not missing code", which is what sends
a session to T3a/park instead of hunting for absent code.

**How to apply:** both `divergence.py` and `tools/brally/msetdiff.py` now strip the
padding and report it separately. Never quote an equality that has not been
padding-corrected, and re-check any older one before building on it. Same
session, `msetdiff.py` also had to learn that the object stores the reloc
ADDEND (so `push 0` / `[esi]` / `[esi + 4]` are the original's
`push 0x106e9a38` / `[esi + <abs>]`) - 76 rows of noise down to 37 real.
See [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md), scenedl-0x1000eaf0-state,
[counting-reconciliation](counting-reconciliation.md).
