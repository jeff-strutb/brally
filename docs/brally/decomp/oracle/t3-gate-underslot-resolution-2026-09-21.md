# T3 gate underslot resolution

*Recorded 2026-09-21.*

> T3 image gate now GREEN; under-slot placement resolves via augment_maps like the annex path (commit f21ec9f5), plus the FUN_100656F0 collision fix

2026-09-21: **BRGlide T3+ image gate PASSES** (`CONTRACT-VALID (T3+) GATE
PASSED`, exit 0). Two fixes landed:

1. **f21ec9f5** - `tools/image_build_t3.py` main (under-slot) placement path
   now resolves relocations with `augment_maps` + `address_in_name`, the SAME
   resolution the annex (over-slot) path already trusted and that
   `compiled_functions`' docstring documents this lane as passing. Before, the
   under-slot path used `fnmap` + image-paired `sites` only; a certified T3
   body's residue is a reschedule, so its reloc slots sit at offsets pairing
   can't recover → a few refs stayed unresolved and `blocked>0` failed the gate
   on BrGhostPickBlend (0x10005810), BrModelSwap (0x100302A0), BrCarTrailStep
   (0x10032E40). **`ref_fill` stays False** - a slot with no known address still
   BLOCKS, never copies a reference dword (the 0x1006E360 page-fault class,
   [t3-image-reference-fill-unsound](t3-image-reference-fill-unsound.md)). Verified: all three place unres=0,
   fromref=0, correct call/data target addresses.

2. **82be257f** - 0x100656F0 (driving 2-D barycentric containment test in
   `src/core/driving/br_tritest.c`) was certified naming itself
   `BrTriContainsPoint`, colliding with the byte-exact geometry function
   0x10034FC0. One C symbol binds one VA → the image gate dropped 0x100656F0 as
   "symbol not in obj". Renamed to `FUN_100656F0` (canonical name is blank in
   config/functions_glide.csv; `FUN_<VA>` self-encodes the VA). Its shared-body
   twin 0x1006C740 is already `BrTriContainsPoint2D`.

REUSABLE: **"symbol not in obj" for a function that clearly exists = a
NAME collision** (two @implements sharing a C symbol), not a missing symbol  - 
grep report.csv for the name. **`blocked on an unknown address` fails the gate**
(image_build_t3 line ~927 `claims_bad = ... or blocked`); the addresses are
usually all recoverable by augment_maps (declared `/* 0x<VA> */`, self-encoding
names, CRT imports) - the fix is wiring, not filing. See
[oracle-runs-orchestrators](oracle-runs-orchestrators.md), [t3-certified-standard](../rules/t3-certified-standard.md).

CAUTION already handled this session: the gate emits exit 2 + a "TREE CHANGED
WHILE GRADING" banner when a parallel session/crank writes the tree mid-run;
record nothing from a raced run - re-grade on a still tree ([image-build-gate](image-build-gate.md)).
