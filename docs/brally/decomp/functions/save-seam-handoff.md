# Save seam handoff

*Recorded 2026-09-06.*

> 2026-09-05 hand-off spec: the save-slot/save-file seam (0x1003B130..0x1003BDE0, 0x100695C0..0x1006A080) - what is exact, what is parked and why, what to take next, with the exact commands

# Save-slot / save-file seam - hand-off (written 2026-09-05, session 20)

**What it is.** The record-list callbacks of the two save-slot menus
(TimeAttack `<n>.grf`, RallySeason `<n>.brf`) and the save-file writers/readers.
Plain cdecl C, first-compile territory; the C++ vcall family next to it is dry.

**Exact (6):** 0x1003B580, 0x1003BCA0 (probes, `src/brally/core/menus/br_saveprobe.c`);
0x1003B350, 0x1003BAC0 (name commits, `br_savename.c`); 0x10069DE0 (ghost
writer, `src/brally/core/settings/br_ghostsave.c`); 0x10008AB0 BrPodOpen
(`src/brally/core/cpp/0x10008AB0.cpp`, C twin retired in ghidra_batch.c).

**Parked, dead-probe lists in each header - read before touching:**
- 0x1003B6D0 / 0x1003BDE0 / 0x1003B130 in `src/brally/core/menus/br_savebegin.c`
  (one frame slot; one register pair; a 7-byte schedule). 20 slot spellings dead.
- 0x100695C0 season reader `src/brally/core/settings/br_seasonload.c`: open block
  laid inline in every C spelling, and `setne al` without zeroing = C++ `bool`.
  A bool .cpp of the same body scored WORSE (653) - needs a fresh C++ read.
- 0x10039620 `src/brally/core/cpp/0x10039620.cpp`: 572/563, one cross-jump asymmetry.

**Done 2026-09-06 (both transcribed instruction-complete, committed, PARKED on
allocation/layout walls - NOT byte-exact):**
- 0x10069A80 ghost reader -> `src/brally/core/cpp/0x10069A80.cpp` BrGhostLoad, a C++
  bool free TU (mangled `?BrGhostLoad@@YA_NPADH@Z`). 848/828, register-blind +
  block-order = 0. Two coupled residues, dead-listed in the header: VC5 pulls
  the large install block up as fall-through (same wall the season reader
  carries) + an fp/0xc register tie. The C++ lane DID kill the bool residue;
  block layout is NOT reachable by control-flow spelling (flat early-goto ==
  nested Ghidra shape, byte-identical).
- 0x1006A080 five-way loader -> `src/brally/core/settings/br_saveload.c` BrSaveLoad,
  plain C char. 608/641, register-blind 2+16. Root: the original REMATERIALISES
  the second arg (loads [arg] twice); VC5 CSEs it into ebx, making arg a 5th
  callee-saved value -> `mode` spills -> the tail epilogues become identical and
  cross-jump (the 14-insn deficit). For a plain param VC5 will not
  rematerialise (the "do not cache what the original re-reads" idiom applies
  only to computed subscripts) - no source handle found. Dead list in the header.

**Season reader 0x100695C0** is parked-claimed by another session (2026-09-06);
the "try it as a C++ bool TU" follow-up is theirs. C++ would kill its bool
residue but not its block-layout one (see the ghost-reader result).

**Image gate 2026-09-06: PASSED**, all four in-scope binaries 0 diff bytes;
BRGlide 1124 placed / 38.39% of .text. The two new parked TUs are reference-
filled (not placed) and cause no collision.

**Why:** these fell fast because the family's constructs are all proven now
(see VC5-IDIOMS tail entries added this session).

**How to apply:** procedure in [resume-state](../log/resume-state.md) session-20 block; idioms:
extern arrays for scanned strings, if/else + ONE return so saves sink past a
guard, same-object struct for a pointer + buffer when a reload stays below a
strcpy tail, frame layout size-sorted with spilled scalars lowest, block-scoped
locals share a slot, constant args cannot reach a this-in-ecx callee from C.
Related: [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md), [parallel-session-clobber](../traps/parallel-session-clobber.md).
