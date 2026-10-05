# D3d twin glide transcription

*Recorded 2026-09-21.*

> How to transcribe a Glide twin whose D3D-shaped port body diffs heavily; __fastcall+globals arm, induction pointers, dead-init removal

A whole `sliceN_MM.c` tagged `d3d` is still graded against the GLIDE twin via
`config/shared.csv` (BR_REF=glide). A `shared,body` row whose Glide twin diffs
by most of its bytes means the two builds have DIFFERENT shape - usually the
Glide twin is `__fastcall(pCar)` with `pEnv`/`pSeed` fields as file-scope
globals, while the D3D port body is multi-arg cdecl. The existing D3D reverse is
the Rosetta stone: same algorithm, constants, struct offsets, helper calls.

**Fix pattern (mirror `BrCarSub9020` in `src/core/slice4_53.c`):**
- Keep `@implements <d3d VA> d3d <Name>`. Add a `#ifdef BR_MATCHING_BUILD` arm:
  `void __fastcall <Name>(struct BrCar *pCar)`, `pEnv->x` → `extern` DAT_<addr>
  globals, RNG → `BrRandom` (0x100353D0), truncation → plain `(int)`/`(short)`
  casts to hit `_ftol` (0x10074560), **NOT** `BrFtolTrunc` (0x1007C8A0, a
  different trunc). Port body stays in `#else`.
- Guard the header decl (`#ifdef BR_MATCHING_BUILD` fastcall 1-arg / `#else`
  original) and every caller site for the arg-count change.
- The `@t3`/`@t4-pass` tags carry the **GLIDE** VA (the report-row VA), while
  `@implements` keeps the d3d VA. Verified precedent: slice1_01.c `@t3 0x10003430`
  + `@implements 0x100030E0 d3d`.

**Two biggest codegen levers (both took 0x10032880 BrCarWheelFx to oracle
EQUIVALENT, insns 405/405, from a 1220-byte diff):**
1. **Delete dead zero-inits copied from the d3d draft.** `vecB=vecA; vecC=vecA;`
   upfront (both always overwritten) cost +53 bytes / +10 insns of frame+spill.
   Removing them: +3 bytes / +0 insns. Check every upfront init is actually live.
2. **Write induction pointers, not `CAR_*(pCar, base + i*stride)`.** VC5
   strength-reduces the original to base pointers advanced by a fixed byte stride
   each iteration; the `base + i*stride` form emits scaled-index addressing and
   diverges structurally. Declare the base pointers before the loop, walk them at
   the bottom (`p = (T*)((unsigned char*)p + stride)`).

Residue that stays (→ T3, not T4): VC5 CSE-merges two same-stride induction
pointers (keeps `&A - B` constant), costing frame slots; and the x87
commutative-subtract operand-role fork (`a_local - b_mem` comes out
`fld b_mem / fsubr a_local` vs the original's `fld a_local / fsub b_mem`) - see
x87-wall-mechanism. Neither moves from source. Certify via
[equivalence-oracle-in-image-2026-09-10](../oracle/equivalence-oracle-in-image-2026-09-10.md) + Gate B ledger ([t3-certified-standard](../rules/t3-certified-standard.md)).
See also [glide-is-the-reference](../rules/glide-is-the-reference.md), [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md).
