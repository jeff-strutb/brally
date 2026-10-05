# Scene entity model

*Recorded 2026-08-17.*

> The scene-entity data model - the foundation the render half of milestone 6 is gated on. Anchored: arrays, base/count globals, stride, existing walkers all identified; the 0x2B68 entity STRUCT TYPE is the gap to fill by unioning reader accesses.

# Scene-entity data model - anchored, ready to build (2026-08-17)

The render half of milestone 6 (the F3D emitter cluster, see
[render-f3d-emitter-cluster](render-f3d-emitter-cluster.md), and more) is gated on this. Investigation found
the model is PARTLY built already (base/count globals + walkers exist); the gap
is the entity STRUCT TYPE. "Query the tree" - do not restart from scratch.

## The arrays (confirmed)
- **Entity (car) array:** base `0x10ACEDB0` (port: `g_brPACEDB0`, slice4_50.c),
  count `0x100B36FC` (port: `g_br0B36FC` / `cCars` / `g_BrCarCount` / `nEntB`),
  **stride 0x2B68**. Each 0x2B68-byte record is currently OPAQUE (`void*`).
- **Sibling array at `0x10ACDEA8`**, exactly 0x0F08 before the entity base
  (slice2_17.h:50, slice2_12.c:544). "0x10ACEDB0 - 0x10ACDEA8 = 0x0F08, zeroed
  on removal" - this is the entity add/remove bookkeeping. NOT the same array;
  don't conflate.
- **A second, adjacent count `0x100B36F8`** (g_0B36F8) drives a DIFFERENT
  0x80-stride array the render emitter 0x10013A40 loops over - reconcile which
  is which before typing (0x100B36F8 vs 0x100B36FC are one dword apart).

## Existing readers/walkers (start the field-union here)
- `BrEntityCountActive(pvRecords, cRecords)` - impl in slice2_13 (declared XSLICE
  in slice4_50.c:32); walks the entity array counting "active" - reveals stride
  and the active field.
- `0x10005470` (slice2_12.h/.c) - takes base+count as params, walks entities.
- Render emitter `0x10013A40` (D3D) - triple-nested walk: entities (count
  g_0B36F8, 0x80 stride) -> sub-meshes (0xd8 stride) -> vertices (0x18 stride,
  16-bit fields at -2/0/+2/+0x16/+0x18/+0x1a). Reads entity fields at +0x60
  (sub-list head), +0x11A0, +0x26D4, +0x2340.

## Method (the project's discipline: bound every field, no guessing)
1. Reconcile the two arrays (0x2B68 car array vs 0x80-stride g_0B36F8 array)  - 
   find each one's populator/definer (grep writers of 0x10ACEDB0 and the base of
   the g_0B36F8 array); the definer reveals layout cleanly (fields written in
   order).
2. Union every field access across ALL readers (the walkers above + every render
   function on the frontier) into one entity struct type; width/type each field
   from its access. Leave gaps as `uint8_t padXX[]` rather than guessing.
3. Define the struct in a header (its own module, e.g. br_scene.h), retype the
   `void*` globals to it, land incrementally.
4. THEN the emitter cluster unblocks (its +0x26D4/+0x2340/+0x11A0 become named).

## Status
Direction chosen by user. Anchored this session; the actual struct build is a
large multi-function task - a focused effort starts from the anchors above.
Tree is green at commit 8f18e6b; nothing here is committed yet (analysis only).
See [collision-response-decomp-state](collision-response-decomp-state.md), [readme-status-was-stale](../decomp/traps/readme-status-was-stale.md).
