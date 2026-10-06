# Corridor scan

*Recorded 2026-09-15.*

> 0x1005D060 BrAiScanCorridor (the eight-deep corridor lookahead, br_ai.h rule 8's binding gap) fully reverse-engineered and transcribed to T2 in br_ctlai.c; byte-exact is blocked in the C lane by the recursive-thiscall-with-computed-args wall -- it needs a C++ member rewrite.

**0x1005D060 BrAiScanCorridor is DONE at the reverse-engineering level and
committed as a T2 transcription (c2f33418, src/brally/core/driving/br_ctlai.c, under
`#ifdef BR_MATCHING_BUILD`, NO @implements).** It fills the corridor-scan
"binding gap" br_ai.h rule 8 names. First-pass sweep: 848/856 B (-8), INSNS
+12, RAW 82+70, REGNORM 29+17. Structure is faithful and complete.

**What it is:** `__thiscall(this=car, int depth, int mid, NODE *node)`,
`ret 0xc`, recursive. Insets both edges of `node->aPt[mid]` 0.2 toward centre
(`BrVec3Lerp`, imm 0x3e4ccccd), copies the centre, builds two midpoints
(`BrVec3Midpoint`), runs `BrSeg2Intersect` (0x100350F0, cdecl, 4 ptr args,
car+0x30 = car pos) down the corridor; on the deepest corridor yet, records
best node/pt, sets the bias pair via two more seg-intersects, copies the aim
and the mid-centre, and `memcpy`s levels 1..depth of three carried edge
arrays into g_aBrAiScanA/B (the throttle ladder's inputs). Recurses into the
next point, or into `node->pNext` + `->pSib` children when the point index
hits `node->count`. The per-depth arrays are BrVec3[9] at fixed addresses
(insetA 0x10B1CB3C, centre 0x10B1CA7C, insetB 0x10B1CADC, midB 0x10B1C8E4,
midA 0x10B1C884, hitA 0x10B1CAF4, hitB 0x10B1CB54, out3 0x10B1C958), all
declared in br_ctlai.c. The node/car structs (BrAiPathNode, BrAiPathPt,
BrAiCar) were already modelled there.

**WHY NOT BYTE-EXACT IN C (the wall):** it is a recursive thiscall whose
recursion passes COMPUTED int args (`depth+1`, `mid+1`). A real thiscall
member pushes those from a register (`inc reg; push reg`). The C-lane
`__fastcall`-as-thiscall trick must type the depth arg `float` (or an
aggregate) to keep edx free and get `ret 0xc` -- which forces each recursion
arg through a stack slot (`*(int*)&aNext = depth+1; push [aNext]`), costing
the +4 frame slot and ~half the +12 insns. This is the C++ thiscall-wall
class ([cxx-thiscall-wall](../cpp-lane/cxx-thiscall-wall.md), [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md) "one arg = fastcall
exactly" -- computed multi-arg recursion is NOT). The rest of the residue is
prologue register-colouring (which reg carries the zero; null-branch slot
numbering) -- allocator, not source ([parked-is-not-walled](parked-is-not-walled.md)).

**NEXT LEVER (byte-exact T4):** rewrite as a C++ member in src/brally/core/cpp/, so
the recursion is `this->Scan(depth+1, mid+1, node)` with int args pushed from
registers and edx naturally free. The committed C body is the exact reference
-- copy its logic, keep the array/struct offsets. This is a real T4/T3 shot.

**Probes measured this session:** field-pointer vs inline `aPt[mid]` access
INERT (VC5 folds the +0x40 struct offset into the base lea either way);
dropping the `mid` local for in-place `midArg.v` PAID (RAW -16). Both on the
C body; the wall is the convention, not these spellings.

Related: [cpp-lane-owns-top-c-targets](../cpp-lane/cpp-lane-owns-top-c-targets.md), [cxx-thiscall-wall](../cpp-lane/cxx-thiscall-wall.md),
[pool-refresh-method-2026-09-10](pool-refresh-method-2026-09-10.md).
