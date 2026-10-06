# Masked reloc operand swap

*Recorded 2026-09-24.*

> byte sweep / fn.py mask relocations - a swapped fld [g1]; fmul [g2] pair reads BYTE-EXACT; check every DIR32 vs the original absolute; first declaration in the TU sets the fld side

2026-09-24, 0x10016C90 BrWeatherStepParticles (1142 B, now T4): fn.py said
BYTE-EXACT and the sweep said match while 5 both-memory x87 pairs had their
symbols swapped (constant on the fld side where the original loads the
variable). Masking hides the symbol, so only a per-reloc check sees it.

- Check: for each DIR32 in the obj, symbol `DAT_<hex>` + addend must equal
  the dword at the same offset of `build/brally/win32/match/orig/<VA>.bin`; REL32 target
  must equal the callee VA. Scratch script used this session:
  coff_relocs.func_relocs + that comparison (no in-tree tool yet).
- Fix lever: declaration order - the LATER-declared symbol takes the fld side,
  and it is the FIRST declaration in the TU that counts (dt was first declared
  in the lightning arm above, so 318/334 had to be declared there, ahead of it).
- A7 whole-image run would also catch it; the one-file sweep never does.

**Why:** "byte-exact" rows can be wrong at run time. **How to apply:** run the
reloc check before every T4 commit of a loose-globals matching arm. Idioms:
docs/brally/VC5-IDIOMS.md tail. Related: [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md), [placed-image-verification](../oracle/placed-image-verification.md).
