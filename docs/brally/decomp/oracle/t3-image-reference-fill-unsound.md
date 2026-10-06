# T3 image reference fill unsound

*Recorded 2026-09-18.*

> Real-hardware crash traced to image_build reference-filling T3 reloc slots at the same offset; fixed 81dd5efb - T3 lane now blocks unnameable slots and resolves DAT_/FUN_ names by address; 56/118 T3 place, 62 blocked on statics

**2026-09-18: the T3 image page-faulted on real Win98 hardware** (project lead test:
byte-exact BRGlide ran; BRGlide.T3.dll crashed at 0x1006e372 in BrTimeUpdate,
`mov [0x00000011], ecx`).

**Root cause:** `image_build.compiled_functions` filled an UNRESOLVABLE reloc
slot with the reference image's dword AT THE SAME BODY OFFSET. Sound only for
masked byte-exact matches; a T3 body's certified residue is a reschedule, so
its slots sit at DIFFERENT offsets and the copied dword is arbitrary
instruction bytes. 89 of 118 placed T3 functions carried such slots - the old
"every relocation resolved: 118" was standing on garbage. The gate never saw
it because bytes differing inside a T3 span are "expected residue".

**Why:** offset-keyed reference data is only valid when layout is identical.
T3 ≠ layout-identical, by definition. Any future tool that copies per-offset
data from the original into a T3 body has this same hole.

**How to apply (fixed in 81dd5efb):**
- T3 lane passes `ref_fill=False` → an unnameable slot BLOCKS the function
  (honest "blocked on an unknown address"), never fills from reference.
- `extra_resolve=t3b_env.address_in_name` recovers DAT_/FUN_/BrSubXXXXXXXX
  names (they encode their own VA, section-validated) - the same convention
  the A5 oracle certified under.
- Byte-exact (C/T4/EXE) lanes keep ref_fill=True - sound there.

**Current state:** 56/118 T3 functions place validly; 62 blocked on hand-named
statics (`_g_s17$S695`, `_have`, `_car_at`) and hand-named globals with no
surveyed address (`_g_brRaceBeginOps`, `_g_br73`, `_BrBitReaderRead` ≈1744
symbols / 4748 slots). The gate now reports FAILED on those blocked claims  - 
that is the truth, not a regression. Blocked functions keep ORIGINAL bytes in
the emitted image, so BRGlide.T3.dll(.FAILED) is runnable and tests the 56.

**Follow-up lever:** recover static/hand-named-global addresses for the 62  - 
e.g. teach reloc_learn to pair a T3 recompile's reloc sites with the
original's by opcode+register (BrTimeUpdate's three `mov [imm32]` stores pair
uniquely), or survey the globals into config/brally/globals_glide.csv. Related:
[oracle-resolution-advances-2026-09-16](oracle-resolution-advances-2026-09-16.md), [dollar-label-reloc-trap](../traps/dollar-label-reloc-trap.md).
