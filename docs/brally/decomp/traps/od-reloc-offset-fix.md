# Od reloc offset fix

*Recorded 2026-08-22.*

> /Od COFF objects had broken reloc masking - offsets were section-relative but comparison used function-relative indices

`parse_coff_obj` in `tools/brally/match_diff.py` stored reloc offsets as section-relative
(`r_vaddr` from the section's reloc table). With `/O2` COMDAT each function IS its
own section (`value=0`), so section-relative == function-relative - correct by
accident. With `/Od` all functions share one `.text` section; a function at offset
`value` inside `.text` has `i` (function-relative) != `r_vaddr` (section-relative)
unless `value=0` (first function only). All other functions in /Od objects reported
false diffs because no reloc bytes were masked.

**Fix (`342f9f6`):** `relocs = {r - value for r in sec['relocs']}` - subtract the
symbol's section offset to make all relocs function-relative. Negative results
never match `i in [0, len)` so they're harmless. BrLogSet and a sibling matched
immediately after.

**How to apply:** If a /Od-variant function shows N "real diffs" that are all at
positions corresponding to address constants (call targets, global addresses), and
the difference is `orig=real_addr` vs `recomp=00000000`, it's this bug  - 
not a source issue. Run the sweep; the fix is already in place.
