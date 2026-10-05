# Render f3d emitter cluster

*Recorded 2026-08-17.*

> The F3D display-list emitter cluster (4 fns, 2 milestone-6 frontier boxes) is scoped but BLOCKED on the scene-entity data model, not ready to transcribe. Cursor owner solved; the entity struct it loops over is not modeled.

# F3D display-list emitter cluster - scoped but BLOCKED (2026-08-17)

**Reassessed after reading the actual bytes:** these emitters are NOT a clean
"transcribe the command sequence" win. The cursor-owner blocker IS solved (see
below), but a deeper one remains: the leaf 0x10013A40 has guard branches and
**loops over g_0B36F8 scene entities, each a ~0x2340+ byte structure**, emitting
commands that read fields at +0x60 / +0x11A0 / +0x26D4 / +0x2340. That entity
type (likely the 0x2B68-stride scene-entity array the slice2_14 skip note names)
is NOT modeled with these offsets. Transcribing now = guessing struct layouts,
which "would poison the rest" (the project's own rule). The real prerequisite is
the **scene-entity data model**, a larger multi-function RE task - the same wall
the render-frontier skip notes document. Do NOT start these until that exists.

## What IS pinned down (so the model work starts here)
The four emitters and their order:
- `0x10013A40` (843B, glide 0x10010FB0) - leaf; calls `BrRdpSetCombineLERP`
  (0x1002F900, ported). Guards on g_6C6620 and g_0B380C (==2 or ==8). Loops
  over g_0B36F8 entities.
- `0x10013D90` (846B, glide 0x10011300) - leaf; calls `BrMat4TransformPoint4`
  (0x1003B2A0, ported) + `BrFtol` (0x1007C8A0, ported).
- `0x100140E0` (847B, glide 0x10011650) - FRONTIER; calls 0x10013D90 x4 +
  BrMtxInvert (0x1003B4F0) + BrMtxMul (0x1003B470) + BrRdpSetCombineLERP.
- `0x10014450` (837B, glide 0x100119C0) - FRONTIER; calls 0x10013A40, 0x10013D90,
  BrRdpSetCombineLERP.

## Cursor owner - SOLVED, reuse it
slice2_17.c owns the g_6C0680 command cursor as `BrS17State.pGfx`, via
`BrS17GetState`, idiom `s17_emit(w0,w1)` = `{p=pGfx; pGfx=p+2; p[0]=w0;
p[1]=w1;}` (read cursor, bump 8 bytes, then store). Emitted stream is the format
br_dl.c / br_dlcmd.h already interpret (G_TRI2 etc.), so it is verifiable against
a known reader once the entity model exists.

## Cross-packet globals the cluster reads (partly modelled already)
g_6C6620 (slice5_60/slice2_18: colour-substitution arm), g_6C32D0 (slice6_72
holds a pointer), g_1829108, g_0B36F8 (entity count), g_0B380C (a mode). The
ENTITY STRUCTURE is the gap.

See [collision-response-decomp-state](collision-response-decomp-state.md), [readme-status-was-stale](../decomp/traps/readme-status-was-stale.md).
